#include "rpg2lx/js/js_runtime.hpp"
#include <iostream>
#include <sstream>

extern "C" {
#include "quickjs.h"
}

namespace rpg2lx {

struct JsRuntimeImpl {
    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;

    std::function<int(int)> get_var_hook;
    std::function<void(int, int)> set_var_hook;
    std::function<bool(int)> get_switch_hook;
    std::function<void(int, bool)> set_switch_hook;
    std::function<void(const std::string&)> log_hook;

    ~JsRuntimeImpl() {
        if (ctx) {
            JS_FreeContext(ctx);
            ctx = nullptr;
        }
        if (rt) {
            JS_FreeRuntime(rt);
            rt = nullptr;
        }
    }
};

namespace {

// C callbacks from QuickJS
JSValue js_print(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    auto* impl = static_cast<JsRuntimeImpl*>(JS_GetContextOpaque(ctx));
    std::ostringstream ss;
    for (int i = 0; i < argc; ++i) {
        if (i > 0) ss << " ";
        const char* str = JS_ToCString(ctx, argv[i]);
        if (str) {
            ss << str;
            JS_FreeCString(ctx, str);
        }
    }
    if (impl && impl->log_hook) {
        impl->log_hook(ss.str());
    } else {
        std::cout << "[JS] " << ss.str() << "\n";
    }
    return JS_UNDEFINED;
}

JSValue js_get_variable(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    auto* impl = static_cast<JsRuntimeImpl*>(JS_GetContextOpaque(ctx));
    if (!impl || !impl->get_var_hook || argc < 1) return JS_NewInt32(ctx, 0);
    int32_t var_id = 0;
    JS_ToInt32(ctx, &var_id, argv[0]);
    return JS_NewInt32(ctx, impl->get_var_hook(var_id));
}

JSValue js_set_variable(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    auto* impl = static_cast<JsRuntimeImpl*>(JS_GetContextOpaque(ctx));
    if (!impl || !impl->set_var_hook || argc < 2) return JS_UNDEFINED;
    int32_t var_id = 0;
    int32_t value = 0;
    JS_ToInt32(ctx, &var_id, argv[0]);
    JS_ToInt32(ctx, &value, argv[1]);
    impl->set_var_hook(var_id, value);
    return JS_UNDEFINED;
}

JSValue js_get_switch(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    auto* impl = static_cast<JsRuntimeImpl*>(JS_GetContextOpaque(ctx));
    if (!impl || !impl->get_switch_hook || argc < 1) return JS_NewBool(ctx, false);
    int32_t switch_id = 0;
    JS_ToInt32(ctx, &switch_id, argv[0]);
    return JS_NewBool(ctx, impl->get_switch_hook(switch_id));
}

JSValue js_set_switch(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    auto* impl = static_cast<JsRuntimeImpl*>(JS_GetContextOpaque(ctx));
    if (!impl || !impl->set_switch_hook || argc < 2) return JS_UNDEFINED;
    int32_t switch_id = 0;
    JS_ToInt32(ctx, &switch_id, argv[0]);
    bool value = JS_ToBool(ctx, argv[1]);
    impl->set_switch_hook(switch_id, value);
    return JS_UNDEFINED;
}

JSValue create_battler_js_object(JSContext* ctx, const JsRuntime::BattlerContext& b) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "id", JS_NewInt32(ctx, b.id));
    JS_SetPropertyStr(ctx, obj, "name", JS_NewString(ctx, b.name.c_str()));
    JS_SetPropertyStr(ctx, obj, "level", JS_NewInt32(ctx, b.level));
    JS_SetPropertyStr(ctx, obj, "hp", JS_NewInt32(ctx, b.hp));
    JS_SetPropertyStr(ctx, obj, "mhp", JS_NewInt32(ctx, b.mhp));
    JS_SetPropertyStr(ctx, obj, "mp", JS_NewInt32(ctx, b.mp));
    JS_SetPropertyStr(ctx, obj, "mmp", JS_NewInt32(ctx, b.mmp));
    JS_SetPropertyStr(ctx, obj, "tp", JS_NewInt32(ctx, b.tp));
    JS_SetPropertyStr(ctx, obj, "atk", JS_NewInt32(ctx, b.atk));
    JS_SetPropertyStr(ctx, obj, "def", JS_NewInt32(ctx, b.def));
    JS_SetPropertyStr(ctx, obj, "mat", JS_NewInt32(ctx, b.mat));
    JS_SetPropertyStr(ctx, obj, "mdf", JS_NewInt32(ctx, b.mdf));
    JS_SetPropertyStr(ctx, obj, "agi", JS_NewInt32(ctx, b.agi));
    JS_SetPropertyStr(ctx, obj, "luk", JS_NewInt32(ctx, b.luk));
    return obj;
}

} // namespace

JsRuntime::JsRuntime() : impl_(std::make_unique<JsRuntimeImpl>()) {}

JsRuntime::~JsRuntime() = default;

JsRuntime::JsRuntime(JsRuntime&&) noexcept = default;
JsRuntime& JsRuntime::operator=(JsRuntime&&) noexcept = default;

bool JsRuntime::initialize(std::string& error) {
    error.clear();
    reset();

    impl_->rt = JS_NewRuntime();
    if (!impl_->rt) {
        error = "Failed to create QuickJS runtime";
        return false;
    }

    impl_->ctx = JS_NewContext(impl_->rt);
    if (!impl_->ctx) {
        error = "Failed to create QuickJS context";
        return false;
    }

    JS_SetContextOpaque(impl_->ctx, impl_.get());

    JSValue global = JS_GetGlobalObject(impl_->ctx);

    // Setup window and global alias
    JS_SetPropertyStr(impl_->ctx, global, "window", JS_DupValue(impl_->ctx, global));
    JS_SetPropertyStr(impl_->ctx, global, "global", JS_DupValue(impl_->ctx, global));

    // Setup console.log, console.warn, console.error
    JSValue console = JS_NewObject(impl_->ctx);
    JS_SetPropertyStr(impl_->ctx, console, "log", JS_NewCFunction(impl_->ctx, js_print, "log", 1));
    JS_SetPropertyStr(impl_->ctx, console, "warn", JS_NewCFunction(impl_->ctx, js_print, "warn", 1));
    JS_SetPropertyStr(impl_->ctx, console, "error", JS_NewCFunction(impl_->ctx, js_print, "error", 1));
    JS_SetPropertyStr(impl_->ctx, global, "console", console);

    // Setup RPG Maker MV standard helpers and environment
    const char* init_script = R"JS(
        Math.randomInt = function(max) {
            return Math.floor(max * Math.random());
        };

        var $gameVariables = {
            value: function(id) { return __native_get_var(id); },
            setValue: function(id, val) { __native_set_var(id, Math.floor(val)); }
        };

        var $gameSwitches = {
            value: function(id) { return Boolean(__native_get_switch(id)); },
            setValue: function(id, val) { __native_set_switch(id, Boolean(val)); }
        };

        var v = new Proxy({}, {
            get: function(target, prop) {
                var id = parseInt(prop, 10);
                return (!isNaN(id)) ? $gameVariables.value(id) : 0;
            }
        });
    )JS";

    // Bind native functions used by script
    JS_SetPropertyStr(impl_->ctx, global, "__native_get_var", JS_NewCFunction(impl_->ctx, js_get_variable, "__native_get_var", 1));
    JS_SetPropertyStr(impl_->ctx, global, "__native_set_var", JS_NewCFunction(impl_->ctx, js_set_variable, "__native_set_var", 2));
    JS_SetPropertyStr(impl_->ctx, global, "__native_get_switch", JS_NewCFunction(impl_->ctx, js_get_switch, "__native_get_switch", 1));
    JS_SetPropertyStr(impl_->ctx, global, "__native_set_switch", JS_NewCFunction(impl_->ctx, js_set_switch, "__native_set_switch", 2));

    JS_FreeValue(impl_->ctx, global);

    auto result = eval(init_script, "<bootstrap>");
    if (!result.success) {
        error = "JS bootstrap error: " + result.error;
        return false;
    }

    return true;
}

void JsRuntime::reset() {
    impl_ = std::make_unique<JsRuntimeImpl>();
}

JsEvalResult JsRuntime::eval(const std::string& code, const std::string& filename) {
    JsEvalResult res;
    if (!impl_ || !impl_->ctx) {
        res.error = "QuickJS context not initialized";
        return res;
    }

    JSValue val = JS_Eval(impl_->ctx, code.c_str(), code.size(), filename.c_str(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(impl_->ctx);
        const char* err_str = JS_ToCString(impl_->ctx, exception);
        res.error = err_str ? err_str : "Unknown JS exception";
        if (err_str) JS_FreeCString(impl_->ctx, err_str);
        JS_FreeValue(impl_->ctx, exception);
        res.success = false;
    } else {
        res.success = true;
        const char* str = JS_ToCString(impl_->ctx, val);
        if (str) {
            res.string_value = str;
            JS_FreeCString(impl_->ctx, str);
        }
        double d = 0.0;
        if (JS_ToFloat64(impl_->ctx, &d, val) == 0) {
            res.number_value = d;
        }
        res.bool_value = JS_ToBool(impl_->ctx, val);
    }
    JS_FreeValue(impl_->ctx, val);
    return res;
}

double JsRuntime::eval_number(const std::string& expr, double fallback) {
    auto res = eval(expr);
    return res.success ? res.number_value : fallback;
}

bool JsRuntime::eval_bool(const std::string& expr, bool fallback) {
    auto res = eval(expr);
    return res.success ? res.bool_value : fallback;
}

double JsRuntime::eval_formula(const std::string& formula,
                              const BattlerContext& user,
                              const BattlerContext& target,
                              std::function<int(int)> get_variable,
                              double fallback) {
    if (!impl_ || !impl_->ctx || formula.empty()) return fallback;

    // Set variable hook temporarily if provided
    auto prev_hook = impl_->get_var_hook;
    if (get_variable) impl_->get_var_hook = get_variable;

    JSValue global = JS_GetGlobalObject(impl_->ctx);
    JSValue user_obj = create_battler_js_object(impl_->ctx, user);
    JSValue target_obj = create_battler_js_object(impl_->ctx, target);

    JS_SetPropertyStr(impl_->ctx, global, "a", user_obj);
    JS_SetPropertyStr(impl_->ctx, global, "b", target_obj);
    JS_FreeValue(impl_->ctx, global);

    // Evaluate formula wrapped in Number(...)
    std::string wrapped = "Number(" + formula + ")";
    auto res = eval(wrapped, "<formula>");

    if (get_variable) impl_->get_var_hook = prev_hook;

    return res.success ? res.number_value : fallback;
}

void JsRuntime::register_game_state_hooks(
    std::function<int(int)> get_var,
    std::function<void(int, int)> set_var,
    std::function<bool(int)> get_switch,
    std::function<void(int, bool)> set_switch,
    std::function<void(const std::string&)> log_msg
) {
    if (impl_) {
        impl_->get_var_hook = std::move(get_var);
        impl_->set_var_hook = std::move(set_var);
        impl_->get_switch_hook = std::move(get_switch);
        impl_->set_switch_hook = std::move(set_switch);
        impl_->log_hook = std::move(log_msg);
    }
}

} // namespace rpg2lx

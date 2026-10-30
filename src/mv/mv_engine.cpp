#include "rpg2lx/mv_engine.hpp"

#include "mv_bitmap_ops.hpp"
#include "mv_js_bridge.hpp"
#include "mv_scene_renderer.hpp"
#include "rpg2lx/font_renderer.hpp"
#include "rpg2lx/json/json_parser.hpp"
#include "rpg2lx/mv_audio.hpp"
#include "rpg2lx/project_source.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

extern "C" {
#include "quickjs.h"
}

namespace rpg2lx {

namespace fs = std::filesystem;
using namespace mv_detail;

namespace {

std::string strip_utf8_bom(std::string s) {
    if (s.size() >= 3 &&
        static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF) {
        s.erase(0, 3);
    }
    return s;
}

bool read_utf8_file(const fs::path& path, std::string& out) {
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) return false;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = strip_utf8_bom(ss.str());
    return true;
}

bool write_utf8_file(const fs::path& path, const std::string& content) {
    std::error_code ec;
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), ec);
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(f);
}

std::string js_to_std_string(JSContext* ctx, JSValueConst val) {
    const char* cstr = JS_ToCString(ctx, val);
    if (!cstr) return {};
    std::string out(cstr);
    JS_FreeCString(ctx, cstr);
    return out;
}

std::string format_js_exception(JSContext* ctx) {
    JSValue ex = JS_GetException(ctx);
    std::string msg = js_to_std_string(ctx, ex);
    if (JS_IsObject(ex)) {
        JSValue st = JS_GetPropertyStr(ctx, ex, "stack");
        if (!JS_IsUndefined(st) && !JS_IsNull(st)) {
            const std::string stack_str = js_to_std_string(ctx, st);
            if (!stack_str.empty()) msg += "\n" + stack_str;
        }
        JS_FreeValue(ctx, st);
    }
    JS_FreeValue(ctx, ex);
    return msg;
}

TextAlign parse_align_str(const std::string& s) {
    if (s == "center") return TextAlign::Center;
    if (s == "right" || s == "end") return TextAlign::Right;
    return TextAlign::Left;
}

void nbmp_finalizer(JSRuntime* /*rt*/, JSValue val) {
    const JSClassID cid = JS_GetClassID(val);
    if (cid != JS_INVALID_CLASS_ID) {
        auto* ptr = static_cast<NativeBitmap*>(JS_GetOpaque(val, cid));
        delete ptr;
    }
}

} // namespace

struct MvEngine::Impl {
    fs::path project_root;
    fs::path save_dir;
    std::string encryption_key_hex;
    std::vector<std::uint8_t> encryption_key_bytes;

    SDL_Renderer* renderer = nullptr;
    int host_w = 640;
    int host_h = 480;
    int screen_w = 816;
    int screen_h = 624;

    bool exit_requested = false;
    std::string fatal_error;
    std::uint64_t frames = 0;
    std::chrono::steady_clock::time_point boot_tp = std::chrono::steady_clock::now();

    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;
    JSClassID nbmp_class_id = 0;
    MvAtoms atoms;

    JSValue fn_drain = JS_UNDEFINED;
    JSValue fn_step = JS_UNDEFINED;
    JSValue fn_set_input = JS_UNDEFINED;
    JSValue fn_clear_input = JS_UNDEFINED;
    JSValue fn_ptr_move = JS_UNDEFINED;
    JSValue fn_ptr_down = JS_UNDEFINED;
    JSValue fn_ptr_up = JS_UNDEFINED;

    FontRenderer font;
    MvAudioEngine audio;
    std::unordered_map<std::string, std::shared_ptr<DecodedImage>> image_cache;

#ifdef RPG2LX_WITH_SDL
    MvSceneRenderer scene_renderer;
#endif

    ~Impl() {
        shutdown();
    }

    double now_ms() const {
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - boot_tp).count();
        const double sim_ms = static_cast<double>(frames) * (1000.0 / 60.0);
        return std::max(elapsed, sim_ms);
    }

    JSValue wrap_bitmap(NativeBitmap* bmp) {
        JSValue obj = JS_NewObjectClass(ctx, nbmp_class_id);
        if (JS_IsException(obj)) {
            delete bmp;
            return obj;
        }
        JS_SetOpaque(obj, bmp);
        return obj;
    }

    NativeBitmap* unwrap_bitmap(JSValueConst val) const {
        if (!JS_IsObject(val)) return nullptr;
        return static_cast<NativeBitmap*>(JS_GetOpaque(val, nbmp_class_id));
    }

    // Resolve paths for Node 'fs' shim:
    // Writes ALWAYS go strictly into save_dir; reads check save_dir first, then project_root (read-only).
    bool resolve_fs_path(const std::string& raw, fs::path& out_path, bool for_write) const {
        std::string p = raw;
        for (char& c : p) if (c == '\\') c = '/';
        if (p.find("..") != std::string::npos) return false;

        static constexpr const char* kSavePrefix = "/__rpg2lx_saves__/";
        if (p.rfind(kSavePrefix, 0) == 0) {
            const std::string sub = p.substr(std::strlen(kSavePrefix));
            if (sub.empty()) {
                out_path = save_dir;
                return true;
            }
            out_path = save_dir / fs::u8path(sub);
            if (!for_write) {
                std::error_code ec;
                if (!fs::exists(out_path, ec)) {
                    const fs::path legacy = project_root / "save" / fs::u8path(sub);
                    if (fs::exists(legacy, ec)) out_path = legacy;
                }
            }
            return true;
        }

        if (p == "/__rpg2lx_saves__") {
            out_path = save_dir;
            return true;
        }

        const std::string rel = url_decode_rel(p);
        if (for_write) {
            if (rel.empty()) return false;
            out_path = save_dir / fs::u8path(rel);
            return true;
        }
        if (rel.empty()) {
            out_path = project_root;
            return true;
        }
        const fs::path in_save = save_dir / fs::u8path(rel);
        std::error_code ec;
        if (fs::exists(in_save, ec)) {
            out_path = in_save;
            return true;
        }
        out_path = project_root / fs::u8path(rel);
        return true;
    }

    void drain_jobs() {
        if (!rt) return;
        JSContext* pctx = nullptr;
        int guard = 0;
        while (guard++ < 256 && JS_ExecutePendingJob(rt, &pctx) > 0) {}
    }

    bool eval_code(const std::string& code, const std::string& filename, std::string& error) {
        JSValue r = JS_Eval(ctx, code.c_str(), code.size(), filename.c_str(), JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(r)) {
            error = format_js_exception(ctx);
            JS_FreeValue(ctx, r);
            return false;
        }
        JS_FreeValue(ctx, r);
        drain_jobs();
        return true;
    }

    bool eval_project_script(const std::string& rel_url, std::string& error) {
        const std::string rel = url_decode_rel(rel_url);
        if (rel.empty() || rel.find("..") != std::string::npos) {
            error = "Invalid script path: " + rel_url;
            return false;
        }
        std::string code;
        if (!read_utf8_file(project_root / fs::u8path(rel), code)) {
            error = "Missing script file: " + rel;
            return false;
        }
        return eval_code(code, rel, error);
    }

    void shutdown() {
        audio.shutdown();

        if (ctx) {
            JS_FreeValue(ctx, fn_drain); fn_drain = JS_UNDEFINED;
            JS_FreeValue(ctx, fn_step); fn_step = JS_UNDEFINED;
            JS_FreeValue(ctx, fn_set_input); fn_set_input = JS_UNDEFINED;
            JS_FreeValue(ctx, fn_clear_input); fn_clear_input = JS_UNDEFINED;
            JS_FreeValue(ctx, fn_ptr_move); fn_ptr_move = JS_UNDEFINED;
            JS_FreeValue(ctx, fn_ptr_down); fn_ptr_down = JS_UNDEFINED;
            JS_FreeValue(ctx, fn_ptr_up); fn_ptr_up = JS_UNDEFINED;
            atoms.release(ctx);
            JS_FreeContext(ctx);
            ctx = nullptr;
        }
        if (rt) {
            JS_FreeRuntime(rt);
            rt = nullptr;
        }

        image_cache.clear();
#ifdef RPG2LX_WITH_SDL
        scene_renderer.release();
        font.release();
#endif
        renderer = nullptr;
    }
};

namespace {

MvEngine::Impl* get_impl(JSContext* ctx) {
    return static_cast<MvEngine::Impl*>(JS_GetContextOpaque(ctx));
}

JSValue cb_now_ms(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* im = get_impl(ctx);
    return JS_NewFloat64(ctx, im ? im->now_ms() : 0.0);
}

JSValue cb_log(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;
    int32_t lvl = 0;
    JS_ToInt32(ctx, &lvl, argv[0]);
    const std::string msg = js_to_std_string(ctx, argv[1]);
    if (lvl >= 2) std::cerr << "[MV Error] " << msg << '\n';
    return JS_UNDEFINED;
}

JSValue cb_report_error(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_UNDEFINED;
    const std::string msg = js_to_std_string(ctx, argv[0]);
    std::cerr << "[MV Exception] " << msg << '\n';
    if (im->fatal_error.empty()) im->fatal_error = msg;
    return JS_UNDEFINED;
}

JSValue cb_request_exit(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    if (auto* im = get_impl(ctx)) im->exit_requested = true;
    return JS_UNDEFINED;
}

JSValue cb_on_resize(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 2) return JS_UNDEFINED;
    int32_t w = 816, h = 624;
    JS_ToInt32(ctx, &w, argv[0]);
    JS_ToInt32(ctx, &h, argv[1]);
    if (w > 0 && h > 0) {
        im->screen_w = w;
        im->screen_h = h;
        // Adapt readability boost when downscaling > 480p MV canvas onto 640x480 handheld screen
        if (im->host_h > 0 && im->host_h <= 540 && h > im->host_h) {
            im->font.set_readability_scale(1.10f);
        } else {
            im->font.set_readability_scale(1.0f);
        }
    }
    return JS_UNDEFINED;
}

JSValue cb_load_script_sync(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_FALSE;
    const std::string src = js_to_std_string(ctx, argv[0]);
    std::string err;
    if (!im->eval_project_script(src, err)) {
        std::cerr << "[MV Plugin] " << err << '\n';
        return JS_FALSE;
    }
    return JS_TRUE;
}

JSValue cb_read_project_text(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_NULL;
    const std::string rel = url_decode_rel(js_to_std_string(ctx, argv[0]));
    if (rel.empty() || rel.find("..") != std::string::npos) return JS_NULL;
    std::string txt;
    if (!read_utf8_file(im->project_root / fs::u8path(rel), txt)) return JS_NULL;
    return JS_NewStringLen(ctx, txt.data(), txt.size());
}

JSValue cb_read_project_bytes(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_NULL;
    const std::string rel = url_decode_rel(js_to_std_string(ctx, argv[0]));
    if (rel.empty() || rel.find("..") != std::string::npos) return JS_NULL;
    std::vector<std::uint8_t> bytes;
    if (!read_binary_file(im->project_root / fs::u8path(rel), bytes)) return JS_NULL;
    return JS_NewArrayBufferCopy(ctx, bytes.data(), bytes.size());
}

JSValue cb_save_read(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_NULL;
    const std::string name = url_decode_rel(js_to_std_string(ctx, argv[0]));
    if (name.empty() || name.find("..") != std::string::npos) return JS_NULL;
    std::string txt;
    if (!read_utf8_file(im->save_dir / fs::u8path(name), txt)) return JS_NULL;
    return JS_NewStringLen(ctx, txt.data(), txt.size());
}

JSValue cb_save_write(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 2) return JS_FALSE;
    const std::string name = url_decode_rel(js_to_std_string(ctx, argv[0]));
    if (name.empty() || name.find("..") != std::string::npos) return JS_FALSE;
    const std::string data = js_to_std_string(ctx, argv[1]);
    return JS_NewBool(ctx, write_utf8_file(im->save_dir / fs::u8path(name), data));
}

JSValue cb_fs_exists(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_FALSE;
    fs::path p;
    if (!im->resolve_fs_path(js_to_std_string(ctx, argv[0]), p, false)) return JS_FALSE;
    std::error_code ec;
    return JS_NewBool(ctx, fs::exists(p, ec));
}

JSValue cb_fs_mkdir(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_FALSE;
    fs::path p;
    if (!im->resolve_fs_path(js_to_std_string(ctx, argv[0]), p, true)) return JS_FALSE;
    std::error_code ec;
    fs::create_directories(p, ec);
    return JS_NewBool(ctx, !ec);
}

JSValue cb_fs_read_text(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_NULL;
    fs::path p;
    if (!im->resolve_fs_path(js_to_std_string(ctx, argv[0]), p, false)) return JS_NULL;
    std::string txt;
    if (!read_utf8_file(p, txt)) return JS_NULL;
    return JS_NewStringLen(ctx, txt.data(), txt.size());
}

JSValue cb_fs_read_bytes(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_NULL;
    fs::path p;
    if (!im->resolve_fs_path(js_to_std_string(ctx, argv[0]), p, false)) return JS_NULL;
    std::vector<std::uint8_t> bytes;
    if (!read_binary_file(p, bytes)) return JS_NULL;
    return JS_NewArrayBufferCopy(ctx, bytes.data(), bytes.size());
}

JSValue cb_fs_write_text(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 2) return JS_FALSE;
    fs::path p;
    if (!im->resolve_fs_path(js_to_std_string(ctx, argv[0]), p, true)) return JS_FALSE;
    return JS_NewBool(ctx, write_utf8_file(p, js_to_std_string(ctx, argv[1])));
}

JSValue cb_fs_remove(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_FALSE;
    fs::path p;
    if (!im->resolve_fs_path(js_to_std_string(ctx, argv[0]), p, true)) return JS_FALSE;
    std::error_code ec;
    return JS_NewBool(ctx, fs::remove(p, ec));
}

JSValue cb_fs_readdir(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    JSValue arr = JS_NewArray(ctx);
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return arr;
    fs::path p;
    if (!im->resolve_fs_path(js_to_std_string(ctx, argv[0]), p, false)) return arr;
    std::error_code ec;
    if (!fs::is_directory(p, ec)) return arr;
    uint32_t idx = 0;
    for (const auto& entry : fs::directory_iterator(p, ec)) {
        const std::string name = entry.path().filename().u8string();
        JS_SetPropertyUint32(ctx, arr, idx++, JS_NewString(ctx, name.c_str()));
    }
    return arr;
}

JSValue cb_fs_is_dir(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_FALSE;
    fs::path p;
    if (!im->resolve_fs_path(js_to_std_string(ctx, argv[0]), p, false)) return JS_FALSE;
    std::error_code ec;
    return JS_NewBool(ctx, fs::is_directory(p, ec));
}

JSValue cb_bmp_create(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im) return JS_NULL;
    int32_t w = 1, h = 1;
    if (argc >= 1) JS_ToInt32(ctx, &w, argv[0]);
    if (argc >= 2) JS_ToInt32(ctx, &h, argv[1]);
    return im->wrap_bitmap(new NativeBitmap(w, h));
}

JSValue cb_bmp_load_file(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_NULL;
    const std::string raw_url = js_to_std_string(ctx, argv[0]);
    const std::string key = url_decode_rel(raw_url);
    if (key.empty()) return JS_NULL;

    std::shared_ptr<DecodedImage> decoded;
    auto it = im->image_cache.find(key);
    if (it != im->image_cache.end()) {
        decoded = it->second;
    } else {
        decoded = decode_project_image(im->project_root, raw_url, im->encryption_key_bytes);
        if (decoded) {
            if (im->image_cache.size() > 384) {
                // Prune images only held by the cache itself
                for (auto cit = im->image_cache.begin(); cit != im->image_cache.end(); ) {
                    if (cit->second.use_count() == 1) cit = im->image_cache.erase(cit);
                    else ++cit;
                }
            }
            im->image_cache.emplace(key, decoded);
        }
    }
    if (!decoded) return JS_NULL;
    return im->wrap_bitmap(new NativeBitmap(decoded));
}

JSValue cb_bmp_width(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    NativeBitmap* nb = (im && argc >= 1) ? im->unwrap_bitmap(argv[0]) : nullptr;
    return JS_NewInt32(ctx, nb ? nb->width : 0);
}

JSValue cb_bmp_height(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    NativeBitmap* nb = (im && argc >= 1) ? im->unwrap_bitmap(argv[0]) : nullptr;
    return JS_NewInt32(ctx, nb ? nb->height : 0);
}

JSValue cb_bmp_resize(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 3) return JS_UNDEFINED;
    if (NativeBitmap* nb = im->unwrap_bitmap(argv[0])) {
        int32_t nw = 1, nh = 1;
        JS_ToInt32(ctx, &nw, argv[1]);
        JS_ToInt32(ctx, &nh, argv[2]);
        nb->resize(nw, nh);
    }
    return JS_UNDEFINED;
}

JSValue cb_bmp_clear_rect(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 5) return JS_UNDEFINED;
    if (NativeBitmap* nb = im->unwrap_bitmap(argv[0])) {
        double x = 0, y = 0, w = 0, h = 0;
        JS_ToFloat64(ctx, &x, argv[1]);
        JS_ToFloat64(ctx, &y, argv[2]);
        JS_ToFloat64(ctx, &w, argv[3]);
        JS_ToFloat64(ctx, &h, argv[4]);
        nb->clear_rect(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)),
                       static_cast<int>(std::ceil(w)), static_cast<int>(std::ceil(h)));
    }
    return JS_UNDEFINED;
}

JSValue cb_bmp_fill_rect(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 7) return JS_UNDEFINED;
    if (NativeBitmap* nb = im->unwrap_bitmap(argv[0])) {
        double x = 0, y = 0, w = 0, h = 0;
        int32_t op = 255;
        JS_ToFloat64(ctx, &x, argv[1]);
        JS_ToFloat64(ctx, &y, argv[2]);
        JS_ToFloat64(ctx, &w, argv[3]);
        JS_ToFloat64(ctx, &h, argv[4]);
        const RgbaColor col = parse_css_color(js_to_std_string(ctx, argv[5]));
        JS_ToInt32(ctx, &op, argv[6]);
        const std::string comp = argc >= 8 ? js_to_std_string(ctx, argv[7]) : "source-over";
        nb->fill_rect(static_cast<int>(std::round(x)), static_cast<int>(std::round(y)),
                      static_cast<int>(std::round(w)), static_cast<int>(std::round(h)), col, op, comp);
    }
    return JS_UNDEFINED;
}

JSValue cb_bmp_grad_fill_rect(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 9) return JS_UNDEFINED;
    if (NativeBitmap* nb = im->unwrap_bitmap(argv[0])) {
        double x = 0, y = 0, w = 0, h = 0;
        int32_t op = 255;
        JS_ToFloat64(ctx, &x, argv[1]);
        JS_ToFloat64(ctx, &y, argv[2]);
        JS_ToFloat64(ctx, &w, argv[3]);
        JS_ToFloat64(ctx, &h, argv[4]);
        const RgbaColor c0 = parse_css_color(js_to_std_string(ctx, argv[5]));
        const RgbaColor c1 = parse_css_color(js_to_std_string(ctx, argv[6]));
        const bool vert = JS_ToBool(ctx, argv[7]) != 0;
        JS_ToInt32(ctx, &op, argv[8]);
        nb->gradient_fill_rect(static_cast<int>(std::round(x)), static_cast<int>(std::round(y)),
                               static_cast<int>(std::round(w)), static_cast<int>(std::round(h)), c0, c1, vert, op);
    }
    return JS_UNDEFINED;
}

JSValue cb_bmp_draw_circle(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 6) return JS_UNDEFINED;
    if (NativeBitmap* nb = im->unwrap_bitmap(argv[0])) {
        double cx = 0, cy = 0, r = 0;
        int32_t op = 255;
        JS_ToFloat64(ctx, &cx, argv[1]);
        JS_ToFloat64(ctx, &cy, argv[2]);
        JS_ToFloat64(ctx, &r, argv[3]);
        const RgbaColor col = parse_css_color(js_to_std_string(ctx, argv[4]));
        JS_ToInt32(ctx, &op, argv[5]);
        nb->draw_circle(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(r), col, op);
    }
    return JS_UNDEFINED;
}

JSValue cb_bmp_blt(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 11) return JS_UNDEFINED;
    NativeBitmap* dst = im->unwrap_bitmap(argv[0]);
    NativeBitmap* src = im->unwrap_bitmap(argv[1]);
    if (dst && src) {
        double sx = 0, sy = 0, sw = 0, sh = 0, dx = 0, dy = 0, dw = 0, dh = 0;
        int32_t op = 255;
        JS_ToFloat64(ctx, &sx, argv[2]);
        JS_ToFloat64(ctx, &sy, argv[3]);
        JS_ToFloat64(ctx, &sw, argv[4]);
        JS_ToFloat64(ctx, &sh, argv[5]);
        JS_ToFloat64(ctx, &dx, argv[6]);
        JS_ToFloat64(ctx, &dy, argv[7]);
        JS_ToFloat64(ctx, &dw, argv[8]);
        JS_ToFloat64(ctx, &dh, argv[9]);
        JS_ToInt32(ctx, &op, argv[10]);
        const std::string comp = argc >= 12 ? js_to_std_string(ctx, argv[11]) : "source-over";
        dst->blt(*src,
                 static_cast<int>(std::floor(sx)), static_cast<int>(std::floor(sy)),
                 static_cast<int>(std::floor(sw)), static_cast<int>(std::floor(sh)),
                 static_cast<int>(std::floor(dx)), static_cast<int>(std::floor(dy)),
                 static_cast<int>(std::floor(dw)), static_cast<int>(std::floor(dh)),
                 op, comp);
    }
    return JS_UNDEFINED;
}

JSValue cb_bmp_draw_text(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 13) return JS_UNDEFINED;
    NativeBitmap* nb = im->unwrap_bitmap(argv[0]);
    if (!nb) return JS_UNDEFINED;

    const std::string text = js_to_std_string(ctx, argv[1]);
    if (text.empty()) return JS_UNDEFINED;

    double x = 0, y = 0, max_w = 0, line_h = 0, out_w = 4;
    int32_t font_sz = 28, op = 255;
    JS_ToFloat64(ctx, &x, argv[2]);
    JS_ToFloat64(ctx, &y, argv[3]);
    JS_ToFloat64(ctx, &max_w, argv[4]);
    JS_ToFloat64(ctx, &line_h, argv[5]);
    const TextAlign align = parse_align_str(js_to_std_string(ctx, argv[6]));
    JS_ToInt32(ctx, &font_sz, argv[7]);
    const bool italic = JS_ToBool(ctx, argv[8]) != 0;
    const RgbaColor tcol = parse_css_color(js_to_std_string(ctx, argv[9]), {255, 255, 255, 255});
    const RgbaColor ocol = parse_css_color(js_to_std_string(ctx, argv[10]), {0, 0, 0, 128});
    JS_ToFloat64(ctx, &out_w, argv[11]);
    JS_ToInt32(ctx, &op, argv[12]);

    TextStyle style;
    style.font_size = std::clamp<int>(font_sz, 8, 160);
    style.italic = italic;
    style.text_color = tcol;
    style.outline_color = ocol;
    style.outline_width = std::clamp(static_cast<int>(std::round(out_w)), 0, 16);
    style.paint_opacity = static_cast<std::uint8_t>(std::clamp<int>(op, 0, 255));

    im->font.draw_text_rgba(nb->write_pixels(), nb->width, nb->height, text,
                            static_cast<int>(std::round(x)),
                            static_cast<int>(std::round(y)),
                            static_cast<int>(std::round(max_w)),
                            static_cast<int>(std::round(line_h)),
                            align, style);
    return JS_UNDEFINED;
}

JSValue cb_bmp_canvas_text(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 12) return JS_UNDEFINED;
    NativeBitmap* nb = im->unwrap_bitmap(argv[0]);
    if (!nb) return JS_UNDEFINED;

    const std::string text = js_to_std_string(ctx, argv[1]);
    if (text.empty()) return JS_UNDEFINED;

    double tx = 0, ty = 0, max_w = 0;
    int32_t font_sz = 28, line_w = 0, op = 255;
    JS_ToFloat64(ctx, &tx, argv[2]);
    JS_ToFloat64(ctx, &ty, argv[3]);
    JS_ToFloat64(ctx, &max_w, argv[4]);
    const TextAlign align = parse_align_str(js_to_std_string(ctx, argv[5]));
    JS_ToInt32(ctx, &font_sz, argv[6]);
    const bool italic = JS_ToBool(ctx, argv[7]) != 0;
    const RgbaColor fill_c = parse_css_color(js_to_std_string(ctx, argv[8]), {0, 0, 0, 0});
    const RgbaColor stroke_c = parse_css_color(js_to_std_string(ctx, argv[9]), {0, 0, 0, 0});
    JS_ToInt32(ctx, &line_w, argv[10]);
    JS_ToInt32(ctx, &op, argv[11]);

    int box_w = static_cast<int>(std::round(max_w));
    int box_x = static_cast<int>(std::round(tx));
    if (align == TextAlign::Center) {
        if (box_w <= 0) box_w = nb->width;
        box_x -= box_w / 2;
    } else if (align == TextAlign::Right) {
        if (box_w <= 0) box_w = nb->width;
        box_x -= box_w;
    }

    TextStyle style;
    style.font_size = std::clamp<int>(font_sz, 8, 160);
    style.italic = italic;
    style.text_color = fill_c;
    style.outline_color = stroke_c;
    style.outline_width = stroke_c.a > 0 ? std::clamp<int>(line_w, 0, 16) : 0;
    style.paint_opacity = static_cast<std::uint8_t>(std::clamp<int>(op, 0, 255));

    const int approx_lh = std::max(12, style.font_size);
    const int box_y = static_cast<int>(std::round(ty - approx_lh * 0.86));
    im->font.draw_text_rgba(nb->write_pixels(), nb->width, nb->height, text,
                            box_x, box_y, box_w, approx_lh, align, style);
    return JS_UNDEFINED;
}

JSValue cb_measure_text(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_NewInt32(ctx, 0);
    const std::string text = js_to_std_string(ctx, argv[0]);
    int32_t sz = 28;
    if (argc >= 2) JS_ToInt32(ctx, &sz, argv[1]);
    const bool italic = (argc >= 3) ? (JS_ToBool(ctx, argv[2]) != 0) : false;
    return JS_NewInt32(ctx, im->font.measure_text_px(text, sz, italic));
}

JSValue cb_bmp_get_pixel_hex(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 3) return JS_NewString(ctx, "#000000");
    NativeBitmap* nb = im->unwrap_bitmap(argv[0]);
    if (!nb) return JS_NewString(ctx, "#000000");
    int32_t x = 0, y = 0;
    JS_ToInt32(ctx, &x, argv[1]);
    JS_ToInt32(ctx, &y, argv[2]);
    const std::string hex = nb->get_pixel_hex(x, y);
    return JS_NewString(ctx, hex.c_str());
}

JSValue cb_bmp_get_alpha(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 3) return JS_NewInt32(ctx, 0);
    NativeBitmap* nb = im->unwrap_bitmap(argv[0]);
    if (!nb) return JS_NewInt32(ctx, 0);
    int32_t x = 0, y = 0;
    JS_ToInt32(ctx, &x, argv[1]);
    JS_ToInt32(ctx, &y, argv[2]);
    return JS_NewInt32(ctx, nb->get_alpha(x, y));
}

JSValue cb_bmp_get_pixels(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 5) return JS_NULL;
    NativeBitmap* nb = im->unwrap_bitmap(argv[0]);
    int32_t rx = 0, ry = 0, rw = 1, rh = 1;
    JS_ToInt32(ctx, &rx, argv[1]);
    JS_ToInt32(ctx, &ry, argv[2]);
    JS_ToInt32(ctx, &rw, argv[3]);
    JS_ToInt32(ctx, &rh, argv[4]);
    rw = std::clamp<int32_t>(rw, 1, 4096);
    rh = std::clamp<int32_t>(rh, 1, 4096);
    std::vector<std::uint32_t> out(static_cast<std::size_t>(rw) * static_cast<std::size_t>(rh), 0u);
    if (nb) {
        const std::uint32_t* spx = nb->read_pixels();
        for (int y = 0; y < rh; ++y) {
            const int sy = ry + y;
            if (sy < 0 || sy >= nb->height) continue;
            for (int x = 0; x < rw; ++x) {
                const int sx = rx + x;
                if (sx >= 0 && sx < nb->width) {
                    out[static_cast<std::size_t>(y) * rw + x] = spx[static_cast<std::size_t>(sy) * nb->width + sx];
                }
            }
        }
    }
    return JS_NewArrayBufferCopy(ctx, reinterpret_cast<const uint8_t*>(out.data()), out.size() * 4);
}

JSValue cb_bmp_put_pixels(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 6) return JS_UNDEFINED;
    NativeBitmap* nb = im->unwrap_bitmap(argv[0]);
    if (!nb) return JS_UNDEFINED;
    int32_t dx = 0, dy = 0, rw = 0, rh = 0;
    JS_ToInt32(ctx, &dx, argv[1]);
    JS_ToInt32(ctx, &dy, argv[2]);
    JS_ToInt32(ctx, &rw, argv[3]);
    JS_ToInt32(ctx, &rh, argv[4]);
    size_t byte_len = 0;
    uint8_t* buf = JS_GetArrayBuffer(ctx, &byte_len, argv[5]);
    if (!buf || rw <= 0 || rh <= 0 || byte_len < static_cast<size_t>(rw) * static_cast<size_t>(rh) * 4) {
        return JS_UNDEFINED;
    }
    const auto* spx = reinterpret_cast<const std::uint32_t*>(buf);
    std::uint32_t* dpx = nb->write_pixels();
    for (int y = 0; y < rh; ++y) {
        const int ty = dy + y;
        if (ty < 0 || ty >= nb->height) continue;
        for (int x = 0; x < rw; ++x) {
            const int tx = dx + x;
            if (tx >= 0 && tx < nb->width) {
                dpx[static_cast<std::size_t>(ty) * nb->width + tx] = spx[static_cast<std::size_t>(y) * rw + x];
            }
        }
    }
    return JS_UNDEFINED;
}

JSValue cb_bmp_adjust_tone(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 4) return JS_UNDEFINED;
    if (NativeBitmap* nb = im->unwrap_bitmap(argv[0])) {
        int32_t r = 0, g = 0, b = 0;
        JS_ToInt32(ctx, &r, argv[1]);
        JS_ToInt32(ctx, &g, argv[2]);
        JS_ToInt32(ctx, &b, argv[3]);
        nb->adjust_tone(r, g, b);
    }
    return JS_UNDEFINED;
}

JSValue cb_bmp_rotate_hue(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 2) return JS_UNDEFINED;
    if (NativeBitmap* nb = im->unwrap_bitmap(argv[0])) {
        double deg = 0.0;
        JS_ToFloat64(ctx, &deg, argv[1]);
        nb->rotate_hue(deg);
    }
    return JS_UNDEFINED;
}

JSValue cb_bmp_blur(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (im && argc >= 1) {
        if (NativeBitmap* nb = im->unwrap_bitmap(argv[0])) nb->blur();
    }
    return JS_UNDEFINED;
}

JSValue cb_render_stage(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
#ifdef RPG2LX_WITH_SDL
    auto* im = get_impl(ctx);
    if (im && im->renderer && argc >= 3) {
        int32_t w = im->screen_w, h = im->screen_h;
        JS_ToInt32(ctx, &w, argv[1]);
        JS_ToInt32(ctx, &h, argv[2]);
        im->scene_renderer.render_stage(ctx, argv[0], w, h);
    }
#else
    (void)ctx; (void)argc; (void)argv;
#endif
    return JS_UNDEFINED;
}

JSValue cb_render_stage_to_bmp(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
#ifdef RPG2LX_WITH_SDL
    auto* im = get_impl(ctx);
    if (im && im->renderer && argc >= 4) {
        NativeBitmap* target = im->unwrap_bitmap(argv[1]);
        int32_t w = im->screen_w, h = im->screen_h;
        JS_ToInt32(ctx, &w, argv[2]);
        JS_ToInt32(ctx, &h, argv[3]);
        if (target) im->scene_renderer.render_stage_to_bitmap(ctx, argv[0], target, w, h);
    }
#else
    (void)ctx; (void)argc; (void)argv;
#endif
    return JS_UNDEFINED;
}

JSValue cb_audio_create(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_NewInt32(ctx, 0);
    return JS_NewInt32(ctx, im->audio.create_player(url_decode_rel(js_to_std_string(ctx, argv[0]))));
}

JSValue cb_audio_destroy(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (im && argc >= 1) {
        int32_t id = 0; JS_ToInt32(ctx, &id, argv[0]);
        im->audio.destroy_player(id);
    }
    return JS_UNDEFINED;
}

JSValue cb_audio_is_ready(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_FALSE;
    int32_t id = 0; JS_ToInt32(ctx, &id, argv[0]);
    return JS_NewBool(ctx, im->audio.is_ready(id));
}

JSValue cb_audio_is_playing(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_FALSE;
    int32_t id = 0; JS_ToInt32(ctx, &id, argv[0]);
    return JS_NewBool(ctx, im->audio.is_playing(id));
}

JSValue cb_audio_play(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (im && argc >= 2) {
        int32_t id = 0; JS_ToInt32(ctx, &id, argv[0]);
        const bool loop = JS_ToBool(ctx, argv[1]) != 0;
        double offset = 0.0;
        if (argc >= 3) JS_ToFloat64(ctx, &offset, argv[2]);
        im->audio.play(id, loop, offset);
    }
    return JS_UNDEFINED;
}

JSValue cb_audio_stop(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (im && argc >= 1) {
        int32_t id = 0; JS_ToInt32(ctx, &id, argv[0]);
        im->audio.stop(id);
    }
    return JS_UNDEFINED;
}

JSValue cb_audio_set_vol(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (im && argc >= 2) {
        int32_t id = 0; double v = 1.0;
        JS_ToInt32(ctx, &id, argv[0]);
        JS_ToFloat64(ctx, &v, argv[1]);
        im->audio.set_volume(id, static_cast<float>(v));
    }
    return JS_UNDEFINED;
}

JSValue cb_audio_set_pitch(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (im && argc >= 2) {
        int32_t id = 0; double p = 1.0;
        JS_ToInt32(ctx, &id, argv[0]);
        JS_ToFloat64(ctx, &p, argv[1]);
        im->audio.set_pitch(id, static_cast<float>(p));
    }
    return JS_UNDEFINED;
}

JSValue cb_audio_set_pan(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (im && argc >= 2) {
        int32_t id = 0; double p = 0.0;
        JS_ToInt32(ctx, &id, argv[0]);
        JS_ToFloat64(ctx, &p, argv[1]);
        im->audio.set_pan(id, static_cast<float>(p));
    }
    return JS_UNDEFINED;
}

JSValue cb_audio_fade_in(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (im && argc >= 2) {
        int32_t id = 0; double d = 0.0;
        JS_ToInt32(ctx, &id, argv[0]);
        JS_ToFloat64(ctx, &d, argv[1]);
        im->audio.fade_in(id, d);
    }
    return JS_UNDEFINED;
}

JSValue cb_audio_fade_out(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (im && argc >= 2) {
        int32_t id = 0; double d = 0.0;
        JS_ToInt32(ctx, &id, argv[0]);
        JS_ToFloat64(ctx, &d, argv[1]);
        im->audio.fade_out(id, d);
    }
    return JS_UNDEFINED;
}

JSValue cb_audio_seek(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (!im || argc < 1) return JS_NewFloat64(ctx, 0.0);
    int32_t id = 0; JS_ToInt32(ctx, &id, argv[0]);
    return JS_NewFloat64(ctx, im->audio.seek_position(id));
}

JSValue cb_audio_master_vol(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* im = get_impl(ctx);
    if (im && argc >= 1) {
        double v = 1.0; JS_ToFloat64(ctx, &v, argv[0]);
        im->audio.set_master_volume(static_cast<float>(v));
    }
    return JS_UNDEFINED;
}

struct BridgeFunc {
    const char* name;
    int length;
    JSCFunction* fn;
};

const BridgeFunc kRpg2lxFuncs[] = {
    {"nowMs", 0, cb_now_ms},
    {"log", 2, cb_log},
    {"reportError", 1, cb_report_error},
    {"requestExit", 0, cb_request_exit},
    {"onResize", 2, cb_on_resize},
    {"loadScriptSync", 1, cb_load_script_sync},
    {"readProjectText", 1, cb_read_project_text},
    {"readProjectBytes", 1, cb_read_project_bytes},
    {"saveRead", 1, cb_save_read},
    {"saveWrite", 2, cb_save_write},
    {"fsExists", 1, cb_fs_exists},
    {"fsMkdir", 1, cb_fs_mkdir},
    {"fsReadText", 1, cb_fs_read_text},
    {"fsReadBytes", 1, cb_fs_read_bytes},
    {"fsWriteText", 2, cb_fs_write_text},
    {"fsRemove", 1, cb_fs_remove},
    {"fsReaddir", 1, cb_fs_readdir},
    {"fsIsDir", 1, cb_fs_is_dir},
    {"bmpCreate", 2, cb_bmp_create},
    {"bmpLoadFile", 1, cb_bmp_load_file},
    {"bmpWidth", 1, cb_bmp_width},
    {"bmpHeight", 1, cb_bmp_height},
    {"bmpResize", 3, cb_bmp_resize},
    {"bmpClearRect", 5, cb_bmp_clear_rect},
    {"bmpFillRect", 8, cb_bmp_fill_rect},
    {"bmpGradientFillRect", 9, cb_bmp_grad_fill_rect},
    {"bmpDrawCircle", 6, cb_bmp_draw_circle},
    {"bmpBlt", 12, cb_bmp_blt},
    {"bmpDrawText", 13, cb_bmp_draw_text},
    {"bmpCanvasText", 12, cb_bmp_canvas_text},
    {"measureText", 3, cb_measure_text},
    {"bmpGetPixelHex", 3, cb_bmp_get_pixel_hex},
    {"bmpGetAlpha", 3, cb_bmp_get_alpha},
    {"bmpGetPixels", 5, cb_bmp_get_pixels},
    {"bmpPutPixels", 6, cb_bmp_put_pixels},
    {"bmpAdjustTone", 4, cb_bmp_adjust_tone},
    {"bmpRotateHue", 2, cb_bmp_rotate_hue},
    {"bmpBlur", 1, cb_bmp_blur},
    {"renderStage", 3, cb_render_stage},
    {"renderStageToBitmap", 4, cb_render_stage_to_bmp},
    {"audioCreate", 1, cb_audio_create},
    {"audioDestroy", 1, cb_audio_destroy},
    {"audioIsReady", 1, cb_audio_is_ready},
    {"audioIsPlaying", 1, cb_audio_is_playing},
    {"audioPlay", 3, cb_audio_play},
    {"audioStop", 1, cb_audio_stop},
    {"audioSetVolume", 2, cb_audio_set_vol},
    {"audioSetPitch", 2, cb_audio_set_pitch},
    {"audioSetPan", 2, cb_audio_set_pan},
    {"audioFadeIn", 2, cb_audio_fade_in},
    {"audioFadeOut", 2, cb_audio_fade_out},
    {"audioSeek", 1, cb_audio_seek},
    {"audioSetMasterVolume", 1, cb_audio_master_vol},
};

} // namespace

MvEngine::MvEngine() : impl_(std::make_unique<Impl>()) {}
MvEngine::~MvEngine() = default;

bool MvEngine::is_supported_project(const fs::path& project_path) {
    const auto resolved = resolve_mv_project(project_path);
    if (!resolved || !resolved.source) return false;
    const fs::path& root = resolved.source->root;
    std::error_code ec;
    static constexpr const char* kRequiredScripts[] = {
        "js/rpg_core.js",
        "js/rpg_managers.js",
        "js/rpg_objects.js",
        "js/rpg_scenes.js",
        "js/rpg_sprites.js",
        "js/rpg_windows.js",
    };
    for (const char* s : kRequiredScripts) {
        if (!fs::is_regular_file(root / s, ec)) return false;
    }
    return true;
}

bool MvEngine::boot(const fs::path& project_path,
                    const fs::path& save_dir,
                    SDL_Renderer* renderer,
                    int host_screen_w,
                    int host_screen_h,
                    std::string& error) {
    error.clear();
    impl_->shutdown();

    const auto resolved = resolve_mv_project(project_path);
    if (!resolved || !resolved.source) {
        error = resolved.error.empty() ? "Invalid RPG Maker MV project" : resolved.error;
        return false;
    }

    impl_->project_root = resolved.source->root;
    impl_->save_dir = save_dir.empty() ? (impl_->project_root / "save") : save_dir;
    impl_->renderer = renderer;
    impl_->host_w = host_screen_w > 0 ? host_screen_w : 640;
    impl_->host_h = host_screen_h > 0 ? host_screen_h : 480;
    impl_->screen_w = 816;
    impl_->screen_h = 624;
    impl_->exit_requested = false;
    impl_->fatal_error.clear();
    impl_->frames = 0;
    impl_->boot_tp = std::chrono::steady_clock::now();

    std::error_code ec;
    fs::create_directories(impl_->save_dir, ec);

    // Pre-read encryptionKey from data/System.json if present
    impl_->encryption_key_hex.clear();
    impl_->encryption_key_bytes.clear();
    {
        std::string sys_txt;
        if (read_utf8_file(impl_->project_root / "data/System.json", sys_txt)) {
            std::string json_err;
            const auto parsed = JsonParser::parse(sys_txt, json_err);
            if (json_err.empty() && parsed.is_object()) {
                if (const auto* k = parsed.find("encryptionKey"); k && k->is_string()) {
                    impl_->encryption_key_hex = k->get_string();
                    impl_->encryption_key_bytes = hex_key_to_bytes(impl_->encryption_key_hex);
                }
            }
        }
    }

    // Initialize TrueType GameFont + handheld readability tuning
    impl_->font.load_project_font(impl_->project_root);
    if (impl_->host_h <= 540) {
        impl_->font.set_readability_scale(1.10f);
    } else {
        impl_->font.set_readability_scale(1.0f);
    }

    // Initialize audio mixer (gracefully runs silent if audio device unavailable)
    impl_->audio.init(impl_->project_root, impl_->encryption_key_hex);

    // Initialize QuickJS runtime & NativeBitmap class
    impl_->rt = JS_NewRuntime();
    if (!impl_->rt) {
        error = "Failed to allocate QuickJS runtime";
        return false;
    }
    JS_SetMemoryLimit(impl_->rt, 256u * 1024u * 1024u);
    JS_SetMaxStackSize(impl_->rt, 2u * 1024u * 1024u);

    impl_->nbmp_class_id = 0;
    JS_NewClassID(impl_->rt, &impl_->nbmp_class_id);
    JSClassDef cls_def{};
    cls_def.class_name = "NativeBitmap";
    cls_def.finalizer = nbmp_finalizer;
    JS_NewClass(impl_->rt, impl_->nbmp_class_id, &cls_def);

    impl_->ctx = JS_NewContext(impl_->rt);
    if (!impl_->ctx) {
        error = "Failed to allocate QuickJS context";
        impl_->shutdown();
        return false;
    }
    JS_SetContextOpaque(impl_->ctx, impl_.get());
    impl_->atoms.init(impl_->ctx);

#ifdef RPG2LX_WITH_SDL
    impl_->scene_renderer.init(renderer, impl_->nbmp_class_id, &impl_->atoms);
#endif

    // Install __rpg2lx native module on globalThis
    JSValue global = JS_GetGlobalObject(impl_->ctx);
    JSValue bridge = JS_NewObject(impl_->ctx);
    for (const auto& f : kRpg2lxFuncs) {
        JS_SetPropertyStr(impl_->ctx, bridge, f.name,
                          JS_NewCFunction(impl_->ctx, f.fn, f.name, f.length));
    }
    JS_SetPropertyStr(impl_->ctx, global, "__rpg2lx", bridge);
    JS_FreeValue(impl_->ctx, global);

    // 1. Pre-core DOM / Canvas / PIXI / NW.js bootstrap
    if (!impl_->eval_code(kMvPreCoreBootstrapJs, "<mv_pre_core>", error)) {
        impl_->shutdown();
        return false;
    }

    // 2. Optional lz-string.js (required for .rpgsave base64 compression in StorageManager)
    if (fs::is_regular_file(impl_->project_root / "js/libs/lz-string.js", ec)) {
        if (!impl_->eval_project_script("js/libs/lz-string.js", error)) {
            impl_->shutdown();
            return false;
        }
    } else {
        // Minimal LZString fallback if project omitted js/libs/lz-string.js
        static constexpr const char* kLzFallback = R"JS(
            window.LZString = {
                compressToBase64: function(s) { return String(s); },
                decompressFromBase64: function(s) { return String(s); }
            };
        )JS";
        impl_->eval_code(kLzFallback, "<lz_fallback>", error);
    }

    // 3. Load unmodified rpg_core.js + native acceleration hooks
    if (!impl_->eval_project_script("js/rpg_core.js", error) ||
        !impl_->eval_code(kMvPostCorePatchJs, "<mv_post_core>", error)) {
        impl_->shutdown();
        return false;
    }

    // 4. Load unmodified rpg_managers.js + StorageManager/SceneManager/Input hooks
    if (!impl_->eval_project_script("js/rpg_managers.js", error) ||
        !impl_->eval_code(kMvPostManagersPatchJs, "<mv_post_managers>", error)) {
        impl_->shutdown();
        return false;
    }

    // 5. Load unmodified rpg_objects.js, rpg_scenes.js, rpg_sprites.js, rpg_windows.js
    static constexpr const char* kUpperScripts[] = {
        "js/rpg_objects.js",
        "js/rpg_scenes.js",
        "js/rpg_sprites.js",
        "js/rpg_windows.js",
    };
    for (const char* script : kUpperScripts) {
        if (!impl_->eval_project_script(script, error)) {
            impl_->shutdown();
            return false;
        }
    }

    // 6. Load js/plugins.js & initialize enabled plugins (including CustomLogo.js, Community_Basic.js, etc.)
    if (fs::is_regular_file(impl_->project_root / "js/plugins.js", ec)) {
        if (!impl_->eval_project_script("js/plugins.js", error)) {
            impl_->shutdown();
            return false;
        }
    } else {
        impl_->eval_code("var $plugins = [];", "<empty_plugins>", error);
    }

    // 7. Cache fast per-frame JS entrypoints & start Scene_Boot (just like js/main.js!)
    static constexpr const char* kStartMainJs = R"JS(
        if (typeof $plugins !== 'undefined' && Array.isArray($plugins)) {
            PluginManager.setup($plugins);
        }
        SceneManager.run(Scene_Boot);
    )JS";
    if (!impl_->eval_code(kStartMainJs, "js/main.js", error)) {
        impl_->shutdown();
        return false;
    }

    JSValue g = JS_GetGlobalObject(impl_->ctx);
    impl_->fn_drain = JS_GetPropertyStr(impl_->ctx, g, "__drainFrameCallbacks");
    impl_->fn_step = JS_GetPropertyStr(impl_->ctx, g, "__rpg2lxStepScene");
    impl_->fn_set_input = JS_GetPropertyStr(impl_->ctx, g, "__rpg2lxSetInput");
    impl_->fn_clear_input = JS_GetPropertyStr(impl_->ctx, g, "__rpg2lxClearInput");
    impl_->fn_ptr_move = JS_GetPropertyStr(impl_->ctx, g, "__rpg2lxPointerMove");
    impl_->fn_ptr_down = JS_GetPropertyStr(impl_->ctx, g, "__rpg2lxPointerDown");
    impl_->fn_ptr_up = JS_GetPropertyStr(impl_->ctx, g, "__rpg2lxPointerUp");
    JS_FreeValue(impl_->ctx, g);

    return true;
}

void MvEngine::shutdown() {
    impl_->shutdown();
}

void MvEngine::set_button(const std::string& mv_key_name, bool pressed) {
    if (!impl_->ctx || !JS_IsFunction(impl_->ctx, impl_->fn_set_input)) return;
    JSValue args[2] = {
        JS_NewString(impl_->ctx, mv_key_name.c_str()),
        JS_NewBool(impl_->ctx, pressed)
    };
    JSValue r = JS_Call(impl_->ctx, impl_->fn_set_input, JS_UNDEFINED, 2, args);
    JS_FreeValue(impl_->ctx, r);
    JS_FreeValue(impl_->ctx, args[0]);
    JS_FreeValue(impl_->ctx, args[1]);
}

void MvEngine::clear_inputs() {
    if (!impl_->ctx || !JS_IsFunction(impl_->ctx, impl_->fn_clear_input)) return;
    JSValue r = JS_Call(impl_->ctx, impl_->fn_clear_input, JS_UNDEFINED, 0, nullptr);
    JS_FreeValue(impl_->ctx, r);
}

void MvEngine::on_pointer_move(int game_x, int game_y) {
    if (!impl_->ctx || !JS_IsFunction(impl_->ctx, impl_->fn_ptr_move)) return;
    JSValue args[2] = {JS_NewInt32(impl_->ctx, game_x), JS_NewInt32(impl_->ctx, game_y)};
    JSValue r = JS_Call(impl_->ctx, impl_->fn_ptr_move, JS_UNDEFINED, 2, args);
    JS_FreeValue(impl_->ctx, r);
}

void MvEngine::on_pointer_down(int game_x, int game_y, bool right_button) {
    if (!impl_->ctx || !JS_IsFunction(impl_->ctx, impl_->fn_ptr_down)) return;
    JSValue args[3] = {
        JS_NewInt32(impl_->ctx, game_x),
        JS_NewInt32(impl_->ctx, game_y),
        JS_NewBool(impl_->ctx, right_button)
    };
    JSValue r = JS_Call(impl_->ctx, impl_->fn_ptr_down, JS_UNDEFINED, 3, args);
    JS_FreeValue(impl_->ctx, r);
}

void MvEngine::on_pointer_up(int game_x, int game_y) {
    if (!impl_->ctx || !JS_IsFunction(impl_->ctx, impl_->fn_ptr_up)) return;
    JSValue args[2] = {JS_NewInt32(impl_->ctx, game_x), JS_NewInt32(impl_->ctx, game_y)};
    JSValue r = JS_Call(impl_->ctx, impl_->fn_ptr_up, JS_UNDEFINED, 2, args);
    JS_FreeValue(impl_->ctx, r);
}

bool MvEngine::step_and_render(std::string& error) {
    error.clear();
    if (!impl_->ctx) {
        error = "MvEngine not initialized";
        return false;
    }

    if (JS_IsFunction(impl_->ctx, impl_->fn_drain)) {
        JSValue r1 = JS_Call(impl_->ctx, impl_->fn_drain, JS_UNDEFINED, 0, nullptr);
        if (JS_IsException(r1)) {
            error = format_js_exception(impl_->ctx);
        }
        JS_FreeValue(impl_->ctx, r1);
        impl_->drain_jobs();
    }

    if (error.empty() && JS_IsFunction(impl_->ctx, impl_->fn_step)) {
        JSValue r2 = JS_Call(impl_->ctx, impl_->fn_step, JS_UNDEFINED, 0, nullptr);
        if (JS_IsException(r2)) {
            error = format_js_exception(impl_->ctx);
        }
        JS_FreeValue(impl_->ctx, r2);
        impl_->drain_jobs();
    }

    ++impl_->frames;

    if (!impl_->fatal_error.empty()) {
        error = impl_->fatal_error;
        return false;
    }
    return error.empty();
}

int MvEngine::screen_width() const { return impl_->screen_w; }
int MvEngine::screen_height() const { return impl_->screen_h; }
bool MvEngine::wants_exit() const { return impl_->exit_requested; }
std::uint64_t MvEngine::frame_count() const { return impl_->frames; }

std::string MvEngine::eval_js(const std::string& script, std::string& error) {
    error.clear();
    if (!impl_->ctx) {
        error = "MvEngine not initialized";
        return {};
    }
    JSValue val = JS_Eval(impl_->ctx, script.c_str(), script.size(), "<eval>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        error = format_js_exception(impl_->ctx);
        JS_FreeValue(impl_->ctx, val);
        return {};
    }
    const std::string res = js_to_std_string(impl_->ctx, val);
    JS_FreeValue(impl_->ctx, val);
    impl_->drain_jobs();
    return res;
}

} // namespace rpg2lx

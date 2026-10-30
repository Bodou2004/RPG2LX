#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace rpg2lx {

struct JsEvalResult {
    bool success = false;
    std::string string_value;
    double number_value = 0.0;
    bool bool_value = false;
    std::string error;
};

// Forward declaration of internal QuickJS wrapper data
struct JsRuntimeImpl;

class JsRuntime {
public:
    JsRuntime();
    ~JsRuntime();

    JsRuntime(const JsRuntime&) = delete;
    JsRuntime& operator=(const JsRuntime&) = delete;

    JsRuntime(JsRuntime&&) noexcept;
    JsRuntime& operator=(JsRuntime&&) noexcept;

    bool initialize(std::string& error);
    void reset();

    // Evaluate arbitrary JS code
    JsEvalResult eval(const std::string& code, const std::string& filename = "<eval>");

    // Evaluate an expression as a double (e.g. for damage formulas)
    double eval_number(const std::string& expr, double fallback = 0.0);

    // Evaluate an expression as a boolean (e.g. for conditional branch scripts)
    bool eval_bool(const std::string& expr, bool fallback = false);

    // Evaluate an RPG Maker MV damage/effect formula with 'a' (user), 'b' (target), 'v' (variables)
    // battler stats: hp, mhp, mp, mmp, tp, atk, def, mat, mdf, agi, luk, level
    struct BattlerContext {
        int id = 1;
        std::string name;
        int level = 1;
        int hp = 100;
        int mhp = 100;
        int mp = 50;
        int mmp = 50;
        int tp = 0;
        int atk = 10;
        int def = 10;
        int mat = 10;
        int mdf = 10;
        int agi = 10;
        int luk = 10;
    };

    double eval_formula(const std::string& formula,
                         const BattlerContext& user,
                         const BattlerContext& target,
                         std::function<int(int)> get_variable,
                         double fallback = 0.0);

    // Register variable and switch access hooks so scripts ($gameVariables, $gameSwitches) interact with GameSession
    void register_game_state_hooks(
        std::function<int(int)> get_var,
        std::function<void(int, int)> set_var,
        std::function<bool(int)> get_switch,
        std::function<void(int, bool)> set_switch,
        std::function<void(const std::string&)> log_msg
    );

private:
    std::unique_ptr<JsRuntimeImpl> impl_;
};

} // namespace rpg2lx

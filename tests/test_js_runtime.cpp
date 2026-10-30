#include "rpg2lx/js/js_runtime.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace rpg2lx;

int main() {
    std::cout << "[Test] Initializing JsRuntime...\n";
    JsRuntime js;
    std::string error;
    bool ok = js.initialize(error);
    if (!ok) {
        std::cerr << "Initialization failed: " << error << "\n";
        return 1;
    }

    // 1. Basic evaluation
    auto res = js.eval("1 + 2 * 3");
    assert(res.success);
    assert(std::abs(res.number_value - 7.0) < 0.0001);
    std::cout << "1. Basic eval: OK (7.0)\n";

    // 2. Modern ES6 syntax (arrow function, let, template strings)
    res = js.eval("let square = (x) => x * x; square(5)");
    assert(res.success);
    assert(std::abs(res.number_value - 25.0) < 0.0001);
    std::cout << "2. Modern ES6 syntax: OK (25.0)\n";

    // 3. Math.randomInt
    res = js.eval("Math.randomInt(10)");
    assert(res.success);
    assert(res.number_value >= 0 && res.number_value < 10);
    std::cout << "3. Math.randomInt: OK\n";

    // 4. Damage formula evaluation with 'a' and 'b'
    JsRuntime::BattlerContext hero;
    hero.name = "Harold";
    hero.atk = 25;
    hero.hp = 450;

    JsRuntime::BattlerContext goblin;
    goblin.name = "Goblin";
    goblin.def = 10;
    goblin.hp = 80;

    double dmg = js.eval_formula("a.atk * 4 - b.def * 2", hero, goblin, nullptr, 0.0);
    // 25 * 4 - 10 * 2 = 100 - 20 = 80
    assert(std::abs(dmg - 80.0) < 0.0001);
    std::cout << "4. Damage formula a.atk*4 - b.def*2: OK (" << dmg << ")\n";

    // 5. Formula with 'v' variables
    auto var_provider = [](int id) -> int {
        if (id == 1) return 15;
        if (id == 5) return 50;
        return 0;
    };
    double dmg_var = js.eval_formula("a.atk + v[1] * 2", hero, goblin, var_provider, 0.0);
    // 25 + 15 * 2 = 55
    assert(std::abs(dmg_var - 55.0) < 0.0001);
    std::cout << "5. Formula with v[1]: OK (" << dmg_var << ")\n";

    // 6. Safe exception handling
    res = js.eval("syntax error ?%%@");
    assert(!res.success);
    assert(!res.error.empty());
    std::cout << "6. Syntax error caught gracefully: " << res.error << "\n";

    // 7. eval_bool for conditional branching
    assert(js.eval_bool("5 > 3", false) == true);
    assert(js.eval_bool("10 === 20", true) == false);
    std::cout << "7. eval_bool: OK\n";

    std::cout << "All JsRuntime unit tests passed successfully!\n";
    return 0;
}

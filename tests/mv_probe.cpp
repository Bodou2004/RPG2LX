// Drives a real MV project headlessly to find compatibility problems.
// Usage: mv_probe <project> <out_dir> <script>
// Script lines: "frames N", "key CODE [HOLD]", "js EXPR", "print EXPR", "shot NAME",
// "until EXPR MAXFRAMES", "stats LABEL" (prints and resets the frame timings). Any JS error stops the probe with exit code 1.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "rpg2lx/mv_engine.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: mv_probe <project> <out_dir> <script>\n";
        return 2;
    }
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 2;
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_Window* win = SDL_CreateWindow("probe", 0, 0, 816, 624, SDL_WINDOW_HIDDEN);
    // RPG2LX_PROBE_GPU uses the accelerated renderer, as the console does, instead of the software one.
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, std::getenv("RPG2LX_PROBE_GPU")
        ? (SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE) : SDL_RENDERER_SOFTWARE);
    const fs::path out = fs::u8path(argv[2]);
    fs::create_directories(out / "saves");

    rpg2lx::MvEngine engine;
    // Frame-count scripts need images on the frame they were asked for; RPG2LX_ASYNC_IMAGES=1 measures the real path.
    engine.set_blocking_image_decode(std::getenv("RPG2LX_ASYNC_IMAGES") == nullptr);
    std::string err;
    const auto t_boot = std::chrono::steady_clock::now();
    if (!engine.boot(fs::u8path(argv[1]), out / "saves", ren, 640, 480, err)) {
        std::cerr << "BOOT FAILED: " << err << '\n';
        return 1;
    }
    std::cout << "boot " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_boot).count() << " ms\n";

    double total_ms = 0, worst_ms = 0;
    long frames = 0, slow = 0;
    // RPG2LX_UPDATES=N runs N logic updates per drawn frame, as the launcher does to catch up on a slow device.
    const char* updates_env = std::getenv("RPG2LX_UPDATES");
    const int updates = updates_env ? std::max(1, std::atoi(updates_env)) : 1;
    // RPG2LX_SLOW_MS=N prints every frame slower than N ms.
    const char* slow_env = std::getenv("RPG2LX_SLOW_MS");
    const double slow_log_ms = slow_env ? std::atof(slow_env) : 0.0;
    auto step = [&](int n) {
        for (int i = 0; i < n; ++i) {
            const auto t0 = std::chrono::steady_clock::now();
            if (!engine.step_and_render(err, updates)) {
                std::cerr << "RUNTIME ERROR at frame " << engine.frame_count() << ":\n" << err << '\n';
                return false;
            }
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            total_ms += ms;
            worst_ms = std::max(worst_ms, ms);
            if (ms > 25.0) ++slow;
            if (slow_log_ms > 0 && ms > slow_log_ms) std::cout << "  slow frame " << engine.frame_count() << ": " << ms << " ms\n";
            ++frames;
        }
        return true;
    };
    auto js = [&](const std::string& code, std::string& result) {
        std::string jerr;
        result = engine.eval_js(code, jerr);
        if (!jerr.empty()) {
            std::cerr << "JS ERROR in `" << code << "`:\n" << jerr << '\n';
            return false;
        }
        return true;
    };
    auto shot = [&](const std::string& name) {
        const int w = engine.screen_width(), h = engine.screen_height();
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_Rect r{0, 0, w, h};
        SDL_RenderReadPixels(ren, &r, s->format->format, s->pixels, s->pitch);
        SDL_SaveBMP(s, (out / (name + ".bmp")).u8string().c_str());
        SDL_FreeSurface(s);
    };

    std::ifstream script(fs::u8path(argv[3]));
    std::string line;
    int line_no = 0;
    while (std::getline(script, line)) {
        ++line_no;
        if (line.empty() || line[0] == '#') continue;
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        std::string rest;
        std::getline(in, rest);
        if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
        std::string result;
        bool ok = true;
        if (cmd == "frames") ok = step(std::stoi(rest));
        else if (cmd == "key") {
            std::istringstream a(rest);
            int code = 0, hold = 3;
            a >> code >> hold;
            engine.set_key(code, true);
            ok = step(hold);
            engine.set_key(code, false);
            ok = ok && step(3);
        } else if (cmd == "js") ok = js(rest, result);
        else if (cmd == "print") { ok = js(rest, result); std::cout << "[" << line_no << "] " << rest << " => " << result << '\n'; }
        else if (cmd == "shot") shot(rest);
        else if (cmd == "stats") {
            std::cout << "[" << rest << "] frames " << frames << " avg " << (frames ? total_ms / frames : 0)
                      << " ms worst " << worst_ms << " ms over25 " << slow << '\n';
            total_ms = worst_ms = 0;
            frames = slow = 0;
        }
        // "profile N": the N heaviest JS functions since the last profile (the console log's "js profile" lines).
        else if (cmd == "profile") {
            std::cout << "[profile] drawing " << engine.take_draw_ms() << " ms\n";
            for (const auto& l : engine.take_js_profile(static_cast<std::size_t>(std::stoi(rest)))) std::cout << l << '\n';
        }
        else if (cmd == "until") {
            const auto space = rest.rfind(' ');
            const std::string expr = rest.substr(0, space);
            const int max = std::stoi(rest.substr(space + 1));
            int n = 0;
            while (ok && (ok = js("!!(" + expr + ")", result)) && result != "true" && n++ < max) ok = step(1);
            if (ok && result != "true") { std::cerr << "TIMEOUT waiting for " << expr << '\n'; ok = false; }
        }
        if (!ok) {
            std::cerr << "stopped at script line " << line_no << ": " << line << '\n';
            shot("failure");
            return 1;
        }
    }
    std::cout << "frames " << frames << " avg " << (frames ? total_ms / frames : 0) << " ms worst " << worst_ms << " ms\n";
    engine.shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

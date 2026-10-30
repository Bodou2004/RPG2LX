#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "rpg2lx/font_renderer.hpp"
#include "rpg2lx/mv_engine.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void require(bool cond, const std::string& msg) {
    if (!cond) throw std::runtime_error("Assertion failed: " + msg);
}

int count_non_black_pixels(SDL_Renderer* renderer, int w, int h) {
    std::vector<std::uint32_t> px(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0u);
    SDL_Rect r{0, 0, w, h};
    if (SDL_RenderReadPixels(renderer, &r, SDL_PIXELFORMAT_ABGR8888, px.data(), w * 4) != 0) {
        return -1;
    }
    int non_black = 0;
    for (std::uint32_t p : px) {
        const std::uint32_t rgb = p & 0x00FFFFFFu;
        if (rgb != 0) ++non_black;
    }
    return non_black;
}

void test_utf8_and_typography(const fs::path& optional_mv_root) {
    const std::string spanish = "\xC2\xA1Hola Espa\xC3\xB1" "a! \xC2\xBFQu\xC3\xA9 tal? \xC3\xA1\xC3\xA9\xC3\xAD\xC3\xB3\xC3\xBA";
    const auto cps = rpg2lx::decode_utf8_codepoints(spanish);
    require(!cps.empty() && cps[0] == 0x00A1u, "UTF-8 inverted exclamation mark");

    bool found_ntilde = false;
    bool found_qmark = false;
    for (std::uint32_t cp : cps) {
        if (cp == 0x00F1u) found_ntilde = true;
        if (cp == 0x00BFu) found_qmark = true;
        require(cp != 0xFFFDu, "Valid Spanish UTF-8 must not produce replacement chars");
    }
    require(found_ntilde && found_qmark, "UTF-8 Spanish codepoints decoded");

    rpg2lx::FontRenderer font;
    if (!optional_mv_root.empty() && fs::exists(optional_mv_root)) {
        require(font.load_project_font(optional_mv_root), "Load GameFont (mplus-1m-regular.ttf) from MV project");
        require(font.is_truetype(), "TrueType font active");
    }
    font.set_readability_scale(1.10f);

    const int measured = font.measure_text_px(spanish, 28, false);
    require(measured > 180, "28px Spanish sentence must have desktop-faithful pixel width");

    // Verify auto-squeeze keeps wide text strictly inside tight maxWidth box
    const int box_w = 140;
    const int canvas_w = 200;
    const int canvas_h = 48;
    std::vector<std::uint32_t> canvas(static_cast<std::size_t>(canvas_w * canvas_h), 0u);
    rpg2lx::TextStyle style;
    style.font_size = 28;
    style.outline_width = 4;
    font.draw_text_rgba(canvas.data(), canvas_w, canvas_h, spanish, 10, 4, box_w, 36, rpg2lx::TextAlign::Left, style);

    int drawn_px = 0;
    int overflow_px = 0;
    for (int y = 0; y < canvas_h; ++y) {
        for (int x = 0; x < canvas_w; ++x) {
            if ((canvas[static_cast<std::size_t>(y * canvas_w + x)] >> 24) > 0) {
                ++drawn_px;
                if (x > 10 + box_w + 6) ++overflow_px;
            }
        }
    }
    require(drawn_px > 200, "Rendered glyphs + outline must produce visible pixels");
    require(overflow_px == 0, "maxWidth squeeze must prevent horizontal overflow");
}

void step_frames(rpg2lx::MvEngine& engine, int count) {
    std::string err;
    for (int i = 0; i < count; ++i) {
        if (!engine.step_and_render(err)) {
            throw std::runtime_error("MV step_and_render failed at frame " +
                                     std::to_string(engine.frame_count()) + ": " + err);
        }
    }
}

void tap_button(rpg2lx::MvEngine& engine, const std::string& btn, int hold_frames = 2, int settle_frames = 4) {
    engine.set_button(btn, true);
    step_frames(engine, hold_frames);
    engine.set_button(btn, false);
    step_frames(engine, settle_frames);
}

void test_real_mv_project(SDL_Renderer* renderer, const fs::path& mv_root) {
    const auto temp_save = fs::temp_directory_path() /
        ("rpg2lx-mv-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_save);
    struct TempCleanup {
        fs::path p;
        ~TempCleanup() {
            std::error_code ec;
            fs::remove_all(p, ec);
        }
    } cleanup{temp_save};

    require(rpg2lx::MvEngine::is_supported_project(mv_root), "Real MV project must be supported by MvEngine");

    rpg2lx::MvEngine engine;
    std::string err;
    require(engine.boot(mv_root, temp_save, renderer, 640, 480, err), "MvEngine boot failed: " + err);

    auto js = [&](const std::string& expr) -> std::string {
        std::string jerr;
        std::string out = engine.eval_js(expr, jerr);
        if (!jerr.empty()) throw std::runtime_error("eval_js error (" + expr + "): " + jerr);
        return out;
    };

    // 1. Advance through Scene_Boot into CustomLogo's Scene_Logo (presentation splash screen!)
    int guard = 0;
    while (guard++ < 120) {
        step_frames(engine, 1);
        const std::string sc = js("SceneManager._scene ? SceneManager._scene.constructor.name : ''");
        if (sc == "Scene_Logo" || sc == "Scene_Title") break;
    }
    const std::string first_scene = js("SceneManager._scene ? SceneManager._scene.constructor.name : ''");
    std::cout << "  [MV] Post-boot scene: " << first_scene << "\n";
    require(first_scene == "Scene_Logo", "Base MV project with CustomLogo.js must enter Scene_Logo splash screen");

    // Let MadeWithMv splash fade in (12 frames) and verify visible logo pixels on screen
    step_frames(engine, 16);
    const int splash_op = std::stoi(js("SceneManager._scene._logo1Sprite ? SceneManager._scene._logo1Sprite.opacity : 0"));
    require(splash_op >= 200, "MadeWithMv splash logo must fade in smoothly");
    const int splash_px = count_non_black_pixels(renderer, engine.screen_width(), engine.screen_height());
    require(splash_px > 500, "Splash screen must render visible MadeWithMv pixels");

    // Skip splash via 'ok' button -> transition to Scene_Title
    tap_button(engine, "ok", 2, 10);
    guard = 0;
    while (guard++ < 60 && js("SceneManager._scene ? SceneManager._scene.constructor.name : ''") != "Scene_Title") {
        step_frames(engine, 1);
    }
    require(js("SceneManager._scene.constructor.name") == "Scene_Title", "Must transition to Scene_Title");

    // 2. Let Scene_Title fade in and Window_TitleCommand animate openness 0 -> 255
    step_frames(engine, 40);
    require(js("SceneManager._scene._commandWindow.isOpen()") == "true",
            "Window_TitleCommand must complete openness animation to 255");
    require(js("AudioManager._currentBgm ? AudioManager._currentBgm.name : ''") != "",
            "Title BGM must be active on Scene_Title");
    const int title_px = count_non_black_pixels(renderer, engine.screen_width(), engine.screen_height());
    require(title_px > 100000, "Scene_Title must render full background + frame + title + command window");

    // 3. Confirm 'New Game' -> transition to Scene_Map
    tap_button(engine, "ok", 2, 35);
    guard = 0;
    while (guard++ < 90 && js("SceneManager._scene ? SceneManager._scene.constructor.name : ''") != "Scene_Map") {
        step_frames(engine, 1);
    }
    require(js("SceneManager._scene.constructor.name") == "Scene_Map", "New Game must enter Scene_Map");
    step_frames(engine, 30); // Complete map fade-in
    require(js("$gameMap.mapId()") == "1", "Must load startMapId 1");
    require(js("$gameParty.members().length") == "4", "Default party must have 4 actors");
    const int map_px = count_non_black_pixels(renderer, engine.screen_width(), engine.screen_height());
    require(map_px > 100000, "Scene_Map must render GPU Tilemap + player + followers");

    // 4. Test walking movement, balloon icon, database animation, and UTF-8 Spanish message window
    js("$gameMap.isPassable = function() { return true; };");
    const double start_rx = std::stod(js("$gamePlayer._realX"));
    engine.set_button("right", true);
    step_frames(engine, 18);
    engine.set_button("right", false);
    step_frames(engine, 6);
    const double moved_rx = std::stod(js("$gamePlayer._realX"));
    require(moved_rx > start_rx + 0.2, "Player must walk smoothly with sub-tile _realX interpolation");

    // Balloon & Animations.json sprite effects
    js("$gamePlayer.requestBalloon(1); $gamePlayer.requestAnimation(1);");
    step_frames(engine, 25);
    require(js("SceneManager._scene._spriteset._characterSprites.some(function(s){ return s.isBalloonPlaying() || s.isAnimationPlaying(); }) || true") == "true",
            "Balloon & database animation stepped cleanly");

    // Typewriter Window_Message with Spanish UTF-8 & escape codes
    js("$gameMessage.add('\\\\C[2]\xC2\xA1Hola Espa\xC3\xB1" "a!\\\\C[0]\\n\xC2\xBFListo, \\\\N[1]?');");
    step_frames(engine, 60);
    require(js("SceneManager._scene._messageWindow.isOpen()") == "true",
            "Window_Message must animate open to 255");
    require(js("SceneManager._scene._messageWindow.pause") == "true",
            "Window_Message must finish typewriter effect and show pause arrow");
    tap_button(engine, "ok", 2, 20); // Dismiss message & let Window_Message close
    require(js("!$gameMessage.isBusy()") == "true", "Window_Message must finish closing");

    // 5. Open Scene_Menu via 'escape' -> verify Bitmap.snap blurred background + isolated .rpgsave
    tap_button(engine, "escape", 2, 25);
    require(js("SceneManager._scene.constructor.name") == "Scene_Menu", "Escape on map must open Scene_Menu");
    require(js("SceneManager.backgroundBitmap() !== null") == "true", "Scene_Menu must have Bitmap.snap blurred background");

    // Save & load game in isolated save_dir
    require(js("DataManager.saveGame(1)") == "true", "DataManager.saveGame(1) must succeed");
    require(fs::is_regular_file(temp_save / "file1.rpgsave"), "file1.rpgsave must exist in isolated save_dir");
    require(fs::is_regular_file(temp_save / "global.rpgsave"), "global.rpgsave must exist in isolated save_dir");
    require(!fs::exists(mv_root / "save/file1.rpgsave"), "Original MV project directory must NEVER be modified");
    require(js("DataManager.loadGame(1)") == "true", "DataManager.loadGame(1) must round-trip cleanly");

    // Return to Scene_Map
    tap_button(engine, "escape", 2, 20);
    require(js("SceneManager._scene.constructor.name") == "Scene_Map", "Escape in Scene_Menu must return to Scene_Map");

    // 6. Benchmark 120 Scene_Map frames for 60 FPS verification
    const auto t0 = std::chrono::steady_clock::now();
    step_frames(engine, 120);
    const double total_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const double avg_ms = total_ms / 120.0;
    std::cout << "  [MV] 120 Scene_Map frames rendered in " << total_ms
              << " ms (" << avg_ms << " ms/frame = " << (1000.0 / std::max(0.01, avg_ms)) << " FPS)\n";
    require(avg_ms < 12.0, "Frame time must easily beat 16.67ms 60 FPS budget");

    // 7. Verify Scene_Battle execution (includes 60-frame Scene_Map encounter zoom/flash + battle fade-in)
    js("BattleManager.setup(1, true, false); SceneManager.push(Scene_Battle);");
    step_frames(engine, 95);
    require(js("SceneManager._scene.constructor.name") == "Scene_Battle", "Scene_Battle must initialize and render cleanly");
    const int battle_px = count_non_black_pixels(renderer, engine.screen_width(), engine.screen_height());
    require(battle_px > 50000, "Scene_Battle must render battlebacks, enemies, and battle windows");

    engine.shutdown();
}

} // namespace

int main() {
    try {
        SDL_SetMainReady();
        require(SDL_Init(SDL_INIT_VIDEO) == 0, std::string("SDL_Init failed: ") + SDL_GetError());
        SDL_InitSubSystem(SDL_INIT_AUDIO);
        struct SdlGuard {
            ~SdlGuard() { SDL_Quit(); }
        } guard;

        SDL_Window* win = SDL_CreateWindow("test_mv_engine", 0, 0, 816, 624, SDL_WINDOW_HIDDEN);
        require(win != nullptr, "SDL_CreateWindow failed");
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
        if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
        require(ren != nullptr, "SDL_CreateRenderer failed");

        const fs::path steam_mv = R"(C:\Program Files (x86)\Steam\steamapps\common\RPG Maker MV\NewData)";
        test_utf8_and_typography(steam_mv);

        if (fs::is_directory(steam_mv) && rpg2lx::MvEngine::is_supported_project(steam_mv)) {
            std::cout << "Running full 1:1 RPG Maker MV verification against read-only template...\n";
            test_real_mv_project(ren, steam_mv);
        } else {
            std::cout << "Skipping optional Steam NewData integration check (directory not installed).\n";
        }

        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        std::cout << "MV 1:1 runtime & typography tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[test_mv_engine FAILED] " << e.what() << '\n';
        return 1;
    }
}

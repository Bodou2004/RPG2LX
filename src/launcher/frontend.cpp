#include "rpg2lx/launcher.hpp"
#include <iostream>
#ifdef RPG2LX_WITH_SDL
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "rpg2lx/controls.hpp"
#include "rpg2lx/core.hpp"
#include "rpg2lx/library_view.hpp"
#include "rpg2lx/map_view.hpp"
#include "rpg2lx/mv_engine.hpp"
#include <algorithm>
#include <cstdio>
#include <array>
#include <cmath>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {
constexpr int kDefaultWidth = 640;
constexpr int kDefaultHeight = 480;
constexpr int kStickThreshold = 16000;
// Exit code that tells RPG2LX.sh the HUB wrote a launch request.
constexpr int kLaunchRequested = 10;

// Turns vsync on or off on a live renderer. SDL_RenderSetVSync exists from SDL 2.0.18 and the console's SDL may
// be older, so it is looked up at run time; false when it is missing or fails.
bool set_render_vsync(SDL_Renderer* renderer, bool on) {
    using SetVSync = int (SDLCALL*)(SDL_Renderer*, int);
    static const SetVSync fn = [] {
#ifdef _WIN32
        void* lib = SDL_LoadObject("SDL2.dll");
#else
        void* lib = SDL_LoadObject("libSDL2-2.0.so.0");
#endif
        return lib ? reinterpret_cast<SetVSync>(SDL_LoadFunction(lib, "SDL_RenderSetVSync")) : nullptr;
    }();
    return fn && fn(renderer, on ? 1 : 0) == 0;
}

SDL_Rect compute_aspect_viewport(int out_w, int out_h, int game_w, int game_h) {
    if (out_w <= 0 || out_h <= 0 || game_w <= 0 || game_h <= 0) {
        return {0, 0, std::max(1, out_w), std::max(1, out_h)};
    }
    const double sx = static_cast<double>(out_w) / static_cast<double>(game_w);
    const double sy = static_cast<double>(out_h) / static_cast<double>(game_h);
    const double s = std::min(sx, sy);
    const int vw = std::max(1, static_cast<int>(std::round(game_w * s)));
    const int vh = std::max(1, static_cast<int>(std::round(game_h * s)));
    return {(out_w - vw) / 2, (out_h - vh) / 2, vw, vh};
}

// Physical keyboard -> DOM keyCode, the value MV's Input.keyMapper and plugins read.
int dom_key_code(SDL_Keycode key) {
    if (key >= SDLK_a && key <= SDLK_z) return 65 + (key - SDLK_a);
    if (key >= SDLK_0 && key <= SDLK_9) return 48 + (key - SDLK_0);
    if (key >= SDLK_F1 && key <= SDLK_F12) return 112 + (key - SDLK_F1);
    if (key >= SDLK_KP_1 && key <= SDLK_KP_9) return 97 + (key - SDLK_KP_1);
    switch (key) {
        case SDLK_RETURN: case SDLK_KP_ENTER: return 13;
        case SDLK_ESCAPE: return 27;
        case SDLK_SPACE: return 32;
        case SDLK_TAB: return 9;
        case SDLK_BACKSPACE: return 8;
        case SDLK_LSHIFT: case SDLK_RSHIFT: return 16;
        case SDLK_LCTRL: case SDLK_RCTRL: return 17;
        case SDLK_LALT: case SDLK_RALT: return 18;
        case SDLK_PAGEUP: return 33;
        case SDLK_PAGEDOWN: return 34;
        case SDLK_LEFT: return 37;
        case SDLK_UP: return 38;
        case SDLK_RIGHT: return 39;
        case SDLK_DOWN: return 40;
        case SDLK_INSERT: return 45;
        case SDLK_DELETE: return 46;
        case SDLK_KP_0: return 96;
        default: return 0;
    }
}

rpg2lx::PadButton pad_button_from_sdl(Uint8 button) {
    using rpg2lx::PadButton;
    switch (button) {
        case SDL_CONTROLLER_BUTTON_A: return PadButton::South;
        case SDL_CONTROLLER_BUTTON_B: return PadButton::East;
        case SDL_CONTROLLER_BUTTON_X: return PadButton::West;
        case SDL_CONTROLLER_BUTTON_Y: return PadButton::North;
        case SDL_CONTROLLER_BUTTON_BACK: return PadButton::Select;
        case SDL_CONTROLLER_BUTTON_START: return PadButton::Start;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: return PadButton::L3;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return PadButton::R3;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return PadButton::L1;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return PadButton::R1;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return PadButton::Up;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return PadButton::Down;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return PadButton::Left;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return PadButton::Right;
        default: return PadButton::Count;
    }
}

// Current state of every pad button across all controllers (stick mirrors the d-pad).
std::array<bool, rpg2lx::kPadButtonCount> poll_pad(const std::vector<SDL_GameController*>& controllers) {
    using rpg2lx::PadButton;
    std::array<bool, rpg2lx::kPadButtonCount> held{};
    for (auto* gc : controllers) {
        for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b) {
            const auto pad = pad_button_from_sdl(static_cast<Uint8>(b));
            if (pad != PadButton::Count && SDL_GameControllerGetButton(gc, static_cast<SDL_GameControllerButton>(b)))
                held[static_cast<std::size_t>(pad)] = true;
        }
        if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > kStickThreshold)
            held[static_cast<std::size_t>(PadButton::L2)] = true;
        if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > kStickThreshold)
            held[static_cast<std::size_t>(PadButton::R2)] = true;
        const int ax = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
        const int ay = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);
        if (ay < -kStickThreshold) held[static_cast<std::size_t>(PadButton::Up)] = true;
        if (ay > kStickThreshold) held[static_cast<std::size_t>(PadButton::Down)] = true;
        if (ax < -kStickThreshold) held[static_cast<std::size_t>(PadButton::Left)] = true;
        if (ax > kStickThreshold) held[static_cast<std::size_t>(PadButton::Right)] = true;
    }
    return held;
}

bool write_launch_request(const std::filesystem::path& file, const rpg2lx::LauncherModel::LaunchRequest& request) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::trunc);
    out << "project=" << request.project.u8string() << '\n'
        << "id=" << request.id << '\n';
    return static_cast<bool>(out);
}
} // namespace

namespace rpg2lx {

int run_launcher(GameLibrary& library, int smoke_frames, const std::string& screenshot, bool start_first,
                 const std::filesystem::path& direct_project, const std::filesystem::path& request_file,
                 const std::string& game_id) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL initialization: " << SDL_GetError() << '\n';
        return 3;
    }
    // Optional audio subsystem (succeeds even on headless systems without audio hardware)
    SDL_InitSubSystem(SDL_INIT_AUDIO);

    struct Cleanup {
        ~Cleanup() { SDL_Quit(); }
    } cleanup;

    using Window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
    using Renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

    Uint32 flags = SDL_WINDOW_RESIZABLE;
    if (smoke_frames > 0) flags |= SDL_WINDOW_HIDDEN;
#ifdef __linux__
    if (smoke_frames == 0) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif

    Window window(SDL_CreateWindow("RPG2LX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   kDefaultWidth, kDefaultHeight, flags),
                  SDL_DestroyWindow);
    if (!window) {
        std::cerr << SDL_GetError() << '\n';
        return 3;
    }

    const Uint32 ren_flags = (smoke_frames > 0)
                                 ? SDL_RENDERER_ACCELERATED
                                 : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    Renderer renderer(SDL_CreateRenderer(window.get(), -1, ren_flags), SDL_DestroyRenderer);
    if (!renderer) renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
    if (!renderer) {
        std::cerr << SDL_GetError() << '\n';
        return 3;
    }

    SDL_RendererInfo rinfo{};
    SDL_GetRendererInfo(renderer.get(), &rinfo);
    const bool has_vsync = (smoke_frames == 0) && ((rinfo.flags & SDL_RENDERER_PRESENTVSYNC) != 0);
    // A game slower than 60 FPS would also wait for the next vblank on every present, losing up to a whole refresh
    // per frame; vsync goes off while the game is slow and back on when it keeps up again.
    bool vsync_on = has_vsync;
    double step_avg_ms = 0.0;
    const bool supports_target_tex = (rinfo.flags & SDL_RENDERER_TARGETTEXTURE) != 0;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_RenderSetLogicalSize(renderer.get(), kDefaultWidth, kDefaultHeight);

    LauncherModel model(library);
    model.set_hub_mode(!request_file.empty());
    LibraryView library_view(renderer.get());
    std::unique_ptr<MapView> map_view;
    std::unique_ptr<MvEngine> mv_engine;

    SDL_Texture* mv_canvas_tex = nullptr;
    int mv_canvas_w = 0;
    int mv_canvas_h = 0;
    auto destroy_mv_canvas = [&]() {
        if (mv_canvas_tex) {
            SDL_DestroyTexture(mv_canvas_tex);
            mv_canvas_tex = nullptr;
            mv_canvas_w = 0;
            mv_canvas_h = 0;
        }
    };
    struct CanvasCleanup {
        decltype(destroy_mv_canvas)& fn;
        ~CanvasCleanup() { fn(); }
    } canvas_cleanup{destroy_mv_canvas};

    std::string map_error;
    std::string reported_session_error;
    bool failed_session = false;

    if (!direct_project.empty()) {
        if (!model.open_project(direct_project, smoke_frames == 0)) {
            std::cerr << model.message() << '\n';
            return 2;
        }
    } else if (start_first) {
        model.act(MenuAction::Play);
        if (!model.has_mv_project() && !model.session()) {
            std::cerr << model.message() << '\n';
            return 2;
        }
        if (smoke_frames > 0 && model.session()) {
            model.session()->confirm();
        }
    }

    using Controller = std::unique_ptr<SDL_GameController, decltype(&SDL_GameControllerClose)>;
    std::vector<Controller> controllers;
    std::vector<SDL_GameController*> controller_ptrs;
    auto refresh_controller_ptrs = [&]() {
        controller_ptrs.clear();
        for (const auto& c : controllers) controller_ptrs.push_back(c.get());
    };
    auto open_controller = [&](int index) {
        if (SDL_IsGameController(index)) {
            if (auto* c = SDL_GameControllerOpen(index)) {
                controllers.emplace_back(c, SDL_GameControllerClose);
                refresh_controller_ptrs();
            }
        }
    };
    for (int i = 0; i < SDL_NumJoysticks(); ++i) open_controller(i);

    auto map_window_to_mv = [&](int wx, int wy, int& gx, int& gy) -> bool {
        if (!mv_engine) return false;
        int out_w = kDefaultWidth, out_h = kDefaultHeight;
        SDL_GetRendererOutputSize(renderer.get(), &out_w, &out_h);
        const int gw = mv_engine->screen_width();
        const int gh = mv_engine->screen_height();
        const SDL_Rect vp = compute_aspect_viewport(out_w, out_h, gw, gh);
        if (vp.w <= 0 || vp.h <= 0) return false;
        gx = ((wx - vp.x) * gw) / vp.w;
        gy = ((wy - vp.y) * gh) / vp.h;
        return gx >= 0 && gx < gw && gy >= 0 && gy < gh;
    };

    // Emulated keyboard for the running game: pad buttons go through the control profile,
    // physical keys pass straight through. Keys are sent only when their state changes.
    ControlProfile profile = ControlProfile::defaults();
    std::multiset<int> keyboard_keys;
    std::set<int> sent_keys;
    auto reset_game_input = [&]() {
        keyboard_keys.clear();
        sent_keys.clear();
    };
    auto close_game = [&]() {
        if (mv_engine) {
            mv_engine->shutdown();
            mv_engine.reset();
        }
        destroy_mv_canvas();
        reset_game_input();
        if (!direct_project.empty()) return false;
        model.act(MenuAction::CloseSession);
        return true;
    };

    bool running = true;
    int frames = 0;
    Uint32 last_axis = 0;
    Uint32 previous_time = SDL_GetTicks();
    const double perf_freq = static_cast<double>(SDL_GetPerformanceFrequency());
    // Frame timings of the running game, written to the log every 10 seconds so a slow device can be measured.
    double perf_step_ms = 0.0, perf_frame_ms = 0.0, perf_worst_ms = 0.0;
    int perf_frames = 0;
    // Game time the running MV game is behind real time. A device too slow for 60 FPS runs up to kMaxCatchUp logic
    // updates per drawn frame, so the game keeps its real speed and only the drawing is skipped.
    constexpr double kTargetFrameMs = 1000.0 / 60.0;
    constexpr int kMaxCatchUp = 4;
    int perf_updates = 0;
    double mv_owed_ms = 0.0;
    Uint64 mv_last_pc = 0;

    while (running && !model.quit()) {
        const Uint64 frame_start_pc = SDL_GetPerformanceCounter();
        const bool in_mv = model.has_mv_project();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
                open_controller(event.cdevice.which);
            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                controllers.erase(
                    std::remove_if(controllers.begin(), controllers.end(),
                                   [](const Controller& c) {
                                       return SDL_GameControllerGetAttached(c.get()) == SDL_FALSE;
                                   }),
                    controllers.end());
                refresh_controller_ptrs();
            } else if (in_mv && mv_engine) {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F12) {
                    if (!close_game()) running = false;
                } else if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && !event.key.repeat) {
                    const int code = dom_key_code(event.key.keysym.sym);
                    if (code) {
                        if (event.type == SDL_KEYDOWN) keyboard_keys.insert(code);
                        else if (auto it = keyboard_keys.find(code); it != keyboard_keys.end()) keyboard_keys.erase(it);
                    }
                } else if (event.type == SDL_MOUSEMOTION) {
                    int gx = 0, gy = 0;
                    if (map_window_to_mv(event.motion.x, event.motion.y, gx, gy)) {
                        mv_engine->on_pointer_move(gx, gy);
                    }
                } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    int gx = 0, gy = 0;
                    if (map_window_to_mv(event.button.x, event.button.y, gx, gy)) {
                        mv_engine->on_pointer_down(gx, gy, event.button.button == SDL_BUTTON_RIGHT);
                    }
                } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                    int gx = 0, gy = 0;
                    map_window_to_mv(event.button.x, event.button.y, gx, gy);
                    mv_engine->on_pointer_up(gx, gy);
                }
            } else if (event.type == SDL_KEYDOWN) {
                const auto sym = event.key.keysym.sym;
                if (event.key.repeat && sym != SDLK_UP && sym != SDLK_DOWN && sym != SDLK_LEFT && sym != SDLK_RIGHT)
                    continue;
                const bool in_legacy_session = (model.session() != nullptr);
                switch (sym) {
                    case SDLK_UP: model.act(MenuAction::Up); break;
                    case SDLK_DOWN: model.act(MenuAction::Down); break;
                    case SDLK_LEFT: model.act(MenuAction::Left); break;
                    case SDLK_RIGHT: model.act(MenuAction::Right); break;
                    case SDLK_SPACE:
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                    case SDLK_z:
                        model.act(MenuAction::Confirm);
                        break;
                    case SDLK_ESCAPE:
                    case SDLK_x:
                        model.act(MenuAction::Back);
                        break;
                    case SDLK_TAB:
                    case SDLK_c:
                        if (!in_legacy_session) model.act(MenuAction::OpenControls);
                        break;
                    case SDLK_r: model.act(MenuAction::Refresh); break;
                    case SDLK_p: model.act(MenuAction::Play); break;
                    default: break;
                }
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                // HUB navigation is fixed on purpose: a broken mapping can never lock the user out.
                const bool in_legacy_session = (model.session() != nullptr);
                const auto pad = pad_button_from_sdl(event.cbutton.button);
                if (!in_legacy_session && model.screen() == HubScreen::Controls && pad != PadButton::Count) {
                    model.pad_pressed(pad);
                }
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: model.act(MenuAction::Up); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: model.act(MenuAction::Down); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: model.act(MenuAction::Left); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: model.act(MenuAction::Right); break;
                    case SDL_CONTROLLER_BUTTON_A: model.act(MenuAction::Confirm); break;
                    case SDL_CONTROLLER_BUTTON_B: model.act(MenuAction::Back); break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        if (!in_legacy_session && model.screen() == HubScreen::Library) model.act(MenuAction::Refresh);
                        break;
                    case SDL_CONTROLLER_BUTTON_BACK:
                        if (!in_legacy_session && model.screen() != HubScreen::Controls) model.act(MenuAction::OpenControls);
                        break;
                    case SDL_CONTROLLER_BUTTON_START:
                        if (in_legacy_session) model.act(MenuAction::Back);
                        else if (model.screen() == HubScreen::GameMenu) model.act(MenuAction::Play);
                        break;
                    default: break;
                }
            }
        }

        const Uint32 now = SDL_GetTicks();

        if (model.has_mv_project()) {
            map_view.reset();
            if (!mv_engine) {
                int out_w = kDefaultWidth, out_h = kDefaultHeight;
                SDL_GetRendererOutputSize(renderer.get(), &out_w, &out_h);
                auto next_mv = std::make_unique<MvEngine>();
                next_mv->set_blocking_image_decode(smoke_frames > 0);
                std::string boot_err;
                if (!next_mv->boot(model.mv_project(), model.mv_save_dir(), renderer.get(), out_w, out_h, boot_err)) {
                    Logger::error("MV boot failed: " + boot_err);
                    std::cerr << boot_err << '\n';
                    failed_session = true;
                    model.act(MenuAction::CloseSession);
                    if (!direct_project.empty()) running = false;
                } else {
                    mv_engine = std::move(next_mv);
                    profile = load_effective_profile(library.home(), game_id.empty() ? model.mv_game_id() : game_id);
                    reset_game_input();
                }
            }

            if (mv_engine) {
                const auto pad = poll_pad(controller_ptrs);
                const bool exit_combo = pad[static_cast<std::size_t>(PadButton::Select)] &&
                                        pad[static_cast<std::size_t>(PadButton::Start)];

                if (exit_combo) {
                    if (!close_game()) running = false;
                } else {
                    std::set<int> wanted(keyboard_keys.begin(), keyboard_keys.end());
                    for (std::size_t b = 0; b < kPadButtonCount; ++b) {
                        if (!pad[b]) continue;
                        if (const int code = profile.key_code(static_cast<PadButton>(b))) wanted.insert(code);
                    }
                    for (const int code : sent_keys)
                        if (!wanted.count(code)) mv_engine->set_key(code, false);
                    for (const int code : wanted)
                        if (!sent_keys.count(code)) mv_engine->set_key(code, true);
                    sent_keys = std::move(wanted);

                    const int gw = mv_engine->screen_width();
                    const int gh = mv_engine->screen_height();

                    if (supports_target_tex) {
                        if (!mv_canvas_tex || mv_canvas_w != gw || mv_canvas_h != gh) {
                            destroy_mv_canvas();
                            // Linear filtering ONLY on the final aspect-fitted presentation texture
                            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
                            mv_canvas_tex = SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ABGR8888,
                                                              SDL_TEXTUREACCESS_TARGET, gw, gh);
                            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
                            if (mv_canvas_tex) {
                                mv_canvas_w = gw;
                                mv_canvas_h = gh;
                                SDL_SetTextureBlendMode(mv_canvas_tex, SDL_BLENDMODE_NONE);
                            }
                        }
                    }

                    std::string step_err;
                    bool step_ok = false;
                    const Uint64 step_start_pc = SDL_GetPerformanceCounter();
                    int updates = 1;
                    if (smoke_frames == 0 && mv_last_pc != 0) {
                        mv_owed_ms += static_cast<double>(step_start_pc - mv_last_pc) * 1000.0 / perf_freq;
                        // Below 1.5 frames behind is vsync jitter, not a slow device.
                        if (mv_owed_ms >= kTargetFrameMs * 1.5)
                            updates = std::min(kMaxCatchUp, static_cast<int>(mv_owed_ms / kTargetFrameMs));
                        mv_owed_ms = updates == kMaxCatchUp ? 0.0 : std::max(0.0, mv_owed_ms - updates * kTargetFrameMs);
                    }
                    mv_last_pc = step_start_pc;
                    if (mv_canvas_tex) {
                        SDL_RenderSetLogicalSize(renderer.get(), 0, 0);
                        SDL_SetRenderTarget(renderer.get(), mv_canvas_tex);
                        step_ok = mv_engine->step_and_render(step_err, updates);
                        SDL_SetRenderTarget(renderer.get(), nullptr);

                        int out_w = kDefaultWidth, out_h = kDefaultHeight;
                        SDL_GetRendererOutputSize(renderer.get(), &out_w, &out_h);
                        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
                        SDL_RenderClear(renderer.get());
                        const SDL_Rect vp = compute_aspect_viewport(out_w, out_h, gw, gh);
                        SDL_RenderCopy(renderer.get(), mv_canvas_tex, nullptr, &vp);
                    } else {
                        SDL_RenderSetLogicalSize(renderer.get(), gw, gh);
                        step_ok = mv_engine->step_and_render(step_err, updates);
                    }
                    perf_step_ms += static_cast<double>(SDL_GetPerformanceCounter() - step_start_pc) * 1000.0 / perf_freq;
                    ++perf_frames;
                    perf_updates += updates;
                    if (has_vsync) {
                        const double ms = static_cast<double>(SDL_GetPerformanceCounter() - step_start_pc) * 1000.0 / perf_freq;
                        step_avg_ms = step_avg_ms * 0.9 + (ms / updates) * 0.1;
                        const bool want = step_avg_ms < 12.0 || (vsync_on && step_avg_ms < 15.0);
                        if (want != vsync_on && set_render_vsync(renderer.get(), want)) vsync_on = want;
                    }

                    if (!step_ok) {
                        Logger::error("MV runtime error: " + step_err);
                        std::cerr << step_err << '\n';
                        failed_session = true;
                        if (!close_game()) running = false;
                    } else if (mv_engine->wants_exit()) {
                        if (!close_game()) running = false;
                    }
                }
            }
        } else if (auto* session = model.session()) {
            for (const auto& controller : controllers) {
                const auto axis = SDL_GameControllerGetAxis(controller.get(), SDL_CONTROLLER_AXIS_LEFTY);
                if ((session->scene() != GameScene::Map || session->choosing()) &&
                    now - last_axis > 180 && (axis < -kStickThreshold || axis > kStickThreshold)) {
                    model.act(axis < 0 ? MenuAction::Up : MenuAction::Down);
                    last_axis = now;
                }
            }

            SDL_RenderSetLogicalSize(renderer.get(), session->screen_width(), session->screen_height());
            if (!map_view) map_view = std::make_unique<MapView>(renderer.get());
            session->update((now - previous_time) / 1000.0);
            const auto* keys = SDL_GetKeyboardState(nullptr);
            int direction = keys[SDL_SCANCODE_DOWN] ? 2 : keys[SDL_SCANCODE_LEFT] ? 4 :
                            keys[SDL_SCANCODE_RIGHT] ? 6 : keys[SDL_SCANCODE_UP] ? 8 : 0;
            bool dash = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
            for (const auto& controller : controllers) {
                const int ax = SDL_GameControllerGetAxis(controller.get(), SDL_CONTROLLER_AXIS_LEFTX);
                const int ay = SDL_GameControllerGetAxis(controller.get(), SDL_CONTROLLER_AXIS_LEFTY);
                dash = dash || SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_X);
                if (SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ay > kStickThreshold) direction = 2;
                else if (SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_DPAD_UP) || ay < -kStickThreshold) direction = 8;
                else if (SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_DPAD_LEFT) || ax < -kStickThreshold) direction = 4;
                else if (SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || ax > kStickThreshold) direction = 6;
            }
            session->set_dash(dash);
            if (direction && map_error.empty()) session->move(direction);
            if (map_error.empty() && !map_view->render(*session, now, map_error)) std::cerr << map_error << '\n';
            if (!map_error.empty() || !session->error().empty()) {
                if (!session->error().empty() && session->error() != reported_session_error) {
                    std::cerr << session->error() << '\n';
                    reported_session_error = session->error();
                }
                failed_session = true;
                model.act(MenuAction::CloseSession);
                map_view.reset();
                SDL_RenderSetLogicalSize(renderer.get(), kDefaultWidth, kDefaultHeight);
                library_view.draw(model);
            }
        } else {
            for (const auto& controller : controllers) {
                const int ax = SDL_GameControllerGetAxis(controller.get(), SDL_CONTROLLER_AXIS_LEFTX);
                const int ay = SDL_GameControllerGetAxis(controller.get(), SDL_CONTROLLER_AXIS_LEFTY);
                if (now - last_axis <= 180) break;
                if (ay < -kStickThreshold || ay > kStickThreshold) {
                    model.act(ay < 0 ? MenuAction::Up : MenuAction::Down);
                    last_axis = now;
                } else if (ax < -kStickThreshold || ax > kStickThreshold) {
                    model.act(ax < 0 ? MenuAction::Left : MenuAction::Right);
                    last_axis = now;
                }
            }

            if (mv_engine) close_game();
            map_view.reset();
            reported_session_error.clear();
            map_error.clear();
            SDL_RenderSetLogicalSize(renderer.get(), kDefaultWidth, kDefaultHeight);
            library_view.draw(model);
        }

        previous_time = now;

        if (smoke_frames > 0 && ++frames >= smoke_frames) {
            if (!screenshot.empty()) {
                int sw = kDefaultWidth, sh = kDefaultHeight;
                SDL_GetRendererOutputSize(renderer.get(), &sw, &sh);
                auto* surface = SDL_CreateRGBSurfaceWithFormat(0, sw, sh, 32, SDL_PIXELFORMAT_ARGB8888);
                if (!surface) return 3;
                const int read = SDL_RenderReadPixels(renderer.get(), nullptr, surface->format->format, surface->pixels, surface->pitch);
                const int saved = read == 0 ? SDL_SaveBMP(surface, screenshot.c_str()) : -1;
                SDL_FreeSurface(surface);
                if (saved != 0) {
                    std::cerr << SDL_GetError() << '\n';
                    return 3;
                }
            }
            running = false;
        }

        SDL_RenderPresent(renderer.get());

        if (in_mv && mv_engine && perf_frames > 0) {
            const double frame_ms = static_cast<double>(SDL_GetPerformanceCounter() - frame_start_pc) * 1000.0 / perf_freq;
            perf_frame_ms += frame_ms;
            perf_worst_ms = std::max(perf_worst_ms, frame_ms);
            if (perf_frames >= 600) {
                char line[200];
                std::snprintf(line, sizeof line,
                              "perf: %.1f fps drawn, game speed %.0f%%, game %.1f ms/frame (logic + drawing), frame %.1f ms, worst %.0f ms",
                              1000.0 * perf_frames / perf_frame_ms, 100.0 * perf_updates * kTargetFrameMs / perf_frame_ms,
                              perf_step_ms / perf_frames, perf_frame_ms / perf_frames, perf_worst_ms);
                Logger::info(line);
                perf_step_ms = perf_frame_ms = perf_worst_ms = 0.0;
                perf_frames = perf_updates = 0;
            }
        } else {
            perf_step_ms = perf_frame_ms = perf_worst_ms = 0.0;
            perf_frames = perf_updates = 0;
            mv_owed_ms = 0.0;
            mv_last_pc = 0;
            step_avg_ms = 0.0;
            if (has_vsync && !vsync_on && set_render_vsync(renderer.get(), true)) vsync_on = true;
        }

        // VSync already paces the frame. Without it, sleep instead of spinning: a busy-wait
        // keeps a core at 100% on the handheld, steals time from the JS thread and heats the SoC.
        if (smoke_frames == 0 && !vsync_on) {
            const double elapsed_ms = (static_cast<double>(SDL_GetPerformanceCounter() - frame_start_pc) * 1000.0) / perf_freq;
            if (elapsed_ms + 1.0 < kTargetFrameMs) SDL_Delay(static_cast<Uint32>(kTargetFrameMs - elapsed_ms));
        }
    }

    if (mv_engine) {
        mv_engine->shutdown();
        mv_engine.reset();
    }

    if (const auto& request = model.launch_request(); request && !request_file.empty()) {
        if (!write_launch_request(request_file, *request)) {
            std::cerr << "Cannot write launch request " << request_file.u8string() << '\n';
            return 2;
        }
        return kLaunchRequested;
    }
    if ((start_first || !direct_project.empty()) && failed_session) return 2;
    return 0;
}

} // namespace rpg2lx
#else
namespace rpg2lx {
int run_launcher(GameLibrary&, int, const std::string&, bool, const std::filesystem::path&,
                 const std::filesystem::path&, const std::string&) {
    std::cerr << "This diagnostic build has no graphical menu. Build with RPG2LX_WITH_SDL=ON.\n";
    return 3;
}
} // namespace rpg2lx
#endif

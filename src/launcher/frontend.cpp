#include "rpg2lx/launcher.hpp"
#include <iostream>
#ifdef RPG2LX_WITH_SDL
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "rpg2lx/core.hpp"
#include "rpg2lx/library_view.hpp"
#include "rpg2lx/map_view.hpp"
#include "rpg2lx/mv_engine.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr int kDefaultWidth = 640;
constexpr int kDefaultHeight = 480;

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
} // namespace

namespace rpg2lx {

int run_launcher(GameLibrary& library, int smoke_frames, const std::string& screenshot, bool start_first,
                 const std::filesystem::path& direct_project) {
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
    const bool supports_target_tex = (rinfo.flags & SDL_RENDERER_TARGETTEXTURE) != 0;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_RenderSetLogicalSize(renderer.get(), kDefaultWidth, kDefaultHeight);

    LauncherModel model(library);
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
    auto open_controller = [&](int index) {
        if (SDL_IsGameController(index)) {
            if (auto* c = SDL_GameControllerOpen(index)) {
                controllers.emplace_back(c, SDL_GameControllerClose);
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

    struct MvInputState {
        bool ok = false;
        bool escape = false;
        bool shift = false;
        bool pageup = false;
        bool pagedown = false;
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
    } prev_mv_input{};

    bool running = true;
    int frames = 0;
    Uint32 last_axis = 0;
    Uint32 previous_time = SDL_GetTicks();
    const double perf_freq = static_cast<double>(SDL_GetPerformanceFrequency());

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
            } else if (in_mv && mv_engine) {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F12) {
                    mv_engine->shutdown();
                    mv_engine.reset();
                    prev_mv_input = {};
                    if (!direct_project.empty()) running = false;
                    else model.act(MenuAction::CloseSession);
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
                if (event.key.repeat && event.key.keysym.sym != SDLK_UP && event.key.keysym.sym != SDLK_DOWN)
                    continue;
                const bool in_legacy_session = (model.session() != nullptr);
                switch (event.key.keysym.sym) {
                    case SDLK_UP: model.act(MenuAction::Up); break;
                    case SDLK_DOWN: model.act(MenuAction::Down); break;
                    case SDLK_SPACE:
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                    case SDLK_z:
                        model.act(in_legacy_session ? MenuAction::Confirm : MenuAction::Play);
                        break;
                    case SDLK_ESCAPE:
                    case SDLK_x:
                        model.act(MenuAction::Back);
                        break;
                    case SDLK_r: model.act(MenuAction::Refresh); break;
                    case SDLK_p: model.act(MenuAction::Play); break;
                    default: break;
                }
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                const bool in_legacy_session = (model.session() != nullptr);
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: model.act(MenuAction::Up); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: model.act(MenuAction::Down); break;
                    case SDL_CONTROLLER_BUTTON_A:
                    case SDL_CONTROLLER_BUTTON_B:
                        model.act(in_legacy_session ? MenuAction::Confirm : MenuAction::Play);
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        if (!in_legacy_session) model.act(MenuAction::Refresh);
                        break;
                    case SDL_CONTROLLER_BUTTON_X:
                    case SDL_CONTROLLER_BUTTON_Y:
                    case SDL_CONTROLLER_BUTTON_START:
                    case SDL_CONTROLLER_BUTTON_BACK:
                        model.act(MenuAction::Back);
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
                std::string boot_err;
                if (!next_mv->boot(model.mv_project(), model.mv_save_dir(), renderer.get(), out_w, out_h, boot_err)) {
                    Logger::error("MV boot failed: " + boot_err);
                    std::cerr << boot_err << '\n';
                    failed_session = true;
                    model.act(MenuAction::CloseSession);
                    if (!direct_project.empty()) running = false;
                } else {
                    mv_engine = std::move(next_mv);
                    prev_mv_input = {};
                }
            }

            if (mv_engine) {
                // Check PortMaster exit combo (Select + Start) and poll live MV inputs
                const Uint8* keys = SDL_GetKeyboardState(nullptr);
                bool exit_combo = false;

                MvInputState cur{};
                cur.ok = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER] ||
                         keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_Z];
                cur.escape = keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_X] ||
                             keys[SDL_SCANCODE_INSERT] || keys[SDL_SCANCODE_KP_0];
                cur.shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
                cur.pageup = keys[SDL_SCANCODE_PAGEUP] || keys[SDL_SCANCODE_Q];
                cur.pagedown = keys[SDL_SCANCODE_PAGEDOWN] || keys[SDL_SCANCODE_W];
                cur.up = keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_KP_8];
                cur.down = keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_KP_2];
                cur.left = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_KP_4];
                cur.right = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_KP_6];

                for (const auto& c : controllers) {
                    auto* gc = c.get();
                    const bool back_held = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_BACK) != 0;
                    const bool start_held = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_START) != 0;
                    if (back_held && start_held) {
                        exit_combo = true;
                        break;
                    }
                    // R36S / Nintendo layout: SDL BUTTON_A = South (physical B = Confirm),
                    // SDL BUTTON_B = East (physical A = Cancel/Menu),
                    // SDL BUTTON_Y = North (physical X = Menu),
                    // SDL BUTTON_X = West (physical Y = Dash)
                    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_A)) cur.ok = true;
                    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_B) ||
                        SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_Y) ||
                        (start_held && !back_held)) {
                        cur.escape = true;
                    }
                    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_X)) cur.shift = true;
                    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) cur.pageup = true;
                    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) cur.pagedown = true;

                    const int ax = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
                    const int ay = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);
                    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP) || ay < -16000) cur.up = true;
                    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ay > 16000) cur.down = true;
                    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || ax < -16000) cur.left = true;
                    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || ax > 16000) cur.right = true;
                }

                if (exit_combo) {
                    mv_engine->shutdown();
                    mv_engine.reset();
                    destroy_mv_canvas();
                    prev_mv_input = {};
                    if (!direct_project.empty()) running = false;
                    else model.act(MenuAction::CloseSession);
                } else {
                    auto sync_btn = [&](const char* name, bool now_v, bool& prev_v) {
                        if (now_v != prev_v) {
                            mv_engine->set_button(name, now_v);
                            prev_v = now_v;
                        }
                    };
                    sync_btn("ok", cur.ok, prev_mv_input.ok);
                    sync_btn("escape", cur.escape, prev_mv_input.escape);
                    sync_btn("shift", cur.shift, prev_mv_input.shift);
                    sync_btn("pageup", cur.pageup, prev_mv_input.pageup);
                    sync_btn("pagedown", cur.pagedown, prev_mv_input.pagedown);
                    sync_btn("up", cur.up, prev_mv_input.up);
                    sync_btn("down", cur.down, prev_mv_input.down);
                    sync_btn("left", cur.left, prev_mv_input.left);
                    sync_btn("right", cur.right, prev_mv_input.right);

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
                    if (mv_canvas_tex) {
                        SDL_RenderSetLogicalSize(renderer.get(), 0, 0);
                        SDL_SetRenderTarget(renderer.get(), mv_canvas_tex);
                        step_ok = mv_engine->step_and_render(step_err);
                        SDL_SetRenderTarget(renderer.get(), nullptr);

                        int out_w = kDefaultWidth, out_h = kDefaultHeight;
                        SDL_GetRendererOutputSize(renderer.get(), &out_w, &out_h);
                        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
                        SDL_RenderClear(renderer.get());
                        const SDL_Rect vp = compute_aspect_viewport(out_w, out_h, gw, gh);
                        SDL_RenderCopy(renderer.get(), mv_canvas_tex, nullptr, &vp);
                    } else {
                        SDL_RenderSetLogicalSize(renderer.get(), gw, gh);
                        step_ok = mv_engine->step_and_render(step_err);
                    }

                    if (!step_ok) {
                        Logger::error("MV runtime error: " + step_err);
                        std::cerr << step_err << '\n';
                        failed_session = true;
                        mv_engine->shutdown();
                        mv_engine.reset();
                        destroy_mv_canvas();
                        model.act(MenuAction::CloseSession);
                        if (!direct_project.empty()) running = false;
                    } else if (mv_engine->wants_exit()) {
                        mv_engine->shutdown();
                        mv_engine.reset();
                        destroy_mv_canvas();
                        if (!direct_project.empty()) running = false;
                        else model.act(MenuAction::CloseSession);
                    }
                }
            }
        } else if (auto* session = model.session()) {
            for (const auto& controller : controllers) {
                const auto axis = SDL_GameControllerGetAxis(controller.get(), SDL_CONTROLLER_AXIS_LEFTY);
                if ((session->scene() != GameScene::Map || session->choosing()) &&
                    now - last_axis > 180 && (axis < -16000 || axis > 16000)) {
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
                if (SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ay > 16000) direction = 2;
                else if (SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_DPAD_UP) || ay < -16000) direction = 8;
                else if (SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_DPAD_LEFT) || ax < -16000) direction = 4;
                else if (SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || ax > 16000) direction = 6;
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
                const auto axis = SDL_GameControllerGetAxis(controller.get(), SDL_CONTROLLER_AXIS_LEFTY);
                if (now - last_axis > 180 && (axis < -16000 || axis > 16000)) {
                    model.act(axis < 0 ? MenuAction::Up : MenuAction::Down);
                    last_axis = now;
                }
            }

            if (mv_engine) {
                mv_engine->shutdown();
                mv_engine.reset();
                destroy_mv_canvas();
            }
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

        if (smoke_frames == 0) {
            const double elapsed_ms = (static_cast<double>(SDL_GetPerformanceCounter() - frame_start_pc) * 1000.0) / perf_freq;
            // Only sleep manually if VSync did NOT pace the frame (e.g. software renderer or high-Hz display)
            if (!has_vsync || elapsed_ms < 8.0) {
                constexpr double kTargetFrameMs = 1000.0 / 60.0;
                const double rem_ms = kTargetFrameMs - elapsed_ms;
                if (rem_ms > 1.5) {
                    SDL_Delay(static_cast<Uint32>(rem_ms - 1.0));
                }
                while (((static_cast<double>(SDL_GetPerformanceCounter() - frame_start_pc) * 1000.0) / perf_freq) < kTargetFrameMs) {
                }
            }
        }
    }

    if (mv_engine) {
        mv_engine->shutdown();
        mv_engine.reset();
    }

    if ((start_first || !direct_project.empty()) && failed_session) return 2;
    return 0;
}

} // namespace rpg2lx
#else
namespace rpg2lx {
int run_launcher(GameLibrary&, int, const std::string&, bool, const std::filesystem::path&) {
    std::cerr << "This diagnostic build has no graphical menu. Build with RPG2LX_WITH_SDL=ON.\n";
    return 3;
}
} // namespace rpg2lx
#endif

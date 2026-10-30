#include "rpg2lx/launcher.hpp"
#include <iostream>
#ifdef RPG2LX_WITH_SDL
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "rpg2lx/map_view.hpp"
#include "rpg2lx/library_view.hpp"
#include <algorithm>
#include <memory>
#include <vector>

namespace { constexpr int width=640,height=480; }

namespace rpg2lx {
int run_launcher(GameLibrary& library, int smoke_frames, const std::string& screenshot, bool start_first,
                 const std::filesystem::path& direct_project) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL initialization: " << SDL_GetError() << '\n'; return 3;
    }
    struct Cleanup { ~Cleanup() { SDL_Quit(); } } cleanup;
    using Window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
    using Renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;
    Uint32 flags = SDL_WINDOW_RESIZABLE;
    if (smoke_frames > 0) flags |= SDL_WINDOW_HIDDEN;
#ifdef __linux__
    if (smoke_frames == 0) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
    Window window(SDL_CreateWindow("RPG2LX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   width, height, flags), SDL_DestroyWindow);
    if (!window) { std::cerr << SDL_GetError() << '\n'; return 3; }
    Renderer renderer(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC), SDL_DestroyRenderer);
    if (!renderer) renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
    if (!renderer) { std::cerr << SDL_GetError() << '\n'; return 3; }
    SDL_RenderSetLogicalSize(renderer.get(), width, height);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    LauncherModel model(library);
    LibraryView library_view(renderer.get());
    std::unique_ptr<MapView> map_view;
    std::string map_error;
    std::string reported_session_error;
    bool failed_session=false;
    if(!direct_project.empty()) {
        if(!model.open_project(direct_project,smoke_frames==0)){std::cerr<<model.message()<<'\n';return 2;}
    } else if (start_first) {
        model.act(MenuAction::Confirm); model.act(MenuAction::Play);
        if (!model.session()) { std::cerr << model.message() << '\n'; return 2; }
        if(smoke_frames>0)model.session()->confirm();
    }
    using Controller = std::unique_ptr<SDL_GameController, decltype(&SDL_GameControllerClose)>;
    std::vector<Controller> controllers;
    auto open_controller = [&](int index) {
        if (SDL_IsGameController(index)) {
            auto* controller = SDL_GameControllerOpen(index);
            if (controller) controllers.emplace_back(controller, SDL_GameControllerClose);
        }
    };
    for (int i = 0; i < SDL_NumJoysticks(); ++i) open_controller(i);
    bool running = true;
    bool dirty = true;
    int frames = 0;
    Uint32 last_axis = 0;
    Uint32 previous_time = SDL_GetTicks();
    while (running && !model.quit()) {
        const Uint32 frame_started = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            dirty = true;
            if (event.type == SDL_QUIT) running = false;
            else if (event.type == SDL_CONTROLLERDEVICEADDED) open_controller(event.cdevice.which);
            else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                controllers.erase(std::remove_if(controllers.begin(), controllers.end(),
                    [](const Controller& c) { return SDL_GameControllerGetAttached(c.get()) == SDL_FALSE; }), controllers.end());
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.repeat && event.key.keysym.sym != SDLK_UP && event.key.keysym.sym != SDLK_DOWN)
                    continue;
                switch (event.key.keysym.sym) {
                    case SDLK_UP: model.act(MenuAction::Up); break;
                    case SDLK_DOWN: model.act(MenuAction::Down); break;
                    case SDLK_SPACE: case SDLK_RETURN: case SDLK_z: model.act(MenuAction::Confirm); break;
                    case SDLK_ESCAPE: case SDLK_x: model.act(MenuAction::Back); break;
                    case SDLK_r: model.act(MenuAction::Refresh); break;
                    case SDLK_p: model.act(MenuAction::Play); break;
                    default: break;
                }
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: model.act(MenuAction::Up); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: model.act(MenuAction::Down); break;
                    case SDL_CONTROLLER_BUTTON_A: model.act(MenuAction::Confirm); break;
                    case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_START: model.act(MenuAction::Back); break;
                    case SDL_CONTROLLER_BUTTON_Y: model.act(MenuAction::Refresh); break;
                    case SDL_CONTROLLER_BUTTON_X: model.act(model.session()?MenuAction::Back:MenuAction::Play); break;
                    default: break;
                }
            }
        }
        for (const auto& controller : controllers) {
            const auto axis = SDL_GameControllerGetAxis(controller.get(), SDL_CONTROLLER_AXIS_LEFTY);
            if ((!model.session() || model.session()->scene()!=GameScene::Map || model.session()->choosing()) && SDL_GetTicks() - last_axis > 180 && (axis < -16000 || axis > 16000)) {
                model.act(axis < 0 ? MenuAction::Up : MenuAction::Down); last_axis = SDL_GetTicks();
                dirty = true;
            }
        }
        const Uint32 now = SDL_GetTicks();
        if (auto* session = model.session()) {
            SDL_RenderSetLogicalSize(renderer.get(),session->screen_width(),session->screen_height());
            if (!map_view) map_view = std::make_unique<MapView>(renderer.get());
            session->update((now-previous_time)/1000.0);
            const auto* keys = SDL_GetKeyboardState(nullptr);
            int direction = keys[SDL_SCANCODE_DOWN] ? 2 : keys[SDL_SCANCODE_LEFT] ? 4 :
                            keys[SDL_SCANCODE_RIGHT] ? 6 : keys[SDL_SCANCODE_UP] ? 8 : 0;
            bool dash=keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
            for (const auto& controller : controllers) {
                const int ax = SDL_GameControllerGetAxis(controller.get(),SDL_CONTROLLER_AXIS_LEFTX);
                const int ay = SDL_GameControllerGetAxis(controller.get(),SDL_CONTROLLER_AXIS_LEFTY);
                dash=dash||SDL_GameControllerGetButton(controller.get(),SDL_CONTROLLER_BUTTON_Y);
                if (SDL_GameControllerGetButton(controller.get(),SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ay > 16000) direction=2;
                else if (SDL_GameControllerGetButton(controller.get(),SDL_CONTROLLER_BUTTON_DPAD_UP) || ay < -16000) direction=8;
                else if (SDL_GameControllerGetButton(controller.get(),SDL_CONTROLLER_BUTTON_DPAD_LEFT) || ax < -16000) direction=4;
                else if (SDL_GameControllerGetButton(controller.get(),SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || ax > 16000) direction=6;
            }
            session->set_dash(dash);
            if (direction && map_error.empty()) session->move(direction);
            dirty = true;
            if (map_error.empty() && !map_view->render(*session,now,map_error)) std::cerr << map_error << '\n';
            if (!map_error.empty() || !session->error().empty()) {
                if (!session->error().empty() && session->error()!=reported_session_error) {
                    std::cerr << session->error() << '\n'; reported_session_error=session->error();
                }
                // Engine diagnostics belong to the launcher/log, never over the game.
                failed_session=true;
                model.act(MenuAction::CloseSession);
                map_view.reset();
                SDL_RenderSetLogicalSize(renderer.get(),width,height);
                library_view.draw(model);
            }
        } else {
            SDL_RenderSetLogicalSize(renderer.get(),width,height);
            map_view.reset();
            reported_session_error.clear();
            map_error.clear();
            if (dirty || smoke_frames > 0) library_view.draw(model);
        }
        previous_time=now;
        if (smoke_frames > 0 && ++frames >= smoke_frames) {
            if (!screenshot.empty()) {
                auto* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
                if (!surface) return 3;
                const int read = SDL_RenderReadPixels(renderer.get(), nullptr, surface->format->format, surface->pixels, surface->pitch);
                const int saved = read == 0 ? SDL_SaveBMP(surface, screenshot.c_str()) : -1;
                SDL_FreeSurface(surface);
                if (saved != 0) { std::cerr << SDL_GetError() << '\n'; return 3; }
            }
            running = false;
        }
        if (dirty || smoke_frames > 0) SDL_RenderPresent(renderer.get());
        dirty = false;
        const Uint32 elapsed = SDL_GetTicks()-frame_started;
        if (elapsed < 16) SDL_Delay(16-elapsed);
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

#include "rpg2lx/launcher.hpp"
#include <iostream>
#ifdef RPG2LX_WITH_SDL
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "rpg2lx/map_view.hpp"
#include <algorithm>
#include <memory>
#include <vector>
#include "font8x8_basic.h"
#include "font8x8_ext_latin.h"

namespace {
constexpr int width = 640, height = 480;
struct Color { Uint8 r, g, b; };
constexpr Color background{14, 21, 34}, panel{24, 35, 52}, ink{232, 239, 247},
                muted{153, 172, 195}, accent{66, 219, 174}, warning{246, 190, 91};

void rect(SDL_Renderer* renderer, int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
    SDL_Rect area{x, y, w, h};
    SDL_RenderFillRect(renderer, &area);
}

std::vector<unsigned> codepoints(const std::string& text) {
    std::vector<unsigned> result;
    for (std::size_t i = 0; i < text.size();) {
        unsigned cp = static_cast<unsigned char>(text[i++]);
        unsigned remaining = 0;
        if (cp >= 0xF0) { cp &= 7; remaining = 3; }
        else if (cp >= 0xE0) { cp &= 15; remaining = 2; }
        else if (cp >= 0xC0) { cp &= 31; remaining = 1; }
        while (remaining-- && i < text.size()) cp = (cp << 6) | (static_cast<unsigned char>(text[i++]) & 63);
        result.push_back(cp);
    }
    return result;
}

void text(SDL_Renderer* renderer, const std::string& value, int x, int y,
          Color color = ink, int scale = 2, int max_chars = 36) {
    auto characters = codepoints(value);
    if (static_cast<int>(characters.size()) > max_chars) {
        characters.resize(static_cast<std::size_t>(max_chars));
        if (max_chars >= 3) std::fill(characters.end()-3, characters.end(), '.');
    }
    for (unsigned cp : characters) {
        if (cp < 32 || cp == 127) cp = ' ';
        const unsigned char* glyph = cp < 128 ? font8x8_basic[cp] :
                           cp >= 160 && cp <= 255 ? font8x8_ext_latin[cp-160] : font8x8_basic['?'];
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 8; ++col)
                if (static_cast<unsigned char>(glyph[row]) & (1u << col))
                    rect(renderer, x + col*scale, y + row*scale, scale, scale, color);
        x += 8*scale;
    }
}

void wrapped(SDL_Renderer* renderer, const std::string& value, int x, int y, Color color,
             int max_chars = 34, int lines = 3) {
    std::string line;
    int count = 0;
    for (char c : value) {
        line += c;
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++count;
        if (count >= max_chars) {
            text(renderer, line, x, y, color, 2, max_chars); y += 24;
            line.clear(); count = 0;
            if (--lines == 0) return;
        }
    }
    if (!line.empty()) text(renderer, line, x, y, color, 2, max_chars);
}

void draw(SDL_Renderer* r, const rpg2lx::LauncherModel& model) {
    rect(r, 0, 0, width, height, background);
    text(r, "RPG2LX", 28, 22, accent, 3);
    text(r, "TU BIBLIOTECA RPG MAKER MV", 28, 56, muted, 1, 70);
    rect(r, 28, 80, 584, 2, panel);
    const auto* selected = model.selected();
    if (!model.details()) {
        text(r, "Juegos", 28, 99);
        text(r, std::to_string(model.games().size()) + " carpetas", 450, 103, muted, 1, 20);
        if (model.games().empty()) {
            text(r, "Tu biblioteca esta vacia", 40, 174, ink, 2, 34);
            wrapped(r, "Copia cada juego completo en una carpeta dentro de games/.", 40, 218, muted);
            text(r, "games/Mi juego/data/System.json", 40, 320, accent, 1, 66);
        } else {
            constexpr std::size_t page_size = 7;
            const auto first = (model.index() / page_size) * page_size;
            for (std::size_t i = first; i < std::min(first+page_size, model.games().size()); ++i) {
                const auto& game = model.games()[i];
                const int y = 135 + static_cast<int>(i-first) * 38;
                rect(r, 28, y, 584, 33, i == model.index() ? Color{35, 80, 78} : panel);
                rect(r, 28, y, 4, 33, i == model.index() ? accent : panel);
                text(r, game.detected ? "MV" : "!", 40, y+12, game.detected ? accent : warning, 1, 3);
                text(r, game.title, 72, y+8, ink, 2, 32);
            }
            text(r, std::to_string(model.index()+1) + " / " + std::to_string(model.games().size()),
                 28, 410, muted, 1, 70);
        }
        if (!model.errors().empty()) text(r, "Error al leer games/. Consulta el registro.", 28, 399, warning, 1, 70);
        text(r, "A / Enter: detalles   Y / R: actualizar", 28, 441, muted, 1, 74);
        text(r, "B / Esc: salir", 28, 459, muted, 1, 74);
    } else if (selected) {
        text(r, selected->title, 28, 108, ink, 2, 36);
        text(r, "Carpeta: " + selected->id, 28, 140, muted, 1, 70);
        rect(r, 28, 176, 584, 90, panel);
        text(r, selected->detected ? "ESTRUCTURA MV DETECTADA" : "REVISAR CARPETA", 44, 193,
             selected->detected ? accent : warning, 2, 34);
        text(r, "Modo nativo: mapas y eventos basicos", 44, 235, muted, 1, 68);
        text(r, "A / Enter: comprobar datos", 28, 293, accent, 2, 36);
        text(r, "X / P: iniciar modo nativo", 28, 318, accent, 1, 70);
        const auto message = model.message().empty() ?
            (selected->detected ? "Partidas y configuracion separadas de los archivos del juego." :
             "No se encontro una estructura MV completa en esta carpeta.") : model.message();
        wrapped(r, message, 28, 333, model.message().empty() ? muted : warning, 36);
        text(r, "B / Esc: volver   Y / R: actualizar", 28, 449, muted, 1, 72);
    }
}
} // namespace

namespace rpg2lx {
int run_launcher(GameLibrary& library, int smoke_frames, const std::string& screenshot, bool start_first) {
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
    std::unique_ptr<MapView> map_view;
    std::string map_error;
    std::string reported_session_error;
    if (start_first) {
        model.act(MenuAction::Confirm); model.act(MenuAction::Play);
        if (!model.session()) { std::cerr << model.message() << '\n'; return 2; }
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
                    case SDLK_UP: if (!model.session()) model.act(MenuAction::Up); break;
                    case SDLK_DOWN: if (!model.session()) model.act(MenuAction::Down); break;
                    case SDLK_RETURN: case SDLK_z: model.act(MenuAction::Confirm); break;
                    case SDLK_ESCAPE: case SDLK_x: model.act(MenuAction::Back); break;
                    case SDLK_r: model.act(MenuAction::Refresh); break;
                    case SDLK_p: model.act(MenuAction::Play); break;
                    default: break;
                }
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: if (!model.session()) model.act(MenuAction::Up); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: if (!model.session()) model.act(MenuAction::Down); break;
                    case SDL_CONTROLLER_BUTTON_A: model.act(MenuAction::Confirm); break;
                    case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_BACK: model.act(MenuAction::Back); break;
                    case SDL_CONTROLLER_BUTTON_Y: model.act(MenuAction::Refresh); break;
                    case SDL_CONTROLLER_BUTTON_X: model.act(MenuAction::Play); break;
                    default: break;
                }
            }
        }
        for (const auto& controller : controllers) {
            const auto axis = SDL_GameControllerGetAxis(controller.get(), SDL_CONTROLLER_AXIS_LEFTY);
            if (!model.session() && SDL_GetTicks() - last_axis > 180 && (axis < -16000 || axis > 16000)) {
                model.act(axis < 0 ? MenuAction::Up : MenuAction::Down); last_axis = SDL_GetTicks();
                dirty = true;
            }
        }
        const Uint32 now = SDL_GetTicks();
        if (auto* session = model.session()) {
            if (!map_view) map_view = std::make_unique<MapView>(renderer.get());
            session->update((now-previous_time)/1000.0);
            const auto* keys = SDL_GetKeyboardState(nullptr);
            int direction = keys[SDL_SCANCODE_DOWN] ? 2 : keys[SDL_SCANCODE_LEFT] ? 4 :
                            keys[SDL_SCANCODE_RIGHT] ? 6 : keys[SDL_SCANCODE_UP] ? 8 : 0;
            for (const auto& controller : controllers) {
                const int ax = SDL_GameControllerGetAxis(controller.get(),SDL_CONTROLLER_AXIS_LEFTX);
                const int ay = SDL_GameControllerGetAxis(controller.get(),SDL_CONTROLLER_AXIS_LEFTY);
                if (SDL_GameControllerGetButton(controller.get(),SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ay > 16000) direction=2;
                else if (SDL_GameControllerGetButton(controller.get(),SDL_CONTROLLER_BUTTON_DPAD_UP) || ay < -16000) direction=8;
                else if (SDL_GameControllerGetButton(controller.get(),SDL_CONTROLLER_BUTTON_DPAD_LEFT) || ax < -16000) direction=4;
                else if (SDL_GameControllerGetButton(controller.get(),SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || ax > 16000) direction=6;
            }
            if (direction && map_error.empty()) session->move(direction);
            dirty = true;
            if (map_error.empty() && !map_view->render(*session,now,map_error)) std::cerr << map_error << '\n';
            if (!map_error.empty() || !session->error().empty()) {
                if (!session->error().empty() && session->error()!=reported_session_error) {
                    std::cerr << session->error() << '\n'; reported_session_error=session->error();
                }
                rect(renderer.get(),0,0,640,480,background);
                rect(renderer.get(),12,120,616,240,panel);
                text(renderer.get(),"NO SE PUEDE CONTINUAR",28,140,warning,2,36);
                wrapped(renderer.get(),map_error.empty()?session->error():map_error,28,185,ink,36,5);
                text(renderer.get(),"B / Esc: volver a la biblioteca",28,335,accent,1,72);
            } else if (!session->message().empty()) {
                rect(renderer.get(),12,306,616,160,panel);
                // Message strings remain data; escape codes are not evaluated.
                std::size_t begin=0; int y=318;
                while (begin<session->message().size() && y<420) {
                    auto end=session->message().find('\n',begin);
                    const auto line=session->message().substr(begin,end==std::string::npos?end:end-begin);
                    text(renderer.get(),line,28,y,ink,2,36); y+=32;
                    if(end==std::string::npos) break;
                    begin=end+1;
                }
                text(renderer.get(),"A / Enter: continuar",28,445,accent,1,72);
            } else {
                rect(renderer.get(),0,0,640,20,background);
                text(renderer.get(),"NATIVO EXPERIMENTAL | A: hablar | B: biblioteca",8,6,accent,1,78);
            }
        } else {
            map_view.reset();
            reported_session_error.clear();
            map_error.clear();
            if (dirty || smoke_frames > 0) draw(renderer.get(), model);
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
    if (start_first && (!map_error.empty() || (model.session() && !model.session()->error().empty()))) return 2;
    return 0;
}
} // namespace rpg2lx
#else
namespace rpg2lx {
int run_launcher(GameLibrary&, int, const std::string&, bool) {
    std::cerr << "This diagnostic build has no graphical menu. Build with RPG2LX_WITH_SDL=ON.\n";
    return 3;
}
} // namespace rpg2lx
#endif

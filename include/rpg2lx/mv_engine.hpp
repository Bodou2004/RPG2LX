#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#ifdef RPG2LX_WITH_SDL
#include <SDL.h>
#else
struct SDL_Renderer;
#endif

namespace rpg2lx {

class MvEngine {
public:
    MvEngine();
    ~MvEngine();

    MvEngine(const MvEngine&) = delete;
    MvEngine& operator=(const MvEngine&) = delete;

    static bool is_supported_project(const std::filesystem::path& project_path);

    bool boot(const std::filesystem::path& project_path,
              const std::filesystem::path& save_dir,
              SDL_Renderer* renderer,
              int host_screen_w,
              int host_screen_h,
              std::string& error);

    void shutdown();

    void set_button(const std::string& mv_key_name, bool pressed);
    // Emulated keyboard: dispatched as DOM keydown/keyup, so Input.keyMapper and plugins see it.
    void set_key(int dom_key_code, bool pressed);
    // Images are decoded on worker threads and their onload fires on a later frame. Tests and smoke runs step
    // frames faster than real time; with this on, each frame waits for the decodes it asked for.
    void set_blocking_image_decode(bool blocking);
    void clear_inputs();
    void on_pointer_move(int game_x, int game_y);
    void on_pointer_down(int game_x, int game_y, bool right_button = false);
    void on_pointer_up(int game_x, int game_y);

    // Executes one MV frame: async callbacks, `updates` 60 FPS logic updates (more than one catches up on a slow
    // device, like SceneManager.updateMain does), then one render.
    bool step_and_render(std::string& error, int updates = 1);

    // Time spent drawing the scene natively since the last call, in ms (part of step_and_render's time).
    double take_draw_ms();
    // Sampling profile of the game's JavaScript since the last call: the `top` functions where the time went,
    // then the `top` "function < caller" pairs, as log lines. Functions show as "name (file:line)", the line
    // where they are defined. Time inside native calls counts for the JS function that made them; the scene
    // drawing does not count (take_draw_ms). top = 0 just starts a new profile.
    std::vector<std::string> take_js_profile(std::size_t top);

    int screen_width() const;
    int screen_height() const;
    bool wants_exit() const;
    std::uint64_t frame_count() const;

    // Direct QuickJS inspection / scripting hook (for testing & verification)
    std::string eval_js(const std::string& script, std::string& error);

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace rpg2lx

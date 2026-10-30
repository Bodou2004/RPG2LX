#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

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
    void clear_inputs();
    void on_pointer_move(int game_x, int game_y);
    void on_pointer_down(int game_x, int game_y, bool right_button = false);
    void on_pointer_up(int game_x, int game_y);

    // Executes one 60 FPS MV frame (async callbacks + SceneManager.updateMain + GPU render)
    bool step_and_render(std::string& error);

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

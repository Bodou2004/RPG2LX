#pragma once
#include "rpg2lx/browser_engine.hpp"
#include "rpg2lx/controls.hpp"
#include "rpg2lx/library.hpp"
#include "rpg2lx/session.hpp"

#include <optional>

namespace rpg2lx {
enum class MenuAction { Up, Down, Left, Right, Confirm, Back, Refresh, Play, CloseSession, OpenControls };

enum class HubScreen { Library, GameMenu, Controls };

// Game menu rows, in display order.
enum class GameMenuItem { Play, Engine, GameControls, ResetGameControls, Back, Count };
inline constexpr std::size_t kGameMenuCount = static_cast<std::size_t>(GameMenuItem::Count);
// Controls editor: one row per pad button, then "reset to defaults".
inline constexpr std::size_t kControlsRowCount = kPadButtonCount + 1;

class LauncherModel {
public:
    explicit LauncherModel(GameLibrary& library);
    void act(MenuAction action);
    // Controls editor: jump to the row of the button that was pressed.
    void pad_pressed(PadButton button);
    void refresh();
    bool open_project(const std::filesystem::path& project, bool title = true);
    const GameEntry* selected() const;
    const std::vector<GameEntry>& games() const { return games_; }
    const std::vector<std::string>& errors() const { return errors_; }
    std::size_t index() const { return index_; }
    bool details() const { return screen_ != HubScreen::Library; }
    bool quit() const { return quit_; }
    const std::string& message() const { return message_; }
    const std::filesystem::path& home() const { return library_.home(); }
    GameSession* session() { return session_ ? &*session_ : nullptr; }
    const GameSession* session() const { return session_ ? &*session_ : nullptr; }

    bool has_mv_project() const { return !mv_project_.empty(); }
    const std::filesystem::path& mv_project() const { return mv_project_; }
    const std::filesystem::path& mv_save_dir() const { return mv_save_dir_; }
    const std::string& mv_game_id() const { return mv_game_id_; }

    // HUB mode: Play records a launch request and quits instead of running in-process.
    void set_hub_mode(bool hub) { hub_mode_ = hub; }
    struct LaunchRequest {
        std::filesystem::path project;
        std::string id;
        Engine engine = Engine::Native;
    };
    const std::optional<LaunchRequest>& launch_request() const { return launch_request_; }

    HubScreen screen() const { return screen_; }
    std::size_t menu_index() const { return menu_index_; }
    const ControlProfile& controls() const { return controls_; }
    // Engine of the game whose menu is open (saved per game in config/games/<id>/engine.txt).
    Engine engine() const { return engine_; }
    bool browser_available() const { return browser_engine_installed(library_.home()); }
    bool editing_global_controls() const { return controls_game_id_.empty(); }

private:
    void play_selected();
    void open_controls(const std::string& game_id);
    void close_controls();

    GameLibrary& library_;
    std::vector<GameEntry> games_;
    std::vector<std::string> errors_;
    std::size_t index_ = 0;
    bool quit_ = false;
    bool hub_mode_ = false;
    std::string message_;
    std::optional<GameSession> session_;
    std::filesystem::path mv_project_;
    std::filesystem::path mv_save_dir_;
    std::string mv_game_id_;
    std::optional<LaunchRequest> launch_request_;

    HubScreen screen_ = HubScreen::Library;
    HubScreen controls_return_ = HubScreen::Library;
    std::size_t menu_index_ = 0;
    ControlProfile controls_ = ControlProfile::defaults();
    std::string controls_game_id_;
    Engine engine_ = Engine::Native;
};

int run_launcher(GameLibrary& library, int smoke_frames = 0,
                 const std::string& screenshot = {}, bool start_first = false,
                 const std::filesystem::path& direct_project = {},
                 const std::filesystem::path& request_file = {},
                 const std::string& game_id = {});
} // namespace rpg2lx

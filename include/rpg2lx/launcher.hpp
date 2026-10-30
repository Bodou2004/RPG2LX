#pragma once
#include "rpg2lx/library.hpp"
#include "rpg2lx/session.hpp"

namespace rpg2lx {
enum class MenuAction { Up, Down, Confirm, Back, Refresh, Play };

class LauncherModel {
public:
    explicit LauncherModel(GameLibrary& library);
    void act(MenuAction action);
    void refresh();
    const GameEntry* selected() const;
    const std::vector<GameEntry>& games() const { return games_; }
    const std::vector<std::string>& errors() const { return errors_; }
    std::size_t index() const { return index_; }
    bool details() const { return details_; }
    bool quit() const { return quit_; }
    const std::string& message() const { return message_; }
    GameSession* session() { return session_ ? &*session_ : nullptr; }
    const GameSession* session() const { return session_ ? &*session_ : nullptr; }
private:
    GameLibrary& library_;
    std::vector<GameEntry> games_;
    std::vector<std::string> errors_;
    std::size_t index_ = 0;
    bool details_ = false;
    bool quit_ = false;
    std::string message_;
    std::optional<GameSession> session_;
};

int run_launcher(GameLibrary& library, int smoke_frames = 0,
                 const std::string& screenshot = {}, bool start_first = false);
} // namespace rpg2lx

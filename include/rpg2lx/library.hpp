#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace rpg2lx {

struct GameEntry {
    std::string id; // Folder name, independent of the displayed game title.
    std::string title;
    std::filesystem::path project;
    bool detected = false;
    std::string diagnostic;
};

class GameLibrary {
public:
    explicit GameLibrary(std::filesystem::path home);
    const std::filesystem::path& home() const { return home_; }
    std::filesystem::path games_path() const { return home_ / "games"; }
    bool initialize(std::string& error) const;
    std::vector<GameEntry> scan(std::vector<std::string>& errors) const;
    // Creates per-game directories only after a known game is selected.
    bool prepare_game(const GameEntry& game, std::string& error) const;
private:
    std::filesystem::path home_;
};

std::filesystem::path executable_directory(const char* argv0);

} // namespace rpg2lx

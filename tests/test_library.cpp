#include "rpg2lx/launcher.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
static void require(bool condition, const std::string& error) {
    if (!condition) throw std::runtime_error(error);
}

int main(int argc, char** argv) {
    const auto temp = fs::temp_directory_path() /
        ("rpg2lx-library-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(temp);
    // Temporary test directory is owned by this test, never supplied by a caller.
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{temp};
    try {
        require(argc == 2, "Pass fixtures directory");
        rpg2lx::GameLibrary library(temp / "RPG2LX");
        std::string error;
        require(library.initialize(error), error);
        for (const auto* folder : {"games", "saves", "config/games", "logs"})
            require(fs::is_directory(library.home() / folder), "Missing application directory");
        require(library.initialize(error), "Initialization must be idempotent");
        std::vector<std::string> errors;
        require(library.scan(errors).empty() && errors.empty(), "Empty library");
        rpg2lx::LauncherModel model(library);
        model.act(rpg2lx::MenuAction::Down);
        model.act(rpg2lx::MenuAction::Confirm);
        require(!model.selected() && !model.details(), "Empty navigation must be safe");

        auto add = [&](const std::string& id, bool www) {
            const auto folder = library.games_path() / fs::u8path(id);
            fs::create_directories(folder);
            const auto project = www ? folder / "www" : folder;
            fs::copy(fs::path(argv[1]) / "valid_project", project, fs::copy_options::recursive);
        };
        add("Adventure One", false);
        add("Adventure Two", true); // Same title, distinct save identities.
        fs::create_directory(library.games_path() / "Broken");
        fs::create_directory(library.games_path() / ".hidden");
        std::ofstream(library.games_path() / "README.txt") << "Put games here";
        auto games = library.scan(errors);
        require(games.size() == 3, "Only immediate non-hidden real directories must be listed");
        require(games[0].id == "Broken" && !games[0].detected, "Broken folder must have diagnostic");
        require(games[1].detected && games[2].detected && games[1].title == games[2].title,
                "Direct and www layouts");
        require(library.prepare_game(games[1], error), error);
        require(library.prepare_game(games[2], error), error);
        require(fs::is_directory(library.home() / "saves/Adventure One") &&
                fs::is_directory(library.home() / "saves/Adventure Two"), "Separate saves for identical titles");
        require(!fs::exists(games[1].project / "saves"), "Never write saves in game assets");
        auto forged = games[1]; forged.id = "../outside";
        require(!library.prepare_game(forged, error), "Traversal ID accepted");
        forged = games[1]; forged.project = temp;
        require(!library.prepare_game(forged, error), "Forged project accepted");

        model.refresh();
        model.act(rpg2lx::MenuAction::Down);
        require(model.selected()->id == "Adventure One", "Navigation order");
        model.act(rpg2lx::MenuAction::Confirm);
        require(model.details(), "Confirm opens details");
        model.act(rpg2lx::MenuAction::Confirm);
        require(model.message().find("desarrollo") != std::string::npos, "Do not claim to run games");
        model.act(rpg2lx::MenuAction::Back);
        require(!model.details() && !model.quit(), "Back from details returns to library");
        model.refresh();
        require(model.selected()->id == "Adventure One", "Refresh retains selection by folder ID");
        model.act(rpg2lx::MenuAction::Confirm);
        require(model.details(), "Details reopened before removing game");
        fs::remove_all(library.games_path() / "Adventure One");
        model.refresh();
        require(model.selected() && model.index() == 0, "Removed selection resets safely");
        require(!model.details(), "Removed game returns to library");
        model.act(rpg2lx::MenuAction::Back);
        require(model.quit(), "Back from library exits");

        fs::create_directories(temp / "blocked");
        std::ofstream(temp / "blocked/games") << "not a directory";
        rpg2lx::GameLibrary blocked(temp / "blocked");
        require(!blocked.initialize(error) && !error.empty(), "Directory creation errors must be reported");
        std::cout << "Library and menu model tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}

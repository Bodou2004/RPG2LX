#include "rpg2lx/library.hpp"
#include "rpg2lx/json/json_parser.hpp"
#include "rpg2lx/project_source.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <system_error>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;
namespace rpg2lx {
namespace {
bool real_directory(const fs::path& path) {
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    return !ec && fs::is_directory(status) && !fs::is_symlink(status);
}

bool ensure_directory(const fs::path& path, std::string& error) {
    std::error_code ec;
    if (fs::exists(path, ec)) {
        if (real_directory(path)) return true;
        error = "Expected a real directory: " + path.u8string();
        return false;
    }
    fs::create_directories(path, ec);
    if (ec) { error = path.u8string() + ": " + ec.message(); return false; }
    return true;
}

bool file_exists(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

GameEntry inspect_folder(const fs::path& folder) {
    GameEntry game;
    game.id = folder.filename().u8string();
    game.title = game.id;
    game.project = folder;
    const auto source=resolve_mv_project(folder);
    if(!source){game.diagnostic=source.error;return game;}
    game.project=source.source->root;
    const auto system = game.project / "data/System.json";
    std::error_code ec;
    const auto size = fs::file_size(system, ec);
    if (ec) { game.diagnostic = "Missing data/System.json (also checked www/)"; return game; }
    if (size > 8 * 1024 * 1024) { game.diagnostic = "System.json exceeds 8 MiB limit"; return game; }
    std::ifstream input(system, std::ios::binary);
    if (!input) { game.diagnostic = "Cannot open System.json"; return game; }
    std::string contents;
    char buffer[4096];
    while (input.read(buffer, sizeof buffer) || input.gcount()) {
        if (contents.size() + static_cast<size_t>(input.gcount()) > 8 * 1024 * 1024) {
            game.diagnostic = "System.json exceeds 8 MiB limit"; return game;
        }
        contents.append(buffer, static_cast<size_t>(input.gcount()));
    }
    if (!input.eof()) { game.diagnostic = "Failed while reading System.json"; return game; }
    std::string error;
    const auto data = JsonParser::parse(contents, error);
    if (!error.empty() || !data.is_object()) { game.diagnostic = "Invalid System.json: " + error; return game; }
    const auto* title = data.find("gameTitle");
    const auto* start = data.find("startMapId");
    if (!title || !title->is_string() || !start || start->get_int(-1) < 1) {
        game.diagnostic = "System.json requires gameTitle and a positive integer startMapId";
        return game;
    }
    if (!title->str_val.empty()) game.title = title->str_val;
    for (const auto* name : {"MapInfos.json", "Actors.json", "CommonEvents.json"}) {
        if (!file_exists(game.project / "data" / name)) {
            game.diagnostic = std::string("Missing data/") + name;
            return game;
        }
    }
    game.detected = true;
    game.diagnostic = "MV structure detected; native exploration is experimental";
    return game;
}
} // namespace

GameLibrary::GameLibrary(fs::path home) : home_(fs::absolute(std::move(home)).lexically_normal()) {}

bool GameLibrary::initialize(std::string& error) const {
    error.clear();
    if (!ensure_directory(home_, error)) return false;
    for (const auto* name : {"games", "saves", "config", "logs"})
        if (!ensure_directory(home_ / name, error)) return false;
    return ensure_directory(home_ / "config/games", error);
}

std::vector<GameEntry> GameLibrary::scan(std::vector<std::string>& errors) const {
    errors.clear();
    std::vector<GameEntry> games;
    if (!real_directory(games_path())) { errors.push_back("games is missing or not a real directory"); return games; }
    std::error_code ec;
    fs::directory_iterator it(games_path(), ec), end;
    if (ec) { errors.push_back("Cannot scan games: " + ec.message()); return games; }
    while (it != end) {
        const auto folder = it->path();
        if (real_directory(folder) && folder.filename().u8string().front() != '.') {
            try { games.push_back(inspect_folder(folder)); }
            catch (const std::exception& e) {
                games.push_back({folder.filename().u8string(), folder.filename().u8string(), folder, false, e.what()});
            }
        }
        it.increment(ec);
        if (ec) { errors.push_back("Cannot finish scan: " + ec.message()); break; }
    }
    std::sort(games.begin(), games.end(), [](const GameEntry& a, const GameEntry& b) {
        return a.title == b.title ? a.id < b.id : a.title < b.title;
    });
    return games;
}

bool GameLibrary::prepare_game(const GameEntry& game, std::string& error) const {
    error.clear();
    // Do not accept a caller-provided arbitrary path or traversal as a save ID.
    const auto id = fs::u8path(game.id);
    if (game.id.empty() || id != id.filename() || id == "." || id == "..") {
        error = "Invalid game folder ID"; return false;
    }
    std::vector<std::string> errors;
    const auto games = scan(errors);
    const auto found = std::find_if(games.begin(), games.end(), [&](const GameEntry& known) {
        return known.id == game.id && known.detected && known.project == game.project;
    });
    if (found == games.end()) { error = "Game is no longer available or has invalid data"; return false; }
    if (!initialize(error)) return false;
    return ensure_directory(home_ / "saves" / id, error) &&
           ensure_directory(home_ / "config/games" / id, error);
}

fs::path executable_directory(const char* argv0) {
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768);
    const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length && length < buffer.size()) return fs::path(std::wstring(buffer.data(), length)).parent_path();
#elif defined(__linux__)
    std::error_code ec;
    auto path = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return path.parent_path();
#endif
    return fs::absolute(fs::u8path(argv0)).parent_path();
}
} // namespace rpg2lx

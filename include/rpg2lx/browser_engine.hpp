#pragma once
#include <filesystem>
#include <string>

namespace rpg2lx {

// Browser engine: the game runs in NW.js (Chromium + Node), the same runtime RPG Maker MV exports ship with, for
// plugins the native engine cannot run or runs slowly. NW.js lives in <home>/nwjs/.
enum class Engine { Native, Browser };

const char* engine_label(Engine engine);
// The game's chosen engine; without a choice, Browser when NW.js is installed.
Engine load_engine(const std::filesystem::path& home, const std::string& game_id);
bool save_engine(const std::filesystem::path& home, const std::string& game_id, Engine engine);
bool browser_engine_installed(const std::filesystem::path& home);

// Writes the NW.js wrapper app for a game into <home>/config/games/<id>/nwapp and returns that folder. The game's
// own folder is never modified. The app opens the game's index.html, saves into <home>/saves/<id> (shared with
// the native engine) and logs errors and frame rates into <home>/logs/browser.log.
bool write_browser_app(const std::filesystem::path& home, const std::string& game_id,
                       const std::filesystem::path& project, std::filesystem::path& app_dir, std::string& error);

// gptokeyb mapping for the browser engine, built from the game's control profile. MV actions become the key MV
// assigns to them by default (Enter for "ok", Escape for "escape"...).
bool write_browser_gptk(const std::filesystem::path& home, const std::string& game_id,
                        const std::filesystem::path& file, std::string& error);

} // namespace rpg2lx

#pragma once
#include <filesystem>
#include <string>

namespace rpg2lx {

// Browser engine: the game runs in NW.js (Chromium + Node), the same runtime RPG Maker MV exports ship with, for
// plugins the native engine cannot run or runs slowly. NW.js lives in <home>/nwjs/.
// Auto: the browser when Chromium can draw with the GPU on this device (RPG2LX.sh probes it once and keeps the
// answer in config/browser-gpu.txt), the native engine otherwise. Browser forces it, even drawing on the CPU.
enum class Engine { Native, Browser, Auto };

// What RPG2LX.sh found about Chromium and the GPU: not probed yet, a real GPU, or CPU drawing only.
enum class BrowserGpu { Unknown, Found, None };
BrowserGpu browser_gpu(const std::filesystem::path& home);

// "native", "browser" or "auto": engine.txt and the launch request.
const char* engine_id(Engine engine);
// HUB text for the game menu, with what Auto picks once the GPU is known ("Auto (nativo)").
std::string engine_label(Engine engine, BrowserGpu gpu);
// The game's chosen engine; without a choice, Auto when NW.js is installed, else Native.
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

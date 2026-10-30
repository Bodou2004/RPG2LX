#include "rpg2lx/core.hpp"
#include "rpg2lx/map.hpp"
#include "rpg2lx/launcher.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

void print_help() {
    std::cout << "rpg2lx - RPG Maker MV Runtime & Inspector for Linux PortMaster\n";
    std::cout << "Usage:\n";
    std::cout << "  rpg2lx --help                  Show this help message\n";
    std::cout << "  rpg2lx --version               Show version information\n";
    std::cout << "  rpg2lx inspect <project_path>  Inspect RPG Maker MV project structure\n";
    std::cout << "  rpg2lx inspect-map <map.json>  Validate map layers and event data\n";
    std::cout << "  rpg2lx launcher [--home DIR]  Open the game library (default command)\n";
    std::cout << "  rpg2lx list [--home DIR]      List folders detected in games/\n";
}

void print_version() {
    std::cout << "rpg2lx version 0.1.0-alpha (Phase 0/1 milestone)\n";
    std::cout << "Target: Linux ARM64 (RK3326 / R36S) & Host Development\n";
}

static int rpg2lx_main(int argc, char* argv[]) {
    std::string command = argc < 2 ? "launcher" : argv[1];

    if (command == "launcher" || command == "list") {
        try {
            auto home = rpg2lx::executable_directory(argv[0]);
            int smoke_frames = 0;
            std::string screenshot;
            for (int i = 2; i < argc; ++i) {
                const std::string arg = argv[i];
                if (arg == "--home" && i+1 < argc) home = std::filesystem::u8path(argv[++i]);
                else if (command == "launcher" && arg == "--smoke-frames" && i+1 < argc) {
                    const std::string number = argv[++i];
                    if (number.empty() || number.find_first_not_of("0123456789") != std::string::npos)
                        throw std::runtime_error("--smoke-frames expects an integer between 1 and 60");
                    smoke_frames = std::stoi(number);
                    if (smoke_frames < 1 || smoke_frames > 60) throw std::runtime_error("Invalid smoke frame count");
                } else if (command == "launcher" && arg == "--screenshot" && i+1 < argc) screenshot = argv[++i];
                else { std::cerr << "Invalid or incomplete launcher option: " << arg << '\n'; return 1; }
            }
            if (!screenshot.empty() && smoke_frames == 0) {
                std::cerr << "--screenshot requires --smoke-frames\n"; return 1;
            }
            rpg2lx::GameLibrary library(home);
            std::string error;
            if (!library.initialize(error)) { std::cerr << error << '\n'; return 2; }
            if (command == "list") {
                std::vector<std::string> errors;
                const auto games = library.scan(errors);
                std::cout << "RPG2LX games: " << games.size() << "\nFolder: " << library.games_path().u8string() << '\n';
                for (const auto& game : games)
                    std::cout << (game.detected ? "[MV] " : "[INCOMPLETE] ") << game.title
                              << " | " << game.id << "\n  " << game.diagnostic << '\n';
                for (const auto& scan_error : errors) std::cerr << scan_error << '\n';
                return errors.empty() ? 0 : 2;
            }
            std::ofstream log(library.home() / "logs/launcher.log", std::ios::app);
            if (!log) { std::cerr << "Cannot open logs/launcher.log\n"; return 2; }
            log << "\nRPG2LX launcher | " << library.home().u8string() << '\n';
            struct Redirect {
                std::streambuf* out;
                std::streambuf* err;
                explicit Redirect(std::ostream& log) : out(std::cout.rdbuf(log.rdbuf())), err(std::cerr.rdbuf(log.rdbuf())) {}
                ~Redirect() { std::cout.rdbuf(out); std::cerr.rdbuf(err); }
            } redirect(log);
            return rpg2lx::run_launcher(library, smoke_frames, screenshot);
        } catch (const std::exception& e) {
            std::cerr << "Launcher: " << e.what() << '\n'; return 1;
        }
    }

    if (command == "--help" || command == "-h") {
        print_help();
        return 0;
    }

    if (command == "--version" || command == "-v") {
        print_version();
        return 0;
    }

    if (command == "inspect-map") {
        if (argc != 3) {
            std::cerr << "Error: inspect-map requires exactly one map file.\n";
            return 1;
        }
        auto result = rpg2lx::MapLoader::load(std::filesystem::u8path(argv[2]));
        if (!result) {
            std::cerr << "[MAP] " << result.error << '\n';
            return 2;
        }
        const auto& map = *result.map;
        std::cout << "Map: " << map.display_name << "\nSize: " << map.width << 'x' << map.height
                  << "\nTileset: " << map.tileset_id << "\nLayers: 6\nCells: " << map.cells.size()
                  << "\nEvents: " << map.events.size() << '\n';
        return 0;
    }

    if (command == "inspect") {
        if (argc != 3) {
            std::cerr << "Error: 'inspect' requires a project path argument.\n";
            std::cerr << "Example: rpg2lx inspect ./fixtures/valid_project\n";
            return 1;
        }

        std::string project_path = argv[2];
        rpg2lx::ProjectMetadata meta = rpg2lx::ProjectInspector::inspect(project_path);

        std::cout << "\n--- Project Inspection Summary ---\n";
        std::cout << "Status: " << (meta.is_valid ? "VALID RPG Maker MV Project Structure" : "INVALID / INCOMPLETE") << "\n";
        std::cout << "Game Title: " << (meta.game_title.empty() ? "(Unknown)" : meta.game_title) << "\n";
        std::cout << "Start Map ID: " << (meta.start_map_id.empty() ? "(None)" : meta.start_map_id) << "\n";
        std::cout << "Screen Resolution: " << meta.screen_width << "x" << meta.screen_height << "\n";
        std::cout << "Database Records: " << meta.actor_count << " actors, "
                  << meta.map_count << " maps, " << meta.common_event_count << " common events\n";

        std::cout << "Detected Plugins: " << meta.plugins.size() << "\n";
        for (const auto& p : meta.plugins) {
            std::cout << "  - " << p.name << " [" << (p.status ? "ENABLED" : "DISABLED") << "]"
                      << (p.description.empty() ? "" : (" - " + p.description)) << "\n";
            if (!p.parameters.empty()) {
                for (const auto& kv : p.parameters) {
                    std::cout << "      * " << kv.first << " = " << kv.second << "\n";
                }
            }
        }

        if (!meta.missing_files.empty()) {
            std::cout << "Missing Files (" << meta.missing_files.size() << "):\n";
            for (const auto& f : meta.missing_files) {
                std::cout << "  [!] " << f << "\n";
            }
        }

        if (!meta.warnings.empty()) {
            std::cout << "Warnings / Errors (" << meta.warnings.size() << "):\n";
            for (const auto& w : meta.warnings) {
                std::cout << "  [*] " << w << "\n";
            }
        }

        return meta.is_valid ? 0 : 2;
    }

    std::cerr << "Unknown command: " << command << "\n";
    print_help();
    return 1;
}

int main(int argc, char* argv[]) {
    try {
#ifdef _WIN32
        // Windows' narrow argv uses the active ANSI code page. Read UTF-16
        // arguments and convert explicitly so Spanish game folders remain intact.
        int wide_count = 0;
        wchar_t** wide = CommandLineToArgvW(GetCommandLineW(), &wide_count);
        if (!wide) throw std::runtime_error("Cannot read command-line arguments");
        struct FreeArgs { wchar_t** data; ~FreeArgs() { LocalFree(data); } } cleanup{wide};
        std::vector<std::string> strings;
        strings.reserve(static_cast<std::size_t>(wide_count));
        for (int i = 0; i < wide_count; ++i) {
            const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, nullptr, 0, nullptr, nullptr);
            if (!length) throw std::runtime_error("Invalid Unicode argument");
            std::string utf8(static_cast<std::size_t>(length), '\0');
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, utf8.data(), length, nullptr, nullptr);
            utf8.pop_back();
            strings.push_back(std::move(utf8));
        }
        std::vector<char*> arguments;
        for (auto& value : strings) arguments.push_back(value.data());
        (void)argc; (void)argv;
        return rpg2lx_main(wide_count, arguments.data());
#else
        return rpg2lx_main(argc, argv);
#endif
    } catch (const std::exception& error) {
        std::cerr << "RPG2LX: " << error.what() << '\n';
        return 2;
    }
}

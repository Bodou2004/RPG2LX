#include "rpg2lx/core.hpp"
#include "rpg2lx/map.hpp"
#include <iostream>
#include <string>
#include <vector>

void print_help() {
    std::cout << "rpg2lx - RPG Maker MV Runtime & Inspector for Linux PortMaster\n";
    std::cout << "Usage:\n";
    std::cout << "  rpg2lx --help                  Show this help message\n";
    std::cout << "  rpg2lx --version               Show version information\n";
    std::cout << "  rpg2lx inspect <project_path>  Inspect RPG Maker MV project structure\n";
    std::cout << "  rpg2lx inspect-map <map.json>  Validate map layers and event data\n";
}

void print_version() {
    std::cout << "rpg2lx version 0.1.0-alpha (Phase 0/1 milestone)\n";
    std::cout << "Target: Linux ARM64 (RK3326 / R36S) & Host Development\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    std::string command = argv[1];

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

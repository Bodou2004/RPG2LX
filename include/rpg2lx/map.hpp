#pragma once

#include "rpg2lx/json/json_parser.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rpg2lx {

struct EventCommand {
    int code = 0;
    int indent = 0;
    std::vector<JsonValue> parameters;
};

struct EventPage {
    int trigger = 0;
    std::vector<EventCommand> commands;
    // Preserve settings until the event runtime implements their semantics.
    JsonValue settings;
};

struct MapEvent {
    int id = 0;
    std::string name;
    int x = 0;
    int y = 0;
    std::vector<EventPage> pages;
};

// Source data, independent of renderer and mutable game state.
struct MapData {
    int width = 0;
    int height = 0;
    int tileset_id = 0;
    int scroll_type = 0;
    std::string display_name;
    std::vector<std::uint16_t> cells;
    std::vector<MapEvent> events;

    bool loops_x() const { return scroll_type == 2 || scroll_type == 3; }
    bool loops_y() const { return scroll_type == 1 || scroll_type == 3; }
    // Four tile layers, shadow bits at z=4, region IDs at z=5.
    std::uint16_t cell(int x, int y, int z) const;
    std::uint16_t region(int x, int y) const { return cell(x, y, 5); }
};

struct MapLoadResult {
    std::optional<MapData> map;
    std::string error;
    explicit operator bool() const { return map.has_value(); }
};

class MapLoader {
public:
    static MapLoadResult parse(const std::string& json);
    static MapLoadResult load(const std::filesystem::path& path);
};

} // namespace rpg2lx

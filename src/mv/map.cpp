#include "rpg2lx/map.hpp"
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace rpg2lx {
namespace {
constexpr std::size_t max_map_bytes = 64 * 1024 * 1024;
constexpr std::size_t max_cells = 6 * 1024 * 1024;

const JsonValue& field(const JsonValue& object, const std::string& key) {
    const auto* value = object.find(key);
    if (!value) throw std::runtime_error("Missing field: " + key);
    return *value;
}

int integer(const JsonValue& value, int low, int high, const std::string& context) {
    if (!value.is_number() || !std::isfinite(value.num_val) ||
        std::trunc(value.num_val) != value.num_val ||
        value.num_val < low || value.num_val > high) {
        throw std::runtime_error(context + ": expected integer in range " +
                                 std::to_string(low) + ".." + std::to_string(high));
    }
    return static_cast<int>(value.num_val);
}

const std::vector<JsonValue>& array(const JsonValue& value, const std::string& context) {
    if (!value.is_array()) throw std::runtime_error(context + ": expected array");
    return value.arr_val;
}

std::string text(const JsonValue& value, const std::string& context) {
    if (!value.is_string()) throw std::runtime_error(context + ": expected string");
    return value.str_val;
}
} // namespace

std::uint16_t MapData::cell(int x, int y, int z) const {
    if (width <= 0 || height <= 0 || z < 0 || z >= 6) return 0;
    if (loops_x()) x = (x % width + width) % width;
    if (loops_y()) y = (y % height + height) % height;
    if (x < 0 || y < 0 || x >= width || y >= height) return 0;
    const auto index = (static_cast<std::size_t>(z) * height + y) * width + x;
    return index < cells.size() ? cells[index] : 0;
}

MapLoadResult MapLoader::parse(const std::string& json) {
    try {
        if (json.size() > max_map_bytes) throw std::runtime_error("Map exceeds 64 MiB input limit");
        std::string error;
        auto root = JsonParser::parse(json, error);
        if (!error.empty()) throw std::runtime_error("JSON: " + error);
        if (!root.is_object()) throw std::runtime_error("Map root must be an object");
        MapData map;
        map.width = integer(field(root, "width"), 1, 10000, "width");
        map.height = integer(field(root, "height"), 1, 10000, "height");
        const auto count = static_cast<std::size_t>(map.width) * map.height * 6;
        if (count > max_cells) throw std::runtime_error("Map exceeds cell budget");
        map.tileset_id = integer(field(root, "tilesetId"), 1, 2147483647, "tilesetId");
        map.scroll_type = integer(field(root, "scrollType"), 0, 3, "scrollType");
        if (const auto* name = root.find("displayName")) map.display_name = text(*name, "displayName");
        const auto& data = array(field(root, "data"), "data");
        if (data.size() != count) throw std::runtime_error("data length must equal width * height * 6");
        map.cells.reserve(count);
        const auto plane = count / 6;
        for (std::size_t i = 0; i < count; ++i) {
            const auto layer = i / plane;
            const auto limit = layer == 4 ? 15 : layer == 5 ? 255 : 8191;
            map.cells.push_back(static_cast<std::uint16_t>(integer(data[i], 0, limit,
                "data[" + std::to_string(i) + "]")));
        }

        const auto& events = array(field(root, "events"), "events");
        for (std::size_t i = 0; i < events.size(); ++i) {
            const auto& source = events[i];
            if (source.is_null()) continue;
            if (!source.is_object()) throw std::runtime_error("Event must be object or null");
            MapEvent event;
            event.id = integer(field(source, "id"), 1, 2147483647, "event.id");
            if (static_cast<std::size_t>(event.id) != i)
                throw std::runtime_error("Event id must match its array index");
            event.name = text(field(source, "name"), "event.name");
            event.x = integer(field(source, "x"), 0, map.width - 1, "event.x");
            event.y = integer(field(source, "y"), 0, map.height - 1, "event.y");
            for (const auto& source_page : array(field(source, "pages"), "event.pages")) {
                EventPage page;
                page.trigger = integer(field(source_page, "trigger"), 0, 4, "page.trigger");
                page.settings = source_page;
                // Commands have a separate representation; do not keep a second copy.
                page.settings.obj_val.erase("list");
                for (const auto& source_cmd : array(field(source_page, "list"), "page.list")) {
                    EventCommand cmd;
                    cmd.code = integer(field(source_cmd, "code"), 0, 2147483647, "command.code");
                    cmd.indent = integer(field(source_cmd, "indent"), 0, 10000, "command.indent");
                    cmd.parameters = array(field(source_cmd, "parameters"), "command.parameters");
                    page.commands.push_back(std::move(cmd));
                }
                event.pages.push_back(std::move(page));
            }
            map.events.push_back(std::move(event));
        }
        return {std::move(map), {}};
    } catch (const std::exception& e) {
        return {std::nullopt, e.what()};
    }
}

MapLoadResult MapLoader::load(const std::filesystem::path& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) return {std::nullopt, "Cannot read map: " + path.u8string() + ": " + ec.message()};
    if (size > max_map_bytes) return {std::nullopt, "Map exceeds 64 MiB input limit"};
    std::ifstream file(path, std::ios::binary);
    if (!file) return {std::nullopt, "Cannot open map: " + path.u8string()};
    std::string content;
    content.reserve(static_cast<std::size_t>(size));
    char buffer[8192];
    while (file.read(buffer, sizeof buffer) || file.gcount()) {
        if (content.size() + static_cast<std::size_t>(file.gcount()) > max_map_bytes)
            return {std::nullopt, "Map exceeds 64 MiB input limit"};
        content.append(buffer, static_cast<std::size_t>(file.gcount()));
    }
    if (!file.eof()) return {std::nullopt, "Error reading map: " + path.u8string()};
    auto result = parse(content);
    if (!result) result.error = path.u8string() + ": " + result.error;
    return result;
}
} // namespace rpg2lx

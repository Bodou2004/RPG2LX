#include "rpg2lx/core.hpp"
#include "rpg2lx/json/json_parser.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cmath>
#include <limits>
#include <regex>

namespace fs = std::filesystem;

namespace rpg2lx {

static LogLevel g_current_level = LogLevel::Info;

void Logger::set_level(LogLevel level) {
    g_current_level = level;
}

LogLevel Logger::get_level() {
    return g_current_level;
}

void Logger::debug(const std::string& msg) {
    log(LogLevel::Debug, msg);
}

void Logger::info(const std::string& msg) {
    log(LogLevel::Info, msg);
}

void Logger::warn(const std::string& msg) {
    log(LogLevel::Warn, msg);
}

void Logger::error(const std::string& msg) {
    log(LogLevel::Error, msg);
}

void Logger::log(LogLevel level, const std::string& msg) {
    if (static_cast<int>(level) < static_cast<int>(g_current_level)) {
        return;
    }

    const char* prefix = "[INFO]";
    switch (level) {
        case LogLevel::Debug: prefix = "[DEBUG]"; break;
        case LogLevel::Info:  prefix = "[INFO] "; break;
        case LogLevel::Warn:  prefix = "[WARN] "; break;
        case LogLevel::Error: prefix = "[ERROR]"; break;
    }
    std::cout << prefix << " " << msg << std::endl;
}

static bool read_file_content(const fs::path& p, std::string& out_content, std::string& out_error) {
    std::error_code ec;
    if (!fs::exists(p, ec) || !fs::is_regular_file(p, ec)) {
        out_error = "File does not exist or is not a regular file: " + p.string();
        return false;
    }
    std::ifstream file(p, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        out_error = "Failed to open file: " + p.string();
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    if (file.bad()) {
        out_error = "Failed while reading file: " + p.string();
        return false;
    }
    out_content = ss.str();
    return true;
}

static void parse_plugins_js(const std::string& js_content, std::vector<PluginInfo>& out_plugins, std::string& out_warning) {
    // RPG Maker MV plugins.js format:
    // var $plugins =
    // [
    //   {"name":"...","status":true,"description":"...","parameters":{"key":"val"}}
    // ];
    // Remove comments only outside JSON strings, preserving bracket characters in strings.
    std::string script;
    bool in_string = false, escape = false;
    for (size_t i = 0; i < js_content.size(); ++i) {
        char c = js_content[i];
        if (in_string) {
            script += c;
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if (c == '"') in_string = false;
        } else if (c == '"') { in_string = true; script += c; }
        else if (c == '/' && i + 1 < js_content.size() && js_content[i+1] == '/') {
            i += 2;
            while (i < js_content.size() && js_content[i] != '\n') ++i;
            script += '\n';
        } else if (c == '/' && i + 1 < js_content.size() && js_content[i+1] == '*') {
            auto end_comment = js_content.find("*/", i + 2);
            if (end_comment == std::string::npos) { out_warning = "Unterminated plugins.js comment"; return; }
            i = end_comment + 1;
            script += ' ';
        } else script += c;
    }
    if (script.compare(0, 3, "\xEF\xBB\xBF") == 0) script.erase(0, 3);
    const std::regex prefix(R"(^\s*(var|let|const)\s+\$plugins\s*=\s*)");
    std::smatch match;
    if (!std::regex_search(script, match, prefix)) {
        out_warning = "Unsupported plugins.js: expected a static $plugins assignment";
        return;
    }
    size_t start = static_cast<size_t>(match.length());
    size_t end = script.rfind(']');
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        out_warning = "Could not locate JSON array brackets in js/plugins.js";
        return;
    }

    if (script[start] != '[' || !std::regex_match(script.substr(end + 1), std::regex(R"(\s*;?\s*)"))) {
        out_warning = "Unsupported plugins.js: expected only a JSON array assignment";
        return;
    }
    std::string array_slice = script.substr(start, (end - start + 1));
    std::string err;
    JsonValue root = JsonParser::parse(array_slice, err);
    if (!err.empty() || !root.is_array()) {
        out_warning = "Malformed plugin configuration in js/plugins.js: " + err;
        return;
    }

    for (const auto& item : root.arr_val) {
        if (!item.is_object()) { out_warning = "Plugin entry must be an object"; return; }
        PluginInfo info;
        const JsonValue* name_val = item.find("name");
        if (name_val && name_val->is_string()) {
            info.name = name_val->get_string();
        } else {
            out_warning = "Plugin name must be a string";
            return;
        }

        const JsonValue* status_val = item.find("status");
        if (status_val && status_val->is_bool()) {
            info.status = status_val->get_bool();
        } else { out_warning = "Plugin status must be boolean"; return; }

        const JsonValue* desc_val = item.find("description");
        if (desc_val && desc_val->is_string()) {
            info.description = desc_val->get_string();
        }

        const JsonValue* params_val = item.find("parameters");
        if (params_val && params_val->is_object()) {
            for (const auto& kv : params_val->obj_val) {
                if (kv.second.is_string()) {
                    info.parameters[kv.first] = kv.second.get_string();
                } else if (kv.second.is_number()) {
                    info.parameters[kv.first] = std::to_string(kv.second.get_double());
                } else if (kv.second.is_bool()) {
                    info.parameters[kv.first] = kv.second.get_bool() ? "true" : "false";
                }
            }
        }
        out_plugins.push_back(info);
    }
}

ProjectMetadata ProjectInspector::inspect(const std::string& project_path) {
    ProjectMetadata meta;
    std::error_code ec;
    fs::path root = fs::u8path(project_path);

    Logger::info("Inspecting RPG Maker MV project at: " + project_path);

    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        Logger::error("Directory does not exist or cannot be accessed: " + project_path);
        meta.warnings.push_back("Directory does not exist or cannot be accessed: " + project_path);
        return meta;
    }

    std::vector<std::string> required_data = {
        "data/System.json",
        "data/MapInfos.json",
        "data/Actors.json",
        "data/CommonEvents.json"
    };

    for (const auto& rel : required_data) {
        fs::path p = root / rel;
        if (!fs::exists(p, ec)) {
            meta.missing_files.push_back(rel);
            Logger::warn("Missing required RPG Maker MV file: " + rel);
        }
    }

    // Parse and validate System.json
    fs::path system_json = root / "data/System.json";
    if (fs::exists(system_json, ec)) {
        std::string content, read_err;
        if (!read_file_content(system_json, content, read_err)) {
            meta.warnings.push_back("Failed to read System.json: " + read_err);
        } else {
            std::string parse_err;
            JsonValue root_val = JsonParser::parse(content, parse_err);
            if (!parse_err.empty()) {
                Logger::error("System.json syntax error: " + parse_err);
                meta.warnings.push_back("System.json syntax error: " + parse_err);
            } else if (!root_val.is_object()) {
                meta.warnings.push_back("System.json root must be a JSON object");
            } else {
                const JsonValue* title_val = root_val.find("gameTitle");
                if (title_val && title_val->is_string()) {
                    meta.game_title = title_val->get_string();
                } else meta.warnings.push_back("System.json gameTitle must be a string");
                const JsonValue* start_map_val = root_val.find("startMapId");
                if (start_map_val && start_map_val->is_number() &&
                    std::isfinite(start_map_val->num_val) &&
                    std::trunc(start_map_val->num_val) == start_map_val->num_val &&
                    start_map_val->num_val >= 1 &&
                    start_map_val->num_val <= std::numeric_limits<int>::max()) {
                    meta.start_map_id = std::to_string(start_map_val->get_int());
                } else meta.warnings.push_back("System.json startMapId must be a positive integer");
                Logger::info("Detected game title: " + (meta.game_title.empty() ? "(Untitled)" : meta.game_title));
            }
        }
    }

    // Parse and validate MapInfos.json
    fs::path map_infos_path = root / "data/MapInfos.json";
    if (fs::exists(map_infos_path, ec)) {
        std::string content, read_err;
        if (read_file_content(map_infos_path, content, read_err)) {
            std::string parse_err;
            JsonValue val = JsonParser::parse(content, parse_err);
            if (!parse_err.empty()) {
                meta.warnings.push_back("MapInfos.json syntax error: " + parse_err);
            } else if (!val.is_array()) {
                meta.warnings.push_back("MapInfos.json root must be a JSON array");
            } else {
                for (const auto& item : val.arr_val) {
                    if (item.is_object()) meta.map_count++;
                }
            }
        } else meta.warnings.push_back(read_err);
    }

    // Parse and validate Actors.json
    fs::path actors_path = root / "data/Actors.json";
    if (fs::exists(actors_path, ec)) {
        std::string content, read_err;
        if (read_file_content(actors_path, content, read_err)) {
            std::string parse_err;
            JsonValue val = JsonParser::parse(content, parse_err);
            if (!parse_err.empty()) {
                meta.warnings.push_back("Actors.json syntax error: " + parse_err);
            } else if (!val.is_array()) {
                meta.warnings.push_back("Actors.json root must be a JSON array");
            } else {
                for (const auto& item : val.arr_val) {
                    if (item.is_object()) meta.actor_count++;
                }
            }
        } else meta.warnings.push_back(read_err);
    }

    // Parse and validate CommonEvents.json
    fs::path common_events_path = root / "data/CommonEvents.json";
    if (fs::exists(common_events_path, ec)) {
        std::string content, read_err;
        if (read_file_content(common_events_path, content, read_err)) {
            std::string parse_err;
            JsonValue val = JsonParser::parse(content, parse_err);
            if (!parse_err.empty()) {
                meta.warnings.push_back("CommonEvents.json syntax error: " + parse_err);
            } else if (!val.is_array()) {
                meta.warnings.push_back("CommonEvents.json root must be a JSON array");
            } else {
                for (const auto& item : val.arr_val) {
                    if (item.is_object()) meta.common_event_count++;
                }
            }
        } else meta.warnings.push_back(read_err);
    }

    // Parse package.json (optional for MV desktop builds)
    fs::path package_json = root / "package.json";
    if (fs::exists(package_json, ec)) {
        std::string content, read_err;
        if (read_file_content(package_json, content, read_err)) {
            std::string parse_err;
            JsonValue pkg = JsonParser::parse(content, parse_err);
            if (parse_err.empty() && pkg.is_object()) {
                if (meta.game_title.empty()) {
                    const JsonValue* name_val = pkg.find("name");
                    if (name_val && name_val->is_string()) {
                        meta.game_title = name_val->get_string();
                    }
                }
                const JsonValue* win_val = pkg.find("window");
                if (win_val && win_val->is_object()) {
                    const JsonValue* w_val = win_val->find("width");
                    const JsonValue* h_val = win_val->find("height");
                    if (w_val) {
                        const int width = w_val->get_int(-1);
                        if (width > 0) meta.screen_width = width;
                        else meta.warnings.push_back("package.json window.width must be a positive integer");
                    }
                    if (h_val) {
                        const int height = h_val->get_int(-1);
                        if (height > 0) meta.screen_height = height;
                        else meta.warnings.push_back("package.json window.height must be a positive integer");
                    }
                }
            } else meta.warnings.push_back("package.json must contain a valid JSON object: " + parse_err);
        } else meta.warnings.push_back(read_err);
    }

    // Parse js/plugins.js strictly without executing code
    fs::path plugins_js = root / "js/plugins.js";
    if (fs::exists(plugins_js, ec)) {
        std::string content, read_err;
        if (read_file_content(plugins_js, content, read_err)) {
            std::string warn;
            parse_plugins_js(content, meta.plugins, warn);
            if (!warn.empty()) {
                meta.warnings.push_back(warn);
            } else {
                int active_count = 0;
                for (const auto& p : meta.plugins) {
                    if (p.status) active_count++;
                }
                Logger::info("Detected " + std::to_string(meta.plugins.size()) +
                             " plugins declared (" + std::to_string(active_count) +
                             " active). Execution isolated.");
            }
        } else meta.warnings.push_back(read_err);
    }

    meta.is_valid = (meta.missing_files.empty() && meta.warnings.empty());
    return meta;
}

} // namespace rpg2lx

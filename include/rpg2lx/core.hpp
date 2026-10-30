#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace rpg2lx {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

class Logger {
public:
    static void set_level(LogLevel level);
    static LogLevel get_level();
    
    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);
    
    static void log(LogLevel level, const std::string& msg);
};

struct PluginInfo {
    std::string name;
    bool status = false;
    std::string description;
    std::map<std::string, std::string> parameters;
};

struct ProjectMetadata {
    std::string game_title;
    std::string start_map_id;
    int screen_width = 816;
    int screen_height = 624;
    int actor_count = 0;
    int map_count = 0;
    int common_event_count = 0;
    std::vector<PluginInfo> plugins;
    std::vector<std::string> missing_files;
    std::vector<std::string> warnings;
    bool is_valid = false;
};

class ProjectInspector {
public:
    static ProjectMetadata inspect(const std::string& project_path);
};

} // namespace rpg2lx

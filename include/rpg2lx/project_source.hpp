#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace rpg2lx {
struct ProjectSource {
    std::filesystem::path root;
    std::string engine_version;
    bool mv_script_found = false;
};
struct ProjectSourceResult {
    std::optional<ProjectSource> source;
    std::string error;
    explicit operator bool() const { return source.has_value(); }
};
// Resolve an existing MV editor/export directory, www/, index.html or Game.exe.
// This is read-only: no import, conversion, patching or script execution.
ProjectSourceResult resolve_mv_project(const std::filesystem::path& input);
} // namespace rpg2lx

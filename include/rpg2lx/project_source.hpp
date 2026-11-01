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
// root / rel, matching each component case-insensitively when the exact spelling does not exist.
// MV games are authored on Windows, where "img/Pictures/Hero.PNG" and "img/pictures/hero.png"
// are the same file; on the handheld's Linux filesystem they are not.
// Returns an empty path when `rel` could leave `root` (see is_safe_relative), so no caller can be tricked into it.
std::filesystem::path find_project_file(const std::filesystem::path& root, const std::string& rel);
// True when `rel` is a relative path with no root, drive or ".." component: joined to a folder it stays inside.
// A ".." inside a name ("Hero..png") is fine. Pass it already percent-decoded.
bool is_safe_relative(const std::string& rel);
} // namespace rpg2lx

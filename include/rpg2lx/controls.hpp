#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

namespace rpg2lx {

// Console buttons, in SDL GameController order. The HUB shows them by position because
// PortMaster devices disagree on whether the south button is printed "A" or "B".
enum class PadButton : std::size_t {
    South, East, West, North,
    Select, Start, L3, R3, L1, R1,
    Up, Down, Left, Right,
    L2, R2,
    Count
};
inline constexpr std::size_t kPadButtonCount = static_cast<std::size_t>(PadButton::Count);

// One emulated keyboard key: DOM keyCode (what MV and its plugins read) and gptokeyb name.
struct KeyInfo {
    const char* label;
    int key_code;
    const char* gptk;
};

const std::array<KeyInfo, 34>& key_table();
const char* pad_button_label(PadButton button);
const char* pad_button_gptk(PadButton button);
// Index in key_table() of the key with this DOM keyCode, or -1.
int key_index_for_code(int key_code);

// Button -> key profile. key_index < 0 means "unassigned".
struct ControlProfile {
    std::array<int, kPadButtonCount> key_index{};

    static ControlProfile defaults();
    int key_code(PadButton button) const;
    void cycle(PadButton button, int direction);

    bool load(const std::filesystem::path& file, std::string& error);
    bool save(const std::filesystem::path& file, std::string& error) const;
    // gptokeyb mapping used when a game runs inside the browser engine.
    bool write_gptk(const std::filesystem::path& file, std::string& error) const;
};

// Global profile lives in config/controls.ini; a game may override it in
// config/games/<id>/controls.ini.
std::filesystem::path global_controls_file(const std::filesystem::path& home);
std::filesystem::path game_controls_file(const std::filesystem::path& home, const std::string& game_id);
ControlProfile load_effective_profile(const std::filesystem::path& home, const std::string& game_id);

} // namespace rpg2lx

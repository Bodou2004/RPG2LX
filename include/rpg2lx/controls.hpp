#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

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

// What a button sends. Positive codes are DOM keyCodes (a real key). Negative codes are MV
// actions ("ok", "escape"...): the engine presses whichever key the game's Input.keyMapper
// assigns to that action, so games that remap their keyboard (J = ok, WASD...) just work.
struct KeyInfo {
    std::string label;
    int key_code;
    std::string id; // stored in controls.ini
};

const std::vector<KeyInfo>& key_table();
const char* pad_button_label(PadButton button);
// Index in key_table() of the entry with this code, or -1.
int key_index_for_code(int key_code);
// MV action names, indexed by -code - 1 (code -1 is "ok").
const std::vector<std::string>& mv_action_names();

// Button -> key profile. key_index < 0 means "unassigned".
struct ControlProfile {
    std::array<int, kPadButtonCount> key_index{};

    static ControlProfile defaults();
    int key_code(PadButton button) const;
    void cycle(PadButton button, int direction);

    bool load(const std::filesystem::path& file, std::string& error);
    bool save(const std::filesystem::path& file, std::string& error) const;
};

// Global profile lives in config/controls.ini; a game may override it in
// config/games/<id>/controls.ini.
std::filesystem::path global_controls_file(const std::filesystem::path& home);
std::filesystem::path game_controls_file(const std::filesystem::path& home, const std::string& game_id);
ControlProfile load_effective_profile(const std::filesystem::path& home, const std::string& game_id);

} // namespace rpg2lx

#include "rpg2lx/controls.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>

namespace rpg2lx {
namespace {

constexpr std::array<KeyInfo, 34> kKeys{{
    {"Enter", 13, "enter"},       {"Escape", 27, "esc"},        {"Espacio", 32, "space"},
    {"Shift", 16, "leftshift"},   {"Ctrl", 17, "leftctrl"},     {"Alt", 18, "leftalt"},
    {"Tab", 9, "tab"},            {"Retroceso", 8, "backspace"},{"RePag", 33, "pageup"},
    {"AvPag", 34, "pagedown"},    {"Arriba", 38, "up"},         {"Abajo", 40, "down"},
    {"Izquierda", 37, "left"},    {"Derecha", 39, "right"},     {"Insert", 45, "insert"},
    {"Z", 90, "z"}, {"X", 88, "x"}, {"C", 67, "c"}, {"V", 86, "v"}, {"A", 65, "a"},
    {"S", 83, "s"}, {"D", 68, "d"}, {"Q", 81, "q"}, {"W", 87, "w"}, {"E", 69, "e"},
    {"R", 82, "r"}, {"1", 49, "1"}, {"2", 50, "2"}, {"3", 51, "3"}, {"4", 52, "4"},
    {"F2", 113, "f2"}, {"F3", 114, "f3"}, {"F4", 115, "f4"}, {"F9", 120, "f9"},
}};

struct ButtonInfo {
    const char* ini;
    const char* label;
    const char* gptk;
};

constexpr std::array<ButtonInfo, kPadButtonCount> kButtons{{
    {"south", "Btn inferior", "a"},  {"east", "Btn derecho", "b"},
    {"west", "Btn izquierdo", "x"},  {"north", "Btn superior", "y"},
    {"select", "Select", "back"},   {"start", "Start", "start"},
    {"l3", "L3", "l3"},             {"r3", "R3", "r3"},
    {"l1", "L1", "l1"},             {"r1", "R1", "r1"},
    {"up", "Cruceta arriba", "up"}, {"down", "Cruceta abajo", "down"},
    {"left", "Cruceta izq.", "left"}, {"right", "Cruceta der.", "right"},
    {"l2", "L2", "l2"},             {"r2", "R2", "r2"},
}};

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

} // namespace

const std::array<KeyInfo, 34>& key_table() { return kKeys; }

const char* pad_button_label(PadButton button) {
    return kButtons[static_cast<std::size_t>(button)].label;
}

const char* pad_button_gptk(PadButton button) {
    return kButtons[static_cast<std::size_t>(button)].gptk;
}

int key_index_for_code(int key_code) {
    for (std::size_t i = 0; i < kKeys.size(); ++i)
        if (kKeys[i].key_code == key_code) return static_cast<int>(i);
    return -1;
}

ControlProfile ControlProfile::defaults() {
    // Same layout RPG Maker MV players expect from a keyboard (Input.keyMapper defaults).
    ControlProfile p;
    p.key_index.fill(-1);
    auto set = [&](PadButton b, int code) { p.key_index[static_cast<std::size_t>(b)] = key_index_for_code(code); };
    set(PadButton::South, 13);  // ok
    set(PadButton::East, 27);   // escape / cancel
    set(PadButton::West, 16);   // shift / dash
    set(PadButton::North, 88);  // X = menu
    set(PadButton::Select, 9);  // tab
    set(PadButton::Start, 27);  // menu
    set(PadButton::L1, 33);     // pageup
    set(PadButton::R1, 34);     // pagedown
    set(PadButton::L2, 17);     // control
    set(PadButton::R2, 18);     // alt (control in MV)
    set(PadButton::Up, 38);
    set(PadButton::Down, 40);
    set(PadButton::Left, 37);
    set(PadButton::Right, 39);
    return p;
}

int ControlProfile::key_code(PadButton button) const {
    const int index = key_index[static_cast<std::size_t>(button)];
    return index >= 0 && index < static_cast<int>(kKeys.size()) ? kKeys[static_cast<std::size_t>(index)].key_code : 0;
}

void ControlProfile::cycle(PadButton button, int direction) {
    // -1 (unassigned) is part of the cycle so a button can be disabled.
    const int count = static_cast<int>(kKeys.size()) + 1;
    int& index = key_index[static_cast<std::size_t>(button)];
    index = ((index + 1 + direction) % count + count) % count - 1;
}

bool ControlProfile::load(const std::filesystem::path& file, std::string& error) {
    std::ifstream in(file);
    if (!in) {
        error = "No se pudo leer " + file.u8string();
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const auto name = lower(trim(line.substr(0, eq)));
        const auto value = lower(trim(line.substr(eq + 1)));
        for (std::size_t b = 0; b < kButtons.size(); ++b) {
            if (name != kButtons[b].ini) continue;
            key_index[b] = -1;
            for (std::size_t k = 0; k < kKeys.size(); ++k)
                if (value == kKeys[k].gptk) key_index[b] = static_cast<int>(k);
        }
    }
    return true;
}

bool ControlProfile::save(const std::filesystem::path& file, std::string& error) const {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    const auto temp = file.parent_path() / (file.filename().u8string() + ".tmp");
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            error = "No se pudo escribir " + file.u8string();
            return false;
        }
        out << "# RPG2LX: boton de la consola = tecla emulada\n";
        for (std::size_t b = 0; b < kButtons.size(); ++b) {
            const int k = key_index[b];
            out << kButtons[b].ini << " = " << (k >= 0 ? kKeys[static_cast<std::size_t>(k)].gptk : "none") << '\n';
        }
        if (!out) {
            error = "No se pudo escribir " + file.u8string();
            return false;
        }
    }
    std::filesystem::rename(temp, file, ec);
    if (ec) {
        error = "No se pudo guardar " + file.u8string() + ": " + ec.message();
        return false;
    }
    return true;
}

bool ControlProfile::write_gptk(const std::filesystem::path& file, std::string& error) const {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::trunc);
    if (!out) {
        error = "No se pudo escribir " + file.u8string();
        return false;
    }
    for (std::size_t b = 0; b < kButtons.size(); ++b) {
        const int k = key_index[b];
        out << kButtons[b].gptk << " = " << (k >= 0 ? kKeys[static_cast<std::size_t>(k)].gptk : "") << '\n';
    }
    // Left stick mirrors the arrow keys.
    out << "left_analog_up = up\nleft_analog_down = down\nleft_analog_left = left\nleft_analog_right = right\n";
    return static_cast<bool>(out);
}

std::filesystem::path global_controls_file(const std::filesystem::path& home) {
    return home / "config" / "controls.ini";
}

std::filesystem::path game_controls_file(const std::filesystem::path& home, const std::string& game_id) {
    return home / "config" / "games" / std::filesystem::u8path(game_id) / "controls.ini";
}

ControlProfile load_effective_profile(const std::filesystem::path& home, const std::string& game_id) {
    ControlProfile profile = ControlProfile::defaults();
    std::string ignored;
    std::error_code ec;
    if (std::filesystem::is_regular_file(global_controls_file(home), ec))
        profile.load(global_controls_file(home), ignored);
    if (!game_id.empty() && std::filesystem::is_regular_file(game_controls_file(home, game_id), ec))
        profile.load(game_controls_file(home, game_id), ignored);
    return profile;
}

} // namespace rpg2lx

#include "rpg2lx/controls.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <utility>
#include <system_error>

namespace rpg2lx {
namespace {

// Actions first (they follow each game's own keyboard layout), then real keys.
std::vector<KeyInfo> build_keys() {
    std::vector<KeyInfo> keys;
    const char* actions[][2] = {
        {"ok", "Aceptar"}, {"escape", "Cancelar"}, {"menu", "Menu"}, {"shift", "Correr"},
        {"pageup", "Pag. anterior"}, {"pagedown", "Pag. siguiente"}, {"up", "Mover arriba"},
        {"down", "Mover abajo"}, {"left", "Mover izq."}, {"right", "Mover der."},
        {"control", "Control"}, {"tab", "Tab"},
    };
    for (std::size_t i = 0; i < std::size(actions); ++i)
        keys.push_back({std::string("[") + actions[i][1] + "]", -static_cast<int>(i) - 1, std::string("act:") + actions[i][0]});
    const std::pair<const char*, int> named[] = {
        {"Enter", 13}, {"Escape", 27}, {"Espacio", 32}, {"Shift", 16}, {"Ctrl", 17}, {"Alt", 18},
        {"Tab", 9}, {"Retroceso", 8}, {"RePag", 33}, {"AvPag", 34}, {"Flecha arriba", 38},
        {"Flecha abajo", 40}, {"Flecha izq.", 37}, {"Flecha der.", 39}, {"Insert", 45}, {"Supr", 46},
        {"Inicio", 36}, {"Fin", 35},
    };
    const char* ids[] = {"enter", "esc", "space", "shift", "ctrl", "alt", "tab", "backspace", "pageup",
                         "pagedown", "up", "down", "left", "right", "insert", "delete", "home", "end"};
    for (std::size_t i = 0; i < std::size(named); ++i) keys.push_back({named[i].first, named[i].second, ids[i]});
    for (char c = 'A'; c <= 'Z'; ++c)
        keys.push_back({std::string(1, c), c, std::string(1, static_cast<char>(c - 'A' + 'a'))});
    for (char c = '0'; c <= '9'; ++c) keys.push_back({std::string(1, c), c, std::string(1, c)});
    for (int f = 1; f <= 12; ++f) keys.push_back({"F" + std::to_string(f), 111 + f, "f" + std::to_string(f)});
    return keys;
}

const std::vector<KeyInfo>& keys() {
    static const std::vector<KeyInfo> table = build_keys();
    return table;
}

struct ButtonInfo {
    const char* ini;
    const char* label;
};

constexpr std::array<ButtonInfo, kPadButtonCount> kButtons{{
    {"south", "Btn inferior"}, {"east", "Btn derecho"}, {"west", "Btn izquierdo"}, {"north", "Btn superior"},
    {"select", "Select"}, {"start", "Start"}, {"l3", "L3"}, {"r3", "R3"}, {"l1", "L1"}, {"r1", "R1"},
    {"up", "Cruceta arriba"}, {"down", "Cruceta abajo"}, {"left", "Cruceta izq."}, {"right", "Cruceta der."},
    {"l2", "L2"}, {"r2", "R2"},
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

const std::vector<KeyInfo>& key_table() { return keys(); }

const std::vector<std::string>& mv_action_names() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> out;
        for (const auto& k : keys())
            if (k.key_code < 0) out.push_back(k.id.substr(4));
        return out;
    }();
    return names;
}

const char* pad_button_label(PadButton button) {
    return kButtons[static_cast<std::size_t>(button)].label;
}

int key_index_for_code(int key_code) {
    for (std::size_t i = 0; i < keys().size(); ++i)
        if (keys()[i].key_code == key_code) return static_cast<int>(i);
    return -1;
}

ControlProfile ControlProfile::defaults() {
    // MV actions, so every game gets its own keys for them (Enter/Z in a standard game, J in
    // one that remapped "ok"). Codes are -(index in mv_action_names()) - 1.
    ControlProfile p;
    p.key_index.fill(-1);
    auto set = [&](PadButton b, const char* action) {
        const auto& names = mv_action_names();
        const auto it = std::find(names.begin(), names.end(), action);
        p.key_index[static_cast<std::size_t>(b)] = key_index_for_code(-static_cast<int>(it - names.begin()) - 1);
    };
    set(PadButton::South, "ok");
    set(PadButton::East, "escape");
    set(PadButton::West, "shift");
    set(PadButton::North, "menu");
    set(PadButton::Start, "menu");
    set(PadButton::Select, "tab");
    set(PadButton::L1, "pageup");
    set(PadButton::R1, "pagedown");
    set(PadButton::L2, "control");
    set(PadButton::Up, "up");
    set(PadButton::Down, "down");
    set(PadButton::Left, "left");
    set(PadButton::Right, "right");
    return p;
}

int ControlProfile::key_code(PadButton button) const {
    const int index = key_index[static_cast<std::size_t>(button)];
    return index >= 0 && index < static_cast<int>(keys().size()) ? keys()[static_cast<std::size_t>(index)].key_code : 0;
}

void ControlProfile::cycle(PadButton button, int direction) {
    // -1 (unassigned) is part of the cycle so a button can be disabled.
    const int count = static_cast<int>(keys().size()) + 1;
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
            for (std::size_t k = 0; k < keys().size(); ++k)
                if (value == keys()[k].id) key_index[b] = static_cast<int>(k);
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
            out << kButtons[b].ini << " = " << (k >= 0 ? keys()[static_cast<std::size_t>(k)].id : std::string("none")) << '\n';
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

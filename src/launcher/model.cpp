#include "rpg2lx/launcher.hpp"
#include "rpg2lx/core.hpp"
#include "rpg2lx/mv_engine.hpp"
#include "rpg2lx/project_source.hpp"
#include <algorithm>
#include <cstdint>

namespace rpg2lx {
LauncherModel::LauncherModel(GameLibrary& library) : library_(library) { refresh(); }

const GameEntry* LauncherModel::selected() const {
    return index_ < games_.size() ? &games_[index_] : nullptr;
}

bool LauncherModel::open_project(const std::filesystem::path& project, bool title) {
    const auto resolved = resolve_mv_project(project);
    if (!resolved || !resolved.source) {
        message_ = "No se pudo abrir el proyecto: " + resolved.error;
        Logger::error(message_);
        return false;
    }

    const auto& root = resolved.source->root;
    std::string save_id;
    for (const auto& game : games_) {
        if (game.detected && game.project == root) {
            save_id = game.id;
            break;
        }
    }
    if (save_id.empty()) {
        std::uint64_t hash = 14695981039346656037ULL;
        for (unsigned char ch : root.u8string()) {
            hash ^= ch;
            hash *= 1099511628211ULL;
        }
        save_id = "external-" + std::to_string(hash);
    }
    const auto save_dir = library_.home() / "saves" / std::filesystem::u8path(save_id);

    // Real RPG Maker MV project with full JS corescripts -> run 1:1 MvEngine!
    if (MvEngine::is_supported_project(root)) {
        session_.reset();
        mv_project_ = root;
        mv_save_dir_ = save_dir;
        mv_game_id_ = save_id;
        message_.clear();
        return true;
    }

    // Fallback for minimal synthetic unit-test fixtures without js/rpg_*.js
    mv_project_.clear();
    mv_save_dir_.clear();
    GameSession next;
    std::string error;
    if (!next.boot(project, error, !title)) {
        message_ = "No se pudo abrir el proyecto: " + error;
        Logger::error(message_);
        return false;
    }
    for (const auto& warning : next.warnings()) Logger::warn(warning);
    next.set_save_directory(save_dir);
    if (title) next.show_title();
    session_ = std::move(next);
    message_.clear();
    return true;
}

void LauncherModel::refresh() {
    const auto old_id = selected() ? selected()->id : std::string{};
    games_ = library_.scan(errors_);
    index_ = 0;
    bool retained = false;
    for (std::size_t i = 0; i < games_.size(); ++i) {
        if (games_[i].id == old_id) {
            index_ = i;
            retained = true;
        }
    }
    if (!retained && screen_ == HubScreen::GameMenu) screen_ = HubScreen::Library;
    message_.clear();
}

void LauncherModel::play_selected() {
    const auto* game = selected();
    if (!game || !game->detected) {
        message_ = "Carpeta incompleta. Revisa los archivos del juego.";
        return;
    }
    std::string error;
    if (!library_.prepare_game(*game, error)) {
        message_ = "No se pudo iniciar: " + error;
        Logger::error(message_);
        return;
    }
    if (!MvEngine::is_supported_project(game->project)) {
        message_ = "Faltan js/rpg_*.js: este juego no se puede ejecutar.";
        if (!hub_mode_) open_project(game->project);
        return;
    }
    const Engine engine = load_engine(library_.home(), game->id);
    if (engine == Engine::Browser && (!hub_mode_ || !browser_engine_installed(library_.home()))) {
        message_ = hub_mode_ ? "Falta NW.js en la carpeta rpg2lx/nwjs/." : "El motor navegador solo funciona desde PortMaster.";
        return;
    }
    if (hub_mode_) {
        const auto resolved = resolve_mv_project(game->project);
        launch_request_ = LaunchRequest{resolved ? resolved.source->root : game->project, game->id, engine};
        quit_ = true;
        return;
    }
    open_project(game->project);
}

void LauncherModel::open_controls(const std::string& game_id) {
    controls_game_id_ = game_id;
    controls_ = load_effective_profile(library_.home(), game_id);
    controls_return_ = screen_;
    screen_ = HubScreen::Controls;
    menu_index_ = 0;
    message_.clear();
}

void LauncherModel::close_controls() {
    std::string error;
    const auto file = controls_game_id_.empty() ? global_controls_file(library_.home())
                                                : game_controls_file(library_.home(), controls_game_id_);
    message_ = controls_.save(file, error) ? "Controles guardados." : error;
    screen_ = controls_return_;
    menu_index_ = controls_return_ == HubScreen::GameMenu ? static_cast<std::size_t>(GameMenuItem::GameControls) : 0;
}

void LauncherModel::pad_pressed(PadButton button) {
    if (screen_ != HubScreen::Controls) return;
    // Navigation buttons keep their meaning; the rest jump to their own row.
    switch (button) {
        case PadButton::South: case PadButton::East:
        case PadButton::Up: case PadButton::Down: case PadButton::Left: case PadButton::Right:
            return;
        default:
            menu_index_ = static_cast<std::size_t>(button);
    }
}

void LauncherModel::act(MenuAction action) {
    if (!mv_project_.empty()) {
        if (action == MenuAction::CloseSession) {
            mv_project_.clear();
            mv_save_dir_.clear();
            mv_game_id_.clear();
            message_.clear();
        }
        return;
    }
    if (session_) {
        if (action == MenuAction::CloseSession) { session_.reset(); message_.clear(); }
        else if (action == MenuAction::Back) session_->cancel();
        else if (action == MenuAction::Up) session_->navigate(8);
        else if (action == MenuAction::Down) session_->navigate(2);
        else if (action == MenuAction::Confirm) session_->confirm();
        if (session_ && session_->wants_library()) { session_.reset(); message_.clear(); }
        return;
    }

    if (screen_ == HubScreen::Controls) {
        if (action == MenuAction::Up) menu_index_ = (menu_index_ + kControlsRowCount - 1) % kControlsRowCount;
        else if (action == MenuAction::Down) menu_index_ = (menu_index_ + 1) % kControlsRowCount;
        else if ((action == MenuAction::Left || action == MenuAction::Right) && menu_index_ < kPadButtonCount)
            controls_.cycle(static_cast<PadButton>(menu_index_), action == MenuAction::Left ? -1 : 1);
        else if (action == MenuAction::Confirm && menu_index_ == kPadButtonCount) {
            controls_ = ControlProfile::defaults();
            message_ = "Controles por defecto.";
        } else if (action == MenuAction::Confirm && menu_index_ < kPadButtonCount)
            controls_.cycle(static_cast<PadButton>(menu_index_), 1);
        else if (action == MenuAction::Back || action == MenuAction::OpenControls) close_controls();
        return;
    }

    if (screen_ == HubScreen::GameMenu) {
        const auto* game = selected();
        if (!game) { screen_ = HubScreen::Library; return; }
        const auto item = static_cast<GameMenuItem>(menu_index_);
        if (action == MenuAction::Up) menu_index_ = (menu_index_ + kGameMenuCount - 1) % kGameMenuCount;
        else if (action == MenuAction::Down) menu_index_ = (menu_index_ + 1) % kGameMenuCount;
        else if (action == MenuAction::Back) { screen_ = HubScreen::Library; message_.clear(); }
        else if (action == MenuAction::Play) play_selected();
        else if (item == GameMenuItem::Engine &&
                 (action == MenuAction::Left || action == MenuAction::Right || action == MenuAction::Confirm)) {
            engine_ = engine_ == Engine::Native ? Engine::Browser : Engine::Native;
            save_engine(library_.home(), game->id, engine_);
            message_ = engine_ == Engine::Browser && !browser_available() ? "Falta NW.js en la carpeta rpg2lx/nwjs/." : "";
        } else if (action == MenuAction::Confirm) {
            switch (item) {
                case GameMenuItem::Play: play_selected(); break;
                case GameMenuItem::GameControls: open_controls(game->id); break;
                case GameMenuItem::ResetGameControls: {
                    std::error_code ec;
                    std::filesystem::remove(game_controls_file(library_.home(), game->id), ec);
                    message_ = "Este juego usa ahora los controles globales.";
                    break;
                }
                case GameMenuItem::Back: screen_ = HubScreen::Library; message_.clear(); break;
                default: break;
            }
        }
        return;
    }

    // Library
    if (action == MenuAction::Refresh) { refresh(); return; }
    if (action == MenuAction::OpenControls) { open_controls({}); return; }
    if (action == MenuAction::Back) { quit_ = true; return; }
    if (action == MenuAction::Up && !games_.empty()) {
        index_ = index_ == 0 ? games_.size() - 1 : index_ - 1;
        message_.clear();
    }
    if (action == MenuAction::Down && !games_.empty()) {
        index_ = (index_ + 1) % games_.size();
        message_.clear();
    }
    if (action == MenuAction::Play) { play_selected(); return; }
    if (action != MenuAction::Confirm || !selected()) return;
    if (!selected()->detected) {
        message_ = "Carpeta incompleta. Revisa los archivos del juego.";
        return;
    }
    screen_ = HubScreen::GameMenu;
    menu_index_ = 0;
    engine_ = load_engine(library_.home(), selected()->id);
    message_.clear();
}
} // namespace rpg2lx

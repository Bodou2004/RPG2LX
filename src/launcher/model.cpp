#include "rpg2lx/launcher.hpp"
#include "rpg2lx/core.hpp"
#include <algorithm>

namespace rpg2lx {
LauncherModel::LauncherModel(GameLibrary& library) : library_(library) { refresh(); }

const GameEntry* LauncherModel::selected() const {
    return index_ < games_.size() ? &games_[index_] : nullptr;
}

bool LauncherModel::open_project(const std::filesystem::path& project, bool title) {
    GameSession next;
    std::string error;
    if(!next.boot(project,error,!title)) {
        message_="No se pudo abrir el proyecto: "+error;
        Logger::error(message_);return false;
    }
    for(const auto& warning:next.warnings())Logger::warn(warning);
    std::string save_id;
    for(const auto& game:games_)if(game.detected&&game.project==next.project()){save_id=game.id;break;}
    if(save_id.empty()) {
        // Direct exports with the same folder name must not share saves.
        std::uint64_t hash=14695981039346656037ULL;
        for(unsigned char ch:next.project().u8string()){hash^=ch;hash*=1099511628211ULL;}
        save_id="external-"+std::to_string(hash);
    }
    next.set_save_directory(library_.home()/"saves"/std::filesystem::u8path(save_id));
    if(title)next.show_title();
    session_=std::move(next);message_.clear();return true;
}

void LauncherModel::refresh() {
    const auto old_id = selected() ? selected()->id : std::string{};
    games_ = library_.scan(errors_);
    index_ = 0;
    bool retained = false;
    for (std::size_t i = 0; i < games_.size(); ++i)
        if (games_[i].id == old_id) { index_ = i; retained = true; }
    if (!retained) details_ = false;
    message_.clear();
}

void LauncherModel::act(MenuAction action) {
    if (session_) {
        if (action == MenuAction::CloseSession) { session_.reset(); message_.clear(); }
        else if (action == MenuAction::Back) session_->cancel();
        else if (action == MenuAction::Up) session_->navigate(8);
        else if (action == MenuAction::Down) session_->navigate(2);
        else if (action == MenuAction::Confirm) session_->confirm();
        if(session_&&session_->wants_library()){session_.reset();message_.clear();}
        return;
    }
    if (action == MenuAction::Refresh) { refresh(); return; }
    if (action == MenuAction::Back) {
        if (details_) { details_ = false; message_.clear(); }
        else quit_ = true;
        return;
    }
    if (action == MenuAction::Up && !details_ && !games_.empty())
        index_ = index_ == 0 ? games_.size() - 1 : index_ - 1;
    if (action == MenuAction::Down && !details_ && !games_.empty())
        index_ = (index_ + 1) % games_.size();
    if (action == MenuAction::Play && selected() && selected()->detected) {
        std::string error;
        if (!library_.prepare_game(*selected(),error)) {
            message_ = "No se pudo iniciar: " + error;
            Logger::error(message_); return;
        }
        open_project(selected()->project);return;
    }
    if (action != MenuAction::Confirm || !selected()) return;
    if (!details_) { details_ = true; return; }
    if (!selected()->detected) { message_ = "Carpeta incompleta. Revisa los archivos del juego."; return; }
    const auto report = ProjectInspector::inspect(selected()->project.u8string());
    if (!report.is_valid) {
        message_ = "Hay errores en los datos. Consulta logs/launcher.log.";
        for (const auto& text : report.missing_files) Logger::error("Missing: " + text);
        for (const auto& text : report.warnings) Logger::error(text);
        return;
    }
    std::string error;
    if (!library_.prepare_game(*selected(), error)) { message_ = error; return; }
    message_ = "Datos comprobados. El motor de juego sigue en desarrollo.";
}
} // namespace rpg2lx

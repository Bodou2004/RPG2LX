#pragma once
#include "rpg2lx/session.hpp"
#include <memory>
struct SDL_Renderer;

namespace rpg2lx {
class MapView {
public:
    explicit MapView(SDL_Renderer* renderer);
    ~MapView();
    bool render(const GameSession& session, unsigned milliseconds, std::string& error);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rpg2lx

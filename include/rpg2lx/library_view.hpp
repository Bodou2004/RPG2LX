#pragma once

#include <memory>

struct SDL_Renderer;

namespace rpg2lx {

class LauncherModel;

class LibraryView {
public:
    explicit LibraryView(SDL_Renderer* renderer);
    ~LibraryView();

    LibraryView(const LibraryView&) = delete;
    LibraryView& operator=(const LibraryView&) = delete;
    LibraryView(LibraryView&&) noexcept;
    LibraryView& operator=(LibraryView&&) noexcept;

    void draw(const LauncherModel& model);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rpg2lx

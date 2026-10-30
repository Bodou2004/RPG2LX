#pragma once
#include <vector>
namespace rpg2lx {
struct TilePatch { int image, sx, sy, w, h, dx, dy; };
// Source rectangles for standard 48px MV tiles and autotile quarter pieces.
std::vector<TilePatch> tile_patches(int tile, unsigned animation_frame = 0, bool table = false);
} // namespace rpg2lx

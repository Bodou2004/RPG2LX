// MV tile source coordinates adapted from the MIT-licensed official corescript.
// Copyright (c) 2015 KADOKAWA CORPORATION./YOJI OJIMA
// See docs/legal/MV-CORESCRIPT-LICENSE.txt.
#include "rpg2lx/tile_layout.hpp"
#include "autotile_tables.hpp"
#include <cmath>
#include <stdexcept>

namespace rpg2lx {
std::vector<TilePatch> tile_patches(int tile, unsigned frame, bool table) {
    if (tile == 0) return {};
    if (tile < 0 || tile >= 8192 || (tile >= 1024 && tile < 1536) || (tile >= 1664 && tile < 2048))
        throw std::runtime_error("Invalid standard MV tile ID: " + std::to_string(tile));
    if (tile < 2048) {
        const int sheet = tile >= 1536 ? 4 : 5 + tile / 256;
        return {{sheet, ((tile/128)%2*8 + tile%8)*48, ((tile%256)/8%16)*48, 48,48,0,0}};
    }
    const int kind = (tile-2048)/48, shape = (tile-2048)%48;
    const int tx = kind%8, ty = kind/8;
    int sheet = 0, bx = 0, by = 0;
    const int (*quarters)[4][2] = floor_quarters;
    int shapes = 48;
    bool is_table = false;
    if (tile < 2816) {
        const int water[] = {0,1,2,1};
        if (kind == 0 || kind == 1) { bx = water[frame%4]*2; by = kind*3; }
        else if (kind == 2 || kind == 3) { bx = 6; by = (kind-2)*3; }
        else {
            bx = tx/4*8; by = ty*6 + (tx/2)%2*3;
            if (kind%2 == 0) bx += water[frame%4]*2;
            else { bx += 6; by += frame%3; quarters = waterfall_quarters; shapes = 4; }
        }
    } else if (tile < 4352) {
        sheet = 1; bx = tx*2; by = (ty-2)*3; is_table = table;
    } else if (tile < 5888) {
        sheet = 2; bx = tx*2; by = (ty-6)*2; quarters = wall_quarters; shapes = 16;
    } else {
        sheet = 3; bx = tx*2; by = static_cast<int>(std::floor((ty-10)*2.5 + (ty%2 ? 0.5 : 0)));
        if (ty%2) { quarters = wall_quarters; shapes = 16; }
    }
    if (shape >= shapes) throw std::runtime_error("Invalid autotile shape for tile " + std::to_string(tile));
    std::vector<TilePatch> result;
    for (int i = 0; i < 4; ++i) {
        const int qx = quarters[shape][i][0], qy = quarters[shape][i][1];
        const int sx = (bx*2+qx)*24, sy = (by*2+qy)*24;
        const int dx = (i%2)*24, dy = (i/2)*24;
        if (is_table && (qy == 1 || qy == 5)) {
            const int order[] = {0,3,2,1};
            const int other = qy == 1 ? order[qx] : qx;
            result.push_back({sheet,(bx*2+other)*24,(by*2+3)*24,24,24,dx,dy});
            result.push_back({sheet,sx,sy,24,12,dx,dy+12});
        } else result.push_back({sheet,sx,sy,24,24,dx,dy});
    }
    return result;
}
} // namespace rpg2lx

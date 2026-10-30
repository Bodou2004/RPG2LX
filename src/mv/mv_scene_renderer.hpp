#pragma once

#include "mv_bitmap_ops.hpp"
#include "rpg2lx/tile_layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

extern "C" {
#include "quickjs.h"
}

namespace rpg2lx::mv_detail {

struct MvAtoms {
    JSAtom visible = JS_ATOM_NULL;
    JSAtom renderable = JS_ATOM_NULL;
    JSAtom x = JS_ATOM_NULL;
    JSAtom y = JS_ATOM_NULL;
    JSAtom scale = JS_ATOM_NULL;
    JSAtom pivot = JS_ATOM_NULL;
    JSAtom anchor = JS_ATOM_NULL;
    JSAtom rotation = JS_ATOM_NULL;
    JSAtom alpha = JS_ATOM_NULL;
    JSAtom blendMode = JS_ATOM_NULL;
    JSAtom children = JS_ATOM_NULL;
    JSAtom length = JS_ATOM_NULL;
    JSAtom _bitmap = JS_ATOM_NULL;
    JSAtom _activeNativeRef = JS_ATOM_NULL;
    JSAtom _frame = JS_ATOM_NULL;
    JSAtom _realFrame = JS_ATOM_NULL;
    JSAtom width = JS_ATOM_NULL;
    JSAtom height = JS_ATOM_NULL;
    JSAtom _width = JS_ATOM_NULL;
    JSAtom _height = JS_ATOM_NULL;
    JSAtom _blendColor = JS_ATOM_NULL;
    JSAtom _colorTone = JS_ATOM_NULL;
    JSAtom _isWindow = JS_ATOM_NULL;
    JSAtom _openness = JS_ATOM_NULL;
    JSAtom _windowMask = JS_ATOM_NULL;
    JSAtom _isTilingSprite = JS_ATOM_NULL;
    JSAtom origin = JS_ATOM_NULL;
    JSAtom tilePosition = JS_ATOM_NULL;
    JSAtom tileScale = JS_ATOM_NULL;
    JSAtom _isPixiGraphics = JS_ATOM_NULL;
    JSAtom _graphics = JS_ATOM_NULL;
    JSAtom _red = JS_ATOM_NULL;
    JSAtom _green = JS_ATOM_NULL;
    JSAtom _blue = JS_ATOM_NULL;
    JSAtom _gray = JS_ATOM_NULL;
    JSAtom _nativeTilePass = JS_ATOM_NULL;
    JSAtom _parentTilemap = JS_ATOM_NULL;
    JSAtom _mapWidth = JS_ATOM_NULL;
    JSAtom _mapHeight = JS_ATOM_NULL;
    JSAtom _tileWidth = JS_ATOM_NULL;
    JSAtom _tileHeight = JS_ATOM_NULL;
    JSAtom _mapData = JS_ATOM_NULL;
    JSAtom flags = JS_ATOM_NULL;
    JSAtom bitmaps = JS_ATOM_NULL;
    JSAtom animationFrame = JS_ATOM_NULL;
    JSAtom horizontalWrap = JS_ATOM_NULL;
    JSAtom verticalWrap = JS_ATOM_NULL;
    JSAtom _mapRevision = JS_ATOM_NULL;

    void init(JSContext* ctx) {
        visible = JS_NewAtom(ctx, "visible");
        renderable = JS_NewAtom(ctx, "renderable");
        x = JS_NewAtom(ctx, "x");
        y = JS_NewAtom(ctx, "y");
        scale = JS_NewAtom(ctx, "scale");
        pivot = JS_NewAtom(ctx, "pivot");
        anchor = JS_NewAtom(ctx, "anchor");
        rotation = JS_NewAtom(ctx, "rotation");
        alpha = JS_NewAtom(ctx, "alpha");
        blendMode = JS_NewAtom(ctx, "blendMode");
        children = JS_NewAtom(ctx, "children");
        length = JS_NewAtom(ctx, "length");
        _bitmap = JS_NewAtom(ctx, "_bitmap");
        _activeNativeRef = JS_NewAtom(ctx, "_activeNativeRef");
        _frame = JS_NewAtom(ctx, "_frame");
        _realFrame = JS_NewAtom(ctx, "_realFrame");
        width = JS_NewAtom(ctx, "width");
        height = JS_NewAtom(ctx, "height");
        _width = JS_NewAtom(ctx, "_width");
        _height = JS_NewAtom(ctx, "_height");
        _blendColor = JS_NewAtom(ctx, "_blendColor");
        _colorTone = JS_NewAtom(ctx, "_colorTone");
        _isWindow = JS_NewAtom(ctx, "_isWindow");
        _openness = JS_NewAtom(ctx, "_openness");
        _windowMask = JS_NewAtom(ctx, "_windowMask");
        _isTilingSprite = JS_NewAtom(ctx, "_isTilingSprite");
        origin = JS_NewAtom(ctx, "origin");
        tilePosition = JS_NewAtom(ctx, "tilePosition");
        tileScale = JS_NewAtom(ctx, "tileScale");
        _isPixiGraphics = JS_NewAtom(ctx, "_isPixiGraphics");
        _graphics = JS_NewAtom(ctx, "_graphics");
        _red = JS_NewAtom(ctx, "_red");
        _green = JS_NewAtom(ctx, "_green");
        _blue = JS_NewAtom(ctx, "_blue");
        _gray = JS_NewAtom(ctx, "_gray");
        _nativeTilePass = JS_NewAtom(ctx, "_nativeTilePass");
        _parentTilemap = JS_NewAtom(ctx, "_parentTilemap");
        _mapWidth = JS_NewAtom(ctx, "_mapWidth");
        _mapHeight = JS_NewAtom(ctx, "_mapHeight");
        _tileWidth = JS_NewAtom(ctx, "_tileWidth");
        _tileHeight = JS_NewAtom(ctx, "_tileHeight");
        _mapData = JS_NewAtom(ctx, "_mapData");
        flags = JS_NewAtom(ctx, "flags");
        bitmaps = JS_NewAtom(ctx, "bitmaps");
        animationFrame = JS_NewAtom(ctx, "animationFrame");
        horizontalWrap = JS_NewAtom(ctx, "horizontalWrap");
        verticalWrap = JS_NewAtom(ctx, "verticalWrap");
        _mapRevision = JS_NewAtom(ctx, "_mapRevision");
    }

    void release(JSContext* ctx) {
        if (!ctx) return;
        JSAtom* all[] = {
            &visible, &renderable, &x, &y, &scale, &pivot, &anchor, &rotation,
            &alpha, &blendMode, &children, &length, &_bitmap, &_activeNativeRef,
            &_frame, &_realFrame, &width, &height, &_width, &_height,
            &_blendColor, &_colorTone, &_isWindow, &_openness, &_windowMask,
            &_isTilingSprite, &origin, &tilePosition, &tileScale,
            &_isPixiGraphics, &_graphics, &_red, &_green, &_blue, &_gray,
            &_nativeTilePass, &_parentTilemap, &_mapWidth, &_mapHeight,
            &_tileWidth, &_tileHeight, &_mapData, &flags, &bitmaps,
            &animationFrame, &horizontalWrap, &verticalWrap, &_mapRevision
        };
        for (JSAtom* a : all) {
            if (*a != JS_ATOM_NULL) {
                JS_FreeAtom(ctx, *a);
                *a = JS_ATOM_NULL;
            }
        }
    }
};

inline double prop_num(JSContext* ctx, JSValueConst obj, JSAtom atom, double def = 0.0) {
    JSValue v = JS_GetProperty(ctx, obj, atom);
    if (JS_IsUndefined(v) || JS_IsNull(v) || JS_IsException(v)) {
        JS_FreeValue(ctx, v);
        return def;
    }
    double out = def;
    JS_ToFloat64(ctx, &out, v);
    JS_FreeValue(ctx, v);
    return std::isnan(out) ? def : out;
}

inline int prop_int(JSContext* ctx, JSValueConst obj, JSAtom atom, int def = 0) {
    JSValue v = JS_GetProperty(ctx, obj, atom);
    if (JS_IsUndefined(v) || JS_IsNull(v) || JS_IsException(v)) {
        JS_FreeValue(ctx, v);
        return def;
    }
    int32_t out = def;
    JS_ToInt32(ctx, &out, v);
    JS_FreeValue(ctx, v);
    return out;
}

inline bool prop_bool(JSContext* ctx, JSValueConst obj, JSAtom atom, bool def = false) {
    JSValue v = JS_GetProperty(ctx, obj, atom);
    if (JS_IsUndefined(v) || JS_IsException(v)) {
        JS_FreeValue(ctx, v);
        return def;
    }
    const int b = JS_ToBool(ctx, v);
    JS_FreeValue(ctx, v);
    return b < 0 ? def : (b != 0);
}

#ifdef RPG2LX_WITH_SDL

struct TransformState {
    double x = 0.0;
    double y = 0.0;
    double sx = 1.0;
    double sy = 1.0;
    double rot = 0.0;
    double alpha = 1.0;
};

class MvSceneRenderer {
public:
    void init(SDL_Renderer* renderer, JSClassID bmp_class_id, const MvAtoms* atoms) {
        renderer_ = renderer;
        bmp_class_id_ = bmp_class_id;
        atoms_ = atoms;
        SDL_RendererInfo info{};
        if (renderer_ && SDL_GetRendererInfo(renderer_, &info) == 0) {
            supports_rt_ = (info.flags & SDL_RENDERER_TARGETTEXTURE) != 0;
        }
    }

    void release() {
        destroy_rt(tile_lower_rt_);
        destroy_rt(tile_upper_rt_);
        destroy_rt(snap_rt_);
        destroy_rt(win_layer_rt_);
        cached_map_data_.clear();
        cached_flags_.clear();
        cached_map_rev_ = -1;
        tile_lower_key_ = {};
        tile_upper_key_ = {};
        renderer_ = nullptr;
    }

    void invalidate_tile_cache() {
        tile_lower_key_ = {};
        tile_upper_key_ = {};
    }

    void render_stage(JSContext* ctx, JSValueConst stage, int width, int height) {
        if (!renderer_ || !atoms_ || JS_IsUndefined(stage) || JS_IsNull(stage)) return;
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderSetClipRect(renderer_, nullptr);
        TransformState root{};
        render_node(ctx, stage, root, width, height);
        SDL_RenderSetClipRect(renderer_, nullptr);
    }

    void render_stage_to_bitmap(JSContext* ctx, JSValueConst stage, NativeBitmap* target_bmp, int width, int height) {
        if (!renderer_ || !target_bmp || width <= 0 || height <= 0) return;

        SDL_Texture* prev_target = SDL_GetRenderTarget(renderer_);
        ensure_rt(snap_rt_, snap_w_, snap_h_, width, height);

        if (snap_rt_) {
            if (SDL_SetRenderTarget(renderer_, snap_rt_) == 0) {
                render_stage(ctx, stage, width, height);
                target_bmp->resize(width, height);
                std::uint32_t* dst = target_bmp->write_pixels();
                SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ABGR8888, dst, width * 4);
                SDL_SetRenderTarget(renderer_, prev_target);
                return;
            }
        }
        // Fallback if render targets unavailable (e.g. dummy video driver)
        render_stage(ctx, stage, width, height);
        target_bmp->resize(width, height);
        std::uint32_t* dst = target_bmp->write_pixels();
        SDL_Rect r{0, 0, width, height};
        SDL_RenderReadPixels(renderer_, &r, SDL_PIXELFORMAT_ABGR8888, dst, width * 4);
    }

private:
    struct TileCacheKey {
        bool valid = false;
        bool non_empty = true;
        int start_tx = 0;
        int start_ty = 0;
        int anim_mod = -1;
        int map_rev = -1;
        int cols = 0;
        int rows = 0;
        bool matches(const TileCacheKey& o) const {
            return valid && o.valid && start_tx == o.start_tx && start_ty == o.start_ty &&
                   anim_mod == o.anim_mod && map_rev == o.map_rev && cols == o.cols && rows == o.rows;
        }
    };

    SDL_Renderer* renderer_ = nullptr;
    JSClassID bmp_class_id_ = 0;
    const MvAtoms* atoms_ = nullptr;
    bool supports_rt_ = false;

    SDL_Texture* tile_lower_rt_ = nullptr;
    int tile_lower_w_ = 0, tile_lower_h_ = 0;
    TileCacheKey tile_lower_key_{};

    SDL_Texture* tile_upper_rt_ = nullptr;
    int tile_upper_w_ = 0, tile_upper_h_ = 0;
    TileCacheKey tile_upper_key_{};

    SDL_Texture* snap_rt_ = nullptr;
    int snap_w_ = 0, snap_h_ = 0;

    SDL_Texture* win_layer_rt_ = nullptr;
    int win_layer_w_ = 0, win_layer_h_ = 0;
    bool in_win_layer_ = false;

    int cached_map_rev_ = -1;
    int cached_map_w_ = 0;
    int cached_map_h_ = 0;
    std::vector<int> cached_map_data_;
    std::vector<int> cached_flags_;

    static void destroy_rt(SDL_Texture*& tex) {
        if (tex) {
            SDL_DestroyTexture(tex);
            tex = nullptr;
        }
    }

    void ensure_rt(SDL_Texture*& tex, int& cur_w, int& cur_h, int want_w, int want_h) {
        if (!supports_rt_ || !renderer_) return;
        if (tex && cur_w == want_w && cur_h == want_h) return;
        destroy_rt(tex);
        tex = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, want_w, want_h);
        if (tex) {
            cur_w = want_w;
            cur_h = want_h;
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        } else {
            cur_w = 0;
            cur_h = 0;
        }
    }

    static SDL_BlendMode to_sdl_blend(int mv_blend) {
        switch (mv_blend) {
            case 1: return SDL_BLENDMODE_ADD;      // PIXI.BLEND_MODES.ADD
            case 2: return SDL_BLENDMODE_MOD;      // PIXI.BLEND_MODES.MULTIPLY
            case 3: return SDL_BLENDMODE_ADD;      // PIXI.BLEND_MODES.SCREEN (closest standard SDL2 hw mode)
            default: return SDL_BLENDMODE_BLEND;
        }
    }

    NativeBitmap* get_bitmap_from_js_bitmap(JSContext* ctx, JSValueConst bmp_obj) const {
        if (!JS_IsObject(bmp_obj)) return nullptr;
        JSValue ref = JS_GetProperty(ctx, bmp_obj, atoms_->_activeNativeRef);
        if (!JS_IsObject(ref)) {
            JS_FreeValue(ctx, ref);
            return nullptr;
        }
        auto* nb = static_cast<NativeBitmap*>(JS_GetOpaque(ref, bmp_class_id_));
        JS_FreeValue(ctx, ref);
        return nb;
    }

    void sync_tilemap_arrays(JSContext* ctx, JSValueConst tm_obj, int rev, int mw, int mh) {
        if (cached_map_rev_ == rev && cached_map_w_ == mw && cached_map_h_ == mh && !cached_map_data_.empty()) {
            return;
        }
        cached_map_rev_ = rev;
        cached_map_w_ = mw;
        cached_map_h_ = mh;
        cached_map_data_.clear();
        cached_flags_.clear();

        JSValue data_arr = JS_GetProperty(ctx, tm_obj, atoms_->_mapData);
        if (JS_IsObject(data_arr)) {
            const int len = prop_int(ctx, data_arr, atoms_->length, 0);
            if (len > 0 && len <= 4 * 1024 * 1024) {
                cached_map_data_.resize(static_cast<std::size_t>(len), 0);
                for (int i = 0; i < len; ++i) {
                    JSValue el = JS_GetPropertyUint32(ctx, data_arr, static_cast<uint32_t>(i));
                    int32_t v = 0;
                    JS_ToInt32(ctx, &v, el);
                    JS_FreeValue(ctx, el);
                    cached_map_data_[static_cast<std::size_t>(i)] = v;
                }
            }
        }
        JS_FreeValue(ctx, data_arr);

        JSValue flags_arr = JS_GetProperty(ctx, tm_obj, atoms_->flags);
        if (JS_IsObject(flags_arr)) {
            const int flen = std::min(8192, prop_int(ctx, flags_arr, atoms_->length, 0));
            if (flen > 0) {
                cached_flags_.resize(static_cast<std::size_t>(flen), 0);
                for (int i = 0; i < flen; ++i) {
                    JSValue el = JS_GetPropertyUint32(ctx, flags_arr, static_cast<uint32_t>(i));
                    int32_t v = 0;
                    JS_ToInt32(ctx, &v, el);
                    JS_FreeValue(ctx, el);
                    cached_flags_[static_cast<std::size_t>(i)] = v;
                }
            }
        }
        JS_FreeValue(ctx, flags_arr);
        invalidate_tile_cache();
    }

    int read_tile(int mx, int my, int z, bool hwrap, bool vwrap) const {
        if (cached_map_data_.empty() || cached_map_w_ <= 0 || cached_map_h_ <= 0) return 0;
        if (hwrap) {
            mx %= cached_map_w_;
            if (mx < 0) mx += cached_map_w_;
        }
        if (vwrap) {
            my %= cached_map_h_;
            if (my < 0) my += cached_map_h_;
        }
        if (mx < 0 || mx >= cached_map_w_ || my < 0 || my >= cached_map_h_) return 0;
        const std::size_t idx = (static_cast<std::size_t>(z) * cached_map_h_ + my) * cached_map_w_ + mx;
        return idx < cached_map_data_.size() ? cached_map_data_[idx] : 0;
    }

    bool is_higher_tile(int tid) const {
        if (tid <= 0 || static_cast<std::size_t>(tid) >= cached_flags_.size()) return false;
        return (cached_flags_[static_cast<std::size_t>(tid)] & 0x10) != 0;
    }

    bool is_table_tile(int tid) const {
        if (tid < 2816 || tid >= 4352 || static_cast<std::size_t>(tid) >= cached_flags_.size()) return false;
        return (cached_flags_[static_cast<std::size_t>(tid)] & 0x80) != 0;
    }

    static bool is_shadowing_tile(int tid) {
        return tid >= 4352 && tid < 8192;
    }

    void draw_single_tile(SDL_Texture* const sheets[9], int tid, unsigned anim_frame, int dx, int dy) {
        if (tid <= 0 || tid >= 8192) return;
        std::vector<TilePatch> patches;
        try {
            patches = tile_patches(tid, anim_frame, is_table_tile(tid));
        } catch (...) {
            return;
        }
        for (const auto& p : patches) {
            if (p.image < 0 || p.image >= 9 || !sheets[p.image]) continue;
            SDL_Rect src{p.sx, p.sy, p.w, p.h};
            SDL_Rect dst{dx + p.dx, dy + p.dy, p.w, p.h};
            SDL_RenderCopy(renderer_, sheets[p.image], &src, &dst);
        }
    }

    void draw_table_edge(SDL_Texture* const sheets[9], int upper_tid, unsigned anim_frame, int dx, int dy) {
        if (upper_tid < 2816 || upper_tid >= 4352 || !sheets[1]) return;
        std::vector<TilePatch> patches;
        try {
            patches = tile_patches(upper_tid, anim_frame, false);
        } catch (...) {
            return;
        }
        if (patches.size() < 4) return;
        for (int i = 0; i < 2; ++i) {
            const auto& p = patches[static_cast<std::size_t>(2 + i)];
            SDL_Rect src{p.sx, p.sy + 12, 24, 12};
            SDL_Rect dst{dx + i * 24, dy, 24, 12};
            SDL_RenderCopy(renderer_, sheets[1], &src, &dst);
        }
    }

    bool paint_tile_grid(SDL_Texture* const sheets[9], int pass, int start_tx, int start_ty,
                         int cols, int rows, int tw, int th, unsigned anim_frame,
                         bool hwrap, bool vwrap, int base_x, int base_y) {
        bool drew_any = false;
        for (int gy = 0; gy < rows; ++gy) {
            const int my = start_ty + gy;
            const int dy = base_y + gy * th;
            for (int gx = 0; gx < cols; ++gx) {
                const int mx = start_tx + gx;
                const int dx = base_x + gx * tw;

                const int t0 = read_tile(mx, my, 0, hwrap, vwrap);
                const int t1 = read_tile(mx, my, 1, hwrap, vwrap);
                const int t2 = read_tile(mx, my, 2, hwrap, vwrap);
                const int t3 = read_tile(mx, my, 3, hwrap, vwrap);

                if (pass == 1) {
                    if (t0 > 0 && !is_higher_tile(t0)) { draw_single_tile(sheets, t0, anim_frame, dx, dy); drew_any = true; }
                    if (t1 > 0 && !is_higher_tile(t1)) { draw_single_tile(sheets, t1, anim_frame, dx, dy); drew_any = true; }

                    const int shadow = read_tile(mx, my, 4, hwrap, vwrap) & 0x0F;
                    if (shadow != 0) {
                        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 128);
                        const int qw = tw / 2;
                        const int qh = th / 2;
                        for (int q = 0; q < 4; ++q) {
                            if (shadow & (1 << q)) {
                                SDL_Rect sr{dx + (q % 2) * qw, dy + (q / 2) * qh, qw, qh};
                                SDL_RenderFillRect(renderer_, &sr);
                                drew_any = true;
                            }
                        }
                    }

                    const int upper_t1 = read_tile(mx, my - 1, 1, hwrap, vwrap);
                    if (is_table_tile(upper_t1) && !is_table_tile(t1) && !is_shadowing_tile(t0)) {
                        draw_table_edge(sheets, upper_t1, anim_frame, dx, dy);
                        drew_any = true;
                    }

                    if (t2 > 0 && !is_higher_tile(t2)) { draw_single_tile(sheets, t2, anim_frame, dx, dy); drew_any = true; }
                    if (t3 > 0 && !is_higher_tile(t3)) { draw_single_tile(sheets, t3, anim_frame, dx, dy); drew_any = true; }
                } else {
                    if (t0 > 0 && is_higher_tile(t0)) { draw_single_tile(sheets, t0, anim_frame, dx, dy); drew_any = true; }
                    if (t1 > 0 && is_higher_tile(t1)) { draw_single_tile(sheets, t1, anim_frame, dx, dy); drew_any = true; }
                    if (t2 > 0 && is_higher_tile(t2)) { draw_single_tile(sheets, t2, anim_frame, dx, dy); drew_any = true; }
                    if (t3 > 0 && is_higher_tile(t3)) { draw_single_tile(sheets, t3, anim_frame, dx, dy); drew_any = true; }
                }
            }
        }
        return drew_any;
    }

    void render_tilemap_pass(JSContext* ctx, JSValueConst layer_obj, int pass,
                             const TransformState& tr, int screen_w, int screen_h) {
        JSValue tm = JS_GetProperty(ctx, layer_obj, atoms_->_parentTilemap);
        if (!JS_IsObject(tm)) {
            JS_FreeValue(ctx, tm);
            return;
        }

        const int mw = prop_int(ctx, tm, atoms_->_mapWidth, 0);
        const int mh = prop_int(ctx, tm, atoms_->_mapHeight, 0);
        const int tw = std::max(1, prop_int(ctx, tm, atoms_->_tileWidth, 48));
        const int th = std::max(1, prop_int(ctx, tm, atoms_->_tileHeight, 48));
        const int rev = prop_int(ctx, tm, atoms_->_mapRevision, 0);
        const int anim_frame = std::max(0, prop_int(ctx, tm, atoms_->animationFrame, 0));
        const bool hwrap = prop_bool(ctx, tm, atoms_->horizontalWrap, false);
        const bool vwrap = prop_bool(ctx, tm, atoms_->verticalWrap, false);

        JSValue orig = JS_GetProperty(ctx, tm, atoms_->origin);
        const int ox = static_cast<int>(std::floor(prop_num(ctx, orig, atoms_->x, 0.0)));
        const int oy = static_cast<int>(std::floor(prop_num(ctx, orig, atoms_->y, 0.0)));
        JS_FreeValue(ctx, orig);

        SDL_Texture* sheets[9] = {nullptr};
        bool all_ready = true;
        JSValue bmps = JS_GetProperty(ctx, tm, atoms_->bitmaps);
        if (JS_IsObject(bmps)) {
            const int blen = std::min(9, prop_int(ctx, bmps, atoms_->length, 0));
            for (int i = 0; i < blen; ++i) {
                JSValue b = JS_GetPropertyUint32(ctx, bmps, static_cast<uint32_t>(i));
                if (JS_IsObject(b)) {
                    NativeBitmap* nb = get_bitmap_from_js_bitmap(ctx, b);
                    if (nb) {
                        sheets[i] = nb->get_texture(renderer_);
                        if (sheets[i]) {
                            SDL_SetTextureAlphaMod(sheets[i], 255);
                            SDL_SetTextureColorMod(sheets[i], 255, 255, 255);
                            SDL_SetTextureBlendMode(sheets[i], SDL_BLENDMODE_BLEND);
                        }
                    } else {
                        all_ready = false;
                    }
                }
                JS_FreeValue(ctx, b);
            }
        }
        JS_FreeValue(ctx, bmps);

        if (mw > 0 && mh > 0 && all_ready) {
            sync_tilemap_arrays(ctx, tm, rev, mw, mh);
        }
        JS_FreeValue(ctx, tm);

        if (mw <= 0 || mh <= 0 || !all_ready || cached_map_data_.empty()) return;

        const int start_tx = static_cast<int>(std::floor(static_cast<double>(ox) / tw));
        const int start_ty = static_cast<int>(std::floor(static_cast<double>(oy) / th));
        const int cols = (screen_w + tw - 1) / tw + 2;
        const int rows = (screen_h + th - 1) / th + 2;
        const int rt_w = cols * tw;
        const int rt_h = rows * th;

        SDL_Texture*& cache_rt = (pass == 1) ? tile_lower_rt_ : tile_upper_rt_;
        int& cache_w = (pass == 1) ? tile_lower_w_ : tile_upper_w_;
        int& cache_h = (pass == 1) ? tile_lower_h_ : tile_upper_h_;
        TileCacheKey& cache_key = (pass == 1) ? tile_lower_key_ : tile_upper_key_;

        ensure_rt(cache_rt, cache_w, cache_h, rt_w, rt_h);

        TileCacheKey want_key{true, true, start_tx, start_ty, anim_frame % 12, rev, cols, rows};
        if (cache_rt) {
            if (!cache_key.matches(want_key)) {
                SDL_Texture* prev_rt = SDL_GetRenderTarget(renderer_);
                if (SDL_SetRenderTarget(renderer_, cache_rt) == 0) {
                    SDL_RenderSetClipRect(renderer_, nullptr);
                    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
                    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
                    SDL_RenderClear(renderer_);
                    want_key.non_empty = paint_tile_grid(sheets, pass, start_tx, start_ty, cols, rows, tw, th,
                                                         static_cast<unsigned>(anim_frame), hwrap, vwrap, 0, 0);
                    SDL_SetRenderTarget(renderer_, prev_rt);
                    cache_key = want_key;
                }
            }
            if (cache_key.valid) {
                if (!cache_key.non_empty) return;
                const auto a_u8 = static_cast<Uint8>(std::clamp(std::round(tr.alpha * 255.0), 0.0, 255.0));
                SDL_SetTextureAlphaMod(cache_rt, a_u8);
                SDL_Rect dst{
                    static_cast<int>(std::round(tr.x)) + (start_tx * tw - ox),
                    static_cast<int>(std::round(tr.y)) + (start_ty * th - oy),
                    rt_w,
                    rt_h
                };
                SDL_RenderCopy(renderer_, cache_rt, nullptr, &dst);
                return;
            }
        }

        // Direct fallback when SDL_RENDERER_TARGETTEXTURE is unavailable
        paint_tile_grid(sheets, pass, start_tx, start_ty, cols, rows, tw, th,
                        static_cast<unsigned>(anim_frame), hwrap, vwrap,
                        static_cast<int>(std::round(tr.x)) + (start_tx * tw - ox),
                        static_cast<int>(std::round(tr.y)) + (start_ty * th - oy));
    }

    void render_tiling_sprite(JSContext* ctx, JSValueConst node, const TransformState& tr) {
        JSValue bmp_val = JS_GetProperty(ctx, node, atoms_->_bitmap);
        NativeBitmap* nb = get_bitmap_from_js_bitmap(ctx, bmp_val);
        JS_FreeValue(ctx, bmp_val);
        if (!nb || nb->is_blank()) return;

        SDL_Texture* tex = nb->get_texture(renderer_);
        if (!tex) return;

        const int tw = static_cast<int>(std::round(prop_num(ctx, node, atoms_->_width, 0.0) * std::abs(tr.sx)));
        const int th = static_cast<int>(std::round(prop_num(ctx, node, atoms_->_height, 0.0) * std::abs(tr.sy)));
        if (tw <= 0 || th <= 0) return;

        JSValue frm = JS_GetProperty(ctx, node, atoms_->_frame);
        int fx = prop_int(ctx, frm, atoms_->x, 0);
        int fy = prop_int(ctx, frm, atoms_->y, 0);
        int fw = prop_int(ctx, frm, atoms_->width, 0);
        int fh = prop_int(ctx, frm, atoms_->height, 0);
        JS_FreeValue(ctx, frm);
        if (fw <= 0 || fh <= 0) {
            fx = 0; fy = 0; fw = nb->width; fh = nb->height;
        }
        fx = std::clamp(fx, 0, nb->width);
        fy = std::clamp(fy, 0, nb->height);
        fw = std::clamp(fw, 0, nb->width - fx);
        fh = std::clamp(fh, 0, nb->height - fy);
        if (fw <= 0 || fh <= 0) return;

        JSValue tp = JS_GetProperty(ctx, node, atoms_->tilePosition);
        int ox = static_cast<int>(std::round(prop_num(ctx, tp, atoms_->x, 0.0)));
        int oy = static_cast<int>(std::round(prop_num(ctx, tp, atoms_->y, 0.0)));
        JS_FreeValue(ctx, tp);

        ox %= fw;
        if (ox > 0) ox -= fw;
        oy %= fh;
        if (oy > 0) oy -= fh;

        const int base_x = static_cast<int>(std::round(tr.x));
        const int base_y = static_cast<int>(std::round(tr.y));

        SDL_Rect prev_clip{};
        const SDL_bool had_clip = SDL_RenderIsClipEnabled(renderer_);
        if (had_clip) SDL_RenderGetClipRect(renderer_, &prev_clip);

        SDL_Rect clip{base_x, base_y, tw, th};
        if (had_clip) {
            SDL_Rect inter{};
            if (!SDL_IntersectRect(&prev_clip, &clip, &inter)) return;
            clip = inter;
        }
        SDL_RenderSetClipRect(renderer_, &clip);

        const auto a_u8 = static_cast<Uint8>(std::clamp(std::round(tr.alpha * 255.0), 0.0, 255.0));
        const int blend = prop_int(ctx, node, atoms_->blendMode, 0);
        SDL_SetTextureBlendMode(tex, to_sdl_blend(blend));
        SDL_SetTextureAlphaMod(tex, a_u8);
        SDL_SetTextureColorMod(tex, 255, 255, 255);

        SDL_Rect src{fx, fy, fw, fh};
        if (fw == 1 && fh == 1) {
            SDL_RenderCopy(renderer_, tex, &src, &clip);
        } else {
            for (int y = oy; y < th; y += fh) {
                for (int x = ox; x < tw; x += fw) {
                    SDL_Rect dst{base_x + x, base_y + y, fw, fh};
                    SDL_RenderCopy(renderer_, tex, &src, &dst);
                }
            }
        }

        SDL_RenderSetClipRect(renderer_, had_clip ? &prev_clip : nullptr);
    }

    void render_sprite_bitmap(JSContext* ctx, JSValueConst node, NativeBitmap* nb,
                              const TransformState& tr, double piv_x, double piv_y) {
        if (!nb || nb->is_blank()) return;
        SDL_Texture* tex = nb->get_texture(renderer_);
        if (!tex) return;

        JSValue rfrm = JS_GetProperty(ctx, node, atoms_->_realFrame);
        int rx = prop_int(ctx, rfrm, atoms_->x, 0);
        int ry = prop_int(ctx, rfrm, atoms_->y, 0);
        int rw = prop_int(ctx, rfrm, atoms_->width, 0);
        int rh = prop_int(ctx, rfrm, atoms_->height, 0);
        JS_FreeValue(ctx, rfrm);

        JSValue frm = JS_GetProperty(ctx, node, atoms_->_frame);
        double fw = prop_num(ctx, frm, atoms_->width, static_cast<double>(rw));
        double fh = prop_num(ctx, frm, atoms_->height, static_cast<double>(rh));
        JS_FreeValue(ctx, frm);

        if (rw <= 0 || rh <= 0) return;

        JSValue anc = JS_GetProperty(ctx, node, atoms_->anchor);
        const double ax = prop_num(ctx, anc, atoms_->x, 0.0);
        const double ay = prop_num(ctx, anc, atoms_->y, 0.0);
        JS_FreeValue(ctx, anc);

        const double abs_sx = std::abs(tr.sx);
        const double abs_sy = std::abs(tr.sy);
        if (abs_sx < 1e-4 || abs_sy < 1e-4) return;

        const double local_ox = -ax * fw - piv_x;
        const double local_oy = -ay * fh - piv_y;

        const int dst_w = std::max(1, static_cast<int>(std::round(rw * abs_sx)));
        const int dst_h = std::max(1, static_cast<int>(std::round(rh * abs_sy)));

        int flip_flags = SDL_FLIP_NONE;
        if (tr.sx < 0.0) flip_flags |= SDL_FLIP_HORIZONTAL;
        if (tr.sy < 0.0) flip_flags |= SDL_FLIP_VERTICAL;

        SDL_Rect src{rx, ry, rw, rh};
        SDL_Rect dst{};
        SDL_Point center{};
        const bool has_rot = std::abs(tr.rot) > 1e-4;

        if (!has_rot) {
            const double dx = (tr.sx >= 0.0) ? (tr.x + local_ox * abs_sx) : (tr.x - (local_ox + rw) * abs_sx);
            const double dy = (tr.sy >= 0.0) ? (tr.y + local_oy * abs_sy) : (tr.y - (local_oy + rh) * abs_sy);
            dst = {static_cast<int>(std::round(dx)), static_cast<int>(std::round(dy)), dst_w, dst_h};
        } else {
            const double cx = (tr.sx >= 0.0) ? (-local_ox * abs_sx) : ((local_ox + rw) * abs_sx);
            const double cy = (tr.sy >= 0.0) ? (-local_oy * abs_sy) : ((local_oy + rh) * abs_sy);
            center = {static_cast<int>(std::round(cx)), static_cast<int>(std::round(cy))};
            dst = {static_cast<int>(std::round(tr.x)) - center.x,
                   static_cast<int>(std::round(tr.y)) - center.y,
                   dst_w, dst_h};
        }

        // Read optional _colorTone [r,g,b,gray] and _blendColor [r,g,b,a]
        int tone_r = 0, tone_g = 0, tone_b = 0;
        JSValue tone = JS_GetProperty(ctx, node, atoms_->_colorTone);
        if (JS_IsObject(tone)) {
            JSValue v0 = JS_GetPropertyUint32(ctx, tone, 0);
            JSValue v1 = JS_GetPropertyUint32(ctx, tone, 1);
            JSValue v2 = JS_GetPropertyUint32(ctx, tone, 2);
            JS_ToInt32(ctx, &tone_r, v0);
            JS_ToInt32(ctx, &tone_g, v1);
            JS_ToInt32(ctx, &tone_b, v2);
            JS_FreeValue(ctx, v0); JS_FreeValue(ctx, v1); JS_FreeValue(ctx, v2);
        }
        JS_FreeValue(ctx, tone);

        int bc_r = 0, bc_g = 0, bc_b = 0, bc_a = 0;
        JSValue bcol = JS_GetProperty(ctx, node, atoms_->_blendColor);
        if (JS_IsObject(bcol)) {
            JSValue v3 = JS_GetPropertyUint32(ctx, bcol, 3);
            JS_ToInt32(ctx, &bc_a, v3);
            JS_FreeValue(ctx, v3);
            if (bc_a > 0) {
                JSValue v0 = JS_GetPropertyUint32(ctx, bcol, 0);
                JSValue v1 = JS_GetPropertyUint32(ctx, bcol, 1);
                JSValue v2 = JS_GetPropertyUint32(ctx, bcol, 2);
                JS_ToInt32(ctx, &bc_r, v0);
                JS_ToInt32(ctx, &bc_g, v1);
                JS_ToInt32(ctx, &bc_b, v2);
                JS_FreeValue(ctx, v0); JS_FreeValue(ctx, v1); JS_FreeValue(ctx, v2);
            }
        }
        JS_FreeValue(ctx, bcol);

        const auto a_u8 = static_cast<Uint8>(std::clamp(std::round(tr.alpha * 255.0), 0.0, 255.0));
        const int blend = prop_int(ctx, node, atoms_->blendMode, 0);
        const double deg = tr.rot * (180.0 / 3.14159265358979323846);

        const int mod_scale = 255 - std::clamp(bc_a, 0, 255);
        const auto mod_r = static_cast<Uint8>((std::clamp(255 + std::min(0, tone_r), 0, 255) * mod_scale + 127) / 255);
        const auto mod_g = static_cast<Uint8>((std::clamp(255 + std::min(0, tone_g), 0, 255) * mod_scale + 127) / 255);
        const auto mod_b = static_cast<Uint8>((std::clamp(255 + std::min(0, tone_b), 0, 255) * mod_scale + 127) / 255);

        SDL_SetTextureBlendMode(tex, to_sdl_blend(blend));
        SDL_SetTextureAlphaMod(tex, a_u8);
        SDL_SetTextureColorMod(tex, mod_r, mod_g, mod_b);

        if (!has_rot && flip_flags == SDL_FLIP_NONE) {
            SDL_RenderCopy(renderer_, tex, &src, &dst);
        } else {
            SDL_RenderCopyEx(renderer_, tex, &src, &dst, deg,
                             has_rot ? &center : nullptr, static_cast<SDL_RendererFlip>(flip_flags));
        }

        // Additive highlight pass for positive _colorTone or _blendColor flash (masked automatically by tex alpha!)
        const int add_r = std::clamp(std::max(0, tone_r) + (bc_r * bc_a) / 255, 0, 255);
        const int add_g = std::clamp(std::max(0, tone_g) + (bc_g * bc_a) / 255, 0, 255);
        const int add_b = std::clamp(std::max(0, tone_b) + (bc_b * bc_a) / 255, 0, 255);
        if (add_r > 0 || add_g > 0 || add_b > 0) {
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(tex, a_u8);
            SDL_SetTextureColorMod(tex, static_cast<Uint8>(add_r), static_cast<Uint8>(add_g), static_cast<Uint8>(add_b));
            if (!has_rot && flip_flags == SDL_FLIP_NONE) {
                SDL_RenderCopy(renderer_, tex, &src, &dst);
            } else {
                SDL_RenderCopyEx(renderer_, tex, &src, &dst, deg,
                                 has_rot ? &center : nullptr, static_cast<SDL_RendererFlip>(flip_flags));
            }
            SDL_SetTextureColorMod(tex, 255, 255, 255);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        }
    }

    void render_window_layer(JSContext* ctx, JSValueConst node, const TransformState& tr, int screen_w, int screen_h) {
        JSValue kids = JS_GetProperty(ctx, node, atoms_->children);
        const int n = JS_IsObject(kids) ? prop_int(ctx, kids, atoms_->length, 0) : 0;
        if (n <= 0) {
            JS_FreeValue(ctx, kids);
            return;
        }

        // Count visible open windows to see if overlapping-window masking is needed
        int open_win_count = 0;
        for (int i = 0; i < n; ++i) {
            JSValue ch = JS_GetPropertyUint32(ctx, kids, static_cast<uint32_t>(i));
            if (JS_IsObject(ch) && prop_bool(ctx, ch, atoms_->_isWindow, false) &&
                prop_bool(ctx, ch, atoms_->visible, true) && prop_int(ctx, ch, atoms_->_openness, 255) > 0) {
                ++open_win_count;
            }
            JS_FreeValue(ctx, ch);
        }

        if (open_win_count >= 2 && supports_rt_ && !in_win_layer_) {
            ensure_rt(win_layer_rt_, win_layer_w_, win_layer_h_, screen_w, screen_h);
            if (win_layer_rt_) {
                SDL_Texture* prev_rt = SDL_GetRenderTarget(renderer_);
                if (SDL_SetRenderTarget(renderer_, win_layer_rt_) == 0) {
                    in_win_layer_ = true;
                    SDL_RenderSetClipRect(renderer_, nullptr);
                    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
                    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
                    SDL_RenderClear(renderer_);

                    for (int i = 0; i < n; ++i) {
                        JSValue ch = JS_GetPropertyUint32(ctx, kids, static_cast<uint32_t>(i));
                        if (JS_IsObject(ch) && prop_bool(ctx, ch, atoms_->_isWindow, false)) {
                            const int op = prop_int(ctx, ch, atoms_->_openness, 255);
                            if (prop_bool(ctx, ch, atoms_->visible, true) && op > 0) {
                                const double wx = tr.x + prop_num(ctx, ch, atoms_->x, 0.0);
                                const double wy = tr.y + prop_num(ctx, ch, atoms_->y, 0.0);
                                const double ww = prop_num(ctx, ch, atoms_->_width, 0.0);
                                const double wh = prop_num(ctx, ch, atoms_->_height, 0.0);
                                const double open_k = op / 255.0;
                                SDL_Rect mask_r{
                                    static_cast<int>(std::round(wx)),
                                    static_cast<int>(std::round(wy + wh * 0.5 * (1.0 - open_k))),
                                    static_cast<int>(std::round(ww)),
                                    static_cast<int>(std::round(wh * open_k))
                                };
                                if (mask_r.w > 0 && mask_r.h > 0) {
                                    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
                                    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
                                    SDL_RenderFillRect(renderer_, &mask_r);
                                }
                                render_node(ctx, ch, tr, screen_w, screen_h);
                            }
                        }
                        JS_FreeValue(ctx, ch);
                    }

                    in_win_layer_ = false;
                    SDL_SetRenderTarget(renderer_, prev_rt);
                    SDL_SetTextureBlendMode(win_layer_rt_, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureAlphaMod(win_layer_rt_,
                                           static_cast<Uint8>(std::clamp(std::round(tr.alpha * 255.0), 0.0, 255.0)));
                    SDL_RenderCopy(renderer_, win_layer_rt_, nullptr, nullptr);

                    // Render any non-window children inside WindowLayer
                    for (int i = 0; i < n; ++i) {
                        JSValue ch = JS_GetPropertyUint32(ctx, kids, static_cast<uint32_t>(i));
                        if (JS_IsObject(ch) && !prop_bool(ctx, ch, atoms_->_isWindow, false)) {
                            render_node(ctx, ch, tr, screen_w, screen_h);
                        }
                        JS_FreeValue(ctx, ch);
                    }
                    JS_FreeValue(ctx, kids);
                    return;
                }
            }
        }

        for (int i = 0; i < n; ++i) {
            JSValue ch = JS_GetPropertyUint32(ctx, kids, static_cast<uint32_t>(i));
            if (JS_IsObject(ch)) render_node(ctx, ch, tr, screen_w, screen_h);
            JS_FreeValue(ctx, ch);
        }
        JS_FreeValue(ctx, kids);
    }

    void render_node(JSContext* ctx, JSValueConst node, const TransformState& parent, int screen_w, int screen_h) {
        if (!JS_IsObject(node)) return;
        if (!prop_bool(ctx, node, atoms_->visible, true)) return;
        if (!prop_bool(ctx, node, atoms_->renderable, true)) return;

        const double local_alpha = std::clamp(prop_num(ctx, node, atoms_->alpha, 1.0), 0.0, 1.0);
        const double eff_alpha = parent.alpha * local_alpha;
        if (eff_alpha <= 0.001) return;

        // Window openness check
        if (prop_bool(ctx, node, atoms_->_isWindow, false)) {
            if (prop_int(ctx, node, atoms_->_openness, 255) <= 0) return;
        }

        // Compute world transform
        const double lx = prop_num(ctx, node, atoms_->x, 0.0);
        const double ly = prop_num(ctx, node, atoms_->y, 0.0);

        JSValue sc = JS_GetProperty(ctx, node, atoms_->scale);
        const double lsx = JS_IsObject(sc) ? prop_num(ctx, sc, atoms_->x, 1.0) : 1.0;
        const double lsy = JS_IsObject(sc) ? prop_num(ctx, sc, atoms_->y, 1.0) : 1.0;
        JS_FreeValue(ctx, sc);

        JSValue pv = JS_GetProperty(ctx, node, atoms_->pivot);
        const double pvx = JS_IsObject(pv) ? prop_num(ctx, pv, atoms_->x, 0.0) : 0.0;
        const double pvy = JS_IsObject(pv) ? prop_num(ctx, pv, atoms_->y, 0.0) : 0.0;
        JS_FreeValue(ctx, pv);

        const double lrot = prop_num(ctx, node, atoms_->rotation, 0.0);

        TransformState cur{};
        if (std::abs(parent.rot) < 1e-4) {
            cur.x = parent.x + lx * parent.sx;
            cur.y = parent.y + ly * parent.sy;
        } else {
            const double c = std::cos(parent.rot);
            const double s = std::sin(parent.rot);
            const double rx = lx * parent.sx;
            const double ry = ly * parent.sy;
            cur.x = parent.x + rx * c - ry * s;
            cur.y = parent.y + rx * s + ry * c;
        }
        cur.sx = parent.sx * lsx;
        cur.sy = parent.sy * lsy;
        cur.rot = parent.rot + lrot;
        cur.alpha = eff_alpha;

        // 1. Tilemap lower/upper pass?
        const int tile_pass = prop_int(ctx, node, atoms_->_nativeTilePass, 0);
        if (tile_pass == 1 || tile_pass == 2) {
            render_tilemap_pass(ctx, node, tile_pass, parent, screen_w, screen_h);
            return;
        }

        // 2. WindowLayer?
        JSValue wmask = JS_GetProperty(ctx, node, atoms_->_windowMask);
        const bool is_window_layer = JS_IsObject(wmask);
        JS_FreeValue(ctx, wmask);
        if (is_window_layer) {
            render_window_layer(ctx, node, cur, screen_w, screen_h);
            return;
        }

        // 3. TilingSprite?
        if (prop_bool(ctx, node, atoms_->_isTilingSprite, false)) {
            render_tiling_sprite(ctx, node, cur);
        } else {
            // 4. ScreenSprite or ToneSprite?
            JSValue r_val = JS_GetProperty(ctx, node, atoms_->_red);
            const bool has_rgb = JS_IsNumber(r_val);
            JS_FreeValue(ctx, r_val);
            if (has_rgb) {
                JSValue gfx = JS_GetProperty(ctx, node, atoms_->_graphics);
                const bool is_screen_sprite = JS_IsObject(gfx);
                JS_FreeValue(ctx, gfx);

                const int r = prop_int(ctx, node, atoms_->_red, 0);
                const int g = prop_int(ctx, node, atoms_->_green, 0);
                const int b = prop_int(ctx, node, atoms_->_blue, 0);

                if (is_screen_sprite) {
                    if (r >= 0 && cur.alpha > 0.002) {
                        const int blend = prop_int(ctx, node, atoms_->blendMode, 0);
                        SDL_SetRenderDrawBlendMode(renderer_, to_sdl_blend(blend));
                        SDL_SetRenderDrawColor(renderer_,
                                               static_cast<Uint8>(std::clamp(r, 0, 255)),
                                               static_cast<Uint8>(std::clamp(g, 0, 255)),
                                               static_cast<Uint8>(std::clamp(b, 0, 255)),
                                               static_cast<Uint8>(std::clamp(std::round(cur.alpha * 255.0), 0.0, 255.0)));
                        SDL_Rect full{0, 0, screen_w, screen_h};
                        SDL_RenderFillRect(renderer_, &full);
                    }
                    return; // Skip internal _graphics child
                } else {
                    // ToneSprite
                    SDL_Rect full{0, 0, screen_w, screen_h};
                    if (r < 0 || g < 0 || b < 0) {
                        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_MOD);
                        SDL_SetRenderDrawColor(renderer_,
                                               static_cast<Uint8>(std::clamp(255 + std::min(0, r), 0, 255)),
                                               static_cast<Uint8>(std::clamp(255 + std::min(0, g), 0, 255)),
                                               static_cast<Uint8>(std::clamp(255 + std::min(0, b), 0, 255)),
                                               255);
                        SDL_RenderFillRect(renderer_, &full);
                    }
                    if (r > 0 || g > 0 || b > 0) {
                        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_ADD);
                        SDL_SetRenderDrawColor(renderer_,
                                               static_cast<Uint8>(std::clamp(std::max(0, r), 0, 255)),
                                               static_cast<Uint8>(std::clamp(std::max(0, g), 0, 255)),
                                               static_cast<Uint8>(std::clamp(std::max(0, b), 0, 255)),
                                               255);
                        SDL_RenderFillRect(renderer_, &full);
                    }
                }
            } else {
                // 5. Regular Sprite with _bitmap?
                JSValue bmp_val = JS_GetProperty(ctx, node, atoms_->_bitmap);
                if (JS_IsObject(bmp_val)) {
                    NativeBitmap* nb = get_bitmap_from_js_bitmap(ctx, bmp_val);
                    if (nb) {
                        render_sprite_bitmap(ctx, node, nb, cur, pvx, pvy);
                    }
                }
                JS_FreeValue(ctx, bmp_val);
            }
        }

        // 6. Recurse into children
        JSValue kids = JS_GetProperty(ctx, node, atoms_->children);
        if (JS_IsObject(kids)) {
            const int n = prop_int(ctx, kids, atoms_->length, 0);
            if (n > 0) {
                TransformState child_tr = cur;
                child_tr.x -= pvx * cur.sx;
                child_tr.y -= pvy * cur.sy;
                for (int i = 0; i < n; ++i) {
                    JSValue ch = JS_GetPropertyUint32(ctx, kids, static_cast<uint32_t>(i));
                    if (JS_IsObject(ch)) render_node(ctx, ch, child_tr, screen_w, screen_h);
                    JS_FreeValue(ctx, ch);
                }
            }
        }
        JS_FreeValue(ctx, kids);
    }
};

#endif // RPG2LX_WITH_SDL

} // namespace rpg2lx::mv_detail

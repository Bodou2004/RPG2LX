#pragma once

#include "rpg2lx/font_renderer.hpp"
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef RPG2LX_WITH_SDL
#include <SDL.h>
#endif

namespace rpg2lx::mv_detail {

namespace fs = std::filesystem;

inline std::string url_decode_rel(const std::string& raw) {
    std::string clean;
    clean.reserve(raw.size());
    std::size_t n = raw.size();
    // Strip query string or hash
    for (std::size_t i = 0; i < n; ++i) {
        if (raw[i] == '?' || raw[i] == '#') {
            n = i;
            break;
        }
    }
    auto hex_val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    for (std::size_t i = 0; i < n; ++i) {
        if (raw[i] == '%' && i + 2 < n) {
            const int hi = hex_val(raw[i + 1]);
            const int lo = hex_val(raw[i + 2]);
            if (hi >= 0 && lo >= 0) {
                clean.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        clean.push_back(raw[i] == '\\' ? '/' : raw[i]);
    }
    while (!clean.empty() && (clean.front() == '/' || clean.front() == '.')) {
        if (clean.rfind("./", 0) == 0) {
            clean.erase(0, 2);
        } else if (clean.front() == '/') {
            clean.erase(0, 1);
        } else {
            break;
        }
    }
    return clean;
}

inline std::vector<std::uint8_t> hex_key_to_bytes(const std::string& hex) {
    std::vector<std::uint8_t> out;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        const int h = nib(hex[i]);
        const int l = nib(hex[i + 1]);
        if (h < 0 || l < 0) break;
        out.push_back(static_cast<std::uint8_t>((h << 4) | l));
    }
    return out;
}

inline bool read_binary_file(const fs::path& path, std::vector<std::uint8_t>& out) {
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) return false;
    const auto sz = fs::file_size(path, ec);
    if (ec || sz > 128u * 1024u * 1024u) return false;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.resize(static_cast<std::size_t>(sz));
    if (sz > 0) {
        f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(sz));
        if (!f) return false;
    }
    return true;
}

struct DecodedImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
#ifdef RPG2LX_WITH_SDL
    SDL_Texture* shared_tex = nullptr;
    ~DecodedImage() {
        if (shared_tex) {
            SDL_DestroyTexture(shared_tex);
            shared_tex = nullptr;
        }
    }
#endif
};

inline std::shared_ptr<DecodedImage> decode_project_image(const fs::path& project_root,
                                                          const std::string& raw_url,
                                                          const std::vector<std::uint8_t>& enc_key) {
    const std::string rel = url_decode_rel(raw_url);
    if (rel.empty() || rel.find("..") != std::string::npos) return nullptr;

    fs::path full = project_root / fs::u8path(rel);
    std::error_code ec;
    bool encrypted = false;

    std::string ext = full.extension().u8string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".rpgmvp") {
        encrypted = true;
    } else if (!fs::exists(full, ec)) {
        fs::path alt = full;
        alt.replace_extension(".rpgmvp");
        if (fs::exists(alt, ec)) {
            full = alt;
            encrypted = true;
        } else {
            return nullptr;
        }
    }

    std::vector<std::uint8_t> bytes;
    if (!read_binary_file(full, bytes) || bytes.size() < 8) return nullptr;

    const std::uint8_t* decode_ptr = bytes.data();
    std::size_t decode_len = bytes.size();
    std::vector<std::uint8_t> decrypted;

    if (encrypted || (bytes.size() > 32 && std::memcmp(bytes.data(), "RPGMV", 5) == 0)) {
        if (bytes.size() <= 32) return nullptr;
        decrypted.assign(bytes.begin() + 16, bytes.end());
        static constexpr std::uint8_t kPngSig[16] = {
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
            0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52
        };
        if (enc_key.size() >= 16) {
            for (std::size_t i = 0; i < 16; ++i) decrypted[i] ^= enc_key[i];
        } else {
            std::memcpy(decrypted.data(), kPngSig, 16);
        }
        decode_ptr = decrypted.data();
        decode_len = decrypted.size();
    }

    int w = 0, h = 0, ch = 0;
    stbi_uc* rgba = stbi_load_from_memory(decode_ptr, static_cast<int>(decode_len), &w, &h, &ch, 4);
    if (!rgba || w <= 0 || h <= 0) {
        if (rgba) stbi_image_free(rgba);
        return nullptr;
    }

    auto img = std::make_shared<DecodedImage>();
    img->width = w;
    img->height = h;
    img->pixels.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    std::memcpy(img->pixels.data(), rgba, img->pixels.size() * sizeof(std::uint32_t));
    stbi_image_free(rgba);
    return img;
}

inline std::uint32_t pack_px(std::uint32_t r, std::uint32_t g, std::uint32_t b, std::uint32_t a) {
    return (r & 0xFFu) | ((g & 0xFFu) << 8) | ((b & 0xFFu) << 16) | ((a & 0xFFu) << 24);
}

inline void composite_src_over(std::uint32_t& dst, std::uint32_t sr, std::uint32_t sg, std::uint32_t sb, std::uint32_t sa) {
    if (sa == 0) return;
    if (sa >= 255) {
        dst = pack_px(sr, sg, sb, 255);
        return;
    }
    const std::uint32_t da = (dst >> 24) & 0xFFu;
    if (da == 0) {
        dst = pack_px(sr, sg, sb, sa);
        return;
    }
    const std::uint32_t dr = dst & 0xFFu;
    const std::uint32_t dg = (dst >> 8) & 0xFFu;
    const std::uint32_t db = (dst >> 16) & 0xFFu;
    const std::uint32_t inv_sa = 255u - sa;
    const std::uint32_t out_a = sa + (da * inv_sa + 127u) / 255u;
    if (out_a == 0) {
        dst = 0;
        return;
    }
    const std::uint32_t out_r = (sr * sa * 255u + dr * da * inv_sa + out_a * 127u) / (out_a * 255u);
    const std::uint32_t out_g = (sg * sa * 255u + dg * da * inv_sa + out_a * 127u) / (out_a * 255u);
    const std::uint32_t out_b = (sb * sa * 255u + db * da * inv_sa + out_a * 127u) / (out_a * 127u + out_a * 128u);
    dst = pack_px(std::min(255u, out_r), std::min(255u, out_g), std::min(255u, out_b), std::min(255u, out_a));
}

struct NativeBitmap {
    int width = 1;
    int height = 1;
    std::shared_ptr<DecodedImage> shared_img;
    std::vector<std::uint32_t> own_pixels;
    bool dirty = true;
    bool blank = true;

#ifdef RPG2LX_WITH_SDL
    SDL_Texture* own_tex = nullptr;
    int tex_w = 0;
    int tex_h = 0;
#endif

    NativeBitmap(int w, int h)
        : width(std::max(1, std::min(w, 8192))),
          height(std::max(1, std::min(h, 8192))),
          own_pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0),
          dirty(true),
          blank(true) {}

    explicit NativeBitmap(std::shared_ptr<DecodedImage> img)
        : width(img ? img->width : 1),
          height(img ? img->height : 1),
          shared_img(std::move(img)),
          dirty(false),
          blank(false) {
        if (!shared_img) {
            own_pixels.assign(1, 0);
            dirty = true;
            blank = true;
        }
    }

    ~NativeBitmap() {
#ifdef RPG2LX_WITH_SDL
        if (own_tex) {
            SDL_DestroyTexture(own_tex);
            own_tex = nullptr;
        }
#endif
    }

    bool is_blank() const {
        if (blank && !shared_img) return true;
        if (width == 1 && height == 1) {
            return ((read_pixels()[0] >> 24) & 0xFFu) == 0;
        }
        return false;
    }

    const std::uint32_t* read_pixels() const {
        return shared_img ? shared_img->pixels.data() : own_pixels.data();
    }

    std::uint32_t* write_pixels() {
        if (shared_img) {
            own_pixels = shared_img->pixels;
            shared_img.reset();
        }
        dirty = true;
        blank = false;
        return own_pixels.data();
    }

#ifdef RPG2LX_WITH_SDL
    SDL_Texture* get_texture(SDL_Renderer* renderer) {
        if (!renderer || width <= 0 || height <= 0) return nullptr;
        if (shared_img) {
            if (!shared_img->shared_tex) {
                shared_img->shared_tex = SDL_CreateTexture(
                    renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC,
                    shared_img->width, shared_img->height);
                if (shared_img->shared_tex) {
                    SDL_SetTextureBlendMode(shared_img->shared_tex, SDL_BLENDMODE_BLEND);
                    SDL_UpdateTexture(shared_img->shared_tex, nullptr,
                                      shared_img->pixels.data(), shared_img->width * 4);
                }
            }
            return shared_img->shared_tex;
        }

        if (!own_tex || tex_w != width || tex_h != height) {
            if (own_tex) SDL_DestroyTexture(own_tex);
            own_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                        SDL_TEXTUREACCESS_STREAMING, width, height);
            tex_w = width;
            tex_h = height;
            if (own_tex) SDL_SetTextureBlendMode(own_tex, SDL_BLENDMODE_BLEND);
            dirty = true;
        }
        if (dirty && own_tex && !own_pixels.empty()) {
            SDL_UpdateTexture(own_tex, nullptr, own_pixels.data(), width * 4);
            dirty = false;
        }
        return own_tex;
    }
#endif

    void resize(int nw, int nh) {
        nw = std::clamp(nw, 1, 8192);
        nh = std::clamp(nh, 1, 8192);
        if (nw == width && nh == height && !shared_img) {
            if (!blank) {
                std::fill(own_pixels.begin(), own_pixels.end(), 0u);
                dirty = true;
                blank = true;
            }
            return;
        }
        shared_img.reset();
        width = nw;
        height = nh;
        own_pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0u);
        dirty = true;
        blank = true;
    }

    void clear_rect(int rx, int ry, int rw, int rh) {
        int x0 = std::max(0, rx);
        int y0 = std::max(0, ry);
        int x1 = std::min(width, rx + rw);
        int y1 = std::min(height, ry + rh);
        if (x0 >= x1 || y0 >= y1) return;
        if (!shared_img && blank) return;
        std::uint32_t* dst = write_pixels();
        if (x0 == 0 && x1 == width) {
            std::memset(dst + static_cast<std::size_t>(y0) * width, 0,
                        static_cast<std::size_t>(y1 - y0) * width * sizeof(std::uint32_t));
            if (y0 == 0 && y1 == height) blank = true;
        } else {
            const std::size_t row_bytes = static_cast<std::size_t>(x1 - x0) * sizeof(std::uint32_t);
            for (int y = y0; y < y1; ++y) {
                std::memset(dst + static_cast<std::size_t>(y) * width + x0, 0, row_bytes);
            }
        }
    }

    void fill_rect(int rx, int ry, int rw, int rh, RgbaColor col, int paint_opacity, const std::string& comp) {
        int x0 = std::max(0, rx);
        int y0 = std::max(0, ry);
        int x1 = std::min(width, rx + rw);
        int y1 = std::min(height, ry + rh);
        if (x0 >= x1 || y0 >= y1) return;

        const std::uint32_t eff_a = (static_cast<std::uint32_t>(col.a) *
                                     static_cast<std::uint32_t>(std::clamp(paint_opacity, 0, 255)) + 127u) / 255u;
        std::uint32_t* dst = write_pixels();

        if (comp == "copy" || (comp == "source-over" && eff_a >= 255u)) {
            const std::uint32_t px = pack_px(col.r, col.g, col.b, eff_a);
            for (int y = y0; y < y1; ++y) {
                std::fill_n(dst + static_cast<std::size_t>(y) * width + x0, x1 - x0, px);
            }
            return;
        }
        if (eff_a == 0) return;

        if (comp == "lighter") {
            const std::uint32_t ar = (col.r * eff_a + 127u) / 255u;
            const std::uint32_t ag = (col.g * eff_a + 127u) / 255u;
            const std::uint32_t ab = (col.b * eff_a + 127u) / 255u;
            for (int y = y0; y < y1; ++y) {
                std::uint32_t* row = dst + static_cast<std::size_t>(y) * width;
                for (int x = x0; x < x1; ++x) {
                    const std::uint32_t p = row[x];
                    const std::uint32_t da = (p >> 24) & 0xFFu;
                    if (!da) continue;
                    row[x] = pack_px(std::min(255u, (p & 0xFFu) + ar),
                                     std::min(255u, ((p >> 8) & 0xFFu) + ag),
                                     std::min(255u, ((p >> 16) & 0xFFu) + ab),
                                     da);
                }
            }
            return;
        }

        if (comp == "difference") {
            for (int y = y0; y < y1; ++y) {
                std::uint32_t* row = dst + static_cast<std::size_t>(y) * width;
                for (int x = x0; x < x1; ++x) {
                    const std::uint32_t p = row[x];
                    const std::uint32_t da = (p >> 24) & 0xFFu;
                    if (!da) continue;
                    const int dr = static_cast<int>(p & 0xFFu);
                    const int dg = static_cast<int>((p >> 8) & 0xFFu);
                    const int db = static_cast<int>((p >> 16) & 0xFFu);
                    row[x] = pack_px(static_cast<std::uint32_t>(std::abs(dr - static_cast<int>(col.r))),
                                     static_cast<std::uint32_t>(std::abs(dg - static_cast<int>(col.g))),
                                     static_cast<std::uint32_t>(std::abs(db - static_cast<int>(col.b))),
                                     da);
                }
            }
            return;
        }

        if (comp == "source-atop") {
            for (int y = y0; y < y1; ++y) {
                std::uint32_t* row = dst + static_cast<std::size_t>(y) * width;
                for (int x = x0; x < x1; ++x) {
                    const std::uint32_t p = row[x];
                    const std::uint32_t da = (p >> 24) & 0xFFu;
                    if (!da) continue;
                    const std::uint32_t inv = 255u - eff_a;
                    row[x] = pack_px(((p & 0xFFu) * inv + col.r * eff_a + 127u) / 255u,
                                     (((p >> 8) & 0xFFu) * inv + col.g * eff_a + 127u) / 255u,
                                     (((p >> 16) & 0xFFu) * inv + col.b * eff_a + 127u) / 255u,
                                     da);
                }
            }
            return;
        }

        if (comp == "saturation") {
            for (int y = y0; y < y1; ++y) {
                std::uint32_t* row = dst + static_cast<std::size_t>(y) * width;
                for (int x = x0; x < x1; ++x) {
                    const std::uint32_t p = row[x];
                    const std::uint32_t da = (p >> 24) & 0xFFu;
                    if (!da) continue;
                    const std::uint32_t dr = p & 0xFFu;
                    const std::uint32_t dg = (p >> 8) & 0xFFu;
                    const std::uint32_t db = (p >> 16) & 0xFFu;
                    const std::uint32_t lum = (dr * 77u + dg * 150u + db * 29u) >> 8;
                    const std::uint32_t inv = 255u - eff_a;
                    row[x] = pack_px((dr * inv + lum * eff_a + 127u) / 255u,
                                     (dg * inv + lum * eff_a + 127u) / 255u,
                                     (db * inv + lum * eff_a + 127u) / 255u,
                                     da);
                }
            }
            return;
        }

        for (int y = y0; y < y1; ++y) {
            std::uint32_t* row = dst + static_cast<std::size_t>(y) * width;
            for (int x = x0; x < x1; ++x) {
                composite_src_over(row[x], col.r, col.g, col.b, eff_a);
            }
        }
    }

    void gradient_fill_rect(int rx, int ry, int rw, int rh, RgbaColor c0, RgbaColor c1, bool vertical, int paint_opacity) {
        if (rw <= 0 || rh <= 0 || paint_opacity <= 0) return;
        const int x0 = std::max(0, rx);
        const int y0 = std::max(0, ry);
        const int x1 = std::min(width, rx + rw);
        const int y1 = std::min(height, ry + rh);
        if (x0 >= x1 || y0 >= y1) return;

        std::uint32_t* dst = write_pixels();
        const std::uint32_t op = static_cast<std::uint32_t>(std::clamp(paint_opacity, 0, 255));
        const float span = static_cast<float>(std::max(1, (vertical ? rh : rw) - 1));

        for (int y = y0; y < y1; ++y) {
            std::uint32_t* row = dst + static_cast<std::size_t>(y) * width;
            if (vertical) {
                const float t = std::clamp((y - ry) / span, 0.0f, 1.0f);
                const auto r = static_cast<std::uint32_t>(std::round(c0.r + (c1.r - c0.r) * t));
                const auto g = static_cast<std::uint32_t>(std::round(c0.g + (c1.g - c0.g) * t));
                const auto b = static_cast<std::uint32_t>(std::round(c0.b + (c1.b - c0.b) * t));
                const auto a = (static_cast<std::uint32_t>(std::round(c0.a + (c1.a - c0.a) * t)) * op + 127u) / 255u;
                for (int x = x0; x < x1; ++x) composite_src_over(row[x], r, g, b, a);
            } else {
                for (int x = x0; x < x1; ++x) {
                    const float t = std::clamp((x - rx) / span, 0.0f, 1.0f);
                    const auto r = static_cast<std::uint32_t>(std::round(c0.r + (c1.r - c0.r) * t));
                    const auto g = static_cast<std::uint32_t>(std::round(c0.g + (c1.g - c0.g) * t));
                    const auto b = static_cast<std::uint32_t>(std::round(c0.b + (c1.b - c0.b) * t));
                    const auto a = (static_cast<std::uint32_t>(std::round(c0.a + (c1.a - c0.a) * t)) * op + 127u) / 255u;
                    composite_src_over(row[x], r, g, b, a);
                }
            }
        }
    }

    void draw_circle(float cx, float cy, float radius, RgbaColor col, int paint_opacity) {
        if (radius <= 0.0f || paint_opacity <= 0) return;
        const int x0 = std::max(0, static_cast<int>(std::floor(cx - radius - 1.0f)));
        const int y0 = std::max(0, static_cast<int>(std::floor(cy - radius - 1.0f)));
        const int x1 = std::min(width, static_cast<int>(std::ceil(cx + radius + 1.0f)));
        const int y1 = std::min(height, static_cast<int>(std::ceil(cy + radius + 1.0f)));
        if (x0 >= x1 || y0 >= y1) return;

        std::uint32_t* dst = write_pixels();
        const float base_a = (col.a / 255.0f) * (std::clamp(paint_opacity, 0, 255) / 255.0f);
        for (int y = y0; y < y1; ++y) {
            std::uint32_t* row = dst + static_cast<std::size_t>(y) * width;
            const float dy = (y + 0.5f) - cy;
            for (int x = x0; x < x1; ++x) {
                const float dx = (x + 0.5f) - cx;
                const float dist = std::sqrt(dx * dx + dy * dy);
                const float cov = std::clamp(radius + 0.5f - dist, 0.0f, 1.0f);
                if (cov > 0.0f) {
                    const auto sa = static_cast<std::uint32_t>(std::round(cov * base_a * 255.0f));
                    composite_src_over(row[x], col.r, col.g, col.b, sa);
                }
            }
        }
    }

    void blt(const NativeBitmap& src, int sx, int sy, int sw, int sh,
             int dx, int dy, int dw, int dh, int paint_opacity, const std::string& comp) {
        if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0 || paint_opacity <= 0) return;
        if (sx < 0 || sy < 0 || sx + sw > src.width || sy + sh > src.height) return;

        // Handle self-blit safely via copy
        std::vector<std::uint32_t> self_copy;
        const std::uint32_t* spx = src.read_pixels();
        int spitch = src.width;
        if (&src == this) {
            self_copy.assign(static_cast<std::size_t>(sw) * static_cast<std::size_t>(sh), 0u);
            for (int r = 0; r < sh; ++r) {
                std::memcpy(self_copy.data() + static_cast<std::size_t>(r) * sw,
                            spx + static_cast<std::size_t>(sy + r) * src.width + sx,
                            static_cast<std::size_t>(sw) * sizeof(std::uint32_t));
            }
            spx = self_copy.data();
            spitch = sw;
            sx = 0;
            sy = 0;
        }

        std::uint32_t* dpx = write_pixels();
        const std::uint32_t op = static_cast<std::uint32_t>(std::clamp(paint_opacity, 0, 255));
        const bool is_copy = (comp == "copy");
        const bool is_dest_in = (comp == "destination-in");

        if (sw == dw && sh == dh) {
            int cx0 = std::max(0, -dx);
            int cy0 = std::max(0, -dy);
            int cx1 = std::min(dw, width - dx);
            int cy1 = std::min(dh, height - dy);
            if (cx0 >= cx1 || cy0 >= cy1) return;

            for (int r = cy0; r < cy1; ++r) {
                const std::uint32_t* srow = spx + static_cast<std::size_t>(sy + r) * spitch + (sx + cx0);
                std::uint32_t* drow = dpx + static_cast<std::size_t>(dy + r) * width + (dx + cx0);
                const int cnt = cx1 - cx0;
                if (is_copy && op == 255u) {
                    std::memcpy(drow, srow, static_cast<std::size_t>(cnt) * sizeof(std::uint32_t));
                } else if (is_dest_in) {
                    for (int c = 0; c < cnt; ++c) {
                        const std::uint32_t sa = ((srow[c] >> 24) & 0xFFu);
                        const std::uint32_t dp = drow[c];
                        const std::uint32_t da = (((dp >> 24) & 0xFFu) * sa + 127u) / 255u;
                        drow[c] = (dp & 0x00FFFFFFu) | (da << 24);
                    }
                } else {
                    for (int c = 0; c < cnt; ++c) {
                        const std::uint32_t sp = srow[c];
                        const std::uint32_t sa = (((sp >> 24) & 0xFFu) * op + 127u) / 255u;
                        if (!sa) continue;
                        if (is_copy) drow[c] = (sp & 0x00FFFFFFu) | (sa << 24);
                        else composite_src_over(drow[c], sp & 0xFFu, (sp >> 8) & 0xFFu, (sp >> 16) & 0xFFu, sa);
                    }
                }
            }
            return;
        }

        // Scaled blit (used for Windowskin 9-slice back/frame/cursor & portraits)
        const int x0 = std::max(0, dx);
        const int y0 = std::max(0, dy);
        const int x1 = std::min(width, dx + dw);
        const int y1 = std::min(height, dy + dh);
        if (x0 >= x1 || y0 >= y1) return;

        for (int y = y0; y < y1; ++y) {
            const int src_y = sy + std::clamp(((y - dy) * sh) / dh, 0, sh - 1);
            const std::uint32_t* srow = spx + static_cast<std::size_t>(src_y) * spitch;
            std::uint32_t* drow = dpx + static_cast<std::size_t>(y) * width;
            for (int x = x0; x < x1; ++x) {
                const int src_x = sx + std::clamp(((x - dx) * sw) / dw, 0, sw - 1);
                const std::uint32_t sp = srow[src_x];
                const std::uint32_t sa = (((sp >> 24) & 0xFFu) * op + 127u) / 255u;
                if (!sa) continue;
                if (is_copy) drow[x] = (sp & 0x00FFFFFFu) | (sa << 24);
                else composite_src_over(drow[x], sp & 0xFFu, (sp >> 8) & 0xFFu, (sp >> 16) & 0xFFu, sa);
            }
        }
    }

    void adjust_tone(int tr, int tg, int tb) {
        if (!tr && !tg && !tb) return;
        std::uint32_t* dst = write_pixels();
        const std::size_t total = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        for (std::size_t i = 0; i < total; ++i) {
            const std::uint32_t p = dst[i];
            const std::uint32_t a = (p >> 24) & 0xFFu;
            if (!a) continue;
            const int r = std::clamp(static_cast<int>(p & 0xFFu) + tr, 0, 255);
            const int g = std::clamp(static_cast<int>((p >> 8) & 0xFFu) + tg, 0, 255);
            const int b = std::clamp(static_cast<int>((p >> 16) & 0xFFu) + tb, 0, 255);
            dst[i] = pack_px(static_cast<std::uint32_t>(r), static_cast<std::uint32_t>(g),
                             static_cast<std::uint32_t>(b), a);
        }
    }

    void rotate_hue(double offset_deg) {
        int deg = static_cast<int>(std::round(offset_deg)) % 360;
        if (deg < 0) deg += 360;
        if (deg == 0) return;

        const double rad = deg * (3.14159265358979323846 / 180.0);
        const double cos_a = std::cos(rad);
        const double sin_a = std::sin(rad);
        // Luminance-preserving RGB hue rotation matrix
        const double m00 = 0.213 + cos_a * 0.787 - sin_a * 0.213;
        const double m01 = 0.715 - cos_a * 0.715 - sin_a * 0.715;
        const double m02 = 0.072 - cos_a * 0.072 + sin_a * 0.928;
        const double m10 = 0.213 - cos_a * 0.213 + sin_a * 0.143;
        const double m11 = 0.715 + cos_a * 0.285 + sin_a * 0.140;
        const double m12 = 0.072 - cos_a * 0.072 - sin_a * 0.283;
        const double m20 = 0.213 - cos_a * 0.213 - sin_a * 0.787;
        const double m21 = 0.715 - cos_a * 0.715 + sin_a * 0.715;
        const double m22 = 0.072 + cos_a * 0.928 + sin_a * 0.072;

        std::uint32_t* dst = write_pixels();
        const std::size_t total = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        for (std::size_t i = 0; i < total; ++i) {
            const std::uint32_t p = dst[i];
            const std::uint32_t a = (p >> 24) & 0xFFu;
            if (!a) continue;
            const double r = static_cast<double>(p & 0xFFu);
            const double g = static_cast<double>((p >> 8) & 0xFFu);
            const double b = static_cast<double>((p >> 16) & 0xFFu);
            const auto nr = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::round(r * m00 + g * m01 + b * m02)), 0, 255));
            const auto ng = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::round(r * m10 + g * m11 + b * m12)), 0, 255));
            const auto nb = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::round(r * m20 + g * m21 + b * m22)), 0, 255));
            dst[i] = pack_px(nr, ng, nb, a);
        }
    }

    void blur() {
        if (width < 2 || height < 2) return;
        std::uint32_t* px = write_pixels();
        std::vector<std::uint32_t> tmp(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

        // Fast 2-pass separable radius-1 box blur matching MV's SceneManager.snapForBackground
        for (int pass = 0; pass < 2; ++pass) {
            for (int y = 0; y < height; ++y) {
                const std::uint32_t* srow = px + static_cast<std::size_t>(y) * width;
                std::uint32_t* drow = tmp.data() + static_cast<std::size_t>(y) * width;
                for (int x = 0; x < width; ++x) {
                    const std::uint32_t p0 = srow[x > 0 ? x - 1 : 0];
                    const std::uint32_t p1 = srow[x];
                    const std::uint32_t p2 = srow[x + 1 < width ? x + 1 : width - 1];
                    const std::uint32_t r = ((p0 & 0xFFu) + (p1 & 0xFFu) * 2u + (p2 & 0xFFu) + 2u) >> 2;
                    const std::uint32_t g = (((p0 >> 8) & 0xFFu) + ((p1 >> 8) & 0xFFu) * 2u + ((p2 >> 8) & 0xFFu) + 2u) >> 2;
                    const std::uint32_t b = (((p0 >> 16) & 0xFFu) + ((p1 >> 16) & 0xFFu) * 2u + ((p2 >> 16) & 0xFFu) + 2u) >> 2;
                    drow[x] = pack_px(r, g, b, 255u);
                }
            }
            for (int y = 0; y < height; ++y) {
                const std::uint32_t* r0 = tmp.data() + static_cast<std::size_t>(y > 0 ? y - 1 : 0) * width;
                const std::uint32_t* r1 = tmp.data() + static_cast<std::size_t>(y) * width;
                const std::uint32_t* r2 = tmp.data() + static_cast<std::size_t>(y + 1 < height ? y + 1 : height - 1) * width;
                std::uint32_t* drow = px + static_cast<std::size_t>(y) * width;
                for (int x = 0; x < width; ++x) {
                    const std::uint32_t p0 = r0[x], p1 = r1[x], p2 = r2[x];
                    const std::uint32_t r = ((p0 & 0xFFu) + (p1 & 0xFFu) * 2u + (p2 & 0xFFu) + 2u) >> 2;
                    const std::uint32_t g = (((p0 >> 8) & 0xFFu) + ((p1 >> 8) & 0xFFu) * 2u + ((p2 >> 8) & 0xFFu) + 2u) >> 2;
                    const std::uint32_t b = (((p0 >> 16) & 0xFFu) + ((p1 >> 16) & 0xFFu) * 2u + ((p2 >> 16) & 0xFFu) + 2u) >> 2;
                    drow[x] = pack_px(r, g, b, 255u);
                }
            }
        }
    }

    std::string get_pixel_hex(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return "#000000";
        const std::uint32_t p = read_pixels()[static_cast<std::size_t>(y) * width + x];
        static constexpr char kHex[] = "0123456789abcdef";
        char buf[8];
        buf[0] = '#';
        buf[1] = kHex[(p >> 4) & 0xFu];
        buf[2] = kHex[p & 0xFu];
        buf[3] = kHex[(p >> 12) & 0xFu];
        buf[4] = kHex[(p >> 8) & 0xFu];
        buf[5] = kHex[(p >> 20) & 0xFu];
        buf[6] = kHex[(p >> 16) & 0xFu];
        buf[7] = '\0';
        return std::string(buf, 7);
    }

    int get_alpha(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return 0;
        return static_cast<int>((read_pixels()[static_cast<std::size_t>(y) * width + x] >> 24) & 0xFFu);
    }
};

} // namespace rpg2lx::mv_detail

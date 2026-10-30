#include "rpg2lx/font_renderer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <unordered_map>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include "stb_image.h"
#include "font8x8_basic.h"
#include "font8x8_ext_latin.h"

namespace fs = std::filesystem;

namespace rpg2lx {

namespace {

int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

std::string trim_lower(const std::string& s) {
    std::size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    std::size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    std::string out;
    out.reserve(b - a);
    for (std::size_t i = a; i < b; ++i) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(s[i]))));
    }
    return out;
}

inline void blend_pixel_rgba32(std::uint32_t& dst_px, std::uint8_t sr, std::uint8_t sg,
                               std::uint8_t sb, std::uint8_t sa) {
    if (sa == 0) return;
    if (sa == 255) {
        dst_px = static_cast<std::uint32_t>(sr) |
                 (static_cast<std::uint32_t>(sg) << 8) |
                 (static_cast<std::uint32_t>(sb) << 16) |
                 0xFF000000u;
        return;
    }
    const std::uint32_t dr = dst_px & 0xFFu;
    const std::uint32_t dg = (dst_px >> 8) & 0xFFu;
    const std::uint32_t db = (dst_px >> 16) & 0xFFu;
    const std::uint32_t da = (dst_px >> 24) & 0xFFu;

    const std::uint32_t inv_sa = 255u - sa;
    const std::uint32_t out_a = sa + (da * inv_sa + 127u) / 255u;
    if (out_a == 0) {
        dst_px = 0;
        return;
    }
    const std::uint32_t out_r = (sr * sa * 255u + dr * da * inv_sa + (out_a * 127u)) / (out_a * 255u);
    const std::uint32_t out_g = (sg * sa * 255u + dg * da * inv_sa + (out_a * 127u)) / (out_a * 255u);
    const std::uint32_t out_b = (sb * sa * 255u + db * da * inv_sa + (out_a * 127u)) / (out_a * 255u);

    dst_px = std::min<std::uint32_t>(255u, out_r) |
             (std::min<std::uint32_t>(255u, out_g) << 8) |
             (std::min<std::uint32_t>(255u, out_b) << 16) |
             (std::min<std::uint32_t>(255u, out_a) << 24);
}

const unsigned char* fallback_glyph8x8(std::uint32_t cp) {
    if (cp < 128) return reinterpret_cast<const unsigned char*>(font8x8_basic[cp]);
    if (cp >= 160 && cp <= 255) return reinterpret_cast<const unsigned char*>(font8x8_ext_latin[cp - 160]);
    if (cp == 0x2026) return reinterpret_cast<const unsigned char*>(font8x8_basic['.']);
    if (cp == 0x2013 || cp == 0x2014) return reinterpret_cast<const unsigned char*>(font8x8_basic['-']);
    if (cp == 0x201C || cp == 0x201D) return reinterpret_cast<const unsigned char*>(font8x8_basic['"']);
    if (cp == 0x2018 || cp == 0x2019) return reinterpret_cast<const unsigned char*>(font8x8_basic['\'']);
    return reinterpret_cast<const unsigned char*>(font8x8_basic['?']);
}

} // namespace

RgbaColor parse_css_color(const std::string& css, RgbaColor fallback) {
    const std::string s = trim_lower(css);
    if (s.empty()) return fallback;

    if (s[0] == '#') {
        if (s.size() == 4) {
            int r = hex_nibble(s[1]), g = hex_nibble(s[2]), b = hex_nibble(s[3]);
            if (r >= 0 && g >= 0 && b >= 0) {
                return {static_cast<std::uint8_t>(r * 17),
                        static_cast<std::uint8_t>(g * 17),
                        static_cast<std::uint8_t>(b * 17), 255};
            }
        } else if (s.size() == 7 || s.size() == 9) {
            int r1 = hex_nibble(s[1]), r2 = hex_nibble(s[2]);
            int g1 = hex_nibble(s[3]), g2 = hex_nibble(s[4]);
            int b1 = hex_nibble(s[5]), b2 = hex_nibble(s[6]);
            int a1 = 15, a2 = 15;
            if (s.size() == 9) {
                a1 = hex_nibble(s[7]);
                a2 = hex_nibble(s[8]);
            }
            if (r1 >= 0 && r2 >= 0 && g1 >= 0 && g2 >= 0 && b1 >= 0 && b2 >= 0 && a1 >= 0 && a2 >= 0) {
                return {static_cast<std::uint8_t>((r1 << 4) | r2),
                        static_cast<std::uint8_t>((g1 << 4) | g2),
                        static_cast<std::uint8_t>((b1 << 4) | b2),
                        static_cast<std::uint8_t>((a1 << 4) | a2)};
            }
        }
        return fallback;
    }

    if (s.rfind("rgba(", 0) == 0 || s.rfind("rgb(", 0) == 0) {
        const auto open = s.find('(');
        const auto close = s.find(')', open);
        if (open != std::string::npos && close != std::string::npos) {
            std::vector<double> nums;
            std::size_t pos = open + 1;
            while (pos < close) {
                const auto comma = std::min(close, s.find(',', pos));
                const std::string part = s.substr(pos, comma - pos);
                char* endp = nullptr;
                double val = std::strtod(part.c_str(), &endp);
                nums.push_back(val);
                pos = comma + 1;
            }
            if (nums.size() >= 3) {
                const auto r = static_cast<std::uint8_t>(std::clamp(std::round(nums[0]), 0.0, 255.0));
                const auto g = static_cast<std::uint8_t>(std::clamp(std::round(nums[1]), 0.0, 255.0));
                const auto b = static_cast<std::uint8_t>(std::clamp(std::round(nums[2]), 0.0, 255.0));
                std::uint8_t a = 255;
                if (nums.size() >= 4) {
                    double av = nums[3];
                    if (av <= 1.0) av *= 255.0;
                    a = static_cast<std::uint8_t>(std::clamp(std::round(av), 0.0, 255.0));
                }
                return {r, g, b, a};
            }
        }
        return fallback;
    }

    if (s == "white") return {255, 255, 255, 255};
    if (s == "black") return {0, 0, 0, 255};
    if (s == "transparent" || s == "none") return {0, 0, 0, 0};
    if (s == "red") return {255, 0, 0, 255};
    if (s == "green") return {0, 128, 0, 255};
    if (s == "lime") return {0, 255, 0, 255};
    if (s == "blue") return {0, 0, 255, 255};
    if (s == "yellow") return {255, 255, 0, 255};
    if (s == "cyan" || s == "aqua") return {0, 255, 255, 255};
    if (s == "magenta" || s == "fuchsia") return {255, 0, 255, 255};
    if (s == "gray" || s == "grey") return {128, 128, 128, 255};
    if (s == "silver") return {192, 192, 192, 255};
    if (s == "orange") return {255, 165, 0, 255};
    return fallback;
}

std::vector<std::uint32_t> decode_utf8_codepoints(const std::string& text) {
    std::vector<std::uint32_t> out;
    out.reserve(text.size());
    const std::size_t n = text.size();
    std::size_t i = 0;
    while (i < n) {
        const unsigned char b0 = static_cast<unsigned char>(text[i]);
        if (b0 < 0x80u) {
            out.push_back(b0);
            ++i;
        } else if ((b0 & 0xE0u) == 0xC0u) {
            if (i + 1 < n && (static_cast<unsigned char>(text[i + 1]) & 0xC0u) == 0x80u) {
                const std::uint32_t cp = ((b0 & 0x1Fu) << 6) |
                                         (static_cast<unsigned char>(text[i + 1]) & 0x3Fu);
                out.push_back(cp >= 0x80u ? cp : 0xFFFDu);
                i += 2;
            } else {
                out.push_back(0xFFFDu);
                ++i;
            }
        } else if ((b0 & 0xF0u) == 0xE0u) {
            if (i + 2 < n &&
                (static_cast<unsigned char>(text[i + 1]) & 0xC0u) == 0x80u &&
                (static_cast<unsigned char>(text[i + 2]) & 0xC0u) == 0x80u) {
                const std::uint32_t cp = ((b0 & 0x0Fu) << 12) |
                                         ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 6) |
                                         (static_cast<unsigned char>(text[i + 2]) & 0x3Fu);
                out.push_back(cp >= 0x800u ? cp : 0xFFFDu);
                i += 3;
            } else {
                out.push_back(0xFFFDu);
                ++i;
            }
        } else if ((b0 & 0xF8u) == 0xF0u) {
            if (i + 3 < n &&
                (static_cast<unsigned char>(text[i + 1]) & 0xC0u) == 0x80u &&
                (static_cast<unsigned char>(text[i + 2]) & 0xC0u) == 0x80u &&
                (static_cast<unsigned char>(text[i + 3]) & 0xC0u) == 0x80u) {
                const std::uint32_t cp = ((b0 & 0x07u) << 18) |
                                         ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 12) |
                                         ((static_cast<unsigned char>(text[i + 2]) & 0x3Fu) << 6) |
                                         (static_cast<unsigned char>(text[i + 3]) & 0x3Fu);
                out.push_back((cp >= 0x10000u && cp <= 0x10FFFFu) ? cp : 0xFFFDu);
                i += 4;
            } else {
                out.push_back(0xFFFDu);
                ++i;
            }
        } else {
            out.push_back(0xFFFDu);
            ++i;
        }
    }
    return out;
}

struct CachedGlyph {
    int width = 0;
    int height = 0;
    int xoff = 0;
    int yoff = 0;
    float advance = 0.0f;
    int glyph_index = 0;
    std::vector<std::uint8_t> bitmap;
};

struct FontRenderer::Impl {
    std::vector<unsigned char> ttf_data;
    stbtt_fontinfo font_info{};
    bool truetype = false;
    float readability_scale = 1.0f;
    mutable std::unordered_map<std::uint64_t, CachedGlyph> glyph_cache;

#ifdef RPG2LX_WITH_SDL
    SDL_Texture* fallback_tex = nullptr;
    SDL_Texture* ttf_atlas_tex = nullptr;
    int atlas_w = 1024;
    int atlas_h = 1024;
    float base_px = 28.0f;
    struct AtlasGlyph {
        SDL_Rect src{0, 0, 0, 0};
        int xoff = 0;
        int yoff = 0;
        float advance = 0.0f;
    };
    std::unordered_map<std::uint32_t, AtlasGlyph> atlas_glyphs;
#endif

    ~Impl() {
#ifdef RPG2LX_WITH_SDL
        release_sdl();
#endif
    }

    float effective_px(int requested_px) const {
        float px = static_cast<float>(std::clamp(requested_px, 8, 160));
        if (readability_scale > 1.001f && px <= 40.0f) {
            // Boost small/medium body & UI text on 480p handheld displays while keeping large titles proportional
            px *= readability_scale;
        }
        return px;
    }

    float scale_for_px(float px) const {
        if (!truetype) return px / 8.0f;
        float s = stbtt_ScaleForMappingEmToPixels(&font_info, px);
        if (s <= 0.0f) {
            s = stbtt_ScaleForPixelHeight(&font_info, px);
        }
        return s;
    }

    const CachedGlyph& get_glyph(std::uint32_t cp, int px_quantized, bool italic) const {
        const std::uint64_t key = (static_cast<std::uint64_t>(cp) << 16) |
                                  (static_cast<std::uint64_t>(px_quantized & 0x7FFF) << 1) |
                                  (italic ? 1ULL : 0ULL);
        const auto it = glyph_cache.find(key);
        if (it != glyph_cache.end()) {
            return it->second;
        }

        if (glyph_cache.size() > 4096) {
            glyph_cache.clear();
        }

        CachedGlyph cg;
        const float px = static_cast<float>(px_quantized) * 0.5f;
        if (truetype) {
            const float scale = scale_for_px(px);
            int idx = stbtt_FindGlyphIndex(&font_info, static_cast<int>(cp));
            if (idx == 0 && cp != ' ') {
                idx = stbtt_FindGlyphIndex(&font_info, '?');
            }
            cg.glyph_index = idx;
            int adv = 0, lsb = 0;
            stbtt_GetGlyphHMetrics(&font_info, idx, &adv, &lsb);
            cg.advance = adv * scale;

            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            stbtt_GetGlyphBitmapBox(&font_info, idx, scale, scale, &x0, &y0, &x1, &y1);
            const int bw = x1 - x0;
            const int bh = y1 - y0;
            if (bw > 0 && bh > 0 && cp != ' ') {
                if (!italic) {
                    cg.width = bw;
                    cg.height = bh;
                    cg.xoff = x0;
                    cg.yoff = y0;
                    cg.bitmap.resize(static_cast<std::size_t>(bw * bh), 0);
                    stbtt_MakeGlyphBitmap(&font_info, cg.bitmap.data(), bw, bh, bw, scale, scale, idx);
                } else {
                    std::vector<std::uint8_t> raw(static_cast<std::size_t>(bw * bh), 0);
                    stbtt_MakeGlyphBitmap(&font_info, raw.data(), bw, bh, bw, scale, scale, idx);
                    const int shear_pad = static_cast<int>(std::ceil(bh * 0.22f)) + 2;
                    const int sw = bw + shear_pad;
                    cg.width = sw;
                    cg.height = bh;
                    cg.xoff = x0;
                    cg.yoff = y0;
                    cg.bitmap.assign(static_cast<std::size_t>(sw * bh), 0);
                    for (int ry = 0; ry < bh; ++ry) {
                        const float shift = (bh - 1 - ry) * 0.20f;
                        const int s_int = static_cast<int>(std::floor(shift));
                        const float frac = shift - s_int;
                        for (int rx = 0; rx < bw; ++rx) {
                            const std::uint8_t v = raw[static_cast<std::size_t>(ry * bw + rx)];
                            if (!v) continue;
                            const int dx0 = rx + s_int;
                            if (dx0 >= 0 && dx0 < sw) {
                                const int add0 = static_cast<int>(std::round(v * (1.0f - frac)));
                                cg.bitmap[static_cast<std::size_t>(ry * sw + dx0)] =
                                    static_cast<std::uint8_t>(std::min(255, cg.bitmap[static_cast<std::size_t>(ry * sw + dx0)] + add0));
                            }
                            if (dx0 + 1 >= 0 && dx0 + 1 < sw && frac > 0.01f) {
                                const int add1 = static_cast<int>(std::round(v * frac));
                                cg.bitmap[static_cast<std::size_t>(ry * sw + dx0 + 1)] =
                                    static_cast<std::uint8_t>(std::min(255, cg.bitmap[static_cast<std::size_t>(ry * sw + dx0 + 1)] + add1));
                            }
                        }
                    }
                }
            }
        } else {
            // Bitmap 8x8 fallback scaled cleanly
            const int scale_i = std::max(1, static_cast<int>(std::round(px / 10.0f)));
            cg.width = 8 * scale_i;
            cg.height = 8 * scale_i;
            cg.xoff = 0;
            cg.yoff = -7 * scale_i;
            cg.advance = static_cast<float>(8 * scale_i);
            cg.bitmap.assign(static_cast<std::size_t>(cg.width * cg.height), 0);
            const unsigned char* g8 = fallback_glyph8x8(cp);
            for (int r = 0; r < 8; ++r) {
                for (int c = 0; c < 8; ++c) {
                    if (g8[r] & (1u << c)) {
                        for (int sy = 0; sy < scale_i; ++sy) {
                            for (int sx = 0; sx < scale_i; ++sx) {
                                cg.bitmap[static_cast<std::size_t>((r * scale_i + sy) * cg.width + (c * scale_i + sx))] = 255;
                            }
                        }
                    }
                }
            }
        }

        return glyph_cache.emplace(key, std::move(cg)).first->second;
    }

    bool load_ttf_bytes(const fs::path& file_path) {
        std::error_code ec;
        if (!fs::exists(file_path, ec) || !fs::is_regular_file(file_path, ec)) return false;
        const auto sz = fs::file_size(file_path, ec);
        if (ec || sz < 64 || sz > 32 * 1024 * 1024) return false;

        std::ifstream f(file_path, std::ios::binary);
        if (!f.is_open()) return false;
        std::vector<unsigned char> buf(static_cast<std::size_t>(sz));
        if (!f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(sz))) return false;

        const int offset = stbtt_GetFontOffsetForIndex(buf.data(), 0);
        if (offset < 0) return false;

        stbtt_fontinfo info{};
        if (!stbtt_InitFont(&info, buf.data(), offset)) return false;

        ttf_data = std::move(buf);
        // Re-init so pointers inside font_info reference ttf_data.data()
        stbtt_InitFont(&font_info, ttf_data.data(), offset);
        truetype = true;
        glyph_cache.clear();
        return true;
    }

#ifdef RPG2LX_WITH_SDL
    void release_sdl() {
        if (fallback_tex) {
            SDL_DestroyTexture(fallback_tex);
            fallback_tex = nullptr;
        }
        if (ttf_atlas_tex) {
            SDL_DestroyTexture(ttf_atlas_tex);
            ttf_atlas_tex = nullptr;
        }
        atlas_glyphs.clear();
    }

    bool build_sdl_textures(SDL_Renderer* renderer) {
        release_sdl();
        // Always build 8x8 fallback atlas
        {
            const int tw = 128, th = 128;
            std::vector<std::uint32_t> rgba(static_cast<std::size_t>(tw * th), 0);
            for (int c = 0; c < 256; ++c) {
                const unsigned char* glyph = fallback_glyph8x8(static_cast<std::uint32_t>(c));
                const int gx = (c % 16) * 8;
                const int gy = (c / 16) * 8;
                for (int row = 0; row < 8; ++row) {
                    for (int col = 0; col < 8; ++col) {
                        if (glyph[row] & (1u << col)) {
                            rgba[static_cast<std::size_t>((gy + row) * tw + (gx + col))] = 0xFFFFFFFFu;
                        }
                    }
                }
            }
            SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
                rgba.data(), tw, th, 32, tw * 4, SDL_PIXELFORMAT_RGBA32);
            if (surf) {
                fallback_tex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_FreeSurface(surf);
                if (fallback_tex) SDL_SetTextureBlendMode(fallback_tex, SDL_BLENDMODE_BLEND);
            }
        }

        if (!truetype) return fallback_tex != nullptr;

        // Pack ASCII + Latin-1 + Latin-Extended + common punctuation/symbols into a crisp 28px GPU atlas
        base_px = 28.0f;
        atlas_w = 1024;
        atlas_h = 1024;
        std::vector<std::uint32_t> rgba(static_cast<std::size_t>(atlas_w * atlas_h), 0);

        std::vector<std::uint32_t> codepoints;
        codepoints.reserve(420);
        for (std::uint32_t c = 32; c <= 0x017F; ++c) codepoints.push_back(c);
        const std::uint32_t extras[] = {
            0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026,
            0x20AC, 0x2122, 0x2190, 0x2191, 0x2192, 0x2193, 0x25A0, 0x25B2,
            0x25B6, 0x25BC, 0x25C0, 0x25CF, 0x2605, 0x2606, 0x2665, 0x266A, 0xFFFD
        };
        for (std::uint32_t cp : extras) codepoints.push_back(cp);

        const int qpx = static_cast<int>(std::round(base_px * 2.0f));
        int pen_x = 2;
        int pen_y = 2;
        int row_h = 0;

        for (std::uint32_t cp : codepoints) {
            const CachedGlyph& cg = get_glyph(cp, qpx, false);
            if (pen_x + cg.width + 2 >= atlas_w) {
                pen_x = 2;
                pen_y += row_h + 2;
                row_h = 0;
            }
            if (pen_y + cg.height + 2 >= atlas_h) break;

            for (int ry = 0; ry < cg.height; ++ry) {
                for (int rx = 0; rx < cg.width; ++rx) {
                    const std::uint8_t a = cg.bitmap[static_cast<std::size_t>(ry * cg.width + rx)];
                    if (a > 0) {
                        rgba[static_cast<std::size_t>((pen_y + ry) * atlas_w + (pen_x + rx))] =
                            (static_cast<std::uint32_t>(a) << 24) | 0x00FFFFFFu;
                    }
                }
            }
            AtlasGlyph ag;
            ag.src = {pen_x, pen_y, cg.width, cg.height};
            ag.xoff = cg.xoff;
            ag.yoff = cg.yoff;
            ag.advance = cg.advance;
            atlas_glyphs[cp] = ag;

            pen_x += cg.width + 2;
            row_h = std::max(row_h, cg.height);
        }

        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
            rgba.data(), atlas_w, atlas_h, 32, atlas_w * 4, SDL_PIXELFORMAT_RGBA32);
        if (!surf) return fallback_tex != nullptr;
        ttf_atlas_tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_FreeSurface(surf);
        if (ttf_atlas_tex) {
            SDL_SetTextureBlendMode(ttf_atlas_tex, SDL_BLENDMODE_BLEND);
        }
        return ttf_atlas_tex != nullptr || fallback_tex != nullptr;
    }
#endif
};

FontRenderer::FontRenderer() : impl_(std::make_unique<Impl>()) {}
FontRenderer::~FontRenderer() = default;
FontRenderer::FontRenderer(FontRenderer&&) noexcept = default;
FontRenderer& FontRenderer::operator=(FontRenderer&&) noexcept = default;

bool FontRenderer::load_font_file(const fs::path& font_file) {
    return impl_->load_ttf_bytes(font_file);
}

bool FontRenderer::load_project_font(const fs::path& project_path) {
    if (project_path.empty()) return false;
    std::error_code ec;
    const fs::path fonts_dir = project_path / "fonts";
    if (!fs::is_directory(fonts_dir, ec)) return false;

    // Prefer mplus-1m-regular.ttf (RPG Maker MV standard GameFont) if present
    const fs::path preferred = fonts_dir / "mplus-1m-regular.ttf";
    if (fs::is_regular_file(preferred, ec) && impl_->load_ttf_bytes(preferred)) {
        return true;
    }

    for (const auto& entry : fs::directory_iterator(fonts_dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().u8string();
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext == ".ttf" || ext == ".otf") {
            if (impl_->load_ttf_bytes(entry.path())) return true;
        }
    }
    return false;
}

void FontRenderer::set_readability_scale(float factor) {
    impl_->readability_scale = std::clamp(factor, 0.75f, 2.0f);
}

float FontRenderer::readability_scale() const {
    return impl_->readability_scale;
}

bool FontRenderer::is_truetype() const {
    return impl_->truetype;
}

int FontRenderer::line_height() const {
    return impl_->truetype ? 36 : 12;
}

int FontRenderer::measure_text_px(const std::string& text, int font_size, bool italic) const {
    if (text.empty()) return 0;
    const auto cps = decode_utf8_codepoints(text);
    const float px = impl_->effective_px(font_size);
    const int qpx = std::max(16, static_cast<int>(std::round(px * 2.0f)));
    const float scale = impl_->scale_for_px(static_cast<float>(qpx) * 0.5f);

    float width = 0.0f;
    int prev_glyph = 0;
    for (std::uint32_t cp : cps) {
        if (cp == '\n' || cp == '\r') break;
        const CachedGlyph& cg = impl_->get_glyph(cp, qpx, italic);
        if (impl_->truetype && prev_glyph != 0 && cg.glyph_index != 0) {
            width += stbtt_GetGlyphKernAdvance(&impl_->font_info, prev_glyph, cg.glyph_index) * scale;
        }
        width += cg.advance;
        prev_glyph = cg.glyph_index;
    }
    return static_cast<int>(std::ceil(width));
}

void FontRenderer::draw_text_rgba(std::uint32_t* pixels, int canvas_w, int canvas_h,
                                  const std::string& text, int x, int y,
                                  int max_width, int line_height,
                                  TextAlign align, const TextStyle& style) {
    if (!pixels || canvas_w <= 0 || canvas_h <= 0 || text.empty() || style.paint_opacity == 0) return;

    const auto cps = decode_utf8_codepoints(text);
    if (cps.empty()) return;

    const float px = impl_->effective_px(style.font_size);
    const int qpx = std::max(16, static_cast<int>(std::round(px * 2.0f)));
    const float scale = impl_->scale_for_px(static_cast<float>(qpx) * 0.5f);

    // Measure natural width
    float natural_w = 0.0f;
    int prev_glyph = 0;
    for (std::uint32_t cp : cps) {
        if (cp == '\n' || cp == '\r') continue;
        const CachedGlyph& cg = impl_->get_glyph(cp, qpx, style.italic);
        if (impl_->truetype && prev_glyph != 0 && cg.glyph_index != 0) {
            natural_w += stbtt_GetGlyphKernAdvance(&impl_->font_info, prev_glyph, cg.glyph_index) * scale;
        }
        natural_w += cg.advance;
        prev_glyph = cg.glyph_index;
    }
    if (natural_w <= 0.5f) return;

    const int eff_max_w = (max_width > 0) ? max_width : 0x7FFFFFFF;
    const float squeeze_x = (natural_w > static_cast<float>(eff_max_w) && eff_max_w > 0)
                                ? (static_cast<float>(eff_max_w) / natural_w)
                                : 1.0f;
    const float drawn_w = natural_w * squeeze_x;

    float start_x = static_cast<float>(x);
    if (max_width > 0) {
        if (align == TextAlign::Center) {
            start_x += (static_cast<float>(max_width) - drawn_w) * 0.5f;
        } else if (align == TextAlign::Right) {
            start_x += static_cast<float>(max_width) - drawn_w;
        }
    }

    const int lh = (line_height > 0) ? line_height : static_cast<int>(std::round(px * 1.28f));
    // Exact RPG Maker MV baseline formula: ty = y + lineHeight - (lineHeight - fontSize * 0.7) / 2
    const int baseline_y = static_cast<int>(std::round(
        static_cast<float>(y) + static_cast<float>(lh) - (static_cast<float>(lh) - px * 0.72f) * 0.5f));

    const int out_r_px = std::clamp((style.outline_width + 1) / 2, 0, 8);

    // Precompute glyph placements along baseline
    struct PlacedGlyph {
        const CachedGlyph* cg;
        int gx;
        int gy;
        int gw;
    };
    std::vector<PlacedGlyph> placed;
    placed.reserve(cps.size());

    float pen_x = start_x;
    prev_glyph = 0;
    for (std::uint32_t cp : cps) {
        if (cp == '\n' || cp == '\r') continue;
        const CachedGlyph& cg = impl_->get_glyph(cp, qpx, style.italic);
        if (impl_->truetype && prev_glyph != 0 && cg.glyph_index != 0) {
            pen_x += stbtt_GetGlyphKernAdvance(&impl_->font_info, prev_glyph, cg.glyph_index) * scale * squeeze_x;
        }
        if (!cg.bitmap.empty() && cg.width > 0 && cg.height > 0) {
            const int gx = static_cast<int>(std::round(pen_x + cg.xoff * squeeze_x));
            const int gy = baseline_y + cg.yoff;
            const int gw = std::max(1, static_cast<int>(std::round(cg.width * squeeze_x)));
            placed.push_back({&cg, gx, gy, gw});
        }
        pen_x += cg.advance * squeeze_x;
        prev_glyph = cg.glyph_index;
    }

    if (placed.empty()) return;

    // Pass 1: Smooth outline stroke (using max-alpha dilation per pixel so semi-transparent outlines never double-darken!)
    if (out_r_px > 0 && style.outline_color.a > 0) {
        int min_x = canvas_w, min_y = canvas_h, max_x = 0, max_y = 0;
        for (const auto& pg : placed) {
            min_x = std::min(min_x, pg.gx - out_r_px);
            min_y = std::min(min_y, pg.gy - out_r_px);
            max_x = std::max(max_x, pg.gx + pg.gw + out_r_px);
            max_y = std::max(max_y, pg.gy + pg.cg->height + out_r_px);
        }
        min_x = std::clamp(min_x, 0, canvas_w);
        min_y = std::clamp(min_y, 0, canvas_h);
        max_x = std::clamp(max_x, 0, canvas_w);
        max_y = std::clamp(max_y, 0, canvas_h);

        const int box_w = max_x - min_x;
        const int box_h = max_y - min_y;
        if (box_w > 0 && box_h > 0 && static_cast<std::size_t>(box_w) * box_h <= 2048 * 512) {
            std::vector<std::uint8_t> stroke_mask(static_cast<std::size_t>(box_w * box_h), 0);
            const int r2_limit = out_r_px * out_r_px + out_r_px;

            for (const auto& pg : placed) {
                const int gh = pg.cg->height;
                const int src_w = pg.cg->width;
                const float inv_sx = (pg.gw > 0) ? (static_cast<float>(src_w) / static_cast<float>(pg.gw)) : 1.0f;

                for (int sy = 0; sy < gh; ++sy) {
                    const int base_y = pg.gy + sy;
                    for (int dx = 0; dx < pg.gw; ++dx) {
                        const int src_x = std::min(src_w - 1, static_cast<int>(dx * inv_sx));
                        const std::uint8_t cov = pg.cg->bitmap[static_cast<std::size_t>(sy * src_w + src_x)];
                        if (cov < 12) continue;
                        const int base_x = pg.gx + dx;

                        for (int oy = -out_r_px; oy <= out_r_px; ++oy) {
                            const int py = base_y + oy;
                            if (py < min_y || py >= max_y) continue;
                            for (int ox = -out_r_px; ox <= out_r_px; ++ox) {
                                if (ox * ox + oy * oy > r2_limit) continue;
                                const int px_i = base_x + ox;
                                if (px_i < min_x || px_i >= max_x) continue;
                                auto& cell = stroke_mask[static_cast<std::size_t>((py - min_y) * box_w + (px_i - min_x))];
                                if (cov > cell) cell = cov;
                            }
                        }
                    }
                }
            }

            const std::uint32_t out_base_a = (static_cast<std::uint32_t>(style.outline_color.a) * style.paint_opacity + 127u) / 255u;
            for (int py = min_y; py < max_y; ++py) {
                std::uint32_t* row = pixels + static_cast<std::size_t>(py) * canvas_w;
                const std::uint8_t* mrow = stroke_mask.data() + static_cast<std::size_t>(py - min_y) * box_w;
                for (int px_i = min_x; px_i < max_x; ++px_i) {
                    const std::uint8_t m = mrow[px_i - min_x];
                    if (m > 0) {
                        const auto sa = static_cast<std::uint8_t>((m * out_base_a + 127u) / 255u);
                        blend_pixel_rgba32(row[px_i], style.outline_color.r, style.outline_color.g, style.outline_color.b, sa);
                    }
                }
            }
        }
    }

    // Pass 2: Crisp glyph body fill
    const std::uint32_t fill_base_a = (static_cast<std::uint32_t>(style.text_color.a) * style.paint_opacity + 127u) / 255u;
    for (const auto& pg : placed) {
        const int gh = pg.cg->height;
        const int src_w = pg.cg->width;
        const float inv_sx = (pg.gw > 0) ? (static_cast<float>(src_w) / static_cast<float>(pg.gw)) : 1.0f;

        for (int sy = 0; sy < gh; ++sy) {
            const int py = pg.gy + sy;
            if (py < 0 || py >= canvas_h) continue;
            std::uint32_t* row = pixels + static_cast<std::size_t>(py) * canvas_w;
            for (int dx = 0; dx < pg.gw; ++dx) {
                const int px_i = pg.gx + dx;
                if (px_i < 0 || px_i >= canvas_w) continue;
                const int src_x = std::min(src_w - 1, static_cast<int>(dx * inv_sx));
                const std::uint8_t cov = pg.cg->bitmap[static_cast<std::size_t>(sy * src_w + src_x)];
                if (cov > 0) {
                    const auto sa = static_cast<std::uint8_t>((cov * fill_base_a + 127u) / 255u);
                    blend_pixel_rgba32(row[px_i], style.text_color.r, style.text_color.g, style.text_color.b, sa);
                }
            }
        }
    }
}

#ifdef RPG2LX_WITH_SDL

bool FontRenderer::init(SDL_Renderer* renderer, const fs::path& project_path) {
    if (!renderer) return false;
    impl_->truetype = false;
    if (!project_path.empty()) {
        load_project_font(project_path);
    }
    return impl_->build_sdl_textures(renderer);
}

void FontRenderer::release() {
    impl_->release_sdl();
}

int FontRenderer::text_width(const std::string& text, int scale) const {
    if (text.empty()) return 0;
    const auto cps = decode_utf8_codepoints(text);
    const int s = std::max(1, scale);
    if (impl_->ttf_atlas_tex && impl_->truetype) {
        // scale: 1 -> ~14px, 2 -> ~24px, 3 -> ~28px, 4 -> ~48px
        const float ratio = (s == 1) ? 0.52f : (s == 2) ? 0.86f : (s == 3) ? 1.05f : (0.5f * s);
        float w = 0.0f;
        for (std::uint32_t cp : cps) {
            if (cp == '\n' || cp == '\r') break;
            auto it = impl_->atlas_glyphs.find(cp);
            if (it == impl_->atlas_glyphs.end()) it = impl_->atlas_glyphs.find('?');
            if (it != impl_->atlas_glyphs.end()) w += it->second.advance * ratio;
        }
        return static_cast<int>(std::ceil(w));
    }
    int count = 0;
    for (std::uint32_t cp : cps) {
        if (cp == '\n' || cp == '\r') break;
        ++count;
    }
    return count * 8 * s;
}

void FontRenderer::draw_text(SDL_Renderer* renderer, const std::string& text, int x, int y,
                             SDL_Color color, int scale, bool shadow, int max_chars) {
    if (!renderer || text.empty() || max_chars <= 0) return;
    const auto cps = decode_utf8_codepoints(text);
    const int s = std::max(1, scale);

    if (impl_->ttf_atlas_tex && impl_->truetype) {
        const float ratio = (s == 1) ? 0.52f : (s == 2) ? 0.86f : (s == 3) ? 1.05f : (0.5f * s);
        const float baseline_offset = impl_->base_px * ratio * 0.86f;
        const float line_step = impl_->base_px * ratio * 1.25f;
        float cur_x = static_cast<float>(x);
        float cur_y = static_cast<float>(y) + baseline_offset;
        const float start_x = cur_x;
        int count = 0;

        for (std::uint32_t cp : cps) {
            if (cp == '\n') {
                cur_x = start_x;
                cur_y += line_step;
                count = 0;
                continue;
            }
            if (cp == '\r') continue;
            if (count >= max_chars) break;

            auto it = impl_->atlas_glyphs.find(cp);
            if (it == impl_->atlas_glyphs.end()) it = impl_->atlas_glyphs.find('?');
            if (it == impl_->atlas_glyphs.end()) continue;

            const auto& ag = it->second;
            if (ag.src.w > 0 && ag.src.h > 0) {
                SDL_Rect dst{
                    static_cast<int>(std::round(cur_x + ag.xoff * ratio)),
                    static_cast<int>(std::round(cur_y + ag.yoff * ratio)),
                    std::max(1, static_cast<int>(std::round(ag.src.w * ratio))),
                    std::max(1, static_cast<int>(std::round(ag.src.h * ratio)))
                };
                if (shadow) {
                    SDL_SetTextureColorMod(impl_->ttf_atlas_tex, 0, 0, 0);
                    SDL_SetTextureAlphaMod(impl_->ttf_atlas_tex, static_cast<Uint8>((color.a * 200) / 255));
                    const SDL_Rect d1{dst.x - 1, dst.y, dst.w, dst.h};
                    const SDL_Rect d2{dst.x + 1, dst.y, dst.w, dst.h};
                    const SDL_Rect d3{dst.x, dst.y - 1, dst.w, dst.h};
                    const SDL_Rect d4{dst.x, dst.y + 1, dst.w, dst.h};
                    SDL_RenderCopy(renderer, impl_->ttf_atlas_tex, &ag.src, &d1);
                    SDL_RenderCopy(renderer, impl_->ttf_atlas_tex, &ag.src, &d2);
                    SDL_RenderCopy(renderer, impl_->ttf_atlas_tex, &ag.src, &d3);
                    SDL_RenderCopy(renderer, impl_->ttf_atlas_tex, &ag.src, &d4);
                }
                SDL_SetTextureColorMod(impl_->ttf_atlas_tex, color.r, color.g, color.b);
                SDL_SetTextureAlphaMod(impl_->ttf_atlas_tex, color.a);
                SDL_RenderCopy(renderer, impl_->ttf_atlas_tex, &ag.src, &dst);
            }
            cur_x += ag.advance * ratio;
            ++count;
        }
        return;
    }

    if (!impl_->fallback_tex) return;
    int cur_x = x;
    int cur_y = y;
    const int start_x = x;
    int count = 0;

    for (std::uint32_t cp : cps) {
        if (cp == '\n') {
            cur_x = start_x;
            cur_y += (8 + 4) * s;
            count = 0;
            continue;
        }
        if (cp == '\r') continue;
        if (count >= max_chars) break;
        const int idx = (cp < 256) ? static_cast<int>(cp) : static_cast<int>('?');
        const SDL_Rect src{(idx % 16) * 8, (idx / 16) * 8, 8, 8};
        const SDL_Rect dst{cur_x, cur_y, 8 * s, 8 * s};
        if (shadow) {
            SDL_SetTextureColorMod(impl_->fallback_tex, 0, 0, 0);
            SDL_SetTextureAlphaMod(impl_->fallback_tex, static_cast<Uint8>((color.a * 190) / 255));
            const SDL_Rect ds{cur_x + 1, cur_y + 1, 8 * s, 8 * s};
            SDL_RenderCopy(renderer, impl_->fallback_tex, &src, &ds);
        }
        SDL_SetTextureColorMod(impl_->fallback_tex, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(impl_->fallback_tex, color.a);
        SDL_RenderCopy(renderer, impl_->fallback_tex, &src, &dst);
        cur_x += 8 * s;
        ++count;
    }
}

#endif

} // namespace rpg2lx

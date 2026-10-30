#include "rpg2lx/library_view.hpp"
#include "rpg2lx/launcher.hpp"
#include "rpg2lx/project_source.hpp"
#include "rpg2lx/json/json_parser.hpp"

#include <SDL.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "font8x8_basic.h"
#include "font8x8_ext_latin.h"
#include "stb_image.h"

namespace fs = std::filesystem;

namespace rpg2lx {

namespace {

constexpr int kScreenWidth = 640;
constexpr int kScreenHeight = 480;

constexpr std::size_t kMaxFileReadBytes = 16 * 1024 * 1024;    // 16 MiB max encoded PNG file
constexpr std::size_t kMaxDecodedImageBytes = 32 * 1024 * 1024; // 32 MiB max decoded uncompressed pixels
constexpr std::size_t kTextureCacheBudget = 64 * 1024 * 1024;   // 64 MiB max texture cache

struct Color {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a = 255;
};

constexpr Color kBgColor{13, 17, 23};          // Deep dark backdrop
constexpr Color kSidebarBg{18, 24, 34};        // Left panel
constexpr Color kCardBg{26, 36, 50};           // Game list item default
constexpr Color kCardSelectedBg{38, 64, 88};    // Game list item selected
constexpr Color kAccentColor{66, 219, 174};    // RPG2LX emerald green
constexpr Color kInkBright{240, 246, 252};     // White / primary text
constexpr Color kInmMuted{139, 155, 175};      // Subtitle / muted text
constexpr Color kWarningColor{246, 190, 91};   // Warning amber
constexpr Color kBorderColor{36, 48, 66};      // Subtle card border
constexpr Color kPreviewFrameBg{20, 28, 40};   // Preview box bg
constexpr Color kBadgeBg{28, 40, 56};          // Pill badge background

void draw_rect(SDL_Renderer* renderer, int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

void draw_rect_outline(SDL_Renderer* renderer, int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r{x, y, w, h};
    SDL_RenderDrawRect(renderer, &r);
}

std::vector<unsigned> decode_utf8(const std::string& text) {
    std::vector<unsigned> result;
    for (std::size_t i = 0; i < text.size();) {
        unsigned cp = static_cast<unsigned char>(text[i++]);
        unsigned remaining = 0;
        if (cp >= 0xF0) { cp &= 7; remaining = 3; }
        else if (cp >= 0xE0) { cp &= 15; remaining = 2; }
        else if (cp >= 0xC0) { cp &= 31; remaining = 1; }
        while (remaining-- && i < text.size()) {
            cp = (cp << 6) | (static_cast<unsigned char>(text[i++]) & 63);
        }
        result.push_back(cp);
    }
    return result;
}

void draw_text(SDL_Renderer* renderer, const std::string& value, int x, int y,
               Color color = kInkBright, int scale = 1, int max_chars = 40) {
    auto cps = decode_utf8(value);
    if (static_cast<int>(cps.size()) > max_chars) {
        cps.resize(static_cast<std::size_t>(max_chars));
        if (max_chars >= 3) {
            std::fill(cps.end() - 3, cps.end(), '.');
        }
    }
    for (unsigned cp : cps) {
        if (cp < 32 || cp == 127) cp = ' ';
        const unsigned char* glyph = (cp < 128)
            ? font8x8_basic[cp]
            : (cp >= 160 && cp <= 255 ? font8x8_ext_latin[cp - 160] : font8x8_basic['?']);
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (static_cast<unsigned char>(glyph[row]) & (1u << col)) {
                    draw_rect(renderer, x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        x += 8 * scale;
    }
}

void draw_wrapped_text(SDL_Renderer* renderer, const std::string& value, int x, int y,
                       Color color, int max_chars = 38, int max_lines = 3, int scale = 1) {
    std::string line;
    int count = 0;
    for (char c : value) {
        line += c;
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++count;
        if (count >= max_chars) {
            draw_text(renderer, line, x, y, color, scale, max_chars);
            y += 8 * scale + 4;
            line.clear();
            count = 0;
            if (--max_lines == 0) return;
        }
    }
    if (!line.empty()) {
        draw_text(renderer, line, x, y, color, scale, max_chars);
    }
}

bool is_safe_filename(const std::string& name) {
    if (name.empty()) return false;
    if (name == "." || name == "..") return false;
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            return false;
        }
    }
    const auto u8f = fs::u8path(name);
    return u8f == u8f.filename();
}

} // namespace

struct LibraryView::Impl {
    SDL_Renderer* renderer;

    using TexturePtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

    struct LoadedTexture {
        TexturePtr texture;
        int w = 0;
        int h = 0;
        std::size_t memory_bytes = 0;
    };

    // Cached textures keyed by normalized absolute or canonical path
    std::map<fs::path, LoadedTexture> texture_cache_;
    std::size_t total_cache_bytes_ = 0;

    // Title info extraction cache per project root
    struct CachedTitleInfo {
        std::string title1;
        std::string title2;
        bool parsed = false;
    };
    std::map<fs::path, CachedTitleInfo> title_cache_;

    explicit Impl(SDL_Renderer* r) : renderer(r) {}

    ~Impl() {
        clear_cache();
    }

    void clear_cache() {
        texture_cache_.clear();
        total_cache_bytes_ = 0;
    }

    CachedTitleInfo get_title_info(const fs::path& project_path) {
        const auto it = title_cache_.find(project_path);
        if (it != title_cache_.end()) {
            return it->second;
        }

        CachedTitleInfo info;
        info.parsed = true;

        const auto resolved = resolve_mv_project(project_path);
        const fs::path root = resolved ? resolved.source->root : project_path;
        const fs::path system_path = root / "data" / "System.json";

        std::error_code ec;
        if (!fs::exists(system_path, ec) || !fs::is_regular_file(system_path, ec)) {
            title_cache_[project_path] = info;
            return info;
        }

        const auto file_size = fs::file_size(system_path, ec);
        if (ec || file_size > 8 * 1024 * 1024) { // Don't parse unreasonably huge JSON
            title_cache_[project_path] = info;
            return info;
        }

        std::ifstream input(system_path, std::ios::binary);
        if (!input.is_open()) {
            title_cache_[project_path] = info;
            return info;
        }

        std::string content(static_cast<std::size_t>(file_size),'\0');
        if(!input.read(content.data(),static_cast<std::streamsize>(content.size())))return info;
        std::string err;
        const auto root_val = JsonParser::parse(content, err);
        if (err.empty() && root_val.is_object()) {
            const auto* t1 = root_val.find("title1Name");
            if (t1 && t1->is_string()) {
                info.title1 = t1->get_string();
            }
            const auto* t2 = root_val.find("title2Name");
            if (t2 && t2->is_string()) {
                info.title2 = t2->get_string();
            }
        }

        title_cache_[project_path] = info;
        return info;
    }

    LoadedTexture* load_png(const fs::path& file_path) {
        const auto it = texture_cache_.find(file_path);
        if (it != texture_cache_.end()) {
            return &it->second;
        }

        std::error_code ec;
        if (!fs::exists(file_path, ec) || !fs::is_regular_file(file_path, ec)) {
            return nullptr;
        }

        const auto file_size = fs::file_size(file_path, ec);
        if (ec || file_size == 0 || file_size > kMaxFileReadBytes) {
            return nullptr;
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return nullptr;
        }

        std::vector<unsigned char> buffer(static_cast<std::size_t>(file_size));
        if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size))) {
            return nullptr;
        }

        int w = 0, h = 0, channels = 0;
        if (!stbi_info_from_memory(buffer.data(), static_cast<int>(buffer.size()), &w, &h, &channels)) {
            return nullptr;
        }

        if (w <= 0 || h <= 0) return nullptr;
        const std::size_t decoded_bytes = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4;
        if (decoded_bytes > kMaxDecodedImageBytes) {
            return nullptr;
        }

        // Cache eviction if budget exceeded
        if (total_cache_bytes_ + decoded_bytes > kTextureCacheBudget) {
            clear_cache();
        }

        unsigned char* decoded = stbi_load_from_memory(
            buffer.data(), static_cast<int>(buffer.size()), &w, &h, &channels, 4);
        if (!decoded) {
            return nullptr;
        }

        std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixel_guard(decoded, stbi_image_free);

        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
            decoded, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
        if (!surface) {
            return nullptr;
        }

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!tex) {
            return nullptr;
        }

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

        LoadedTexture entry{
            TexturePtr(tex, SDL_DestroyTexture),
            w,
            h,
            decoded_bytes
        };

        total_cache_bytes_ += decoded_bytes;
        auto insert_res = texture_cache_.emplace(file_path, std::move(entry));
        return &insert_res.first->second;
    }

    void draw_title_preview(const fs::path& project_path, const SDL_Rect& bounds) {
        // Draw frame background
        draw_rect(renderer, bounds.x, bounds.y, bounds.w, bounds.h, kPreviewFrameBg);
        draw_rect_outline(renderer, bounds.x, bounds.y, bounds.w, bounds.h, kBorderColor);

        const auto resolved = resolve_mv_project(project_path);
        const fs::path root = resolved ? resolved.source->root : project_path;
        const CachedTitleInfo info = get_title_info(project_path);

        bool drawn_layer = false;
        SDL_Rect canvas=bounds;
        auto fit=[&](int w,int h){
            const double scale=std::min(static_cast<double>(bounds.w)/w,static_cast<double>(bounds.h)/h);
            canvas.w=std::max(1,static_cast<int>(w*scale));canvas.h=std::max(1,static_cast<int>(h*scale));
            canvas.x=bounds.x+(bounds.w-canvas.w)/2;canvas.y=bounds.y+(bounds.h-canvas.h)/2;
        };

        // Render Title1 layer if present
        if (!info.title1.empty() && is_safe_filename(info.title1)) {
            const fs::path t1_path = root / "img" / "titles1" / fs::u8path(info.title1 + ".png");
            LoadedTexture* tex1 = load_png(t1_path);
            if (tex1 && tex1->texture) {
                fit(tex1->w,tex1->h);
                SDL_RenderCopy(renderer, tex1->texture.get(), nullptr, &canvas);
                drawn_layer = true;
            }
        }

        // Render Title2 layer (composite on top) if present
        if (!info.title2.empty() && is_safe_filename(info.title2)) {
            const fs::path t2_path = root / "img" / "titles2" / fs::u8path(info.title2 + ".png");
            LoadedTexture* tex2 = load_png(t2_path);
            if (tex2 && tex2->texture) {
                if(!drawn_layer)fit(tex2->w,tex2->h);
                SDL_RenderCopy(renderer, tex2->texture.get(), nullptr, &canvas);
                drawn_layer = true;
            }
        }

        // Elegant fallback card if no images were available or defined
        if (!drawn_layer) {
            // Elegant pattern & badge inside frame
            draw_rect(renderer, bounds.x + 8, bounds.y + 8, bounds.w - 16, bounds.h - 16, kSidebarBg);
            draw_rect_outline(renderer, bounds.x + 8, bounds.y + 8, bounds.w - 16, bounds.h - 16, kCardBg);

            // Watermark icon/text
            draw_text(renderer, "[ RPG MAKER MV ]", bounds.x + (bounds.w - 16 * 8) / 2, bounds.y + bounds.h / 2 - 14,
                      kInmMuted, 1, 24);
            draw_text(renderer, "Sin preview de titulo", bounds.x + (bounds.w - 21 * 8) / 2, bounds.y + bounds.h / 2 + 6,
                      Color{90, 105, 125}, 1, 26);
        } else {
            // Inner subtle border highlight on preview
            draw_rect_outline(renderer, bounds.x + 1, bounds.y + 1, bounds.w - 2, bounds.h - 2, Color{255, 255, 255, 30});
        }
    }

    void draw(const LauncherModel& model) {
        // Clear background
        draw_rect(renderer, 0, 0, kScreenWidth, kScreenHeight, kBgColor);

        // Header bar
        draw_rect(renderer, 0, 0, kScreenWidth, 54, kSidebarBg);
        draw_rect(renderer, 0, 54, kScreenWidth, 2, kBorderColor);

        // Header branding
        draw_text(renderer, "RPG2LX", 20, 19, kAccentColor, 2, 10);
        draw_text(renderer, "PORTMASTER RUNTIME", 144, 20, kInkBright, 1, 25);
        draw_text(renderer, "BIBLIOTECA RPG MAKER MV", 144, 34, kInmMuted, 1, 30);

        // Count badge on top right
        const std::string count_str = std::to_string(model.games().size()) + " JUEGOS";
        draw_rect(renderer, kScreenWidth - 120, 16, 100, 22, kBadgeBg);
        draw_rect_outline(renderer, kScreenWidth - 120, 16, 100, 22, kBorderColor);
        draw_text(renderer, count_str, kScreenWidth - 110, 23, kAccentColor, 1, 12);

        const auto* selected = model.selected();

        if (model.games().empty()) {
            // Empty library state
            draw_rect(renderer, 24, 76, kScreenWidth - 48, 340, kSidebarBg);
            draw_rect_outline(renderer, 24, 76, kScreenWidth - 48, 340, kBorderColor);

            draw_text(renderer, "TU BIBLIOTECA ESTA VACIA", 48, 120, kInkBright, 2, 30);
            draw_wrapped_text(renderer,
                "Copia tus proyectos de RPG Maker MV exportados dentro de la carpeta 'games/'.",
                48, 160, kInmMuted, 52, 2, 1);
            draw_text(renderer, "Estructura requerida: games/<NombreJuego>/data/System.json",
                48, 204, kAccentColor, 1, 62);
            draw_text(renderer, "o bien:             games/<NombreJuego>/www/data/System.json",
                48, 224, kAccentColor, 1, 62);
        } else if (!model.details()) {
            // === Split screen view: List (Left) + Selected Preview & Info (Right) ===

            // Left column: Game list (width = 330)
            constexpr int kListX = 16;
            constexpr int kListY = 66;
            constexpr int kListW = 320;
            constexpr std::size_t kPageSize = 6;
            constexpr int kItemHeight = 52;

            const auto first_index = (model.index() / kPageSize) * kPageSize;
            const auto end_index = std::min(first_index + kPageSize, model.games().size());

            for (std::size_t i = first_index; i < end_index; ++i) {
                const auto& game = model.games()[i];
                const int item_y = kListY + static_cast<int>(i - first_index) * (kItemHeight + 6);
                const bool is_selected = (i == model.index());

                // Card background
                draw_rect(renderer, kListX, item_y, kListW, kItemHeight,
                          is_selected ? kCardSelectedBg : kCardBg);
                draw_rect_outline(renderer, kListX, item_y, kListW, kItemHeight,
                                  is_selected ? kAccentColor : kBorderColor);

                // Left accent indicator for selected item
                if (is_selected) {
                    draw_rect(renderer, kListX, item_y, 4, kItemHeight, kAccentColor);
                }

                // Type badge (MV or !)
                const Color badge_color = game.detected ? kAccentColor : kWarningColor;
                draw_rect(renderer, kListX + 10, item_y + 16, 26, 20, kBadgeBg);
                draw_rect_outline(renderer, kListX + 10, item_y + 16, 26, 20, badge_color);
                draw_text(renderer, game.detected ? "MV" : " !", kListX + 15, item_y + 22, badge_color, 1, 3);

                // Game Title & Directory Name
                draw_text(renderer, game.title, kListX + 44, item_y + 12,
                          is_selected ? kInkBright : Color{210, 222, 235}, 1, 32);
                draw_text(renderer, game.id, kListX + 44, item_y + 28,
                          kInmMuted, 1, 34);
            }

            // Pagination text
            const std::string page_text = std::to_string(model.index() + 1) + " de " +
                                          std::to_string(model.games().size());
            draw_text(renderer, page_text, kListX + 4, 420, kInmMuted, 1, 20);

            // Right column: Preview box & Quick info
            constexpr int kRightX = 348;
            constexpr int kRightY = 66;
            constexpr int kRightW = 276;

            if (selected) {
                // Title graphic preview box (16:9 approx, 276 x 155)
                const SDL_Rect preview_rect{kRightX, kRightY, kRightW, 155};
                draw_title_preview(selected->project, preview_rect);

                // Quick metadata panel under preview
                draw_rect(renderer, kRightX, kRightY + 165, kRightW, 178, kSidebarBg);
                draw_rect_outline(renderer, kRightX, kRightY + 165, kRightW, 178, kBorderColor);

                // Title and folder in side panel
                draw_text(renderer, selected->title, kRightX + 12, kRightY + 177, kInkBright, 1, 30);
                draw_text(renderer, "Carpeta: " + selected->id, kRightX + 12, kRightY + 195, kInmMuted, 1, 32);

                // Status chip
                if (selected->detected) {
                    draw_rect(renderer, kRightX + 12, kRightY + 218, 120, 18, Color{18, 56, 42});
                    draw_rect_outline(renderer, kRightX + 12, kRightY + 218, 120, 18, kAccentColor);
                    draw_text(renderer, "ESTRUCTURA MV OK", kRightX + 18, kRightY + 223, kAccentColor, 1, 16);
                } else {
                    draw_rect(renderer, kRightX + 12, kRightY + 218, 124, 18, Color{56, 40, 18});
                    draw_rect_outline(renderer, kRightX + 12, kRightY + 218, 124, 18, kWarningColor);
                    draw_text(renderer, "CARPETA INCOMPLETA", kRightX + 18, kRightY + 223, kWarningColor, 1, 18);
                }

                // Instructions in side panel
                draw_text(renderer, "A / Enter: Ver detalles", kRightX + 12, kRightY + 260, kInkBright, 1, 30);
                draw_text(renderer, "X / P:     Iniciar modo nativo", kRightX + 12, kRightY + 278, kAccentColor, 1, 30);
            }
        } else if (selected) {
            // === Details screen view: Large Title Preview + Full Project Info ===

            constexpr int kPreviewW = 340;
            constexpr int kPreviewH = 191;
            const SDL_Rect preview_rect{20, 68, kPreviewW, kPreviewH};
            draw_title_preview(selected->project, preview_rect);

            // Right column: Detailed stats & checks
            constexpr int kDetailX = 376;
            constexpr int kDetailY = 68;
            constexpr int kDetailW = 244;

            draw_rect(renderer, kDetailX, kDetailY, kDetailW, kPreviewH, kSidebarBg);
            draw_rect_outline(renderer, kDetailX, kDetailY, kDetailW, kPreviewH, kBorderColor);

            draw_text(renderer, "DETALLES DEL PROYECTO", kDetailX + 12, kDetailY + 14, kAccentColor, 1, 26);
            draw_text(renderer, selected->title, kDetailX + 12, kDetailY + 32, kInkBright, 1, 27);
            draw_text(renderer, "ID: " + selected->id, kDetailX + 12, kDetailY + 48, kInmMuted, 1, 27);

            // Status banner
            if (selected->detected) {
                draw_rect(renderer, kDetailX + 12, kDetailY + 70, kDetailW - 24, 24, Color{18, 56, 42});
                draw_rect_outline(renderer, kDetailX + 12, kDetailY + 70, kDetailW - 24, 24, kAccentColor);
                draw_text(renderer, "ESTRUCTURA MV DETECTADA", kDetailX + 20, kDetailY + 78, kAccentColor, 1, 25);
            } else {
                draw_rect(renderer, kDetailX + 12, kDetailY + 70, kDetailW - 24, 24, Color{56, 40, 18});
                draw_rect_outline(renderer, kDetailX + 12, kDetailY + 70, kDetailW - 24, 24, kWarningColor);
                draw_text(renderer, "CARPETA INCOMPLETA", kDetailX + 20, kDetailY + 78, kWarningColor, 1, 25);
            }

            draw_text(renderer, "A / Enter: Comprobar datos", kDetailX + 12, kDetailY + 115, kAccentColor, 1, 28);
            draw_text(renderer, "X / P:     Iniciar modo nativo", kDetailX + 12, kDetailY + 135, kInkBright, 1, 28);

            // Bottom container: Messages and status diagnostics
            draw_rect(renderer, 20, 272, kScreenWidth - 40, 134, kSidebarBg);
            draw_rect_outline(renderer, 20, 272, kScreenWidth - 40, 134, kBorderColor);

            const auto message = model.message().empty()
                ? (selected->detected
                    ? "Partidas y configuracion aisladas de los archivos del juego.\nSoporte actual: mapas, tilesets y eventos basicos."
                    : "No se encontro una estructura MV valida en esta carpeta.")
                : model.message();

            draw_wrapped_text(renderer, message, 36, 290,
                              model.message().empty() ? kInmMuted : kWarningColor, 70, 4, 1);
        }

        // Bottom Footer Bar
        draw_rect(renderer, 0, 442, kScreenWidth, 38, kSidebarBg);
        draw_rect(renderer, 0, 442, kScreenWidth, 1, kBorderColor);

        if (!model.details()) {
            draw_text(renderer, "Cruceta: elegir", 20, 449, kInmMuted, 1, 22);
            draw_text(renderer, "A: detalles   X: jugar", 260, 449, kInkBright, 1, 30);
            draw_text(renderer, "Y: actualizar", 20, 465, kInmMuted, 1, 22);
            draw_text(renderer, "B: salir", 260, 465, kInmMuted, 1, 20);
        } else {
            draw_text(renderer, "A: comprobar datos", 20, 449, kInkBright, 1, 24);
            draw_text(renderer, "X: iniciar juego", 300, 449, kAccentColor, 1, 25);
            draw_text(renderer, "Y: actualizar", 20, 465, kInmMuted, 1, 24);
            draw_text(renderer, "B: volver a la lista", 300, 465, kInmMuted, 1, 25);
        }
    }
};

LibraryView::LibraryView(SDL_Renderer* renderer)
    : impl_(std::make_unique<Impl>(renderer)) {}

LibraryView::~LibraryView() = default;

LibraryView::LibraryView(LibraryView&&) noexcept = default;
LibraryView& LibraryView::operator=(LibraryView&&) noexcept = default;

void LibraryView::draw(const LauncherModel& model) {
    if (impl_) {
        impl_->draw(model);
    }
}

} // namespace rpg2lx

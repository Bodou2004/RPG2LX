#include "rpg2lx/library_view.hpp"
#include "rpg2lx/font_renderer.hpp"
#include "rpg2lx/launcher.hpp"
#include "rpg2lx/project_source.hpp"
#include "rpg2lx/json/json_parser.hpp"

#include <SDL.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "stb_image.h"

namespace fs = std::filesystem;

namespace rpg2lx {

namespace {

constexpr int kScreenWidth = 640;
constexpr int kScreenHeight = 480;

constexpr std::size_t kMaxFileReadBytes = 16 * 1024 * 1024;
constexpr std::size_t kMaxDecodedImageBytes = 32 * 1024 * 1024;
constexpr std::size_t kTextureCacheBudget = 64 * 1024 * 1024;

struct Color {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a = 255;
};

constexpr Color kBgColor{13, 17, 23};
constexpr Color kSidebarBg{18, 24, 34};
constexpr Color kCardBg{26, 36, 50};
constexpr Color kCardSelectedBg{38, 64, 88};
constexpr Color kAccentColor{66, 219, 174};
constexpr Color kInkBright{240, 246, 252};
constexpr Color kInmMuted{155, 170, 190};
constexpr Color kWarningColor{246, 190, 91};
constexpr Color kBorderColor{36, 48, 66};
constexpr Color kPreviewFrameBg{20, 28, 40};
constexpr Color kBadgeBg{28, 40, 56};

void draw_rect(SDL_Renderer* renderer, int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawBlendMode(renderer, c.a < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

void draw_rect_outline(SDL_Renderer* renderer, int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawBlendMode(renderer, c.a < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r{x, y, w, h};
    SDL_RenderDrawRect(renderer, &r);
}

bool is_safe_filename(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            return false;
        }
    }
    const auto u8f = fs::u8path(name);
    return u8f == u8f.filename();
}

void decrypt_rpgmvp_if_needed(std::vector<unsigned char>& buffer, const std::string& hex_key) {
    if (buffer.size() < 32 || hex_key.size() < 32) return;
    if (std::memcmp(buffer.data(), "RPGMV", 5) != 0) return;
    unsigned char key[16]{};
    for (int i = 0; i < 16; ++i) {
        unsigned int b = 0;
        std::stringstream ss;
        ss << std::hex << hex_key.substr(static_cast<std::size_t>(i * 2), 2);
        ss >> b;
        key[i] = static_cast<unsigned char>(b);
    }
    std::vector<unsigned char> out(buffer.size() - 16);
    std::memcpy(out.data(), buffer.data() + 16, out.size());
    for (std::size_t i = 0; i < 16 && i < out.size(); ++i) {
        out[i] ^= key[i];
    }
    buffer = std::move(out);
}

} // namespace

struct LibraryView::Impl {
    SDL_Renderer* renderer;
    FontRenderer font;
    fs::path font_project;
    bool font_initialized = false;

    using TexturePtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

    struct LoadedTexture {
        TexturePtr texture;
        int w = 0;
        int h = 0;
        std::size_t memory_bytes = 0;
    };

    std::map<fs::path, LoadedTexture> texture_cache_;
    std::size_t total_cache_bytes_ = 0;

    struct CachedTitleInfo {
        std::string title1;
        std::string title2;
        std::string encryption_key;
        bool parsed = false;
    };
    std::map<fs::path, CachedTitleInfo> title_cache_;

    explicit Impl(SDL_Renderer* r) : renderer(r) {}

    ~Impl() {
        clear_cache();
    }

    void ensure_font(const LauncherModel& model) {
        fs::path candidate;
        for (const auto& g : model.games()) {
            if (g.detected) {
                const auto resolved = resolve_mv_project(g.project);
                candidate = resolved ? resolved.source->root : g.project;
                break;
            }
        }
        if (!font_initialized || (!candidate.empty() && font_project != candidate && !font.is_truetype())) {
            font.init(renderer, candidate);
            font_project = candidate;
            font_initialized = true;
        }
    }

    void put_text(const std::string& text, int x, int y, Color c = kInkBright,
                  int scale = 1, int max_chars = 60) {
        font.draw_text(renderer, text, x, y, SDL_Color{c.r, c.g, c.b, c.a}, scale, true, max_chars);
    }

    void put_wrapped(const std::string& value, int x, int y, Color c,
                     int max_chars = 46, int max_lines = 3, int scale = 1) {
        const auto cps = decode_utf8_codepoints(value);
        std::string line;
        int count = 0;
        int lines_drawn = 0;
        std::size_t byte_i = 0;
        while (byte_i < value.size() && lines_drawn < max_lines) {
            const unsigned char b0 = static_cast<unsigned char>(value[byte_i]);
            std::size_t step = 1;
            if ((b0 & 0xF8u) == 0xF0u) step = 4;
            else if ((b0 & 0xF0u) == 0xE0u) step = 3;
            else if ((b0 & 0xE0u) == 0xC0u) step = 2;
            if (byte_i + step > value.size()) step = 1;

            if (value[byte_i] == '\n') {
                put_text(line, x, y, c, scale, max_chars + 4);
                y += font.is_truetype() ? 20 * scale : 13 * scale;
                line.clear();
                count = 0;
                ++lines_drawn;
                ++byte_i;
                continue;
            }
            line.append(value, byte_i, step);
            byte_i += step;
            ++count;
            if (count >= max_chars) {
                put_text(line, x, y, c, scale, max_chars + 4);
                y += font.is_truetype() ? 20 * scale : 13 * scale;
                line.clear();
                count = 0;
                ++lines_drawn;
            }
        }
        if (!line.empty() && lines_drawn < max_lines) {
            put_text(line, x, y, c, scale, max_chars + 4);
        }
        (void)cps;
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
        if (ec || file_size > 8 * 1024 * 1024) {
            title_cache_[project_path] = info;
            return info;
        }

        std::ifstream input(system_path, std::ios::binary);
        if (!input.is_open()) {
            title_cache_[project_path] = info;
            return info;
        }

        std::string content(static_cast<std::size_t>(file_size), '\0');
        if (!input.read(content.data(), static_cast<std::streamsize>(content.size()))) return info;
        std::string err;
        const auto root_val = JsonParser::parse(content, err);
        if (err.empty() && root_val.is_object()) {
            if (const auto* t1 = root_val.find("title1Name"); t1 && t1->is_string()) info.title1 = t1->get_string();
            if (const auto* t2 = root_val.find("title2Name"); t2 && t2->is_string()) info.title2 = t2->get_string();
            if (const auto* ek = root_val.find("encryptionKey"); ek && ek->is_string()) info.encryption_key = ek->get_string();
        }

        title_cache_[project_path] = info;
        return info;
    }

    LoadedTexture* load_png(const fs::path& base_no_ext, const std::string& enc_key) {
        const auto it = texture_cache_.find(base_no_ext);
        if (it != texture_cache_.end()) {
            return it->second.texture ? &it->second : nullptr;
        }

        fs::path file_path = fs::u8path(base_no_ext.u8string() + ".png");
        std::error_code ec;
        if (!fs::exists(file_path, ec)) {
            fs::path enc = fs::u8path(base_no_ext.u8string() + ".rpgmvp");
            if (fs::exists(enc, ec)) file_path = enc;
            else {
                texture_cache_.emplace(base_no_ext, LoadedTexture{TexturePtr(nullptr, SDL_DestroyTexture), 0, 0, 0});
                return nullptr;
            }
        }

        const auto file_size = fs::file_size(file_path, ec);
        if (ec || file_size == 0 || file_size > kMaxFileReadBytes) return nullptr;

        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) return nullptr;

        std::vector<unsigned char> buffer(static_cast<std::size_t>(file_size));
        if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size))) return nullptr;
        decrypt_rpgmvp_if_needed(buffer, enc_key);

        int w = 0, h = 0, channels = 0;
        if (!stbi_info_from_memory(buffer.data(), static_cast<int>(buffer.size()), &w, &h, &channels)) return nullptr;
        if (w <= 0 || h <= 0) return nullptr;

        const std::size_t decoded_bytes = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4;
        if (decoded_bytes > kMaxDecodedImageBytes) return nullptr;
        if (total_cache_bytes_ + decoded_bytes > kTextureCacheBudget) clear_cache();

        unsigned char* decoded = stbi_load_from_memory(
            buffer.data(), static_cast<int>(buffer.size()), &w, &h, &channels, 4);
        if (!decoded) return nullptr;
        std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixel_guard(decoded, stbi_image_free);

        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
            decoded, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
        if (!surface) return nullptr;

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!tex) return nullptr;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

        LoadedTexture entry{TexturePtr(tex, SDL_DestroyTexture), w, h, decoded_bytes};
        total_cache_bytes_ += decoded_bytes;
        auto insert_res = texture_cache_.emplace(base_no_ext, std::move(entry));
        return &insert_res.first->second;
    }

    void draw_title_preview(const fs::path& project_path, const SDL_Rect& bounds) {
        draw_rect(renderer, bounds.x, bounds.y, bounds.w, bounds.h, kPreviewFrameBg);
        draw_rect_outline(renderer, bounds.x, bounds.y, bounds.w, bounds.h, kBorderColor);

        const auto resolved = resolve_mv_project(project_path);
        const fs::path root = resolved ? resolved.source->root : project_path;
        const CachedTitleInfo info = get_title_info(project_path);

        bool drawn_layer = false;
        SDL_Rect canvas = bounds;
        auto fit = [&](int w, int h) {
            const double scale = std::min(static_cast<double>(bounds.w) / w, static_cast<double>(bounds.h) / h);
            canvas.w = std::max(1, static_cast<int>(w * scale));
            canvas.h = std::max(1, static_cast<int>(h * scale));
            canvas.x = bounds.x + (bounds.w - canvas.w) / 2;
            canvas.y = bounds.y + (bounds.h - canvas.h) / 2;
        };

        if (!info.title1.empty() && is_safe_filename(info.title1)) {
            LoadedTexture* tex1 = load_png(root / "img" / "titles1" / fs::u8path(info.title1), info.encryption_key);
            if (tex1 && tex1->texture) {
                fit(tex1->w, tex1->h);
                SDL_RenderCopy(renderer, tex1->texture.get(), nullptr, &canvas);
                drawn_layer = true;
            }
        }

        if (!info.title2.empty() && is_safe_filename(info.title2)) {
            LoadedTexture* tex2 = load_png(root / "img" / "titles2" / fs::u8path(info.title2), info.encryption_key);
            if (tex2 && tex2->texture) {
                if (!drawn_layer) fit(tex2->w, tex2->h);
                SDL_RenderCopy(renderer, tex2->texture.get(), nullptr, &canvas);
                drawn_layer = true;
            }
        }

        if (!drawn_layer) {
            draw_rect(renderer, bounds.x + 8, bounds.y + 8, bounds.w - 16, bounds.h - 16, kSidebarBg);
            draw_rect_outline(renderer, bounds.x + 8, bounds.y + 8, bounds.w - 16, bounds.h - 16, kCardBg);
            put_text("RPG MAKER MV", bounds.x + bounds.w / 2 - 58, bounds.y + bounds.h / 2 - 14, kInmMuted, 1, 24);
            put_text("Listo para iniciar", bounds.x + bounds.w / 2 - 64, bounds.y + bounds.h / 2 + 6, Color{95, 112, 134}, 1, 26);
        } else {
            draw_rect_outline(renderer, bounds.x + 1, bounds.y + 1, bounds.w - 2, bounds.h - 2, Color{255, 255, 255, 30});
        }
    }

    void draw(const LauncherModel& model) {
        ensure_font(model);
        draw_rect(renderer, 0, 0, kScreenWidth, kScreenHeight, kBgColor);

        // Header bar
        draw_rect(renderer, 0, 0, kScreenWidth, 52, kSidebarBg);
        draw_rect(renderer, 0, 52, kScreenWidth, 2, kBorderColor);

        put_text("RPG2LX", 20, 14, kAccentColor, 2, 10);
        put_text("RPG MAKER MV - PORTMASTER 1:1", 136, 18, kInkBright, 1, 36);

        const std::string count_str = std::to_string(model.games().size()) + (model.games().size() == 1 ? " JUEGO" : " JUEGOS");
        draw_rect(renderer, kScreenWidth - 118, 15, 98, 22, kBadgeBg);
        draw_rect_outline(renderer, kScreenWidth - 118, 15, 98, 22, kBorderColor);
        put_text(count_str, kScreenWidth - 106, 19, kAccentColor, 1, 14);

        const auto* selected = model.selected();

        if (model.games().empty()) {
            draw_rect(renderer, 24, 74, kScreenWidth - 48, 344, kSidebarBg);
            draw_rect_outline(renderer, 24, 74, kScreenWidth - 48, 344, kBorderColor);

            put_text("BIBLIOTECA VACIA", 48, 114, kInkBright, 2, 30);
            put_wrapped(
                "Copia tus proyectos exportados de RPG Maker MV dentro de la carpeta 'games/'.",
                48, 162, kInmMuted, 54, 2, 1);
            put_text("Ruta admitida 1: games/<Juego>/data/System.json", 48, 214, kAccentColor, 1, 62);
            put_text("Ruta admitida 2: games/<Juego>/www/data/System.json", 48, 236, kAccentColor, 1, 62);
        } else {
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

                draw_rect(renderer, kListX, item_y, kListW, kItemHeight,
                          is_selected ? kCardSelectedBg : kCardBg);
                draw_rect_outline(renderer, kListX, item_y, kListW, kItemHeight,
                                  is_selected ? kAccentColor : kBorderColor);

                if (is_selected) {
                    draw_rect(renderer, kListX, item_y, 4, kItemHeight, kAccentColor);
                }

                const Color badge_color = game.detected ? kAccentColor : kWarningColor;
                draw_rect(renderer, kListX + 10, item_y + 15, 28, 22, kBadgeBg);
                draw_rect_outline(renderer, kListX + 10, item_y + 15, 28, 22, badge_color);
                put_text(game.detected ? "MV" : " !", kListX + 14, item_y + 19, badge_color, 1, 3);

                put_text(game.title, kListX + 46, item_y + 9,
                         is_selected ? kInkBright : Color{210, 222, 235}, 1, 30);
                put_text(game.id, kListX + 46, item_y + 28, kInmMuted, 1, 32);
            }

            const std::string page_text = std::to_string(model.index() + 1) + " / " +
                                          std::to_string(model.games().size());
            put_text(page_text, kListX + 4, 418, kInmMuted, 1, 20);

            constexpr int kRightX = 348;
            constexpr int kRightY = 66;
            constexpr int kRightW = 276;

            if (selected) {
                const SDL_Rect preview_rect{kRightX, kRightY, kRightW, 165};
                draw_title_preview(selected->project, preview_rect);

                draw_rect(renderer, kRightX, kRightY + 175, kRightW, 172, kSidebarBg);
                draw_rect_outline(renderer, kRightX, kRightY + 175, kRightW, 172, kBorderColor);

                put_text(selected->title, kRightX + 12, kRightY + 186, kInkBright, 1, 28);
                put_text("Carpeta: " + selected->id, kRightX + 12, kRightY + 208, kInmMuted, 1, 30);

                if (selected->detected) {
                    draw_rect(renderer, kRightX + 12, kRightY + 234, 152, 22, Color{18, 56, 42});
                    draw_rect_outline(renderer, kRightX + 12, kRightY + 234, 152, 22, kAccentColor);
                    put_text("PROYECTO MV LISTO", kRightX + 20, kRightY + 238, kAccentColor, 1, 20);
                } else {
                    draw_rect(renderer, kRightX + 12, kRightY + 234, 162, 22, Color{56, 40, 18});
                    draw_rect_outline(renderer, kRightX + 12, kRightY + 234, 162, 22, kWarningColor);
                    put_text("CARPETA INCOMPLETA", kRightX + 20, kRightY + 238, kWarningColor, 1, 20);
                }

                if (!model.message().empty()) {
                    put_wrapped(model.message(), kRightX + 12, kRightY + 266, kWarningColor, 32, 3, 1);
                } else {
                    put_text("B / Enter: Iniciar juego", kRightX + 12, kRightY + 272, kAccentColor, 1, 28);
                    put_text("Select+Start: Volver / Salir", kRightX + 12, kRightY + 294, kInmMuted, 1, 30);
                }
            }
        }

        // Bottom Footer Bar
        draw_rect(renderer, 0, 444, kScreenWidth, 36, kSidebarBg);
        draw_rect(renderer, 0, 444, kScreenWidth, 1, kBorderColor);
        put_text("Cruceta: Elegir    B / A: Jugar    R: Actualizar    Start: Salir",
                 20, 454, kInkBright, 1, 72);
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

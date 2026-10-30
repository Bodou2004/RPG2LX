#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#ifdef RPG2LX_WITH_SDL
#include <SDL.h>
#endif

namespace rpg2lx {

struct RgbaColor {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

enum class TextAlign {
    Left,
    Center,
    Right
};

struct TextStyle {
    int font_size = 28;
    bool italic = false;
    RgbaColor text_color{255, 255, 255, 255};
    RgbaColor outline_color{0, 0, 0, 160};
    int outline_width = 4;
    std::uint8_t paint_opacity = 255;
};

RgbaColor parse_css_color(const std::string& css, RgbaColor fallback = {255, 255, 255, 255});
std::vector<std::uint32_t> decode_utf8_codepoints(const std::string& text);

class FontRenderer {
public:
    FontRenderer();
    ~FontRenderer();

    FontRenderer(const FontRenderer&) = delete;
    FontRenderer& operator=(const FontRenderer&) = delete;

    FontRenderer(FontRenderer&&) noexcept;
    FontRenderer& operator=(FontRenderer&&) noexcept;

    // Load TrueType font (.ttf/.otf) from project/fonts or explicit file path
    bool load_font_file(const std::filesystem::path& font_file);
    bool load_project_font(const std::filesystem::path& project_path);

    // Handheld readability factor (e.g. 1.14 on 640x480 screens when downscaling 816x624)
    void set_readability_scale(float factor);
    float readability_scale() const;

    bool is_truetype() const;
    int line_height() const;

    // Software RGBA32 canvas text measurement & outlined rendering (1:1 RPG Maker MV Bitmap.drawText)
    int measure_text_px(const std::string& text, int font_size = 28, bool italic = false) const;

    void draw_text_rgba(std::uint32_t* pixels, int canvas_w, int canvas_h,
                        const std::string& text, int x, int y,
                        int max_width, int line_height,
                        TextAlign align, const TextStyle& style);

#ifdef RPG2LX_WITH_SDL
    bool init(SDL_Renderer* renderer, const std::filesystem::path& project_path = "");
    void release();

    void draw_text(SDL_Renderer* renderer, const std::string& text, int x, int y,
                   SDL_Color color = {255, 255, 255, 255}, int scale = 1,
                   bool shadow = true, int max_chars = 120);

    int text_width(const std::string& text, int scale = 1) const;
#endif

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rpg2lx

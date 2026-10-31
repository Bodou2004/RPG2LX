#pragma once

// Full HTML5 Canvas 2D drawing (paths, strokes, gradients, patterns, transforms, clipping,
// shadows) for NativeBitmap, backed by canvas_ity. The JS context records path commands with
// the transform in effect when each was issued (as the spec requires) and calls canvas2d_draw
// on fill/stroke/clip-affected operations. Only the affected rectangle is rasterised.

#include "canvas_ity.hpp"
#include "mv_bitmap_ops.hpp"
#include "rpg2lx/font_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace rpg2lx {
namespace mv_detail {

// Path encoding: [command, args..., a, b, c, d, e, f] where a..f is the transform at that time.
enum PathCommand { kMoveTo = 0, kLineTo, kQuadTo, kBezierTo, kArc, kArcTo, kRect, kClose, kEllipse };

inline int path_arg_count(int command) {
    switch (command) {
        case kMoveTo: case kLineTo: return 2;
        case kQuadTo: case kRect: return 4;
        case kBezierTo: case kArc: return 6;
        case kArcTo: return 5;
        case kEllipse: return 8;
        default: return 0;
    }
}

struct Canvas2DStyle {
    enum Kind { Color, Linear, Radial, Pattern } kind = Color;
    RgbaColor color{0, 0, 0, 255};
    double x0 = 0, y0 = 0, r0 = 0, x1 = 0, y1 = 0, r1 = 0;
    std::vector<std::pair<double, RgbaColor>> stops;
    const NativeBitmap* pattern = nullptr;
    canvas_ity::repetition_style repetition = canvas_ity::repeat;
    double matrix[6] = {1, 0, 0, 1, 0, 0}; // transform when the gradient/pattern is used
};

struct Canvas2DState {
    double matrix[6] = {1, 0, 0, 1, 0, 0};
    double global_alpha = 1.0;
    std::string composite = "source-over";
    double line_width = 1.0;
    canvas_ity::cap_style line_cap = canvas_ity::butt;
    canvas_ity::join_style line_join = canvas_ity::miter;
    double miter_limit = 10.0;
    std::vector<float> dash;
    double dash_offset = 0.0;
    RgbaColor shadow_color{0, 0, 0, 0};
    double shadow_blur = 0, shadow_x = 0, shadow_y = 0;
    Canvas2DStyle style;
};

enum class Canvas2DMode { Fill, Stroke, Image, Clear };

struct Canvas2DImage {
    const NativeBitmap* source = nullptr;
    double sx = 0, sy = 0, sw = 0, sh = 0, dx = 0, dy = 0, dw = 0, dh = 0;
};

namespace canvas2d_impl {

struct Box {
    double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
    void add(double x, double y) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
    bool empty() const { return x1 < x0 || y1 < y0; }
};

inline void apply(const double* m, double x, double y, double& ox, double& oy) {
    ox = m[0] * x + m[2] * y + m[4];
    oy = m[1] * x + m[3] * y + m[5];
}

inline double max_scale(const double* m) {
    return std::max(std::hypot(m[0], m[1]), std::hypot(m[2], m[3]));
}

inline Box path_bounds(const std::vector<double>& path) {
    Box box;
    std::size_t i = 0;
    while (i < path.size()) {
        const int command = static_cast<int>(path[i]);
        const int n = path_arg_count(command);
        if (i + 1 + n + 6 > path.size()) break;
        const double* a = &path[i + 1];
        const double* m = &path[i + 1 + n];
        double x = 0, y = 0;
        auto add_point = [&](double px, double py) { apply(m, px, py, x, y); box.add(x, y); };
        auto add_circle = [&](double cx, double cy, double r) {
            apply(m, cx, cy, x, y);
            const double s = std::abs(r) * max_scale(m);
            box.add(x - s, y - s);
            box.add(x + s, y + s);
        };
        switch (command) {
            case kMoveTo: case kLineTo: add_point(a[0], a[1]); break;
            case kQuadTo: add_point(a[0], a[1]); add_point(a[2], a[3]); break;
            case kBezierTo: add_point(a[0], a[1]); add_point(a[2], a[3]); add_point(a[4], a[5]); break;
            case kArc: add_circle(a[0], a[1], a[2]); break;
            case kArcTo: add_point(a[0], a[1]); add_point(a[2], a[3]); break;
            case kRect: add_point(a[0], a[1]); add_point(a[0] + a[2], a[1]); add_point(a[0], a[1] + a[3]); add_point(a[0] + a[2], a[1] + a[3]); break;
            case kEllipse: add_circle(a[0], a[1], std::max(std::abs(a[2]), std::abs(a[3]))); break;
            default: break;
        }
        i += 1 + n + 6;
    }
    return box;
}

inline void set_matrix(canvas_ity::canvas& c, const double* m, double ox, double oy) {
    c.set_transform(static_cast<float>(m[0]), static_cast<float>(m[1]), static_cast<float>(m[2]),
                    static_cast<float>(m[3]), static_cast<float>(m[4] - ox), static_cast<float>(m[5] - oy));
}

inline void replay(canvas_ity::canvas& c, const std::vector<double>& path, double ox, double oy) {
    c.begin_path();
    std::size_t i = 0;
    while (i < path.size()) {
        const int command = static_cast<int>(path[i]);
        const int n = path_arg_count(command);
        if (i + 1 + n + 6 > path.size()) break;
        const double* a = &path[i + 1];
        set_matrix(c, &path[i + 1 + n], ox, oy);
        auto f = [](double v) { return static_cast<float>(v); };
        switch (command) {
            case kMoveTo: c.move_to(f(a[0]), f(a[1])); break;
            case kLineTo: c.line_to(f(a[0]), f(a[1])); break;
            case kQuadTo: c.quadratic_curve_to(f(a[0]), f(a[1]), f(a[2]), f(a[3])); break;
            case kBezierTo: c.bezier_curve_to(f(a[0]), f(a[1]), f(a[2]), f(a[3]), f(a[4]), f(a[5])); break;
            case kArc: c.arc(f(a[0]), f(a[1]), f(std::abs(a[2])), f(a[3]), f(a[4]), a[5] != 0); break;
            case kArcTo: c.arc_to(f(a[0]), f(a[1]), f(a[2]), f(a[3]), f(std::abs(a[4]))); break;
            case kRect: c.rectangle(f(a[0]), f(a[1]), f(a[2]), f(a[3])); break;
            case kClose: c.close_path(); break;
            case kEllipse:
                // Unit circle under translate/rotate/scale; the outline keeps the caller's transform.
                c.translate(f(a[0]), f(a[1]));
                c.rotate(f(a[4]));
                c.scale(f(a[2] == 0 ? 1e-6 : a[2]), f(a[3] == 0 ? 1e-6 : a[3]));
                c.arc(0.0f, 0.0f, 1.0f, f(a[5]), f(a[6]), a[7] != 0);
                break;
            default: break;
        }
        i += 1 + n + 6;
    }
}

inline canvas_ity::composite_operation composite(const std::string& op, bool& supported) {
    using namespace canvas_ity;
    supported = true;
    if (op == "source-over") return source_over;
    if (op == "source-in") return source_in;
    if (op == "source-out") return source_out;
    if (op == "source-atop") return source_atop;
    if (op == "destination-over") return destination_over;
    if (op == "destination-in") return destination_in;
    if (op == "destination-out") return destination_out;
    if (op == "destination-atop") return destination_atop;
    if (op == "lighter") return lighter;
    if (op == "copy") return source_copy;
    if (op == "xor") return exclusive_or;
    supported = false; // multiply, screen, overlay...: drawn as source-over
    return source_over;
}

// Operations whose result also changes pixels outside the drawn shape.
inline bool unbounded(const std::string& op) {
    return op == "source-in" || op == "source-out" || op == "destination-in" ||
           op == "destination-atop" || op == "copy";
}

inline void set_style(canvas_ity::canvas& c, canvas_ity::brush_type brush, const Canvas2DStyle& s, double ox, double oy) {
    auto f = [](double v) { return static_cast<float>(v); };
    auto unit = [](std::uint8_t v) { return static_cast<float>(v) / 255.0f; };
    set_matrix(c, s.matrix, ox, oy);
    switch (s.kind) {
        case Canvas2DStyle::Color:
            c.set_color(brush, unit(s.color.r), unit(s.color.g), unit(s.color.b), unit(s.color.a));
            break;
        case Canvas2DStyle::Linear:
        case Canvas2DStyle::Radial:
            if (s.kind == Canvas2DStyle::Linear) c.set_linear_gradient(brush, f(s.x0), f(s.y0), f(s.x1), f(s.y1));
            else c.set_radial_gradient(brush, f(s.x0), f(s.y0), f(s.r0), f(s.x1), f(s.y1), f(s.r1));
            for (const auto& stop : s.stops)
                c.add_color_stop(brush, f(std::clamp(stop.first, 0.0, 1.0)), unit(stop.second.r), unit(stop.second.g),
                                 unit(stop.second.b), unit(stop.second.a));
            break;
        case Canvas2DStyle::Pattern:
            if (s.pattern) {
                c.set_pattern(brush, reinterpret_cast<const unsigned char*>(s.pattern->read_pixels()),
                              s.pattern->width, s.pattern->height, s.pattern->width * 4, s.repetition);
            }
            break;
    }
}

// One scratch canvas_ity surface, grown on demand, reused by every draw call.
struct Scratch {
    std::unique_ptr<canvas_ity::canvas> canvas;
    int w = 0, h = 0;
    canvas_ity::canvas& get(int need_w, int need_h) {
        if (!canvas || need_w != w || need_h != h) {
            canvas = std::make_unique<canvas_ity::canvas>(need_w, need_h);
            w = need_w;
            h = need_h;
        }
        return *canvas;
    }
};

inline Scratch& scratch() {
    static Scratch s;
    return s;
}

} // namespace canvas2d_impl

// Draws into `target`. `clips` are the clip paths in effect (each with its own transforms).
inline void canvas2d_draw(NativeBitmap& target, Canvas2DMode mode, const std::vector<double>& path,
                          const std::vector<std::vector<double>>& clips, const Canvas2DState& st,
                          const Canvas2DImage& image = {}) {
    using namespace canvas2d_impl;
    if (target.width <= 0 || target.height <= 0) return;

    Box box;
    if (mode == Canvas2DMode::Image) {
        double x = 0, y = 0;
        const double corners[4][2] = {{image.dx, image.dy}, {image.dx + image.dw, image.dy},
                                      {image.dx, image.dy + image.dh}, {image.dx + image.dw, image.dy + image.dh}};
        for (const auto& corner : corners) { apply(st.matrix, corner[0], corner[1], x, y); box.add(x, y); }
    } else {
        box = path_bounds(path);
    }
    if (mode == Canvas2DMode::Stroke) {
        double pad = st.line_width * max_scale(st.matrix) * 0.5;
        if (st.line_join == canvas_ity::miter) pad *= std::max(1.0, st.miter_limit);
        if (st.line_cap == canvas_ity::square) pad *= 1.5;
        box.x0 -= pad; box.y0 -= pad; box.x1 += pad; box.y1 += pad;
    }
    if (st.shadow_color.a > 0 && (st.shadow_blur > 0 || st.shadow_x != 0 || st.shadow_y != 0)) {
        const double b = st.shadow_blur * 2.0;
        box.x0 = std::min(box.x0, box.x0 + st.shadow_x - b);
        box.y0 = std::min(box.y0, box.y0 + st.shadow_y - b);
        box.x1 = std::max(box.x1, box.x1 + st.shadow_x + b);
        box.y1 = std::max(box.y1, box.y1 + st.shadow_y + b);
    }
    if (unbounded(st.composite)) box = {0, 0, static_cast<double>(target.width), static_cast<double>(target.height)};
    if (box.empty()) return;

    const int x0 = std::max(0, static_cast<int>(std::floor(box.x0)) - 2);
    const int y0 = std::max(0, static_cast<int>(std::floor(box.y0)) - 2);
    const int x1 = std::min(target.width, static_cast<int>(std::ceil(box.x1)) + 2);
    const int y1 = std::min(target.height, static_cast<int>(std::ceil(box.y1)) + 2);
    if (x1 <= x0 || y1 <= y0) return;
    const int w = x1 - x0, h = y1 - y0;

    canvas_ity::canvas& c = scratch().get(w, h);
    // The scratch surface is shared: restore() drops this call's clip mask and settings.
    struct SaveRestore {
        canvas_ity::canvas& c;
        explicit SaveRestore(canvas_ity::canvas& cv) : c(cv) { c.save(); }
        ~SaveRestore() { c.restore(); }
    } save_restore(c);
    std::uint32_t* pixels = target.write_pixels();
    auto* bytes = reinterpret_cast<unsigned char*>(pixels);
    const int stride = target.width * 4;
    c.global_composite_operation = canvas_ity::source_copy;
    c.set_transform(1, 0, 0, 1, 0, 0);
    c.put_image_data(bytes + y0 * stride + x0 * 4, w, h, stride, 0, 0);

    // Clip: every clip path in effect intersects the drawable region.
    for (const auto& clip : clips) {
        replay(c, clip, x0, y0);
        c.clip();
    }

    bool supported = true;
    c.global_composite_operation = composite(st.composite, supported);
    c.set_global_alpha(static_cast<float>(std::clamp(st.global_alpha, 0.0, 1.0)));
    c.set_shadow_color(st.shadow_color.r / 255.0f, st.shadow_color.g / 255.0f, st.shadow_color.b / 255.0f, st.shadow_color.a / 255.0f);
    c.set_shadow_blur(static_cast<float>(st.shadow_blur));
    c.shadow_offset_x = static_cast<float>(st.shadow_x);
    c.shadow_offset_y = static_cast<float>(st.shadow_y);

    switch (mode) {
        case Canvas2DMode::Fill:
            set_style(c, canvas_ity::fill_style, st.style, x0, y0);
            replay(c, path, x0, y0);
            set_matrix(c, st.matrix, x0, y0);
            c.fill();
            break;
        case Canvas2DMode::Stroke:
            set_style(c, canvas_ity::stroke_style, st.style, x0, y0);
            c.set_line_width(static_cast<float>(st.line_width));
            c.line_cap = st.line_cap;
            c.line_join = st.line_join;
            c.set_miter_limit(static_cast<float>(st.miter_limit));
            c.set_line_dash(st.dash.empty() ? nullptr : st.dash.data(), static_cast<int>(st.dash.size()));
            c.line_dash_offset = static_cast<float>(st.dash_offset);
            replay(c, path, x0, y0);
            set_matrix(c, st.matrix, x0, y0);
            c.stroke();
            break;
        case Canvas2DMode::Clear: {
            c.global_composite_operation = canvas_ity::destination_out;
            c.set_global_alpha(1.0f);
            c.set_shadow_color(0, 0, 0, 0);
            c.set_color(canvas_ity::fill_style, 0, 0, 0, 1);
            replay(c, path, x0, y0);
            set_matrix(c, st.matrix, x0, y0);
            c.fill();
            break;
        }
        case Canvas2DMode::Image:
            if (image.source && image.sw > 0 && image.sh > 0) {
                // canvas_ity draws whole images: clip to the destination and offset the full image.
                set_matrix(c, st.matrix, x0, y0);
                c.begin_path();
                c.rectangle(static_cast<float>(image.dx), static_cast<float>(image.dy),
                            static_cast<float>(image.dw), static_cast<float>(image.dh));
                c.clip();
                const double kx = image.dw / image.sw, ky = image.dh / image.sh;
                c.draw_image(reinterpret_cast<const unsigned char*>(image.source->read_pixels()),
                             image.source->width, image.source->height, image.source->width * 4,
                             static_cast<float>(image.dx - image.sx * kx), static_cast<float>(image.dy - image.sy * ky),
                             static_cast<float>(image.source->width * kx), static_cast<float>(image.source->height * ky));
            }
            break;
    }

    c.get_image_data(bytes + y0 * stride + x0 * 4, w, h, stride, 0, 0);
}

} // namespace mv_detail
} // namespace rpg2lx

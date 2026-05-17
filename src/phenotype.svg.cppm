module;
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
export module phenotype.svg;

import phenotype.svg_contract;
import phenotype.types;

export namespace phenotype::svg {

enum class PaintKind {
    None,
    Color,
    CurrentColor,
};

enum class StrokeLineCap {
    Butt,
    Round,
    Square,
};

enum class StrokeLineJoin {
    Miter,
    Round,
    Bevel,
};

inline auto stroke_line_cap_name(StrokeLineCap cap) noexcept
        -> std::string_view {
    switch (cap) {
    case StrokeLineCap::Butt:   return "butt";
    case StrokeLineCap::Round:  return "round";
    case StrokeLineCap::Square: return "square";
    }
    return "butt";
}

inline auto stroke_line_join_name(StrokeLineJoin join) noexcept
        -> std::string_view {
    switch (join) {
    case StrokeLineJoin::Miter: return "miter";
    case StrokeLineJoin::Round: return "round";
    case StrokeLineJoin::Bevel: return "bevel";
    }
    return "miter";
}

struct Paint {
    PaintKind kind = PaintKind::None;
    Color color = {0, 0, 0, 255};
    float opacity = 1.0f;
};

struct Style {
    Paint fill = {PaintKind::Color, {0, 0, 0, 255}, 1.0f};
    Paint stroke = {PaintKind::None, {0, 0, 0, 255}, 1.0f};
    Paint current_color = {PaintKind::CurrentColor, {0, 0, 0, 255}, 1.0f};
    float stroke_width = 1.0f;
    StrokeLineCap stroke_line_cap = StrokeLineCap::Butt;
    StrokeLineJoin stroke_line_join = StrokeLineJoin::Miter;
    float opacity = 1.0f;
};

struct Shape {
    PathBuilder path;
    Style style;
    std::string element;
};

struct Document {
    float view_min_x = 0.0f;
    float view_min_y = 0.0f;
    float view_width = 24.0f;
    float view_height = 24.0f;
    std::vector<Shape> shapes;
    std::vector<std::string> diagnostics;
    unsigned int unsupported_count = 0;

    bool empty() const noexcept { return shapes.empty(); }
    bool has_diagnostics() const noexcept { return !diagnostics.empty(); }
};

struct RenderOptions {
    Color current_color = {28, 28, 30, 255};
    bool preserve_aspect_ratio = true;
};

using DocumentSummary = phenotype::svg_contract::DocumentSummary;

inline auto subset_policy() noexcept -> std::string_view {
    return phenotype::svg_contract::subset_policy();
}

inline auto supported_elements() noexcept -> std::string_view {
    return phenotype::svg_contract::supported_elements();
}

inline auto supported_path_commands() noexcept -> std::string_view {
    return phenotype::svg_contract::supported_path_commands();
}

inline auto supported_style_attributes() noexcept -> std::string_view {
    return phenotype::svg_contract::supported_style_attributes();
}

inline auto unsupported_policy() noexcept -> std::string_view {
    return phenotype::svg_contract::unsupported_policy();
}

inline auto render_pipeline_policy() noexcept -> std::string_view {
    return phenotype::svg_contract::render_pipeline_policy();
}

inline auto summarize(Document const& doc) noexcept -> DocumentSummary {
    return {
        doc.view_min_x,
        doc.view_min_y,
        doc.view_width,
        doc.view_height,
        doc.shapes.size(),
        doc.diagnostics.size(),
        doc.unsupported_count,
        !doc.shapes.empty(),
        !doc.diagnostics.empty()};
}

} // namespace phenotype::svg

namespace phenotype::svg::detail {

struct Attribute {
    std::string name;
    std::string value;
};

struct Transform {
    float a = 1.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 1.0f;
    float e = 0.0f;
    float f = 0.0f;
};

inline bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

inline bool is_name_char(char c) noexcept {
    auto const u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '_' || c == '-' || c == ':' || c == '.';
}

inline auto lower_ascii(std::string_view in) -> std::string {
    std::string out;
    out.reserve(in.size());
    for (char c : in)
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

inline auto trim(std::string_view in) noexcept -> std::string_view {
    while (!in.empty() && is_space(in.front()))
        in.remove_prefix(1);
    while (!in.empty() && is_space(in.back()))
        in.remove_suffix(1);
    return in;
}

inline auto parse_stroke_line_cap(std::string_view raw)
        -> std::optional<StrokeLineCap> {
    auto const value = lower_ascii(trim(raw));
    if (value == "butt")
        return StrokeLineCap::Butt;
    if (value == "round")
        return StrokeLineCap::Round;
    if (value == "square")
        return StrokeLineCap::Square;
    return std::nullopt;
}

inline auto parse_stroke_line_join(std::string_view raw)
        -> std::optional<StrokeLineJoin> {
    auto const value = lower_ascii(trim(raw));
    if (value == "miter")
        return StrokeLineJoin::Miter;
    if (value == "round")
        return StrokeLineJoin::Round;
    if (value == "bevel")
        return StrokeLineJoin::Bevel;
    return std::nullopt;
}

inline auto parse_float_prefix(std::string_view in)
    -> std::optional<std::pair<float, std::size_t>> {
    in = trim(in);
    if (in.empty())
        return std::nullopt;
    std::string buf{in};
    char const* begin = buf.c_str();
    char* end = nullptr;
    float value = std::strtof(begin, &end);
    if (end == begin)
        return std::nullopt;
    return std::pair{value, static_cast<std::size_t>(end - begin)};
}

inline auto parse_float(std::string_view in) -> std::optional<float> {
    auto const value = trim(in);
    auto parsed = parse_float_prefix(value);
    if (!parsed)
        return std::nullopt;
    auto suffix = trim(value.substr(parsed->second));
    if (!suffix.empty() && suffix[0] != 'p')
        return std::nullopt;
    return parsed->first;
}

inline auto parse_number_list(std::string_view in, unsigned int limit = 0)
    -> std::vector<float> {
    std::vector<float> out;
    std::size_t i = 0;
    while (i < in.size()) {
        while (i < in.size() && (is_space(in[i]) || in[i] == ','))
            ++i;
        if (i >= in.size())
            break;
        auto parsed = parse_float_prefix(in.substr(i));
        if (!parsed) {
            ++i;
            continue;
        }
        out.push_back(parsed->first);
        i += parsed->second;
        if (limit != 0 && out.size() >= limit)
            break;
    }
    return out;
}

inline auto transform_identity() noexcept -> Transform {
    return {};
}

inline auto transform_multiply(Transform lhs, Transform rhs) noexcept
    -> Transform {
    return Transform{
        lhs.a * rhs.a + lhs.c * rhs.b,
        lhs.b * rhs.a + lhs.d * rhs.b,
        lhs.a * rhs.c + lhs.c * rhs.d,
        lhs.b * rhs.c + lhs.d * rhs.d,
        lhs.a * rhs.e + lhs.c * rhs.f + lhs.e,
        lhs.b * rhs.e + lhs.d * rhs.f + lhs.f,
    };
}

inline auto transform_translate(float tx, float ty) noexcept -> Transform {
    return Transform{1.0f, 0.0f, 0.0f, 1.0f, tx, ty};
}

inline auto transform_scale(float sx, float sy) noexcept -> Transform {
    return Transform{sx, 0.0f, 0.0f, sy, 0.0f, 0.0f};
}

inline auto transform_rotate(float degrees) noexcept -> Transform {
    constexpr float pi = 3.14159265358979323846f;
    auto const radians = degrees * pi / 180.0f;
    auto const s = std::sin(radians);
    auto const c = std::cos(radians);
    return Transform{c, s, -s, c, 0.0f, 0.0f};
}

inline auto parse_transform(std::string_view in, std::vector<std::string>& diagnostics)
    -> Transform {
    Transform out = transform_identity();
    std::size_t i = 0;
    while (i < in.size()) {
        while (i < in.size() && (is_space(in[i]) || in[i] == ','))
            ++i;
        if (i >= in.size())
            break;

        auto const name_start = i;
        while (i < in.size() && (std::isalpha(static_cast<unsigned char>(in[i]))
               || in[i] == '-'))
            ++i;
        auto name = lower_ascii(in.substr(name_start, i - name_start));
        while (i < in.size() && is_space(in[i]))
            ++i;
        if (name.empty() || i >= in.size() || in[i] != '(') {
            diagnostics.push_back("svg.transform: malformed transform list");
            break;
        }
        ++i;
        auto const values_start = i;
        int depth = 1;
        while (i < in.size() && depth > 0) {
            if (in[i] == '(')
                ++depth;
            else if (in[i] == ')')
                --depth;
            if (depth > 0)
                ++i;
        }
        if (depth != 0) {
            diagnostics.push_back("svg.transform: unterminated transform");
            break;
        }
        auto values = parse_number_list(in.substr(values_start, i - values_start));
        ++i;

        Transform op = transform_identity();
        bool supported = true;
        if (name == "translate") {
            if (values.empty()) {
                supported = false;
            } else {
                op = transform_translate(
                    values[0],
                    values.size() >= 2 ? values[1] : 0.0f);
            }
        } else if (name == "scale") {
            if (values.empty()) {
                supported = false;
            } else {
                op = transform_scale(
                    values[0],
                    values.size() >= 2 ? values[1] : values[0]);
            }
        } else if (name == "matrix") {
            if (values.size() < 6) {
                supported = false;
            } else {
                op = Transform{
                    values[0], values[1], values[2],
                    values[3], values[4], values[5]};
            }
        } else if (name == "rotate") {
            if (values.empty()) {
                supported = false;
            } else if (values.size() >= 3) {
                op = transform_multiply(
                    transform_multiply(
                        transform_translate(values[1], values[2]),
                        transform_rotate(values[0])),
                    transform_translate(-values[1], -values[2]));
            } else {
                op = transform_rotate(values[0]);
            }
        } else {
            diagnostics.push_back(
                std::string{"svg.transform: unsupported function "} + name);
            continue;
        }

        if (!supported) {
            diagnostics.push_back(
                std::string{"svg.transform: invalid argument count for "} + name);
            continue;
        }
        out = transform_multiply(out, op);
    }
    return out;
}

inline auto parse_attributes(std::string_view in) -> std::vector<Attribute> {
    std::vector<Attribute> out;
    std::size_t i = 0;
    while (i < in.size()) {
        while (i < in.size() && is_space(in[i]))
            ++i;
        if (i >= in.size() || in[i] == '/')
            break;

        auto const name_start = i;
        while (i < in.size() && is_name_char(in[i]))
            ++i;
        if (i == name_start)
            break;
        auto name = lower_ascii(in.substr(name_start, i - name_start));

        while (i < in.size() && is_space(in[i]))
            ++i;
        if (i >= in.size() || in[i] != '=') {
            out.push_back({std::move(name), {}});
            continue;
        }
        ++i;
        while (i < in.size() && is_space(in[i]))
            ++i;
        if (i >= in.size()) {
            out.push_back({std::move(name), {}});
            break;
        }

        std::string value;
        if (in[i] == '"' || in[i] == '\'') {
            char const quote = in[i++];
            auto const value_start = i;
            while (i < in.size() && in[i] != quote)
                ++i;
            value.assign(in.substr(value_start, i - value_start));
            if (i < in.size())
                ++i;
        } else {
            auto const value_start = i;
            while (i < in.size() && !is_space(in[i]) && in[i] != '/')
                ++i;
            value.assign(in.substr(value_start, i - value_start));
        }
        out.push_back({std::move(name), std::move(value)});
    }
    return out;
}

inline auto attr(std::vector<Attribute> const& attrs,
                 std::string_view name) noexcept -> std::optional<std::string_view> {
    for (auto const& a : attrs) {
        if (a.name == name)
            return std::string_view{a.value};
    }
    return std::nullopt;
}

inline auto hex_value(char c) noexcept -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline auto byte_from_hex(char hi, char lo) noexcept -> std::optional<unsigned char> {
    auto const h = hex_value(hi);
    auto const l = hex_value(lo);
    if (h < 0 || l < 0)
        return std::nullopt;
    return static_cast<unsigned char>((h << 4) | l);
}

inline auto parse_color(std::string_view in) -> std::optional<Paint> {
    in = trim(in);
    auto const lower = lower_ascii(in);
    if (lower == "none")
        return Paint{PaintKind::None, {0, 0, 0, 0}, 0.0f};
    if (lower == "currentcolor")
        return Paint{PaintKind::CurrentColor, {0, 0, 0, 255}, 1.0f};
    if (lower == "black")
        return Paint{PaintKind::Color, {0, 0, 0, 255}, 1.0f};
    if (lower == "white")
        return Paint{PaintKind::Color, {255, 255, 255, 255}, 1.0f};
    if (lower == "transparent")
        return Paint{PaintKind::Color, {0, 0, 0, 0}, 0.0f};

    if (in.size() == 4 && in[0] == '#') {
        auto r = hex_value(in[1]);
        auto g = hex_value(in[2]);
        auto b = hex_value(in[3]);
        if (r >= 0 && g >= 0 && b >= 0) {
            return Paint{
                PaintKind::Color,
                {
                    static_cast<unsigned char>((r << 4) | r),
                    static_cast<unsigned char>((g << 4) | g),
                    static_cast<unsigned char>((b << 4) | b),
                    255,
                },
                1.0f,
            };
        }
    }
    if (in.size() == 5 && in[0] == '#') {
        auto r = hex_value(in[1]);
        auto g = hex_value(in[2]);
        auto b = hex_value(in[3]);
        auto a = hex_value(in[4]);
        if (r >= 0 && g >= 0 && b >= 0 && a >= 0) {
            return Paint{
                PaintKind::Color,
                {
                    static_cast<unsigned char>((r << 4) | r),
                    static_cast<unsigned char>((g << 4) | g),
                    static_cast<unsigned char>((b << 4) | b),
                    static_cast<unsigned char>((a << 4) | a),
                },
                static_cast<float>((a << 4) | a) / 255.0f,
            };
        }
    }
    if ((in.size() == 7 || in.size() == 9) && in[0] == '#') {
        auto r = byte_from_hex(in[1], in[2]);
        auto g = byte_from_hex(in[3], in[4]);
        auto b = byte_from_hex(in[5], in[6]);
        auto a = in.size() == 9 ? byte_from_hex(in[7], in[8])
                                : std::optional<unsigned char>{255};
        if (r && g && b && a) {
            return Paint{
                PaintKind::Color,
                {*r, *g, *b, *a},
                static_cast<float>(*a) / 255.0f,
            };
        }
    }

    if (lower.starts_with("rgb(") || lower.starts_with("rgba(")) {
        auto const open = in.find('(');
        auto const close = in.rfind(')');
        if (open != std::string_view::npos && close != std::string_view::npos
            && close > open) {
            auto values = parse_number_list(in.substr(open + 1, close - open - 1), 4);
            if (values.size() >= 3) {
                auto channel = [](float v) {
                    return static_cast<unsigned char>(
                        std::clamp(v, 0.0f, 255.0f) + 0.5f);
                };
                auto alpha = values.size() >= 4
                    ? std::clamp(values[3], 0.0f, 1.0f)
                    : 1.0f;
                return Paint{
                    PaintKind::Color,
                    {
                        channel(values[0]),
                        channel(values[1]),
                        channel(values[2]),
                        static_cast<unsigned char>(alpha * 255.0f + 0.5f),
                    },
                    alpha,
                };
            }
        }
    }

    return std::nullopt;
}

inline auto with_opacity(Paint paint, float opacity) noexcept -> Paint {
    paint.opacity = std::clamp(opacity, 0.0f, 1.0f);
    return paint;
}

inline void apply_property(Style& style,
                           std::string_view raw_name,
                           std::string_view raw_value) {
    auto const name = lower_ascii(trim(raw_name));
    auto const value = trim(raw_value);
    if (name == "fill") {
        if (auto paint = parse_color(value))
            style.fill = *paint;
    } else if (name == "stroke") {
        if (auto paint = parse_color(value))
            style.stroke = *paint;
    } else if (name == "color") {
        if (auto paint = parse_color(value))
            style.current_color = *paint;
    } else if (name == "stroke-width") {
        if (auto width = parse_float(value))
            style.stroke_width = std::max(0.0f, *width);
    } else if (name == "stroke-linecap") {
        if (auto cap = parse_stroke_line_cap(value))
            style.stroke_line_cap = *cap;
    } else if (name == "stroke-linejoin") {
        if (auto join = parse_stroke_line_join(value))
            style.stroke_line_join = *join;
    } else if (name == "opacity") {
        if (auto opacity = parse_float(value))
            style.opacity *= std::clamp(*opacity, 0.0f, 1.0f);
    } else if (name == "fill-opacity") {
        if (auto opacity = parse_float(value))
            style.fill = with_opacity(style.fill, *opacity);
    } else if (name == "stroke-opacity") {
        if (auto opacity = parse_float(value))
            style.stroke = with_opacity(style.stroke, *opacity);
    }
}

inline auto merged_style(Style base,
                         std::vector<Attribute> const& attrs) -> Style {
    for (auto const& a : attrs) {
        if (a.name != "style")
            apply_property(base, a.name, a.value);
    }
    if (auto style_attr = attr(attrs, "style")) {
        std::size_t start = 0;
        while (start < style_attr->size()) {
            auto const end = style_attr->find(';', start);
            auto const item = end == std::string_view::npos
                ? style_attr->substr(start)
                : style_attr->substr(start, end - start);
            auto const colon = item.find(':');
            if (colon != std::string_view::npos)
                apply_property(base, item.substr(0, colon), item.substr(colon + 1));
            if (end == std::string_view::npos)
                break;
            start = end + 1;
        }
    }
    return base;
}

inline auto read_f32(unsigned int bits) noexcept -> float {
    float value = 0.0f;
    std::memcpy(&value, &bits, 4);
    return value;
}

inline auto transformed_path(PathBuilder const& path,
                             Transform transform) -> PathBuilder {
    PathBuilder out;
    unsigned int i = 0;
    auto xy = [&](float& x, float& y) {
        auto const px = read_f32(path.verbs[i++]);
        auto const py = read_f32(path.verbs[i++]);
        x = transform.a * px + transform.c * py + transform.e;
        y = transform.b * px + transform.d * py + transform.f;
    };

    while (i < path.verbs.size()) {
        auto const tag = static_cast<PathVerb>(path.verbs[i++]);
        switch (tag) {
        case PathVerb::MoveTo: {
            float x, y;
            xy(x, y);
            out.move_to(x, y);
            break;
        }
        case PathVerb::LineTo: {
            float x, y;
            xy(x, y);
            out.line_to(x, y);
            break;
        }
        case PathVerb::QuadTo: {
            float cx, cy, x, y;
            xy(cx, cy);
            xy(x, y);
            out.quad_to(cx, cy, x, y);
            break;
        }
        case PathVerb::CubicTo: {
            float c1x, c1y, c2x, c2y, x, y;
            xy(c1x, c1y);
            xy(c2x, c2y);
            xy(x, y);
            out.cubic_to(c1x, c1y, c2x, c2y, x, y);
            break;
        }
        case PathVerb::ArcTo: {
            float cx, cy;
            xy(cx, cy);
            auto const radius = read_f32(path.verbs[i++])
                * ((std::hypot(transform.a, transform.b)
                    + std::hypot(transform.c, transform.d)) * 0.5f);
            auto const start = read_f32(path.verbs[i++]);
            auto const end = read_f32(path.verbs[i++]);
            out.arc_to(cx, cy, radius, start, end);
            break;
        }
        case PathVerb::Close:
            out.close();
            break;
        default:
            i = static_cast<unsigned int>(path.verbs.size());
            break;
        }
    }
    return out;
}

inline auto resolve_current_color(Style const& style,
                                  RenderOptions const& options) noexcept -> Color {
    if (style.current_color.kind == PaintKind::Color)
        return style.current_color.color;
    return options.current_color;
}

inline auto resolve_paint(Paint paint,
                          Style const& style,
                          RenderOptions const& options) noexcept
    -> std::optional<Color> {
    if (paint.kind == PaintKind::None)
        return std::nullopt;

    auto color = paint.kind == PaintKind::CurrentColor
        ? resolve_current_color(style, options)
        : paint.color;
    auto const alpha = std::clamp(
        (static_cast<float>(color.a) / 255.0f) * style.opacity * paint.opacity,
        0.0f,
        1.0f);
    color.a = static_cast<unsigned char>(alpha * 255.0f + 0.5f);
    if (color.a == 0)
        return std::nullopt;
    return color;
}

inline void add_diagnostic(Document& doc, std::string message) {
    doc.diagnostics.push_back(std::move(message));
}

inline void append_rect(PathBuilder& path,
                        float x,
                        float y,
                        float w,
                        float h,
                        float rx,
                        float ry) {
    if (w < 0.0f) {
        x += w;
        w = -w;
    }
    if (h < 0.0f) {
        y += h;
        h = -h;
    }
    rx = std::clamp(rx, 0.0f, w * 0.5f);
    ry = std::clamp(ry, 0.0f, h * 0.5f);
    if (rx == 0.0f && ry == 0.0f) {
        path.move_to(x, y);
        path.line_to(x + w, y);
        path.line_to(x + w, y + h);
        path.line_to(x, y + h);
        path.close();
        return;
    }

    float const k = 0.552284749831f;
    path.move_to(x + rx, y);
    path.line_to(x + w - rx, y);
    path.cubic_to(x + w - rx + rx * k, y,
                  x + w, y + ry - ry * k,
                  x + w, y + ry);
    path.line_to(x + w, y + h - ry);
    path.cubic_to(x + w, y + h - ry + ry * k,
                  x + w - rx + rx * k, y + h,
                  x + w - rx, y + h);
    path.line_to(x + rx, y + h);
    path.cubic_to(x + rx - rx * k, y + h,
                  x, y + h - ry + ry * k,
                  x, y + h - ry);
    path.line_to(x, y + ry);
    path.cubic_to(x, y + ry - ry * k,
                  x + rx - rx * k, y,
                  x + rx, y);
    path.close();
}

inline void append_ellipse(PathBuilder& path,
                           float cx,
                           float cy,
                           float rx,
                           float ry) {
    rx = std::abs(rx);
    ry = std::abs(ry);
    if (rx == 0.0f || ry == 0.0f)
        return;
    if (std::fabs(rx - ry) <= 0.0001f) {
        constexpr float pi = 3.14159265358979323846f;
        path.move_to(cx + rx, cy);
        path.arc_to(cx, cy, rx, 0.0f, 0.5f * pi);
        path.arc_to(cx, cy, rx, 0.5f * pi, pi);
        path.arc_to(cx, cy, rx, pi, 1.5f * pi);
        path.arc_to(cx, cy, rx, 1.5f * pi, 2.0f * pi);
        path.close();
        return;
    }
    float const k = 0.552284749831f;
    path.move_to(cx + rx, cy);
    path.cubic_to(cx + rx, cy + ry * k,
                  cx + rx * k, cy + ry,
                  cx, cy + ry);
    path.cubic_to(cx - rx * k, cy + ry,
                  cx - rx, cy + ry * k,
                  cx - rx, cy);
    path.cubic_to(cx - rx, cy - ry * k,
                  cx - rx * k, cy - ry,
                  cx, cy - ry);
    path.cubic_to(cx + rx * k, cy - ry,
                  cx + rx, cy - ry * k,
                  cx + rx, cy);
    path.close();
}

inline auto parse_points(std::string_view in) -> std::vector<float> {
    return parse_number_list(in);
}

struct PathCursor {
    float x = 0.0f;
    float y = 0.0f;
    float sub_x = 0.0f;
    float sub_y = 0.0f;
    float last_cubic_cx = 0.0f;
    float last_cubic_cy = 0.0f;
    float last_quad_cx = 0.0f;
    float last_quad_cy = 0.0f;
    bool has_cubic_control = false;
    bool has_quad_control = false;
};

inline bool path_skip(std::string_view d, std::size_t& i) noexcept {
    while (i < d.size() && (is_space(d[i]) || d[i] == ','))
        ++i;
    return i < d.size();
}

inline auto path_number(std::string_view d, std::size_t& i) noexcept
    -> std::optional<float> {
    path_skip(d, i);
    if (i >= d.size())
        return std::nullopt;
    auto parsed = parse_float_prefix(d.substr(i));
    if (!parsed)
        return std::nullopt;
    i += parsed->second;
    return parsed->first;
}

inline bool path_has_number(std::string_view d, std::size_t i) noexcept {
    path_skip(d, i);
    if (i >= d.size())
        return false;
    auto const c = d[i];
    return c == '-' || c == '+' || c == '.' || (c >= '0' && c <= '9');
}

inline void reset_path_controls(PathCursor& cursor) noexcept {
    cursor.has_cubic_control = false;
    cursor.has_quad_control = false;
}

inline float vector_angle(float ux, float uy, float vx, float vy) noexcept {
    auto const cross = ux * vy - uy * vx;
    auto const dot = ux * vx + uy * vy;
    return std::atan2(cross, dot);
}

inline void append_svg_arc(PathBuilder& path,
                           float x1,
                           float y1,
                           float rx,
                           float ry,
                           float x_axis_rotation_degrees,
                           bool large_arc,
                           bool sweep,
                           float x2,
                           float y2,
                           bool preserve_native_circular_arc) {
    constexpr float pi = 3.14159265358979323846f;
    if (std::abs(x1 - x2) <= 0.0001f
        && std::abs(y1 - y2) <= 0.0001f)
        return;

    rx = std::abs(rx);
    ry = std::abs(ry);
    if (rx <= 0.0001f || ry <= 0.0001f) {
        path.line_to(x2, y2);
        return;
    }

    auto const phi = x_axis_rotation_degrees * pi / 180.0f;
    auto const cos_phi = std::cos(phi);
    auto const sin_phi = std::sin(phi);
    auto const dx2 = (x1 - x2) * 0.5f;
    auto const dy2 = (y1 - y2) * 0.5f;
    auto const x1p = cos_phi * dx2 + sin_phi * dy2;
    auto const y1p = -sin_phi * dx2 + cos_phi * dy2;

    auto const rx2_initial = rx * rx;
    auto const ry2_initial = ry * ry;
    auto const radii_check =
        (x1p * x1p) / rx2_initial + (y1p * y1p) / ry2_initial;
    if (radii_check > 1.0f) {
        auto const scale = std::sqrt(radii_check);
        rx *= scale;
        ry *= scale;
    }

    auto const rx2 = rx * rx;
    auto const ry2 = ry * ry;
    auto const x1p2 = x1p * x1p;
    auto const y1p2 = y1p * y1p;
    auto const denom = rx2 * y1p2 + ry2 * x1p2;
    if (denom <= 0.000001f) {
        path.line_to(x2, y2);
        return;
    }

    auto const sign = large_arc == sweep ? -1.0f : 1.0f;
    auto const numerator = std::max(
        0.0f,
        (rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2) / denom);
    auto const coef = sign * std::sqrt(numerator);
    auto const cxp = coef * (rx * y1p / ry);
    auto const cyp = coef * (-ry * x1p / rx);
    auto const cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) * 0.5f;
    auto const cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) * 0.5f;

    auto const ux = (x1p - cxp) / rx;
    auto const uy = (y1p - cyp) / ry;
    auto const vx = (-x1p - cxp) / rx;
    auto const vy = (-y1p - cyp) / ry;
    auto theta = vector_angle(1.0f, 0.0f, ux, uy);
    auto delta = vector_angle(ux, uy, vx, vy);
    if (!sweep && delta > 0.0f)
        delta -= 2.0f * pi;
    else if (sweep && delta < 0.0f)
        delta += 2.0f * pi;

    auto const radius_epsilon = 0.0001f * std::max(1.0f, std::max(rx, ry));
    if (preserve_native_circular_arc && std::abs(rx - ry) <= radius_epsilon) {
        auto start = theta;
        auto end = theta + delta;
        if (delta < 0.0f)
            std::swap(start, end);
        path.arc_to(cx, cy, rx, start, end);
        return;
    }

    auto segment_count = static_cast<unsigned int>(
        std::ceil(std::abs(delta) / (0.5f * pi)));
    segment_count = std::max(1u, std::min(8u, segment_count));
    auto const step = delta / static_cast<float>(segment_count);

    auto map_point = [&](float ex, float ey) {
        return std::pair<float, float>{
            cx + rx * cos_phi * ex - ry * sin_phi * ey,
            cy + rx * sin_phi * ex + ry * cos_phi * ey,
        };
    };

    for (unsigned int i = 0; i < segment_count; ++i) {
        auto const next = theta + step;
        auto const alpha = (4.0f / 3.0f) * std::tan((next - theta) * 0.25f);
        auto const cos_t = std::cos(theta);
        auto const sin_t = std::sin(theta);
        auto const cos_n = std::cos(next);
        auto const sin_n = std::sin(next);
        auto [c1x, c1y] = map_point(
            cos_t - alpha * sin_t,
            sin_t + alpha * cos_t);
        auto [c2x, c2y] = map_point(
            cos_n + alpha * sin_n,
            sin_n - alpha * cos_n);
        auto [px, py] = map_point(cos_n, sin_n);
        path.cubic_to(c1x, c1y, c2x, c2y, px, py);
        theta = next;
    }
}

inline bool path_data_is_single_move_arc(std::string_view d) noexcept {
    std::size_t i = 0;
    path_skip(d, i);
    if (i >= d.size())
        return false;
    auto command = d[i++];
    if (std::toupper(static_cast<unsigned char>(command)) != 'M')
        return false;
    if (!path_number(d, i) || !path_number(d, i))
        return false;
    path_skip(d, i);
    if (i >= d.size())
        return false;
    command = d[i++];
    if (std::toupper(static_cast<unsigned char>(command)) != 'A')
        return false;
    for (int n = 0; n < 7; ++n) {
        if (!path_number(d, i))
            return false;
    }
    path_skip(d, i);
    return i >= d.size();
}

inline auto parse_path_data(std::string_view d, Document& doc) -> PathBuilder {
    PathBuilder path;
    PathCursor cursor;
    char command = 0;
    std::size_t i = 0;
    bool const preserve_native_circular_arc = path_data_is_single_move_arc(d);

    while (path_skip(d, i)) {
        if (std::isalpha(static_cast<unsigned char>(d[i]))) {
            command = d[i++];
        } else if (command == 0) {
            add_diagnostic(doc, "svg.path: missing initial command");
            break;
        }

        auto const relative = std::islower(static_cast<unsigned char>(command)) != 0;
        auto const upper = static_cast<char>(
            std::toupper(static_cast<unsigned char>(command)));

        if (upper == 'Z') {
            path.close();
            cursor.x = cursor.sub_x;
            cursor.y = cursor.sub_y;
            reset_path_controls(cursor);
            continue;
        }

        if (upper == 'M') {
            bool first = true;
            while (path_has_number(d, i)) {
                auto x = path_number(d, i);
                auto y = path_number(d, i);
                if (!x || !y)
                    break;
                float px = *x + (relative ? cursor.x : 0.0f);
                float py = *y + (relative ? cursor.y : 0.0f);
                if (first) {
                    path.move_to(px, py);
                    cursor.sub_x = px;
                    cursor.sub_y = py;
                    first = false;
                } else {
                    path.line_to(px, py);
                }
                cursor.x = px;
                cursor.y = py;
                reset_path_controls(cursor);
            }
            continue;
        }

        if (upper == 'L') {
            while (path_has_number(d, i)) {
                auto x = path_number(d, i);
                auto y = path_number(d, i);
                if (!x || !y)
                    break;
                cursor.x = *x + (relative ? cursor.x : 0.0f);
                cursor.y = *y + (relative ? cursor.y : 0.0f);
                path.line_to(cursor.x, cursor.y);
                reset_path_controls(cursor);
            }
            continue;
        }

        if (upper == 'H') {
            while (path_has_number(d, i)) {
                auto x = path_number(d, i);
                if (!x)
                    break;
                cursor.x = *x + (relative ? cursor.x : 0.0f);
                path.line_to(cursor.x, cursor.y);
                reset_path_controls(cursor);
            }
            continue;
        }

        if (upper == 'V') {
            while (path_has_number(d, i)) {
                auto y = path_number(d, i);
                if (!y)
                    break;
                cursor.y = *y + (relative ? cursor.y : 0.0f);
                path.line_to(cursor.x, cursor.y);
                reset_path_controls(cursor);
            }
            continue;
        }

        if (upper == 'Q') {
            while (path_has_number(d, i)) {
                auto cx = path_number(d, i);
                auto cy = path_number(d, i);
                auto x = path_number(d, i);
                auto y = path_number(d, i);
                if (!cx || !cy || !x || !y)
                    break;
                float pcx = *cx + (relative ? cursor.x : 0.0f);
                float pcy = *cy + (relative ? cursor.y : 0.0f);
                cursor.x = *x + (relative ? cursor.x : 0.0f);
                cursor.y = *y + (relative ? cursor.y : 0.0f);
                path.quad_to(pcx, pcy, cursor.x, cursor.y);
                cursor.last_quad_cx = pcx;
                cursor.last_quad_cy = pcy;
                cursor.has_quad_control = true;
                cursor.has_cubic_control = false;
            }
            continue;
        }

        if (upper == 'T') {
            while (path_has_number(d, i)) {
                auto x = path_number(d, i);
                auto y = path_number(d, i);
                if (!x || !y)
                    break;
                float pcx = cursor.has_quad_control
                    ? 2.0f * cursor.x - cursor.last_quad_cx
                    : cursor.x;
                float pcy = cursor.has_quad_control
                    ? 2.0f * cursor.y - cursor.last_quad_cy
                    : cursor.y;
                cursor.x = *x + (relative ? cursor.x : 0.0f);
                cursor.y = *y + (relative ? cursor.y : 0.0f);
                path.quad_to(pcx, pcy, cursor.x, cursor.y);
                cursor.last_quad_cx = pcx;
                cursor.last_quad_cy = pcy;
                cursor.has_quad_control = true;
                cursor.has_cubic_control = false;
            }
            continue;
        }

        if (upper == 'C') {
            while (path_has_number(d, i)) {
                auto c1x = path_number(d, i);
                auto c1y = path_number(d, i);
                auto c2x = path_number(d, i);
                auto c2y = path_number(d, i);
                auto x = path_number(d, i);
                auto y = path_number(d, i);
                if (!c1x || !c1y || !c2x || !c2y || !x || !y)
                    break;
                float pc1x = *c1x + (relative ? cursor.x : 0.0f);
                float pc1y = *c1y + (relative ? cursor.y : 0.0f);
                float pc2x = *c2x + (relative ? cursor.x : 0.0f);
                float pc2y = *c2y + (relative ? cursor.y : 0.0f);
                cursor.x = *x + (relative ? cursor.x : 0.0f);
                cursor.y = *y + (relative ? cursor.y : 0.0f);
                path.cubic_to(pc1x, pc1y, pc2x, pc2y, cursor.x, cursor.y);
                cursor.last_cubic_cx = pc2x;
                cursor.last_cubic_cy = pc2y;
                cursor.has_cubic_control = true;
                cursor.has_quad_control = false;
            }
            continue;
        }

        if (upper == 'S') {
            while (path_has_number(d, i)) {
                auto c2x = path_number(d, i);
                auto c2y = path_number(d, i);
                auto x = path_number(d, i);
                auto y = path_number(d, i);
                if (!c2x || !c2y || !x || !y)
                    break;
                float pc1x = cursor.has_cubic_control
                    ? 2.0f * cursor.x - cursor.last_cubic_cx
                    : cursor.x;
                float pc1y = cursor.has_cubic_control
                    ? 2.0f * cursor.y - cursor.last_cubic_cy
                    : cursor.y;
                float pc2x = *c2x + (relative ? cursor.x : 0.0f);
                float pc2y = *c2y + (relative ? cursor.y : 0.0f);
                cursor.x = *x + (relative ? cursor.x : 0.0f);
                cursor.y = *y + (relative ? cursor.y : 0.0f);
                path.cubic_to(pc1x, pc1y, pc2x, pc2y, cursor.x, cursor.y);
                cursor.last_cubic_cx = pc2x;
                cursor.last_cubic_cy = pc2y;
                cursor.has_cubic_control = true;
                cursor.has_quad_control = false;
            }
            continue;
        }

        if (upper == 'A') {
            while (path_has_number(d, i)) {
                auto rx = path_number(d, i);
                auto ry = path_number(d, i);
                auto rotation = path_number(d, i);
                auto large_arc = path_number(d, i);
                auto sweep = path_number(d, i);
                auto x = path_number(d, i);
                auto y = path_number(d, i);
                if (!rx || !ry || !rotation || !large_arc || !sweep || !x || !y)
                    break;
                auto const end_x = *x + (relative ? cursor.x : 0.0f);
                auto const end_y = *y + (relative ? cursor.y : 0.0f);
                append_svg_arc(
                    path,
                    cursor.x,
                    cursor.y,
                    *rx,
                    *ry,
                    *rotation,
                    *large_arc != 0.0f,
                    *sweep != 0.0f,
                    end_x,
                    end_y,
                    preserve_native_circular_arc);
                cursor.x = end_x;
                cursor.y = end_y;
                reset_path_controls(cursor);
            }
            continue;
        }

        ++doc.unsupported_count;
        add_diagnostic(doc, std::string{"svg.path: unsupported command "} + upper);
        while (path_has_number(d, i)) {
            auto ignored = path_number(d, i);
            (void)ignored;
        }
    }

    return path;
}

inline auto shape_from_tag(std::string const& name,
                           std::vector<Attribute> const& attrs,
                           Style const& base,
                           Transform transform,
                           Document& doc) -> std::optional<Shape> {
    auto style = merged_style(base, attrs);
    Shape shape;
    shape.style = style;
    shape.element = name;

    if (name == "path") {
        auto d = attr(attrs, "d");
        if (!d) {
            add_diagnostic(doc, "svg.path: missing d attribute");
            return std::nullopt;
        }
        shape.path = parse_path_data(*d, doc);
    } else if (name == "rect") {
        auto const x = parse_float(attr(attrs, "x").value_or("0")).value_or(0.0f);
        auto const y = parse_float(attr(attrs, "y").value_or("0")).value_or(0.0f);
        auto const w = parse_float(attr(attrs, "width").value_or("0")).value_or(0.0f);
        auto const h = parse_float(attr(attrs, "height").value_or("0")).value_or(0.0f);
        auto rx = parse_float(attr(attrs, "rx").value_or("-1")).value_or(-1.0f);
        auto ry = parse_float(attr(attrs, "ry").value_or("-1")).value_or(-1.0f);
        if (rx < 0.0f && ry >= 0.0f) rx = ry;
        if (ry < 0.0f && rx >= 0.0f) ry = rx;
        if (rx < 0.0f) rx = 0.0f;
        if (ry < 0.0f) ry = 0.0f;
        append_rect(shape.path, x, y, w, h, rx, ry);
    } else if (name == "circle") {
        auto const cx = parse_float(attr(attrs, "cx").value_or("0")).value_or(0.0f);
        auto const cy = parse_float(attr(attrs, "cy").value_or("0")).value_or(0.0f);
        auto const r = parse_float(attr(attrs, "r").value_or("0")).value_or(0.0f);
        append_ellipse(shape.path, cx, cy, r, r);
    } else if (name == "ellipse") {
        auto const cx = parse_float(attr(attrs, "cx").value_or("0")).value_or(0.0f);
        auto const cy = parse_float(attr(attrs, "cy").value_or("0")).value_or(0.0f);
        auto const rx = parse_float(attr(attrs, "rx").value_or("0")).value_or(0.0f);
        auto const ry = parse_float(attr(attrs, "ry").value_or("0")).value_or(0.0f);
        append_ellipse(shape.path, cx, cy, rx, ry);
    } else if (name == "line") {
        auto const x1 = parse_float(attr(attrs, "x1").value_or("0")).value_or(0.0f);
        auto const y1 = parse_float(attr(attrs, "y1").value_or("0")).value_or(0.0f);
        auto const x2 = parse_float(attr(attrs, "x2").value_or("0")).value_or(0.0f);
        auto const y2 = parse_float(attr(attrs, "y2").value_or("0")).value_or(0.0f);
        shape.style.fill.kind = PaintKind::None;
        shape.path.move_to(x1, y1);
        shape.path.line_to(x2, y2);
    } else if (name == "polyline" || name == "polygon") {
        auto points = attr(attrs, "points");
        if (!points) {
            add_diagnostic(doc, "svg.poly: missing points attribute");
            return std::nullopt;
        }
        auto values = parse_points(*points);
        if (values.size() < 4)
            return std::nullopt;
        shape.path.move_to(values[0], values[1]);
        for (std::size_t i = 2; i + 1 < values.size(); i += 2)
            shape.path.line_to(values[i], values[i + 1]);
        if (name == "polygon")
            shape.path.close();
    } else {
        return std::nullopt;
    }

    if (shape.path.empty())
        return std::nullopt;
    if (auto transform_attr = attr(attrs, "transform"))
        transform = transform_multiply(
            transform,
            parse_transform(*transform_attr, doc.diagnostics));
    auto const local_stroke_scale =
        (std::hypot(transform.a, transform.b)
         + std::hypot(transform.c, transform.d)) * 0.5f;
    shape.style.stroke_width *= local_stroke_scale;
    shape.path = transformed_path(shape.path, transform);
    return shape;
}

inline void apply_svg_metrics(Document& doc,
                              std::vector<Attribute> const& attrs) {
    if (auto vb = attr(attrs, "viewbox")) {
        auto values = parse_number_list(*vb, 4);
        if (values.size() == 4 && values[2] > 0.0f && values[3] > 0.0f) {
            doc.view_min_x = values[0];
            doc.view_min_y = values[1];
            doc.view_width = values[2];
            doc.view_height = values[3];
            return;
        }
    }

    auto const width = parse_float(attr(attrs, "width").value_or("")).value_or(0.0f);
    auto const height = parse_float(attr(attrs, "height").value_or("")).value_or(0.0f);
    if (width > 0.0f && height > 0.0f) {
        doc.view_min_x = 0.0f;
        doc.view_min_y = 0.0f;
        doc.view_width = width;
        doc.view_height = height;
    }
}

inline void hash_mix(std::uint64_t& state, std::uint64_t value) noexcept {
    state ^= value;
    state *= 1099511628211ull;
}

inline auto float_bits(float value) noexcept -> std::uint32_t {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

inline void hash_float(std::uint64_t& state, float value) noexcept {
    hash_mix(state, float_bits(value));
}

inline void hash_string(std::uint64_t& state, std::string_view value) noexcept {
    hash_mix(state, value.size());
    for (unsigned char ch : value)
        hash_mix(state, ch);
}

inline void hash_color(std::uint64_t& state, Color color) noexcept {
    hash_mix(state, color.packed());
}

inline void hash_paint(std::uint64_t& state, Paint const& paint) noexcept {
    hash_mix(state, static_cast<std::uint64_t>(paint.kind));
    hash_color(state, paint.color);
    hash_float(state, paint.opacity);
}

inline void hash_style(std::uint64_t& state, Style const& style) noexcept {
    hash_paint(state, style.fill);
    hash_paint(state, style.stroke);
    hash_paint(state, style.current_color);
    hash_float(state, style.stroke_width);
    hash_mix(state, static_cast<std::uint64_t>(style.stroke_line_cap));
    hash_mix(state, static_cast<std::uint64_t>(style.stroke_line_join));
    hash_float(state, style.opacity);
}

inline void hash_path(std::uint64_t& state, PathBuilder const& path) noexcept {
    hash_mix(state, path.verb_count);
    hash_mix(state, path.verbs.size());
    for (auto word : path.verbs)
        hash_mix(state, word);
}

} // namespace phenotype::svg::detail

export namespace phenotype::svg {

auto parse(std::string_view source) -> Document {
    Document doc;
    std::vector<Style> style_stack;
    std::vector<detail::Transform> transform_stack;
    std::vector<std::string> styled_elements;
    style_stack.push_back(Style{});
    transform_stack.push_back(detail::transform_identity());

    std::size_t cursor = 0;
    while (true) {
        auto const open = source.find('<', cursor);
        if (open == std::string_view::npos)
            break;
        auto const close = source.find('>', open + 1);
        if (close == std::string_view::npos) {
            detail::add_diagnostic(doc, "svg.xml: unterminated tag");
            break;
        }

        auto inside = detail::trim(source.substr(open + 1, close - open - 1));
        cursor = close + 1;
        if (inside.empty())
            continue;
        if (inside[0] == '!' || inside[0] == '?')
            continue;

        bool const closing = inside[0] == '/';
        if (closing) {
            inside.remove_prefix(1);
            inside = detail::trim(inside);
            auto const name_end = inside.find_first_of(" \t\r\n");
            auto const name = detail::lower_ascii(
                name_end == std::string_view::npos
                    ? inside
                    : inside.substr(0, name_end));
            if (!styled_elements.empty()
                && (name == styled_elements.back() || name == "g" || name == "svg")) {
                styled_elements.pop_back();
                if (style_stack.size() > 1) {
                    style_stack.pop_back();
                    transform_stack.pop_back();
                }
            }
            continue;
        }

        bool self_closing = false;
        if (!inside.empty() && inside.back() == '/') {
            self_closing = true;
            inside.remove_suffix(1);
            inside = detail::trim(inside);
        }

        std::size_t i = 0;
        while (i < inside.size() && detail::is_name_char(inside[i]))
            ++i;
        if (i == 0)
            continue;
        auto const name = detail::lower_ascii(inside.substr(0, i));
        auto attrs = detail::parse_attributes(inside.substr(i));

        if (name == "svg") {
            detail::apply_svg_metrics(doc, attrs);
            auto merged = detail::merged_style(style_stack.back(), attrs);
            auto transform = transform_stack.back();
            if (auto transform_attr = detail::attr(attrs, "transform"))
                transform = detail::transform_multiply(
                    transform,
                    detail::parse_transform(*transform_attr, doc.diagnostics));
            if (!self_closing) {
                style_stack.push_back(merged);
                transform_stack.push_back(transform);
                styled_elements.push_back(name);
            }
            continue;
        }

        if (name == "g") {
            auto merged = detail::merged_style(style_stack.back(), attrs);
            auto transform = transform_stack.back();
            if (auto transform_attr = detail::attr(attrs, "transform"))
                transform = detail::transform_multiply(
                    transform,
                    detail::parse_transform(*transform_attr, doc.diagnostics));
            if (!self_closing) {
                style_stack.push_back(merged);
                transform_stack.push_back(transform);
                styled_elements.push_back(name);
            }
            continue;
        }

        if (auto shape = detail::shape_from_tag(
                name, attrs, style_stack.back(), transform_stack.back(), doc)) {
            doc.shapes.push_back(std::move(*shape));
        }
    }

    if (doc.view_width <= 0.0f || doc.view_height <= 0.0f) {
        detail::add_diagnostic(doc, "svg.viewBox: invalid viewport");
        doc.view_width = 24.0f;
        doc.view_height = 24.0f;
    }
    return doc;
}

auto document_token(Document const& doc) noexcept -> std::uint64_t {
    std::uint64_t state = 1469598103934665603ull;
    detail::hash_float(state, doc.view_min_x);
    detail::hash_float(state, doc.view_min_y);
    detail::hash_float(state, doc.view_width);
    detail::hash_float(state, doc.view_height);
    detail::hash_mix(state, doc.shapes.size());
    detail::hash_mix(state, doc.diagnostics.size());
    detail::hash_mix(state, doc.unsupported_count);
    for (auto const& shape : doc.shapes) {
        detail::hash_string(state, shape.element);
        detail::hash_path(state, shape.path);
        detail::hash_style(state, shape.style);
    }
    for (auto const& diagnostic : doc.diagnostics)
        detail::hash_string(state, diagnostic);
    return state == 0 ? 1469598103934665603ull : state;
}

auto paint_token(Document const& doc,
                 float width,
                 float height,
                 Color current_color,
                 bool preserve_aspect_ratio = true) noexcept
        -> std::uint64_t {
    auto state = document_token(doc);
    detail::hash_float(state, width);
    detail::hash_float(state, height);
    detail::hash_color(state, current_color);
    detail::hash_mix(state, preserve_aspect_ratio ? 1u : 0u);
    return state == 0 ? 1469598103934665603ull : state;
}

void paint(Painter& painter,
           Document const& doc,
           float x,
           float y,
           float width,
           float height,
           RenderOptions options = {}) {
    if (width == 0.0f || height == 0.0f
        || doc.view_width == 0.0f || doc.view_height == 0.0f)
        return;

    float sx = width / doc.view_width;
    float sy = height / doc.view_height;
    float tx = x - doc.view_min_x * sx;
    float ty = y - doc.view_min_y * sy;
    float stroke_scale = (std::abs(sx) + std::abs(sy)) * 0.5f;

    if (options.preserve_aspect_ratio) {
        auto const s = std::min(std::abs(sx), std::abs(sy));
        auto const signed_sx = sx < 0.0f ? -s : s;
        auto const signed_sy = sy < 0.0f ? -s : s;
        auto const drawn_w = doc.view_width * signed_sx;
        auto const drawn_h = doc.view_height * signed_sy;
        tx = x + (width - drawn_w) * 0.5f - doc.view_min_x * signed_sx;
        ty = y + (height - drawn_h) * 0.5f - doc.view_min_y * signed_sy;
        sx = signed_sx;
        sy = signed_sy;
        stroke_scale = s;
    }

    auto render_transform = detail::Transform{sx, 0.0f, 0.0f, sy, tx, ty};
    for (auto const& shape : doc.shapes) {
        auto transformed = detail::transformed_path(shape.path, render_transform);
        if (auto fill = detail::resolve_paint(shape.style.fill, shape.style, options))
            painter.fill_path(transformed, *fill);
        if (auto stroke = detail::resolve_paint(shape.style.stroke, shape.style, options)) {
            auto const thickness = shape.style.stroke_width * stroke_scale;
            if (thickness > 0.0f)
                painter.stroke_path(transformed, thickness, *stroke);
        }
    }
}

} // namespace phenotype::svg

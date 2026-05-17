module;
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
export module phenotype.svg;

import phenotype.types;

export namespace phenotype::svg {

enum class PaintKind {
    None,
    Color,
    CurrentColor,
};

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

} // namespace phenotype::svg

namespace phenotype::svg::detail {

struct Attribute {
    std::string name;
    std::string value;
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
                             float tx,
                             float ty,
                             float sx,
                             float sy) -> PathBuilder {
    PathBuilder out;
    unsigned int i = 0;
    auto xy = [&](float& x, float& y) {
        x = read_f32(path.verbs[i++]) * sx + tx;
        y = read_f32(path.verbs[i++]) * sy + ty;
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
                * ((std::abs(sx) + std::abs(sy)) * 0.5f);
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

inline auto parse_path_data(std::string_view d, Document& doc) -> PathBuilder {
    PathBuilder path;
    PathCursor cursor;
    char command = 0;
    std::size_t i = 0;

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

} // namespace phenotype::svg::detail

export namespace phenotype::svg {

auto parse(std::string_view source) -> Document {
    Document doc;
    std::vector<Style> style_stack;
    std::vector<std::string> styled_elements;
    style_stack.push_back(Style{});

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
                if (style_stack.size() > 1)
                    style_stack.pop_back();
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
            if (!self_closing) {
                style_stack.push_back(merged);
                styled_elements.push_back(name);
            }
            continue;
        }

        if (name == "g") {
            auto merged = detail::merged_style(style_stack.back(), attrs);
            if (!self_closing) {
                style_stack.push_back(merged);
                styled_elements.push_back(name);
            }
            continue;
        }

        if (auto shape = detail::shape_from_tag(
                name, attrs, style_stack.back(), doc)) {
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

    for (auto const& shape : doc.shapes) {
        auto transformed = detail::transformed_path(shape.path, tx, ty, sx, sy);
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

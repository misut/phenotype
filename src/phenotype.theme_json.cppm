module;
#include <charconv>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
export module phenotype.theme_json;

import phenotype.types;
import json;
import txn;

namespace phenotype::detail {

inline auto hex_digit(char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline auto parse_hex_color(std::string_view body) -> std::optional<Color> {
    // Body is the substring after '#': 3, 4, 6, or 8 hex digits.
    auto pair = [&](std::size_t i, std::size_t j) -> int {
        auto hi = hex_digit(body[i]);
        auto lo = hex_digit(body[j]);
        if (hi < 0 || lo < 0) return -1;
        return (hi << 4) | lo;
    };
    auto single = [&](std::size_t i) -> int {
        auto d = hex_digit(body[i]);
        if (d < 0) return -1;
        return (d << 4) | d; // expand 0xF -> 0xFF
    };

    int r = -1, g = -1, b = -1, a = 255;
    switch (body.size()) {
    case 3: r = single(0); g = single(1); b = single(2); break;
    case 4: r = single(0); g = single(1); b = single(2); a = single(3); break;
    case 6: r = pair(0, 1); g = pair(2, 3); b = pair(4, 5); break;
    case 8: r = pair(0, 1); g = pair(2, 3); b = pair(4, 5); a = pair(6, 7); break;
    default: return std::nullopt;
    }
    if (r < 0 || g < 0 || b < 0 || a < 0) return std::nullopt;
    return Color{
        static_cast<unsigned char>(r),
        static_cast<unsigned char>(g),
        static_cast<unsigned char>(b),
        static_cast<unsigned char>(a)};
}

inline auto trim(std::string_view s) -> std::string_view {
    auto first = s.find_first_not_of(" \t");
    if (first == std::string_view::npos) return {};
    auto last = s.find_last_not_of(" \t");
    return s.substr(first, last - first + 1);
}

inline auto parse_int_in_range(std::string_view s, int lo, int hi)
    -> std::optional<int> {
    int value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
    if (value < lo || value > hi) return std::nullopt;
    return value;
}

inline auto parse_unit_float(std::string_view s) -> std::optional<double> {
    double value = 0.0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
    if (value < 0.0 || value > 1.0) return std::nullopt;
    return value;
}

inline auto parse_rgb_func(std::string_view s) -> std::optional<Color> {
    bool has_alpha = false;
    std::string_view inside;
    if (s.starts_with("rgba(") && s.ends_with(")")) {
        has_alpha = true;
        inside = s.substr(5, s.size() - 6);
    } else if (s.starts_with("rgb(") && s.ends_with(")")) {
        inside = s.substr(4, s.size() - 5);
    } else {
        return std::nullopt;
    }

    std::vector<std::string_view> parts;
    std::size_t pos = 0;
    while (true) {
        auto comma = inside.find(',', pos);
        auto end = (comma == std::string_view::npos) ? inside.size() : comma;
        parts.push_back(trim(inside.substr(pos, end - pos)));
        if (comma == std::string_view::npos) break;
        pos = comma + 1;
    }

    auto expected_count = has_alpha ? std::size_t{4} : std::size_t{3};
    if (parts.size() != expected_count) return std::nullopt;

    auto r = parse_int_in_range(parts[0], 0, 255);
    auto g = parse_int_in_range(parts[1], 0, 255);
    auto b = parse_int_in_range(parts[2], 0, 255);
    if (!r || !g || !b) return std::nullopt;

    int alpha_byte = 255;
    if (has_alpha) {
        auto a = parse_unit_float(parts[3]);
        if (!a) return std::nullopt;
        // Round 0..1 alpha to nearest 0..255 byte.
        alpha_byte = static_cast<int>((*a) * 255.0 + 0.5);
        if (alpha_byte > 255) alpha_byte = 255;
        if (alpha_byte < 0)   alpha_byte = 0;
    }
    return Color{
        static_cast<unsigned char>(*r),
        static_cast<unsigned char>(*g),
        static_cast<unsigned char>(*b),
        static_cast<unsigned char>(alpha_byte)};
}

inline auto parse_color_string(std::string_view s) -> std::optional<Color> {
    if (s == "transparent") return Color{0, 0, 0, 0};
    if (!s.empty() && s.front() == '#') return parse_hex_color(s.substr(1));
    if (s.starts_with("rgb")) return parse_rgb_func(s);
    return std::nullopt;
}

} // namespace phenotype::detail

namespace phenotype {

// txn ADL hook for Color: accept hex strings (#rgb / #rrggbb /
// #rrggbbaa, plus the 4-digit shorthand), the CSS function form
// (rgb(r,g,b) and rgba(r,g,b,a) with alpha as 0..1), and the literal
// "transparent" alongside the default {r,g,b,a} object form. A string
// that does not parse as a known color produces an explicit error
// rather than falling through to the (would-fail) object path.
template<txn::ValueLike V>
inline auto txn_from_value(txn::tag<Color>, V const& v,
                           std::string const& path)
    -> std::optional<std::expected<Color, txn::ConversionError>> {
    if (!v.is_string()) return std::nullopt; // fall through to {r,g,b,a}
    std::string_view s = v.as_string();
    if (auto parsed = detail::parse_color_string(s); parsed) {
        return std::expected<Color, txn::ConversionError>{*parsed};
    }
    return std::expected<Color, txn::ConversionError>{
        std::unexpected(txn::ConversionError{
            path, std::string{"invalid color string: "} + std::string{s}})};
}

} // namespace phenotype

export namespace phenotype {

// Deserialize a Theme from a JSON string. Missing fields keep their
// C++ default values (txn::Mode::Partial), so callers can pass an
// overlay like {"accent": "#0abab5"} and have every other field fall
// back to the Theme defaults. Color fields accept either an
// {r,g,b,a} object (back-compat) or one of the string forms
// recognized by the ADL hook above (#rgb, #rrggbb, #rrggbbaa,
// rgb(...), rgba(...), or "transparent").
//
// Returns an error string on failure (type mismatch, invalid color
// string). Parse failures (malformed JSON) abort on wasm — callers
// should pre-validate with JSON.parse() on the JS side before passing
// the string into the WASM instance.
inline auto theme_from_json(std::string_view json_str)
    -> std::expected<Theme, std::string> {
    auto value = json::parse(json_str);
    auto result = txn::from_value<Theme>(value, txn::Mode::Partial);
    if (!result)
        return std::unexpected(
            result.error().path + ": " + result.error().message);
    return *result;
}

// Serialize a Theme to a JSON string. Color fields are emitted as
// {r,g,b,a} objects so the output round-trips through both the ADL
// hook and the default reflected path.
inline auto theme_to_json(Theme const& theme) -> std::string {
    auto value = txn::to_value<json::Value>(theme);
    return json::emit(value);
}

} // namespace phenotype

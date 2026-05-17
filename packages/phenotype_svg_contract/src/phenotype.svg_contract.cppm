module;
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
export module phenotype.svg_contract;

export namespace phenotype::svg_contract {

struct DocumentSummary {
    float view_min_x = 0.0f;
    float view_min_y = 0.0f;
    float view_width = 24.0f;
    float view_height = 24.0f;
    std::size_t shape_count = 0;
    std::size_t diagnostic_count = 0;
    unsigned int unsupported_count = 0;
    bool paintable = false;
    bool has_diagnostics = false;
};

struct DocumentInspection {
    DocumentSummary summary;
    std::vector<std::string> diagnostics;
};

inline auto subset_policy() noexcept -> std::string_view {
    return "bounded_svg_vector_image_subset";
}

inline auto supported_elements() noexcept -> std::string_view {
    return "svg,g,path,rect,circle,ellipse,line,polyline,polygon";
}

inline auto supported_path_commands() noexcept -> std::string_view {
    return "M/m L/l H/h V/v Q/q T/t C/c S/s A/a Z/z";
}

inline auto supported_style_attributes() noexcept -> std::string_view {
    return "fill,stroke,stroke-width,stroke-linecap,stroke-linejoin,color,opacity,fill-opacity,stroke-opacity,transform,viewBox,width,height";
}

inline auto unsupported_policy() noexcept -> std::string_view {
    return "unsupported SVG elements and path commands are skipped with diagnostics; core parsing stays pure and never loads external resources";
}

inline auto render_pipeline_policy() noexcept -> std::string_view {
    return "widget::svg_image -> phenotype.svg::paint -> Painter path fill/stroke commands";
}

} // namespace phenotype::svg_contract

namespace phenotype::svg_contract::detail {

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

inline auto trim(std::string_view in) noexcept -> std::string_view {
    while (!in.empty() && is_space(in.front()))
        in.remove_prefix(1);
    while (!in.empty() && is_space(in.back()))
        in.remove_suffix(1);
    return in;
}

inline auto lower_ascii(std::string_view in) -> std::string {
    auto out = std::string{};
    out.reserve(in.size());
    for (char c : in)
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

inline auto parse_float_prefix(std::string_view in)
        -> std::optional<std::pair<float, std::size_t>> {
    in = trim(in);
    if (in.empty())
        return std::nullopt;
    auto buffer = std::string{in};
    char const* begin = buffer.c_str();
    char* end = nullptr;
    float value = std::strtof(begin, &end);
    if (end == begin)
        return std::nullopt;
    return std::pair{value, static_cast<std::size_t>(end - begin)};
}

inline auto parse_number_list(std::string_view in, unsigned int limit)
        -> std::vector<float> {
    auto out = std::vector<float>{};
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

inline auto parse_attributes(std::string_view in) -> std::vector<Attribute> {
    auto out = std::vector<Attribute>{};
    std::size_t i = 0;
    while (i < in.size()) {
        while (i < in.size() && is_space(in[i]))
            ++i;
        if (i >= in.size())
            break;
        auto name_begin = i;
        while (i < in.size() && is_name_char(in[i]))
            ++i;
        if (i == name_begin)
            break;
        auto name = lower_ascii(in.substr(name_begin, i - name_begin));
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
        auto value = std::string{};
        if (in[i] == '"' || in[i] == '\'') {
            char const quote = in[i++];
            auto value_begin = i;
            while (i < in.size() && in[i] != quote)
                ++i;
            value = std::string{in.substr(value_begin, i - value_begin)};
            if (i < in.size())
                ++i;
        } else {
            auto value_begin = i;
            while (i < in.size() && !is_space(in[i]))
                ++i;
            value = std::string{in.substr(value_begin, i - value_begin)};
        }
        out.push_back({std::move(name), std::move(value)});
    }
    return out;
}

inline auto attr(std::vector<Attribute> const& attrs, std::string_view name)
        -> std::optional<std::string_view> {
    auto found = std::ranges::find_if(attrs, [&](Attribute const& attr) {
        return attr.name == name;
    });
    if (found == attrs.end())
        return std::nullopt;
    return std::string_view{found->value};
}

inline bool is_shape_element(std::string_view name) noexcept {
    return name == "path" || name == "rect" || name == "circle"
        || name == "ellipse" || name == "line" || name == "polyline"
        || name == "polygon";
}

inline bool is_known_container(std::string_view name) noexcept {
    return name == "svg" || name == "g";
}

inline bool is_ignored_metadata(std::string_view name) noexcept {
    return name == "title" || name == "desc" || name == "metadata";
}

inline void add_diagnostic(DocumentInspection& inspection,
                           std::string message,
                           bool unsupported = false) {
    inspection.diagnostics.push_back(std::move(message));
    if (unsupported)
        ++inspection.summary.unsupported_count;
}

inline void apply_svg_metrics(DocumentInspection& inspection,
                              std::vector<Attribute> const& attrs) {
    if (auto value = attr(attrs, "viewbox")) {
        auto nums = parse_number_list(*value, 4);
        if (nums.size() == 4) {
            inspection.summary.view_min_x = nums[0];
            inspection.summary.view_min_y = nums[1];
            inspection.summary.view_width = nums[2];
            inspection.summary.view_height = nums[3];
            return;
        }
        add_diagnostic(inspection, "svg.viewBox: invalid viewBox");
    }
    if (auto width = attr(attrs, "width")) {
        if (auto parsed = parse_float(*width))
            inspection.summary.view_width = *parsed;
    }
    if (auto height = attr(attrs, "height")) {
        if (auto parsed = parse_float(*height))
            inspection.summary.view_height = *parsed;
    }
}

inline bool is_supported_path_command(char c) noexcept {
    switch (static_cast<char>(std::tolower(static_cast<unsigned char>(c)))) {
    case 'm':
    case 'l':
    case 'h':
    case 'v':
    case 'q':
    case 't':
    case 'c':
    case 's':
    case 'a':
    case 'z':
        return true;
    default:
        return false;
    }
}

inline bool path_alpha_is_number_exponent(std::string_view path_data,
                                          std::size_t index) noexcept {
    char const c = path_data[index];
    if (c != 'e' && c != 'E')
        return false;
    if (index == 0 || index + 1 >= path_data.size())
        return false;
    auto const previous = static_cast<unsigned char>(path_data[index - 1]);
    auto const next = static_cast<unsigned char>(path_data[index + 1]);
    return (std::isdigit(previous) || path_data[index - 1] == '.')
        && (std::isdigit(next) || path_data[index + 1] == '-'
            || path_data[index + 1] == '+');
}

inline void inspect_path_commands(DocumentInspection& inspection,
                                  std::string_view path_data) {
    for (std::size_t i = 0; i < path_data.size(); ++i) {
        char const c = path_data[i];
        if (!std::isalpha(static_cast<unsigned char>(c)))
            continue;
        if (path_alpha_is_number_exponent(path_data, i))
            continue;
        if (!is_supported_path_command(c)) {
            add_diagnostic(
                inspection,
                std::string{"svg.path: unsupported command '"} + c + "'",
                true);
        }
    }
}

} // namespace phenotype::svg_contract::detail

export namespace phenotype::svg_contract {

inline auto inspect_source(std::string_view source) -> DocumentInspection {
    auto inspection = DocumentInspection{};
    std::size_t cursor = 0;
    while (true) {
        auto const open = source.find('<', cursor);
        if (open == std::string_view::npos)
            break;
        auto const close = source.find('>', open + 1);
        if (close == std::string_view::npos) {
            detail::add_diagnostic(inspection, "svg.xml: unterminated tag");
            break;
        }
        auto inside = detail::trim(source.substr(open + 1, close - open - 1));
        cursor = close + 1;
        if (inside.empty())
            continue;
        if (inside[0] == '!' || inside[0] == '?')
            continue;
        if (inside[0] == '/')
            continue;
        if (!inside.empty() && inside.back() == '/') {
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
            detail::apply_svg_metrics(inspection, attrs);
            continue;
        }
        if (detail::is_shape_element(name)) {
            ++inspection.summary.shape_count;
            if (name == "path") {
                if (auto path = detail::attr(attrs, "d"))
                    detail::inspect_path_commands(inspection, *path);
                else
                    detail::add_diagnostic(inspection, "svg.path: missing d attribute");
            }
            continue;
        }
        if (detail::is_known_container(name) || detail::is_ignored_metadata(name))
            continue;
        detail::add_diagnostic(
            inspection,
            std::string{"svg.element: unsupported element '"} + name + "'",
            true);
    }

    if (inspection.summary.view_width <= 0.0f
        || inspection.summary.view_height <= 0.0f) {
        detail::add_diagnostic(inspection, "svg.viewBox: invalid viewport");
        inspection.summary.view_width = 24.0f;
        inspection.summary.view_height = 24.0f;
    }
    inspection.summary.diagnostic_count = inspection.diagnostics.size();
    inspection.summary.has_diagnostics = !inspection.diagnostics.empty();
    inspection.summary.paintable = inspection.summary.shape_count > 0;
    return inspection;
}

} // namespace phenotype::svg_contract

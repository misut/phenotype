module;
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module glass_showcase_shared;

export namespace glass_showcase_demo {

inline constexpr int k_default_viewport_width = 520;
inline constexpr int k_default_viewport_height = 760;
inline constexpr float k_default_viewport_scale = 1.0f;
inline constexpr std::size_t k_density_count = 3;
inline constexpr std::size_t k_default_density = 1;
inline constexpr std::size_t k_material_kind_count = 4;
inline constexpr std::size_t k_base_material_plan_count = 6;
inline constexpr std::size_t k_inspector_material_plan_count = 1;

struct ToggleBackdrop {};
struct SetBackdropContrast { bool high_contrast = false; };
struct ToggleInspector {};
struct SetInspector { bool open = true; };
struct SelectDensity { std::size_t value = k_default_density; };
struct NoteChanged { std::string text; };
struct Resized {
    int width = k_default_viewport_width;
    int height = k_default_viewport_height;
    float scale = k_default_viewport_scale;
};
struct Reset {};

using Msg = std::variant<
    ToggleBackdrop,
    SetBackdropContrast,
    ToggleInspector,
    SetInspector,
    SelectDensity,
    NoteChanged,
    Resized,
    Reset>;

struct State {
    bool high_contrast_backdrop = false;
    bool inspector_open = true;
    std::size_t selected_density = k_default_density;
    std::string note = "Artifact-ready material contract";
    int viewport_width = k_default_viewport_width;
    int viewport_height = k_default_viewport_height;
    float viewport_scale = k_default_viewport_scale;
};

enum class GlassInputKind {
    Noop,
    ToggleBackdrop,
    SetBackdrop,
    ToggleInspector,
    SetInspector,
    SelectDensity,
    Note,
    Viewport,
    Reset,
};

struct GlassInput {
    GlassInputKind kind = GlassInputKind::Noop;
    std::string value;
    std::size_t density = k_default_density;
    bool flag = false;
    int viewport_width = k_default_viewport_width;
    int viewport_height = k_default_viewport_height;
    float viewport_scale = k_default_viewport_scale;
};

struct GlassInputParseResult {
    bool ok = false;
    GlassInput input{};
    std::string error;
};

struct GlassInputTrace {
    GlassInput input{};
    State state{};
    std::size_t expected_material_plan_count = 0;
    std::string density_label;
    std::string backdrop_label;
    std::string inspector_label;
};

struct GlassDriveResult {
    State state{};
    std::vector<GlassInputTrace> trace;
};

enum class GlassExpectationKind {
    Density,
    Backdrop,
    Inspector,
    Note,
    NoteContains,
    Viewport,
    MaterialCount,
};

struct GlassExpectation {
    GlassExpectationKind kind = GlassExpectationKind::Density;
    std::string value;
    std::size_t density = k_default_density;
    bool flag = false;
    int viewport_width = k_default_viewport_width;
    int viewport_height = k_default_viewport_height;
    float viewport_scale = k_default_viewport_scale;
    std::size_t material_count = 0;
};

struct GlassExpectationParseResult {
    bool ok = false;
    GlassExpectation expectation{};
    std::string error;
};

struct GlassExpectationResult {
    GlassExpectation expectation{};
    bool ok = false;
    std::string actual;
    std::string detail;
};

inline std::string trim(std::string_view text) {
    auto begin = text.begin();
    auto end = text.end();
    while (begin != end
           && static_cast<unsigned char>(*begin) <= static_cast<unsigned char>(' ')) {
        ++begin;
    }
    while (begin != end
           && static_cast<unsigned char>(*(end - 1)) <= static_cast<unsigned char>(' ')) {
        --end;
    }
    return std::string{begin, end};
}

inline std::string lower_copy(std::string_view text) {
    auto out = std::string{text};
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('A')
            && ch <= static_cast<unsigned char>('Z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('A')
                                     + static_cast<unsigned char>('a'));
        }
        return static_cast<char>(ch);
    });
    return out;
}

inline std::optional<int> parse_positive_int(std::string_view text) {
    auto value_text = trim(text);
    if (value_text.empty())
        return std::nullopt;
    char* end = nullptr;
    long value = std::strtol(value_text.c_str(), &end, 10);
    if (end != value_text.c_str() + value_text.size()
        || value <= 0 || value > 100000) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

inline std::optional<float> parse_positive_float(std::string_view text) {
    auto value_text = trim(text);
    if (value_text.empty())
        return std::nullopt;
    char* end = nullptr;
    float value = std::strtof(value_text.c_str(), &end);
    if (end != value_text.c_str() + value_text.size()
        || value <= 0.0f || value > 16.0f) {
        return std::nullopt;
    }
    return value;
}

inline std::optional<std::size_t> parse_density(std::string_view raw) {
    auto value = lower_copy(trim(raw));
    if (value == "comfortable" || value == "comfort" || value == "0")
        return 0;
    if (value == "balanced" || value == "balance" || value == "default"
        || value == "1") {
        return 1;
    }
    if (value == "dense" || value == "compact" || value == "2")
        return 2;
    return std::nullopt;
}

inline std::optional<bool> parse_backdrop_flag(std::string_view raw) {
    auto value = lower_copy(trim(raw));
    if (value == "high" || value == "high-contrast" || value == "contrast"
        || value == "on" || value == "true" || value == "1") {
        return true;
    }
    if (value == "standard" || value == "normal" || value == "default"
        || value == "off" || value == "false" || value == "0") {
        return false;
    }
    return std::nullopt;
}

inline std::optional<bool> parse_inspector_flag(std::string_view raw) {
    auto value = lower_copy(trim(raw));
    if (value == "open" || value == "show" || value == "shown"
        || value == "on" || value == "true" || value == "1") {
        return true;
    }
    if (value == "closed" || value == "close" || value == "hide"
        || value == "hidden" || value == "off" || value == "false"
        || value == "0") {
        return false;
    }
    return std::nullopt;
}

inline std::string viewport_value_label(int width, int height, float scale) {
    auto out = std::ostringstream{};
    out << width << "x" << height;
    if (scale != 1.0f)
        out << "@" << scale;
    return out.str();
}

inline GlassInputParseResult parsed_input(GlassInput input) {
    return GlassInputParseResult{
        .ok = true,
        .input = std::move(input),
    };
}

inline GlassInputParseResult input_parse_error(std::string message) {
    return GlassInputParseResult{
        .ok = false,
        .error = std::move(message),
    };
}

inline GlassInputParseResult parse_viewport_input(std::string_view value) {
    auto text = trim(value);
    auto scale = 1.0f;
    auto scale_separator = text.find('@');
    if (scale_separator != std::string::npos) {
        auto parsed_scale = parse_positive_float(
            std::string_view{text}.substr(scale_separator + 1));
        if (!parsed_scale)
            return input_parse_error("input 'viewport' scale must be positive");
        scale = *parsed_scale;
        text = trim(std::string_view{text}.substr(0, scale_separator));
    }

    auto separator = text.find('x');
    if (separator == std::string::npos)
        separator = text.find('X');
    if (separator == std::string::npos)
        separator = text.find(',');
    if (separator == std::string::npos) {
        return input_parse_error(
            "input 'viewport' requires WIDTHxHEIGHT or WIDTHxHEIGHT@SCALE");
    }

    auto width = parse_positive_int(std::string_view{text}.substr(0, separator));
    auto height = parse_positive_int(std::string_view{text}.substr(separator + 1));
    if (!width || !height)
        return input_parse_error("input 'viewport' width and height must be positive");

    return parsed_input({
        .kind = GlassInputKind::Viewport,
        .value = viewport_value_label(*width, *height, scale),
        .viewport_width = *width,
        .viewport_height = *height,
        .viewport_scale = scale,
    });
}

inline GlassInputParseResult parse_glass_input(std::string_view raw) {
    auto text = trim(raw);
    if (text.empty())
        return parsed_input({});

    auto separator = text.find(':');
    if (separator == std::string::npos)
        separator = text.find('=');
    auto name = lower_copy(trim(separator == std::string::npos
        ? std::string_view{text}
        : std::string_view{text}.substr(0, separator)));
    auto value = separator == std::string::npos
        ? std::string{}
        : trim(std::string_view{text}.substr(separator + 1));

    if (name == "noop")
        return parsed_input({});
    if (name == "reset")
        return parsed_input({.kind = GlassInputKind::Reset});
    if (name == "toggle-backdrop" || name == "toggle_backdrop") {
        return parsed_input({.kind = GlassInputKind::ToggleBackdrop});
    }
    if (name == "backdrop" || name == "high-contrast-backdrop"
        || name == "high_contrast_backdrop") {
        if (value.empty())
            return parsed_input({.kind = GlassInputKind::ToggleBackdrop});
        auto flag = parse_backdrop_flag(value);
        if (!flag) {
            return input_parse_error(
                "input 'backdrop' expects high or standard");
        }
        return parsed_input({
            .kind = GlassInputKind::SetBackdrop,
            .value = *flag ? "high" : "standard",
            .flag = *flag,
        });
    }
    if (name == "toggle-inspector" || name == "toggle_inspector") {
        return parsed_input({.kind = GlassInputKind::ToggleInspector});
    }
    if (name == "inspector") {
        if (value.empty())
            return parsed_input({.kind = GlassInputKind::ToggleInspector});
        auto flag = parse_inspector_flag(value);
        if (!flag) {
            return input_parse_error(
                "input 'inspector' expects open or closed");
        }
        return parsed_input({
            .kind = GlassInputKind::SetInspector,
            .value = *flag ? "open" : "closed",
            .flag = *flag,
        });
    }
    if (name == "density") {
        if (value.empty())
            return input_parse_error("input 'density' requires a value");
        auto density = parse_density(value);
        if (!density) {
            return input_parse_error(
                "input 'density' expects comfortable, balanced, dense, 0, 1, or 2");
        }
        return parsed_input({
            .kind = GlassInputKind::SelectDensity,
            .value = lower_copy(trim(value)),
            .density = *density,
        });
    }
    if (name == "note" || name == "debug-note" || name == "debug_note") {
        return parsed_input({
            .kind = GlassInputKind::Note,
            .value = value,
        });
    }
    if (name == "viewport" || name == "resize" || name == "size") {
        if (value.empty())
            return input_parse_error("input 'viewport' requires a value");
        return parse_viewport_input(value);
    }

    return input_parse_error("unknown glass showcase input: " + name);
}

inline std::string density_label(std::size_t value) {
    switch (value) {
        case 0: return "Comfortable";
        case 2: return "Dense";
        default: return "Balanced";
    }
}

inline std::string density_value_name(std::size_t value) {
    switch (value) {
        case 0: return "comfortable";
        case 2: return "dense";
        default: return "balanced";
    }
}

inline std::string backdrop_value_name(bool high_contrast) {
    return high_contrast ? "high" : "standard";
}

inline std::string inspector_value_name(bool open) {
    return open ? "open" : "closed";
}

inline float progress_value(State const& state) {
    return state.high_contrast_backdrop ? 0.85f : 0.55f;
}

inline std::size_t expected_material_plan_count(State const& state) {
    return k_base_material_plan_count
        + (state.inspector_open ? k_inspector_material_plan_count : 0);
}

inline std::vector<std::string> public_material_kinds() {
    return {"clear", "thin", "regular", "thick"};
}

inline Msg note_changed(std::string text) {
    return NoteChanged{std::move(text)};
}

inline Msg select_density(std::size_t value) {
    return SelectDensity{value};
}

inline Msg resized(int width, int height, float scale) {
    return Resized{width, height, scale};
}

inline void update(State& state, Msg msg) {
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, ToggleBackdrop>) {
            state.high_contrast_backdrop = !state.high_contrast_backdrop;
        } else if constexpr (std::is_same_v<T, SetBackdropContrast>) {
            state.high_contrast_backdrop = m.high_contrast;
        } else if constexpr (std::is_same_v<T, ToggleInspector>) {
            state.inspector_open = !state.inspector_open;
        } else if constexpr (std::is_same_v<T, SetInspector>) {
            state.inspector_open = m.open;
        } else if constexpr (std::is_same_v<T, SelectDensity>) {
            state.selected_density = std::min(m.value, k_density_count - 1);
        } else if constexpr (std::is_same_v<T, NoteChanged>) {
            state.note = m.text;
        } else if constexpr (std::is_same_v<T, Resized>) {
            state.viewport_width = m.width;
            state.viewport_height = m.height;
            state.viewport_scale = m.scale;
        } else if constexpr (std::is_same_v<T, Reset>) {
            state = State{};
        }
    }, std::move(msg));
}

inline std::string glass_input_kind_name(GlassInputKind kind) {
    switch (kind) {
        case GlassInputKind::Noop: return "noop";
        case GlassInputKind::ToggleBackdrop: return "toggle_backdrop";
        case GlassInputKind::SetBackdrop: return "set_backdrop";
        case GlassInputKind::ToggleInspector: return "toggle_inspector";
        case GlassInputKind::SetInspector: return "set_inspector";
        case GlassInputKind::SelectDensity: return "density";
        case GlassInputKind::Note: return "note";
        case GlassInputKind::Viewport: return "viewport";
        case GlassInputKind::Reset: return "reset";
    }
    return "noop";
}

inline std::string glass_input_label(GlassInput const& input) {
    auto label = glass_input_kind_name(input.kind);
    switch (input.kind) {
        case GlassInputKind::SelectDensity:
            return label + ":" + density_value_name(input.density);
        case GlassInputKind::SetBackdrop:
            return label + ":" + backdrop_value_name(input.flag);
        case GlassInputKind::SetInspector:
            return label + ":" + inspector_value_name(input.flag);
        case GlassInputKind::Viewport:
            return label + ":" + input.value;
        case GlassInputKind::Note:
            return label + ":" + input.value;
        default:
            return label;
    }
}

inline void apply_glass_input(State& state, GlassInput const& input) {
    switch (input.kind) {
        case GlassInputKind::Noop:
            return;
        case GlassInputKind::ToggleBackdrop:
            update(state, ToggleBackdrop{});
            return;
        case GlassInputKind::SetBackdrop:
            update(state, SetBackdropContrast{.high_contrast = input.flag});
            return;
        case GlassInputKind::ToggleInspector:
            update(state, ToggleInspector{});
            return;
        case GlassInputKind::SetInspector:
            update(state, SetInspector{.open = input.flag});
            return;
        case GlassInputKind::SelectDensity:
            update(state, SelectDensity{.value = input.density});
            return;
        case GlassInputKind::Note:
            update(state, NoteChanged{.text = input.value});
            return;
        case GlassInputKind::Viewport:
            update(state, Resized{
                .width = input.viewport_width,
                .height = input.viewport_height,
                .scale = input.viewport_scale,
            });
            return;
        case GlassInputKind::Reset:
            update(state, Reset{});
            return;
    }
}

inline GlassInputTrace glass_input_trace(State const& state,
                                         GlassInput const& input) {
    return GlassInputTrace{
        .input = input,
        .state = state,
        .expected_material_plan_count = expected_material_plan_count(state),
        .density_label = density_label(state.selected_density),
        .backdrop_label = backdrop_value_name(state.high_contrast_backdrop),
        .inspector_label = inspector_value_name(state.inspector_open),
    };
}

inline GlassDriveResult drive_glass_showcase(
        std::span<GlassInput const> inputs) {
    auto result = GlassDriveResult{};
    result.trace.reserve(inputs.size());
    for (auto const& input : inputs) {
        apply_glass_input(result.state, input);
        result.trace.push_back(glass_input_trace(result.state, input));
    }
    return result;
}

inline GlassExpectationParseResult parsed_expectation(
        GlassExpectation expectation) {
    return GlassExpectationParseResult{
        .ok = true,
        .expectation = std::move(expectation),
    };
}

inline GlassExpectationParseResult expectation_parse_error(
        std::string message) {
    return GlassExpectationParseResult{
        .ok = false,
        .error = std::move(message),
    };
}

inline GlassExpectationParseResult parse_glass_expectation(
        std::string_view raw) {
    auto text = trim(raw);
    auto separator = text.find(':');
    if (separator == std::string::npos)
        separator = text.find('=');
    if (text.empty() || separator == std::string::npos) {
        return expectation_parse_error(
            "expectation requires kind:value, such as density:dense");
    }

    auto name = lower_copy(trim(std::string_view{text}.substr(0, separator)));
    auto value = trim(std::string_view{text}.substr(separator + 1));
    if (value.empty())
        return expectation_parse_error("expectation '" + name + "' requires a value");

    if (name == "density") {
        auto density = parse_density(value);
        if (!density)
            return expectation_parse_error("expectation 'density' is unknown");
        return parsed_expectation({
            .kind = GlassExpectationKind::Density,
            .value = density_value_name(*density),
            .density = *density,
        });
    }
    if (name == "backdrop") {
        auto flag = parse_backdrop_flag(value);
        if (!flag)
            return expectation_parse_error("expectation 'backdrop' is unknown");
        return parsed_expectation({
            .kind = GlassExpectationKind::Backdrop,
            .value = backdrop_value_name(*flag),
            .flag = *flag,
        });
    }
    if (name == "inspector") {
        auto flag = parse_inspector_flag(value);
        if (!flag)
            return expectation_parse_error("expectation 'inspector' is unknown");
        return parsed_expectation({
            .kind = GlassExpectationKind::Inspector,
            .value = inspector_value_name(*flag),
            .flag = *flag,
        });
    }
    if (name == "note") {
        return parsed_expectation({
            .kind = GlassExpectationKind::Note,
            .value = value,
        });
    }
    if (name == "note-contains" || name == "note_contains") {
        return parsed_expectation({
            .kind = GlassExpectationKind::NoteContains,
            .value = value,
        });
    }
    if (name == "viewport" || name == "size") {
        auto parsed = parse_viewport_input(value);
        if (!parsed.ok)
            return expectation_parse_error(parsed.error);
        return parsed_expectation({
            .kind = GlassExpectationKind::Viewport,
            .value = parsed.input.value,
            .viewport_width = parsed.input.viewport_width,
            .viewport_height = parsed.input.viewport_height,
            .viewport_scale = parsed.input.viewport_scale,
        });
    }
    if (name == "material-count" || name == "material_count"
        || name == "plans" || name == "plan-count") {
        auto count = parse_positive_int(value);
        if (!count)
            return expectation_parse_error(
                "expectation 'material-count' must be a positive integer");
        return parsed_expectation({
            .kind = GlassExpectationKind::MaterialCount,
            .value = std::to_string(*count),
            .material_count = static_cast<std::size_t>(*count),
        });
    }

    return expectation_parse_error("unknown glass showcase expectation: " + name);
}

inline std::string glass_expectation_kind_name(GlassExpectationKind kind) {
    switch (kind) {
        case GlassExpectationKind::Density: return "density";
        case GlassExpectationKind::Backdrop: return "backdrop";
        case GlassExpectationKind::Inspector: return "inspector";
        case GlassExpectationKind::Note: return "note";
        case GlassExpectationKind::NoteContains: return "note-contains";
        case GlassExpectationKind::Viewport: return "viewport";
        case GlassExpectationKind::MaterialCount: return "material-count";
    }
    return "density";
}

inline std::string glass_expectation_label(
        GlassExpectation const& expectation) {
    return glass_expectation_kind_name(expectation.kind) + ":"
        + expectation.value;
}

inline GlassExpectationResult check_glass_expectation(
        GlassDriveResult const& result,
        GlassExpectation const& expectation) {
    auto checked = GlassExpectationResult{
        .expectation = expectation,
    };
    auto const& state = result.state;
    switch (expectation.kind) {
        case GlassExpectationKind::Density:
            checked.actual = density_value_name(state.selected_density);
            checked.ok = state.selected_density == expectation.density;
            checked.detail = checked.ok
                ? "density matched"
                : "density did not match";
            return checked;
        case GlassExpectationKind::Backdrop:
            checked.actual = backdrop_value_name(state.high_contrast_backdrop);
            checked.ok = state.high_contrast_backdrop == expectation.flag;
            checked.detail = checked.ok
                ? "backdrop matched"
                : "backdrop did not match";
            return checked;
        case GlassExpectationKind::Inspector:
            checked.actual = inspector_value_name(state.inspector_open);
            checked.ok = state.inspector_open == expectation.flag;
            checked.detail = checked.ok
                ? "inspector matched"
                : "inspector did not match";
            return checked;
        case GlassExpectationKind::Note:
            checked.actual = state.note;
            checked.ok = state.note == expectation.value;
            checked.detail = checked.ok
                ? "note matched"
                : "note did not match";
            return checked;
        case GlassExpectationKind::NoteContains:
            checked.actual = state.note;
            checked.ok = state.note.find(expectation.value) != std::string::npos;
            checked.detail = checked.ok
                ? "note contained expected text"
                : "note did not contain expected text";
            return checked;
        case GlassExpectationKind::Viewport:
            checked.actual = viewport_value_label(
                state.viewport_width,
                state.viewport_height,
                state.viewport_scale);
            checked.ok = state.viewport_width == expectation.viewport_width
                && state.viewport_height == expectation.viewport_height
                && state.viewport_scale == expectation.viewport_scale;
            checked.detail = checked.ok
                ? "viewport matched"
                : "viewport did not match";
            return checked;
        case GlassExpectationKind::MaterialCount:
            checked.actual = std::to_string(expected_material_plan_count(state));
            checked.ok = expected_material_plan_count(state)
                == expectation.material_count;
            checked.detail = checked.ok
                ? "material plan count matched"
                : "material plan count did not match";
            return checked;
    }
    checked.actual = "<unknown>";
    checked.detail = "unknown expectation";
    return checked;
}

inline std::vector<GlassExpectationResult> check_glass_expectations(
        GlassDriveResult const& result,
        std::span<GlassExpectation const> expectations) {
    auto checked = std::vector<GlassExpectationResult>{};
    checked.reserve(expectations.size());
    for (auto const& expectation : expectations)
        checked.push_back(check_glass_expectation(result, expectation));
    return checked;
}

inline bool glass_expectations_ok(
        std::span<GlassExpectationResult const> expectations) {
    return std::ranges::all_of(expectations, [](auto const& expectation) {
        return expectation.ok;
    });
}

} // namespace glass_showcase_demo

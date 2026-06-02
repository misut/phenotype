module;
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

import json;

export namespace glass_showcase_demo {

inline constexpr int k_default_viewport_width = 520;
inline constexpr int k_default_viewport_height = 860;
inline constexpr float k_default_viewport_scale = 1.0f;
inline constexpr std::size_t k_density_count = 3;
inline constexpr std::size_t k_default_density = 1;
inline constexpr std::size_t k_material_kind_count = 4;
inline constexpr std::size_t k_base_material_plan_count = 15;
inline constexpr std::size_t k_inspector_material_plan_count = 1;
inline constexpr std::size_t k_total_material_probe_count =
    k_base_material_plan_count + k_inspector_material_plan_count;
inline constexpr std::size_t k_probe_stage_count = 4;
inline constexpr std::size_t k_probe_sample_taps = 25;
inline constexpr float k_probe_max_blur_radius = 36.0f;

enum class GlassProbeMaterialKind {
    Clear,
    Thin,
    Regular,
    Thick,
};

struct GlassMaterialProbe {
    std::string_view name;
    std::string_view label;
    std::string_view description;
    GlassProbeMaterialKind kind = GlassProbeMaterialKind::Regular;
    std::uint32_t container_id = 0;
    std::uint32_t union_id = 0;
    bool has_container_id = false;
    bool has_union_id = false;
    bool requires_inspector_open = false;
    bool interactive = false;
    bool morph_transitions = false;
    std::string_view verifier_profile;
    std::string_view likely_layer = "material-blur-pass";
    std::string_view expected_pass = "backdrop-sample-blur";
    std::string_view expected_executor = "backdrop-filter";
    std::string_view expected_luminance_curve = "adaptive-backdrop-luma";
    std::string_view expected_sampling_kernel = "gaussian-5x5";
    std::string_view fallback_path = "translucent-rounded-rect";
    bool requires_backdrop_sampling = true;
    bool requires_luminance_preservation = true;
    bool requires_edge_highlight = true;
    bool requires_noise_dither = true;
    bool excludes_foreground_text = true;
    std::size_t stage_count = k_probe_stage_count;
    std::size_t sample_taps = k_probe_sample_taps;
};

struct GlassProbeContract {
    std::string_view contract_name = "glass_showcase_material_probe_contract";
    std::uint32_t schema_version = 1;
    std::string_view reference_technology = "liquid-glass";
    std::string_view reference_material_policy =
        "liquid-glass-functional-layer";
    std::string_view reference_guidance =
        "Apple Liquid Glass custom views and HIG Materials: blur content behind the surface, preserve vibrant foreground legibility, and degrade deterministically";
    std::string_view backdrop_sampling_contract =
        "deterministic backdrop probe canvas with foreground text excluded";
    std::string_view fallback_contract =
        "translucent rounded rectangle when sampled backdrop is unavailable";
    std::string_view pixel_contract =
        "backdrop-pattern, visible-blur-probe, and visible-blur-smoothness regions";
    std::size_t material_probe_count = k_total_material_probe_count;
    std::size_t base_material_probe_count = k_base_material_plan_count;
    std::size_t inspector_material_probe_count =
        k_inspector_material_plan_count;
    std::size_t active_material_probe_count = k_total_material_probe_count;
    std::size_t stage_count_per_probe = k_probe_stage_count;
    std::size_t sample_taps_per_probe = k_probe_sample_taps;
    float max_blur_radius = k_probe_max_blur_radius;
    std::size_t total_expected_execution_stages =
        k_total_material_probe_count * k_probe_stage_count;
    std::size_t total_expected_sample_taps =
        k_total_material_probe_count * k_probe_sample_taps;
};

inline auto glass_probe_material_kind_name(
        GlassProbeMaterialKind kind) -> std::string_view {
    switch (kind) {
        case GlassProbeMaterialKind::Clear: return "clear";
        case GlassProbeMaterialKind::Thin: return "thin";
        case GlassProbeMaterialKind::Regular: return "regular";
        case GlassProbeMaterialKind::Thick: return "thick";
    }
    return "regular";
}

inline constexpr std::array<GlassMaterialProbe, k_total_material_probe_count>
    k_material_probes{{
        {
            .name = "ramp_clear",
            .label = "Clear Material",
            .description =
                "Lowest opacity surface for contextual controls over rich content.",
            .kind = GlassProbeMaterialKind::Clear,
            .container_id = 1001u,
            .has_container_id = true,
            .morph_transitions = true,
            .verifier_profile = "clear-over-rich-backdrop",
        },
        {
            .name = "ramp_thin",
            .label = "Thin Material",
            .description =
                "Balanced surface for floating navigation and secondary controls.",
            .kind = GlassProbeMaterialKind::Thin,
            .container_id = 1001u,
            .has_container_id = true,
            .morph_transitions = true,
            .verifier_profile = "thin-balanced-backdrop",
        },
        {
            .name = "ramp_regular",
            .label = "Regular Material",
            .description =
                "Default legible glass layer for primary controls and inspectors.",
            .kind = GlassProbeMaterialKind::Regular,
            .container_id = 1001u,
            .has_container_id = true,
            .morph_transitions = true,
            .verifier_profile = "regular-legibility-backdrop",
        },
        {
            .name = "ramp_thick",
            .label = "Thick Material",
            .description =
                "High-contrast surface for dense text over active content.",
            .kind = GlassProbeMaterialKind::Thick,
            .container_id = 1001u,
            .has_container_id = true,
            .morph_transitions = true,
            .verifier_profile = "thick-high-contrast-backdrop",
        },
        {
            .name = "control_layer",
            .label = "Control Layer",
            .description =
                "Interactive regular material proving input, focus, and controls stay debuggable.",
            .kind = GlassProbeMaterialKind::Regular,
            .container_id = 1003u,
            .has_container_id = true,
            .interactive = true,
            .verifier_profile = "regular-legibility-backdrop",
        },
        {
            .name = "checkbox_control_probe",
            .label = "Glass Checkbox",
            .description =
                "Material-backed checkbox indicator proving boolean controls are part of the glass contract.",
            .kind = GlassProbeMaterialKind::Thin,
            .interactive = true,
            .verifier_profile = "thin-balanced-backdrop",
            .likely_layer = "material-control-indicator",
        },
        {
            .name = "radio_control_probe",
            .label = "Glass Radio",
            .description =
                "Material-backed radio indicator proving selection controls expose resolved glass optics.",
            .kind = GlassProbeMaterialKind::Thin,
            .interactive = true,
            .verifier_profile = "thin-balanced-backdrop",
            .likely_layer = "material-control-indicator",
        },
        {
            .name = "switch_control_probe",
            .label = "Glass Switch",
            .description =
                "Material-backed switch track proving animated control state stays artifact-visible.",
            .kind = GlassProbeMaterialKind::Thin,
            .interactive = true,
            .verifier_profile = "thin-balanced-backdrop",
            .likely_layer = "material-control-track",
        },
        {
            .name = "visible_blur_probe",
            .label = "Visible Blur Probe",
            .description =
                "Overlay thin material sampling the deterministic backdrop.",
            .kind = GlassProbeMaterialKind::Thin,
            .container_id = 1002u,
            .union_id = 1u,
            .has_container_id = true,
            .has_union_id = true,
            .morph_transitions = true,
            .verifier_profile = "thin-balanced-backdrop",
        },
        {
            .name = "tooltip_probe",
            .label = "Glass Tooltip Probe",
            .description =
                "Noninteractive overlay tooltip proving lightweight glass affordances stay semantic.",
            .kind = GlassProbeMaterialKind::Thin,
            .verifier_profile = "thin-balanced-backdrop",
        },
        {
            .name = "context_menu_probe",
            .label = "Glass Context Menu Probe",
            .description =
                "Interactive overlay menu proving contextual actions use the same material contract.",
            .kind = GlassProbeMaterialKind::Regular,
            .interactive = true,
            .verifier_profile = "regular-legibility-backdrop",
        },
        {
            .name = "selection_sidebar_probe",
            .label = "Glass Sidebar Selection",
            .description =
                "Selected sidebar pill proving list selection chrome stays on the sampled glass path.",
            .kind = GlassProbeMaterialKind::Thin,
            .interactive = true,
            .verifier_profile = "thin-balanced-backdrop",
            .likely_layer = "material-selection-row",
        },
        {
            .name = "outline_row_probe",
            .label = "Glass Outline Row",
            .description =
                "Selected outline row proving hierarchical list state emits typed material metadata.",
            .kind = GlassProbeMaterialKind::Thin,
            .interactive = true,
            .verifier_profile = "thin-balanced-backdrop",
            .likely_layer = "material-outline-row",
        },
        {
            .name = "table_header_probe",
            .label = "Glass Table Header",
            .description =
                "Sorted content header proving dense table chrome uses standard material without backdrop sampling.",
            .kind = GlassProbeMaterialKind::Clear,
            .interactive = true,
            .verifier_profile = "clear-content-standard-material",
            .likely_layer = "material-standard-pass",
            .expected_pass = "standard-material-fill",
            .expected_executor = "standard-fill",
            .expected_luminance_curve = "fallback-flat",
            .expected_sampling_kernel = "none",
            .fallback_path = "none",
            .requires_backdrop_sampling = false,
            .requires_noise_dither = false,
            .excludes_foreground_text = false,
            .stage_count = 3u,
            .sample_taps = 0u,
        },
        {
            .name = "disclosure_header_probe",
            .label = "Glass Disclosure Header",
            .description =
                "Expanded disclosure header proving app hierarchy chrome keeps a debuggable material node.",
            .kind = GlassProbeMaterialKind::Clear,
            .interactive = true,
            .verifier_profile = "clear-content-standard-material",
            .likely_layer = "material-standard-pass",
            .expected_pass = "standard-material-fill",
            .expected_executor = "standard-fill",
            .expected_luminance_curve = "fallback-flat",
            .expected_sampling_kernel = "none",
            .fallback_path = "none",
            .requires_backdrop_sampling = false,
            .requires_noise_dither = false,
            .excludes_foreground_text = false,
            .stage_count = 3u,
            .sample_taps = 0u,
        },
        {
            .name = "debug_contract",
            .label = "Debug Contract",
            .description =
                "Inspector thick material exposing the probe scene contract in artifacts.",
            .kind = GlassProbeMaterialKind::Thick,
            .requires_inspector_open = true,
            .verifier_profile = "thick-high-contrast-backdrop",
        },
    }};

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

using Action = std::variant<
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

inline std::size_t expected_material_probe_count(State const& state) {
    return k_base_material_plan_count
        + (state.inspector_open ? k_inspector_material_plan_count : 0);
}

inline bool glass_probe_is_active(GlassMaterialProbe const& probe,
                                  State const& state) {
    return !probe.requires_inspector_open || state.inspector_open;
}

inline std::size_t expected_material_execution_stage_count(State const& state) {
    std::size_t total = 0;
    for (auto const& probe : k_material_probes) {
        if (glass_probe_is_active(probe, state))
            total += probe.stage_count;
    }
    return total;
}

inline std::size_t expected_material_sample_tap_count(State const& state) {
    std::size_t total = 0;
    for (auto const& probe : k_material_probes) {
        if (glass_probe_is_active(probe, state))
            total += probe.sample_taps;
    }
    return total;
}

inline GlassProbeContract glass_probe_contract(State const& state) {
    auto contract = GlassProbeContract{};
    contract.active_material_probe_count = expected_material_probe_count(state);
    contract.total_expected_execution_stages =
        expected_material_execution_stage_count(state);
    contract.total_expected_sample_taps =
        expected_material_sample_tap_count(state);
    return contract;
}

inline GlassMaterialProbe glass_material_probe_at(std::size_t index) {
    if (index >= k_material_probes.size())
        return k_material_probes.front();
    return k_material_probes[index];
}

inline json::Value glass_material_probe_debug_json(
        GlassMaterialProbe const& probe) {
    json::Object out;
    out.emplace("name", json::Value{std::string{probe.name}});
    out.emplace("label", json::Value{std::string{probe.label}});
    out.emplace("description", json::Value{std::string{probe.description}});
    out.emplace(
        "kind",
        json::Value{std::string{glass_probe_material_kind_name(probe.kind)}});
    out.emplace(
        "container_id",
        json::Value{static_cast<std::int64_t>(probe.container_id)});
    out.emplace(
        "union_id",
        json::Value{static_cast<std::int64_t>(probe.union_id)});
    out.emplace("has_container_id", json::Value{probe.has_container_id});
    out.emplace("has_union_id", json::Value{probe.has_union_id});
    out.emplace(
        "requires_inspector_open",
        json::Value{probe.requires_inspector_open});
    out.emplace("interactive", json::Value{probe.interactive});
    out.emplace("morph_transitions", json::Value{probe.morph_transitions});
    out.emplace(
        "verifier_profile",
        json::Value{std::string{probe.verifier_profile}});
    out.emplace(
        "likely_layer",
        json::Value{std::string{probe.likely_layer}});
    out.emplace(
        "expected_pass",
        json::Value{std::string{probe.expected_pass}});
    out.emplace(
        "expected_executor",
        json::Value{std::string{probe.expected_executor}});
    out.emplace(
        "expected_luminance_curve",
        json::Value{std::string{probe.expected_luminance_curve}});
    out.emplace(
        "expected_sampling_kernel",
        json::Value{std::string{probe.expected_sampling_kernel}});
    out.emplace(
        "fallback_path",
        json::Value{std::string{probe.fallback_path}});
    out.emplace(
        "requires_backdrop_sampling",
        json::Value{probe.requires_backdrop_sampling});
    out.emplace(
        "requires_luminance_preservation",
        json::Value{probe.requires_luminance_preservation});
    out.emplace(
        "requires_edge_highlight",
        json::Value{probe.requires_edge_highlight});
    out.emplace(
        "requires_noise_dither",
        json::Value{probe.requires_noise_dither});
    out.emplace(
        "excludes_foreground_text",
        json::Value{probe.excludes_foreground_text});
    out.emplace(
        "stage_count",
        json::Value{static_cast<std::int64_t>(probe.stage_count)});
    out.emplace(
        "sample_taps",
        json::Value{static_cast<std::int64_t>(probe.sample_taps)});
    return json::Value{std::move(out)};
}

inline json::Value glass_probe_contract_debug_json(State const& state) {
    auto const contract = glass_probe_contract(state);
    json::Object probes;
    for (auto const& probe : k_material_probes) {
        if (glass_probe_is_active(probe, state)) {
            probes.emplace(
                std::string{probe.name},
                glass_material_probe_debug_json(probe));
        }
    }

    json::Object out;
    out.emplace(
        "contract_name",
        json::Value{std::string{contract.contract_name}});
    out.emplace(
        "schema_version",
        json::Value{static_cast<std::int64_t>(contract.schema_version)});
    out.emplace(
        "reference_technology",
        json::Value{std::string{contract.reference_technology}});
    out.emplace(
        "reference_material_policy",
        json::Value{std::string{contract.reference_material_policy}});
    out.emplace(
        "reference_guidance",
        json::Value{std::string{contract.reference_guidance}});
    out.emplace(
        "backdrop_sampling_contract",
        json::Value{std::string{contract.backdrop_sampling_contract}});
    out.emplace(
        "fallback_contract",
        json::Value{std::string{contract.fallback_contract}});
    out.emplace(
        "pixel_contract",
        json::Value{std::string{contract.pixel_contract}});
    out.emplace(
        "material_probe_count",
        json::Value{static_cast<std::int64_t>(
            contract.material_probe_count)});
    out.emplace(
        "base_material_probe_count",
        json::Value{static_cast<std::int64_t>(
            contract.base_material_probe_count)});
    out.emplace(
        "inspector_material_probe_count",
        json::Value{static_cast<std::int64_t>(
            contract.inspector_material_probe_count)});
    out.emplace(
        "active_material_probe_count",
        json::Value{static_cast<std::int64_t>(
            contract.active_material_probe_count)});
    out.emplace(
        "stage_count_per_probe",
        json::Value{static_cast<std::int64_t>(
            contract.stage_count_per_probe)});
    out.emplace(
        "sample_taps_per_probe",
        json::Value{static_cast<std::int64_t>(
            contract.sample_taps_per_probe)});
    out.emplace("max_blur_radius", json::Value{contract.max_blur_radius});
    out.emplace(
        "total_expected_execution_stages",
        json::Value{static_cast<std::int64_t>(
            contract.total_expected_execution_stages)});
    out.emplace(
        "total_expected_sample_taps",
        json::Value{static_cast<std::int64_t>(
            contract.total_expected_sample_taps)});
    out.emplace("material_probes", json::Value{std::move(probes)});
    return json::Value{std::move(out)};
}

inline json::Value glass_state_debug_json(State const& state) {
    json::Object viewport;
    viewport.emplace(
        "width",
        json::Value{static_cast<std::int64_t>(state.viewport_width)});
    viewport.emplace(
        "height",
        json::Value{static_cast<std::int64_t>(state.viewport_height)});
    viewport.emplace("scale", json::Value{state.viewport_scale});

    json::Object out;
    out.emplace(
        "backdrop",
        json::Value{std::string{
            state.high_contrast_backdrop ? "high" : "standard"}});
    out.emplace(
        "high_contrast_backdrop",
        json::Value{state.high_contrast_backdrop});
    out.emplace(
        "inspector",
        json::Value{std::string{state.inspector_open ? "open" : "closed"}});
    out.emplace("inspector_open", json::Value{state.inspector_open});
    out.emplace(
        "density_index",
        json::Value{static_cast<std::int64_t>(state.selected_density)});
    out.emplace("note", json::Value{state.note});
    out.emplace("viewport", json::Value{std::move(viewport)});
    out.emplace(
        "expected_material_plan_count",
        json::Value{static_cast<std::int64_t>(
            expected_material_probe_count(state))});
    return json::Value{std::move(out)};
}

inline json::Value glass_showcase_application_debug_json(State const& state) {
    json::Object showcase;
    showcase.emplace("schema_version", json::Value{1});
    showcase.emplace("kind", json::Value{"glass_showcase"});
    showcase.emplace("state", glass_state_debug_json(state));
    showcase.emplace("probe_contract", glass_probe_contract_debug_json(state));

    json::Object out;
    out.emplace("glass_showcase", json::Value{std::move(showcase)});
    return json::Value{std::move(out)};
}

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
    ExecutionStages,
    SampleTaps,
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
    std::size_t execution_stages = 0;
    std::size_t sample_taps = 0;
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
    return expected_material_probe_count(state);
}

inline std::vector<std::string> public_material_kinds() {
    return {"clear", "thin", "regular", "thick"};
}

inline Action note_changed(std::string text) {
    return NoteChanged{std::move(text)};
}

inline Action select_density(std::size_t value) {
    return SelectDensity{value};
}

inline Action resized(int width, int height, float scale) {
    return Resized{width, height, scale};
}

inline void apply_action(State& state, Action action) {
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
    }, std::move(action));
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
            apply_action(state, ToggleBackdrop{});
            return;
        case GlassInputKind::SetBackdrop:
            apply_action(state, SetBackdropContrast{.high_contrast = input.flag});
            return;
        case GlassInputKind::ToggleInspector:
            apply_action(state, ToggleInspector{});
            return;
        case GlassInputKind::SetInspector:
            apply_action(state, SetInspector{.open = input.flag});
            return;
        case GlassInputKind::SelectDensity:
            apply_action(state, SelectDensity{.value = input.density});
            return;
        case GlassInputKind::Note:
            apply_action(state, NoteChanged{.text = input.value});
            return;
        case GlassInputKind::Viewport:
            apply_action(state, Resized{
                .width = input.viewport_width,
                .height = input.viewport_height,
                .scale = input.viewport_scale,
            });
            return;
        case GlassInputKind::Reset:
            apply_action(state, Reset{});
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
    if (name == "execution-stages" || name == "execution_stages"
        || name == "stage-count" || name == "stage_count") {
        auto count = parse_positive_int(value);
        if (!count)
            return expectation_parse_error(
                "expectation 'execution-stages' must be a positive integer");
        return parsed_expectation({
            .kind = GlassExpectationKind::ExecutionStages,
            .value = std::to_string(*count),
            .execution_stages = static_cast<std::size_t>(*count),
        });
    }
    if (name == "sample-taps" || name == "sample_taps"
        || name == "tap-count" || name == "tap_count") {
        auto count = parse_positive_int(value);
        if (!count)
            return expectation_parse_error(
                "expectation 'sample-taps' must be a positive integer");
        return parsed_expectation({
            .kind = GlassExpectationKind::SampleTaps,
            .value = std::to_string(*count),
            .sample_taps = static_cast<std::size_t>(*count),
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
        case GlassExpectationKind::ExecutionStages: return "execution-stages";
        case GlassExpectationKind::SampleTaps: return "sample-taps";
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
        case GlassExpectationKind::ExecutionStages:
            checked.actual =
                std::to_string(expected_material_execution_stage_count(state));
            checked.ok = expected_material_execution_stage_count(state)
                == expectation.execution_stages;
            checked.detail = checked.ok
                ? "material execution stage budget matched"
                : "material execution stage budget did not match";
            return checked;
        case GlassExpectationKind::SampleTaps:
            checked.actual =
                std::to_string(expected_material_sample_tap_count(state));
            checked.ok = expected_material_sample_tap_count(state)
                == expectation.sample_taps;
            checked.detail = checked.ok
                ? "material sample tap budget matched"
                : "material sample tap budget did not match";
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

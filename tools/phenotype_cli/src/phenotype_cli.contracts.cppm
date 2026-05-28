export module phenotype_cli.contracts;

import cppx.cli;
import cppx.terminal;
import phenotype_cli.common;
import phenotype.io;
import phenotype.theme_contract;
import std;

namespace phenotype_cli::contracts {

namespace io_contract = phenotype::io;
namespace theme_contract = phenotype::theme_contract;
using namespace phenotype_cli::common;

auto theme_color_json(theme_contract::ColorToken const& color) -> std::string {
    return std::format(
        "{{\"r\":{},\"g\":{},\"b\":{},\"a\":{}}}",
        static_cast<int>(color.r),
        static_cast<int>(color.g),
        static_cast<int>(color.b),
        static_cast<int>(color.a));
}

auto theme_radius_json(theme_contract::RadiusTokens const& radius)
        -> std::string {
    return std::format(
        "{{\"xs\":{},\"sm\":{},\"md\":{},\"lg\":{},\"full\":{}}}",
        radius.xs,
        radius.sm,
        radius.md,
        radius.lg,
        radius.full);
}

auto theme_typography_json(theme_contract::TypographyTokens const& typography)
        -> std::string {
    return std::format(
        "{{\"default_font_family\":{},\"body_font_size\":{},"
        "\"heading_font_size\":{},\"small_font_size\":{},"
        "\"line_height_ratio\":{}}}",
        json_string(typography.default_font_family),
        typography.body_font_size,
        typography.heading_font_size,
        typography.small_font_size,
        typography.line_height_ratio);
}

auto theme_contract_checks() -> std::vector<Check> {
    auto const contract = theme_contract::default_glass_theme_contract();
    auto const roles = theme_contract::glass_surface_roles();
    bool const has_toolbar =
        std::ranges::find(roles, std::string_view{"toolbar"}) != roles.end();
    bool const has_sidebar =
        std::ranges::find(roles, std::string_view{"sidebar"}) != roles.end();
    bool const has_status_bar =
        std::ranges::find(roles, std::string_view{"status_bar"}) != roles.end();
    return {
        {.name = "theme_profile",
         .ok = contract.profile_name == std::string_view{"apple-glass-light"}
            && contract.reference.find("Apple HIG Materials")
                != std::string_view::npos
            && contract.reference.find("Liquid Glass")
                != std::string_view::npos,
         .detail = std::string{contract.reference},
         .hint =
             "Keep the default theme anchored to Apple HIG Materials and Liquid Glass without claiming private API parity."},
        {.name = "pretendard_typography",
         .ok = contract.typography.default_font_family
                == std::string_view{"Pretendard"}
            && contract.font_policy.find("Pretendard")
                != std::string_view::npos
            && contract.typography.line_height_ratio >= 1.5f,
         .detail = std::format(
             "font={} body={} small={} line_height={}",
             contract.typography.default_font_family,
             contract.typography.body_font_size,
             contract.typography.small_font_size,
             contract.typography.line_height_ratio),
         .hint =
             "Default UI typography should prefer Pretendard and keep a readable line-height baseline."},
        {.name = "macos_icon_language",
         .ok = contract.iconography_policy.find("macos_finder")
                != std::string_view::npos
            && contract.iconography_policy.find("simplified_universal_metaphors")
                != std::string_view::npos
            && contract.icon_asset_policy.find("phenotype_owned_svg_symbols")
                != std::string_view::npos
            && contract.icon_asset_policy.find("audited_permissive_svg_sources")
                != std::string_view::npos
            && contract.icon_asset_policy.find("without_embedded_apple_artwork")
                != std::string_view::npos,
         .detail = std::format(
             "{}; {}",
             contract.iconography_policy,
             contract.icon_asset_policy),
         .hint =
             "Default icon language should follow macOS/Finder and SF Symbols semantics while using audited permissive SVG artwork."},
        {.name = "apple_glass_color_tokens",
         .ok = contract.background
                == theme_contract::ColorToken{242, 242, 247, 255}
            && contract.foreground
                == theme_contract::ColorToken{28, 28, 30, 255}
            && contract.accent
                == theme_contract::ColorToken{0, 122, 255, 255}
            && contract.surface.a < 255,
         .detail = std::format(
             "background=#{:02x}{:02x}{:02x} accent=#{:02x}{:02x}{:02x} surface_alpha={}",
             contract.background.r,
             contract.background.g,
             contract.background.b,
             contract.accent.r,
             contract.accent.g,
             contract.accent.b,
             contract.surface.a),
         .hint =
             "Default glass tokens should stay neutral, system-blue accented, and translucent where used as material tint."},
        {.name = "glass_usage_boundary",
         .ok = contract.material_policy.find("pure material planner")
                != std::string_view::npos
            && contract.usage_policy.find("navigation_controls")
                != std::string_view::npos
            && contract.usage_policy.find("not_content_fill")
                != std::string_view::npos,
         .detail = std::string{contract.usage_policy},
         .hint =
             "Liquid-glass styling belongs to functional chrome/navigation controls, not blanket content fills."},
        {.name = "container_and_role_contract",
         .ok = has_toolbar && has_sidebar && has_status_bar
            && contract.container_policy.find("explicit_container_spacing")
                != std::string_view::npos,
         .detail = std::format("roles={} container={}",
                               roles.size(),
                               contract.container_policy),
         .hint =
             "Theme metadata should expose role names and grouped-container spacing for artifact checks."},
        {.name = "performance_and_fallback_contract",
         .ok = contract.performance_policy.find("bounded_glass_surfaces")
                != std::string_view::npos
            && contract.fallback_policy.find("unsupported_backends")
                != std::string_view::npos
            && contract.accessibility_policy.find("reduced_transparency")
                != std::string_view::npos,
         .detail = std::format(
             "{}; {}; {}",
             contract.performance_policy,
             contract.accessibility_policy,
             contract.fallback_policy),
         .hint =
             "Theme metadata must keep resource bounds, accessibility fallbacks, and unsupported backend degradation explicit."},
    };
}

auto theme_contract_json(std::span<Check const> checks) -> std::string {
    auto const contract = theme_contract::default_glass_theme_contract();
    auto const roles = theme_contract::glass_surface_roles();
    return std::format(
        "{{\"schema_version\":1,\"command\":\"theme contract\","
        "\"ok\":{},\"version\":{},"
        "\"profile_name\":{},\"reference\":{},"
        "\"policies\":{{\"font\":{},\"material\":{},"
        "\"iconography\":{},\"icon_asset\":{},\"usage\":{},"
        "\"container\":{},\"performance\":{},\"accessibility\":{},"
        "\"fallback\":{}}},"
        "\"tokens\":{{\"background\":{},\"foreground\":{},\"accent\":{},"
        "\"accent_strong\":{},\"muted\":{},\"border\":{},"
        "\"surface\":{},\"hover_background\":{},"
        "\"disabled_foreground\":{},\"focus_ring\":{},"
        "\"radius\":{},\"typography\":{}}},"
        "\"surface_roles\":{},\"checks\":{}}}",
        all_ok(checks) ? "true" : "false",
        theme_contract::theme_contract_version,
        json_string(contract.profile_name),
        json_string(contract.reference),
        json_string(contract.font_policy),
        json_string(contract.material_policy),
        json_string(contract.iconography_policy),
        json_string(contract.icon_asset_policy),
        json_string(contract.usage_policy),
        json_string(contract.container_policy),
        json_string(contract.performance_policy),
        json_string(contract.accessibility_policy),
        json_string(contract.fallback_policy),
        theme_color_json(contract.background),
        theme_color_json(contract.foreground),
        theme_color_json(contract.accent),
        theme_color_json(contract.accent_strong),
        theme_color_json(contract.muted),
        theme_color_json(contract.border),
        theme_color_json(contract.surface),
        theme_color_json(contract.hover_background),
        theme_color_json(contract.disabled_foreground),
        theme_color_json(contract.focus_ring),
        theme_radius_json(contract.radius),
        theme_typography_json(contract.typography),
        string_view_array_json(roles),
        checks_json(checks));
}

auto parse_float_option(std::string_view text,
                        std::string_view option_name)
        -> std::expected<float, std::string> {
    if (text.empty()) {
        return std::unexpected{
            std::format("--{} requires a numeric value", option_name)};
    }
    float value = 0.0f;
    auto first = text.data();
    auto last = text.data() + text.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last || !std::isfinite(value)) {
        return std::unexpected{
            std::format("--{} must be a finite number", option_name)};
    }
    return value;
}

auto apply_float_option(cppx::cli::Invocation const& invocation,
                        std::string_view name,
                        float& target) -> std::expected<void, std::string> {
    auto value = invocation.value(std::string{name});
    if (!value)
        return {};
    auto parsed = parse_float_option(*value, name);
    if (!parsed)
        return std::unexpected{parsed.error()};
    target = *parsed;
    return {};
}

auto theme_preference_base_json(theme_contract::ThemePreferenceBase const& base)
        -> std::string {
    return std::format(
        "{{\"default_font_family\":{},\"body_font_size\":{},"
        "\"heading_font_size\":{},\"small_font_size\":{},"
        "\"line_height_ratio\":{},\"scroll_delta_multiplier\":{},"
        "\"scroll_horizontal_delta_multiplier\":{},"
        "\"scroll_bar_visibility\":{}}}",
        json_string(base.default_font_family),
        base.body_font_size,
        base.heading_font_size,
        base.small_font_size,
        base.line_height_ratio,
        base.scroll_delta_multiplier,
        base.scroll_horizontal_delta_multiplier,
        json_string(base.scroll_bar_visibility));
}

auto system_theme_preferences_json(
        theme_contract::SystemThemePreferenceSnapshot const& system)
        -> std::string {
    return std::format(
        "{{\"font_family\":{},\"body_font_size\":{},"
        "\"heading_font_size\":{},\"small_font_size\":{},"
        "\"line_height_ratio\":{},\"font_scale\":{},"
        "\"scroll_delta_multiplier\":{},"
        "\"scroll_horizontal_delta_multiplier\":{},"
        "\"color_scheme\":{},\"font_family_available\":{},"
        "\"font_metrics_available\":{},\"font_scale_available\":{},"
        "\"line_height_available\":{},\"scroll_metrics_available\":{},"
        "\"color_scheme_available\":{},\"accent_color_available\":{},"
        "\"accent_color\":{}}}",
        json_string(system.font_family),
        system.body_font_size,
        system.heading_font_size,
        system.small_font_size,
        system.line_height_ratio,
        system.font_scale,
        system.scroll_delta_multiplier,
        system.scroll_horizontal_delta_multiplier,
        json_string(system.color_scheme),
        system.font_family_available ? "true" : "false",
        system.font_metrics_available ? "true" : "false",
        system.font_scale_available ? "true" : "false",
        system.line_height_available ? "true" : "false",
        system.scroll_metrics_available ? "true" : "false",
        system.color_scheme_available ? "true" : "false",
        system.accent_color_available ? "true" : "false",
        theme_color_json(system.accent_color));
}

auto theme_preference_overrides_json(
        theme_contract::ThemePreferenceOverrides const& overrides)
        -> std::string {
    return std::format(
        "{{\"font_family\":{},\"color_scheme\":{},\"font_scale\":{},"
        "\"body_font_size\":{},\"heading_font_size\":{},"
        "\"small_font_size\":{},\"line_height_ratio\":{},"
        "\"scroll_delta_multiplier\":{},"
        "\"scroll_horizontal_delta_multiplier\":{},"
        "\"scroll_bar_visibility\":{},"
        "\"prefer_system_font_family\":{},"
        "\"prefer_system_color_scheme\":{},"
        "\"apply_system_font_metrics\":{},"
        "\"apply_system_font_scale\":{},"
        "\"apply_system_scroll_metrics\":{},"
        "\"apply_system_accent_color\":{}}}",
        json_string(overrides.font_family),
        json_string(overrides.color_scheme),
        overrides.font_scale,
        overrides.body_font_size,
        overrides.heading_font_size,
        overrides.small_font_size,
        overrides.line_height_ratio,
        overrides.scroll_delta_multiplier,
        overrides.scroll_horizontal_delta_multiplier,
        json_string(overrides.scroll_bar_visibility),
        overrides.prefer_system_font_family ? "true" : "false",
        overrides.prefer_system_color_scheme ? "true" : "false",
        overrides.apply_system_font_metrics ? "true" : "false",
        overrides.apply_system_font_scale ? "true" : "false",
        overrides.apply_system_scroll_metrics ? "true" : "false",
        overrides.apply_system_accent_color ? "true" : "false");
}

auto resolved_theme_preferences_json(
        theme_contract::ResolvedThemePreferences const& resolved)
        -> std::string {
    return std::format(
        "{{\"source\":{},\"requested_color_scheme\":{},"
        "\"effective_theme\":{{\"font_family\":{},"
        "\"color_scheme\":{},\"body_font_size\":{},"
        "\"heading_font_size\":{},\"small_font_size\":{},"
        "\"line_height_ratio\":{},\"scroll_delta_multiplier\":{},"
        "\"scroll_horizontal_delta_multiplier\":{},"
        "\"scroll_bar_visibility\":{}}},"
        "\"resolution\":{{\"used_user_font_family\":{},"
        "\"used_system_font_family\":{},"
        "\"used_user_color_scheme\":{},"
        "\"used_system_color_scheme\":{},"
        "\"used_system_font_metrics\":{},"
        "\"used_system_font_scale\":{},"
        "\"used_user_font_scale\":{},"
        "\"used_user_font_size\":{},"
        "\"used_system_line_height\":{},"
        "\"used_user_line_height\":{},"
        "\"used_system_scroll_metrics\":{},"
        "\"used_user_scroll_scale\":{},"
        "\"used_user_scroll_bar_visibility\":{},"
        "\"used_system_accent_color\":{}}}}}",
        json_string(resolved.source),
        json_string(resolved.requested_color_scheme),
        json_string(resolved.effective_font_family),
        json_string(resolved.effective_color_scheme),
        resolved.effective_body_font_size,
        resolved.effective_heading_font_size,
        resolved.effective_small_font_size,
        resolved.effective_line_height_ratio,
        resolved.effective_scroll_delta_multiplier,
        resolved.effective_scroll_horizontal_delta_multiplier,
        json_string(resolved.effective_scroll_bar_visibility),
        resolved.used_user_font_family ? "true" : "false",
        resolved.used_system_font_family ? "true" : "false",
        resolved.used_user_color_scheme ? "true" : "false",
        resolved.used_system_color_scheme ? "true" : "false",
        resolved.used_system_font_metrics ? "true" : "false",
        resolved.used_system_font_scale ? "true" : "false",
        resolved.used_user_font_scale ? "true" : "false",
        resolved.used_user_font_size ? "true" : "false",
        resolved.used_system_line_height ? "true" : "false",
        resolved.used_user_line_height ? "true" : "false",
        resolved.used_system_scroll_metrics ? "true" : "false",
        resolved.used_user_scroll_scale ? "true" : "false",
        resolved.used_user_scroll_bar_visibility ? "true" : "false",
        resolved.used_system_accent_color ? "true" : "false");
}

auto theme_resolve_inputs(cppx::cli::Invocation const& invocation)
        -> std::expected<
            std::tuple<
                theme_contract::ThemePreferenceBase,
                theme_contract::SystemThemePreferenceSnapshot,
                theme_contract::ThemePreferenceOverrides,
                std::string>,
            std::string> {
    using ThemeResolveInputs = std::tuple<
        theme_contract::ThemePreferenceBase,
        theme_contract::SystemThemePreferenceSnapshot,
        theme_contract::ThemePreferenceOverrides,
        std::string>;
    auto base = theme_contract::theme_preference_base(
        theme_contract::default_glass_theme_contract());
    auto system = theme_contract::SystemThemePreferenceSnapshot{};
    auto overrides = theme_contract::ThemePreferenceOverrides{};
    auto source = std::string{
        invocation.value("source").value_or("cli-theme-resolve")};

    if (auto value = invocation.value("system-font-family"))
        system.font_family = *value;
    if (auto value = invocation.value("system-color-scheme"))
        system.color_scheme = *value;
    if (auto value = invocation.value("font-family"))
        overrides.font_family = *value;
    if (auto value = invocation.value("color-scheme"))
        overrides.color_scheme = *value;
    if (auto value = invocation.value("scroll-bar-visibility"))
        overrides.scroll_bar_visibility =
            theme_contract::normalized_scroll_bar_visibility(*value);

    for (auto [name, target] : {
             std::pair<std::string_view, float*>{
                 "system-font-scale", &system.font_scale},
             {"system-body-font-size", &system.body_font_size},
             {"system-heading-font-size", &system.heading_font_size},
             {"system-small-font-size", &system.small_font_size},
             {"system-line-height", &system.line_height_ratio},
             {"system-scroll", &system.scroll_delta_multiplier},
             {"system-horizontal-scroll",
              &system.scroll_horizontal_delta_multiplier},
             {"font-scale", &overrides.font_scale},
             {"body-font-size", &overrides.body_font_size},
             {"heading-font-size", &overrides.heading_font_size},
             {"small-font-size", &overrides.small_font_size},
             {"line-height", &overrides.line_height_ratio},
             {"scroll", &overrides.scroll_delta_multiplier},
             {"horizontal-scroll",
              &overrides.scroll_horizontal_delta_multiplier},
         }) {
        auto applied = apply_float_option(invocation, name, *target);
        if (!applied)
            return std::unexpected{applied.error()};
    }
    system.font_scale_available = invocation.value("system-font-scale")
        .has_value();
    system.font_family_available = invocation.value("system-font-family")
        .has_value();
    system.font_metrics_available =
        invocation.value("system-body-font-size").has_value()
        || invocation.value("system-heading-font-size").has_value()
        || invocation.value("system-small-font-size").has_value();
    system.line_height_available = invocation.value("system-line-height")
        .has_value();
    system.scroll_metrics_available =
        invocation.value("system-scroll").has_value()
        || invocation.value("system-horizontal-scroll").has_value();
    system.color_scheme_available = invocation.value("system-color-scheme")
        .has_value();

    overrides.prefer_system_font_family =
        invocation.has("prefer-system-font-family");
    overrides.prefer_system_color_scheme =
        invocation.has("prefer-system-color-scheme");
    overrides.apply_system_font_metrics =
        !invocation.has("no-system-font-metrics");
    overrides.apply_system_font_scale =
        !invocation.has("no-system-font-scale");
    overrides.apply_system_scroll_metrics =
        !invocation.has("no-system-scroll-metrics");
    overrides.apply_system_accent_color =
        invocation.has("apply-system-accent");
    system.accent_color_available = invocation.has("apply-system-accent");
    return ThemeResolveInputs{
        base, system, overrides, std::move(source)};
}

auto theme_resolve_json(
        theme_contract::ThemePreferenceBase const& base,
        theme_contract::SystemThemePreferenceSnapshot const& system,
        theme_contract::ThemePreferenceOverrides const& overrides,
        theme_contract::ResolvedThemePreferences const& resolved)
        -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"theme resolve\","
        "\"ok\":true,\"contract_version\":{},"
        "\"policy\":{},\"input\":{{\"base\":{},\"system\":{},"
        "\"app_overrides\":{}}},\"resolved\":{}}}",
        theme_contract::theme_contract_version,
        json_string(
            "pure theme preference resolution: explicit app overrides win, "
            "then opted-in OS settings, then package defaults"),
        theme_preference_base_json(base),
        system_theme_preferences_json(system),
        theme_preference_overrides_json(overrides),
        resolved_theme_preferences_json(resolved));
}

auto input_event_kinds_json() -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < io_contract::input_event_kinds.size(); ++i) {
        if (i > 0)
            out += ",";
        out += json_string(io_contract::input_event_kind_name(
            io_contract::input_event_kinds[i]));
    }
    out += "]";
    return out;
}

auto output_observation_kinds_json() -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < io_contract::output_observation_kinds.size();
         ++i) {
        if (i > 0)
            out += ",";
        out += json_string(io_contract::output_observation_kind_name(
            io_contract::output_observation_kinds[i]));
    }
    out += "]";
    return out;
}

struct IoContractSample {
    io_contract::InputScript script;
    io_contract::ArtifactBundleDescriptor bundle;
};

auto io_contract_sample() -> IoContractSample {
    auto frame = io_contract::InputFrame{
        .frame_index = 1,
        .timestamp_ms = 16,
        .deterministic = true,
    };
    frame.events.push_back(io_contract::InputEvent{
        .sequence = 1,
        .payload = io_contract::InputViewportEvent{
            .width = 900,
            .height = 620,
            .scale = 2.0f,
        },
    });
    frame.events.push_back(io_contract::InputEvent{
        .sequence = 2,
        .payload = io_contract::InputPointerEvent{
            .phase = io_contract::PointerPhase::Down,
            .pointer_id = 1,
            .x = 48.0f,
            .y = 52.0f,
            .buttons = 1,
        },
    });

    auto script = io_contract::InputScript{
        .source_name = "cli-contract-sample",
        .deterministic = true,
    };
    script.frames.push_back(std::move(frame));

    auto observation = io_contract::OutputObservation{
        .semantic_tree_present = true,
        .command_stream_present = true,
        .material_plans_present = true,
        .runtime_summary_present = true,
        .machine_readable_failure_shape = true,
        .semantic_node_count = 3,
        .command_stream = {.command_count = 12,
                           .path_count = 2,
                           .material_count = 1,
                           .text_count = 4,
                           .image_count = 1,
                           .bounded = true},
        .material = {.plan_count = 1,
                     .fallback_count = 0,
                     .runtime_pass_count = 2,
                     .semantic_runtime_match = true},
        .likely_layers = {"semantic_tree", "material_plan"},
        .likely_passes = {"input_replay", "render_observation"},
    };
    observation.pixel_regions.push_back(io_contract::OutputPixelRegionSummary{
        .name = "finder-toolbar-probe",
        .luma_delta = 0.18f,
        .sampled_pixels = 64,
        .unique_colors = 14,
        .likely_layer = "material_plan",
        .likely_pass = "render_observation",
    });

    return {
        .script = std::move(script),
        .bundle = {
            .snapshot_json = true,
            .frame_image = true,
            .platform_runtime_details = true,
            .observation = std::move(observation),
        },
    };
}

auto io_contract_checks(IoContractSample const& sample) -> std::vector<Check> {
    auto const event_count = io_contract::input_script_event_count(sample.script);
    auto const replayable = io_contract::input_script_is_replayable(sample.script);
    auto const artifact_debuggable =
        io_contract::artifact_bundle_is_llm_debuggable(sample.bundle);

    return {
        {.name = "contract_version",
         .ok = io_contract::io_contract_version == 1,
         .detail = std::format("version={}", io_contract::io_contract_version),
         .hint = "Bump the CLI schema when the pure IO contract changes."},
        {.name = "input_event_surface",
         .ok = io_contract::input_event_kinds.size() == 7
            && io_contract::input_event_kind_name(
                   io_contract::InputEventKind::Pointer)
                == std::string_view{"pointer"}
            && io_contract::input_event_kind(
                   sample.script.frames.front().events.front())
                == io_contract::InputEventKind::Viewport,
         .detail = std::format(
             "kinds={} first_sample={}",
             io_contract::input_event_kinds.size(),
             io_contract::input_event_kind_name(io_contract::input_event_kind(
                 sample.script.frames.front().events.front()))),
         .hint =
             "Native and CLI adapters must lower platform input to typed event values."},
        {.name = "deterministic_replay",
         .ok = replayable && event_count == 2,
         .detail = std::format(
             "frames={} events={} replayable={}",
             sample.script.frames.size(),
             event_count,
             replayable ? "true" : "false"),
         .hint =
             "Every non-tick input event in a replay script needs a stable non-zero sequence."},
        {.name = "output_observation_surface",
         .ok = io_contract::output_observation_kinds.size() == 6
            && sample.bundle.observation.command_stream.bounded
            && sample.bundle.observation.material.semantic_runtime_match,
         .detail = std::format(
             "kinds={} semantic_nodes={} material_plans={}",
             io_contract::output_observation_kinds.size(),
             sample.bundle.observation.semantic_node_count,
             sample.bundle.observation.material.plan_count),
         .hint =
             "Renderer output must lower into bounded command/material/runtime summaries."},
        {.name = "llm_debuggable_artifact",
         .ok = artifact_debuggable,
         .detail = std::format(
             "snapshot={} frame={} runtime={} likely_layers={} likely_passes={}",
             sample.bundle.snapshot_json ? "true" : "false",
             sample.bundle.frame_image ? "true" : "false",
             sample.bundle.platform_runtime_details ? "true" : "false",
             sample.bundle.observation.likely_layers.size(),
             sample.bundle.observation.likely_passes.size()),
         .hint =
             "Artifact output must include semantic, runtime, machine-readable failure, likely layer, and likely pass data."},
        {.name = "edge_effect_boundary",
         .ok = io_contract::input_contract_policy().find("typed_input_frames")
                != std::string_view::npos
            && io_contract::output_contract_policy().find("typed_observations")
                != std::string_view::npos
            && io_contract::edge_effect_policy().find("filesystem")
                != std::string_view::npos
            && io_contract::edge_effect_policy().find("device")
                != std::string_view::npos
            && io_contract::production_bypass_policy().find("release_adapters")
                != std::string_view::npos,
         .detail = std::string{io_contract::edge_effect_policy()},
         .hint =
             "Filesystem, clocks, process control, capture, shaders, and devices must stay at CLI/backend edges."},
    };
}

auto io_contract_json(IoContractSample const& sample,
                      std::span<Check const> checks) -> std::string {
    auto const event_count = io_contract::input_script_event_count(sample.script);
    auto const replayable = io_contract::input_script_is_replayable(sample.script);
    auto const artifact_debuggable =
        io_contract::artifact_bundle_is_llm_debuggable(sample.bundle);

    return std::format(
        "{{\"schema_version\":1,\"command\":\"io contract\","
        "\"ok\":{},\"version\":{},"
        "\"policies\":{{\"input\":{},\"output\":{},"
        "\"edge_effects\":{},\"production_bypass\":{}}},"
        "\"input_event_kinds\":{},\"output_observation_kinds\":{},"
        "\"sample\":{{\"source\":{},\"frames\":{},\"events\":{},"
        "\"replayable\":{},\"artifact_llm_debuggable\":{},"
        "\"semantic_node_count\":{},\"pixel_region_count\":{}}},"
        "\"checks\":{}}}",
        all_ok(checks) ? "true" : "false",
        io_contract::io_contract_version,
        json_string(io_contract::input_contract_policy()),
        json_string(io_contract::output_contract_policy()),
        json_string(io_contract::edge_effect_policy()),
        json_string(io_contract::production_bypass_policy()),
        input_event_kinds_json(),
        output_observation_kinds_json(),
        json_string(sample.script.source_name),
        sample.script.frames.size(),
        event_count,
        replayable ? "true" : "false",
        artifact_debuggable ? "true" : "false",
        sample.bundle.observation.semantic_node_count,
        sample.bundle.observation.pixel_regions.size(),
        checks_json(checks));
}

export int run_theme_contract(cppx::cli::Invocation const& invocation) {
    auto checks = theme_contract_checks();
    auto const contract = theme_contract::default_glass_theme_contract();
    if (invocation.has("json")) {
        std::println("{}", theme_contract_json(checks));
        return all_ok(checks) ? 0 : 1;
    }

    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "profile",
         .value = std::string{contract.profile_name},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "font",
         .value = std::string{contract.font_policy},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "usage",
         .value = std::string{contract.usage_policy},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "icons",
         .value = std::string{contract.iconography_policy},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "container",
         .value = std::string{contract.container_policy},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "fallback",
         .value = std::string{contract.fallback_policy},
         .status = all_ok(checks) ? cppx::terminal::StatusKind::ok
                                  : cppx::terminal::StatusKind::fail},
    };
    std::println("phenotype theme contract");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    print_checks("checks", checks);
    return all_ok(checks) ? 0 : 1;
}

export int run_theme_resolve(cppx::cli::Invocation const& invocation) {
    auto inputs = theme_resolve_inputs(invocation);
    if (!inputs) {
        return print_error(
            "theme resolve",
            inputs.error(),
            invocation.has("json"));
    }
    auto const& [base, system, overrides, source] = *inputs;
    auto resolved = theme_contract::resolve_theme_preferences(
        base,
        system,
        overrides,
        source);

    if (invocation.has("json")) {
        std::println(
            "{}",
            theme_resolve_json(base, system, overrides, resolved));
        return 0;
    }

    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "source",
         .value = resolved.source,
         .status = cppx::terminal::StatusKind::ok},
        {.label = "font",
         .value = std::format(
             "{} body={} heading={} small={} line_height={}",
             resolved.effective_font_family,
             resolved.effective_body_font_size,
             resolved.effective_heading_font_size,
             resolved.effective_small_font_size,
             resolved.effective_line_height_ratio),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "appearance",
         .value = resolved.effective_color_scheme,
         .status = cppx::terminal::StatusKind::ok},
        {.label = "scroll",
         .value = std::format(
             "vertical={} horizontal={} bars={}",
             resolved.effective_scroll_delta_multiplier,
             resolved.effective_scroll_horizontal_delta_multiplier,
             resolved.effective_scroll_bar_visibility),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "system",
         .value = std::format(
             "font_metrics={} font_scale={} scroll_metrics={} accent={}",
             resolved.used_system_font_metrics ? "true" : "false",
             resolved.used_system_font_scale ? "true" : "false",
             resolved.used_system_scroll_metrics ? "true" : "false",
             resolved.used_system_accent_color ? "true" : "false"),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "user",
         .value = std::format(
             "font_family={} font_size={} font_scale={} scroll_scale={} scroll_bars={}",
             resolved.used_user_font_family ? "true" : "false",
             resolved.used_user_font_size ? "true" : "false",
             resolved.used_user_font_scale ? "true" : "false",
             resolved.used_user_scroll_scale ? "true" : "false",
             resolved.used_user_scroll_bar_visibility ? "true" : "false"),
         .status = cppx::terminal::StatusKind::ok},
    };
    std::println("phenotype theme resolve");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    return 0;
}

export int run_io_contract(cppx::cli::Invocation const& invocation) {
    auto sample = io_contract_sample();
    auto checks = io_contract_checks(sample);
    if (invocation.has("json")) {
        std::println("{}", io_contract_json(sample, checks));
        return all_ok(checks) ? 0 : 1;
    }

    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "version",
         .value = std::format("{}", io_contract::io_contract_version),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "input",
         .value = std::string{io_contract::input_contract_policy()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "output",
         .value = std::string{io_contract::output_contract_policy()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "edge effects",
         .value = std::string{io_contract::edge_effect_policy()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "debuggable",
         .value = io_contract::artifact_bundle_is_llm_debuggable(sample.bundle)
            ? "true" : "false",
         .status = all_ok(checks) ? cppx::terminal::StatusKind::ok
                                  : cppx::terminal::StatusKind::fail},
    };
    std::println("phenotype io contract");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    print_checks("checks", checks);
    return all_ok(checks) ? 0 : 1;
}

}

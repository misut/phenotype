module;
#include <array>
#include <cmath>
#include <string>
#include <string_view>
export module phenotype.theme_contract;

export namespace phenotype::theme_contract {

inline constexpr unsigned int theme_contract_version = 2;

struct ColorToken {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char a = 255;

    constexpr bool operator==(ColorToken const&) const = default;
};

struct RadiusTokens {
    float xs = 0.0f;
    float sm = 0.0f;
    float md = 0.0f;
    float lg = 0.0f;
    float full = 0.0f;

    constexpr bool operator==(RadiusTokens const&) const = default;
};

struct TypographyTokens {
    std::string_view default_font_family;
    float body_font_size = 0.0f;
    float heading_font_size = 0.0f;
    float small_font_size = 0.0f;
    float line_height_ratio = 0.0f;

    constexpr bool operator==(TypographyTokens const&) const = default;
};

struct DefaultGlassThemeContract {
    std::string_view profile_name;
    std::string_view reference;
    std::string_view font_policy;
    std::string_view material_policy;
    std::string_view iconography_policy;
    std::string_view icon_asset_policy;
    std::string_view usage_policy;
    std::string_view container_policy;
    std::string_view performance_policy;
    std::string_view accessibility_policy;
    std::string_view fallback_policy;
    ColorToken background;
    ColorToken foreground;
    ColorToken accent;
    ColorToken accent_strong;
    ColorToken muted;
    ColorToken border;
    ColorToken surface;
    ColorToken hover_background;
    ColorToken disabled_foreground;
    ColorToken focus_ring;
    RadiusTokens radius;
    TypographyTokens typography;
};

struct ThemePreferenceBase {
    std::string default_font_family = "Pretendard";
    float body_font_size = 16.0f;
    float heading_font_size = 22.4f;
    float small_font_size = 14.4f;
    float line_height_ratio = 1.6f;
    float scroll_delta_multiplier = 1.0f;
    float scroll_horizontal_delta_multiplier = 1.0f;
    float motion_duration_multiplier = 1.0f;
};

struct SystemThemePreferenceSnapshot {
    std::string font_family;
    float body_font_size = 0.0f;
    float heading_font_size = 0.0f;
    float small_font_size = 0.0f;
    float line_height_ratio = 1.6f;
    float font_scale = 1.0f;
    float scroll_delta_multiplier = 1.0f;
    float scroll_horizontal_delta_multiplier = 1.0f;
    std::string color_scheme = "light";
    bool font_family_available = false;
    bool font_metrics_available = false;
    bool font_scale_available = false;
    bool line_height_available = false;
    bool scroll_metrics_available = false;
    bool color_scheme_available = false;
    bool accent_color_available = false;
    ColorToken accent_color = {0, 122, 255, 255};
    bool reduce_motion = false;
    bool reduce_motion_available = false;
};

struct ThemePreferenceOverrides {
    std::string font_family;
    std::string color_scheme;
    float font_scale = 1.0f;
    float body_font_size = 0.0f;
    float heading_font_size = 0.0f;
    float small_font_size = 0.0f;
    float line_height_ratio = 0.0f;
    float scroll_delta_multiplier = 1.0f;
    float scroll_horizontal_delta_multiplier = 1.0f;
    bool prefer_system_font_family = false;
    bool prefer_system_color_scheme = false;
    bool apply_system_font_metrics = true;
    bool apply_system_font_scale = true;
    bool apply_system_scroll_metrics = true;
    bool apply_system_accent_color = false;
    bool apply_system_reduce_motion = true;
    float motion_duration_multiplier = 1.0f;
};

struct ResolvedThemePreferences {
    std::string source = "default";
    std::string requested_color_scheme;
    std::string effective_color_scheme = "light";
    std::string effective_font_family = "Pretendard";
    float effective_body_font_size = 16.0f;
    float effective_heading_font_size = 22.4f;
    float effective_small_font_size = 14.4f;
    float effective_line_height_ratio = 1.6f;
    float effective_scroll_delta_multiplier = 1.0f;
    float effective_scroll_horizontal_delta_multiplier = 1.0f;
    float effective_motion_duration_multiplier = 1.0f;
    bool used_user_font_family = false;
    bool used_system_font_family = false;
    bool used_user_color_scheme = false;
    bool used_system_color_scheme = false;
    bool used_system_font_metrics = false;
    bool used_system_font_scale = false;
    bool used_user_font_scale = false;
    bool used_user_font_size = false;
    bool used_system_line_height = false;
    bool used_user_line_height = false;
    bool used_system_scroll_metrics = false;
    bool used_user_scroll_scale = false;
    bool used_system_accent_color = false;
    bool used_system_reduce_motion = false;
    bool used_user_motion_scale = false;
};

inline auto default_glass_theme_contract() noexcept
    -> DefaultGlassThemeContract {
    return {
        .profile_name = "apple-glass-light",
        .reference = "Apple HIG Materials and Liquid Glass inspired portable tokens",
        .font_policy = "Pretendard primary with platform sans fallback",
        .material_policy =
            "pure material planner decides glass execution; theme provides portable tokens",
        .iconography_policy =
            "macos_finder_sf_symbols_aligned_rounded_outline_symbols_with_simplified_universal_metaphors",
        .icon_asset_policy =
            "phenotype_owned_svg_symbols_or_audited_permissive_svg_sources_reference_sf_symbols_semantics_without_embedded_apple_artwork",
        .usage_policy =
            "liquid_glass_for_navigation_controls_and_functional_chrome_not_content_fill",
        .container_policy =
            "group_related_glass_controls_with_explicit_container_spacing",
        .performance_policy =
            "bounded_glass_surfaces_and_backdrop_sampling_budgets",
        .accessibility_policy =
            "deterministic_reduced_transparency_and_contrast_fallbacks",
        .fallback_policy =
            "unsupported_backends_render_semantic_translucent_surfaces",
        .background = {242, 242, 247, 255},
        .foreground = {28, 28, 30, 255},
        .accent = {0, 122, 255, 255},
        .accent_strong = {0, 90, 190, 255},
        .muted = {99, 99, 102, 255},
        .border = {209, 209, 214, 255},
        .surface = {255, 255, 255, 238},
        .hover_background = {229, 229, 234, 255},
        .disabled_foreground = {142, 142, 147, 255},
        .focus_ring = {0, 122, 255, 255},
        .radius = {.xs = 6.0f, .sm = 10.0f, .md = 14.0f,
                   .lg = 22.0f, .full = 9999.0f},
        .typography = {.default_font_family = "Pretendard",
                       .body_font_size = 16.0f,
                       .heading_font_size = 22.4f,
                       .small_font_size = 14.4f,
                       .line_height_ratio = 1.6f},
    };
}

inline auto theme_preference_base(DefaultGlassThemeContract const& contract)
        -> ThemePreferenceBase {
    return {
        .default_font_family =
            std::string{contract.typography.default_font_family},
        .body_font_size = contract.typography.body_font_size,
        .heading_font_size = contract.typography.heading_font_size,
        .small_font_size = contract.typography.small_font_size,
        .line_height_ratio = contract.typography.line_height_ratio,
        .scroll_delta_multiplier = 1.0f,
        .scroll_horizontal_delta_multiplier = 1.0f,
        .motion_duration_multiplier = 1.0f,
    };
}

inline float bounded_theme_preference(
        float value,
        float fallback,
        float minimum,
        float maximum) noexcept {
    if (!(value > 0.0f) || !std::isfinite(value))
        return fallback;
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

inline float bounded_motion_duration_multiplier(
        float value,
        float fallback) noexcept {
    if (value < 0.0f || !std::isfinite(value))
        return fallback;
    if (value > 4.0f)
        return 4.0f;
    return value;
}

inline bool theme_color_scheme_is_dark(std::string_view scheme) noexcept {
    return scheme == "dark" || scheme == "high-contrast-dark";
}

inline bool theme_color_scheme_is_light(std::string_view scheme) noexcept {
    return scheme == "light" || scheme == "high-contrast-light";
}

inline auto resolve_theme_preferences(
        ThemePreferenceBase base,
        SystemThemePreferenceSnapshot const& system,
        ThemePreferenceOverrides const& overrides = {},
        std::string_view source = "default") -> ResolvedThemePreferences {
    ResolvedThemePreferences resolved;
    resolved.source = std::string{source};
    resolved.effective_font_family = base.default_font_family;
    resolved.effective_body_font_size = base.body_font_size;
    resolved.effective_heading_font_size = base.heading_font_size;
    resolved.effective_small_font_size = base.small_font_size;
    resolved.effective_line_height_ratio = base.line_height_ratio;
    resolved.effective_scroll_delta_multiplier =
        base.scroll_delta_multiplier;
    resolved.effective_scroll_horizontal_delta_multiplier =
        base.scroll_horizontal_delta_multiplier;
    resolved.effective_motion_duration_multiplier =
        base.motion_duration_multiplier;

    if (!overrides.font_family.empty()) {
        resolved.effective_font_family = overrides.font_family;
        resolved.used_user_font_family = true;
    } else if (overrides.prefer_system_font_family
               && system.font_family_available
               && !system.font_family.empty()) {
        resolved.effective_font_family = system.font_family;
        resolved.used_system_font_family = true;
    }

    std::string resolved_color_scheme;
    if (!overrides.color_scheme.empty()) {
        resolved_color_scheme = overrides.color_scheme;
        resolved.used_user_color_scheme = true;
    } else if (overrides.prefer_system_color_scheme
               && system.color_scheme_available) {
        resolved_color_scheme = system.color_scheme;
        resolved.used_system_color_scheme = true;
    }
    resolved.requested_color_scheme = resolved_color_scheme;
    if (theme_color_scheme_is_dark(resolved_color_scheme)
        || theme_color_scheme_is_light(resolved_color_scheme)) {
        resolved.effective_color_scheme = resolved_color_scheme.empty()
            ? "light"
            : resolved_color_scheme;
    } else {
        resolved.effective_color_scheme = "light";
        resolved.used_user_color_scheme = false;
        resolved.used_system_color_scheme = false;
    }

    bool const use_system_font_metrics =
        overrides.apply_system_font_metrics
        && system.font_metrics_available
        && system.body_font_size > 0.0f
        && std::isfinite(system.body_font_size);
    resolved.used_system_font_metrics = use_system_font_metrics;
    if (use_system_font_metrics) {
        float const body = bounded_theme_preference(
            system.body_font_size,
            resolved.effective_body_font_size,
            8.0f,
            40.0f);
        float const metric_scale = base.body_font_size > 0.0f
            ? body / base.body_font_size
            : 1.0f;
        resolved.effective_body_font_size = body;
        resolved.effective_heading_font_size = bounded_theme_preference(
            system.heading_font_size,
            resolved.effective_heading_font_size * metric_scale,
            10.0f,
            56.0f);
        resolved.effective_small_font_size = bounded_theme_preference(
            system.small_font_size,
            resolved.effective_small_font_size * metric_scale,
            8.0f,
            32.0f);
    }

    float scale = 1.0f;
    bool const use_system_font_scale =
        !use_system_font_metrics
        && overrides.apply_system_font_scale
        && system.font_scale_available
        && system.font_scale > 0.0f
        && std::isfinite(system.font_scale);
    if (use_system_font_scale) {
        auto const system_scale = bounded_theme_preference(
            system.font_scale,
            1.0f,
            0.75f,
            1.8f);
        scale *= system_scale;
        resolved.used_system_font_scale = true;
    }
    auto const user_scale = bounded_theme_preference(
        overrides.font_scale,
        1.0f,
        0.75f,
        1.8f);
    scale *= user_scale;
    resolved.used_user_font_scale =
        overrides.font_scale > 0.0f
        && std::isfinite(overrides.font_scale)
        && user_scale != 1.0f;

    resolved.effective_body_font_size *= scale;
    resolved.effective_heading_font_size *= scale;
    resolved.effective_small_font_size *= scale;

    if (overrides.body_font_size > 0.0f
        && std::isfinite(overrides.body_font_size)) {
        resolved.effective_body_font_size = bounded_theme_preference(
            overrides.body_font_size,
            resolved.effective_body_font_size,
            8.0f,
            40.0f);
        resolved.used_user_font_size = true;
    }
    if (overrides.heading_font_size > 0.0f
        && std::isfinite(overrides.heading_font_size)) {
        resolved.effective_heading_font_size = bounded_theme_preference(
            overrides.heading_font_size,
            resolved.effective_heading_font_size,
            10.0f,
            56.0f);
        resolved.used_user_font_size = true;
    }
    if (overrides.small_font_size > 0.0f
        && std::isfinite(overrides.small_font_size)) {
        resolved.effective_small_font_size = bounded_theme_preference(
            overrides.small_font_size,
            resolved.effective_small_font_size,
            8.0f,
            32.0f);
        resolved.used_user_font_size = true;
    }
    if (overrides.line_height_ratio > 0.0f
        && std::isfinite(overrides.line_height_ratio)) {
        resolved.effective_line_height_ratio = bounded_theme_preference(
            overrides.line_height_ratio,
            resolved.effective_line_height_ratio,
            1.0f,
            2.2f);
        resolved.used_user_line_height = true;
    } else if (system.line_height_available
               && system.line_height_ratio > 0.0f
               && std::isfinite(system.line_height_ratio)) {
        resolved.effective_line_height_ratio = bounded_theme_preference(
            system.line_height_ratio,
            resolved.effective_line_height_ratio,
            1.0f,
            2.2f);
        resolved.used_system_line_height = true;
    }

    bool const use_system_scroll_metrics =
        overrides.apply_system_scroll_metrics
        && system.scroll_metrics_available
        && ((system.scroll_delta_multiplier > 0.0f
             && std::isfinite(system.scroll_delta_multiplier))
            || (system.scroll_horizontal_delta_multiplier > 0.0f
                && std::isfinite(system.scroll_horizontal_delta_multiplier)));
    float scroll_scale = use_system_scroll_metrics
        ? bounded_theme_preference(
            system.scroll_delta_multiplier,
            base.scroll_delta_multiplier,
            0.25f,
            4.0f)
        : base.scroll_delta_multiplier;
    float horizontal_scroll_scale = use_system_scroll_metrics
        ? bounded_theme_preference(
            system.scroll_horizontal_delta_multiplier,
            scroll_scale,
            0.25f,
            4.0f)
        : base.scroll_horizontal_delta_multiplier;
    resolved.used_system_scroll_metrics = use_system_scroll_metrics;
    auto const app_scroll_scale = bounded_theme_preference(
        overrides.scroll_delta_multiplier,
        1.0f,
        0.25f,
        4.0f);
    auto const app_horizontal_scroll_scale = bounded_theme_preference(
        overrides.scroll_horizontal_delta_multiplier,
        1.0f,
        0.25f,
        4.0f);
    resolved.used_user_scroll_scale =
        app_scroll_scale != 1.0f || app_horizontal_scroll_scale != 1.0f;
    scroll_scale *= app_scroll_scale;
    horizontal_scroll_scale *= app_scroll_scale;
    horizontal_scroll_scale *= app_horizontal_scroll_scale;
    resolved.effective_scroll_delta_multiplier = bounded_theme_preference(
        scroll_scale,
        base.scroll_delta_multiplier,
        0.25f,
        4.0f);
    resolved.effective_scroll_horizontal_delta_multiplier =
        bounded_theme_preference(
            horizontal_scroll_scale,
            base.scroll_horizontal_delta_multiplier,
            0.25f,
            4.0f);
    resolved.used_system_accent_color =
        overrides.apply_system_accent_color
        && system.accent_color_available
        && system.accent_color.a > 0;
    if (overrides.apply_system_reduce_motion
        && system.reduce_motion_available
        && system.reduce_motion) {
        resolved.effective_motion_duration_multiplier = 0.0f;
        resolved.used_system_reduce_motion = true;
    }
    auto const app_motion_scale = bounded_motion_duration_multiplier(
        overrides.motion_duration_multiplier,
        1.0f);
    resolved.used_user_motion_scale = app_motion_scale != 1.0f;
    resolved.effective_motion_duration_multiplier =
        bounded_motion_duration_multiplier(
            resolved.effective_motion_duration_multiplier * app_motion_scale,
            base.motion_duration_multiplier);
    return resolved;
}

inline auto glass_surface_roles() noexcept
    -> std::array<std::string_view, 7> {
    return {
        "window",
        "toolbar",
        "toolbar_group",
        "navigation",
        "sidebar",
        "content",
        "status_bar",
    };
}

inline auto default_theme_profile_name() noexcept -> std::string_view {
    return default_glass_theme_contract().profile_name;
}

inline auto default_theme_reference() noexcept -> std::string_view {
    return default_glass_theme_contract().reference;
}

inline auto default_theme_font_policy() noexcept -> std::string_view {
    return default_glass_theme_contract().font_policy;
}

inline auto default_theme_material_policy() noexcept -> std::string_view {
    return default_glass_theme_contract().material_policy;
}

inline auto default_theme_iconography_policy() noexcept -> std::string_view {
    return default_glass_theme_contract().iconography_policy;
}

inline auto default_theme_icon_asset_policy() noexcept -> std::string_view {
    return default_glass_theme_contract().icon_asset_policy;
}

inline auto default_theme_usage_policy() noexcept -> std::string_view {
    return default_glass_theme_contract().usage_policy;
}

inline auto default_theme_container_policy() noexcept -> std::string_view {
    return default_glass_theme_contract().container_policy;
}

inline auto default_theme_performance_policy() noexcept -> std::string_view {
    return default_glass_theme_contract().performance_policy;
}

inline auto default_theme_accessibility_policy() noexcept -> std::string_view {
    return default_glass_theme_contract().accessibility_policy;
}

inline auto default_theme_fallback_policy() noexcept -> std::string_view {
    return default_glass_theme_contract().fallback_policy;
}

} // namespace phenotype::theme_contract

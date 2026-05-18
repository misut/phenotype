module;
#include <array>
#include <string_view>
export module phenotype.theme_contract;

export namespace phenotype::theme_contract {

inline constexpr unsigned int theme_contract_version = 1;

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

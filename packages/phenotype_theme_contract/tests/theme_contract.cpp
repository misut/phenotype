#include <cassert>
#include <cmath>
#include <cstdio>
#include <string_view>

import phenotype.theme_contract;

namespace theme = phenotype::theme_contract;

int main() {
    auto contract = theme::default_glass_theme_contract();
    assert(theme::theme_contract_version == 2);
    assert(contract.profile_name == "apple-glass-light");
    assert(contract.reference.find("Apple HIG Materials")
           != std::string_view::npos);
    assert(contract.reference.find("Liquid Glass")
           != std::string_view::npos);
    assert(contract.font_policy.find("Pretendard")
           != std::string_view::npos);
    assert(contract.material_policy.find("pure material planner")
           != std::string_view::npos);
    assert(contract.iconography_policy.find("macos_finder")
           != std::string_view::npos);
    assert(contract.iconography_policy.find("sf_symbols_aligned")
           != std::string_view::npos);
    assert(contract.iconography_policy.find("simplified_universal_metaphors")
           != std::string_view::npos);
    assert(contract.icon_asset_policy.find("phenotype_owned_svg_symbols")
           != std::string_view::npos);
    assert(contract.icon_asset_policy.find("audited_permissive_svg_sources")
           != std::string_view::npos);
    assert(contract.icon_asset_policy.find("without_embedded_apple_artwork")
           != std::string_view::npos);
    assert(contract.usage_policy.find("navigation_controls")
           != std::string_view::npos);
    assert(contract.usage_policy.find("not_content_fill")
           != std::string_view::npos);
    assert(contract.container_policy.find("explicit_container_spacing")
           != std::string_view::npos);
    assert(contract.performance_policy.find("bounded_glass_surfaces")
           != std::string_view::npos);
    assert(contract.accessibility_policy.find("reduced_transparency")
           != std::string_view::npos);
    assert(contract.fallback_policy.find("unsupported_backends")
           != std::string_view::npos);
    assert((contract.background == theme::ColorToken{242, 242, 247, 255}));
    assert((contract.foreground == theme::ColorToken{28, 28, 30, 255}));
    assert((contract.accent == theme::ColorToken{0, 122, 255, 255}));
    assert((contract.surface == theme::ColorToken{255, 255, 255, 238}));
    assert(contract.radius.sm == 10.0f);
    assert(contract.radius.md == 14.0f);
    assert(contract.radius.lg == 22.0f);
    assert(contract.typography.default_font_family == "Pretendard");
    assert(theme::glass_surface_roles().size() == 7);

    auto base = theme::theme_preference_base(contract);
    assert(base.scroll_bar_visibility == "auto");
    assert(theme::normalized_scroll_bar_visibility("visible") == "always");
    assert(theme::normalized_scroll_bar_visibility("off") == "hidden");
    auto default_resolved = theme::resolve_theme_preferences(base, {});
    assert(default_resolved.effective_scroll_bar_visibility == "auto");
    assert(!default_resolved.used_system_font_scale);
    assert(!default_resolved.used_system_line_height);
    assert(!default_resolved.used_system_scroll_metrics);
    assert(!default_resolved.used_system_color_scheme);
    theme::SystemThemePreferenceSnapshot unavailable_system{};
    unavailable_system.font_family = "Unavailable System UI";
    unavailable_system.body_font_size = 18.0f;
    unavailable_system.color_scheme = "dark";
    unavailable_system.font_scale = 1.4f;
    unavailable_system.line_height_ratio = 1.8f;
    unavailable_system.scroll_delta_multiplier = 2.0f;
    theme::ThemePreferenceOverrides unavailable_overrides{};
    unavailable_overrides.prefer_system_font_family = true;
    unavailable_overrides.prefer_system_color_scheme = true;
    auto unavailable_resolved = theme::resolve_theme_preferences(
        base,
        unavailable_system,
        unavailable_overrides);
    assert(unavailable_resolved.effective_color_scheme == "light");
    assert(unavailable_resolved.effective_font_family == "Pretendard");
    assert(!unavailable_resolved.used_system_font_family);
    assert(!unavailable_resolved.used_system_font_metrics);
    assert(!unavailable_resolved.used_system_color_scheme);
    assert(!unavailable_resolved.used_system_font_scale);
    assert(!unavailable_resolved.used_system_line_height);
    assert(!unavailable_resolved.used_system_scroll_metrics);

    theme::SystemThemePreferenceSnapshot system{};
    system.font_family = "System UI";
    system.font_scale = 1.25f;
    system.line_height_ratio = 1.7f;
    system.scroll_delta_multiplier = 1.25f;
    system.scroll_horizontal_delta_multiplier = 0.5f;
    system.color_scheme = "dark";
    system.font_family_available = true;
    system.font_metrics_available = true;
    system.font_scale_available = true;
    system.line_height_available = true;
    system.scroll_metrics_available = true;
    system.color_scheme_available = true;
    system.accent_color_available = true;
    system.accent_color = {88, 86, 214, 255};
    system.reduce_motion = true;
    system.reduce_motion_available = true;

    theme::ThemePreferenceOverrides overrides{};
    overrides.prefer_system_font_family = true;
    overrides.prefer_system_color_scheme = true;
    overrides.apply_system_font_metrics = false;
    overrides.apply_system_accent_color = true;
    overrides.font_scale = 1.2f;
    overrides.scroll_delta_multiplier = 1.5f;
    overrides.scroll_horizontal_delta_multiplier = 2.0f;
    overrides.scroll_bar_visibility = "hidden";

    auto resolved = theme::resolve_theme_preferences(
        base,
        system,
        overrides,
        "test-theme-resolve");
    assert(resolved.source == "test-theme-resolve");
    assert(resolved.effective_font_family == "System UI");
    assert(resolved.effective_color_scheme == "dark");
    assert(resolved.used_system_font_family);
    assert(resolved.used_system_color_scheme);
    assert(!resolved.used_system_font_metrics);
    assert(resolved.used_system_font_scale);
    assert(resolved.used_user_font_scale);
    assert(resolved.used_system_line_height);
    assert(resolved.used_system_scroll_metrics);
    assert(resolved.used_user_scroll_scale);
    assert(resolved.used_user_scroll_bar_visibility);
    assert(resolved.used_system_accent_color);
    assert(resolved.used_system_reduce_motion);
    assert(!resolved.used_user_motion_scale);
    assert(std::fabs(resolved.effective_body_font_size - 24.0f) < 0.001f);
    assert(std::fabs(resolved.effective_heading_font_size - 33.6f) < 0.001f);
    assert(std::fabs(resolved.effective_small_font_size - 21.6f) < 0.001f);
    assert(std::fabs(resolved.effective_line_height_ratio - 1.7f) < 0.001f);
    assert(std::fabs(resolved.effective_scroll_delta_multiplier - 1.875f)
           < 0.001f);
    assert(std::fabs(resolved.effective_scroll_horizontal_delta_multiplier
                     - 1.5f)
           < 0.001f);
    assert(resolved.effective_scroll_bar_visibility == "hidden");
    assert(std::fabs(resolved.effective_motion_duration_multiplier - 0.0f)
           < 0.001f);

    theme::ThemePreferenceOverrides motion_overrides{};
    motion_overrides.apply_system_reduce_motion = false;
    motion_overrides.motion_duration_multiplier = 0.5f;
    auto motion_resolved = theme::resolve_theme_preferences(
        base,
        system,
        motion_overrides);
    assert(!motion_resolved.used_system_reduce_motion);
    assert(motion_resolved.used_user_motion_scale);
    assert(std::fabs(motion_resolved.effective_motion_duration_multiplier
                     - 0.5f)
           < 0.001f);
    std::puts("PASS: pure theme contract metadata");
}

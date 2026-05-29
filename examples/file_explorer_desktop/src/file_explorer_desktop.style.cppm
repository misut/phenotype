module;
#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module file_explorer_desktop:style;

import file_explorer_shared;
import phenotype;
import phenotype.native;
import :messages_and_startup;
import :input_messages;
import :runtime_preferences;
import :state_and_debug;

export namespace file_explorer_desktop {
phenotype::Color rgba(int r, int g, int b, int a = 255) {
    return phenotype::Color{
        static_cast<unsigned char>(r),
        static_cast<unsigned char>(g),
        static_cast<unsigned char>(b),
        static_cast<unsigned char>(a),
    };
}

phenotype::FontSpec finder_font(
        phenotype::FontWeight weight = phenotype::FontWeight::Regular) {
    auto const& theme = phenotype::current_theme();
    return phenotype::FontSpec{
        .family = theme.default_font_family,
        .weight = weight,
    };
}

phenotype::Color with_alpha(phenotype::Color color, int alpha) {
    color.a = static_cast<unsigned char>(std::clamp(alpha, 0, 255));
    return color;
}

bool finder_dark_palette() {
    auto const& t = phenotype::current_theme();
    auto luma = [](phenotype::Color c) {
        return static_cast<int>(c.r) * 299
             + static_cast<int>(c.g) * 587
             + static_cast<int>(c.b) * 114;
    };
    return luma(t.background) < luma(t.foreground);
}

phenotype::Color finder_primary_ink() {
    return phenotype::current_theme().foreground;
}

phenotype::Color finder_secondary_ink() {
    return phenotype::current_theme().muted;
}

phenotype::Color finder_tertiary_ink() {
    return finder_dark_palette() ? rgba(142, 142, 147)
                                 : rgba(130, 130, 136);
}

phenotype::MaterialStyle main_content_shell_material_style() {
    using namespace phenotype;
    auto const& t = current_theme();
    bool const dark = finder_dark_palette();
    auto material = layout::plain_material_style(
        dark ? rgba(30, 30, 32) : rgba(255, 255, 255),
        dark ? t.border : rgba(222, 222, 226),
        MaterialSurfaceRole::Content,
        dark ? "plain-dark-main-content-shell"
             : "plain-white-main-content-shell",
        dark ? "plain-dark-content-shell"
             : "plain-white-content-shell");
    material.shadow_alpha = 0.08f;
    material.shadow_radius = 12.0f;
    return material;
}

float main_content_shell_inner_height(
        file_explorer_demo::ExplorerState const& explorer) {
    auto const viewport =
        file_explorer_demo::effective_viewport(explorer, "desktop");
    float const outer_height = std::max(
        0.0f,
        static_cast<float>(viewport.height)
            - file_explorer_demo::k_desktop_window_content_inset * 2.0f);
    float const padding =
        phenotype::layout::space_value(phenotype::SpaceToken::Xs);
    return std::max(0.0f, outer_height - padding * 2.0f);
}

phenotype::layout::MaterialSurfaceOptions main_content_shell_options(
        file_explorer_demo::ExplorerState const& explorer) {
    using namespace phenotype;
    auto options = layout::glass_surface_options(
        layout::GlassSurfacePreset::Content,
        "Main Content Shell");
    options.role = MaterialSurfaceRole::Content;
    options.direction = FlexDirection::Column;
    options.kind = MaterialKind::Regular;
    options.max_width = 0.0f;
    options.fixed_height = main_content_shell_inner_height(explorer);
    options.padding = SpaceToken::Xs;
    options.gap = SpaceToken::Xs;
    options.border_radius = k_window_radius;
    options.shape = MaterialSurfaceShape::RoundedRectangle;
    options.border_width = 1.0f;
    options.has_material_override = true;
    options.material_override = main_content_shell_material_style();
    return options;
}

phenotype::layout::MaterialSurfaceOptions toolbar_group_options(
        char const* label,
        float max_width) {
    using namespace phenotype;
    auto options = layout::glass_surface_options(
        layout::GlassSurfacePreset::ToolbarGroup,
        label);
    options.max_width = max_width;
    options.fixed_height = k_toolbar_group_height;
    options.border_radius = k_toolbar_group_radius;
    options.has_material_override = true;
    options.material_override = layout::plain_material_style(
        finder_dark_palette() ? rgba(46, 46, 48, 230)
                              : rgba(255, 255, 255, 160),
        finder_dark_palette() ? rgba(255, 255, 255, 28)
                              : rgba(0, 0, 0, 18),
        MaterialSurfaceRole::Toolbar,
        "plain-toolbar-group",
        "plain-toolbar-group");
    return options;
}

phenotype::layout::MaterialSurfaceOptions segmented_toolbar_options(
        char const* label,
        float max_width) {
    using namespace phenotype;
    auto options = layout::glass_surface_options(
        layout::GlassSurfacePreset::SegmentedControl,
        label);
    options.role = MaterialSurfaceRole::Toolbar;
    options.max_width = max_width;
    options.fixed_height = k_toolbar_group_height;
    options.border_radius = k_toolbar_group_radius;
    options.has_material_override = true;
    options.material_override = layout::plain_material_style(
        finder_dark_palette() ? rgba(46, 46, 48, 230)
                              : rgba(255, 255, 255, 160),
        finder_dark_palette() ? rgba(255, 255, 255, 28)
                              : rgba(0, 0, 0, 18),
        MaterialSurfaceRole::Toolbar,
        "plain-segmented-toolbar",
        "plain-segmented-toolbar");
    return options;
}

phenotype::layout::MaterialSurfaceOptions content_section_options(
        phenotype::SpaceToken gap = phenotype::SpaceToken::Md) {
    using namespace phenotype;
    auto options = layout::glass_surface_options(
        layout::GlassSurfacePreset::Content,
        "Files");
    options.kind = MaterialKind::None;
    options.gap = gap;
    options.border_radius = 0.0f;
    options.border_width = 0.0f;
    options.shape = MaterialSurfaceShape::Rectangle;
    return options;
}

phenotype::layout::MaterialSurfaceOptions status_section_options() {
    using namespace phenotype;
    auto options = layout::glass_surface_options(
        layout::GlassSurfacePreset::StatusBar,
        "Status Bar");
    options.kind = MaterialKind::None;
    options.padding = SpaceToken::Xs;
    options.border_radius = 0.0f;
    options.border_width = 0.0f;
    options.shape = MaterialSurfaceShape::Rectangle;
    return options;
}

phenotype::MaterialStyle finder_sidebar_material_style() {
    using namespace phenotype;
    bool const dark = finder_dark_palette();
    auto material = layout::plain_material_style(
        dark ? rgba(46, 46, 49) : rgba(222, 222, 222),
        rgba(255, 255, 255, 0),
        MaterialSurfaceRole::Sidebar,
        "plain-sidebar-surface",
        "plain-sidebar-surface");
    material.luminance_floor = 0.01f;
    material.luminance_gain = 1.01f;
    return material;
}

phenotype::layout::MaterialSurfaceOptions finder_sidebar_options() {
    using namespace phenotype;
    auto options = layout::glass_surface_options(
        layout::GlassSurfacePreset::Sidebar,
        "Sidebar");
    options.max_width = k_sidebar_width;
    options.padding = SpaceToken::Lg;
    options.gap = SpaceToken::Xs;
    options.border_radius = k_window_radius;
    options.has_material_override = true;
    options.material_override = finder_sidebar_material_style();
    return options;
}

} // namespace file_explorer_desktop

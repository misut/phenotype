export module phenotype_cli.icons_common;

import cppx.cli;
import phenotype.icon_catalog;
import phenotype_cli.common;
import std;

namespace icon_catalog = phenotype::icon_catalog;

export namespace phenotype_cli::icons {

using namespace phenotype_cli::common;

enum class IconCatalogSet {
    All,
    Sidebar,
    Toolbar,
    FileType,
};

auto icon_catalog_set_name(IconCatalogSet set) -> std::string_view {
    switch (set) {
    case IconCatalogSet::All:      return "all";
    case IconCatalogSet::Sidebar:  return "sidebar";
    case IconCatalogSet::Toolbar:  return "toolbar";
    case IconCatalogSet::FileType: return "file_type";
    }
    return "all";
}

auto icon_catalog_set_count(IconCatalogSet set) -> unsigned int {
    switch (set) {
    case IconCatalogSet::All:      return icon_catalog::all_symbol_count;
    case IconCatalogSet::Sidebar:  return icon_catalog::sidebar_symbol_count;
    case IconCatalogSet::Toolbar:  return icon_catalog::toolbar_symbol_count;
    case IconCatalogSet::FileType: return icon_catalog::file_type_symbol_count;
    }
    return icon_catalog::all_symbol_count;
}

auto icon_catalog_set_symbol(IconCatalogSet set, unsigned int index)
        -> icon_catalog::Symbol {
    switch (set) {
    case IconCatalogSet::All:      return icon_catalog::symbol_at(index);
    case IconCatalogSet::Sidebar:  return icon_catalog::sidebar_symbol_at(index);
    case IconCatalogSet::Toolbar:  return icon_catalog::toolbar_symbol_at(index);
    case IconCatalogSet::FileType: return icon_catalog::file_type_symbol_at(index);
    }
    return icon_catalog::symbol_at(index);
}

auto icon_color_json(icon_catalog::SymbolColor const& color) -> std::string {
    return std::format(
        "{{\"r\":{},\"g\":{},\"b\":{},\"a\":{}}}",
        static_cast<int>(color.r),
        static_cast<int>(color.g),
        static_cast<int>(color.b),
        static_cast<int>(color.a));
}

auto icon_rendering_capabilities_json(
        icon_catalog::SymbolRenderingCapabilities const& capabilities)
        -> std::string {
    return std::format(
        "{{\"policy\":{},\"monochrome\":{},\"hierarchical\":{},"
        "\"palette\":{},\"multicolor\":{}}}",
        json_string(capabilities.policy),
        capabilities.monochrome ? "true" : "false",
        capabilities.hierarchical ? "true" : "false",
        capabilities.palette ? "true" : "false",
        capabilities.multicolor ? "true" : "false");
}

auto icon_source_attribution_json(
        icon_catalog::SymbolSourceAttribution const& source) -> std::string {
    return std::format(
        "{{\"family\":{},\"icon_name\":{},\"license\":{},"
        "\"style\":{},\"license_url\":{},\"source_url\":{},"
        "\"source_revision\":{},"
        "\"copyright\":{},"
        "\"embedded_source\":{},\"modified_for_phenotype\":{},"
        "\"apple_asset\":{},\"platform_extracted\":{},"
        "\"runtime_fetch_required\":{}}}",
        json_string(source.family),
        json_string(source.icon_name),
        json_string(source.license),
        json_string(source.style),
        json_string(source.license_url),
        json_string(source.source_url),
        json_string(source.source_revision),
        json_string(source.copyright),
        source.embedded_source ? "true" : "false",
        source.modified_for_phenotype ? "true" : "false",
        source.apple_asset ? "true" : "false",
        source.platform_extracted ? "true" : "false",
        source.runtime_fetch_required ? "true" : "false");
}

struct IconLookupResult {
    icon_catalog::Symbol symbol = icon_catalog::Symbol::Folder;
    std::string_view match_kind;
};

auto lookup_icon_symbol(std::string_view query)
        -> std::optional<IconLookupResult> {
    if (auto symbol = icon_catalog::symbol_from_name(query)) {
        return IconLookupResult{
            .symbol = *symbol,
            .match_kind = "name",
        };
    }
    if (auto symbol =
            icon_catalog::symbol_from_semantic_reference_name(query)) {
        return IconLookupResult{
            .symbol = *symbol,
            .match_kind = "semantic_reference_name",
        };
    }
    return std::nullopt;
}

auto parse_material_symbols_style(std::string_view value)
        -> std::optional<icon_catalog::MaterialSymbolsStyle> {
    return icon_catalog::material_symbols_style_from_name(value);
}

auto icon_material_symbols_style_from_invocation(
        cppx::cli::Invocation const& invocation)
        -> std::expected<icon_catalog::MaterialSymbolsStyle, std::string> {
    if (auto value = invocation.value("style")) {
        if (auto style = parse_material_symbols_style(*value))
            return *style;
        return std::unexpected{std::format(
            "invalid Material Symbols style '{}'; expected outlined, rounded, or sharp",
            *value)};
    }
    return icon_catalog::default_material_symbols_style();
}

struct MaterialSymbolAxisOptions {
    bool fill = false;
    int weight = 400;
    int grade = 0;
    int optical_size = 24;
};

auto parse_bool_axis(std::string_view value) -> std::optional<bool> {
    if (value == "1" || value == "true" || value == "yes" || value == "on")
        return true;
    if (value == "0" || value == "false" || value == "no" || value == "off")
        return false;
    return std::nullopt;
}

auto parse_int_axis(std::string_view value, int min, int max)
        -> std::optional<int> {
    int out = 0;
    auto const* first = value.data();
    auto const* last = value.data() + value.size();
    auto [ptr, ec] = std::from_chars(first, last, out);
    if (ec != std::errc{} || ptr != last)
        return std::nullopt;
    if (out < min || out > max)
        return std::nullopt;
    return out;
}

auto icon_material_symbol_axes_from_invocation(
        cppx::cli::Invocation const& invocation)
        -> std::expected<MaterialSymbolAxisOptions, std::string> {
    MaterialSymbolAxisOptions axes{};
    if (auto value = invocation.value("fill")) {
        auto fill = parse_bool_axis(*value);
        if (!fill)
            return std::unexpected{"invalid Material Symbols fill axis; expected 0, 1, false, or true"};
        axes.fill = *fill;
    }
    if (auto value = invocation.value("weight")) {
        auto parsed = parse_int_axis(*value, 100, 700);
        if (!parsed)
            return std::unexpected{"invalid Material Symbols weight axis; expected 100..700"};
        axes.weight = *parsed;
    }
    if (auto value = invocation.value("grade")) {
        auto parsed = parse_int_axis(*value, -25, 200);
        if (!parsed)
            return std::unexpected{"invalid Material Symbols grade axis; expected -25..200"};
        axes.grade = *parsed;
    }
    if (auto value = invocation.value("optical-size")) {
        auto parsed = parse_int_axis(*value, 20, 48);
        if (!parsed)
            return std::unexpected{"invalid Material Symbols optical size axis; expected 20..48"};
        axes.optical_size = *parsed;
    }
    return axes;
}

auto icon_material_symbol_axes_json(MaterialSymbolAxisOptions axes,
                                    icon_catalog::MaterialSymbolsStyle style)
        -> std::string {
    return std::format(
        "{{\"style\":{},\"fill\":{},\"weight\":{},\"grade\":{},"
        "\"optical_size\":{},\"rendering_backend\":\"svg_axis_mapping\"}}",
        json_string(icon_catalog::material_symbols_style_name(style)),
        axes.fill ? "true" : "false",
        axes.weight,
        axes.grade,
        axes.optical_size);
}

auto parse_icon_presentation_role(std::string_view value)
        -> std::optional<icon_catalog::SymbolPresentationRole> {
    if (value == "toolbar")
        return icon_catalog::SymbolPresentationRole::Toolbar;
    if (value == "navigation")
        return icon_catalog::SymbolPresentationRole::Navigation;
    if (value == "sidebar")
        return icon_catalog::SymbolPresentationRole::Sidebar;
    if (value == "file_type")
        return icon_catalog::SymbolPresentationRole::FileType;
    if (value == "action")
        return icon_catalog::SymbolPresentationRole::Action;
    return std::nullopt;
}

auto parse_icon_interaction_phase(std::string_view value)
        -> std::optional<icon_catalog::SymbolInteractionPhase> {
    if (value == "normal")
        return icon_catalog::SymbolInteractionPhase::Normal;
    if (value == "hovered")
        return icon_catalog::SymbolInteractionPhase::Hovered;
    if (value == "pressed")
        return icon_catalog::SymbolInteractionPhase::Pressed;
    return std::nullopt;
}

auto icon_presentation_role_from_invocation(
        cppx::cli::Invocation const& invocation,
        icon_catalog::Symbol symbol)
        -> std::expected<icon_catalog::SymbolPresentationRole, std::string> {
    if (auto value = invocation.value("role")) {
        if (auto role = parse_icon_presentation_role(*value))
            return *role;
        return std::unexpected{std::format(
            "invalid icon presentation role '{}'; expected toolbar, navigation, sidebar, file_type, or action",
            *value)};
    }
    return icon_catalog::default_presentation_role(symbol);
}

auto icon_interaction_phase_from_invocation(
        cppx::cli::Invocation const& invocation)
        -> std::expected<icon_catalog::SymbolInteractionPhase, std::string> {
    if (auto value = invocation.value("phase")) {
        if (auto phase = parse_icon_interaction_phase(*value))
            return *phase;
        return std::unexpected{std::format(
            "invalid icon interaction phase '{}'; expected normal, hovered, or pressed",
            *value)};
    }
    return icon_catalog::SymbolInteractionPhase::Normal;
}

auto icon_color_with_opacity(icon_catalog::SymbolColor color,
                             float opacity) noexcept
        -> icon_catalog::SymbolColor {
    auto alpha = static_cast<float>(color.a) * std::clamp(opacity, 0.0f, 1.0f);
    auto rounded = static_cast<int>(alpha + 0.5f);
    color.a = static_cast<unsigned char>(std::clamp(rounded, 0, 255));
    return color;
}

}

export module phenotype_cli.icons;

import cppx.cli;
import cppx.terminal;
import file_explorer_shared;
import phenotype.icon_catalog;
import phenotype.svg_contract;
import phenotype_cli.common;
import phenotype_cli.icons_common;
import phenotype_cli.icons_checks;
import phenotype_cli.icons_render;
import phenotype_cli.icons_svg_inspect;
import std;

namespace fs = std::filesystem;
namespace icon_catalog = phenotype::icon_catalog;
namespace svg_contract = phenotype::svg_contract;

export namespace phenotype_cli::icons {

using namespace phenotype_cli::common;

auto icon_control_chrome_json(
        icon_catalog::SymbolControlChrome const& chrome) -> std::string {
    return std::format(
        "{{\"policy\":{},\"role\":{},\"symbol_tone\":{},"
        "\"symbol_color\":{},\"background_color\":{},"
        "\"hover_background_color\":{},\"corner_radius\":{},"
        "\"hit_target_size\":{},\"activation_hit_target_size\":{},"
        "\"borderless\":{},"
        "\"grouped_control\":{}}}",
        json_string(chrome.policy),
        json_string(icon_catalog::symbol_presentation_role_name(chrome.role)),
        json_string(icon_catalog::symbol_tone_name(chrome.symbol_tone)),
        icon_color_json(chrome.symbol_color),
        icon_color_json(chrome.background_color),
        icon_color_json(chrome.hover_background_color),
        chrome.corner_radius,
        chrome.hit_target_size,
        icon_catalog::activation_hit_target_size(chrome.role),
        chrome.borderless ? "true" : "false",
        chrome.grouped_control ? "true" : "false");
}

auto icon_state_recipe_json(
        icon_catalog::SymbolStateRecipe const& recipe) -> std::string {
    return std::format(
        "{{\"policy\":{},\"role\":{},\"phase\":{},"
        "\"selected\":{},\"enabled\":{},\"symbol_tone\":{},"
        "\"symbol_color\":{},\"background_color\":{},"
        "\"symbol_opacity\":{},\"scale\":{},\"corner_radius\":{},"
        "\"hit_target_size\":{},\"activation_hit_target_size\":{},"
        "\"borderless\":{},"
        "\"grouped_control\":{}}}",
        json_string(recipe.policy),
        json_string(icon_catalog::symbol_presentation_role_name(recipe.role)),
        json_string(icon_catalog::symbol_interaction_phase_name(recipe.phase)),
        recipe.selected ? "true" : "false",
        recipe.enabled ? "true" : "false",
        json_string(icon_catalog::symbol_tone_name(recipe.symbol_tone)),
        icon_color_json(recipe.symbol_color),
        icon_color_json(recipe.background_color),
        recipe.symbol_opacity,
        recipe.scale,
        recipe.corner_radius,
        recipe.hit_target_size,
        icon_catalog::activation_hit_target_size(recipe.role),
        recipe.borderless ? "true" : "false",
        recipe.grouped_control ? "true" : "false");
}

auto icon_state_recipes_json(
        icon_catalog::SymbolPresentationRole role) -> std::string {
    auto const normal = icon_catalog::macos_state_recipe(
        role,
        icon_catalog::SymbolInteractionState{false, true},
        icon_catalog::SymbolInteractionPhase::Normal);
    auto const hovered = icon_catalog::macos_state_recipe(
        role,
        icon_catalog::SymbolInteractionState{false, true},
        icon_catalog::SymbolInteractionPhase::Hovered);
    auto const pressed = icon_catalog::macos_state_recipe(
        role,
        icon_catalog::SymbolInteractionState{false, true},
        icon_catalog::SymbolInteractionPhase::Pressed);
    auto const selected = icon_catalog::macos_state_recipe(
        role,
        icon_catalog::SymbolInteractionState{true, true},
        icon_catalog::SymbolInteractionPhase::Normal);
    auto const disabled = icon_catalog::macos_state_recipe(
        role,
        icon_catalog::SymbolInteractionState{false, false},
        icon_catalog::SymbolInteractionPhase::Normal);
    return std::format(
        "{{\"normal\":{},\"hovered\":{},\"pressed\":{},"
        "\"selected\":{},\"disabled\":{}}}",
        icon_state_recipe_json(normal),
        icon_state_recipe_json(hovered),
        icon_state_recipe_json(pressed),
        icon_state_recipe_json(selected),
        icon_state_recipe_json(disabled));
}

auto icon_reference_source_json(
        icon_catalog::IconReferenceSource const& source) -> std::string {
    return std::format(
        "{{\"name\":{},\"url\":{},\"role\":{},\"license_policy\":{},"
        "\"license_url\":{},\"source_acquisition\":{},"
        "\"used_as_embedded_asset_source\":{},"
        "\"apple_owned_artwork\":{},\"may_embed_svg_source\":{},"
        "\"requires_notice\":{},\"runtime_fetch_allowed\":{},"
        "\"platform_extraction_allowed\":{}}}",
        json_string(source.name),
        json_string(source.url),
        json_string(source.role),
        json_string(source.license_policy),
        json_string(source.license_url),
        json_string(source.source_acquisition),
        source.used_as_embedded_asset_source ? "true" : "false",
        source.apple_owned_artwork ? "true" : "false",
        source.may_embed_svg_source ? "true" : "false",
        source.requires_notice ? "true" : "false",
        source.runtime_fetch_allowed ? "true" : "false",
        source.platform_extraction_allowed ? "true" : "false");
}

auto icon_reference_sources_json() -> std::string {
    auto out = std::string{"["};
    for (unsigned int i = 0; i < icon_catalog::reference_source_count; ++i) {
        if (i > 0)
            out += ",";
        out += icon_reference_source_json(icon_catalog::reference_source_at(i));
    }
    out += "]";
    return out;
}

auto icon_material_symbols_source_icons_json() -> std::string {
    auto out = std::string{"["};
    for (unsigned int i = 0;
         i < icon_catalog::material_symbols_unique_source_icon_count;
         ++i) {
        if (i > 0)
            out += ",";
        out += json_string(icon_catalog::material_symbols_source_icon_name_at(i));
    }
    out += "]";
    return out;
}

auto icon_material_symbols_styles_json() -> std::string {
    auto out = std::string{"["};
    for (unsigned int i = 0; i < icon_catalog::material_symbols_style_count; ++i) {
        if (i > 0)
            out += ",";
        auto const style = icon_catalog::material_symbols_style_at(i);
        out += std::format(
            "{{\"name\":{},\"label\":{},\"font_family\":{},"
            "\"css_class\":{},\"source_directory\":{},\"default\":{}}}",
            json_string(icon_catalog::material_symbols_style_name(style)),
            json_string(icon_catalog::material_symbols_style_label(style)),
            json_string(icon_catalog::material_symbols_font_family(style)),
            json_string(icon_catalog::material_symbols_css_class(style)),
            json_string(icon_catalog::material_symbols_source_directory(style)),
            icon_catalog::is_default_material_symbols_style(style)
                ? "true"
                : "false");
    }
    out += "]";
    return out;
}

auto icon_interaction_tones_json(bool enabled) -> std::string {
    if (!enabled)
        return "{}";
    auto tone_name = [](icon_catalog::SymbolPresentationRole role,
                        bool selected,
                        bool state_enabled) {
        return json_string(icon_catalog::symbol_tone_name(
            icon_catalog::macos_interaction_tone(
                role,
                icon_catalog::SymbolInteractionState{
                    selected,
                    state_enabled,
                })));
    };
    return std::format(
        "{{\"sidebar_selected\":{},\"sidebar_unselected\":{},"
        "\"toolbar_selected\":{},\"toolbar_unselected\":{},"
        "\"disabled\":{}}}",
        tone_name(icon_catalog::SymbolPresentationRole::Sidebar, true, true),
        tone_name(icon_catalog::SymbolPresentationRole::Sidebar, false, true),
        tone_name(icon_catalog::SymbolPresentationRole::Toolbar, true, true),
        tone_name(icon_catalog::SymbolPresentationRole::Toolbar, false, true),
        tone_name(icon_catalog::SymbolPresentationRole::Toolbar, false, false));
}

auto icon_symbol_json(icon_catalog::Symbol symbol,
                      std::string_view reference_set) -> std::string {
    auto const desc = icon_catalog::descriptor(symbol);
    auto const presentation_role =
        icon_catalog::default_presentation_role(symbol);
    auto const presentation_scale =
        icon_catalog::default_scale(presentation_role);
    auto const presentation_metrics =
        icon_catalog::metrics(presentation_role);
    auto const tone = icon_catalog::macos_interaction_tone(
        presentation_role,
        icon_catalog::SymbolInteractionState{false, true});
    auto const selected_tone = icon_catalog::macos_interaction_tone(
        presentation_role,
        icon_catalog::SymbolInteractionState{true, true});
    auto const disabled_tone = icon_catalog::macos_interaction_tone(
        presentation_role,
        icon_catalog::SymbolInteractionState{false, false});
    auto const source = icon_catalog::svg_source(symbol);
    auto const source_attribution = icon_catalog::source_attribution(symbol);
    auto const color = icon_catalog::macos_light_tone_color(tone);
    auto const file_type_color = icon_catalog::macos_file_type_color(symbol);
    auto const control_chrome = icon_catalog::macos_control_chrome(
        presentation_role,
        icon_catalog::SymbolInteractionState{false, true});
    auto const selected_control_chrome = icon_catalog::macos_control_chrome(
        presentation_role,
        icon_catalog::SymbolInteractionState{true, true});
    auto const rendering_capabilities =
        icon_catalog::rendering_capabilities(symbol);
    return std::format(
        "{{\"name\":{},\"semantic_reference_name\":{},"
        "\"reference_set\":{},\"role\":{},\"variant\":{},"
        "\"preferred_rendering\":{},\"default_scale\":{},"
        "\"default_weight\":{},"
        "\"style\":{},\"reference_family\":{},\"reference_policy\":{},"
        "\"grid_size\":{},\"default_stroke_width\":{},"
        "\"stroke_cap\":{},\"stroke_join\":{},"
        "\"secondary_opacity\":{},\"layer_count\":{},"
        "\"uses_current_color\":{},\"round_stroke\":{},\"filled\":{},"
        "\"text_weight_aligned\":{},"
        "\"supports_monochrome\":{},"
        "\"supports_hierarchical_opacity\":{},"
        "\"supports_palette\":{},\"supports_multicolor\":{},"
        "\"rendering_capabilities\":{},"
        "\"uses_svg_path_arcs\":{},"
        "\"phenotype_owned\":{},\"uses_sf_symbols_asset\":{},"
        "\"uses_material_symbols_source\":{},"
        "\"has_svg_source\":{},\"source_bytes\":{},"
        "\"source_attribution\":{},"
        "\"file_type_color\":{},"
        "\"presentation\":{{\"role\":{},\"tone\":{},"
        "\"selected_tone\":{},\"disabled_tone\":{},\"scale\":{},"
        "\"point_size\":{},\"hit_target_size\":{},"
        "\"content_inset\":{},\"optical_y_offset\":{},"
        "\"metrics_policy\":{},\"color\":{},"
        "\"control_chrome\":{},"
        "\"selected_control_chrome\":{},"
        "\"state_recipes\":{}}}}}",
        json_string(desc.name),
        json_string(desc.semantic_reference_name),
        json_string(reference_set),
        json_string(icon_catalog::symbol_role_name(desc.role)),
        json_string(icon_catalog::symbol_variant_name(desc.variant)),
        json_string(icon_catalog::symbol_rendering_mode_name(
            desc.preferred_rendering)),
        json_string(icon_catalog::symbol_scale_name(desc.default_scale)),
        json_string(icon_catalog::symbol_weight_name(desc.default_weight)),
        json_string(desc.style),
        json_string(desc.reference_family),
        json_string(desc.reference_policy),
        desc.grid_size,
        desc.default_stroke_width,
        json_string(icon_catalog::symbol_stroke_cap_name(desc.stroke_cap)),
        json_string(icon_catalog::symbol_stroke_join_name(desc.stroke_join)),
        desc.secondary_opacity,
        desc.layer_count,
        desc.uses_current_color ? "true" : "false",
        desc.round_stroke ? "true" : "false",
        desc.filled ? "true" : "false",
        desc.text_weight_aligned ? "true" : "false",
        desc.supports_monochrome ? "true" : "false",
        desc.supports_hierarchical_opacity ? "true" : "false",
        desc.supports_palette ? "true" : "false",
        desc.supports_multicolor ? "true" : "false",
        icon_rendering_capabilities_json(rendering_capabilities),
        icon_catalog::uses_svg_path_arcs(symbol) ? "true" : "false",
        desc.phenotype_owned ? "true" : "false",
        desc.uses_sf_symbols_asset ? "true" : "false",
        icon_catalog::uses_material_symbols_source(symbol) ? "true" : "false",
        source.empty() ? "false" : "true",
        source.size(),
        icon_source_attribution_json(source_attribution),
        icon_color_json(file_type_color),
        json_string(icon_catalog::symbol_presentation_role_name(
            presentation_role)),
        json_string(icon_catalog::symbol_tone_name(tone)),
        json_string(icon_catalog::symbol_tone_name(selected_tone)),
        json_string(icon_catalog::symbol_tone_name(disabled_tone)),
        json_string(icon_catalog::symbol_scale_name(presentation_scale)),
        presentation_metrics.point_size,
        presentation_metrics.hit_target_size,
        presentation_metrics.content_inset,
        presentation_metrics.optical_y_offset,
        json_string(icon_catalog::metrics_policy()),
        icon_color_json(color),
        icon_control_chrome_json(control_chrome),
        icon_control_chrome_json(selected_control_chrome),
        icon_state_recipes_json(presentation_role));
}

auto icon_symbol_set_json(IconCatalogSet set) -> std::string {
    auto out = std::string{"["};
    auto const count = icon_catalog_set_count(set);
    for (unsigned int i = 0; i < count; ++i) {
        if (i > 0)
            out += ",";
        out += icon_symbol_json(
            icon_catalog_set_symbol(set, i),
            icon_catalog_set_name(set));
    }
    out += "]";
    return out;
}

auto icon_lookup_json(std::string_view query,
                      IconLookupResult const& result) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons lookup\","
        "\"ok\":true,\"query\":{},\"match_kind\":{},\"symbol\":{}}}",
        json_string(query),
        json_string(result.match_kind),
        icon_symbol_json(result.symbol, "lookup"));
}

auto icon_lookup_not_found_json(std::string_view query) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons lookup\","
        "\"ok\":false,\"query\":{},\"error\":\"symbol not found\","
        "\"suggestion\":\"Use phenotype icons catalog --json to list built-in symbol names and semantic references.\"}}",
        json_string(query));
}

auto icon_svg_json(std::string_view query,
                   IconLookupResult const& result,
                   icon_catalog::MaterialSymbolsStyle style) -> std::string {
    auto const desc = icon_catalog::descriptor(result.symbol, style);
    auto const source = icon_catalog::svg_source(result.symbol, style);
    auto const capabilities = icon_catalog::rendering_capabilities(result.symbol);
    auto const source_attribution =
        icon_catalog::source_attribution(result.symbol, style);
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons svg\","
        "\"ok\":true,\"query\":{},\"match_kind\":{},"
        "\"symbol\":{},\"semantic_reference_name\":{},"
        "\"style\":{},\"material_symbols_style\":{},"
        "\"source_format\":\"svg\",\"asset_policy\":{},"
        "\"source_license_policy\":{},\"apple_asset_boundary\":{},"
        "\"source_attribution\":{},"
        "\"rendering_capabilities\":{},"
        "\"source_bytes\":{},\"source\":{}}}",
        json_string(query),
        json_string(result.match_kind),
        json_string(desc.name),
        json_string(desc.semantic_reference_name),
        json_string(desc.style),
        json_string(icon_catalog::material_symbols_style_name(style)),
        json_string(icon_catalog::asset_policy()),
        json_string(icon_catalog::source_license_policy()),
        json_string(icon_catalog::apple_asset_boundary()),
        icon_source_attribution_json(source_attribution),
        icon_rendering_capabilities_json(capabilities),
        source.size(),
        json_string(source));
}

auto icon_svg_not_found_json(std::string_view query) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons svg\","
        "\"ok\":false,\"query\":{},\"error\":\"symbol not found\","
        "\"suggestion\":\"Use phenotype icons catalog --json to list built-in symbol names and semantic references.\"}}",
        json_string(query));
}

auto icon_presentation_json(std::string_view query,
                            IconLookupResult const& result,
                            icon_catalog::SymbolPresentationRole role,
                            icon_catalog::SymbolInteractionPhase phase,
                            icon_catalog::MaterialSymbolsStyle style,
                            MaterialSymbolAxisOptions axes,
                            bool selected,
                            bool enabled) -> std::string {
    auto const desc = icon_catalog::descriptor(result.symbol, style);
    auto const metrics = icon_catalog::metrics(role);
    auto const recipe = icon_catalog::macos_state_recipe(
        role,
        icon_catalog::SymbolInteractionState{selected, enabled},
        phase);
    auto const capabilities = icon_catalog::rendering_capabilities(result.symbol);
    auto const source_attribution =
        icon_catalog::source_attribution(result.symbol, style);
    auto const visible_color =
        icon_color_with_opacity(recipe.symbol_color, recipe.symbol_opacity);
    auto const content_inset =
        (recipe.hit_target_size - metrics.point_size * recipe.scale) / 2.0f;
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons present\","
        "\"ok\":true,\"query\":{},\"match_kind\":{},"
        "\"symbol\":{},\"semantic_reference_name\":{},"
        "\"reference_policy\":{},\"asset_policy\":{},"
        "\"source_license_policy\":{},\"apple_asset_boundary\":{},"
        "\"source_attribution\":{},"
        "\"material_symbols_axes\":{},"
        "\"state\":{{\"role\":{},\"phase\":{},\"selected\":{},"
        "\"enabled\":{}}},"
        "\"presentation\":{{\"policy\":{},\"metrics_policy\":{},"
        "\"hit_target_policy\":{},\"rendering\":{},"
        "\"rendering_capabilities\":{},\"symbol_tone\":{},"
        "\"base_symbol_color\":{},\"visible_symbol_color\":{},"
        "\"background_color\":{},\"symbol_opacity\":{},"
        "\"phase_scale\":{},\"point_size\":{},"
        "\"effective_point_size\":{},\"hit_target_size\":{},"
        "\"activation_hit_target_size\":{},"
        "\"content_inset\":{},\"optical_y_offset\":{},"
        "\"corner_radius\":{},\"borderless\":{},"
        "\"grouped_control\":{},\"likely_layer\":\"icon_glyph\","
        "\"likely_pass\":\"svg_path_or_line_paint\"}}}}",
        json_string(query),
        json_string(result.match_kind),
        json_string(desc.name),
        json_string(desc.semantic_reference_name),
        json_string(icon_catalog::reference_policy()),
        json_string(icon_catalog::asset_policy()),
        json_string(icon_catalog::source_license_policy()),
        json_string(icon_catalog::apple_asset_boundary()),
        icon_source_attribution_json(source_attribution),
        icon_material_symbol_axes_json(axes, style),
        json_string(icon_catalog::symbol_presentation_role_name(role)),
        json_string(icon_catalog::symbol_interaction_phase_name(phase)),
        selected ? "true" : "false",
        enabled ? "true" : "false",
        json_string(recipe.policy),
        json_string(icon_catalog::metrics_policy()),
        json_string(icon_catalog::hit_target_policy()),
        json_string(icon_catalog::symbol_rendering_mode_name(
            desc.preferred_rendering)),
        icon_rendering_capabilities_json(capabilities),
        json_string(icon_catalog::symbol_tone_name(recipe.symbol_tone)),
        icon_color_json(recipe.symbol_color),
        icon_color_json(visible_color),
        icon_color_json(recipe.background_color),
        recipe.symbol_opacity,
        recipe.scale,
        metrics.point_size,
        metrics.point_size * recipe.scale,
        recipe.hit_target_size,
        icon_catalog::activation_hit_target_size(role),
        content_inset,
        metrics.optical_y_offset,
        recipe.corner_radius,
        recipe.borderless ? "true" : "false",
        recipe.grouped_control ? "true" : "false");
}

auto icon_present_not_found_json(std::string_view query) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons present\","
        "\"ok\":false,\"query\":{},\"error\":\"symbol not found\","
        "\"suggestion\":\"Use phenotype icons catalog --json to list built-in symbol names and semantic references.\"}}",
        json_string(query));
}

auto icon_render_not_found_json(std::string_view query) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons render\","
        "\"ok\":false,\"query\":{},\"error\":\"symbol not found\","
        "\"suggestion\":\"Use phenotype icons catalog --json to list built-in symbol names and semantic references.\"}}",
        json_string(query));
}

auto icon_reference_names_json(IconCatalogSet set, bool enabled)
        -> std::string {
    if (!enabled)
        return "[]";
    auto out = std::string{"["};
    auto const count = icon_catalog_set_count(set);
    for (unsigned int i = 0; i < count; ++i) {
        if (i > 0)
            out += ",";
        out += json_string(icon_catalog::semantic_reference_name(
            icon_catalog_set_symbol(set, i)));
    }
    out += "]";
    return out;
}

auto sidebar_symbol_tokens_json(bool enabled) -> std::string {
    if (!enabled)
        return "[]";
    auto out = std::string{"["};
    auto const items = file_explorer_demo::desktop_sidebar_symbol_contract();
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0)
            out += ",";
        auto const& item = items[i];
        out += std::format(
            "{{\"token\":{},\"symbol\":{},\"semantic_reference_name\":{}}}",
            json_string(item.token),
            json_string(icon_catalog::name(item.symbol)),
            json_string(icon_catalog::semantic_reference_name(item.symbol)));
    }
    out += "]";
    return out;
}

auto icon_catalog_json(std::span<Check const> checks) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons catalog\","
        "\"ok\":{},\"style\":{{\"name\":{},\"source_format\":{},"
        "\"family\":\"Material Symbols (new)\","
        "\"default_material_symbols_style\":{},"
        "\"available_material_symbols_styles\":{},"
        "\"svg_subset_policy\":{},\"svg_supported_elements\":{},"
        "\"svg_supported_path_commands\":{},"
        "\"svg_supported_style_attributes\":{},"
        "\"svg_arc_policy\":{},"
        "\"stroke_geometry_policy\":{},\"stroke_cap_policy\":{},"
        "\"stroke_join_policy\":{},"
        "\"design_reference\":{},\"reference_family\":{},"
        "\"reference_policy\":{},\"asset_policy\":{},"
        "\"source_license_policy\":{},"
        "\"preferred_external_source_policy\":{},"
        "\"source_acquisition_policy\":{},"
        "\"document_cache_policy\":{},"
        "\"source_attribution_policy\":{},"
        "\"apple_asset_boundary\":{},"
        "\"interface_metaphor_policy\":{},"
        "\"visual_consistency_policy\":{},"
        "\"alignment\":{},\"variant_policy\":{},"
        "\"presentation_policy\":{},\"tone_policy\":{},"
        "\"default_weight\":{},"
        "\"rendering_capability_policy\":{},"
        "\"interaction_tone_policy\":{},"
        "\"symbol_control_chrome_policy\":{},"
        "\"symbol_interaction_phase_policy\":{},"
        "\"toolbar_symbol_chrome_policy\":{},"
        "\"sidebar_symbol_color_policy\":{},"
        "\"file_type_color_policy\":{},\"default_scale\":{},"
        "\"metrics_policy\":{},\"hit_target_policy\":{},"
        "\"reference_sources\":{}}},"
        "\"counts\":{{\"all\":{},\"phenotype_owned\":{},"
        "\"permissive_source\":{},\"material_symbols_source\":{},"
        "\"material_symbols_unique_source_icons\":{},"
        "\"apple_asset\":{},\"platform_extracted\":{},"
        "\"runtime_fetched\":{},\"audited_source\":{},"
        "\"sidebar\":{},\"toolbar\":{},"
        "\"file_type\":{},"
        "\"outline\":{},\"filled\":{},\"hierarchical\":{},"
        "\"monochrome\":{},\"regular_weight\":{},"
        "\"palette\":{},\"multicolor\":{},"
        "\"reference\":{},\"reference_sources\":{},"
        "\"svg_path_arc\":{},\"round_stroke\":{},"
        "\"interaction_phases\":{}}},"
        "\"source_icons\":{{\"material_symbols_revision\":{},"
        "\"material_symbols_unique_count\":{},"
        "\"material_symbols_style_count\":{},"
        "\"material_symbols_source_variant_count\":{},"
        "\"material_symbols_icon_names\":{},"
        "\"material_symbols_styles\":{}}},"
        "\"symbols\":{},\"sidebar_symbols\":{},"
        "\"toolbar_symbols\":{},\"file_type_symbols\":{},\"checks\":{}}}",
        all_ok(checks) ? "true" : "false",
        json_string(icon_catalog::style_name()),
        json_string(icon_catalog::source_format()),
        json_string(icon_catalog::material_symbols_style_name(
            icon_catalog::default_material_symbols_style())),
        icon_material_symbols_styles_json(),
        json_string(icon_catalog::svg_subset_policy()),
        json_string(icon_catalog::svg_supported_elements()),
        json_string(icon_catalog::svg_supported_path_commands()),
        json_string(icon_catalog::svg_supported_style_attributes()),
        json_string(icon_catalog::svg_arc_policy()),
        json_string(icon_catalog::stroke_geometry_policy()),
        json_string(icon_catalog::stroke_cap_policy()),
        json_string(icon_catalog::stroke_join_policy()),
        json_string(icon_catalog::style_reference()),
        json_string(icon_catalog::reference_family()),
        json_string(icon_catalog::reference_policy()),
        json_string(icon_catalog::asset_policy()),
        json_string(icon_catalog::source_license_policy()),
        json_string(icon_catalog::preferred_external_source_policy()),
        json_string(icon_catalog::source_acquisition_policy()),
        json_string(icon_catalog::document_cache_policy()),
        json_string(icon_catalog::source_attribution_policy()),
        json_string(icon_catalog::apple_asset_boundary()),
        json_string(icon_catalog::interface_metaphor_policy()),
        json_string(icon_catalog::visual_consistency_policy()),
        json_string(icon_catalog::alignment_policy()),
        json_string(icon_catalog::variant_policy()),
        json_string(icon_catalog::presentation_policy()),
        json_string(icon_catalog::tone_policy()),
        json_string(icon_catalog::default_weight_policy()),
        json_string(icon_catalog::rendering_capability_policy()),
        json_string(icon_catalog::interaction_tone_policy()),
        json_string(icon_catalog::symbol_control_chrome_policy()),
        json_string(icon_catalog::symbol_interaction_phase_policy()),
        json_string(icon_catalog::toolbar_symbol_chrome_policy()),
        json_string(icon_catalog::sidebar_symbol_color_policy()),
        json_string(icon_catalog::file_type_color_policy()),
        json_string(icon_catalog::default_scale_policy()),
        json_string(icon_catalog::metrics_policy()),
        json_string(icon_catalog::hit_target_policy()),
        icon_reference_sources_json(),
        icon_catalog::all_symbol_count,
        icon_catalog::phenotype_owned_symbol_count,
        icon_catalog::permissive_source_symbol_count,
        icon_catalog::material_symbols_source_symbol_count,
        icon_catalog::material_symbols_unique_source_icon_count,
        icon_catalog::apple_asset_symbol_count,
        icon_catalog::platform_extracted_symbol_count,
        icon_catalog::runtime_fetched_symbol_count,
        icon_catalog::audited_symbol_source_count,
        icon_catalog::sidebar_symbol_count,
        icon_catalog::toolbar_symbol_count,
        icon_catalog::file_type_symbol_count,
        icon_catalog::outline_symbol_count,
        icon_catalog::filled_symbol_count,
        icon_catalog::hierarchical_symbol_count,
        icon_catalog::monochrome_symbol_count,
        icon_catalog::regular_weight_symbol_count,
        icon_catalog::palette_symbol_count,
        icon_catalog::multicolor_symbol_count,
        icon_catalog::reference_symbol_count,
        icon_catalog::reference_source_count,
        icon_catalog::svg_path_arc_symbol_count,
        icon_catalog::round_stroke_symbol_count,
        icon_catalog::symbol_interaction_phase_count,
        json_string(icon_catalog::material_symbols_source_revision()),
        icon_catalog::material_symbols_unique_source_icon_count,
        icon_catalog::material_symbols_style_count,
        icon_catalog::material_symbols_source_variant_count,
        icon_material_symbols_source_icons_json(),
        icon_material_symbols_styles_json(),
        icon_symbol_set_json(IconCatalogSet::All),
        icon_symbol_set_json(IconCatalogSet::Sidebar),
        icon_symbol_set_json(IconCatalogSet::Toolbar),
        icon_symbol_set_json(IconCatalogSet::FileType),
        checks_json(checks));
}

int run_icons_catalog(cppx::cli::Invocation const& invocation) {
    auto checks = icon_catalog_checks();
    if (invocation.has("json")) {
        std::println("{}", icon_catalog_json(checks));
        return all_ok(checks) ? 0 : 1;
    }

    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "style",
         .value = std::string{icon_catalog::style_name()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "source",
         .value = std::string{icon_catalog::source_format()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "svg subset",
         .value = std::string{icon_catalog::svg_supported_path_commands()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "symbols",
         .value = std::format(
             "{} total, {} sidebar, {} toolbar, {} file-type",
             icon_catalog::all_symbol_count,
             icon_catalog::sidebar_symbol_count,
             icon_catalog::toolbar_symbol_count,
             icon_catalog::file_type_symbol_count),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "reference",
         .value = std::string{icon_catalog::reference_family()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "rendering",
         .value = std::string{icon_catalog::rendering_capability_policy()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "assets",
         .value = std::string{icon_catalog::asset_policy()},
         .status = all_ok(checks) ? cppx::terminal::StatusKind::ok
                                  : cppx::terminal::StatusKind::fail},
        {.label = "cache",
         .value = std::string{icon_catalog::document_cache_policy()},
         .status = cppx::terminal::StatusKind::ok},
    };
    std::println("phenotype icons catalog");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    print_checks("checks", checks);
    return all_ok(checks) ? 0 : 1;
}

int run_icons_lookup(cppx::cli::Invocation const& invocation) {
    auto query = single_positional_text_or_error(
        invocation,
        "icons lookup",
        "name-or-reference");
    if (!query)
        return print_error("icons lookup", query.error(), invocation.has("json"));

    auto result = lookup_icon_symbol(*query);
    if (!result) {
        if (invocation.has("json")) {
            std::println("{}", icon_lookup_not_found_json(*query));
        } else {
            std::println(
                std::cerr,
                "{}",
                cppx::terminal::format_diagnostic({
                    .severity = cppx::terminal::DiagnosticSeverity::error,
                    .message = std::format(
                        "symbol '{}' was not found; run 'phenotype icons catalog' to list built-in symbols",
                        *query),
                    .context = "icons lookup",
                }));
        }
        return 2;
    }

    if (invocation.has("json")) {
        std::println("{}", icon_lookup_json(*query, *result));
        return 0;
    }

    auto const desc = icon_catalog::descriptor(result->symbol);
    auto const role = icon_catalog::default_presentation_role(result->symbol);
    auto const metrics = icon_catalog::metrics(role);
    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "query",
         .value = *query,
         .status = cppx::terminal::StatusKind::ok},
        {.label = "match",
         .value = std::string{result->match_kind},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "symbol",
         .value = std::string{desc.name},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "semantic reference",
         .value = std::string{desc.semantic_reference_name},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "role",
         .value = std::string{icon_catalog::symbol_presentation_role_name(role)},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "metrics",
         .value = std::format(
             "{}pt symbol, {}pt hit target",
             metrics.point_size,
             metrics.hit_target_size),
         .status = cppx::terminal::StatusKind::ok},
    };
    std::println("phenotype icons lookup");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    return 0;
}

int run_icons_svg(cppx::cli::Invocation const& invocation) {
    auto query = single_positional_text_or_error(
        invocation,
        "icons svg",
        "name-or-reference");
    if (!query)
        return print_error("icons svg", query.error(), invocation.has("json"));
    auto style = icon_material_symbols_style_from_invocation(invocation);
    if (!style)
        return print_error("icons svg", style.error(), invocation.has("json"));

    auto result = lookup_icon_symbol(*query);
    if (!result) {
        if (invocation.has("json")) {
            std::println("{}", icon_svg_not_found_json(*query));
        } else {
            std::println(
                std::cerr,
                "{}",
                cppx::terminal::format_diagnostic({
                    .severity = cppx::terminal::DiagnosticSeverity::error,
                    .message = std::format(
                        "symbol '{}' was not found; run 'phenotype icons catalog' to list built-in symbols",
                        *query),
                    .context = "icons svg",
                }));
        }
        return 2;
    }

    if (invocation.has("json")) {
        std::println("{}", icon_svg_json(*query, *result, *style));
        return 0;
    }

    std::println("{}", icon_catalog::svg_source(result->symbol, *style));
    return 0;
}

int run_icons_present(cppx::cli::Invocation const& invocation) {
    auto query = single_positional_text_or_error(
        invocation,
        "icons present",
        "name-or-reference");
    if (!query)
        return print_error("icons present", query.error(), invocation.has("json"));

    auto result = lookup_icon_symbol(*query);
    if (!result) {
        if (invocation.has("json")) {
            std::println("{}", icon_present_not_found_json(*query));
        } else {
            std::println(
                std::cerr,
                "{}",
                cppx::terminal::format_diagnostic({
                    .severity = cppx::terminal::DiagnosticSeverity::error,
                    .message = std::format(
                        "symbol '{}' was not found; run 'phenotype icons catalog' to list built-in symbols",
                        *query),
                    .context = "icons present",
                }));
        }
        return 2;
    }

    auto role = icon_presentation_role_from_invocation(invocation, result->symbol);
    if (!role)
        return print_error("icons present", role.error(), invocation.has("json"));
    auto phase = icon_interaction_phase_from_invocation(invocation);
    if (!phase)
        return print_error("icons present", phase.error(), invocation.has("json"));
    auto style = icon_material_symbols_style_from_invocation(invocation);
    if (!style)
        return print_error("icons present", style.error(), invocation.has("json"));
    auto axes = icon_material_symbol_axes_from_invocation(invocation);
    if (!axes)
        return print_error("icons present", axes.error(), invocation.has("json"));

    auto const selected = invocation.has("selected");
    auto const enabled = !invocation.has("disabled");
    if (invocation.has("json")) {
        std::println(
            "{}",
            icon_presentation_json(
                *query,
                *result,
                *role,
                *phase,
                *style,
                *axes,
                selected,
                enabled));
        return 0;
    }

    auto const desc = icon_catalog::descriptor(result->symbol);
    auto const metrics = icon_catalog::metrics(*role);
    auto const recipe = icon_catalog::macos_state_recipe(
        *role,
        icon_catalog::SymbolInteractionState{selected, enabled},
        *phase);
    auto const visible_color =
        icon_color_with_opacity(recipe.symbol_color, recipe.symbol_opacity);
    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "symbol",
         .value = std::string{desc.name},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "semantic reference",
         .value = std::string{desc.semantic_reference_name},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "state",
         .value = std::format(
             "{} {} selected={} enabled={} style={} fill={} wght={} GRAD={} opsz={}",
             icon_catalog::symbol_presentation_role_name(*role),
             icon_catalog::symbol_interaction_phase_name(*phase),
             selected ? "true" : "false",
             enabled ? "true" : "false",
             icon_catalog::material_symbols_style_name(*style),
             axes->fill ? "true" : "false",
             axes->weight,
             axes->grade,
             axes->optical_size),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "tone",
         .value = std::string{icon_catalog::symbol_tone_name(recipe.symbol_tone)},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "visible rgba",
         .value = std::format(
             "{},{},{},{}",
             visible_color.r,
             visible_color.g,
             visible_color.b,
             visible_color.a),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "metrics",
         .value = std::format(
             "{}pt symbol, {}pt effective, {}pt hit target",
             metrics.point_size,
             metrics.point_size * recipe.scale,
             recipe.hit_target_size),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "asset policy",
         .value = std::string{icon_catalog::asset_policy()},
         .status = cppx::terminal::StatusKind::ok},
    };
    std::println("phenotype icons present");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    return 0;
}

int run_icons_render(cppx::cli::Invocation const& invocation) {
    auto query = single_positional_text_or_error(
        invocation,
        "icons render",
        "name-or-reference");
    if (!query)
        return print_error("icons render", query.error(), invocation.has("json"));

    auto result = lookup_icon_symbol(*query);
    if (!result) {
        if (invocation.has("json")) {
            std::println("{}", icon_render_not_found_json(*query));
        } else {
            std::println(
                std::cerr,
                "{}",
                cppx::terminal::format_diagnostic({
                    .severity = cppx::terminal::DiagnosticSeverity::error,
                    .message = std::format(
                        "symbol '{}' was not found; run 'phenotype icons catalog' to list built-in symbols",
                        *query),
                    .context = "icons render",
                }));
        }
        return 2;
    }

    auto role = icon_presentation_role_from_invocation(invocation, result->symbol);
    if (!role)
        return print_error("icons render", role.error(), invocation.has("json"));
    auto phase = icon_interaction_phase_from_invocation(invocation);
    if (!phase)
        return print_error("icons render", phase.error(), invocation.has("json"));
    auto style = icon_material_symbols_style_from_invocation(invocation);
    if (!style)
        return print_error("icons render", style.error(), invocation.has("json"));
    auto axes = icon_material_symbol_axes_from_invocation(invocation);
    if (!axes)
        return print_error("icons render", axes.error(), invocation.has("json"));

    auto const selected = invocation.has("selected");
    auto const enabled = !invocation.has("disabled");
    auto source = rendered_icon_svg_source(
        *result,
        *role,
        *phase,
        selected,
        enabled,
        *style,
        *axes);
    auto output_path = fs::path{};
    if (auto output = invocation.value("output")) {
        output_path =
            fs::path{absolute_path_string(fs::path{std::string{*output}})};
        auto error = std::string{};
        if (!write_text_file(output_path, source, error)) {
            return print_error(
                "icons render",
                std::format(
                    "failed to write rendered icon SVG to {}: {}",
                    path_string(output_path),
                    error),
                invocation.has("json"));
        }
    }
    if (invocation.has("json")) {
        std::println(
            "{}",
            icon_render_json(
                *query,
                *result,
                *role,
                *phase,
                selected,
                enabled,
                *style,
                *axes,
                source,
                output_path));
        return 0;
    }

    if (!output_path.empty()) {
        auto lines = std::vector<cppx::terminal::StatusLine>{
            {.label = "symbol",
             .value = std::string{icon_catalog::name(result->symbol)},
             .status = cppx::terminal::StatusKind::ok},
            {.label = "output",
             .value = path_string(output_path),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "bytes",
             .value = std::format("{}", source.size()),
             .status = cppx::terminal::StatusKind::ok},
        };
        std::println("phenotype icons render");
        std::println("{}", cppx::terminal::format_status_frame(lines, false));
        return 0;
    }

    std::println("{}", source);
    return 0;
}

int run_svg_inspect(cppx::cli::Invocation const& invocation) {
    auto path_value = single_positional_text_or_error(
        invocation,
        "svg inspect",
        "path");
    if (!path_value)
        return print_error("svg inspect", path_value.error(), invocation.has("json"));

    auto path = fs::path{std::string{*path_value}};
    auto ec = std::error_code{};
    if (!fs::is_regular_file(path, ec) || ec) {
        return print_error(
            "svg inspect",
            std::format(
                "SVG file '{}' is not readable",
                path_string(path)),
            invocation.has("json"));
    }

    auto source = read_text_file(path);
    auto inspection = svg_contract::inspect_source(source);
    auto const summary = inspection.summary;
    if (invocation.has("json")) {
        std::println("{}", svg_inspect_json(path, source, inspection));
        return summary.paintable ? 0 : 2;
    }

    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "path",
         .value = path_string(path),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "subset",
         .value = std::string{svg_contract::subset_policy()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "viewBox",
         .value = std::format(
             "{} {} {} {}",
             summary.view_min_x,
             summary.view_min_y,
             summary.view_width,
             summary.view_height),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "shapes",
         .value = std::format("{}", summary.shape_count),
         .status = summary.paintable
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "unsupported",
         .value = std::format("{}", summary.unsupported_count),
         .status = summary.unsupported_count == 0
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "diagnostics",
         .value = std::format("{}", summary.diagnostic_count),
         .status = summary.diagnostic_count == 0
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "render path",
         .value = std::string{svg_contract::render_pipeline_policy()},
         .status = cppx::terminal::StatusKind::ok},
    };
    std::println("phenotype svg inspect");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    return summary.paintable ? 0 : 2;
}

}

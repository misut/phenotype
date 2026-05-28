export module phenotype_cli.icons_render;

import phenotype.icon_catalog;
import phenotype_cli.common;
import phenotype_cli.icons_common;
import std;

namespace fs = std::filesystem;
namespace icon_catalog = phenotype::icon_catalog;

export namespace phenotype_cli::icons {

using namespace phenotype_cli::common;

auto xml_attribute_escape(std::string_view value) -> std::string {
    auto out = std::string{};
    out.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
        case '&':  out += "&amp;"; break;
        case '<':  out += "&lt;"; break;
        case '>':  out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default:   out.push_back(ch); break;
        }
    }
    return out;
}

auto svg_number(float value) -> std::string {
    return std::format("{:.2f}", value);
}

auto svg_opacity(unsigned char alpha) -> std::string {
    return std::format("{:.3f}", static_cast<float>(alpha) / 255.0f);
}

auto svg_rgb(icon_catalog::SymbolColor color) -> std::string {
    return std::format(
        "rgb({},{},{})",
        static_cast<int>(color.r),
        static_cast<int>(color.g),
        static_cast<int>(color.b));
}

auto icon_svg_inner_source(std::string_view source) -> std::string_view {
    auto const start = source.find('>');
    auto const end = source.rfind("</svg>");
    if (start == std::string_view::npos || end == std::string_view::npos
        || end <= start)
        return source;
    return source.substr(start + 1, end - start - 1);
}

auto rendered_icon_svg_source(IconLookupResult const& result,
                              icon_catalog::SymbolPresentationRole role,
                              icon_catalog::SymbolInteractionPhase phase,
                              bool selected,
                              bool enabled,
                              icon_catalog::MaterialSymbolsStyle style)
        -> std::string {
    auto const desc = icon_catalog::descriptor(result.symbol, style);
    auto const metrics = icon_catalog::metrics(role);
    auto const recipe = icon_catalog::macos_state_recipe(
        role,
        icon_catalog::SymbolInteractionState{selected, enabled},
        phase);
    auto const visible_color =
        icon_color_with_opacity(recipe.symbol_color, recipe.symbol_opacity);
    auto const effective_point_size = metrics.point_size * recipe.scale;
    auto const content_inset =
        (recipe.hit_target_size - effective_point_size) / 2.0f;
    auto const scale = effective_point_size / 24.0f;
    auto const inner = icon_svg_inner_source(
        icon_catalog::svg_source(result.symbol, style));

    auto out = std::format(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"{}\" height=\"{}\" viewBox=\"0 0 {} {}\" role=\"img\" aria-label=\"{}\" data-phenotype-symbol=\"{}\" data-semantic-reference=\"{}\" data-style=\"{}\" data-material-symbols-style=\"{}\" data-role=\"{}\" data-phase=\"{}\" data-asset-policy=\"{}\">",
        svg_number(recipe.hit_target_size),
        svg_number(recipe.hit_target_size),
        svg_number(recipe.hit_target_size),
        svg_number(recipe.hit_target_size),
        xml_attribute_escape(desc.semantic_reference_name),
        xml_attribute_escape(desc.name),
        xml_attribute_escape(desc.semantic_reference_name),
        xml_attribute_escape(desc.style),
        xml_attribute_escape(icon_catalog::material_symbols_style_name(style)),
        xml_attribute_escape(icon_catalog::symbol_presentation_role_name(role)),
        xml_attribute_escape(icon_catalog::symbol_interaction_phase_name(phase)),
        xml_attribute_escape(icon_catalog::asset_policy()));
    if (recipe.background_color.a > 0) {
        out += std::format(
            "<rect x=\"0\" y=\"0\" width=\"{}\" height=\"{}\" rx=\"{}\" fill=\"{}\" fill-opacity=\"{}\"/>",
            svg_number(recipe.hit_target_size),
            svg_number(recipe.hit_target_size),
            svg_number(recipe.corner_radius),
            svg_rgb(recipe.background_color),
            svg_opacity(recipe.background_color.a));
    }
    out += std::format(
        "<g transform=\"translate({} {}) scale({})\" color=\"{}\" opacity=\"{}\">",
        svg_number(content_inset),
        svg_number(content_inset + metrics.optical_y_offset),
        svg_number(scale),
        svg_rgb(visible_color),
        svg_opacity(visible_color.a));
    out += inner;
    out += "</g></svg>";
    return out;
}

auto icon_render_json(std::string_view query,
                      IconLookupResult const& result,
                      icon_catalog::SymbolPresentationRole role,
                      icon_catalog::SymbolInteractionPhase phase,
                      bool selected,
                      bool enabled,
                      icon_catalog::MaterialSymbolsStyle style,
                      std::string_view source,
                      fs::path const& output_path) -> std::string {
    auto const desc = icon_catalog::descriptor(result.symbol, style);
    auto const metrics = icon_catalog::metrics(role);
    auto const recipe = icon_catalog::macos_state_recipe(
        role,
        icon_catalog::SymbolInteractionState{selected, enabled},
        phase);
    auto const visible_color =
        icon_color_with_opacity(recipe.symbol_color, recipe.symbol_opacity);
    auto const source_attribution =
        icon_catalog::source_attribution(result.symbol, style);
    auto const output_json = output_path.empty()
        ? std::string{"null"}
        : std::format(
              "{{\"path\":{},\"written\":true,\"bytes\":{}}}",
              json_string(path_string(output_path)),
              source.size());
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons render\","
        "\"ok\":true,\"query\":{},\"match_kind\":{},"
        "\"symbol\":{},\"semantic_reference_name\":{},"
        "\"style\":{},\"material_symbols_style\":{},"
        "\"asset_policy\":{},\"source_license_policy\":{},"
        "\"apple_asset_boundary\":{},\"source_attribution\":{},"
        "\"state\":{{\"role\":{},\"phase\":{},"
        "\"selected\":{},\"enabled\":{}}},"
        "\"presentation\":{{\"policy\":{},\"symbol_tone\":{},"
        "\"visible_symbol_color\":{},\"background_color\":{},"
        "\"point_size\":{},\"effective_point_size\":{},"
        "\"hit_target_size\":{},\"corner_radius\":{},"
        "\"likely_layer\":\"icon_glyph\","
        "\"likely_pass\":\"standalone_svg_wrapper\"}},"
        "\"svg\":{{\"source_format\":\"svg\",\"source\":{},"
        "\"bytes\":{},\"view_box\":{},\"width\":{},\"height\":{}}},"
        "\"output\":{}}}",
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
        json_string(icon_catalog::symbol_presentation_role_name(role)),
        json_string(icon_catalog::symbol_interaction_phase_name(phase)),
        selected ? "true" : "false",
        enabled ? "true" : "false",
        json_string(recipe.policy),
        json_string(icon_catalog::symbol_tone_name(recipe.symbol_tone)),
        icon_color_json(visible_color),
        icon_color_json(recipe.background_color),
        metrics.point_size,
        metrics.point_size * recipe.scale,
        recipe.hit_target_size,
        recipe.corner_radius,
        json_string(source),
        source.size(),
        json_string(std::format(
            "0 0 {} {}",
            svg_number(recipe.hit_target_size),
            svg_number(recipe.hit_target_size))),
        recipe.hit_target_size,
        recipe.hit_target_size,
        output_json);
}

}

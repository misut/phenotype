export module phenotype_cli.icon_sources;

import cppx.cli;
import cppx.terminal;
import phenotype.icon_catalog;
import phenotype_cli.common;
import std;

namespace icon_catalog = phenotype::icon_catalog;

export namespace phenotype_cli::icon_sources {

using namespace phenotype_cli::common;

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

auto icon_symbols_using_source_json(std::string_view family,
                                    std::string_view icon_name)
        -> std::string {
    auto out = std::string{"["};
    bool first = true;
    for (unsigned int i = 0; i < icon_catalog::all_symbol_count; ++i) {
        auto const symbol = icon_catalog::symbol_at(i);
        auto const source = icon_catalog::source_attribution(symbol);
        if (source.family != family || source.icon_name != icon_name)
            continue;
        if (!first)
            out += ",";
        first = false;
        out += json_string(icon_catalog::name(symbol));
    }
    out += "]";
    return out;
}

auto icon_material_symbols_style_source_json(
        std::string_view icon_name,
        icon_catalog::MaterialSymbolsStyle style) -> std::string {
    auto source_url = icon_catalog::material_symbols_source_url(icon_name, style);
    auto const direct_raw =
        source_url.find("raw.githubusercontent.com") != std::string_view::npos
        && source_url.find("google/material-design-icons") != std::string_view::npos
        && source_url.find(icon_catalog::material_symbols_source_revision())
            != std::string_view::npos
        && source_url.find(icon_catalog::material_symbols_source_directory(style))
            != std::string_view::npos
        && source_url.find("/blob/") == std::string_view::npos
        && source_url.find("/main/") == std::string_view::npos;
    return std::format(
        "{{\"style\":{},\"label\":{},\"font_family\":{},"
        "\"css_class\":{},\"source_directory\":{},"
        "\"source_url\":{},\"direct_raw_url\":{}}}",
        json_string(icon_catalog::material_symbols_style_name(style)),
        json_string(icon_catalog::material_symbols_style_label(style)),
        json_string(icon_catalog::material_symbols_font_family(style)),
        json_string(icon_catalog::material_symbols_css_class(style)),
        json_string(icon_catalog::material_symbols_source_directory(style)),
        json_string(source_url),
        direct_raw ? "true" : "false");
}

auto icon_material_symbols_style_sources_json(
        std::string_view icon_name) -> std::string {
    auto out = std::string{"["};
    for (unsigned int i = 0; i < icon_catalog::material_symbols_style_count; ++i) {
        if (i > 0)
            out += ",";
        out += icon_material_symbols_style_source_json(
            icon_name,
            icon_catalog::material_symbols_style_at(i));
    }
    out += "]";
    return out;
}

auto icon_material_symbols_source_record_json(std::string_view icon_name)
        -> std::string {
    auto source_url = icon_catalog::material_symbols_source_url(icon_name);
    auto const direct_raw =
        source_url.find("raw.githubusercontent.com") != std::string_view::npos
        && source_url.find("google/material-design-icons") != std::string_view::npos
        && source_url.find(icon_catalog::material_symbols_source_revision())
            != std::string_view::npos
        && source_url.find("/blob/") == std::string_view::npos
        && source_url.find("/main/") == std::string_view::npos;
    return std::format(
        "{{\"family\":\"Google Material Symbols\",\"icon_name\":{},\"license\":{},"
        "\"default_style\":{},\"styles\":{},"
        "\"license_url\":{},\"source_url\":{},"
        "\"source_revision\":{},\"direct_raw_url\":{},"
        "\"embedded_source\":true,\"modified_for_phenotype\":true,"
        "\"apple_asset\":false,\"platform_extracted\":false,"
        "\"runtime_fetch_required\":false,\"used_by_symbols\":{}}}",
        json_string(icon_name),
        json_string(icon_catalog::material_symbols_icon_license(icon_name)),
        json_string(icon_catalog::material_symbols_style_name(
            icon_catalog::default_material_symbols_style())),
        icon_material_symbols_style_sources_json(icon_name),
        json_string(icon_catalog::material_symbols_license_url()),
        json_string(source_url),
        json_string(icon_catalog::material_symbols_source_revision()),
        direct_raw ? "true" : "false",
        icon_symbols_using_source_json("Google Material Symbols", icon_name));
}

auto icon_material_symbols_source_records_json() -> std::string {
    auto out = std::string{"["};
    for (unsigned int i = 0;
         i < icon_catalog::material_symbols_unique_source_icon_count;
         ++i) {
        if (i > 0)
            out += ",";
        out += icon_material_symbols_source_record_json(
            icon_catalog::material_symbols_source_icon_name_at(i));
    }
    out += "]";
    return out;
}

auto icon_phenotype_owned_symbols_json() -> std::string {
    auto out = std::string{"["};
    bool first = true;
    for (unsigned int i = 0; i < icon_catalog::all_symbol_count; ++i) {
        auto const symbol = icon_catalog::symbol_at(i);
        auto const desc = icon_catalog::descriptor(symbol);
        if (!desc.phenotype_owned)
            continue;
        if (!first)
            out += ",";
        first = false;
        out += json_string(desc.name);
    }
    out += "]";
    return out;
}

auto icon_source_audit_checks() -> std::vector<Check> {
    unsigned int direct_raw_url_count = 0;
    unsigned int source_url_failure_count = 0;
    unsigned int material_symbols_usage_count = 0;
    unsigned int phenotype_owned_count = 0;
    unsigned int apple_asset_count = 0;
    unsigned int platform_extracted_count = 0;
    unsigned int runtime_fetch_count = 0;
    bool material_symbols_names_unique = true;
    for (unsigned int i = 0;
         i < icon_catalog::material_symbols_unique_source_icon_count;
         ++i) {
        auto const icon_name = icon_catalog::material_symbols_source_icon_name_at(i);
        for (unsigned int style_index = 0;
             style_index < icon_catalog::material_symbols_style_count;
             ++style_index) {
            auto const style = icon_catalog::material_symbols_style_at(style_index);
            auto const source_url =
                icon_catalog::material_symbols_source_url(icon_name, style);
            auto const direct_raw =
                source_url.find("raw.githubusercontent.com")
                    != std::string_view::npos
                && source_url.find(icon_catalog::material_symbols_source_revision())
                    != std::string_view::npos
                && source_url.find(
                       icon_catalog::material_symbols_source_directory(style))
                    != std::string_view::npos
                && source_url.find("/blob/") == std::string_view::npos
                && source_url.find("/main/") == std::string_view::npos;
            if (direct_raw)
                ++direct_raw_url_count;
            else
                ++source_url_failure_count;
        }
        for (unsigned int j = 0; j < i; ++j) {
            material_symbols_names_unique =
                material_symbols_names_unique
                && icon_catalog::material_symbols_source_icon_name_at(j)
                    != icon_name;
        }
    }
    for (unsigned int i = 0; i < icon_catalog::all_symbol_count; ++i) {
        auto const symbol = icon_catalog::symbol_at(i);
        auto const desc = icon_catalog::descriptor(symbol);
        auto const source = icon_catalog::source_attribution(symbol);
        if (desc.phenotype_owned)
            ++phenotype_owned_count;
        if (icon_catalog::uses_material_symbols_source(symbol))
            ++material_symbols_usage_count;
        if (source.apple_asset || desc.uses_sf_symbols_asset)
            ++apple_asset_count;
        if (source.platform_extracted)
            ++platform_extracted_count;
        if (source.runtime_fetch_required)
            ++runtime_fetch_count;
    }
    return {
        {.name = "pinned_direct_svg_sources",
         .ok = direct_raw_url_count
                == icon_catalog::material_symbols_source_variant_count
            && source_url_failure_count == 0
            && material_symbols_names_unique,
         .detail = std::format(
             "material_symbols_unique={} styles={} variants={} direct_raw={} failures={} revision={}",
             icon_catalog::material_symbols_unique_source_icon_count,
             icon_catalog::material_symbols_style_count,
             icon_catalog::material_symbols_source_variant_count,
             direct_raw_url_count,
             source_url_failure_count,
             icon_catalog::material_symbols_source_revision()),
         .hint =
             "Use pinned direct raw SVG URLs with an immutable source revision, not blob/main URLs."},
        {.name = "embedded_asset_boundary",
         .ok = material_symbols_usage_count == icon_catalog::material_symbols_source_symbol_count
            && phenotype_owned_count
                == icon_catalog::phenotype_owned_symbol_count
            && apple_asset_count == 0
            && platform_extracted_count == 0
            && runtime_fetch_count == 0,
         .detail = std::format(
             "material_symbols={} phenotype_owned={} apple_asset={} platform_extracted={} runtime_fetch={}",
             material_symbols_usage_count,
             phenotype_owned_count,
             apple_asset_count,
             platform_extracted_count,
             runtime_fetch_count),
         .hint =
             "Keep Apple/SF Symbols as semantic references only; do not embed or extract platform artwork."},
        {.name = "reference_policy",
         .ok = icon_catalog::reference_source_count == 6
            && icon_catalog::reference_source_at(3)
                   .used_as_embedded_asset_source
            && icon_catalog::reference_source_at(3).may_embed_svg_source
            && icon_catalog::reference_source_at(3).requires_notice
            && icon_catalog::reference_source_at(3).name
                == std::string_view{"Google Material Symbols"}
            && icon_catalog::reference_source_at(3).license_policy.find("Apache-2.0")
                != std::string_view::npos
            && icon_catalog::reference_source_at(3).license_url.find("google/material-design-icons")
                != std::string_view::npos
            && !icon_catalog::reference_source_at(0).may_embed_svg_source
            && !icon_catalog::reference_source_at(0).runtime_fetch_allowed
            && !icon_catalog::reference_source_at(0).platform_extraction_allowed
            && icon_catalog::reference_source_at(4).license_url.find("tabler-icons")
                != std::string_view::npos
            && icon_catalog::reference_source_at(5).license_url.find("iconoir-icons")
                != std::string_view::npos
            && icon_catalog::source_acquisition_policy().find(
                   "platform icon extraction disabled")
                != std::string_view::npos,
         .detail = std::format(
             "references={} embedded_source={} license_url={} acquisition={}",
             icon_catalog::reference_source_count,
             icon_catalog::reference_source_at(3).name,
             icon_catalog::reference_source_at(3).license_url,
             icon_catalog::source_acquisition_policy()),
         .hint =
             "Document every reference source with license URL, acquisition mode, embedding permission, notice requirement, and platform-extraction boundary."},
    };
}

auto icon_sources_json(std::span<Check const> checks) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons sources\","
        "\"ok\":{},\"policy\":{{\"asset_policy\":{},"
        "\"source_license_policy\":{},"
        "\"preferred_external_source_policy\":{},"
        "\"source_acquisition_policy\":{},"
        "\"source_attribution_policy\":{},"
        "\"apple_asset_boundary\":{}}},"
        "\"counts\":{{\"all_symbols\":{},\"audited_symbols\":{},"
        "\"phenotype_owned_symbols\":{},\"material_symbols_source_symbols\":{},"
        "\"material_symbols_unique_source_icons\":{},\"apple_asset_symbols\":{},"
        "\"platform_extracted_symbols\":{},\"runtime_fetched_symbols\":{},"
        "\"reference_sources\":{}}},"
        "\"source_sets\":{{\"material_symbols\":{{\"family\":\"Google Material Symbols\","
        "\"source_revision\":{},\"license_url\":{},"
        "\"unique_source_icon_count\":{},\"style_count\":{},"
        "\"source_variant_count\":{},\"used_symbol_count\":{},"
        "\"records\":{}}},"
        "\"phenotype\":{{\"family\":\"phenotype\","
        "\"license\":\"MIT\",\"license_url\":{},"
        "\"source_revision\":\"phenotype-repository\","
        "\"used_symbol_count\":{},\"symbols\":{}}}}},"
        "\"reference_sources\":{},\"checks\":{}}}",
        all_ok(checks) ? "true" : "false",
        json_string(icon_catalog::asset_policy()),
        json_string(icon_catalog::source_license_policy()),
        json_string(icon_catalog::preferred_external_source_policy()),
        json_string(icon_catalog::source_acquisition_policy()),
        json_string(icon_catalog::source_attribution_policy()),
        json_string(icon_catalog::apple_asset_boundary()),
        icon_catalog::all_symbol_count,
        icon_catalog::audited_symbol_source_count,
        icon_catalog::phenotype_owned_symbol_count,
        icon_catalog::material_symbols_source_symbol_count,
        icon_catalog::material_symbols_unique_source_icon_count,
        icon_catalog::apple_asset_symbol_count,
        icon_catalog::platform_extracted_symbol_count,
        icon_catalog::runtime_fetched_symbol_count,
        icon_catalog::reference_source_count,
        json_string(icon_catalog::material_symbols_source_revision()),
        json_string(icon_catalog::material_symbols_license_url()),
        icon_catalog::material_symbols_unique_source_icon_count,
        icon_catalog::material_symbols_style_count,
        icon_catalog::material_symbols_source_variant_count,
        icon_catalog::material_symbols_source_symbol_count,
        icon_material_symbols_source_records_json(),
        json_string("https://github.com/misut/phenotype/blob/main/LICENSE"),
        icon_catalog::phenotype_owned_symbol_count,
        icon_phenotype_owned_symbols_json(),
        icon_reference_sources_json(),
        checks_json(checks));
}

int run_icons_sources(cppx::cli::Invocation const& invocation) {
    auto checks = icon_source_audit_checks();
    if (invocation.has("json")) {
        std::println("{}", icon_sources_json(checks));
        return all_ok(checks) ? 0 : 1;
    }

    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "embedded",
         .value = std::format(
             "{} Google Material Symbols-backed symbols from {} unique pinned SVGs",
             icon_catalog::material_symbols_source_symbol_count,
             icon_catalog::material_symbols_unique_source_icon_count),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "owned",
         .value = std::format(
             "{} phenotype-owned symbols",
             icon_catalog::phenotype_owned_symbol_count),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "revision",
         .value = std::string{icon_catalog::material_symbols_source_revision()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "license",
         .value = std::string{icon_catalog::source_license_policy()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "Apple boundary",
         .value = std::string{icon_catalog::apple_asset_boundary()},
         .status = all_ok(checks) ? cppx::terminal::StatusKind::ok
                                  : cppx::terminal::StatusKind::fail},
    };
    std::println("phenotype icons sources");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    print_checks("checks", checks);
    return all_ok(checks) ? 0 : 1;
}

} // namespace phenotype_cli::icon_sources

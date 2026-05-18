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
        "\"used_as_embedded_asset_source\":{},"
        "\"apple_owned_artwork\":{}}}",
        json_string(source.name),
        json_string(source.url),
        json_string(source.role),
        json_string(source.license_policy),
        source.used_as_embedded_asset_source ? "true" : "false",
        source.apple_owned_artwork ? "true" : "false");
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

auto icon_lucide_source_record_json(std::string_view icon_name)
        -> std::string {
    auto source_url = icon_catalog::lucide_source_url(icon_name);
    auto const direct_raw =
        source_url.find("raw.githubusercontent.com") != std::string_view::npos
        && source_url.find(icon_catalog::lucide_source_revision())
            != std::string_view::npos
        && source_url.find("/blob/") == std::string_view::npos
        && source_url.find("/main/") == std::string_view::npos;
    return std::format(
        "{{\"family\":\"Lucide\",\"icon_name\":{},\"license\":{},"
        "\"license_url\":{},\"source_url\":{},"
        "\"source_revision\":{},\"direct_raw_url\":{},"
        "\"embedded_source\":true,\"modified_for_phenotype\":true,"
        "\"apple_asset\":false,\"platform_extracted\":false,"
        "\"runtime_fetch_required\":false,\"used_by_symbols\":{}}}",
        json_string(icon_name),
        json_string(icon_catalog::lucide_icon_license(icon_name)),
        json_string(icon_catalog::lucide_license_url()),
        json_string(source_url),
        json_string(icon_catalog::lucide_source_revision()),
        direct_raw ? "true" : "false",
        icon_symbols_using_source_json("Lucide", icon_name));
}

auto icon_lucide_source_records_json() -> std::string {
    auto out = std::string{"["};
    for (unsigned int i = 0;
         i < icon_catalog::lucide_unique_source_icon_count;
         ++i) {
        if (i > 0)
            out += ",";
        out += icon_lucide_source_record_json(
            icon_catalog::lucide_source_icon_name_at(i));
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
    unsigned int lucide_usage_count = 0;
    unsigned int phenotype_owned_count = 0;
    unsigned int apple_asset_count = 0;
    unsigned int platform_extracted_count = 0;
    unsigned int runtime_fetch_count = 0;
    bool lucide_names_unique = true;
    for (unsigned int i = 0;
         i < icon_catalog::lucide_unique_source_icon_count;
         ++i) {
        auto const icon_name = icon_catalog::lucide_source_icon_name_at(i);
        auto const source_url = icon_catalog::lucide_source_url(icon_name);
        auto const direct_raw =
            source_url.find("raw.githubusercontent.com")
                != std::string_view::npos
            && source_url.find(icon_catalog::lucide_source_revision())
                != std::string_view::npos
            && source_url.find("/blob/") == std::string_view::npos
            && source_url.find("/main/") == std::string_view::npos;
        if (direct_raw)
            ++direct_raw_url_count;
        else
            ++source_url_failure_count;
        for (unsigned int j = 0; j < i; ++j) {
            lucide_names_unique =
                lucide_names_unique
                && icon_catalog::lucide_source_icon_name_at(j)
                    != icon_name;
        }
    }
    for (unsigned int i = 0; i < icon_catalog::all_symbol_count; ++i) {
        auto const symbol = icon_catalog::symbol_at(i);
        auto const desc = icon_catalog::descriptor(symbol);
        auto const source = icon_catalog::source_attribution(symbol);
        if (desc.phenotype_owned)
            ++phenotype_owned_count;
        if (icon_catalog::uses_lucide_source(symbol))
            ++lucide_usage_count;
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
                == icon_catalog::lucide_unique_source_icon_count
            && source_url_failure_count == 0
            && lucide_names_unique,
         .detail = std::format(
             "lucide_unique={} direct_raw={} failures={} revision={}",
             icon_catalog::lucide_unique_source_icon_count,
             direct_raw_url_count,
             source_url_failure_count,
             icon_catalog::lucide_source_revision()),
         .hint =
             "Use pinned direct raw SVG URLs with an immutable source revision, not blob/main URLs."},
        {.name = "embedded_asset_boundary",
         .ok = lucide_usage_count == icon_catalog::lucide_source_symbol_count
            && phenotype_owned_count
                == icon_catalog::phenotype_owned_symbol_count
            && apple_asset_count == 0
            && platform_extracted_count == 0
            && runtime_fetch_count == 0,
         .detail = std::format(
             "lucide_symbols={} phenotype_owned={} apple_asset={} platform_extracted={} runtime_fetch={}",
             lucide_usage_count,
             phenotype_owned_count,
             apple_asset_count,
             platform_extracted_count,
             runtime_fetch_count),
         .hint =
             "Keep Apple/SF Symbols as semantic references only; do not embed or extract platform artwork."},
        {.name = "reference_policy",
         .ok = icon_catalog::reference_source_count == 8
            && icon_catalog::reference_source_at(3)
                   .used_as_embedded_asset_source
            && icon_catalog::reference_source_at(3).name
                == std::string_view{"Lucide"}
            && icon_catalog::reference_source_at(3).license_policy.find("ISC")
                != std::string_view::npos
            && icon_catalog::source_acquisition_policy().find(
                   "platform icon extraction disabled")
                != std::string_view::npos,
         .detail = std::format(
             "references={} embedded_source={} acquisition={}",
             icon_catalog::reference_source_count,
             icon_catalog::reference_source_at(3).name,
             icon_catalog::source_acquisition_policy()),
         .hint =
             "Document every reference source and mark which one is allowed as an embedded asset source."},
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
        "\"phenotype_owned_symbols\":{},\"lucide_source_symbols\":{},"
        "\"lucide_unique_source_icons\":{},\"apple_asset_symbols\":{},"
        "\"platform_extracted_symbols\":{},\"runtime_fetched_symbols\":{},"
        "\"reference_sources\":{}}},"
        "\"source_sets\":{{\"lucide\":{{\"family\":\"Lucide\","
        "\"source_revision\":{},\"license_url\":{},"
        "\"unique_source_icon_count\":{},\"used_symbol_count\":{},"
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
        icon_catalog::lucide_source_symbol_count,
        icon_catalog::lucide_unique_source_icon_count,
        icon_catalog::apple_asset_symbol_count,
        icon_catalog::platform_extracted_symbol_count,
        icon_catalog::runtime_fetched_symbol_count,
        icon_catalog::reference_source_count,
        json_string(icon_catalog::lucide_source_revision()),
        json_string(icon_catalog::lucide_license_url()),
        icon_catalog::lucide_unique_source_icon_count,
        icon_catalog::lucide_source_symbol_count,
        icon_lucide_source_records_json(),
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
             "{} Lucide-backed symbols from {} unique pinned SVGs",
             icon_catalog::lucide_source_symbol_count,
             icon_catalog::lucide_unique_source_icon_count),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "owned",
         .value = std::format(
             "{} phenotype-owned symbols",
             icon_catalog::phenotype_owned_symbol_count),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "revision",
         .value = std::string{icon_catalog::lucide_source_revision()},
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

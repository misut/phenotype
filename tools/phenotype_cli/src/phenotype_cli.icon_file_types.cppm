export module phenotype_cli.icon_file_types;

import cppx.cli;
import cppx.terminal;
import file_explorer_shared;
import phenotype.icon_catalog;
import phenotype_cli.common;
import std;

namespace icon_catalog = phenotype::icon_catalog;

export namespace phenotype_cli::icon_file_types {

using namespace phenotype_cli::common;

auto direct_pinned_raw_source(std::string_view source_url) -> bool {
    return source_url.find("raw.githubusercontent.com")
               != std::string_view::npos
        && source_url.find(icon_catalog::lucide_source_revision())
               != std::string_view::npos
        && source_url.find("/blob/") == std::string_view::npos
        && source_url.find("/main/") == std::string_view::npos;
}

auto file_type_icon_source_json(
        icon_catalog::SymbolSourceAttribution const& source) -> std::string {
    return std::format(
        "{{\"family\":{},\"icon_name\":{},\"license\":{},"
        "\"license_url\":{},\"source_url\":{},\"source_revision\":{},"
        "\"copyright\":{},\"direct_pinned_raw_url\":{},"
        "\"embedded_source\":{},\"modified_for_phenotype\":{},"
        "\"apple_asset\":{},\"platform_extracted\":{},"
        "\"runtime_fetch_required\":{}}}",
        json_string(source.family),
        json_string(source.icon_name),
        json_string(source.license),
        json_string(source.license_url),
        json_string(source.source_url),
        json_string(source.source_revision),
        json_string(source.copyright),
        direct_pinned_raw_source(source.source_url) ? "true" : "false",
        source.embedded_source ? "true" : "false",
        source.modified_for_phenotype ? "true" : "false",
        source.apple_asset ? "true" : "false",
        source.platform_extracted ? "true" : "false",
        source.runtime_fetch_required ? "true" : "false");
}

auto file_type_icon_record_json(
        file_explorer_demo::ExplorerSidebarSymbol const& item)
        -> std::string {
    auto const source = icon_catalog::source_attribution(item.symbol);
    return std::format(
        "{{\"token\":{},\"symbol\":{},\"semantic_reference_name\":{},"
        "\"package_asset_name\":{},\"package_asset_source\":{},"
        "\"package_asset_policy\":{},\"source_attribution\":{}}}",
        json_string(item.token),
        json_string(icon_catalog::name(item.symbol)),
        json_string(icon_catalog::semantic_reference_name(item.symbol)),
        json_string(
            file_explorer_demo::file_type_icon_asset_name_for_symbol(
                item.symbol)),
        json_string(
            file_explorer_demo::file_type_icon_asset_source_for_symbol(
                item.symbol)),
        json_string(file_explorer_demo::file_type_icon_asset_policy()),
        file_type_icon_source_json(source));
}

auto file_type_icon_records_json() -> std::string {
    auto out = std::string{"["};
    auto const symbols = file_explorer_demo::file_type_symbol_contract();
    for (std::size_t i = 0; i < symbols.size(); ++i) {
        if (i > 0)
            out += ",";
        out += file_type_icon_record_json(symbols[i]);
    }
    out += "]";
    return out;
}

auto file_type_icon_checks() -> std::vector<Check> {
    auto const symbols = file_explorer_demo::file_type_symbol_contract();
    auto direct_source_count = 0u;
    auto lucide_source_count = 0u;
    auto apple_asset_count = 0u;
    auto platform_extracted_count = 0u;
    auto runtime_fetch_count = 0u;
    auto declared_package_source_count = 0u;

    for (auto const& item : symbols) {
        auto const source = icon_catalog::source_attribution(item.symbol);
        if (source.family == std::string_view{"Lucide"})
            ++lucide_source_count;
        if (direct_pinned_raw_source(source.source_url))
            ++direct_source_count;
        if (source.apple_asset)
            ++apple_asset_count;
        if (source.platform_extracted)
            ++platform_extracted_count;
        if (source.runtime_fetch_required)
            ++runtime_fetch_count;
        auto expected_source = std::string{"assets/icons/file-types/"};
        expected_source += item.token;
        expected_source += ".svg";
        if (file_explorer_demo::file_type_icon_asset_source_for_symbol(
                item.symbol)
            == expected_source) {
            ++declared_package_source_count;
        }
    }

    return {
        {.name = "file_type_icon_catalog",
         .ok = symbols.size() == icon_catalog::file_type_symbol_count,
         .detail = std::format(
             "file_type_symbols={} catalog_file_type_symbols={}",
             symbols.size(),
             icon_catalog::file_type_symbol_count),
         .hint =
             "Keep file_explorer_shared::file_type_symbol_contract aligned with phenotype.icon_catalog."},
        {.name = "file_type_icon_sources",
         .ok = lucide_source_count == symbols.size()
            && direct_source_count == symbols.size(),
         .detail = std::format(
             "lucide={} direct_raw={} revision={}",
             lucide_source_count,
             direct_source_count,
             icon_catalog::lucide_source_revision()),
         .hint =
             "File-type icons should map to pinned direct raw Lucide SVG sources before phenotype adaptation."},
        {.name = "file_type_icon_package_mapping",
         .ok = declared_package_source_count == symbols.size()
            && file_explorer_demo::file_type_icon_license_asset_source()
                == std::string_view{
                    "assets/icons/file-types/LUCIDE_LICENSE.txt"},
         .detail = std::format(
             "package_sources={} license_asset={}",
             declared_package_source_count,
             file_explorer_demo::file_type_icon_license_asset_source()),
         .hint =
             "Each file-type token should map to assets/icons/file-types/<token>.svg plus the Lucide license notice."},
        {.name = "apple_artwork_boundary",
         .ok = apple_asset_count == 0
            && platform_extracted_count == 0
            && runtime_fetch_count == 0,
         .detail = std::format(
             "apple_asset={} platform_extracted={} runtime_fetch={}",
             apple_asset_count,
             platform_extracted_count,
             runtime_fetch_count),
         .hint =
             "Use Apple HIG, Finder, and SF Symbols as semantic references only; do not embed or extract Apple-owned artwork."},
    };
}

auto file_type_icons_json(std::span<Check const> checks) -> std::string {
    auto const symbols = file_explorer_demo::file_type_symbol_contract();
    auto lucide_count = 0u;
    auto apple_asset_count = 0u;
    auto platform_extracted_count = 0u;
    auto runtime_fetch_count = 0u;
    for (auto const& item : symbols) {
        auto const source = icon_catalog::source_attribution(item.symbol);
        if (source.family == std::string_view{"Lucide"})
            ++lucide_count;
        if (source.apple_asset)
            ++apple_asset_count;
        if (source.platform_extracted)
            ++platform_extracted_count;
        if (source.runtime_fetch_required)
            ++runtime_fetch_count;
    }
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons file-types\","
        "\"ok\":{},\"policy\":{{\"asset_policy\":{},"
        "\"source_acquisition_policy\":{},\"apple_asset_boundary\":{},"
        "\"package_asset_policy\":{},\"license_asset\":{}}},"
        "\"counts\":{{\"file_type_symbols\":{},"
        "\"lucide_file_type_symbols\":{},\"apple_asset_symbols\":{},"
        "\"platform_extracted_symbols\":{},\"runtime_fetched_symbols\":{}}},"
        "\"source_revision\":{},\"records\":{},\"checks\":{}}}",
        all_ok(checks) ? "true" : "false",
        json_string(icon_catalog::asset_policy()),
        json_string(icon_catalog::source_acquisition_policy()),
        json_string(icon_catalog::apple_asset_boundary()),
        json_string(file_explorer_demo::file_type_icon_asset_policy()),
        json_string(file_explorer_demo::file_type_icon_license_asset_source()),
        symbols.size(),
        lucide_count,
        apple_asset_count,
        platform_extracted_count,
        runtime_fetch_count,
        json_string(icon_catalog::lucide_source_revision()),
        file_type_icon_records_json(),
        checks_json(checks));
}

int run_icons_file_types(cppx::cli::Invocation const& invocation) {
    auto checks = file_type_icon_checks();
    if (invocation.has("json")) {
        std::println("{}", file_type_icons_json(checks));
        return all_ok(checks) ? 0 : 1;
    }

    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "file types",
         .value = std::format(
             "{} runtime-visible SVG package icons",
             icon_catalog::file_type_symbol_count),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "source",
         .value = std::format(
             "Lucide pinned at {}",
             icon_catalog::lucide_source_revision()),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "license",
         .value = std::string{
             file_explorer_demo::file_type_icon_license_asset_source()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "Apple boundary",
         .value = std::string{icon_catalog::apple_asset_boundary()},
         .status = all_ok(checks) ? cppx::terminal::StatusKind::ok
                                  : cppx::terminal::StatusKind::fail},
    };
    std::println("phenotype icons file-types");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    print_checks("checks", checks);
    return all_ok(checks) ? 0 : 1;
}

} // namespace phenotype_cli::icon_file_types

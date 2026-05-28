export module phenotype_cli.icon_audit;

import cppx.cli;
import cppx.terminal;
import phenotype.icon_catalog;
import phenotype_cli.common;
import phenotype_cli.icon_file_types;
import phenotype_cli.icon_sources;
import phenotype_cli.icons_checks;
import std;

namespace icon_catalog = phenotype::icon_catalog;

export namespace phenotype_cli::icon_audit {

using namespace phenotype_cli::common;

struct IconCheckGroup {
    std::string_view name;
    std::vector<Check> checks;
};

auto icon_check_groups() -> std::vector<IconCheckGroup> {
    return {
        {.name = "catalog",
         .checks = phenotype_cli::icons::icon_catalog_checks()},
        {.name = "sources",
         .checks = phenotype_cli::icon_sources::icon_source_audit_checks()},
        {.name = "file_types",
         .checks = phenotype_cli::icon_file_types::file_type_icon_checks()},
    };
}

auto icon_groups_ok(std::span<IconCheckGroup const> groups) -> bool {
    return std::ranges::all_of(
        groups,
        [](IconCheckGroup const& group) {
            return all_ok(group.checks);
        });
}

auto icon_check_group_json(IconCheckGroup const& group) -> std::string {
    return std::format(
        "{{\"name\":{},\"ok\":{},\"checks\":{}}}",
        json_string(group.name),
        all_ok(group.checks) ? "true" : "false",
        checks_json(group.checks));
}

auto icon_check_groups_json(std::span<IconCheckGroup const> groups)
        -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < groups.size(); ++i) {
        if (i > 0)
            out += ",";
        out += icon_check_group_json(groups[i]);
    }
    out += "]";
    return out;
}

auto icon_check_json(std::span<IconCheckGroup const> groups)
        -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"icons check\","
        "\"ok\":{},\"policy\":{{\"asset_policy\":{},"
        "\"source_acquisition_policy\":{},\"source_attribution_policy\":{},"
        "\"apple_asset_boundary\":{},\"preferred_external_source_policy\":{}}},"
        "\"counts\":{{\"all_symbols\":{},\"material_symbols_source_symbols\":{},"
        "\"material_symbols_unique_source_icons\":{},\"file_type_symbols\":{},"
        "\"reference_sources\":{},\"apple_asset_symbols\":{},"
        "\"platform_extracted_symbols\":{},\"runtime_fetched_symbols\":{}}},"
        "\"source_revision\":{},\"groups\":{}}}",
        icon_groups_ok(groups) ? "true" : "false",
        json_string(icon_catalog::asset_policy()),
        json_string(icon_catalog::source_acquisition_policy()),
        json_string(icon_catalog::source_attribution_policy()),
        json_string(icon_catalog::apple_asset_boundary()),
        json_string(icon_catalog::preferred_external_source_policy()),
        icon_catalog::all_symbol_count,
        icon_catalog::material_symbols_source_symbol_count,
        icon_catalog::material_symbols_unique_source_icon_count,
        icon_catalog::file_type_symbol_count,
        icon_catalog::reference_source_count,
        icon_catalog::apple_asset_symbol_count,
        icon_catalog::platform_extracted_symbol_count,
        icon_catalog::runtime_fetched_symbol_count,
        json_string(icon_catalog::material_symbols_source_revision()),
        icon_check_groups_json(groups));
}

int run_icons_check(cppx::cli::Invocation const& invocation) {
    auto groups = icon_check_groups();
    if (invocation.has("json")) {
        std::println("{}", icon_check_json(groups));
        return icon_groups_ok(groups) ? 0 : 1;
    }

    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "symbols",
         .value = std::format(
             "{} total, {} Google Material Symbols-backed, {} file-type",
             icon_catalog::all_symbol_count,
             icon_catalog::material_symbols_source_symbol_count,
             icon_catalog::file_type_symbol_count),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "source",
         .value = std::format(
             "Google Material Symbols pinned at {}",
             icon_catalog::material_symbols_source_revision()),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "Apple boundary",
         .value = std::string{icon_catalog::apple_asset_boundary()},
         .status = icon_groups_ok(groups) ? cppx::terminal::StatusKind::ok
                                          : cppx::terminal::StatusKind::fail},
    };
    std::println("phenotype icons check");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    for (auto const& group : groups) {
        print_checks(std::string{group.name}, group.checks);
    }
    return icon_groups_ok(groups) ? 0 : 1;
}

} // namespace phenotype_cli::icon_audit

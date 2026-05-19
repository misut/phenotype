export module phenotype_cli.icons_svg_inspect;

import phenotype.svg_contract;
import phenotype_cli.common;
import std;

namespace fs = std::filesystem;
namespace svg_contract = phenotype::svg_contract;

export namespace phenotype_cli::icons {

using namespace phenotype_cli::common;

auto svg_document_summary_json(svg_contract::DocumentSummary const& summary,
                               std::span<std::string const> diagnostics)
        -> std::string {
    return std::format(
        "{{\"view_box\":{{\"min_x\":{},\"min_y\":{},\"width\":{},\"height\":{}}},"
        "\"shape_count\":{},\"diagnostic_count\":{},"
        "\"unsupported_count\":{},\"paintable\":{},"
        "\"has_diagnostics\":{},\"diagnostics\":{}}}",
        summary.view_min_x,
        summary.view_min_y,
        summary.view_width,
        summary.view_height,
        summary.shape_count,
        summary.diagnostic_count,
        summary.unsupported_count,
        summary.paintable ? "true" : "false",
        summary.has_diagnostics ? "true" : "false",
        string_array_json(diagnostics));
}

auto svg_support_json() -> std::string {
    return std::format(
        "{{\"subset_policy\":{},\"supported_elements\":{},"
        "\"supported_path_commands\":{},\"supported_style_attributes\":{},"
        "\"unsupported_policy\":{},\"render_pipeline_policy\":{}}}",
        json_string(svg_contract::subset_policy()),
        json_string(svg_contract::supported_elements()),
        json_string(svg_contract::supported_path_commands()),
        json_string(svg_contract::supported_style_attributes()),
        json_string(svg_contract::unsupported_policy()),
        json_string(svg_contract::render_pipeline_policy()));
}

auto svg_inspect_json(fs::path const& path,
                      std::string_view source,
                      svg_contract::DocumentInspection const& inspection)
        -> std::string {
    auto const& summary = inspection.summary;
    return std::format(
        "{{\"schema_version\":1,\"command\":\"svg inspect\","
        "\"ok\":{},\"source\":{{\"path\":{},\"bytes\":{}}},"
        "\"support\":{},\"document\":{},"
        "\"likely_layer\":\"svg_parser\","
        "\"likely_pass\":\"pure_svg_subset_parse\"}}",
        summary.paintable ? "true" : "false",
        json_string(path_string(path)),
        source.size(),
        svg_support_json(),
        svg_document_summary_json(summary, inspection.diagnostics));
}

}

export module phenotype_cli.file_explorer_json;

import file_explorer_shared;
import phenotype_cli.common;
import std;

namespace phenotype_cli::file_explorer {

using phenotype_cli::common::json_string;

export auto operation_receipt_json(
        file_explorer_demo::OperationReceipt const& receipt) -> std::string {
    auto const& plan = receipt.plan;
    auto plan_json = std::format(
        "{{\"kind\":{},\"display_name\":{},\"source\":{},"
        "\"destination\":{},\"executable\":{},\"sandboxed\":{},"
        "\"mutates_filesystem\":{},\"reads_file\":{},\"reads_directory\":{},"
        "\"writes_file\":{},"
        "\"creates_directory\":{},\"deletes_file\":{},"
        "\"deletes_directory\":{},\"moves_to_trash\":{},"
        "\"permanent_delete\":{},\"fallback_reason\":{}}}",
        json_string(plan.kind),
        json_string(plan.display_name),
        json_string(plan.source),
        json_string(plan.destination),
        plan.executable ? "true" : "false",
        plan.sandboxed ? "true" : "false",
        plan.mutates_filesystem ? "true" : "false",
        plan.reads_file ? "true" : "false",
        plan.reads_directory ? "true" : "false",
        plan.writes_file ? "true" : "false",
        plan.creates_directory ? "true" : "false",
        plan.deletes_file ? "true" : "false",
        plan.deletes_directory ? "true" : "false",
        plan.moves_to_trash ? "true" : "false",
        plan.permanent_delete ? "true" : "false",
        json_string(plan.fallback_reason));
    return std::format(
        "{{\"kind\":{},\"target\":{},\"ok\":{},\"detail\":{},"
        "\"plan\":{}}}",
        json_string(receipt.kind),
        json_string(receipt.target),
        receipt.ok ? "true" : "false",
        json_string(receipt.detail),
        plan_json);
}

} // namespace phenotype_cli::file_explorer

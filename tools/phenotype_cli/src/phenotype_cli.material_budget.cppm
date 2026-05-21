export module phenotype_cli.material_budget;

import cppx.process;
import json;
import phenotype_cli.common;
import std;

export namespace phenotype_cli::material_budget {

using namespace phenotype_cli::common;

struct MaterialBudgetSummary {
    std::int64_t plan_count = -1;
    std::int64_t material_instance_count = -1;
    std::int64_t sampled_backdrop_instance_count = -1;
    std::int64_t fallback_instance_count = -1;
    std::int64_t execution_stage_count = -1;
    std::int64_t backdrop_execution_stage_count = -1;
    std::int64_t draw_calls = -1;
    std::int64_t total_sample_taps = -1;
    std::int64_t upload_bytes = -1;
    std::int64_t buffer_capacity_bytes = -1;
    std::int64_t backdrop_copy_count = -1;
    std::int64_t backdrop_copy_pixels = -1;
    std::int64_t max_backdrop_pixels = -1;
    std::int64_t planned_surface_sample_pixels = -1;
    std::string upload_status;
    std::string draw_status;
    std::string backdrop_copy_policy;
    std::string backdrop_copy_skip_reason;
};

auto trim_copy(std::string_view text) -> std::string {
    auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return {};
    auto last = text.find_last_not_of(" \t\r\n");
    return std::string{text.substr(first, last - first + 1)};
}

auto parse_verifier_json(std::string_view text) -> std::optional<json::Value> {
    auto trimmed = trim_copy(text);
    if (trimmed.empty())
        return std::nullopt;
    try {
        return json::parse(trimmed);
    } catch (std::exception const&) {
    }

    auto end = trimmed.size();
    while (end > 0) {
        auto begin = trimmed.rfind('\n', end - 1);
        auto line = begin == std::string_view::npos
            ? std::string_view{trimmed}.substr(0, end)
            : std::string_view{trimmed}.substr(begin + 1, end - begin - 1);
        auto candidate = trim_copy(line);
        if (!candidate.empty() && candidate.front() == '{') {
            try {
                return json::parse(candidate);
            } catch (std::exception const&) {
            }
        }
        if (begin == std::string_view::npos)
            break;
        end = begin;
    }
    return std::nullopt;
}

auto material_budget_from_report(json::Value const& report)
    -> std::optional<MaterialBudgetSummary> {
    auto prefix = std::initializer_list<std::string_view>{
        "artifact_context",
        "material_contract",
        "executor_budget",
    };
    if (!json_at(report, prefix))
        return std::nullopt;

    return MaterialBudgetSummary{
        .plan_count = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "plan_count"}).value_or(-1),
        .material_instance_count = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "material_instance_count"}).value_or(-1),
        .sampled_backdrop_instance_count = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "sampled_backdrop_instance_count"}).value_or(-1),
        .fallback_instance_count = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "fallback_instance_count"}).value_or(-1),
        .execution_stage_count = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "execution_stage_count"}).value_or(-1),
        .backdrop_execution_stage_count = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "backdrop_execution_stage_count"}).value_or(-1),
        .draw_calls = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "draw_calls"}).value_or(-1),
        .total_sample_taps = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "total_sample_taps"}).value_or(-1),
        .upload_bytes = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "upload_bytes"}).value_or(-1),
        .buffer_capacity_bytes = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "buffer_capacity_bytes"}).value_or(-1),
        .backdrop_copy_count = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "backdrop_copy_count"}).value_or(-1),
        .backdrop_copy_pixels = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "backdrop_copy_pixels"}).value_or(-1),
        .max_backdrop_pixels = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "max_backdrop_pixels"}).value_or(-1),
        .planned_surface_sample_pixels = json_integer_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "planned_surface_sample_pixels"}).value_or(-1),
        .upload_status = json_string_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "upload_status"}).value_or("unknown"),
        .draw_status = json_string_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "draw_status"}).value_or("unknown"),
        .backdrop_copy_policy = json_string_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "backdrop_copy_policy"}).value_or("unknown"),
        .backdrop_copy_skip_reason = json_string_at(
            report,
            {"artifact_context", "material_contract", "executor_budget",
             "backdrop_copy_skip_reason"}).value_or("unknown"),
    };
}

auto material_budget_from_verifier(
        cppx::process::CapturedProcessResult const& result)
    -> std::optional<MaterialBudgetSummary> {
    if (result.stdout_text.empty())
        return std::nullopt;
    auto report = parse_verifier_json(result.stdout_text);
    if (!report)
        return std::nullopt;
    return material_budget_from_report(*report);
}

auto material_budget_from_verifier(
        std::optional<cppx::process::CapturedProcessResult> const& result)
    -> std::optional<MaterialBudgetSummary> {
    if (!result)
        return std::nullopt;
    return material_budget_from_verifier(*result);
}

auto budget_count(std::int64_t value) -> std::string {
    return value >= 0 ? std::format("{}", value) : std::string{"unknown"};
}

auto material_budget_json(std::optional<MaterialBudgetSummary> const& budget)
    -> std::string {
    if (!budget)
        return "null";
    return std::format(
        "{{\"plan_count\":{},\"material_instance_count\":{},"
        "\"sampled_backdrop_instance_count\":{},"
        "\"fallback_instance_count\":{},\"execution_stage_count\":{},"
        "\"backdrop_execution_stage_count\":{},\"draw_calls\":{},"
        "\"total_sample_taps\":{},\"upload_bytes\":{},"
        "\"buffer_capacity_bytes\":{},\"backdrop_copy_count\":{},"
        "\"backdrop_copy_pixels\":{},\"max_backdrop_pixels\":{},"
        "\"planned_surface_sample_pixels\":{},\"upload_status\":{},"
        "\"draw_status\":{},\"backdrop_copy_policy\":{},"
        "\"backdrop_copy_skip_reason\":{}}}",
        budget->plan_count,
        budget->material_instance_count,
        budget->sampled_backdrop_instance_count,
        budget->fallback_instance_count,
        budget->execution_stage_count,
        budget->backdrop_execution_stage_count,
        budget->draw_calls,
        budget->total_sample_taps,
        budget->upload_bytes,
        budget->buffer_capacity_bytes,
        budget->backdrop_copy_count,
        budget->backdrop_copy_pixels,
        budget->max_backdrop_pixels,
        budget->planned_surface_sample_pixels,
        json_string(budget->upload_status),
        json_string(budget->draw_status),
        json_string(budget->backdrop_copy_policy),
        json_string(budget->backdrop_copy_skip_reason));
}

} // namespace phenotype_cli::material_budget

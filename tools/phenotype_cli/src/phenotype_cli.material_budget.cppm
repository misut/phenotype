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
    std::int64_t active_execution_stage_count = -1;
    std::int64_t backdrop_execution_stage_count = -1;
    std::int64_t dropped_execution_stage_count = -1;
    std::int64_t draw_calls = -1;
    std::int64_t total_sample_taps = -1;
    std::int64_t max_sample_taps = -1;
    std::int64_t upload_bytes = -1;
    std::int64_t buffer_capacity_bytes = -1;
    double upload_utilization = -1.0;
    std::int64_t backdrop_copy_count = -1;
    std::int64_t backdrop_copy_pixels = -1;
    std::int64_t max_backdrop_pixels = -1;
    double backdrop_copy_utilization = -1.0;
    std::int64_t planned_frame_capture_count = -1;
    std::int64_t planned_frame_capture_pixels = -1;
    std::int64_t planned_surface_sample_pixels = -1;
    std::optional<bool> pipeline_ready;
    std::optional<bool> backdrop_source_ready;
    std::optional<bool> upload_required;
    std::optional<bool> draw_required;
    std::optional<bool> uploaded;
    std::optional<bool> drawn;
    std::optional<bool> backdrop_copy_required;
    std::int64_t backdrop_copy_skipped_count = -1;
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

auto json_number_at(json::Value const& value,
                    std::initializer_list<std::string_view> path)
    -> std::optional<double> {
    auto const* found = json_at(value, path);
    if (!found || !found->is_number())
        return std::nullopt;
    return found->as_float();
}

auto budget_integer_at(json::Value const& report, std::string_view key)
    -> std::optional<std::int64_t> {
    return json_integer_at(report, {
        "artifact_context",
        "material_contract",
        "executor_budget",
        key,
    });
}

auto budget_number_at(json::Value const& report, std::string_view key)
    -> std::optional<double> {
    return json_number_at(report, {
        "artifact_context",
        "material_contract",
        "executor_budget",
        key,
    });
}

auto budget_bool_at(json::Value const& report, std::string_view key)
    -> std::optional<bool> {
    return json_bool_at(report, {
        "artifact_context",
        "material_contract",
        "executor_budget",
        key,
    });
}

auto budget_string_at(json::Value const& report, std::string_view key)
    -> std::optional<std::string> {
    return json_string_at(report, {
        "artifact_context",
        "material_contract",
        "executor_budget",
        key,
    });
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
        .plan_count = budget_integer_at(report, "plan_count").value_or(-1),
        .material_instance_count = budget_integer_at(
            report,
            "material_instance_count").value_or(-1),
        .sampled_backdrop_instance_count = budget_integer_at(
            report,
            "sampled_backdrop_instance_count").value_or(-1),
        .fallback_instance_count = budget_integer_at(
            report,
            "fallback_instance_count").value_or(-1),
        .execution_stage_count = budget_integer_at(
            report,
            "execution_stage_count").value_or(-1),
        .active_execution_stage_count = budget_integer_at(
            report,
            "active_execution_stage_count").value_or(-1),
        .backdrop_execution_stage_count = budget_integer_at(
            report,
            "backdrop_execution_stage_count").value_or(-1),
        .dropped_execution_stage_count = budget_integer_at(
            report,
            "dropped_execution_stage_count").value_or(-1),
        .draw_calls = budget_integer_at(report, "draw_calls").value_or(-1),
        .total_sample_taps = budget_integer_at(
            report,
            "total_sample_taps").value_or(-1),
        .max_sample_taps = budget_integer_at(
            report,
            "max_sample_taps").value_or(-1),
        .upload_bytes = budget_integer_at(report, "upload_bytes").value_or(-1),
        .buffer_capacity_bytes = budget_integer_at(
            report,
            "buffer_capacity_bytes").value_or(-1),
        .upload_utilization = budget_number_at(
            report,
            "upload_utilization").value_or(-1.0),
        .backdrop_copy_count = budget_integer_at(
            report,
            "backdrop_copy_count").value_or(-1),
        .backdrop_copy_pixels = budget_integer_at(
            report,
            "backdrop_copy_pixels").value_or(-1),
        .max_backdrop_pixels = budget_integer_at(
            report,
            "max_backdrop_pixels").value_or(-1),
        .backdrop_copy_utilization = budget_number_at(
            report,
            "backdrop_copy_utilization").value_or(-1.0),
        .planned_frame_capture_count = budget_integer_at(
            report,
            "planned_frame_capture_count").value_or(-1),
        .planned_frame_capture_pixels = budget_integer_at(
            report,
            "planned_frame_capture_pixels").value_or(-1),
        .planned_surface_sample_pixels = budget_integer_at(
            report,
            "planned_surface_sample_pixels").value_or(-1),
        .pipeline_ready = budget_bool_at(report, "pipeline_ready"),
        .backdrop_source_ready = budget_bool_at(
            report,
            "backdrop_source_ready"),
        .upload_required = budget_bool_at(report, "upload_required"),
        .draw_required = budget_bool_at(report, "draw_required"),
        .uploaded = budget_bool_at(report, "uploaded"),
        .drawn = budget_bool_at(report, "drawn"),
        .backdrop_copy_required = budget_bool_at(
            report,
            "backdrop_copy_required"),
        .backdrop_copy_skipped_count = budget_integer_at(
            report,
            "backdrop_copy_skipped_count").value_or(-1),
        .upload_status = budget_string_at(report, "upload_status")
            .value_or("unknown"),
        .draw_status = budget_string_at(report, "draw_status")
            .value_or("unknown"),
        .backdrop_copy_policy = budget_string_at(
            report,
            "backdrop_copy_policy").value_or("unknown"),
        .backdrop_copy_skip_reason = budget_string_at(
            report,
            "backdrop_copy_skip_reason").value_or("unknown"),
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

auto budget_ratio(double value) -> std::string {
    return value >= 0.0 ? std::format("{:.6g}", value) : std::string{"null"};
}

auto budget_utilization_text(double value) -> std::string {
    return value >= 0.0
        ? std::format("{:.1f}%", value * 100.0)
        : std::string{"unknown"};
}

auto budget_ratio_text(std::int64_t value, std::int64_t capacity)
    -> std::string {
    if (value < 0 || capacity < 0)
        return "unknown";
    if (capacity == 0)
        return value == 0 ? std::string{"0.0%"} : std::string{"unknown"};
    return budget_utilization_text(
        static_cast<double>(value) / static_cast<double>(capacity));
}

auto budget_bool(std::optional<bool> value) -> std::string {
    if (!value)
        return "null";
    return *value ? "true" : "false";
}

auto budget_bool_text(std::optional<bool> value) -> std::string {
    if (!value)
        return "unknown";
    return *value ? "true" : "false";
}

auto material_budget_json(std::optional<MaterialBudgetSummary> const& budget)
    -> std::string {
    if (!budget)
        return "null";
    return std::format(
        "{{\"plan_count\":{},\"material_instance_count\":{},"
        "\"sampled_backdrop_instance_count\":{},"
        "\"fallback_instance_count\":{},\"execution_stage_count\":{},"
        "\"active_execution_stage_count\":{},"
        "\"backdrop_execution_stage_count\":{},"
        "\"dropped_execution_stage_count\":{},\"draw_calls\":{},"
        "\"total_sample_taps\":{},\"max_sample_taps\":{},"
        "\"upload_bytes\":{},\"buffer_capacity_bytes\":{},"
        "\"upload_utilization\":{},\"backdrop_copy_count\":{},"
        "\"backdrop_copy_pixels\":{},\"max_backdrop_pixels\":{},"
        "\"backdrop_copy_utilization\":{},"
        "\"planned_frame_capture_count\":{},"
        "\"planned_frame_capture_pixels\":{},"
        "\"planned_surface_sample_pixels\":{},\"pipeline_ready\":{},"
        "\"backdrop_source_ready\":{},\"upload_required\":{},"
        "\"draw_required\":{},\"uploaded\":{},\"drawn\":{},"
        "\"backdrop_copy_required\":{},\"backdrop_copy_skipped_count\":{},"
        "\"upload_status\":{},\"draw_status\":{},"
        "\"backdrop_copy_policy\":{},\"backdrop_copy_skip_reason\":{}}}",
        budget->plan_count,
        budget->material_instance_count,
        budget->sampled_backdrop_instance_count,
        budget->fallback_instance_count,
        budget->execution_stage_count,
        budget->active_execution_stage_count,
        budget->backdrop_execution_stage_count,
        budget->dropped_execution_stage_count,
        budget->draw_calls,
        budget->total_sample_taps,
        budget->max_sample_taps,
        budget->upload_bytes,
        budget->buffer_capacity_bytes,
        budget_ratio(budget->upload_utilization),
        budget->backdrop_copy_count,
        budget->backdrop_copy_pixels,
        budget->max_backdrop_pixels,
        budget_ratio(budget->backdrop_copy_utilization),
        budget->planned_frame_capture_count,
        budget->planned_frame_capture_pixels,
        budget->planned_surface_sample_pixels,
        budget_bool(budget->pipeline_ready),
        budget_bool(budget->backdrop_source_ready),
        budget_bool(budget->upload_required),
        budget_bool(budget->draw_required),
        budget_bool(budget->uploaded),
        budget_bool(budget->drawn),
        budget_bool(budget->backdrop_copy_required),
        budget->backdrop_copy_skipped_count,
        json_string(budget->upload_status),
        json_string(budget->draw_status),
        json_string(budget->backdrop_copy_policy),
        json_string(budget->backdrop_copy_skip_reason));
}

} // namespace phenotype_cli::material_budget

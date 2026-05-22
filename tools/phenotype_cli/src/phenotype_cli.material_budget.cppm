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

struct VerifierManifestSummary {
    std::string name;
    std::int64_t pixel_regions = -1;
    std::int64_t pixel_region_metrics = -1;
    std::int64_t pixel_region_metric_comparisons = -1;
    std::int64_t forbid_pixel_region_colors = -1;
    std::int64_t runtime_numeric_bounds = -1;
    std::int64_t material_executor_budget_bounds = -1;
    std::vector<std::string> material_executor_budget_bound_keys;
    std::vector<std::string> material_executor_budget_fields;
    std::vector<std::string> material_executor_budget_coverage_required_fields;
};

struct MaterialBudgetCoverageSummary {
    std::int64_t guardable_field_count = 0;
    std::int64_t observed_field_count = 0;
    std::int64_t guarded_observed_field_count = 0;
    std::int64_t unguarded_observed_field_count = 0;
    std::int64_t required_field_count = 0;
    std::int64_t covered_required_field_count = 0;
    std::int64_t missing_required_field_count = 0;
    std::int64_t manifest_field_count = 0;
    std::int64_t manifest_bound_key_count = 0;
    std::vector<std::string> observed_fields;
    std::vector<std::string> guarded_observed_fields;
    std::vector<std::string> unguarded_observed_fields;
    std::vector<std::string> required_fields;
    std::vector<std::string> covered_required_fields;
    std::vector<std::string> missing_required_fields;
    std::vector<std::string> manifest_fields;
    std::vector<std::string> manifest_bound_keys;
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

auto verifier_report_from_result(
        cppx::process::CapturedProcessResult const& result)
    -> std::optional<json::Value> {
    if (result.stdout_text.empty())
        return std::nullopt;
    return parse_verifier_json(result.stdout_text);
}

auto verifier_report_from_result(
        std::optional<cppx::process::CapturedProcessResult> const& result)
    -> std::optional<json::Value> {
    if (!result)
        return std::nullopt;
    return verifier_report_from_result(*result);
}

auto json_number_at(json::Value const& value,
                    std::initializer_list<std::string_view> path)
    -> std::optional<double> {
    auto const* found = json_at(value, path);
    if (!found || !found->is_number())
        return std::nullopt;
    return found->as_float();
}

auto json_string_array_at(json::Value const& value,
                          std::initializer_list<std::string_view> path)
    -> std::vector<std::string> {
    auto out = std::vector<std::string>{};
    auto const* array = json_array_at(value, path);
    if (!array)
        return out;
    for (auto const& item : *array) {
        if (item.is_string())
            out.push_back(item.as_string());
    }
    return out;
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
    auto report = verifier_report_from_result(result);
    if (!report)
        return std::nullopt;
    return material_budget_from_report(*report);
}

auto verifier_manifest_summary_from_report(json::Value const& report)
    -> std::optional<VerifierManifestSummary> {
    if (!json_object_at(report, {"manifest"}))
        return std::nullopt;
    return VerifierManifestSummary{
        .name = json_string_at(report, {"manifest", "name"})
            .value_or("unknown"),
        .pixel_regions = json_integer_at(
            report,
            {"manifest", "pixel_regions"}).value_or(-1),
        .pixel_region_metrics = json_integer_at(
            report,
            {"manifest", "pixel_region_metrics"}).value_or(-1),
        .pixel_region_metric_comparisons = json_integer_at(
            report,
            {"manifest", "pixel_region_metric_comparisons"}).value_or(-1),
        .forbid_pixel_region_colors = json_integer_at(
            report,
            {"manifest", "forbid_pixel_region_colors"}).value_or(-1),
        .runtime_numeric_bounds = json_integer_at(
            report,
            {"manifest", "runtime_numeric_bounds"}).value_or(-1),
        .material_executor_budget_bounds = json_integer_at(
            report,
            {"manifest", "material_executor_budget_bounds"}).value_or(-1),
        .material_executor_budget_bound_keys = json_string_array_at(
            report,
            {"manifest", "material_executor_budget_bound_keys"}),
        .material_executor_budget_fields = json_string_array_at(
            report,
            {"manifest", "material_executor_budget_fields"}),
        .material_executor_budget_coverage_required_fields =
            json_string_array_at(
                report,
                {"manifest",
                 "material_executor_budget_coverage_required_fields"}),
    };
}

auto verifier_manifest_summary_from_verifier(
        cppx::process::CapturedProcessResult const& result)
    -> std::optional<VerifierManifestSummary> {
    auto report = verifier_report_from_result(result);
    if (!report)
        return std::nullopt;
    return verifier_manifest_summary_from_report(*report);
}

auto verifier_manifest_summary_from_verifier(
        std::optional<cppx::process::CapturedProcessResult> const& result)
    -> std::optional<VerifierManifestSummary> {
    if (!result)
        return std::nullopt;
    return verifier_manifest_summary_from_verifier(*result);
}

auto material_budget_from_verifier(
        std::optional<cppx::process::CapturedProcessResult> const& result)
    -> std::optional<MaterialBudgetSummary> {
    if (!result)
        return std::nullopt;
    return material_budget_from_verifier(*result);
}

auto sorted_unique(std::vector<std::string> values) -> std::vector<std::string> {
    std::ranges::sort(values);
    auto last = std::ranges::unique(values);
    values.erase(last.begin(), last.end());
    return values;
}

auto contains_field(std::span<std::string const> fields,
                    std::string_view field) -> bool {
    return std::ranges::find(fields, field) != fields.end();
}

auto material_budget_guardable_fields() -> std::vector<std::string> {
    return {
        "active_execution_stage_count",
        "backdrop_copy_count",
        "backdrop_copy_pixels",
        "backdrop_copy_skipped_count",
        "backdrop_copy_utilization",
        "backdrop_execution_stage_count",
        "buffer_capacity_bytes",
        "draw_calls",
        "dropped_execution_stage_count",
        "execution_stage_count",
        "fallback_instance_count",
        "material_instance_count",
        "max_backdrop_pixels",
        "max_sample_taps",
        "plan_count",
        "planned_frame_capture_count",
        "planned_frame_capture_pixels",
        "planned_surface_sample_pixels",
        "sampled_backdrop_instance_count",
        "total_sample_taps",
        "upload_bytes",
        "upload_utilization",
    };
}

void append_if_known(std::vector<std::string>& fields,
                     std::string field,
                     std::int64_t value) {
    if (value >= 0)
        fields.push_back(std::move(field));
}

void append_if_known(std::vector<std::string>& fields,
                     std::string field,
                     double value) {
    if (value >= 0.0)
        fields.push_back(std::move(field));
}

auto observed_material_budget_fields(MaterialBudgetSummary const& budget)
    -> std::vector<std::string> {
    auto fields = std::vector<std::string>{};
    append_if_known(fields, "plan_count", budget.plan_count);
    append_if_known(
        fields,
        "material_instance_count",
        budget.material_instance_count);
    append_if_known(
        fields,
        "sampled_backdrop_instance_count",
        budget.sampled_backdrop_instance_count);
    append_if_known(
        fields,
        "fallback_instance_count",
        budget.fallback_instance_count);
    append_if_known(
        fields,
        "execution_stage_count",
        budget.execution_stage_count);
    append_if_known(
        fields,
        "active_execution_stage_count",
        budget.active_execution_stage_count);
    append_if_known(
        fields,
        "backdrop_execution_stage_count",
        budget.backdrop_execution_stage_count);
    append_if_known(
        fields,
        "dropped_execution_stage_count",
        budget.dropped_execution_stage_count);
    append_if_known(fields, "draw_calls", budget.draw_calls);
    append_if_known(fields, "total_sample_taps", budget.total_sample_taps);
    append_if_known(fields, "max_sample_taps", budget.max_sample_taps);
    append_if_known(fields, "upload_bytes", budget.upload_bytes);
    append_if_known(
        fields,
        "buffer_capacity_bytes",
        budget.buffer_capacity_bytes);
    append_if_known(fields, "upload_utilization", budget.upload_utilization);
    append_if_known(fields, "backdrop_copy_count", budget.backdrop_copy_count);
    append_if_known(fields, "backdrop_copy_pixels", budget.backdrop_copy_pixels);
    append_if_known(fields, "max_backdrop_pixels", budget.max_backdrop_pixels);
    append_if_known(
        fields,
        "backdrop_copy_utilization",
        budget.backdrop_copy_utilization);
    append_if_known(
        fields,
        "planned_frame_capture_count",
        budget.planned_frame_capture_count);
    append_if_known(
        fields,
        "planned_frame_capture_pixels",
        budget.planned_frame_capture_pixels);
    append_if_known(
        fields,
        "planned_surface_sample_pixels",
        budget.planned_surface_sample_pixels);
    append_if_known(
        fields,
        "backdrop_copy_skipped_count",
        budget.backdrop_copy_skipped_count);
    return sorted_unique(std::move(fields));
}

auto material_budget_coverage_summary(
        std::optional<MaterialBudgetSummary> const& budget,
        std::optional<VerifierManifestSummary> const& manifest)
    -> std::optional<MaterialBudgetCoverageSummary> {
    if (!budget || !manifest)
        return std::nullopt;

    auto observed = observed_material_budget_fields(*budget);
    auto manifest_fields = sorted_unique(manifest->material_executor_budget_fields);
    auto manifest_bound_keys = sorted_unique(
        manifest->material_executor_budget_bound_keys);
    auto required_fields = sorted_unique(
        manifest->material_executor_budget_coverage_required_fields);
    auto guarded = std::vector<std::string>{};
    auto unguarded = std::vector<std::string>{};
    for (auto const& field : observed) {
        if (contains_field(manifest_fields, field)) {
            guarded.push_back(field);
        } else {
            unguarded.push_back(field);
        }
    }
    auto covered_required = std::vector<std::string>{};
    auto missing_required = std::vector<std::string>{};
    for (auto const& field : required_fields) {
        if (contains_field(guarded, field)) {
            covered_required.push_back(field);
        } else {
            missing_required.push_back(field);
        }
    }

    auto guardable = material_budget_guardable_fields();
    auto bound_key_count = manifest_bound_keys.empty()
        ? manifest->material_executor_budget_bounds
        : static_cast<std::int64_t>(manifest_bound_keys.size());
    return MaterialBudgetCoverageSummary{
        .guardable_field_count = static_cast<std::int64_t>(guardable.size()),
        .observed_field_count = static_cast<std::int64_t>(observed.size()),
        .guarded_observed_field_count =
            static_cast<std::int64_t>(guarded.size()),
        .unguarded_observed_field_count =
            static_cast<std::int64_t>(unguarded.size()),
        .required_field_count =
            static_cast<std::int64_t>(required_fields.size()),
        .covered_required_field_count =
            static_cast<std::int64_t>(covered_required.size()),
        .missing_required_field_count =
            static_cast<std::int64_t>(missing_required.size()),
        .manifest_field_count = static_cast<std::int64_t>(manifest_fields.size()),
        .manifest_bound_key_count = bound_key_count,
        .observed_fields = std::move(observed),
        .guarded_observed_fields = std::move(guarded),
        .unguarded_observed_fields = std::move(unguarded),
        .required_fields = std::move(required_fields),
        .covered_required_fields = std::move(covered_required),
        .missing_required_fields = std::move(missing_required),
        .manifest_fields = std::move(manifest_fields),
        .manifest_bound_keys = std::move(manifest_bound_keys),
    };
}

auto budget_count(std::int64_t value) -> std::string {
    return value >= 0 ? std::format("{}", value) : std::string{"unknown"};
}

auto manifest_count_json(std::int64_t value) -> std::string {
    return value >= 0 ? std::format("{}", value) : std::string{"null"};
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

auto material_budget_coverage_json(
        std::optional<MaterialBudgetCoverageSummary> const& coverage)
    -> std::string {
    if (!coverage)
        return "null";
    return std::format(
        "{{\"guardable_field_count\":{},\"observed_field_count\":{},"
        "\"guarded_observed_field_count\":{},"
        "\"unguarded_observed_field_count\":{},"
        "\"required_field_count\":{},\"covered_required_field_count\":{},"
        "\"missing_required_field_count\":{},"
        "\"manifest_field_count\":{},\"manifest_bound_key_count\":{},"
        "\"observed_fields\":{},\"guarded_observed_fields\":{},"
        "\"unguarded_observed_fields\":{},\"required_fields\":{},"
        "\"covered_required_fields\":{},\"missing_required_fields\":{},"
        "\"manifest_fields\":{},\"manifest_bound_keys\":{}}}",
        coverage->guardable_field_count,
        coverage->observed_field_count,
        coverage->guarded_observed_field_count,
        coverage->unguarded_observed_field_count,
        coverage->required_field_count,
        coverage->covered_required_field_count,
        coverage->missing_required_field_count,
        coverage->manifest_field_count,
        coverage->manifest_bound_key_count,
        string_array_json(coverage->observed_fields),
        string_array_json(coverage->guarded_observed_fields),
        string_array_json(coverage->unguarded_observed_fields),
        string_array_json(coverage->required_fields),
        string_array_json(coverage->covered_required_fields),
        string_array_json(coverage->missing_required_fields),
        string_array_json(coverage->manifest_fields),
        string_array_json(coverage->manifest_bound_keys));
}

auto budget_field_list_text(std::vector<std::string> const& fields,
                            std::size_t limit = 8) -> std::string {
    if (fields.empty())
        return "none";
    auto text = std::string{};
    auto count = std::min(limit, fields.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0)
            text += ", ";
        text += fields[i];
    }
    if (fields.size() > limit)
        text += std::format(", +{} more", fields.size() - limit);
    return text;
}

auto material_budget_coverage_text(MaterialBudgetCoverageSummary const& coverage)
    -> std::string {
    auto required = coverage.required_field_count > 0
        ? std::format(
            " required={}/{}",
            coverage.covered_required_field_count,
            coverage.required_field_count)
        : std::string{};
    auto missing = coverage.missing_required_fields.empty()
        ? std::string{}
        : std::format(
            " missing-required=({})",
            budget_field_list_text(coverage.missing_required_fields));
    return std::format(
        "guarded={}/{} observed={} guard-keys={} unguarded={} ({})",
        coverage.guarded_observed_field_count,
        coverage.guardable_field_count,
        coverage.observed_field_count,
        coverage.manifest_bound_key_count,
        coverage.unguarded_observed_field_count,
        budget_field_list_text(coverage.unguarded_observed_fields))
        + required
        + missing;
}

auto verifier_manifest_summary_json(
        std::optional<VerifierManifestSummary> const& manifest)
    -> std::string {
    if (!manifest)
        return "null";
    return std::format(
        "{{\"name\":{},\"pixel_regions\":{},"
        "\"pixel_region_metrics\":{},"
        "\"pixel_region_metric_comparisons\":{},"
        "\"forbid_pixel_region_colors\":{},"
        "\"runtime_numeric_bounds\":{},"
        "\"material_executor_budget_bounds\":{},"
        "\"material_executor_budget_bound_keys\":{},"
        "\"material_executor_budget_fields\":{},"
        "\"material_executor_budget_coverage_required_fields\":{}}}",
        json_string(manifest->name),
        manifest_count_json(manifest->pixel_regions),
        manifest_count_json(manifest->pixel_region_metrics),
        manifest_count_json(manifest->pixel_region_metric_comparisons),
        manifest_count_json(manifest->forbid_pixel_region_colors),
        manifest_count_json(manifest->runtime_numeric_bounds),
        manifest_count_json(manifest->material_executor_budget_bounds),
        string_array_json(manifest->material_executor_budget_bound_keys),
        string_array_json(manifest->material_executor_budget_fields),
        string_array_json(
            manifest->material_executor_budget_coverage_required_fields));
}

} // namespace phenotype_cli::material_budget

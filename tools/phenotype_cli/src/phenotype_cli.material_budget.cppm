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

struct MaterialQualityPolicySummary {
    std::int64_t backdrop_sampling_disabled = -1;
    std::int64_t noise_disabled = -1;
    std::int64_t shadow_disabled = -1;
    double max_blur_radius = -1.0;
    std::int64_t max_sample_taps = -1;
    std::int64_t max_backdrop_pixels = -1;
};

struct MaterialResourceBoundsSummary {
    double max_plan_blur_radius = -1.0;
    std::int64_t max_plan_sample_taps = -1;
    std::int64_t total_plan_sample_taps = -1;
    double max_budget_blur_radius = -1.0;
    std::int64_t max_sample_taps = -1;
    std::int64_t max_sampling_kernel_radius = -1;
    std::int64_t max_pass_count = -1;
    std::int64_t max_execution_stages = -1;
    std::int64_t max_paint_layers = -1;
    std::int64_t max_backdrop_pixels = -1;
    std::int64_t max_frame_capture_count = -1;
    std::int64_t max_frame_capture_pixels = -1;
    std::int64_t total_surface_sample_pixels = -1;
    std::int64_t max_surface_sample_pixels = -1;
    double max_refraction_offset_pixels = -1.0;
    double max_container_spacing = -1.0;
    double max_paint_layer_inflate = -1.0;
    std::int64_t total_runtime_passes = -1;
    std::int64_t active_runtime_passes = -1;
    std::int64_t backdrop_runtime_passes = -1;
    std::int64_t total_execution_stages = -1;
    std::int64_t active_execution_stages = -1;
    std::int64_t backdrop_execution_stages = -1;
    std::int64_t dropped_execution_stages = -1;
    std::int64_t max_execution_stage_count = -1;
    std::int64_t max_execution_stage_capacity = -1;
    std::int64_t total_paint_layers = -1;
    std::int64_t active_paint_layers = -1;
    std::int64_t dropped_paint_layers = -1;
    std::int64_t max_paint_layer_count = -1;
    std::int64_t max_paint_layer_capacity = -1;
    std::int64_t max_pass_texture_copy_pixels = -1;
    std::int64_t total_pass_texture_copy_pixels = -1;
    std::int64_t unbounded_texture_copy = -1;
    std::int64_t non_deterministic_fallback = -1;
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
    std::int64_t material_resource_bounds = -1;
    std::vector<std::string> material_resource_bound_keys;
    std::vector<std::string> material_resource_bound_fields;
    std::vector<std::string> material_executor_budget_coverage_required_fields;
    std::int64_t material_quality_policy_bounds = -1;
    std::vector<std::string> material_quality_policy_bound_keys;
    std::vector<std::string> material_quality_policy_fields;
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

struct MaterialBudgetBoundSummary {
    std::int64_t bound_count = -1;
    std::int64_t pass_count = -1;
    std::int64_t fail_count = -1;
    std::string tightest_bound_key;
    std::string tightest_bound_field;
    std::optional<double> tightest_bound_margin;
    std::vector<std::string> failed_keys;
};

struct MaterialBudgetBoundResult {
    std::string key;
    std::string field;
    std::string bound;
    std::optional<double> expected;
    std::optional<double> actual;
    std::optional<bool> ok;
    std::optional<double> margin;
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

auto json_object_number(json::Object const& object, std::string_view key)
    -> std::optional<double> {
    auto const* found = json_object_member(object, key);
    if (!found || !found->is_number())
        return std::nullopt;
    return found->as_float();
}

auto json_object_integer(json::Object const& object, std::string_view key)
    -> std::optional<std::int64_t> {
    auto const* found = json_object_member(object, key);
    if (!found || !found->is_number())
        return std::nullopt;
    return found->as_integer();
}

auto json_object_bool(json::Object const& object, std::string_view key)
    -> std::optional<bool> {
    auto const* found = json_object_member(object, key);
    if (!found || !found->is_bool())
        return std::nullopt;
    return found->as_bool();
}

auto json_object_string(json::Object const& object, std::string_view key)
    -> std::string {
    auto const* found = json_object_member(object, key);
    return found && found->is_string() ? found->as_string() : std::string{};
}

auto json_object_string_array(json::Object const& object, std::string_view key)
    -> std::vector<std::string> {
    auto out = std::vector<std::string>{};
    auto const* found = json_object_member(object, key);
    if (!found || !found->is_array())
        return out;
    for (auto const& item : found->as_array()) {
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
        .material_resource_bounds = json_integer_at(
            report,
            {"manifest", "material_resource_bounds"}).value_or(-1),
        .material_resource_bound_keys = json_string_array_at(
            report,
            {"manifest", "material_resource_bound_keys"}),
        .material_resource_bound_fields = json_string_array_at(
            report,
            {"manifest", "material_resource_bound_fields"}),
        .material_executor_budget_coverage_required_fields =
            json_string_array_at(
                report,
                {"manifest",
                 "material_executor_budget_coverage_required_fields"}),
        .material_quality_policy_bounds = json_integer_at(
            report,
            {"manifest", "material_quality_policy_bounds"}).value_or(-1),
        .material_quality_policy_bound_keys = json_string_array_at(
            report,
            {"manifest", "material_quality_policy_bound_keys"}),
        .material_quality_policy_fields = json_string_array_at(
            report,
            {"manifest", "material_quality_policy_fields"}),
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

auto material_quality_policy_from_report(json::Value const& report)
    -> std::optional<MaterialQualityPolicySummary> {
    if (!json_object_at(report, {"material_plans", "quality_policy"}))
        return std::nullopt;
    return MaterialQualityPolicySummary{
        .backdrop_sampling_disabled = json_integer_at(
            report,
            {"material_plans", "quality_policy",
             "backdrop_sampling_disabled"}).value_or(-1),
        .noise_disabled = json_integer_at(
            report,
            {"material_plans", "quality_policy", "noise_disabled"})
            .value_or(-1),
        .shadow_disabled = json_integer_at(
            report,
            {"material_plans", "quality_policy", "shadow_disabled"})
            .value_or(-1),
        .max_blur_radius = json_number_at(
            report,
            {"material_plans", "quality_policy", "max_blur_radius"})
            .value_or(-1.0),
        .max_sample_taps = json_integer_at(
            report,
            {"material_plans", "quality_policy", "max_sample_taps"})
            .value_or(-1),
        .max_backdrop_pixels = json_integer_at(
            report,
            {"material_plans", "quality_policy", "max_backdrop_pixels"})
            .value_or(-1),
    };
}

auto material_quality_policy_from_verifier(
        cppx::process::CapturedProcessResult const& result)
    -> std::optional<MaterialQualityPolicySummary> {
    auto report = verifier_report_from_result(result);
    if (!report)
        return std::nullopt;
    return material_quality_policy_from_report(*report);
}

auto material_quality_policy_from_verifier(
        std::optional<cppx::process::CapturedProcessResult> const& result)
    -> std::optional<MaterialQualityPolicySummary> {
    if (!result)
        return std::nullopt;
    return material_quality_policy_from_verifier(*result);
}

auto resource_bounds_integer_at(json::Value const& report,
                                std::string_view key)
    -> std::optional<std::int64_t> {
    return json_integer_at(report, {"material_plans", "resource_bounds", key});
}

auto resource_bounds_number_at(json::Value const& report,
                               std::string_view key) -> std::optional<double> {
    return json_number_at(report, {"material_plans", "resource_bounds", key});
}

auto material_resource_bounds_from_report(json::Value const& report)
    -> std::optional<MaterialResourceBoundsSummary> {
    if (!json_object_at(report, {"material_plans", "resource_bounds"}))
        return std::nullopt;
    return MaterialResourceBoundsSummary{
        .max_plan_blur_radius =
            resource_bounds_number_at(report, "max_plan_blur_radius")
                .value_or(-1.0),
        .max_plan_sample_taps =
            resource_bounds_integer_at(report, "max_plan_sample_taps")
                .value_or(-1),
        .total_plan_sample_taps =
            resource_bounds_integer_at(report, "total_plan_sample_taps")
                .value_or(-1),
        .max_budget_blur_radius =
            resource_bounds_number_at(report, "max_budget_blur_radius")
                .value_or(-1.0),
        .max_sample_taps =
            resource_bounds_integer_at(report, "max_sample_taps").value_or(-1),
        .max_sampling_kernel_radius =
            resource_bounds_integer_at(report, "max_sampling_kernel_radius")
                .value_or(-1),
        .max_pass_count =
            resource_bounds_integer_at(report, "max_pass_count").value_or(-1),
        .max_execution_stages =
            resource_bounds_integer_at(report, "max_execution_stages")
                .value_or(-1),
        .max_paint_layers =
            resource_bounds_integer_at(report, "max_paint_layers").value_or(-1),
        .max_backdrop_pixels =
            resource_bounds_integer_at(report, "max_backdrop_pixels")
                .value_or(-1),
        .max_frame_capture_count =
            resource_bounds_integer_at(report, "max_frame_capture_count")
                .value_or(-1),
        .max_frame_capture_pixels =
            resource_bounds_integer_at(report, "max_frame_capture_pixels")
                .value_or(-1),
        .total_surface_sample_pixels =
            resource_bounds_integer_at(report, "total_surface_sample_pixels")
                .value_or(-1),
        .max_surface_sample_pixels =
            resource_bounds_integer_at(report, "max_surface_sample_pixels")
                .value_or(-1),
        .max_refraction_offset_pixels =
            resource_bounds_number_at(report, "max_refraction_offset_pixels")
                .value_or(-1.0),
        .max_container_spacing =
            resource_bounds_number_at(report, "max_container_spacing")
                .value_or(-1.0),
        .max_paint_layer_inflate =
            resource_bounds_number_at(report, "max_paint_layer_inflate")
                .value_or(-1.0),
        .total_runtime_passes =
            resource_bounds_integer_at(report, "total_runtime_passes")
                .value_or(-1),
        .active_runtime_passes =
            resource_bounds_integer_at(report, "active_runtime_passes")
                .value_or(-1),
        .backdrop_runtime_passes =
            resource_bounds_integer_at(report, "backdrop_runtime_passes")
                .value_or(-1),
        .total_execution_stages =
            resource_bounds_integer_at(report, "total_execution_stages")
                .value_or(-1),
        .active_execution_stages =
            resource_bounds_integer_at(report, "active_execution_stages")
                .value_or(-1),
        .backdrop_execution_stages =
            resource_bounds_integer_at(report, "backdrop_execution_stages")
                .value_or(-1),
        .dropped_execution_stages =
            resource_bounds_integer_at(report, "dropped_execution_stages")
                .value_or(-1),
        .max_execution_stage_count =
            resource_bounds_integer_at(report, "max_execution_stage_count")
                .value_or(-1),
        .max_execution_stage_capacity =
            resource_bounds_integer_at(report, "max_execution_stage_capacity")
                .value_or(-1),
        .total_paint_layers =
            resource_bounds_integer_at(report, "total_paint_layers")
                .value_or(-1),
        .active_paint_layers =
            resource_bounds_integer_at(report, "active_paint_layers")
                .value_or(-1),
        .dropped_paint_layers =
            resource_bounds_integer_at(report, "dropped_paint_layers")
                .value_or(-1),
        .max_paint_layer_count =
            resource_bounds_integer_at(report, "max_paint_layer_count")
                .value_or(-1),
        .max_paint_layer_capacity =
            resource_bounds_integer_at(report, "max_paint_layer_capacity")
                .value_or(-1),
        .max_pass_texture_copy_pixels =
            resource_bounds_integer_at(report, "max_pass_texture_copy_pixels")
                .value_or(-1),
        .total_pass_texture_copy_pixels =
            resource_bounds_integer_at(report, "total_pass_texture_copy_pixels")
                .value_or(-1),
        .unbounded_texture_copy =
            resource_bounds_integer_at(report, "unbounded_texture_copy")
                .value_or(-1),
        .non_deterministic_fallback =
            resource_bounds_integer_at(report, "non_deterministic_fallback")
                .value_or(-1),
    };
}

auto material_resource_bounds_from_verifier(
        cppx::process::CapturedProcessResult const& result)
    -> std::optional<MaterialResourceBoundsSummary> {
    auto report = verifier_report_from_result(result);
    if (!report)
        return std::nullopt;
    return material_resource_bounds_from_report(*report);
}

auto material_resource_bounds_from_verifier(
        std::optional<cppx::process::CapturedProcessResult> const& result)
    -> std::optional<MaterialResourceBoundsSummary> {
    if (!result)
        return std::nullopt;
    return material_resource_bounds_from_verifier(*result);
}

auto material_bound_summary_from_object(json::Object const& object)
    -> MaterialBudgetBoundSummary {
    return MaterialBudgetBoundSummary{
        .bound_count = json_object_integer(object, "bound_count").value_or(-1),
        .pass_count = json_object_integer(object, "pass_count").value_or(-1),
        .fail_count = json_object_integer(object, "fail_count").value_or(-1),
        .tightest_bound_key = json_object_string(object, "tightest_bound_key"),
        .tightest_bound_field =
            json_object_string(object, "tightest_bound_field"),
        .tightest_bound_margin =
            json_object_number(object, "tightest_bound_margin"),
        .failed_keys = json_object_string_array(object, "failed_keys"),
    };
}

auto material_bound_results_from_array(json::Array const& results)
    -> std::vector<MaterialBudgetBoundResult> {
    auto out = std::vector<MaterialBudgetBoundResult>{};
    out.reserve(results.size());
    for (auto const& item : results) {
        if (!item.is_object())
            continue;
        auto const& object = item.as_object();
        out.push_back(MaterialBudgetBoundResult{
            .key = json_object_string(object, "key"),
            .field = json_object_string(object, "field"),
            .bound = json_object_string(object, "bound"),
            .expected = json_object_number(object, "expected"),
            .actual = json_object_number(object, "actual"),
            .ok = json_object_bool(object, "ok"),
            .margin = json_object_number(object, "margin"),
        });
    }
    return out;
}

auto material_budget_bound_summary_from_report(json::Value const& report)
    -> std::optional<MaterialBudgetBoundSummary> {
    auto const* summary = json_object_at(report, {
        "artifact_context",
        "material_contract",
        "executor_budget_bound_summary",
    });
    if (!summary)
        return std::nullopt;
    return material_bound_summary_from_object(*summary);
}

auto material_budget_bound_results_from_report(json::Value const& report)
    -> std::optional<std::vector<MaterialBudgetBoundResult>> {
    auto const* results = json_array_at(report, {
        "artifact_context",
        "material_contract",
        "executor_budget_bound_results",
    });
    if (!results)
        return std::nullopt;
    return material_bound_results_from_array(*results);
}

auto material_resource_bound_summary_from_report(json::Value const& report)
    -> std::optional<MaterialBudgetBoundSummary> {
    auto const* summary = json_object_at(
        report,
        {"material_plans", "resource_bound_summary"});
    if (!summary)
        return std::nullopt;
    return material_bound_summary_from_object(*summary);
}

auto material_resource_bound_results_from_report(json::Value const& report)
    -> std::optional<std::vector<MaterialBudgetBoundResult>> {
    auto const* results = json_array_at(
        report,
        {"material_plans", "resource_bound_results"});
    if (!results)
        return std::nullopt;
    return material_bound_results_from_array(*results);
}

auto material_quality_policy_bound_summary_from_report(json::Value const& report)
    -> std::optional<MaterialBudgetBoundSummary> {
    auto const* summary = json_object_at(
        report,
        {"material_plans", "quality_policy_bound_summary"});
    if (!summary)
        return std::nullopt;
    return material_bound_summary_from_object(*summary);
}

auto material_quality_policy_bound_results_from_report(json::Value const& report)
    -> std::optional<std::vector<MaterialBudgetBoundResult>> {
    auto const* results = json_array_at(
        report,
        {"material_plans", "quality_policy_bound_results"});
    if (!results)
        return std::nullopt;
    return material_bound_results_from_array(*results);
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

auto budget_number_text(double value) -> std::string {
    return value >= 0.0 ? std::format("{:.6g}", value) : std::string{"unknown"};
}

auto budget_optional_number(std::optional<double> value) -> std::string {
    return value ? std::format("{:.6g}", *value) : std::string{"null"};
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

auto material_quality_policy_json(
        std::optional<MaterialQualityPolicySummary> const& policy)
    -> std::string {
    if (!policy)
        return "null";
    return std::format(
        "{{\"backdrop_sampling_disabled\":{},\"noise_disabled\":{},"
        "\"shadow_disabled\":{},\"max_blur_radius\":{},"
        "\"max_sample_taps\":{},\"max_backdrop_pixels\":{}}}",
        manifest_count_json(policy->backdrop_sampling_disabled),
        manifest_count_json(policy->noise_disabled),
        manifest_count_json(policy->shadow_disabled),
        budget_ratio(policy->max_blur_radius),
        manifest_count_json(policy->max_sample_taps),
        manifest_count_json(policy->max_backdrop_pixels));
}

auto material_resource_bounds_json(
        std::optional<MaterialResourceBoundsSummary> const& bounds)
    -> std::string {
    if (!bounds)
        return "null";
    return std::format(
        "{{\"max_plan_blur_radius\":{},\"max_plan_sample_taps\":{},"
        "\"total_plan_sample_taps\":{},\"max_budget_blur_radius\":{},"
        "\"max_sample_taps\":{},\"max_sampling_kernel_radius\":{},"
        "\"max_pass_count\":{},\"max_execution_stages\":{},"
        "\"max_paint_layers\":{},\"max_backdrop_pixels\":{},"
        "\"max_frame_capture_count\":{},\"max_frame_capture_pixels\":{},"
        "\"total_surface_sample_pixels\":{},"
        "\"max_surface_sample_pixels\":{},"
        "\"max_refraction_offset_pixels\":{},"
        "\"max_container_spacing\":{},\"max_paint_layer_inflate\":{},"
        "\"total_runtime_passes\":{},\"active_runtime_passes\":{},"
        "\"backdrop_runtime_passes\":{},\"total_execution_stages\":{},"
        "\"active_execution_stages\":{},\"backdrop_execution_stages\":{},"
        "\"dropped_execution_stages\":{},"
        "\"max_execution_stage_count\":{},"
        "\"max_execution_stage_capacity\":{},"
        "\"total_paint_layers\":{},\"active_paint_layers\":{},"
        "\"dropped_paint_layers\":{},\"max_paint_layer_count\":{},"
        "\"max_paint_layer_capacity\":{},"
        "\"max_pass_texture_copy_pixels\":{},"
        "\"total_pass_texture_copy_pixels\":{},"
        "\"unbounded_texture_copy\":{},"
        "\"non_deterministic_fallback\":{}}}",
        budget_ratio(bounds->max_plan_blur_radius),
        manifest_count_json(bounds->max_plan_sample_taps),
        manifest_count_json(bounds->total_plan_sample_taps),
        budget_ratio(bounds->max_budget_blur_radius),
        manifest_count_json(bounds->max_sample_taps),
        manifest_count_json(bounds->max_sampling_kernel_radius),
        manifest_count_json(bounds->max_pass_count),
        manifest_count_json(bounds->max_execution_stages),
        manifest_count_json(bounds->max_paint_layers),
        manifest_count_json(bounds->max_backdrop_pixels),
        manifest_count_json(bounds->max_frame_capture_count),
        manifest_count_json(bounds->max_frame_capture_pixels),
        manifest_count_json(bounds->total_surface_sample_pixels),
        manifest_count_json(bounds->max_surface_sample_pixels),
        budget_ratio(bounds->max_refraction_offset_pixels),
        budget_ratio(bounds->max_container_spacing),
        budget_ratio(bounds->max_paint_layer_inflate),
        manifest_count_json(bounds->total_runtime_passes),
        manifest_count_json(bounds->active_runtime_passes),
        manifest_count_json(bounds->backdrop_runtime_passes),
        manifest_count_json(bounds->total_execution_stages),
        manifest_count_json(bounds->active_execution_stages),
        manifest_count_json(bounds->backdrop_execution_stages),
        manifest_count_json(bounds->dropped_execution_stages),
        manifest_count_json(bounds->max_execution_stage_count),
        manifest_count_json(bounds->max_execution_stage_capacity),
        manifest_count_json(bounds->total_paint_layers),
        manifest_count_json(bounds->active_paint_layers),
        manifest_count_json(bounds->dropped_paint_layers),
        manifest_count_json(bounds->max_paint_layer_count),
        manifest_count_json(bounds->max_paint_layer_capacity),
        manifest_count_json(bounds->max_pass_texture_copy_pixels),
        manifest_count_json(bounds->total_pass_texture_copy_pixels),
        manifest_count_json(bounds->unbounded_texture_copy),
        manifest_count_json(bounds->non_deterministic_fallback));
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

auto material_budget_bound_summary_json(
        std::optional<MaterialBudgetBoundSummary> const& summary)
    -> std::string {
    if (!summary)
        return "null";
    return std::format(
        "{{\"bound_count\":{},\"pass_count\":{},\"fail_count\":{},"
        "\"tightest_bound_key\":{},\"tightest_bound_field\":{},"
        "\"tightest_bound_margin\":{},\"failed_keys\":{}}}",
        manifest_count_json(summary->bound_count),
        manifest_count_json(summary->pass_count),
        manifest_count_json(summary->fail_count),
        json_string(summary->tightest_bound_key),
        json_string(summary->tightest_bound_field),
        budget_optional_number(summary->tightest_bound_margin),
        string_array_json(summary->failed_keys));
}

auto material_budget_bound_result_json(MaterialBudgetBoundResult const& result)
    -> std::string {
    return std::format(
        "{{\"key\":{},\"field\":{},\"bound\":{},\"expected\":{},"
        "\"actual\":{},\"ok\":{},\"margin\":{}}}",
        json_string(result.key),
        json_string(result.field),
        json_string(result.bound),
        budget_optional_number(result.expected),
        budget_optional_number(result.actual),
        budget_bool(result.ok),
        budget_optional_number(result.margin));
}

auto material_budget_bound_results_json(
        std::optional<std::vector<MaterialBudgetBoundResult>> const& results)
    -> std::string {
    if (!results)
        return "null";
    auto out = std::string{"["};
    for (std::size_t i = 0; i < results->size(); ++i) {
        if (i > 0)
            out += ",";
        out += material_budget_bound_result_json((*results)[i]);
    }
    out += "]";
    return out;
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

auto material_quality_policy_text(MaterialQualityPolicySummary const& policy)
    -> std::string {
    return std::format(
        "disabled: backdrop-sampling={} noise={} shadow={} "
        "limits: blur={} sample-taps={} backdrop-pixels={}",
        budget_count(policy.backdrop_sampling_disabled),
        budget_count(policy.noise_disabled),
        budget_count(policy.shadow_disabled),
        budget_number_text(policy.max_blur_radius),
        budget_count(policy.max_sample_taps),
        budget_count(policy.max_backdrop_pixels));
}

auto material_resource_bounds_lines(MaterialResourceBoundsSummary const& bounds)
    -> std::vector<std::string> {
    return {
        std::format(
            "plan: blur={} taps={}/{} budget-blur={} budget-taps={} "
            "kernel={} pass-cap={}",
            budget_number_text(bounds.max_plan_blur_radius),
            budget_count(bounds.max_plan_sample_taps),
            budget_count(bounds.total_plan_sample_taps),
            budget_number_text(bounds.max_budget_blur_radius),
            budget_count(bounds.max_sample_taps),
            budget_count(bounds.max_sampling_kernel_radius),
            budget_count(bounds.max_pass_count)),
        std::format(
            "capture: backdrop-pixels={} frame-capture={}/{} px "
            "surface-sample={}/{} px copy={}/{} px",
            budget_count(bounds.max_backdrop_pixels),
            budget_count(bounds.max_frame_capture_count),
            budget_count(bounds.max_frame_capture_pixels),
            budget_count(bounds.max_surface_sample_pixels),
            budget_count(bounds.total_surface_sample_pixels),
            budget_count(bounds.max_pass_texture_copy_pixels),
            budget_count(bounds.total_pass_texture_copy_pixels)),
        std::format(
            "execution: runtime-passes={}/{}/{} stages={}/{}/{} "
            "dropped={} max-stage={}/{}",
            budget_count(bounds.total_runtime_passes),
            budget_count(bounds.active_runtime_passes),
            budget_count(bounds.backdrop_runtime_passes),
            budget_count(bounds.total_execution_stages),
            budget_count(bounds.active_execution_stages),
            budget_count(bounds.backdrop_execution_stages),
            budget_count(bounds.dropped_execution_stages),
            budget_count(bounds.max_execution_stage_count),
            budget_count(bounds.max_execution_stage_capacity)),
        std::format(
            "paint/safety: layers={}/{} dropped={} max-layer={}/{} "
            "refraction={} spacing={} inflate={} unbounded-copy={} "
            "non-deterministic-fallback={}",
            budget_count(bounds.total_paint_layers),
            budget_count(bounds.active_paint_layers),
            budget_count(bounds.dropped_paint_layers),
            budget_count(bounds.max_paint_layer_count),
            budget_count(bounds.max_paint_layer_capacity),
            budget_number_text(bounds.max_refraction_offset_pixels),
            budget_number_text(bounds.max_container_spacing),
            budget_number_text(bounds.max_paint_layer_inflate),
            budget_count(bounds.unbounded_texture_copy),
            budget_count(bounds.non_deterministic_fallback)),
    };
}

auto material_budget_bound_summary_text(MaterialBudgetBoundSummary const& summary)
    -> std::string {
    auto tightest = !summary.tightest_bound_key.empty()
        ? std::format(
            " tightest={} margin={}",
            summary.tightest_bound_key,
            budget_optional_number(summary.tightest_bound_margin))
        : std::string{};
    auto failed = summary.failed_keys.empty()
        ? std::string{}
        : std::format(" failed=({})", budget_field_list_text(summary.failed_keys));
    return std::format(
        "passed={}/{} failed={}",
        budget_count(summary.pass_count),
        budget_count(summary.bound_count),
        budget_count(summary.fail_count))
        + tightest
        + failed;
}

auto material_budget_bound_expected_text(MaterialBudgetBoundResult const& result)
    -> std::string {
    auto op = std::string{"=="};
    if (result.bound == "lte") {
        op = "<=";
    } else if (result.bound == "gte") {
        op = ">=";
    }
    return op + budget_optional_number(result.expected);
}

auto material_budget_bound_result_text(MaterialBudgetBoundResult const& result)
    -> std::string {
    auto status = result.ok ? (*result.ok ? "pass" : "fail") : "unknown";
    return std::format(
        "{} {} actual={} expected{} margin={}",
        status,
        result.key,
        budget_optional_number(result.actual),
        material_budget_bound_expected_text(result),
        budget_optional_number(result.margin));
}

auto material_budget_bound_result_by_key(
        std::vector<MaterialBudgetBoundResult> const& results,
        std::string_view key) -> MaterialBudgetBoundResult const* {
    auto found = std::ranges::find_if(
        results,
        [key](MaterialBudgetBoundResult const& result) {
            return result.key == key;
        });
    return found == results.end() ? nullptr : &*found;
}

auto material_budget_bound_detail_lines(
        std::optional<std::vector<MaterialBudgetBoundResult>> const& results,
        std::optional<MaterialBudgetBoundSummary> const& summary)
    -> std::vector<std::string> {
    auto lines = std::vector<std::string>{};
    if (!results || results->empty())
        return lines;

    auto rendered_keys = std::vector<std::string>{};
    auto const failed_limit = std::size_t{8};
    for (auto const& result : *results) {
        if (!result.ok || *result.ok)
            continue;
        if (rendered_keys.size() >= failed_limit)
            continue;
        lines.push_back("failed: " + material_budget_bound_result_text(result));
        rendered_keys.push_back(result.key);
    }
    auto const failed_count = std::ranges::count_if(
        *results,
        [](MaterialBudgetBoundResult const& result) {
            return result.ok && !*result.ok;
        });
    if (static_cast<std::size_t>(failed_count) > failed_limit) {
        lines.push_back(std::format(
            "+{} more failed bounds",
            static_cast<std::size_t>(failed_count) - failed_limit));
    }

    if (summary && !summary->tightest_bound_key.empty()
        && !contains_field(rendered_keys, summary->tightest_bound_key)) {
        if (auto const* tightest = material_budget_bound_result_by_key(
                *results,
                summary->tightest_bound_key)) {
            lines.push_back(
                "tightest: " + material_budget_bound_result_text(*tightest));
        }
    }
    if (lines.empty()) {
        lines.push_back(
            "first: " + material_budget_bound_result_text(results->front()));
    }
    return lines;
}

auto compact_failure_number_text(double value) -> std::string {
    auto text = std::format("{:.6g}", value);
    return text == "-0" ? "0" : text;
}

auto truncate_failure_text(std::string text, std::size_t max_size = 160)
        -> std::string {
    if (text.size() <= max_size)
        return text;
    if (max_size <= 3)
        return text.substr(0, max_size);
    text.resize(max_size - 3);
    text += "...";
    return text;
}

auto compact_failure_json_text(json::Value const& value,
                               int depth = 0) -> std::string {
    if (value.is_null())
        return "null";
    if (value.is_string())
        return truncate_failure_text(value.as_string());
    if (value.is_bool())
        return value.as_bool() ? "true" : "false";
    if (value.is_number())
        return compact_failure_number_text(value.as_float());
    if (value.is_array()) {
        if (depth > 0)
            return std::format("[{} items]", value.as_array().size());
        auto text = std::string{"["};
        auto index = std::size_t{0};
        for (auto const& item : value.as_array()) {
            if (index >= 4)
                break;
            if (index > 0)
                text += ", ";
            text += compact_failure_json_text(item, depth + 1);
            ++index;
        }
        if (value.as_array().size() > index)
            text += std::format(", +{} more", value.as_array().size() - index);
        text += "]";
        return truncate_failure_text(std::move(text));
    }
    if (value.is_object()) {
        if (depth > 0)
            return std::format("{{{} fields}}", value.as_object().size());
        auto text = std::string{"{"};
        auto index = std::size_t{0};
        for (auto const& [key, item] : value.as_object()) {
            if (index >= 4)
                break;
            if (index > 0)
                text += ", ";
            text += std::format(
                "{}: {}",
                key,
                compact_failure_json_text(item, depth + 1));
            ++index;
        }
        if (value.as_object().size() > index)
            text += std::format(", +{} more", value.as_object().size() - index);
        text += "}";
        return truncate_failure_text(std::move(text));
    }
    return "<unknown>";
}

auto json_object_compact_failure_text(json::Object const& object,
                                      std::string_view key) -> std::string {
    auto const* value = json_object_member(object, key);
    return value ? compact_failure_json_text(*value) : std::string{};
}

auto json_object_failure_string(json::Object const& object,
                                std::string_view key) -> std::string {
    auto const* value = json_object_member(object, key);
    return value && value->is_string() ? value->as_string() : std::string{};
}

auto failure_optional_string_json(std::string_view text) -> std::string {
    return text.empty() ? std::string{"null"} : json_string(text);
}

auto json_object_failure_string_json(json::Object const& object,
                                     std::string_view key) -> std::string {
    return failure_optional_string_json(json_object_failure_string(object, key));
}

auto json_object_compact_failure_text_json(json::Object const& object,
                                           std::string_view key) -> std::string {
    auto text = json_object_compact_failure_text(object, key);
    return failure_optional_string_json(text);
}

auto compact_failure_location_text(json::Object const& failure)
        -> std::string {
    auto parts = std::vector<std::string>{};
    if (auto path = json_object_failure_string(failure, "path"); !path.empty())
        parts.push_back(std::format("path={}", path));
    if (auto region = json_object_failure_string(failure, "region");
        !region.empty()) {
        parts.push_back(std::format("region={}", region));
    }
    if (auto layer = json_object_failure_string(failure, "likely_layer");
        !layer.empty()) {
        parts.push_back(std::format("layer={}", layer));
    }
    if (auto pass = json_object_failure_string(failure, "likely_pass");
        !pass.empty()) {
        parts.push_back(std::format("pass={}", pass));
    }

    auto text = std::string{};
    for (auto const& part : parts) {
        if (!text.empty())
            text += ",";
        text += part;
    }
    return text;
}

auto failure_json_value_or_null(json::Value const& report,
                                std::initializer_list<std::string_view> path)
        -> std::string {
    auto const* value = json_at(report, path);
    return value ? json::emit(*value) : std::string{"null"};
}

auto failure_executor_budget_json(json::Value const& report) -> std::string {
    if (!json_object_at(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget"})) {
        return "null";
    }

    return std::format(
        "{{\"plan_count\":{},\"sampled_backdrop_instance_count\":{},"
        "\"fallback_instance_count\":{},\"draw_calls\":{},"
        "\"total_sample_taps\":{},\"upload_utilization\":{},"
        "\"backdrop_copy_utilization\":{},\"pipeline_ready\":{},"
        "\"backdrop_source_ready\":{},\"upload_status\":{},"
        "\"draw_status\":{},\"backdrop_copy_policy\":{},"
        "\"backdrop_copy_skip_reason\":{}}}",
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "plan_count"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "sampled_backdrop_instance_count"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "fallback_instance_count"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "draw_calls"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "total_sample_taps"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "upload_utilization"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "backdrop_copy_utilization"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "pipeline_ready"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "backdrop_source_ready"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "upload_status"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "draw_status"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "backdrop_copy_policy"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "backdrop_copy_skip_reason"}));
}

auto failure_resource_bounds_json(json::Value const& report) -> std::string {
    auto bounds = material_resource_bounds_from_report(report);
    if (!bounds)
        return "null";
    return std::format(
        "{{\"max_plan_sample_taps\":{},\"total_plan_sample_taps\":{},"
        "\"max_backdrop_pixels\":{},\"max_frame_capture_count\":{},"
        "\"max_frame_capture_pixels\":{},"
        "\"total_surface_sample_pixels\":{},"
        "\"total_execution_stages\":{},\"active_execution_stages\":{},"
        "\"backdrop_execution_stages\":{},\"dropped_execution_stages\":{},"
        "\"total_paint_layers\":{},\"active_paint_layers\":{},"
        "\"dropped_paint_layers\":{},"
        "\"max_pass_texture_copy_pixels\":{},"
        "\"total_pass_texture_copy_pixels\":{},"
        "\"unbounded_texture_copy\":{},"
        "\"non_deterministic_fallback\":{},"
        "\"max_refraction_offset_pixels\":{},"
        "\"max_container_spacing\":{},\"max_paint_layer_inflate\":{}}}",
        manifest_count_json(bounds->max_plan_sample_taps),
        manifest_count_json(bounds->total_plan_sample_taps),
        manifest_count_json(bounds->max_backdrop_pixels),
        manifest_count_json(bounds->max_frame_capture_count),
        manifest_count_json(bounds->max_frame_capture_pixels),
        manifest_count_json(bounds->total_surface_sample_pixels),
        manifest_count_json(bounds->total_execution_stages),
        manifest_count_json(bounds->active_execution_stages),
        manifest_count_json(bounds->backdrop_execution_stages),
        manifest_count_json(bounds->dropped_execution_stages),
        manifest_count_json(bounds->total_paint_layers),
        manifest_count_json(bounds->active_paint_layers),
        manifest_count_json(bounds->dropped_paint_layers),
        manifest_count_json(bounds->max_pass_texture_copy_pixels),
        manifest_count_json(bounds->total_pass_texture_copy_pixels),
        manifest_count_json(bounds->unbounded_texture_copy),
        manifest_count_json(bounds->non_deterministic_fallback),
        budget_ratio(bounds->max_refraction_offset_pixels),
        budget_ratio(bounds->max_container_spacing),
        budget_ratio(bounds->max_paint_layer_inflate));
}

auto failure_material_context_json(json::Value const& report) -> std::string {
    auto resource = failure_resource_bounds_json(report);
    auto quality = material_quality_policy_json(
        material_quality_policy_from_report(report));
    if (resource == "null" && quality == "null")
        return "null";
    return std::format(
        "{{\"resource_bounds\":{},\"quality_policy\":{}}}",
        resource,
        quality);
}

auto failure_artifact_context_json(json::Value const& report) -> std::string {
    if (!json_object_at(report, {"failure_summary", "artifact_context"}))
        return "null";

    return std::format(
        "{{\"platform\":{},\"backend\":{},\"material_contract\":{{"
        "\"semantic_material_nodes\":{},"
        "\"renderer_plan_contract_version\":{},"
        "\"renderer_plan_count\":{},"
        "\"renderer_plans_present\":{},"
        "\"resolved_plan_count\":{},"
        "\"executor_budget\":{},"
        "\"fallback_paths\":{},"
        "\"pass_executors\":{},"
        "\"decision_first_blockers\":{},"
        "\"reference_material_policies\":{},"
        "\"reference_accessibility_responses\":{},"
        "\"reference_performance_responses\":{},"
        "\"decision_reduced_transparency\":{},"
        "\"decision_increase_contrast\":{},"
        "\"decision_reduce_motion\":{},"
        "\"app_probe_contract_name\":{},"
        "\"app_probe_reference_technology\":{}}}}}",
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "platform"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "backend"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "semantic_material_nodes"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "renderer_plan_contract_version"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "renderer_plan_count"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "renderer_plans_present"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "resolved_plan_count"}),
        failure_executor_budget_json(report),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "fallback_paths"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "pass_executors"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "decision_first_blockers"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "plan_reference_model", "material_policies"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "plan_reference_model", "accessibility_responses"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "plan_reference_model", "performance_responses"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "decision_reduced_transparency"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "decision_increase_contrast"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "decision_reduce_motion"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "app_probe_contract_name"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "app_probe_reference_technology"}));
}

auto compact_failure_count_map_text(
        json::Value const& report,
        std::initializer_list<std::string_view> path,
        std::size_t limit = 4)
        -> std::string {
    auto const* object = json_object_at(report, path);
    if (!object)
        return {};

    auto entries = std::vector<std::pair<std::string, std::int64_t>>{};
    for (auto const& [key, value] : *object) {
        if (!value.is_number())
            continue;
        entries.emplace_back(key, value.as_integer());
    }
    if (entries.empty())
        return {};

    std::ranges::sort(entries, [](auto const& left, auto const& right) {
        if (left.second != right.second)
            return left.second > right.second;
        return left.first < right.first;
    });

    auto text = std::string{};
    auto count = std::min(limit, entries.size());
    for (auto index = std::size_t{0}; index < count; ++index) {
        if (!text.empty())
            text += ", ";
        text += std::format(
            "{}={}",
            truncate_failure_text(entries[index].first, 72),
            entries[index].second);
    }
    if (entries.size() > count)
        text += std::format(", +{} more", entries.size() - count);
    return text;
}

auto failure_context_text_value(json::Value const& report,
                                std::initializer_list<std::string_view> path)
        -> std::string {
    auto const* value = json_at(report, path);
    if (!value)
        return {};
    return compact_failure_json_text(*value);
}

void append_failure_context_part(std::vector<std::string>& parts,
                                 std::string_view label,
                                 std::string value) {
    if (value.empty())
        return;
    parts.push_back(std::format("{}={}", label, std::move(value)));
}

auto failure_executor_context_text(json::Value const& report) -> std::string {
    if (!json_object_at(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget"})) {
        return {};
    }

    auto parts = std::vector<std::string>{};
    append_failure_context_part(
        parts,
        "plans",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "plan_count"}));
    append_failure_context_part(
        parts,
        "sampled",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "sampled_backdrop_instance_count"}));
    append_failure_context_part(
        parts,
        "fallback",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "fallback_instance_count"}));
    append_failure_context_part(
        parts,
        "draws",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "draw_calls"}));
    append_failure_context_part(
        parts,
        "taps",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "total_sample_taps"}));
    append_failure_context_part(
        parts,
        "upload",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "upload_status"}));
    append_failure_context_part(
        parts,
        "draw",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "draw_status"}));
    append_failure_context_part(
        parts,
        "backdrop",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "executor_budget", "backdrop_copy_policy"}));

    auto text = std::string{};
    for (auto const& part : parts) {
        if (!text.empty())
            text += ",";
        text += part;
    }
    return text;
}

auto failure_artifact_context_line(json::Value const& report) -> std::string {
    if (!json_object_at(report, {"failure_summary", "artifact_context"}))
        return {};

    auto parts = std::vector<std::string>{};
    append_failure_context_part(
        parts,
        "platform",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "platform"}));
    append_failure_context_part(
        parts,
        "backend",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "backend"}));
    append_failure_context_part(
        parts,
        "semantic-nodes",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "semantic_material_nodes"}));
    append_failure_context_part(
        parts,
        "renderer-plans",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "renderer_plan_count"}));
    append_failure_context_part(
        parts,
        "resolved-plans",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "resolved_plan_count"}));
    append_failure_context_part(
        parts,
        "executor",
        failure_executor_context_text(report));
    append_failure_context_part(
        parts,
        "fallbacks",
        compact_failure_count_map_text(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "fallback_paths"}));
    append_failure_context_part(
        parts,
        "blockers",
        compact_failure_count_map_text(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "decision_first_blockers"}));
    append_failure_context_part(
        parts,
        "pass-executors",
        compact_failure_count_map_text(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "pass_executors"}));
    append_failure_context_part(
        parts,
        "ref-policy",
        compact_failure_count_map_text(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "plan_reference_model", "material_policies"},
            2));
    append_failure_context_part(
        parts,
        "a11y",
        compact_failure_count_map_text(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "plan_reference_model", "accessibility_responses"},
            2));
    append_failure_context_part(
        parts,
        "perf",
        compact_failure_count_map_text(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "plan_reference_model", "performance_responses"},
            2));
    append_failure_context_part(
        parts,
        "reduced-transparency",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "decision_reduced_transparency"}));
    append_failure_context_part(
        parts,
        "increase-contrast",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "decision_increase_contrast"}));
    append_failure_context_part(
        parts,
        "reduce-motion",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "decision_reduce_motion"}));
    append_failure_context_part(
        parts,
        "probe",
        failure_context_text_value(
            report,
            {"failure_summary", "artifact_context", "material_contract",
             "app_probe_reference_technology"}));

    auto text = std::string{};
    for (auto const& part : parts) {
        if (!text.empty())
            text += " ";
        text += part;
    }
    return text;
}

auto failure_resource_bounds_line(json::Value const& report) -> std::string {
    auto bounds = material_resource_bounds_from_report(report);
    if (!bounds)
        return {};
    return std::format(
        "plan-taps={}/{} runtime={}/{}/{} stages-dropped={} "
        "paint={}/{}/{} copy={}/{} unbounded-copy={} "
        "non-deterministic-fallback={} refraction={} spacing={} inflate={}",
        budget_count(bounds->max_plan_sample_taps),
        budget_count(bounds->total_plan_sample_taps),
        budget_count(bounds->total_execution_stages),
        budget_count(bounds->active_execution_stages),
        budget_count(bounds->backdrop_execution_stages),
        budget_count(bounds->dropped_execution_stages),
        budget_count(bounds->total_paint_layers),
        budget_count(bounds->active_paint_layers),
        budget_count(bounds->dropped_paint_layers),
        budget_count(bounds->max_pass_texture_copy_pixels),
        budget_count(bounds->total_pass_texture_copy_pixels),
        budget_count(bounds->unbounded_texture_copy),
        budget_count(bounds->non_deterministic_fallback),
        budget_number_text(bounds->max_refraction_offset_pixels),
        budget_number_text(bounds->max_container_spacing),
        budget_number_text(bounds->max_paint_layer_inflate));
}

auto failure_quality_policy_line(json::Value const& report) -> std::string {
    auto policy = material_quality_policy_from_report(report);
    return policy ? material_quality_policy_text(*policy) : std::string{};
}

auto verifier_bound_summaries_json(json::Value const& report) -> std::string {
    auto executor = material_budget_bound_summary_from_report(report);
    auto resource = material_resource_bound_summary_from_report(report);
    auto quality = material_quality_policy_bound_summary_from_report(report);
    if (!executor && !resource && !quality)
        return "null";

    return std::format(
        "{{\"executor\":{},\"resource\":{},\"quality\":{}}}",
        material_budget_bound_summary_json(executor),
        material_budget_bound_summary_json(resource),
        material_budget_bound_summary_json(quality));
}

auto compact_bound_summary_text(
        std::string_view label,
        std::optional<MaterialBudgetBoundSummary> const& summary)
        -> std::string {
    if (!summary)
        return {};

    auto text = std::format(
        "{}={}/{},fail={}",
        label,
        budget_count(summary->pass_count),
        budget_count(summary->bound_count),
        budget_count(summary->fail_count));
    if (!summary->tightest_bound_key.empty()) {
        text += std::format(",tightest={}", summary->tightest_bound_key);
    }
    if (!summary->failed_keys.empty()) {
        text += std::format(
            ",failed=({})",
            budget_field_list_text(summary->failed_keys));
    }
    return text;
}

auto verifier_bound_summaries_text(json::Value const& report) -> std::string {
    auto parts = std::vector<std::string>{};
    if (auto executor = compact_bound_summary_text(
            "executor",
            material_budget_bound_summary_from_report(report));
        !executor.empty()) {
        parts.push_back(std::move(executor));
    }
    if (auto resource = compact_bound_summary_text(
            "resource",
            material_resource_bound_summary_from_report(report));
        !resource.empty()) {
        parts.push_back(std::move(resource));
    }
    if (auto quality = compact_bound_summary_text(
            "quality",
            material_quality_policy_bound_summary_from_report(report));
        !quality.empty()) {
        parts.push_back(std::move(quality));
    }

    auto text = std::string{};
    for (auto const& part : parts) {
        if (!text.empty())
            text += "; ";
        text += part;
    }
    return text;
}

auto failed_bound_results(
        std::optional<std::vector<MaterialBudgetBoundResult>> const& results)
        -> std::vector<MaterialBudgetBoundResult> {
    auto failed = std::vector<MaterialBudgetBoundResult>{};
    if (!results)
        return failed;
    for (auto const& result : *results) {
        if (result.ok && !*result.ok)
            failed.push_back(result);
    }
    return failed;
}

auto failed_bound_results_json(
        std::vector<MaterialBudgetBoundResult> const& results)
        -> std::string {
    auto out = std::string{"["};
    for (auto index = std::size_t{0}; index < results.size(); ++index) {
        if (index > 0)
            out += ",";
        out += material_budget_bound_result_json(results[index]);
    }
    out += "]";
    return out;
}

auto verifier_failed_bound_results_json(json::Value const& report)
        -> std::string {
    auto executor = failed_bound_results(
        material_budget_bound_results_from_report(report));
    auto resource = failed_bound_results(
        material_resource_bound_results_from_report(report));
    auto quality = failed_bound_results(
        material_quality_policy_bound_results_from_report(report));
    if (executor.empty() && resource.empty() && quality.empty())
        return "null";

    return std::format(
        "{{\"executor\":{},\"resource\":{},\"quality\":{}}}",
        failed_bound_results_json(executor),
        failed_bound_results_json(resource),
        failed_bound_results_json(quality));
}

auto compact_failed_bound_results_text(
        std::string_view label,
        std::vector<MaterialBudgetBoundResult> const& results)
        -> std::string {
    if (results.empty())
        return {};

    auto const limit = std::size_t{3};
    auto count = std::min(limit, results.size());
    auto text = std::format("{}=", label);
    for (auto index = std::size_t{0}; index < count; ++index) {
        if (index > 0)
            text += " | ";
        text += material_budget_bound_result_text(results[index]);
    }
    if (results.size() > count)
        text += std::format(" | +{} more", results.size() - count);
    return text;
}

auto verifier_failed_bound_results_text(json::Value const& report)
        -> std::string {
    auto parts = std::vector<std::string>{};
    if (auto executor = compact_failed_bound_results_text(
            "executor",
            failed_bound_results(
                material_budget_bound_results_from_report(report)));
        !executor.empty()) {
        parts.push_back(std::move(executor));
    }
    if (auto resource = compact_failed_bound_results_text(
            "resource",
            failed_bound_results(
                material_resource_bound_results_from_report(report)));
        !resource.empty()) {
        parts.push_back(std::move(resource));
    }
    if (auto quality = compact_failed_bound_results_text(
            "quality",
            failed_bound_results(
                material_quality_policy_bound_results_from_report(report)));
        !quality.empty()) {
        parts.push_back(std::move(quality));
    }

    auto text = std::string{};
    for (auto const& part : parts) {
        if (!text.empty())
            text += "; ";
        text += part;
    }
    return text;
}

auto tightest_bound_result(
        std::optional<std::vector<MaterialBudgetBoundResult>> const& results,
        std::optional<MaterialBudgetBoundSummary> const& summary)
        -> std::optional<MaterialBudgetBoundResult> {
    if (!results || !summary || summary->tightest_bound_key.empty())
        return std::nullopt;
    auto const* result = material_budget_bound_result_by_key(
        *results,
        summary->tightest_bound_key);
    return result ? std::optional<MaterialBudgetBoundResult>{*result}
                  : std::nullopt;
}

auto verifier_tightest_bound_results_json(json::Value const& report)
        -> std::string {
    auto executor = tightest_bound_result(
        material_budget_bound_results_from_report(report),
        material_budget_bound_summary_from_report(report));
    auto resource = tightest_bound_result(
        material_resource_bound_results_from_report(report),
        material_resource_bound_summary_from_report(report));
    auto quality = tightest_bound_result(
        material_quality_policy_bound_results_from_report(report),
        material_quality_policy_bound_summary_from_report(report));
    if (!executor && !resource && !quality)
        return "null";

    return std::format(
        "{{\"executor\":{},\"resource\":{},\"quality\":{}}}",
        executor ? material_budget_bound_result_json(*executor)
                 : std::string{"null"},
        resource ? material_budget_bound_result_json(*resource)
                 : std::string{"null"},
        quality ? material_budget_bound_result_json(*quality)
                : std::string{"null"});
}

auto compact_tightest_bound_result_text(
        std::string_view label,
        std::optional<MaterialBudgetBoundResult> const& result)
        -> std::string {
    return result
        ? std::format(
            "{}={}",
            label,
            material_budget_bound_result_text(*result))
        : std::string{};
}

auto verifier_tightest_bound_results_text(json::Value const& report)
        -> std::string {
    auto parts = std::vector<std::string>{};
    if (auto executor = compact_tightest_bound_result_text(
            "executor",
            tightest_bound_result(
                material_budget_bound_results_from_report(report),
                material_budget_bound_summary_from_report(report)));
        !executor.empty()) {
        parts.push_back(std::move(executor));
    }
    if (auto resource = compact_tightest_bound_result_text(
            "resource",
            tightest_bound_result(
                material_resource_bound_results_from_report(report),
                material_resource_bound_summary_from_report(report)));
        !resource.empty()) {
        parts.push_back(std::move(resource));
    }
    if (auto quality = compact_tightest_bound_result_text(
            "quality",
            tightest_bound_result(
                material_quality_policy_bound_results_from_report(report),
                material_quality_policy_bound_summary_from_report(report)));
        !quality.empty()) {
        parts.push_back(std::move(quality));
    }

    auto text = std::string{};
    for (auto const& part : parts) {
        if (!text.empty())
            text += "; ";
        text += part;
    }
    return text;
}

struct BoundPressureSource {
    std::string key;
    std::string field;
    std::string bound;
    std::optional<double> expected;
    std::optional<double> actual;
    std::optional<bool> ok;
    std::optional<double> margin;
};

struct BoundPressureMargins {
    std::int64_t zero_count = 0;
    std::int64_t negative_count = 0;
    std::vector<BoundPressureSource> zero_sources;
    std::vector<BoundPressureSource> negative_sources;
};

auto bound_pressure_source_from_result(MaterialBudgetBoundResult const& result)
        -> BoundPressureSource {
    return BoundPressureSource{
        .key = result.key,
        .field = result.field,
        .bound = result.bound,
        .expected = result.expected,
        .actual = result.actual,
        .ok = result.ok,
        .margin = result.margin,
    };
}

auto bound_pressure_margins(
        std::optional<std::vector<MaterialBudgetBoundResult>> const& results)
        -> BoundPressureMargins {
    auto margins = BoundPressureMargins{};
    if (!results)
        return margins;

    for (auto const& result : *results) {
        if (!result.margin)
            continue;
        if (*result.margin == 0.0) {
            ++margins.zero_count;
            margins.zero_sources.push_back(
                bound_pressure_source_from_result(result));
        } else if (*result.margin < 0.0) {
            ++margins.negative_count;
            margins.negative_sources.push_back(
                bound_pressure_source_from_result(result));
        }
    }
    return margins;
}

auto bound_pressure_source_json(BoundPressureSource const& source)
        -> std::string {
    return std::format(
        "{{\"key\":{},\"field\":{},\"bound\":{},\"expected\":{},"
        "\"actual\":{},\"ok\":{},\"margin\":{}}}",
        json_string(source.key),
        json_string(source.field),
        json_string(source.bound),
        budget_optional_number(source.expected),
        budget_optional_number(source.actual),
        budget_bool(source.ok),
        budget_optional_number(source.margin));
}

auto bound_pressure_sources_json(
        std::vector<BoundPressureSource> const& sources) -> std::string {
    auto out = std::string{"["};
    for (auto index = std::size_t{0}; index < sources.size(); ++index) {
        if (index > 0)
            out += ",";
        out += bound_pressure_source_json(sources[index]);
    }
    out += "]";
    return out;
}

auto bound_pressure_source_text(BoundPressureSource const& source)
        -> std::string {
    auto op = std::string{"=="};
    if (source.bound == "lte") {
        op = "<=";
    } else if (source.bound == "gte") {
        op = ">=";
    }
    auto status = source.ok ? (*source.ok ? "pass" : "fail") : "unknown";
    auto target = source.field.empty()
        ? source.key
        : std::format("{}/{}", source.key, source.field);
    return std::format(
        "{} {} actual={} expected{} margin={}",
        status,
        target,
        budget_optional_number(source.actual),
        op + budget_optional_number(source.expected),
        budget_optional_number(source.margin));
}

auto bound_pressure_sources_text(
        std::vector<BoundPressureSource> const& sources) -> std::string {
    auto text = std::string{};
    for (auto index = std::size_t{0}; index < sources.size(); ++index) {
        if (index > 0)
            text += ", ";
        text += bound_pressure_source_text(sources[index]);
    }
    return text;
}

auto bound_pressure_state(
        std::optional<std::vector<MaterialBudgetBoundResult>> const& results,
        std::optional<MaterialBudgetBoundSummary> const& summary)
        -> std::string {
    if (!summary)
        return {};

    auto margins = bound_pressure_margins(results);
    auto tightest = tightest_bound_result(results, summary);

    auto state = std::string{"unknown"};
    if (summary->fail_count > 0 || margins.negative_count > 0) {
        state = "fail";
    } else if (margins.zero_count > 0
               || (summary->tightest_bound_margin
                   && *summary->tightest_bound_margin == 0.0)) {
        state = "tight";
    } else if (summary->pass_count >= 0) {
        state = "pass";
    }

    return std::format(
        "{{\"state\":{},\"bound_count\":{},\"pass_count\":{},"
        "\"fail_count\":{},\"zero_margin_count\":{},"
        "\"negative_margin_count\":{},\"zero_margin_sources\":{},"
        "\"negative_margin_sources\":{},\"tightest_bound_key\":{},"
        "\"tightest_bound_field\":{},\"tightest_bound_margin\":{},"
        "\"tightest_bound_result\":{},\"failed_keys\":{}}}",
        json_string(state),
        manifest_count_json(summary->bound_count),
        manifest_count_json(summary->pass_count),
        manifest_count_json(summary->fail_count),
        margins.zero_count,
        margins.negative_count,
        bound_pressure_sources_json(margins.zero_sources),
        bound_pressure_sources_json(margins.negative_sources),
        json_string(summary->tightest_bound_key),
        json_string(summary->tightest_bound_field),
        budget_optional_number(summary->tightest_bound_margin),
        tightest ? material_budget_bound_result_json(*tightest)
                 : std::string{"null"},
        string_array_json(summary->failed_keys));
}

auto verifier_bound_pressure_json(json::Value const& report) -> std::string {
    auto executor = bound_pressure_state(
        material_budget_bound_results_from_report(report),
        material_budget_bound_summary_from_report(report));
    auto resource = bound_pressure_state(
        material_resource_bound_results_from_report(report),
        material_resource_bound_summary_from_report(report));
    auto quality = bound_pressure_state(
        material_quality_policy_bound_results_from_report(report),
        material_quality_policy_bound_summary_from_report(report));
    if (executor.empty() && resource.empty() && quality.empty())
        return "null";

    return std::format(
        "{{\"executor\":{},\"resource\":{},\"quality\":{}}}",
        executor.empty() ? std::string{"null"} : executor,
        resource.empty() ? std::string{"null"} : resource,
        quality.empty() ? std::string{"null"} : quality);
}

auto verifier_bound_pressure_json(
        std::optional<cppx::process::CapturedProcessResult> const& result)
        -> std::string {
    auto report = verifier_report_from_result(result);
    return report ? verifier_bound_pressure_json(*report) : std::string{"null"};
}

auto bound_pressure_text(
        std::string_view label,
        std::optional<std::vector<MaterialBudgetBoundResult>> const& results,
        std::optional<MaterialBudgetBoundSummary> const& summary)
        -> std::string {
    if (!summary)
        return {};

    auto margins = bound_pressure_margins(results);
    auto tightest = tightest_bound_result(results, summary);

    auto state = std::string{"unknown"};
    if (summary->fail_count > 0 || margins.negative_count > 0) {
        state = "fail";
    } else if (margins.zero_count > 0
               || (summary->tightest_bound_margin
                   && *summary->tightest_bound_margin == 0.0)) {
        state = "tight";
    } else if (summary->pass_count >= 0) {
        state = "pass";
    }

    auto text = std::format(
        "{}={},pass={}/{},fail={},zero={},negative={},tightest={},field={},tightest-margin={}",
        label,
        state,
        budget_count(summary->pass_count),
        budget_count(summary->bound_count),
        budget_count(summary->fail_count),
        margins.zero_count,
        margins.negative_count,
        summary->tightest_bound_key.empty() ? "<none>" : summary->tightest_bound_key,
        summary->tightest_bound_field.empty() ? "<none>" : summary->tightest_bound_field,
        budget_optional_number(summary->tightest_bound_margin));
    if (tightest) {
        text += std::format(
            ",tightest-result=({})",
            bound_pressure_source_text(
                bound_pressure_source_from_result(*tightest)));
    }
    if (!margins.zero_sources.empty()) {
        text += std::format(
            ",zero-sources=({})",
            bound_pressure_sources_text(margins.zero_sources));
    }
    if (!margins.negative_sources.empty()) {
        text += std::format(
            ",negative-sources=({})",
            bound_pressure_sources_text(margins.negative_sources));
    }
    if (!summary->failed_keys.empty()) {
        text += std::format(
            ",failed=({})",
            budget_field_list_text(summary->failed_keys));
    }
    return text;
}

auto verifier_bound_pressure_text(json::Value const& report) -> std::string {
    auto parts = std::vector<std::string>{};
    if (auto executor = bound_pressure_text(
            "executor",
            material_budget_bound_results_from_report(report),
            material_budget_bound_summary_from_report(report));
        !executor.empty()) {
        parts.push_back(std::move(executor));
    }
    if (auto resource = bound_pressure_text(
            "resource",
            material_resource_bound_results_from_report(report),
            material_resource_bound_summary_from_report(report));
        !resource.empty()) {
        parts.push_back(std::move(resource));
    }
    if (auto quality = bound_pressure_text(
            "quality",
            material_quality_policy_bound_results_from_report(report),
            material_quality_policy_bound_summary_from_report(report));
        !quality.empty()) {
        parts.push_back(std::move(quality));
    }

    auto text = std::string{};
    for (auto const& part : parts) {
        if (!text.empty())
            text += "; ";
        text += part;
    }
    return text;
}

auto verifier_bound_pressure_text(
        std::optional<cppx::process::CapturedProcessResult> const& result)
        -> std::string {
    auto report = verifier_report_from_result(result);
    return report ? verifier_bound_pressure_text(*report) : std::string{};
}

auto verifier_material_budget_coverage_json(json::Value const& report)
        -> std::string {
    return material_budget_coverage_json(material_budget_coverage_summary(
        material_budget_from_report(report),
        verifier_manifest_summary_from_report(report)));
}

auto verifier_material_budget_coverage_text(json::Value const& report)
        -> std::string {
    auto coverage = material_budget_coverage_summary(
        material_budget_from_report(report),
        verifier_manifest_summary_from_report(report));
    return coverage ? material_budget_coverage_text(*coverage) : std::string{};
}

auto verifier_manifest_summary_json(
        std::optional<VerifierManifestSummary> const& manifest)
    -> std::string;

auto verifier_manifest_context_json(json::Value const& report) -> std::string {
    return verifier_manifest_summary_json(
        verifier_manifest_summary_from_report(report));
}

auto verifier_manifest_context_text(json::Value const& report) -> std::string {
    auto manifest = verifier_manifest_summary_from_report(report);
    if (!manifest)
        return {};

    auto text = std::format(
        "name={} runtime={} pixel-regions={} budget={} resource={} quality={}",
        manifest->name,
        budget_count(manifest->runtime_numeric_bounds),
        budget_count(manifest->pixel_regions),
        budget_count(manifest->material_executor_budget_bounds),
        budget_count(manifest->material_resource_bounds),
        budget_count(manifest->material_quality_policy_bounds));
    if (!manifest->material_executor_budget_coverage_required_fields.empty()) {
        text += std::format(
            " required-budget=({})",
            budget_field_list_text(
                manifest->material_executor_budget_coverage_required_fields));
    }
    if (!manifest->material_executor_budget_bound_keys.empty()) {
        text += std::format(
            " budget-keys=({})",
            budget_field_list_text(manifest->material_executor_budget_bound_keys));
    }
    if (!manifest->material_resource_bound_keys.empty()) {
        text += std::format(
            " resource-keys=({})",
            budget_field_list_text(manifest->material_resource_bound_keys));
    }
    if (!manifest->material_quality_policy_bound_keys.empty()) {
        text += std::format(
            " quality-keys=({})",
            budget_field_list_text(manifest->material_quality_policy_bound_keys));
    }
    return text;
}

auto verifier_failure_detail_lines(json::Object const& failure)
        -> std::vector<std::string> {
    auto lines = std::vector<std::string>{};
    auto name = json_object_failure_string(failure, "name");
    if (name.empty())
        name = json_object_failure_string(failure, "message");
    if (name.empty())
        name = "unnamed verifier failure";
    lines.push_back("  - " + truncate_failure_text(std::move(name), 180));

    if (auto location = compact_failure_location_text(failure);
        !location.empty()) {
        lines.push_back("    " + location);
    }

    auto expected = json_object_compact_failure_text(failure, "expected");
    auto actual = json_object_compact_failure_text(failure, "actual");
    if (!expected.empty() || !actual.empty()) {
        lines.push_back(std::format(
            "    expected={} actual={}",
            expected.empty() ? "<unspecified>" : expected,
            actual.empty() ? "<unspecified>" : actual));
    }

    if (auto hint = json_object_failure_string(failure, "hint");
        !hint.empty()) {
        lines.push_back(
            "    hint: " + truncate_failure_text(std::move(hint), 220));
    }
    if (auto action = json_object_failure_string(failure, "suggested_action");
        !action.empty()) {
        lines.push_back(
            "    action: " + truncate_failure_text(std::move(action), 220));
    }
    return lines;
}

auto verifier_failure_detail_json(json::Object const& failure)
        -> std::string {
    return std::format(
        "{{\"name\":{},\"message\":{},\"path\":{},\"region\":{},"
        "\"likely_layer\":{},\"likely_pass\":{},\"expected\":{},"
        "\"actual\":{},\"hint\":{},\"suggested_action\":{}}}",
        json_object_failure_string_json(failure, "name"),
        json_object_failure_string_json(failure, "message"),
        json_object_failure_string_json(failure, "path"),
        json_object_failure_string_json(failure, "region"),
        json_object_failure_string_json(failure, "likely_layer"),
        json_object_failure_string_json(failure, "likely_pass"),
        json_object_compact_failure_text_json(failure, "expected"),
        json_object_compact_failure_text_json(failure, "actual"),
        json_object_failure_string_json(failure, "hint"),
        json_object_failure_string_json(failure, "suggested_action"));
}

auto verifier_failure_details_json(json::Value const& report,
                                   std::size_t limit) -> std::string {
    auto const* failures = json_array_at(report, {"failures"});
    if (!failures)
        return "[]";

    auto out = std::string{"["};
    auto printed = std::size_t{0};
    for (auto const& failure : *failures) {
        if (printed >= limit)
            break;
        if (!failure.is_object())
            continue;
        if (printed > 0)
            out += ",";
        out += verifier_failure_detail_json(failure.as_object());
        ++printed;
    }
    out += "]";
    return out;
}

auto verifier_failure_summary_json(json::Value const& report)
        -> std::string {
    auto count = json_integer_at(report, {"failure_summary", "count"})
        .value_or(0);
    if (count <= 0)
        return "null";

    auto const* failures = json_array_at(report, {"failures"});
    auto const limit = std::size_t{3};
    auto printable_count = std::size_t{0};
    if (failures) {
        for (auto const& failure : *failures) {
            if (printable_count >= limit)
                break;
            if (failure.is_object())
                ++printable_count;
        }
    }
    auto truncated = count > static_cast<std::int64_t>(printable_count)
        ? "true"
        : "false";
    return std::format(
        "{{\"count\":{},\"top_likely_layer\":{},"
        "\"top_likely_pass\":{},\"top_suggested_action\":{},"
        "\"artifact_context\":{},\"bound_summaries\":{},"
        "\"failed_bound_results\":{},"
        "\"tightest_bound_results\":{},"
        "\"bound_pressure\":{},"
        "\"manifest_context\":{},\"budget_coverage\":{},"
        "\"material_context\":{},"
        "\"by_likely_layer\":{},\"by_likely_pass\":{},\"by_region\":{},"
        "\"by_path\":{},"
        "\"by_suggested_action\":{},\"first_failure\":{},"
        "\"failures\":{},\"truncated\":{}}}",
        count,
        failure_optional_string_json(json_string_at(
            report,
            {"failure_summary", "top_likely_layer"}).value_or("")),
        failure_optional_string_json(json_string_at(
            report,
            {"failure_summary", "top_likely_pass"}).value_or("")),
        failure_optional_string_json(json_string_at(
            report,
            {"failure_summary", "top_suggested_action"}).value_or("")),
        failure_artifact_context_json(report),
        verifier_bound_summaries_json(report),
        verifier_failed_bound_results_json(report),
        verifier_tightest_bound_results_json(report),
        verifier_bound_pressure_json(report),
        verifier_manifest_context_json(report),
        verifier_material_budget_coverage_json(report),
        failure_material_context_json(report),
        failure_json_value_or_null(
            report,
            {"failure_summary", "by_likely_layer"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "by_likely_pass"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "by_region"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "by_path"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "by_suggested_action"}),
        failure_json_value_or_null(
            report,
            {"failure_summary", "first_failure"}),
        verifier_failure_details_json(report, limit),
        truncated);
}

auto verifier_failure_summary_json(
        std::optional<cppx::process::CapturedProcessResult> const& result)
        -> std::string {
    auto report = verifier_report_from_result(result);
    return report ? verifier_failure_summary_json(*report) : std::string{"null"};
}

auto verifier_failure_summary_lines(json::Value const& report)
        -> std::vector<std::string> {
    auto lines = std::vector<std::string>{};
    auto count = json_integer_at(report, {"failure_summary", "count"})
        .value_or(0);
    if (count <= 0)
        return lines;

    auto top_layer = json_string_at(
        report,
        {"failure_summary", "top_likely_layer"}).value_or("<unknown>");
    auto top_pass = json_string_at(
        report,
        {"failure_summary", "top_likely_pass"}).value_or("<unknown>");
    auto top_action = json_string_at(
        report,
        {"failure_summary", "top_suggested_action"}).value_or("");
    lines.push_back(std::format(
        "verifier failures: count={} top-layer={} top-pass={}",
        count,
        top_layer,
        top_pass));
    if (!top_action.empty()) {
        lines.push_back("  top-action: " + truncate_failure_text(
            std::move(top_action),
            220));
    }
    if (auto context = failure_artifact_context_line(report);
        !context.empty()) {
        lines.push_back("  context: " + context);
    }
    if (auto bounds = verifier_bound_summaries_text(report);
        !bounds.empty()) {
        lines.push_back("  bounds: " + bounds);
    }
    if (auto failed_bounds = verifier_failed_bound_results_text(report);
        !failed_bounds.empty()) {
        lines.push_back("  failed-bounds: " + failed_bounds);
    }
    if (auto tightest_bounds = verifier_tightest_bound_results_text(report);
        !tightest_bounds.empty()) {
        lines.push_back("  tightest-bounds: " + tightest_bounds);
    }
    if (auto pressure = verifier_bound_pressure_text(report);
        !pressure.empty()) {
        lines.push_back("  pressure: " + pressure);
    }
    if (auto manifest = verifier_manifest_context_text(report);
        !manifest.empty()) {
        lines.push_back("  manifest: " + manifest);
    }
    if (auto coverage = verifier_material_budget_coverage_text(report);
        !coverage.empty()) {
        lines.push_back("  coverage: " + coverage);
    }
    if (auto resources = failure_resource_bounds_line(report);
        !resources.empty()) {
        lines.push_back("  resources: " + resources);
    }
    if (auto quality = failure_quality_policy_line(report);
        !quality.empty()) {
        lines.push_back("  quality: " + quality);
    }
    if (auto by_layer = compact_failure_count_map_text(
            report,
            {"failure_summary", "by_likely_layer"});
        !by_layer.empty()) {
        lines.push_back("  by-layer: " + by_layer);
    }
    if (auto by_pass = compact_failure_count_map_text(
            report,
            {"failure_summary", "by_likely_pass"});
        !by_pass.empty()) {
        lines.push_back("  by-pass: " + by_pass);
    }
    if (auto by_region = compact_failure_count_map_text(
            report,
            {"failure_summary", "by_region"},
            3);
        !by_region.empty()) {
        lines.push_back("  by-region: " + by_region);
    }
    if (auto by_path = compact_failure_count_map_text(
            report,
            {"failure_summary", "by_path"},
            3);
        !by_path.empty()) {
        lines.push_back("  by-path: " + by_path);
    }
    if (auto by_action = compact_failure_count_map_text(
            report,
            {"failure_summary", "by_suggested_action"},
            2);
        !by_action.empty()) {
        lines.push_back("  by-action: " + by_action);
    }

    auto const* failures = json_array_at(report, {"failures"});
    if (!failures)
        return lines;
    auto printed = std::size_t{0};
    for (auto const& failure : *failures) {
        if (printed >= 3)
            break;
        if (!failure.is_object())
            continue;
        auto detail_lines = verifier_failure_detail_lines(failure.as_object());
        lines.insert(lines.end(), detail_lines.begin(), detail_lines.end());
        ++printed;
    }
    if (static_cast<std::int64_t>(printed) < count) {
        lines.push_back(std::format(
            "  ... +{} more failures; rerun with --json for the full report",
            count - static_cast<std::int64_t>(printed)));
    }
    return lines;
}

auto verifier_failure_summary_lines(
        std::optional<cppx::process::CapturedProcessResult> const& result)
        -> std::vector<std::string> {
    auto report = verifier_report_from_result(result);
    return report ? verifier_failure_summary_lines(*report)
                  : std::vector<std::string>{};
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
        "\"material_resource_bounds\":{},"
        "\"material_resource_bound_keys\":{},"
        "\"material_resource_bound_fields\":{},"
        "\"material_executor_budget_coverage_required_fields\":{},"
        "\"material_quality_policy_bounds\":{},"
        "\"material_quality_policy_bound_keys\":{},"
        "\"material_quality_policy_fields\":{}}}",
        json_string(manifest->name),
        manifest_count_json(manifest->pixel_regions),
        manifest_count_json(manifest->pixel_region_metrics),
        manifest_count_json(manifest->pixel_region_metric_comparisons),
        manifest_count_json(manifest->forbid_pixel_region_colors),
        manifest_count_json(manifest->runtime_numeric_bounds),
        manifest_count_json(manifest->material_executor_budget_bounds),
        string_array_json(manifest->material_executor_budget_bound_keys),
        string_array_json(manifest->material_executor_budget_fields),
        manifest_count_json(manifest->material_resource_bounds),
        string_array_json(manifest->material_resource_bound_keys),
        string_array_json(manifest->material_resource_bound_fields),
        string_array_json(
            manifest->material_executor_budget_coverage_required_fields),
        manifest_count_json(manifest->material_quality_policy_bounds),
        string_array_json(manifest->material_quality_policy_bound_keys),
        string_array_json(manifest->material_quality_policy_fields));
}

} // namespace phenotype_cli::material_budget

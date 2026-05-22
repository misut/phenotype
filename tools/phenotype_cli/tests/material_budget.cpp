#include <cassert>
#include <string_view>

import json;
import phenotype_cli.material_budget;
import std;

namespace {

using namespace phenotype_cli::material_budget;

auto contains_text(std::string_view haystack, std::string_view needle) -> bool {
    return haystack.find(needle) != std::string_view::npos;
}

auto contains_line(std::vector<std::string> const& lines,
                   std::string_view needle) -> bool {
    return std::ranges::any_of(lines, [&](std::string const& line) {
        return contains_text(line, needle);
    });
}

auto sample_report() -> json::Value {
    return json::parse(R"json({
      "manifest": {
        "name": "unit-material-gate",
        "pixel_regions": 2,
        "pixel_region_metrics": 3,
        "pixel_region_metric_comparisons": 4,
        "forbid_pixel_region_colors": 1,
        "runtime_numeric_bounds": 5,
        "material_executor_budget_bounds": 3,
        "material_executor_budget_bound_keys": [
          "execution_stage_count_lte",
          "draw_calls_gte",
          "upload_utilization_lte"
        ],
        "material_executor_budget_fields": [
          "draw_calls",
          "execution_stage_count",
          "upload_utilization"
        ],
        "material_executor_budget_coverage_required_fields": [
          "draw_calls",
          "execution_stage_count"
        ],
        "material_resource_bounds": 2,
        "material_resource_bound_keys": [
          "max_plan_sample_taps_lte",
          "require_bounded_texture_copy"
        ],
        "material_resource_bound_fields": [
          "max_plan_sample_taps",
          "unbounded_texture_copy"
        ],
        "material_quality_policy_bounds": 2,
        "material_quality_policy_bound_keys": [
          "max_blur_radius_lte",
          "require_noise_allowed"
        ],
        "material_quality_policy_fields": [
          "max_blur_radius",
          "noise_disabled"
        ]
      },
      "artifact_context": {
        "material_contract": {
          "executor_budget": {
            "plan_count": 3,
            "material_instance_count": 3,
            "sampled_backdrop_instance_count": 2,
            "fallback_instance_count": 0,
            "execution_stage_count": 8,
            "active_execution_stage_count": 8,
            "backdrop_execution_stage_count": 2,
            "dropped_execution_stage_count": 0,
            "draw_calls": 2,
            "total_sample_taps": 50,
            "max_sample_taps": 25,
            "upload_bytes": 256,
            "buffer_capacity_bytes": 4096,
            "upload_utilization": 0.0625,
            "backdrop_copy_count": 1,
            "backdrop_copy_pixels": 1024,
            "max_backdrop_pixels": 4096,
            "backdrop_copy_utilization": 0.25,
            "planned_frame_capture_count": 1,
            "planned_frame_capture_pixels": 4096,
            "planned_surface_sample_pixels": 2048,
            "pipeline_ready": true,
            "backdrop_source_ready": true,
            "upload_required": true,
            "draw_required": true,
            "uploaded": true,
            "drawn": true,
            "backdrop_copy_required": true,
            "backdrop_copy_skipped_count": 0,
            "upload_status": "uploaded",
            "draw_status": "drawn",
            "backdrop_copy_policy": "bounded",
            "backdrop_copy_skip_reason": "none"
          },
          "executor_budget_bound_summary": {
            "bound_count": 3,
            "pass_count": 2,
            "fail_count": 1,
            "tightest_bound_key": "upload_utilization_lte",
            "tightest_bound_field": "upload_utilization",
            "tightest_bound_margin": 0.9375,
            "failed_keys": ["draw_calls_gte"]
          },
          "executor_budget_bound_results": [
            {
              "key": "execution_stage_count_lte",
              "field": "execution_stage_count",
              "bound": "lte",
              "expected": 8,
              "actual": 8,
              "ok": true,
              "margin": 0
            },
            {
              "key": "draw_calls_gte",
              "field": "draw_calls",
              "bound": "gte",
              "expected": 3,
              "actual": 2,
              "ok": false,
              "margin": -1
            },
            {
              "key": "upload_utilization_lte",
              "field": "upload_utilization",
              "bound": "lte",
              "expected": 1,
              "actual": 0.0625,
              "ok": true,
              "margin": 0.9375
            }
          ]
        }
      },
      "material_plans": {
        "resource_bounds": {
          "max_plan_blur_radius": 36,
          "max_plan_sample_taps": 25,
          "total_plan_sample_taps": 50,
          "max_budget_blur_radius": 36,
          "max_sample_taps": 25,
          "max_sampling_kernel_radius": 2,
          "max_pass_count": 1,
          "max_execution_stages": 4,
          "max_paint_layers": 3,
          "max_backdrop_pixels": 4096,
          "max_frame_capture_count": 1,
          "max_frame_capture_pixels": 4096,
          "total_surface_sample_pixels": 2048,
          "max_surface_sample_pixels": 1024,
          "max_refraction_offset_pixels": 3.5,
          "max_container_spacing": 20,
          "max_paint_layer_inflate": 6,
          "total_runtime_passes": 3,
          "active_runtime_passes": 3,
          "backdrop_runtime_passes": 2,
          "total_execution_stages": 8,
          "active_execution_stages": 8,
          "backdrop_execution_stages": 2,
          "dropped_execution_stages": 0,
          "max_execution_stage_count": 4,
          "max_execution_stage_capacity": 4,
          "total_paint_layers": 6,
          "active_paint_layers": 6,
          "dropped_paint_layers": 0,
          "max_paint_layer_count": 3,
          "max_paint_layer_capacity": 4,
          "max_pass_texture_copy_pixels": 1024,
          "total_pass_texture_copy_pixels": 2048,
          "unbounded_texture_copy": 0,
          "non_deterministic_fallback": 0
        },
        "resource_bound_summary": {
          "bound_count": 2,
          "pass_count": 2,
          "fail_count": 0,
          "tightest_bound_key": "max_plan_sample_taps_lte",
          "tightest_bound_field": "max_plan_sample_taps",
          "tightest_bound_margin": 0,
          "failed_keys": []
        },
        "resource_bound_results": [
          {
            "key": "max_plan_sample_taps_lte",
            "field": "max_plan_sample_taps",
            "bound": "lte",
            "expected": 25,
            "actual": 25,
            "ok": true,
            "margin": 0
          },
          {
            "key": "require_bounded_texture_copy",
            "field": "unbounded_texture_copy",
            "bound": "equals",
            "expected": 0,
            "actual": 0,
            "ok": true,
            "margin": 0
          }
        ],
        "quality_policy": {
          "backdrop_sampling_disabled": 0,
          "noise_disabled": 0,
          "shadow_disabled": 0,
          "max_blur_radius": 36,
          "max_sample_taps": 25,
          "max_backdrop_pixels": 4096
        },
        "quality_policy_bound_summary": {
          "bound_count": 2,
          "pass_count": 2,
          "fail_count": 0,
          "tightest_bound_key": "require_noise_allowed",
          "tightest_bound_field": "noise_disabled",
          "tightest_bound_margin": 0,
          "failed_keys": []
        },
        "quality_policy_bound_results": [
          {
            "key": "max_blur_radius_lte",
            "field": "max_blur_radius",
            "bound": "lte",
            "expected": 36,
            "actual": 36,
            "ok": true,
            "margin": 0
          },
          {
            "key": "require_noise_allowed",
            "field": "noise_disabled",
            "bound": "equals",
            "expected": 0,
            "actual": 0,
            "ok": true,
            "margin": 0
          }
        ]
      },
      "failure_summary": {
        "count": 4,
        "by_likely_layer": {
          "material-control": 2,
          "platform-runtime": 1
        },
        "by_likely_pass": {
          "material-executor": 2,
          "sampled-backdrop": 1
        },
        "by_suggested_action": {
          "record a semantic material plan before capture": 1,
          "reduce backdrop sample taps before raising the guard": 2,
          "tighten resource bounds before capture": 1
        },
        "top_likely_layer": "material-control",
        "top_likely_pass": "sampled-backdrop",
        "top_suggested_action": "reduce backdrop sample taps before raising the guard",
        "first_failure": {
          "name": "pixel_region_tint_mismatch",
          "path": "window.material.executor_budget.upload_utilization",
          "expected": {
            "max": {
              "upload_utilization": 1
            }
          },
          "actual": 1.25,
          "likely_layer": "material-control",
          "likely_pass": "sampled-backdrop",
          "hint": "keep upload utilization under the manifest guard",
          "suggested_action": "reduce backdrop sample taps before raising the guard"
        }
      },
      "failures": [
        {
          "name": "pixel_region_tint_mismatch",
          "path": "window.material.executor_budget.upload_utilization",
          "region": "toolbarGlass",
          "likely_layer": "material-control",
          "likely_pass": "sampled-backdrop",
          "expected": {
            "max": {
              "upload_utilization": 1
            },
            "samples": [
              "toolbar",
              "sidebar"
            ]
          },
          "actual": 1.25,
          "hint": "keep upload utilization under the manifest guard",
          "suggested_action": "reduce backdrop sample taps before raising the guard"
        },
        {
          "message": "fallback material rendered without semantic plan",
          "path": "material.plans[2].semantic",
          "actual": null,
          "suggested_action": "record a semantic material plan before capture"
        },
        {
          "name": "resource_bound_failed",
          "region": "contentGlass",
          "expected": false,
          "actual": true
        },
        {
          "name": "extra_failure_not_printed"
        }
      ]
    })json");
}

} // namespace

int main() {
    auto report = sample_report();

    auto manifest = verifier_manifest_summary_from_report(report);
    assert(manifest);
    assert(manifest->name == "unit-material-gate");
    assert(manifest->material_executor_budget_bounds == 3);
    assert(manifest->material_resource_bounds == 2);
    assert(manifest->material_quality_policy_bounds == 2);
    assert(manifest->material_executor_budget_bound_keys.size() == 3);
    assert(manifest->material_resource_bound_keys.size() == 2);
    assert(manifest->material_quality_policy_bound_keys.size() == 2);

    auto budget = material_budget_from_report(report);
    assert(budget);
    assert(budget->plan_count == 3);
    assert(budget->execution_stage_count == 8);
    assert(budget->draw_calls == 2);
    assert(budget->upload_utilization == 0.0625);
    assert(budget->backdrop_copy_required && *budget->backdrop_copy_required);
    assert(budget->upload_status == "uploaded");

    auto coverage = material_budget_coverage_summary(budget, manifest);
    assert(coverage);
    assert(coverage->required_field_count == 2);
    assert(coverage->covered_required_field_count == 2);
    assert(coverage->missing_required_field_count == 0);
    assert(coverage->manifest_bound_key_count == 3);
    assert(contains_text(
        material_budget_coverage_text(*coverage),
        "required=2/2"));

    auto resource_bounds = material_resource_bounds_from_report(report);
    assert(resource_bounds);
    assert(resource_bounds->max_plan_sample_taps == 25);
    assert(resource_bounds->total_execution_stages == 8);
    assert(resource_bounds->unbounded_texture_copy == 0);
    assert(material_resource_bounds_lines(*resource_bounds).size() == 4);

    auto quality_policy = material_quality_policy_from_report(report);
    assert(quality_policy);
    assert(quality_policy->noise_disabled == 0);
    assert(quality_policy->max_blur_radius == 36.0);
    assert(contains_text(
        material_quality_policy_text(*quality_policy),
        "noise=0"));

    auto executor_summary = material_budget_bound_summary_from_report(report);
    assert(executor_summary);
    assert(executor_summary->bound_count == 3);
    assert(executor_summary->fail_count == 1);
    assert(executor_summary->failed_keys.size() == 1);
    assert(executor_summary->failed_keys[0] == "draw_calls_gte");

    auto executor_results = material_budget_bound_results_from_report(report);
    assert(executor_results);
    assert(executor_results->size() == 3);
    auto executor_lines = material_budget_bound_detail_lines(
        executor_results,
        executor_summary);
    assert(contains_line(executor_lines, "failed: fail draw_calls_gte"));
    assert(contains_line(executor_lines, "tightest: pass upload_utilization_lte"));
    auto executor_json = material_budget_bound_results_json(executor_results);
    assert(contains_text(executor_json, "\"key\":\"draw_calls_gte\""));
    assert(contains_text(executor_json, "\"margin\":-1"));

    auto resource_summary = material_resource_bound_summary_from_report(report);
    auto resource_results = material_resource_bound_results_from_report(report);
    assert(resource_summary);
    assert(resource_results);
    assert(resource_summary->bound_count == 2);
    assert(resource_results->size() == 2);
    assert(contains_line(
        material_budget_bound_detail_lines(resource_results, resource_summary),
        "tightest: pass max_plan_sample_taps_lte"));

    auto quality_summary =
        material_quality_policy_bound_summary_from_report(report);
    auto quality_results =
        material_quality_policy_bound_results_from_report(report);
    assert(quality_summary);
    assert(quality_results);
    assert(quality_summary->tightest_bound_key == "require_noise_allowed");
    assert(contains_line(
        material_budget_bound_detail_lines(quality_results, quality_summary),
        "tightest: pass require_noise_allowed"));

    auto failure_json = verifier_failure_summary_json(report);
    assert(contains_text(failure_json, "\"count\":4"));
    assert(contains_text(
        failure_json,
        "\"top_likely_layer\":\"material-control\""));
    assert(contains_text(
        failure_json,
        "\"top_likely_pass\":\"sampled-backdrop\""));
    assert(contains_text(
        failure_json,
        "\"top_suggested_action\":\"reduce backdrop sample taps before raising the guard\""));
    assert(contains_text(
        failure_json,
        "\"by_likely_layer\":{\"material-control\":2,\"platform-runtime\":1}"));
    assert(contains_text(
        failure_json,
        "\"by_likely_pass\":{\"material-executor\":2,\"sampled-backdrop\":1}"));
    assert(contains_text(
        failure_json,
        "\"by_suggested_action\":{\"record a semantic material plan before capture\":1,\"reduce backdrop sample taps before raising the guard\":2,\"tighten resource bounds before capture\":1}"));
    assert(contains_text(
        failure_json,
        "\"first_failure\":{\"actual\":1.25"));
    assert(contains_text(
        failure_json,
        "\"name\":\"pixel_region_tint_mismatch\""));
    assert(contains_text(failure_json, "\"actual\":\"1.25\""));
    assert(contains_text(failure_json, "\"truncated\":true"));

    auto failure_lines = verifier_failure_summary_lines(report);
    assert(contains_line(
        failure_lines,
        "verifier failures: count=4 top-layer=material-control top-pass=sampled-backdrop"));
    assert(contains_line(
        failure_lines,
        "top-action: reduce backdrop sample taps before raising the guard"));
    assert(contains_line(
        failure_lines,
        "by-layer: material-control=2, platform-runtime=1"));
    assert(contains_line(
        failure_lines,
        "by-pass: material-executor=2, sampled-backdrop=1"));
    assert(contains_line(
        failure_lines,
        "by-action: reduce backdrop sample taps before raising the guard=2, record a semantic material plan before capture=1, +1 more"));
    assert(contains_line(failure_lines, "pixel_region_tint_mismatch"));
    assert(contains_line(
        failure_lines,
        "path=window.material.executor_budget.upload_utilization,region=toolbarGlass,layer=material-control,pass=sampled-backdrop"));
    assert(contains_line(failure_lines, "actual=1.25"));
    assert(contains_line(
        failure_lines,
        "hint: keep upload utilization under the manifest guard"));
    assert(contains_line(
        failure_lines,
        "action: reduce backdrop sample taps before raising the guard"));
    assert(contains_line(
        failure_lines,
        "... +1 more failures; rerun with --json for the full report"));

    auto passing_report = json::parse(
        R"json({"failure_summary":{"count":0},"failures":[]})json");
    assert(verifier_failure_summary_json(passing_report) == "null");
    assert(verifier_failure_summary_lines(passing_report).empty());
}

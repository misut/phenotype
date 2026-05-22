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

auto count_text(std::string_view haystack, std::string_view needle)
        -> std::size_t {
    auto count = std::size_t{0};
    auto offset = std::size_t{0};
    while (true) {
        auto found = haystack.find(needle, offset);
        if (found == std::string_view::npos)
            return count;
        ++count;
        offset = found + needle.size();
    }
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
        "runtime_details": 4,
        "runtime_detail_paths": [
          "renderer.material_executor_summary.material_draw_status",
          "renderer.material_executor_summary.material_sampled_backdrop_drawn",
          "renderer.material_executor_summary.material_sampled_backdrop_uploaded",
          "renderer.material_executor_summary.material_upload_status"
        ],
        "debug_details": 3,
        "debug_detail_paths": [
          "application.file_explorer.chrome.geometry.titlebar_transparent",
          "application.file_explorer.input.commands.duplicate",
          "platform_capabilities.material_capability_profile"
        ],
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
        "material_executor_budget_coverage_min_bound_key_count": 3,
        "material_executor_budget_coverage_min_guarded_field_count": 2,
        "material_executor_budget_coverage_min_observed_field_count": 21,
        "material_resource_bound_coverage_required_fields": [
          "bounded_texture_copy",
          "max_plan_sample_taps"
        ],
        "material_resource_bound_coverage_min_bound_key_count": 2,
        "material_resource_bound_coverage_min_guarded_field_count": 2,
        "material_resource_bound_coverage_min_observed_field_count": 2,
        "material_resource_bounds": 2,
        "material_resource_bound_keys": [
          "max_plan_sample_taps_lte",
          "require_bounded_texture_copy"
        ],
        "material_resource_bound_fields": [
          "max_plan_sample_taps",
          "bounded_texture_copy"
        ],
        "material_quality_policy_bounds": 2,
        "material_quality_policy_bound_keys": [
          "max_blur_radius_lte",
          "require_noise_allowed"
        ],
        "material_quality_policy_fields": [
          "max_blur_radius",
          "noise_disabled"
        ],
        "material_quality_policy_coverage_required_fields": [
          "max_blur_radius",
          "noise_disabled"
        ],
        "material_quality_policy_coverage_min_bound_key_count": 2,
        "material_quality_policy_coverage_min_guarded_field_count": 2,
        "material_quality_policy_coverage_min_observed_field_count": 6
      },
      "artifact_context": {
        "platform": "test",
        "backend": "synthetic",
        "material_contract": {
          "semantic_material_nodes": 3,
          "renderer_plan_contract_version": 42,
          "renderer_plan_count": 3,
          "renderer_plans_present": true,
          "resolved_plan_count": 3,
          "decision_reduced_transparency": false,
          "decision_increase_contrast": true,
          "decision_reduce_motion": false,
          "app_probe_contract_name": "glass_showcase_material_probe_contract",
          "app_probe_reference_technology": "liquid-glass",
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
          "executor_budget_sources": {
            "draw_calls": {
              "metric": "draw_calls",
              "value": 2,
              "source_key": "material_draw_calls",
              "source_path": "debug.platform_runtime.details.renderer.material_executor_summary.material_draw_calls",
              "likely_layer": "platform-runtime",
              "likely_pass": "material-executor"
            },
            "max_backdrop_pixels": {
              "metric": "max_backdrop_pixels",
              "value": 4096,
              "source_key": "max_backdrop_pixels",
              "source_path": "debug.platform_runtime.details.renderer.material_plans#resource_bounds.max_backdrop_pixels",
              "likely_layer": "platform-runtime",
              "likely_pass": "resource-budget"
            },
            "upload_utilization": {
              "metric": "upload_utilization",
              "value": 0.0625,
              "source_key": "upload_utilization",
              "source_path": "debug.platform_runtime.details.renderer.material_executor_summary.material_upload_bytes",
              "likely_layer": "platform-runtime",
              "likely_pass": "material-executor",
              "detail": {
                "numerator": 256,
                "numerator_path": "debug.platform_runtime.details.renderer.material_executor_summary.material_upload_bytes",
                "denominator": 4096,
                "denominator_path": "debug.platform_runtime.details.renderer.material_executor_summary.material_buffer_capacity_bytes"
              }
            }
          },
          "executor_budget_coverage": {
            "guardable_field_count": 22,
            "observed_field_count": 22,
            "guarded_observed_field_count": 3,
            "unguarded_observed_field_count": 19,
            "required_field_count": 2,
            "covered_required_field_count": 2,
            "missing_required_field_count": 0,
            "manifest_field_count": 3,
            "manifest_bound_key_count": 3,
            "observed_fields": [
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
              "upload_utilization"
            ],
            "guarded_observed_fields": [
              "draw_calls",
              "execution_stage_count",
              "upload_utilization"
            ],
            "unguarded_observed_fields": [
              "active_execution_stage_count",
              "backdrop_copy_count",
              "backdrop_copy_pixels",
              "backdrop_copy_skipped_count",
              "backdrop_copy_utilization",
              "backdrop_execution_stage_count",
              "buffer_capacity_bytes",
              "dropped_execution_stage_count",
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
              "upload_bytes"
            ],
            "unguarded_observed_sources": {
              "max_backdrop_pixels": {
                "metric": "max_backdrop_pixels",
                "value": 4096,
                "source_key": "max_backdrop_pixels",
                "source_path": "debug.platform_runtime.details.renderer.material_plans#resource_bounds.max_backdrop_pixels",
                "likely_layer": "platform-runtime",
                "likely_pass": "resource-budget"
              }
            },
            "required_fields": [
              "draw_calls",
              "execution_stage_count"
            ],
            "covered_required_fields": [
              "draw_calls",
              "execution_stage_count"
            ],
            "missing_required_fields": [],
            "missing_observed_fields": [],
            "manifest_fields": [
              "draw_calls",
              "execution_stage_count",
              "upload_utilization"
            ],
            "manifest_bound_keys": [
              "draw_calls_gte",
              "execution_stage_count_lte",
              "upload_utilization_lte"
            ]
          },
          "executor_budget_bound_summary": {
            "bound_count": 3,
            "pass_count": 2,
            "fail_count": 1,
            "zero_margin_count": 1,
            "negative_margin_count": 1,
            "zero_margin_sources": [
              {
                "key": "execution_stage_count_lte",
                "field": "execution_stage_count",
                "bound": "lte",
                "expected": 8,
                "actual": 8,
                "ok": true,
                "margin": 0
              }
            ],
            "negative_margin_sources": [
              {
                "key": "draw_calls_gte",
                "field": "draw_calls",
                "bound": "gte",
                "expected": 3,
                "actual": 2,
                "ok": false,
                "margin": -1
              }
            ],
            "tightest_bound_key": "upload_utilization_lte",
            "tightest_bound_field": "upload_utilization",
            "tightest_bound_margin": 0.9375,
            "tightest_bound_result": {
              "key": "upload_utilization_lte",
              "field": "upload_utilization",
              "bound": "lte",
              "expected": 1,
              "actual": 0.0625,
              "ok": true,
              "margin": 0.9375
            },
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
        "resource_bound_sources": {
          "max_plan_sample_taps": {
            "metric": "max_plan_sample_taps",
            "value": 25,
            "plan_id": "material.toolbar.liquid-glass",
            "kind": "regular",
            "role": "toolbar",
            "plan_path": "debug.platform_runtime.details.renderer.material_plans[0]",
            "source_path": "debug.platform_runtime.details.renderer.material_plans[0].sample_taps",
            "likely_layer": "material-blur-pass",
            "likely_pass": "material-plan",
            "container": {
              "mode": "union",
              "container_id": 41,
              "union_id": 7,
              "spacing": 20
            }
          },
          "max_pass_texture_copy_pixels": {
            "metric": "max_pass_texture_copy_pixels",
            "value": 1024,
            "plan_id": "material.content.liquid-glass",
            "kind": "thin",
            "role": "content",
            "plan_path": "debug.platform_runtime.details.renderer.material_plans[1]",
            "source_path": "debug.platform_runtime.details.renderer.material_plans[1].passes[1].max_texture_copy_pixels",
            "likely_layer": "material-blur-pass",
            "likely_pass": "backdrop-sample-blur",
            "container": {
              "mode": "union",
              "container_id": 41,
              "union_id": 7,
              "spacing": 20
            },
            "detail": {
              "source": "passes",
              "index": 1
            }
          },
          "max_refraction_offset_pixels": {
            "metric": "max_refraction_offset_pixels",
            "value": 3.5,
            "plan_id": "material.content.liquid-glass",
            "kind": "thin",
            "role": "content",
            "plan_path": "debug.platform_runtime.details.renderer.material_plans[1]",
            "source_path": "debug.platform_runtime.details.renderer.material_plans[1].resource_budget.max_refraction_offset_pixels",
            "likely_layer": "material-refraction",
            "likely_pass": "resource-budget",
            "container": {
              "mode": "union",
              "container_id": 41,
              "union_id": 7,
              "spacing": 20
            }
          }
        },
        "container_groups": {
          "group_count": 3,
          "multi_surface_group_count": 1,
          "union_group_count": 1,
          "morph_group_count": 2,
          "interactive_group_count": 1,
          "shared_backdrop_scope_group_count": 3,
          "shared_capture_surface_count": 6,
          "shared_capture_saved_surface_count": 3,
          "max_shared_capture_group_surfaces": 4,
          "shape_blend_execution_group_count": 1,
          "shape_blend_execution_surface_count": 4,
          "fallback_mixed_group_count": 0,
          "max_group_size": 4,
          "max_active_surfaces": 4,
          "max_sampled_backdrop_surfaces": 4,
          "max_fallback_surfaces": 0,
          "total_shape_pair_count": 6,
          "blend_candidate_pair_count": 3,
          "union_candidate_pair_count": 0,
          "morph_candidate_pair_count": 3,
          "separated_pair_count": 3,
          "min_shape_gap": 8,
          "max_shape_gap": 656,
          "max_blend_distance": 28,
          "max_group_bounds_width": 472,
          "max_group_bounds_height": 1288,
          "max_group_bounds_area": 566720,
          "max_shape_blend_strength": 0.6
        },
        "container_group_sources": {
          "max_group_size": {
            "metric": "max_group_size",
            "container_id": 41,
            "surface_count": 4,
            "active_surfaces": 4,
            "sampled_backdrop_surfaces": 4,
            "fallback_surfaces": 0,
            "union_surfaces": 1,
            "morph_surfaces": 2,
            "interactive_surfaces": 1,
            "shared_backdrop_scope_surfaces": 4,
            "shared_capture_saved_surfaces": 3,
            "shape_blend_execution": true,
            "shape_blend_execution_surfaces": 4,
            "shape_blend_strength": 0.6,
            "shape_pair_count": 6,
            "blend_candidate_pair_count": 3,
            "union_candidate_pair_count": 0,
            "morph_candidate_pair_count": 3,
            "separated_pair_count": 3,
            "min_shape_gap": 8,
            "max_shape_gap": 656,
            "max_blend_distance": 28,
            "bounds": {"w": 472, "h": 1288, "area": 566720},
            "plan_ids": [
              "material.toolbar.liquid-glass",
              "material.content.liquid-glass"
            ],
            "roles": ["toolbar", "content"],
            "kinds": ["regular", "thin"],
            "plan_paths": [
              "debug.platform_runtime.details.renderer.material_plans[0]",
              "debug.platform_runtime.details.renderer.material_plans[1]"
            ]
          },
          "max_group_bounds_area": {
            "metric": "max_group_bounds_area",
            "container_id": 41,
            "surface_count": 4,
            "active_surfaces": 4,
            "sampled_backdrop_surfaces": 4,
            "fallback_surfaces": 0,
            "union_surfaces": 1,
            "morph_surfaces": 2,
            "interactive_surfaces": 1,
            "shared_backdrop_scope_surfaces": 4,
            "shared_capture_saved_surfaces": 3,
            "shape_blend_execution": true,
            "shape_blend_execution_surfaces": 4,
            "shape_blend_strength": 0.6,
            "shape_pair_count": 6,
            "blend_candidate_pair_count": 3,
            "union_candidate_pair_count": 0,
            "morph_candidate_pair_count": 3,
            "separated_pair_count": 3,
            "min_shape_gap": 8,
            "max_shape_gap": 656,
            "max_blend_distance": 28,
            "bounds": {"w": 472, "h": 1288, "area": 566720},
            "plan_ids": ["material.toolbar.liquid-glass"],
            "roles": ["toolbar"],
            "kinds": ["regular"],
            "plan_paths": [
              "debug.platform_runtime.details.renderer.material_plans[0]"
            ]
          }
        },
        "resource_bound_coverage": {
          "guardable_field_count": 4,
          "observed_field_count": 3,
          "guarded_field_count": 2,
          "required_field_count": 2,
          "covered_required_field_count": 2,
          "observed_required_field_count": 2,
          "missing_guarded_fields": [],
          "missing_observed_fields": [],
          "unguarded_observed_field_count": 1,
          "observed_fields": [
            "bounded_texture_copy",
            "max_frame_capture_pixels",
            "max_plan_sample_taps"
          ],
          "guarded_fields": [
            "bounded_texture_copy",
            "max_plan_sample_taps"
          ],
          "unguarded_observed_fields": [
            "max_frame_capture_pixels"
          ],
          "unguarded_observed_sources": {
            "max_frame_capture_pixels": {
              "metric": "max_frame_capture_pixels",
              "value": 4096,
              "plan_id": "material.content.liquid-glass",
              "kind": "thin",
              "role": "content",
              "plan_path": "debug.platform_runtime.details.renderer.material_plans[1]",
              "source_path": "debug.platform_runtime.details.renderer.material_plans[1].resource_budget.max_frame_capture_pixels",
              "likely_layer": "material-backdrop",
              "likely_pass": "resource-budget"
            }
          },
          "required_fields": [
            "bounded_texture_copy",
            "max_plan_sample_taps"
          ],
          "bound_key_count": 2,
          "bound_keys": [
            "max_plan_sample_taps_lte",
            "require_bounded_texture_copy"
          ]
        },
	        "resource_bound_summary": {
	          "bound_count": 2,
	          "pass_count": 2,
	          "fail_count": 0,
	          "zero_margin_count": 2,
	          "negative_margin_count": 0,
	          "zero_margin_sources": [
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
	          "negative_margin_sources": [],
	          "tightest_bound_key": "max_plan_sample_taps_lte",
	          "tightest_bound_field": "max_plan_sample_taps",
	          "tightest_bound_margin": 0,
	          "tightest_bound_result": {
	            "key": "max_plan_sample_taps_lte",
	            "field": "max_plan_sample_taps",
	            "bound": "lte",
	            "expected": 25,
	            "actual": 25,
	            "ok": true,
	            "margin": 0
	          },
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
        "quality_policy_sources": {
          "max_blur_radius": {
            "metric": "max_blur_radius",
            "value": 36,
            "plan_id": "material.toolbar.liquid-glass",
            "kind": "regular",
            "role": "toolbar",
            "plan_path": "debug.platform_runtime.details.renderer.material_plans[0]",
            "source_path": "debug.platform_runtime.details.renderer.material_plans[0].quality_policy.max_blur_radius",
            "likely_layer": "material-blur-pass",
            "likely_pass": "quality-policy",
            "container": {
              "mode": "union",
              "container_id": 41,
              "union_id": 7,
              "spacing": 20
            }
          },
          "max_sample_taps": {
            "metric": "max_sample_taps",
            "value": 25,
            "plan_id": "material.toolbar.liquid-glass",
            "kind": "regular",
            "role": "toolbar",
            "plan_path": "debug.platform_runtime.details.renderer.material_plans[0]",
            "source_path": "debug.platform_runtime.details.renderer.material_plans[0].quality_policy.max_sample_taps",
            "likely_layer": "material-blur-pass",
            "likely_pass": "quality-policy",
            "container": {
              "mode": "union",
              "container_id": 41,
              "union_id": 7,
              "spacing": 20
            }
          },
          "max_backdrop_pixels": {
            "metric": "max_backdrop_pixels",
            "value": 4096,
            "plan_id": "material.content.liquid-glass",
            "kind": "thin",
            "role": "content",
            "plan_path": "debug.platform_runtime.details.renderer.material_plans[1]",
            "source_path": "debug.platform_runtime.details.renderer.material_plans[1].quality_policy.max_backdrop_pixels",
            "likely_layer": "material-blur-pass",
            "likely_pass": "quality-policy",
            "container": {
              "mode": "union",
              "container_id": 41,
              "union_id": 7,
              "spacing": 20
            }
          }
        },
        "quality_policy_bound_coverage": {
          "guardable_field_count": 6,
          "observed_field_count": 6,
          "guarded_field_count": 2,
          "required_field_count": 2,
          "covered_required_field_count": 2,
          "observed_required_field_count": 2,
          "missing_guarded_fields": [],
          "missing_observed_fields": [],
          "unguarded_observed_field_count": 4,
          "observed_fields": [
            "backdrop_sampling_disabled",
            "max_backdrop_pixels",
            "max_blur_radius",
            "max_sample_taps",
            "noise_disabled",
            "shadow_disabled"
          ],
          "guarded_fields": [
            "max_blur_radius",
            "noise_disabled"
          ],
          "unguarded_observed_fields": [
            "backdrop_sampling_disabled",
            "max_backdrop_pixels",
            "max_sample_taps",
            "shadow_disabled"
          ],
          "unguarded_observed_sources": {
            "max_sample_taps": {
              "metric": "max_sample_taps",
              "value": 25,
              "plan_id": "material.toolbar.liquid-glass",
              "kind": "regular",
              "role": "toolbar",
              "plan_path": "debug.platform_runtime.details.renderer.material_plans[0]",
              "source_path": "debug.platform_runtime.details.renderer.material_plans[0].quality_policy.max_sample_taps",
              "likely_layer": "material-blur-pass",
              "likely_pass": "quality-policy"
            }
          },
          "required_fields": [
            "max_blur_radius",
            "noise_disabled"
          ],
          "bound_key_count": 2,
          "bound_keys": [
            "max_blur_radius_lte",
            "require_noise_allowed"
          ]
        },
	        "quality_policy_bound_summary": {
	          "bound_count": 2,
	          "pass_count": 2,
	          "fail_count": 0,
	          "zero_margin_count": 2,
	          "negative_margin_count": 0,
	          "zero_margin_sources": [
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
	          ],
	          "negative_margin_sources": [],
	          "tightest_bound_key": "require_noise_allowed",
	          "tightest_bound_field": "noise_disabled",
	          "tightest_bound_margin": 0,
	          "tightest_bound_result": {
	            "key": "require_noise_allowed",
	            "field": "noise_disabled",
	            "bound": "equals",
	            "expected": 0,
	            "actual": 0,
	            "ok": true,
	            "margin": 0
	          },
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
        "by_region": {
          "contentGlass": 1,
          "toolbarGlass": 1
        },
        "by_path": {
          "material.plans[2].semantic": 1,
          "window.material.executor_budget.upload_utilization": 2,
          "window.material.resource_bounds.unbounded_texture_copy": 1,
          "window.pixel_regions.toolbarGlass": 1
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
        },
        "artifact_context": {
          "platform": "test",
          "backend": "synthetic",
          "material_contract": {
            "semantic_material_nodes": 3,
            "renderer_plan_contract_version": 42,
            "renderer_plan_count": 3,
            "renderer_plans_present": true,
            "resolved_plan_count": 3,
            "executor_budget": {
              "plan_count": 3,
              "sampled_backdrop_instance_count": 2,
              "fallback_instance_count": 0,
              "draw_calls": 2,
              "total_sample_taps": 50,
              "upload_utilization": 0.0625,
              "backdrop_copy_utilization": 0.25,
              "pipeline_ready": true,
              "backdrop_source_ready": true,
              "upload_status": "uploaded",
              "draw_status": "drawn",
              "backdrop_copy_policy": "bounded",
              "backdrop_copy_skip_reason": "none"
            },
            "executor_budget_sources": {
              "draw_calls": {
                "metric": "draw_calls",
                "value": 2,
                "source_key": "material_draw_calls",
                "source_path": "debug.platform_runtime.details.renderer.material_executor_summary.material_draw_calls",
                "likely_layer": "platform-runtime",
                "likely_pass": "material-executor"
              },
              "upload_utilization": {
                "metric": "upload_utilization",
                "value": 0.0625,
                "source_key": "upload_utilization",
                "source_path": "debug.platform_runtime.details.renderer.material_executor_summary.material_upload_bytes",
                "likely_layer": "platform-runtime",
                "likely_pass": "material-executor",
                "detail": {
                  "numerator": 256,
                  "numerator_path": "debug.platform_runtime.details.renderer.material_executor_summary.material_upload_bytes",
                  "denominator": 4096,
                  "denominator_path": "debug.platform_runtime.details.renderer.material_executor_summary.material_buffer_capacity_bytes"
                }
              }
            },
            "fallback_paths": {
              "unsupported-backend": 1
            },
            "pass_executors": {
              "color-fill": 1,
              "sampled-backdrop": 2
            },
            "decision_first_blockers": {
              "reduced-transparency": 1,
              "unsupported-backend": 1
            },
            "plan_reference_model": {
              "material_policies": {
                "system-adaptive": 2,
                "standard-material": 1
              },
              "accessibility_responses": {
                "increase-contrast": 1,
                "reduce-transparency": 2
              },
              "performance_responses": {
                "bounded-backdrop": 2,
                "deterministic-fallback": 1
              }
            },
            "decision_reduced_transparency": false,
            "decision_increase_contrast": true,
            "decision_reduce_motion": false,
            "app_probe_contract_name": "glass_showcase_material_probe_contract",
            "app_probe_reference_technology": "liquid-glass"
          }
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
          "name": "material resource bound coverage guards required fields",
          "region": "contentGlass",
          "expected": {
            "required_fields": [
              "max_frame_capture_pixels"
            ]
          },
          "actual": {
            "guarded_fields": [
              "bounded_texture_copy",
              "max_plan_sample_taps"
            ],
            "missing_fields": [
              "max_frame_capture_pixels"
            ],
            "missing_field_sources": {
              "max_frame_capture_pixels": {
                "metric": "max_frame_capture_pixels",
                "value": 4096,
                "source_path": "debug.platform_runtime.details.renderer.material_plans[1].resource_budget.max_frame_capture_pixels",
                "likely_pass": "resource-budget"
              }
            }
          }
        },
        {
          "name": "material executor budget coverage min_guarded_field_count is satisfied",
          "path": "manifest.require_material_executor_budget_coverage.min_guarded_field_count",
          "expected": {
            ">=": 2
          },
          "actual": {
            "count": 1,
            "bound_keys": [
              "draw_calls_gte"
            ],
            "guarded_fields": [
              "draw_calls"
            ],
            "unguarded_observed_fields": [
              "planned_frame_capture_pixels"
            ],
            "unguarded_observed_sources": {
              "planned_frame_capture_pixels": {
                "metric": "planned_frame_capture_pixels",
                "value": 0,
                "source_path": "debug.platform_runtime.details.renderer.material_executor_summary.planned_frame_capture_pixels",
                "likely_pass": "material-executor"
              }
            }
          }
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
    assert(manifest->runtime_details == 4);
    assert(manifest->runtime_detail_paths.size() == 4);
    assert(manifest->debug_details == 3);
    assert(manifest->debug_detail_paths.size() == 3);
    assert(manifest->material_executor_budget_bounds == 3);
    assert(manifest->material_resource_bounds == 2);
    assert(manifest->material_quality_policy_bounds == 2);
    assert(manifest->material_executor_budget_bound_keys.size() == 3);
    assert(manifest->material_resource_bound_keys.size() == 2);
    assert(manifest->material_quality_policy_bound_keys.size() == 2);
    assert(
        manifest->material_resource_bound_coverage_required_fields.size() == 2);
    assert(
        manifest->material_quality_policy_coverage_required_fields.size() == 2);
    assert(manifest->material_executor_budget_coverage_min_bound_key_count == 3);
    assert(
        manifest->material_executor_budget_coverage_min_observed_field_count
        == 21);
    assert(manifest->material_resource_bound_coverage_min_guarded_field_count
        == 2);
    assert(manifest->material_quality_policy_coverage_min_observed_field_count
        == 6);
    assert(contains_text(
        verifier_manifest_coverage_minimums_text(*manifest),
        "budget=(keys=3 guarded=2 observed=21)"));
    assert(contains_text(
        verifier_manifest_runtime_detail_paths_text(*manifest),
        "renderer.material_executor_summary.material_draw_status"));
    assert(contains_text(
        verifier_manifest_debug_detail_paths_text(*manifest),
        "application.file_explorer.chrome.geometry.titlebar_transparent"));
    auto manifest_json = verifier_manifest_summary_json(manifest);
    assert(contains_text(manifest_json, R"("runtime_details":4)"));
    assert(contains_text(manifest_json, R"("debug_details":3)"));
    assert(contains_text(
        manifest_json,
        R"("runtime_detail_paths":["renderer.material_executor_summary.material_draw_status")"));
    assert(contains_text(
        manifest_json,
        R"("debug_detail_paths":["application.file_explorer.chrome.geometry.titlebar_transparent")"));
    auto pressure_sources = std::vector<BoundPressureSource>{
        {.key = "a", .field = "field_a", .bound = "lte",
         .expected = 1.0, .actual = 1.0, .ok = true, .margin = 0.0},
        {.key = "b", .field = "field_b", .bound = "lte",
         .expected = 2.0, .actual = 2.0, .ok = true, .margin = 0.0},
        {.key = "c", .field = "field_c", .bound = "lte",
         .expected = 3.0, .actual = 3.0, .ok = true, .margin = 0.0},
        {.key = "d", .field = "field_d", .bound = "lte",
         .expected = 4.0, .actual = 4.0, .ok = true, .margin = 0.0},
        {.key = "e", .field = "field_e", .bound = "lte",
         .expected = 5.0, .actual = 5.0, .ok = true, .margin = 0.0},
    };
    assert(contains_text(
        bound_pressure_sources_text(pressure_sources),
        "+1 more"));

    auto budget = material_budget_from_report(report);
    assert(budget);
    assert(budget->plan_count == 3);
    assert(budget->execution_stage_count == 8);
    assert(budget->draw_calls == 2);
    assert(budget->upload_utilization == 0.0625);
    assert(budget->backdrop_copy_required && *budget->backdrop_copy_required);
    assert(budget->upload_status == "uploaded");
    assert(budget->sources.size() == 3);
    assert(budget->sources[0].metric == "draw_calls");
    assert(budget->sources[0].source_key == "material_draw_calls");
    assert(budget->sources[1].metric == "max_backdrop_pixels");
    assert(budget->sources[1].likely_pass == "resource-budget");
    assert(budget->sources[2].metric == "upload_utilization");
    assert(contains_text(
        budget->sources[2].detail_json,
        "\"denominator_path\""));
    assert(contains_text(
        material_budget_json(budget),
        "\"sources\":{\"draw_calls\":{\"metric\":\"draw_calls\""));
    assert(contains_text(
        material_budget_sources_text(budget->sources),
        "draw_calls=2 layer=platform-runtime"));

    auto coverage = material_budget_coverage_summary(budget, manifest);
    assert(coverage);
    assert(coverage->required_field_count == 2);
    assert(coverage->covered_required_field_count == 2);
    assert(coverage->missing_required_field_count == 0);
    assert(coverage->manifest_bound_key_count == 3);
    assert(contains_text(
        material_budget_coverage_text(*coverage),
        "required=2/2"));
    auto raw_coverage = material_budget_coverage_from_report(report);
    assert(raw_coverage);
    assert(raw_coverage->guarded_observed_field_count == 3);
    assert(raw_coverage->unguarded_observed_field_count == 19);
    assert(raw_coverage->manifest_bound_key_count == 3);
    assert(raw_coverage->manifest_bound_keys.size() == 3);
    assert(contains_text(
        raw_coverage->unguarded_observed_sources_json,
        "\"max_backdrop_pixels\""));
    assert(contains_text(
        raw_coverage->unguarded_observed_sources_json,
        "material_plans#resource_bounds.max_backdrop_pixels"));
    assert(contains_text(
        verifier_material_budget_coverage_json(report),
        "\"manifest_bound_keys\":[\"draw_calls_gte\","
        "\"execution_stage_count_lte\",\"upload_utilization_lte\"]"));
    assert(contains_text(
        verifier_material_budget_coverage_json(report),
        "\"unguarded_observed_sources\":{\"max_backdrop_pixels\""));
    assert(contains_text(
        verifier_material_budget_coverage_text(report),
        "unguarded=19"));
    assert(contains_text(
        verifier_material_budget_coverage_text(report),
        "guard-key-list=(draw_calls_gte, execution_stage_count_lte, "
        "upload_utilization_lte)"));
    assert(contains_text(
        verifier_material_budget_coverage_text(report),
        "unguarded-sources=(max_backdrop_pixels=4096 pass=resource-budget"));

    auto resource_coverage =
        material_resource_bound_coverage_from_report(report);
    assert(resource_coverage);
    assert(resource_coverage->bound_key_count == 2);
    assert(resource_coverage->bound_keys.size() == 2);
    assert(resource_coverage->bound_keys[0] == "max_plan_sample_taps_lte");
    assert(resource_coverage->bound_keys[1] == "require_bounded_texture_copy");
    assert(resource_coverage->guarded_field_count == 2);
    assert(resource_coverage->observed_field_count == 3);
    assert(resource_coverage->covered_required_field_count == 2);
    assert(resource_coverage->observed_required_field_count == 2);
    assert(resource_coverage->unguarded_observed_field_count == 1);
    assert(resource_coverage->unguarded_observed_fields.size() == 1);
    assert(resource_coverage->missing_guarded_fields.empty());
    assert(resource_coverage->missing_observed_fields.empty());
    assert(contains_text(
        material_bound_coverage_json(resource_coverage),
        "\"bound_keys\":[\"max_plan_sample_taps_lte\","
        "\"require_bounded_texture_copy\"]"));
    assert(contains_text(
        material_bound_coverage_json(resource_coverage),
        "\"unguarded_observed_fields\":[\"max_frame_capture_pixels\"]"));
    assert(contains_text(
        material_bound_coverage_json(resource_coverage),
        "\"unguarded_observed_sources\":{\"max_frame_capture_pixels\""));
    assert(contains_text(
        material_bound_coverage_json(resource_coverage),
        "\"required_fields\":[\"bounded_texture_copy\",\"max_plan_sample_taps\"]"));
    assert(contains_text(
        material_bound_coverage_text(*resource_coverage),
        "guard-key-list=(max_plan_sample_taps_lte, "
        "require_bounded_texture_copy)"));
    assert(contains_text(
        material_bound_coverage_text(*resource_coverage),
        "unguarded=1 (max_frame_capture_pixels)"));
    assert(contains_text(
        material_bound_coverage_text(*resource_coverage),
        "unguarded-sources=(max_frame_capture_pixels=4096 pass=resource-budget"));

    auto quality_coverage =
        material_quality_policy_coverage_from_report(report);
    assert(quality_coverage);
    assert(quality_coverage->guardable_field_count == 6);
    assert(quality_coverage->observed_field_count == 6);
    assert(quality_coverage->guarded_field_count == 2);
    assert(quality_coverage->bound_keys.size() == 2);
    assert(quality_coverage->bound_keys[0] == "max_blur_radius_lte");
    assert(quality_coverage->bound_keys[1] == "require_noise_allowed");
    assert(contains_text(
        material_bound_coverage_json(quality_coverage),
        "\"bound_keys\":[\"max_blur_radius_lte\","
        "\"require_noise_allowed\"]"));
    assert(contains_text(
        material_bound_coverage_text(*quality_coverage),
        "unguarded=4 (backdrop_sampling_disabled, max_backdrop_pixels, "
        "max_sample_taps, shadow_disabled)"));
    assert(contains_text(
        material_bound_coverage_text(*quality_coverage),
        "guard-key-list=(max_blur_radius_lte, require_noise_allowed)"));
    assert(contains_text(
        material_bound_coverage_json(quality_coverage),
        "\"unguarded_observed_sources\":{\"max_sample_taps\""));
    assert(contains_text(
        material_bound_coverage_text(*quality_coverage),
        "unguarded-sources=(max_sample_taps=25 pass=quality-policy"));

    auto resource_bounds = material_resource_bounds_from_report(report);
    assert(resource_bounds);
    assert(resource_bounds->max_plan_sample_taps == 25);
    assert(resource_bounds->total_execution_stages == 8);
    assert(resource_bounds->unbounded_texture_copy == 0);
    assert(resource_bounds->sources.size() == 3);
    assert(resource_bounds->sources[0].metric == "max_plan_sample_taps");
    assert(resource_bounds->sources[0].plan_id
        == "material.toolbar.liquid-glass");
    assert(contains_text(resource_bounds->sources[1].detail_json, "\"source\""));
    assert(contains_text(resource_bounds->sources[1].detail_json, "\"passes\""));
    assert(contains_text(resource_bounds->sources[1].detail_json, "\"index\""));
    assert(contains_text(
        material_resource_bounds_json(resource_bounds),
        "\"sources\":{\"max_plan_sample_taps\":{\"metric\":\"max_plan_sample_taps\""));
    assert(material_resource_bounds_lines(*resource_bounds).size() == 5);
    assert(contains_line(
        material_resource_bounds_lines(*resource_bounds),
        "sources: max_plan_sample_taps=25 layer=material-blur-pass"));

    auto container_groups = material_container_group_summary_from_report(report);
    assert(container_groups);
    assert(container_groups->group_count == 3);
    assert(container_groups->multi_surface_group_count == 1);
    assert(container_groups->union_group_count == 1);
    assert(container_groups->morph_group_count == 2);
    assert(container_groups->shape_blend_execution_surface_count == 4);
    assert(container_groups->max_group_bounds_area == 566720.0);
    assert(container_groups->sources.size() == 2);
    assert(container_groups->sources[0].metric == "max_group_size");
    assert(container_groups->sources[0].container_id == 41);
    assert(container_groups->sources[0].plan_ids.size() == 2);
    assert(container_groups->sources[0].bounds_area == 566720.0);
    assert(contains_text(
        material_container_group_summary_json(container_groups),
        "\"morph_candidate_pair_count\":3"));
    assert(contains_text(
        material_container_group_summary_json(container_groups),
        "\"sources\":{\"max_group_size\":{\"metric\":\"max_group_size\""));
    assert(contains_text(
        material_container_group_summary_text(*container_groups),
        "groups=3 multi=1 union=1 morph=2"));
    assert(contains_text(
        material_container_group_summary_text(*container_groups),
        "gap=8..656 blend-distance=28"));
    assert(contains_text(
        material_container_group_summary_text(*container_groups),
        "sources: max_group_size=cid41 surfaces=4"));

    auto quality_policy = material_quality_policy_from_report(report);
    assert(quality_policy);
    assert(quality_policy->noise_disabled == 0);
    assert(quality_policy->max_blur_radius == 36.0);
    assert(quality_policy->sources.size() == 3);
    assert(quality_policy->sources[0].metric == "max_blur_radius");
    assert(quality_policy->sources[0].source_path
        == "debug.platform_runtime.details.renderer.material_plans[0]"
           ".quality_policy.max_blur_radius");
    assert(quality_policy->sources[2].metric == "max_backdrop_pixels");
    assert(contains_text(
        material_quality_policy_text(*quality_policy),
        "noise=0"));
    assert(contains_text(
        material_quality_policy_json(quality_policy),
        "\"sources\":{\"max_blur_radius\":{\"metric\":\"max_blur_radius\""));
    assert(contains_text(
        material_quality_policy_text(*quality_policy),
        "sources: max_blur_radius=36 layer=material-blur-pass"));

    auto executor_summary = material_budget_bound_summary_from_report(report);
    assert(executor_summary);
    assert(executor_summary->bound_count == 3);
    assert(executor_summary->fail_count == 1);
    assert(executor_summary->zero_margin_count == 1);
    assert(executor_summary->negative_margin_count == 1);
    assert(executor_summary->zero_margin_sources.size() == 1);
    assert(executor_summary->zero_margin_sources[0].key
        == "execution_stage_count_lte");
    assert(executor_summary->negative_margin_sources.size() == 1);
    assert(executor_summary->negative_margin_sources[0].key == "draw_calls_gte");
    assert(executor_summary->tightest_bound_result);
    assert(executor_summary->tightest_bound_result->key
        == "upload_utilization_lte");
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
    auto executor_summary_json =
        material_budget_bound_summary_json(executor_summary);
    assert(contains_text(
        executor_summary_json,
        "\"zero_margin_sources\":[{\"key\":\"execution_stage_count_lte\""));
    assert(contains_text(
        executor_summary_json,
        "\"negative_margin_sources\":[{\"key\":\"draw_calls_gte\""));
    assert(contains_text(
        executor_summary_json,
        "\"tightest_bound_result\":{\"key\":\"upload_utilization_lte\""));
    assert(contains_text(
        material_budget_bound_summary_text(*executor_summary),
        "zero=1 negative=1"));

    auto sourced_bound_json = json::parse(R"json({
      "key": "upload_utilization_lte",
      "field": "upload_utilization",
      "bound": "lte",
      "expected": 1,
      "actual": 0.0625,
      "ok": true,
      "margin": 0.9375,
      "source": {
        "metric": "upload_utilization",
        "value": 0.0625,
        "source_key": "upload_utilization",
        "source_path": "debug.platform_runtime.details.renderer.material_executor_summary.material_upload_bytes",
        "likely_layer": "platform-runtime",
        "likely_pass": "material-executor"
      }
    })json");
    auto sourced_bound =
        material_bound_result_from_object(sourced_bound_json.as_object());
    assert(sourced_bound.source_metric == "upload_utilization");
    assert(sourced_bound.source_key == "upload_utilization");
    assert(sourced_bound.source_likely_pass == "material-executor");
    auto sourced_bound_compact_json =
        material_budget_bound_result_json(sourced_bound);
    assert(contains_text(sourced_bound_compact_json, "\"source\":{"));
    assert(contains_text(
        sourced_bound_compact_json,
        "\"metric\":\"upload_utilization\""));
    assert(contains_text(
        material_budget_bound_result_text(sourced_bound),
        "source=upload_utilization path=debug.platform_runtime.details.renderer.material_executor_summary.material_upload_bytes pass=material-executor"));
    auto sourced_pressure_source =
        bound_pressure_source_from_result(sourced_bound);
    assert(sourced_pressure_source.source_metric == "upload_utilization");
    auto sourced_pressure_json =
        bound_pressure_source_json(sourced_pressure_source);
    assert(contains_text(sourced_pressure_json, "\"source\":{"));
    assert(contains_text(
        sourced_pressure_json,
        "\"source_path\":\"debug.platform_runtime.details.renderer.material_executor_summary.material_upload_bytes\""));
    assert(contains_text(
        bound_pressure_source_text(sourced_pressure_source),
        "source=upload_utilization path=debug.platform_runtime.details.renderer.material_executor_summary.material_upload_bytes pass=material-executor"));
    auto sourced_pressure_summary = MaterialBudgetBoundSummary{
        .bound_count = 1,
        .pass_count = 1,
        .fail_count = 0,
        .zero_margin_count = 1,
        .negative_margin_count = 0,
        .tightest_bound_key = "upload_utilization_lte",
        .tightest_bound_field = "upload_utilization",
        .tightest_bound_margin = 0.9375,
        .zero_margin_sources = {sourced_bound},
        .tightest_bound_result = sourced_bound,
    };
    auto no_bound_results =
        std::optional<std::vector<MaterialBudgetBoundResult>>{};
    auto sourced_pressure_summary_opt =
        std::optional<MaterialBudgetBoundSummary>{sourced_pressure_summary};
    assert(contains_text(
        bound_pressure_state(no_bound_results, sourced_pressure_summary_opt),
        "\"source\":{"));
    assert(contains_text(
        bound_pressure_text(
            "executor",
            no_bound_results,
            sourced_pressure_summary_opt),
        "source=upload_utilization path=debug.platform_runtime.details.renderer.material_executor_summary.material_upload_bytes pass=material-executor"));

    auto pressure_json = verifier_bound_pressure_json(report);
    auto tightest_json = verifier_tightest_bound_results_json(report);
    assert(contains_text(
        tightest_json,
        "\"executor\":{\"key\":\"upload_utilization_lte\",\"field\":\"upload_utilization\""));
    assert(contains_text(
        tightest_json,
        "\"resource\":{\"key\":\"max_plan_sample_taps_lte\",\"field\":\"max_plan_sample_taps\""));
    assert(contains_text(
        tightest_json,
        "\"quality\":{\"key\":\"require_noise_allowed\",\"field\":\"noise_disabled\""));
    assert(contains_text(
        verifier_tightest_bound_results_text(report),
        "executor=pass upload_utilization_lte actual=0.0625 expected<=1 margin=0.9375"));
    auto headroom_json = verifier_nearest_headroom_results_json(report);
    assert(contains_text(
        headroom_json,
        "\"executor\":{\"key\":\"upload_utilization_lte\",\"field\":\"upload_utilization\""));
    assert(contains_text(headroom_json, "\"resource\":null"));
    assert(contains_text(headroom_json, "\"quality\":null"));
    assert(contains_text(
        verifier_nearest_headroom_results_text(report),
        "executor=pass upload_utilization_lte actual=0.0625 expected<=1 margin=0.9375"));
    assert(contains_text(
        pressure_json,
        "\"executor\":{\"state\":\"fail\",\"bound_count\":3,\"pass_count\":2,\"fail_count\":1,\"zero_margin_count\":1,\"negative_margin_count\":1,\"zero_margin_sources\":[{\"key\":\"execution_stage_count_lte\",\"field\":\"execution_stage_count\",\"bound\":\"lte\",\"expected\":8,\"actual\":8,\"ok\":true,\"margin\":0}],\"negative_margin_sources\":[{\"key\":\"draw_calls_gte\",\"field\":\"draw_calls\",\"bound\":\"gte\",\"expected\":3,\"actual\":2,\"ok\":false,\"margin\":-1}],\"tightest_bound_key\":\"upload_utilization_lte\",\"tightest_bound_field\":\"upload_utilization\",\"tightest_bound_margin\":0.9375,\"tightest_bound_result\":{\"key\":\"upload_utilization_lte\",\"field\":\"upload_utilization\",\"bound\":\"lte\",\"expected\":1,\"actual\":0.0625,\"ok\":true,\"margin\":0.9375},\"failed_keys\":[\"draw_calls_gte\"]}"));
    assert(contains_text(
        pressure_json,
        "\"resource\":{\"state\":\"tight\",\"bound_count\":2,\"pass_count\":2,\"fail_count\":0,\"zero_margin_count\":2,\"negative_margin_count\":0,\"zero_margin_sources\":[{\"key\":\"max_plan_sample_taps_lte\",\"field\":\"max_plan_sample_taps\",\"bound\":\"lte\",\"expected\":25,\"actual\":25,\"ok\":true,\"margin\":0},{\"key\":\"require_bounded_texture_copy\",\"field\":\"unbounded_texture_copy\",\"bound\":\"equals\",\"expected\":0,\"actual\":0,\"ok\":true,\"margin\":0}],\"negative_margin_sources\":[],\"tightest_bound_key\":\"max_plan_sample_taps_lte\",\"tightest_bound_field\":\"max_plan_sample_taps\",\"tightest_bound_margin\":0,\"tightest_bound_result\":{\"key\":\"max_plan_sample_taps_lte\",\"field\":\"max_plan_sample_taps\",\"bound\":\"lte\",\"expected\":25,\"actual\":25,\"ok\":true,\"margin\":0},\"failed_keys\":[]}"));
    assert(contains_text(
        pressure_json,
        "\"quality\":{\"state\":\"tight\",\"bound_count\":2,\"pass_count\":2,\"fail_count\":0,\"zero_margin_count\":2,\"negative_margin_count\":0,\"zero_margin_sources\":[{\"key\":\"max_blur_radius_lte\",\"field\":\"max_blur_radius\",\"bound\":\"lte\",\"expected\":36,\"actual\":36,\"ok\":true,\"margin\":0},{\"key\":\"require_noise_allowed\",\"field\":\"noise_disabled\",\"bound\":\"equals\",\"expected\":0,\"actual\":0,\"ok\":true,\"margin\":0}],\"negative_margin_sources\":[],\"tightest_bound_key\":\"require_noise_allowed\",\"tightest_bound_field\":\"noise_disabled\",\"tightest_bound_margin\":0,\"tightest_bound_result\":{\"key\":\"require_noise_allowed\",\"field\":\"noise_disabled\",\"bound\":\"equals\",\"expected\":0,\"actual\":0,\"ok\":true,\"margin\":0},\"failed_keys\":[]}"));
    assert(
        verifier_bound_pressure_text(report)
        == "executor=fail,pass=2/3,fail=1,zero=1,negative=1,tightest=upload_utilization_lte,field=upload_utilization,tightest-margin=0.9375,tightest-result=(pass upload_utilization_lte/upload_utilization actual=0.0625 expected<=1 margin=0.9375),zero-sources=(pass execution_stage_count_lte/execution_stage_count actual=8 expected<=8 margin=0),negative-sources=(fail draw_calls_gte/draw_calls actual=2 expected>=3 margin=-1),failed=(draw_calls_gte); resource=tight,pass=2/2,fail=0,zero=2,negative=0,tightest=max_plan_sample_taps_lte,field=max_plan_sample_taps,tightest-margin=0,tightest-result=(pass max_plan_sample_taps_lte/max_plan_sample_taps actual=25 expected<=25 margin=0),zero-sources=(pass max_plan_sample_taps_lte/max_plan_sample_taps actual=25 expected<=25 margin=0, pass require_bounded_texture_copy/unbounded_texture_copy actual=0 expected==0 margin=0); quality=tight,pass=2/2,fail=0,zero=2,negative=0,tightest=require_noise_allowed,field=noise_disabled,tightest-margin=0,tightest-result=(pass require_noise_allowed/noise_disabled actual=0 expected==0 margin=0),zero-sources=(pass max_blur_radius_lte/max_blur_radius actual=36 expected<=36 margin=0, pass require_noise_allowed/noise_disabled actual=0 expected==0 margin=0)");

    auto resource_summary = material_resource_bound_summary_from_report(report);
    auto resource_results = material_resource_bound_results_from_report(report);
    assert(resource_summary);
    assert(resource_results);
    assert(resource_summary->bound_count == 2);
    assert(resource_summary->zero_margin_count == 2);
    assert(resource_summary->negative_margin_count == 0);
    assert(resource_summary->zero_margin_sources.size() == 2);
    assert(resource_summary->tightest_bound_result);
    assert(resource_summary->tightest_bound_result->key
        == "max_plan_sample_taps_lte");
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
    assert(quality_summary->zero_margin_count == 2);
    assert(quality_summary->tightest_bound_result);
    assert(quality_summary->tightest_bound_result->key
        == "require_noise_allowed");
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
        "\"artifact_context\":{\"platform\":\"test\",\"backend\":\"synthetic\""));
    assert(contains_text(
        failure_json,
        "\"backdrop_copy_skip_reason\":\"none\",\"sources\":{"));
    assert(contains_text(failure_json, "\"draw_calls\":{"));
    assert(contains_text(failure_json, "\"metric\":\"draw_calls\""));
    assert(contains_text(
        failure_json,
        "\"upload_utilization\":{"));
    assert(contains_text(failure_json, "\"metric\":\"upload_utilization\""));
    assert(contains_text(failure_json, "\"source_key\":\"upload_utilization\""));
    assert(contains_text(
        failure_json,
        "\"bound_summaries\":{\"executor\":{\"bound_count\":3,\"pass_count\":2,\"fail_count\":1,\"zero_margin_count\":1,\"negative_margin_count\":1"));
    assert(contains_text(
        failure_json,
        "\"zero_margin_sources\":[{\"key\":\"execution_stage_count_lte\",\"field\":\"execution_stage_count\""));
    assert(contains_text(
        failure_json,
        "\"negative_margin_sources\":[{\"key\":\"draw_calls_gte\",\"field\":\"draw_calls\""));
    assert(contains_text(
        failure_json,
        "\"tightest_bound_result\":{\"key\":\"upload_utilization_lte\",\"field\":\"upload_utilization\""));
    assert(contains_text(
        failure_json,
        "\"resource\":{\"bound_count\":2,\"pass_count\":2,\"fail_count\":0,\"zero_margin_count\":2,\"negative_margin_count\":0"));
    assert(contains_text(
        failure_json,
        "\"quality\":{\"bound_count\":2,\"pass_count\":2,\"fail_count\":0,\"zero_margin_count\":2,\"negative_margin_count\":0"));
    assert(contains_text(
        failure_json,
        "\"failed_bound_results\":{\"executor\":[{\"key\":\"draw_calls_gte\",\"field\":\"draw_calls\",\"bound\":\"gte\",\"expected\":3,\"actual\":2,\"ok\":false,\"margin\":-1}],\"resource\":[],\"quality\":[]}"));
    assert(contains_text(
        failure_json,
        "\"tightest_bound_results\":{\"executor\":{\"key\":\"upload_utilization_lte\",\"field\":\"upload_utilization\",\"bound\":\"lte\",\"expected\":1,\"actual\":0.0625,\"ok\":true,\"margin\":0.9375},\"resource\":{\"key\":\"max_plan_sample_taps_lte\",\"field\":\"max_plan_sample_taps\",\"bound\":\"lte\",\"expected\":25,\"actual\":25,\"ok\":true,\"margin\":0},\"quality\":{\"key\":\"require_noise_allowed\",\"field\":\"noise_disabled\",\"bound\":\"equals\",\"expected\":0,\"actual\":0,\"ok\":true,\"margin\":0}}"));
    assert(contains_text(
        failure_json,
        "\"nearest_headroom_results\":{\"executor\":{\"key\":\"upload_utilization_lte\",\"field\":\"upload_utilization\",\"bound\":\"lte\",\"expected\":1,\"actual\":0.0625,\"ok\":true,\"margin\":0.9375},\"resource\":null,\"quality\":null}"));
    assert(contains_text(
        failure_json,
        "\"bound_pressure\":{\"executor\":{\"state\":\"fail\",\"bound_count\":3,\"pass_count\":2,\"fail_count\":1,\"zero_margin_count\":1,\"negative_margin_count\":1,\"zero_margin_sources\":[{\"key\":\"execution_stage_count_lte\",\"field\":\"execution_stage_count\",\"bound\":\"lte\",\"expected\":8,\"actual\":8,\"ok\":true,\"margin\":0}],\"negative_margin_sources\":[{\"key\":\"draw_calls_gte\",\"field\":\"draw_calls\",\"bound\":\"gte\",\"expected\":3,\"actual\":2,\"ok\":false,\"margin\":-1}],\"tightest_bound_key\":\"upload_utilization_lte\",\"tightest_bound_field\":\"upload_utilization\",\"tightest_bound_margin\":0.9375,\"tightest_bound_result\":{\"key\":\"upload_utilization_lte\",\"field\":\"upload_utilization\",\"bound\":\"lte\",\"expected\":1,\"actual\":0.0625,\"ok\":true,\"margin\":0.9375},\"failed_keys\":[\"draw_calls_gte\"]},\"resource\":{\"state\":\"tight\",\"bound_count\":2,\"pass_count\":2,\"fail_count\":0,\"zero_margin_count\":2,\"negative_margin_count\":0,\"zero_margin_sources\":[{\"key\":\"max_plan_sample_taps_lte\",\"field\":\"max_plan_sample_taps\",\"bound\":\"lte\",\"expected\":25,\"actual\":25,\"ok\":true,\"margin\":0},{\"key\":\"require_bounded_texture_copy\",\"field\":\"unbounded_texture_copy\",\"bound\":\"equals\",\"expected\":0,\"actual\":0,\"ok\":true,\"margin\":0}],\"negative_margin_sources\":[],\"tightest_bound_key\":\"max_plan_sample_taps_lte\",\"tightest_bound_field\":\"max_plan_sample_taps\",\"tightest_bound_margin\":0,\"tightest_bound_result\":{\"key\":\"max_plan_sample_taps_lte\",\"field\":\"max_plan_sample_taps\",\"bound\":\"lte\",\"expected\":25,\"actual\":25,\"ok\":true,\"margin\":0},\"failed_keys\":[]},\"quality\":{\"state\":\"tight\",\"bound_count\":2,\"pass_count\":2,\"fail_count\":0,\"zero_margin_count\":2,\"negative_margin_count\":0,\"zero_margin_sources\":[{\"key\":\"max_blur_radius_lte\",\"field\":\"max_blur_radius\",\"bound\":\"lte\",\"expected\":36,\"actual\":36,\"ok\":true,\"margin\":0},{\"key\":\"require_noise_allowed\",\"field\":\"noise_disabled\",\"bound\":\"equals\",\"expected\":0,\"actual\":0,\"ok\":true,\"margin\":0}],\"negative_margin_sources\":[],\"tightest_bound_key\":\"require_noise_allowed\",\"tightest_bound_field\":\"noise_disabled\",\"tightest_bound_margin\":0,\"tightest_bound_result\":{\"key\":\"require_noise_allowed\",\"field\":\"noise_disabled\",\"bound\":\"equals\",\"expected\":0,\"actual\":0,\"ok\":true,\"margin\":0},\"failed_keys\":[]}}"));
    assert(contains_text(
        failure_json,
        "\"manifest_context\":{\"name\":\"unit-material-gate\""));
    assert(contains_text(
        failure_json,
        "\"material_resource_bound_fields\":[\"max_plan_sample_taps\",\"bounded_texture_copy\"]"));
    assert(contains_text(
        failure_json,
        "\"material_resource_bound_coverage_required_fields\":[\"bounded_texture_copy\",\"max_plan_sample_taps\"]"));
    assert(contains_text(
        failure_json,
        "\"material_resource_bound_coverage_min_guarded_field_count\":2"));
    assert(contains_text(
        failure_json,
        "\"material_quality_policy_coverage_required_fields\":[\"max_blur_radius\",\"noise_disabled\"]"));
    assert(contains_text(
        failure_json,
        "\"material_quality_policy_coverage_min_observed_field_count\":6"));
    assert(contains_text(
        failure_json,
        "\"budget_coverage\":{\"guardable_field_count\":22,\"observed_field_count\":22,\"guarded_observed_field_count\":3,\"unguarded_observed_field_count\":19,\"required_field_count\":2,\"covered_required_field_count\":2,\"missing_required_field_count\":0,\"manifest_field_count\":3,\"manifest_bound_key_count\":3"));
    assert(contains_text(
        failure_json,
        "\"resource_bound_coverage\":{\"guardable_field_count\":4,\"observed_field_count\":3,\"guarded_field_count\":2,\"required_field_count\":2,\"covered_required_field_count\":2,\"observed_required_field_count\":2,\"bound_key_count\":2"));
    assert(contains_text(
        failure_json,
        "\"quality_policy_bound_coverage\":{\"guardable_field_count\":6,\"observed_field_count\":6,\"guarded_field_count\":2,\"required_field_count\":2,\"covered_required_field_count\":2,\"observed_required_field_count\":2,\"bound_key_count\":2"));
    assert(contains_text(
        failure_json,
        "\"material_context\":{\"resource_bounds\":{\"max_plan_sample_taps\":25,\"total_plan_sample_taps\":50,\"max_backdrop_pixels\":4096"));
    assert(contains_text(
        failure_json,
        "\"sources\":{\"max_plan_sample_taps\":{\"metric\":\"max_plan_sample_taps\""));
    assert(contains_text(
        failure_json,
        "\"quality_policy\":{\"backdrop_sampling_disabled\":0,\"noise_disabled\":0,\"shadow_disabled\":0,\"max_blur_radius\":36,\"max_sample_taps\":25,\"max_backdrop_pixels\":4096,\"sources\":{\"max_blur_radius\":{\"metric\":\"max_blur_radius\""));
    assert(contains_text(
        failure_json,
        "\"container_groups\":{\"group_count\":3,\"multi_surface_group_count\":1,\"union_group_count\":1,\"morph_group_count\":2"));
    assert(contains_text(
        failure_json,
        "\"sources\":{\"max_group_size\":{\"metric\":\"max_group_size\""));
    assert(contains_text(
        failure_json,
        "\"by_likely_layer\":{\"material-control\":2,\"platform-runtime\":1}"));
    assert(contains_text(
        failure_json,
        "\"by_likely_pass\":{\"material-executor\":2,\"sampled-backdrop\":1}"));
    assert(contains_text(
        failure_json,
        "\"by_region\":{\"contentGlass\":1,\"toolbarGlass\":1}"));
    assert(contains_text(
        failure_json,
        "\"by_path\":{\"material.plans[2].semantic\":1,\"window.material.executor_budget.upload_utilization\":2,\"window.material.resource_bounds.unbounded_texture_copy\":1,\"window.pixel_regions.toolbarGlass\":1}"));
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
    assert(contains_text(
        failure_json,
        "\"missing_field_sources\":\"max_frame_capture_pixels=4096 pass=resource-budget"));
    assert(contains_text(
        failure_json,
        "\"quality_policy_bound_coverage\":{\"guardable_field_count\":6"));
    assert(contains_text(
        failure_json,
        "\"missing_field_sources\":\"max_frame_capture_pixels=4096 pass=resource-budget path=debug.platform_runtime.details.renderer.material_plans[1].resource_budget.max_frame_capture_pixels\""));
    assert(contains_text(
        failure_json,
        "\"missing_field_source_details\":{\"entries\":[{\"field\":\"max_frame_capture_pixels\""));
    assert(count_text(
        failure_json,
        "\"missing_field_source_details\":{\"entries\":[{\"field\":\"max_frame_capture_pixels\"")
        >= 2);
    assert(contains_text(
        failure_json,
        "\"text\":\"max_frame_capture_pixels=4096 pass=resource-budget path=debug.platform_runtime.details.renderer.material_plans[1].resource_budget.max_frame_capture_pixels\""));
    assert(contains_text(
        failure_json,
        "\"total_count\":1,\"shown_count\":1,\"omitted_count\":0,\"truncated\":false"));
    assert(contains_text(
        failure_json,
        "\"coverage_minimum_failures\":\"budget.min_guarded_field_count expected={>=: 2} actual=count=1 bound-keys=(draw_calls_gte) guarded=(draw_calls) unguarded=(planned_frame_capture_pixels) sources=(planned_frame_capture_pixels=0 pass=material-executor path=debug.platform_runtime.details.renderer.material_executor_summary.planned_frame_capture_pixels)\""));
    assert(contains_text(
        failure_json,
        "\"coverage_minimum_failure_details\":{\"entries\":[{\"label\":\"budget.min_guarded_field_count\""));
    assert(contains_text(
        failure_json,
        "\"actual\":{\"bound_keys\":[\"draw_calls_gte\"],\"count\":1,\"guarded_fields\":[\"draw_calls\"]"));
    assert(contains_text(
        failure_json,
        "\"unguarded_observed_fields\":[\"planned_frame_capture_pixels\"]"));
    assert(contains_text(
        failure_json,
        "\"actual_text\":\"count=1 bound-keys=(draw_calls_gte) guarded=(draw_calls) unguarded=(planned_frame_capture_pixels) sources=(planned_frame_capture_pixels=0 pass=material-executor path=debug.platform_runtime.details.renderer.material_executor_summary.planned_frame_capture_pixels)\""));
    assert(contains_text(
        failure_json,
        "\"total_count\":1,\"shown_count\":1,\"omitted_count\":0,\"truncated\":false"));
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
        "context: platform=test backend=synthetic semantic-nodes=3 renderer-plans=3 resolved-plans=3 executor=plans=3,sampled=2,fallback=0,draws=2,taps=50,upload=uploaded,draw=drawn,backdrop=bounded fallbacks=unsupported-backend=1 blockers=reduced-transparency=1, unsupported-backend=1 pass-executors=sampled-backdrop=2, color-fill=1 ref-policy=system-adaptive=2, standard-material=1 a11y=reduce-transparency=2, increase-contrast=1 perf=bounded-backdrop=2, deterministic-fallback=1 reduced-transparency=false increase-contrast=true reduce-motion=false probe=liquid-glass"));
    assert(contains_line(
        failure_lines,
        "bounds: executor=2/3,fail=1,tightest=upload_utilization_lte,zero=1,negative=1,failed=(draw_calls_gte); resource=2/2,fail=0,tightest=max_plan_sample_taps_lte,zero=2,negative=0; quality=2/2,fail=0,tightest=require_noise_allowed,zero=2,negative=0"));
    assert(contains_line(
        failure_lines,
        "failed-bounds: executor=fail draw_calls_gte actual=2 expected>=3 margin=-1"));
    assert(contains_line(
        failure_lines,
        "tightest-bounds: executor=pass upload_utilization_lte actual=0.0625 expected<=1 margin=0.9375; resource=pass max_plan_sample_taps_lte actual=25 expected<=25 margin=0; quality=pass require_noise_allowed actual=0 expected==0 margin=0"));
    assert(contains_line(
        failure_lines,
        "nearest-headroom: executor=pass upload_utilization_lte actual=0.0625 expected<=1 margin=0.9375"));
    assert(contains_line(
        failure_lines,
        "pressure: executor=fail,pass=2/3,fail=1,zero=1,negative=1,tightest=upload_utilization_lte,field=upload_utilization,tightest-margin=0.9375,tightest-result=(pass upload_utilization_lte/upload_utilization actual=0.0625 expected<=1 margin=0.9375),zero-sources=(pass execution_stage_count_lte/execution_stage_count actual=8 expected<=8 margin=0),negative-sources=(fail draw_calls_gte/draw_calls actual=2 expected>=3 margin=-1),failed=(draw_calls_gte); resource=tight,pass=2/2,fail=0,zero=2,negative=0,tightest=max_plan_sample_taps_lte,field=max_plan_sample_taps,tightest-margin=0,tightest-result=(pass max_plan_sample_taps_lte/max_plan_sample_taps actual=25 expected<=25 margin=0),zero-sources=(pass max_plan_sample_taps_lte/max_plan_sample_taps actual=25 expected<=25 margin=0, pass require_bounded_texture_copy/unbounded_texture_copy actual=0 expected==0 margin=0); quality=tight,pass=2/2,fail=0,zero=2,negative=0,tightest=require_noise_allowed,field=noise_disabled,tightest-margin=0,tightest-result=(pass require_noise_allowed/noise_disabled actual=0 expected==0 margin=0),zero-sources=(pass max_blur_radius_lte/max_blur_radius actual=36 expected<=36 margin=0, pass require_noise_allowed/noise_disabled actual=0 expected==0 margin=0)"));
    assert(contains_line(
        failure_lines,
        "manifest: name=unit-material-gate runtime-details=4 debug-details=3 runtime-bounds=5 pixel-regions=2 budget=3 resource=2 quality=2 required-budget=(draw_calls, execution_stage_count) required-resource=(bounded_texture_copy, max_plan_sample_taps) required-quality=(max_blur_radius, noise_disabled) runtime-detail-paths=(renderer.material_executor_summary.material_draw_status, renderer.material_executor_summary.material_sampled_backdrop_drawn, renderer.material_executor_summary.material_sampled_backdrop_uploaded, renderer.material_executor_summary.material_upload_status) debug-detail-paths=(application.file_explorer.chrome.geometry.titlebar_transparent, application.file_explorer.input.commands.duplicate, platform_capabilities.material_capability_profile) coverage-minimums=(budget=(keys=3 guarded=2 observed=21) resource=(keys=2 guarded=2 observed=2) quality=(keys=2 guarded=2 observed=6)) budget-keys=(execution_stage_count_lte, draw_calls_gte, upload_utilization_lte) resource-keys=(max_plan_sample_taps_lte, require_bounded_texture_copy) quality-keys=(max_blur_radius_lte, require_noise_allowed)"));
    assert(contains_line(
        failure_lines,
        "coverage: guarded=3/22 observed=22 guard-keys=3 unguarded=19 (active_execution_stage_count, backdrop_copy_count, backdrop_copy_pixels, backdrop_copy_skipped_count, backdrop_copy_utilization, backdrop_execution_stage_count, buffer_capacity_bytes, dropped_execution_stage_count, +11 more) guard-key-list=(draw_calls_gte, execution_stage_count_lte, upload_utilization_lte)"));
    assert(contains_line(
        failure_lines,
        "unguarded-sources=(max_backdrop_pixels=4096 pass=resource-budget"));
    assert(contains_line(
        failure_lines,
        "resource-coverage: guarded=2/4 observed=3 guard-keys=2 required=2/2 observed-required=2/2"));
    assert(contains_line(
        failure_lines,
        "quality-coverage: guarded=2/6 observed=6 guard-keys=2 required=2/2 observed-required=2/2"));
    assert(contains_line(
        failure_lines,
        "coverage-missing-sources: max_frame_capture_pixels=4096 pass=resource-budget path=debug.platform_runtime.details.renderer.material_plans[1].resource_budget.max_frame_capture_pixels"));
    assert(contains_line(
        failure_lines,
        "coverage-minimum-failures: budget.min_guarded_field_count expected={>=: 2} actual=count=1 bound-keys=(draw_calls_gte) guarded=(draw_calls) unguarded=(planned_frame_capture_pixels) sources=(planned_frame_capture_pixels=0 pass=material-executor path=debug.platform_runtime.details.renderer.material_executor_summary.planned_frame_capture_pixels)"));
    assert(contains_line(
        failure_lines,
        "budget-sources: draw_calls=2 layer=platform-runtime pass=material-executor key=material_draw_calls"));
    assert(contains_line(
        failure_lines,
        "container-groups: groups=3 multi=1 union=1 morph=2 interactive=1"));
    assert(contains_line(
        failure_lines,
        "sources: max_group_size=cid41 surfaces=4"));
    assert(contains_line(
        failure_lines,
        "resources: plan-taps=25/50 runtime=8/8/2 stages-dropped=0 paint=6/6/0 copy=1024/2048 unbounded-copy=0 non-deterministic-fallback=0 refraction=3.5 spacing=20 inflate=6"));
    assert(contains_line(
        failure_lines,
        "quality: disabled: backdrop-sampling=0 noise=0 shadow=0 limits: blur=36 sample-taps=25 backdrop-pixels=4096"));
    assert(contains_line(
        failure_lines,
        "by-layer: material-control=2, platform-runtime=1"));
    assert(contains_line(
        failure_lines,
        "by-pass: material-executor=2, sampled-backdrop=1"));
    assert(contains_line(
        failure_lines,
        "by-region: contentGlass=1, toolbarGlass=1"));
    assert(contains_line(
        failure_lines,
        "by-path: window.material.executor_budget.upload_utilization=2, material.plans[2].semantic=1, window.material.resource_bounds.unbounded_texture_copy=1, +1 more"));
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
        "missing-field-sources: max_frame_capture_pixels=4096 pass=resource-budget path=debug.platform_runtime.details.renderer.material_plans[1].resource_budget.max_frame_capture_pixels"));
    assert(contains_line(
        failure_lines,
        "... +1 more failures; rerun with --json for the full report"));

    auto minimum_report = json::parse(
        R"json({
          "failure_summary": {
            "count": 1,
            "top_likely_layer": "artifact-manifest",
            "top_likely_pass": "material-executor"
          },
          "failures": [
            {
              "name": "material executor budget coverage min_guarded_field_count is satisfied",
              "path": "manifest.require_material_executor_budget_coverage.min_guarded_field_count",
              "likely_layer": "artifact-manifest",
              "likely_pass": "material-executor",
              "expected": {
                ">=": 2
              },
              "actual": {
                "count": 1,
                "bound_keys": [
                  "draw_calls_gte"
                ],
                "guarded_fields": [
                  "draw_calls"
                ],
                "unguarded_observed_fields": [
                  "planned_frame_capture_pixels"
                ],
                "unguarded_observed_sources": {
                  "planned_frame_capture_pixels": {
                    "metric": "planned_frame_capture_pixels",
                    "value": 0,
                    "source_path": "debug.platform_runtime.details.renderer.material_executor_summary.planned_frame_capture_pixels",
                    "likely_pass": "material-executor"
                  }
                }
              }
            }
          ]
        })json");
    auto minimum_json = verifier_failure_summary_json(minimum_report);
    assert(contains_text(
        minimum_json,
        "\"actual\":\"count=1 bound-keys=(draw_calls_gte) guarded=(draw_calls) unguarded=(planned_frame_capture_pixels) sources=(planned_frame_capture_pixels=0 pass=material-executor path=debug.platform_runtime.details.renderer.material_executor_summary.planned_frame_capture_pixels)\""));
    assert(contains_text(
        minimum_json,
        "\"coverage_minimum_failure_details\":{\"entries\":[{\"label\":\"budget.min_guarded_field_count\""));
    assert(contains_text(
        minimum_json,
        "\"expected\":{\">=\":2},\"actual\":{\"bound_keys\":[\"draw_calls_gte\"],\"count\":1,\"guarded_fields\":[\"draw_calls\"]"));
    assert(contains_text(
        minimum_json,
        "\"total_count\":1,\"shown_count\":1,\"omitted_count\":0,\"truncated\":false"));
    auto minimum_lines = verifier_failure_summary_lines(minimum_report);
    assert(contains_line(
        minimum_lines,
        "actual=count=1 bound-keys=(draw_calls_gte) guarded=(draw_calls) unguarded=(planned_frame_capture_pixels) sources=(planned_frame_capture_pixels=0 pass=material-executor path=debug.platform_runtime.details.renderer.material_executor_summary.planned_frame_capture_pixels)"));

    auto many_minimum_report = json::parse(
        R"json({
          "failure_summary": {
            "count": 6,
            "top_likely_layer": "artifact-manifest",
            "top_likely_pass": "material-executor"
          },
          "failures": [
            {
              "path": "manifest.require_material_executor_budget_coverage.min_bound_key_count",
              "expected": {">=": 2},
              "actual": {"count": 1, "bound_keys": ["draw_calls_gte"]}
            },
            {
              "path": "manifest.require_material_executor_budget_coverage.min_guarded_field_count",
              "expected": {">=": 2},
              "actual": {"count": 1, "guarded_fields": ["draw_calls"]}
            },
            {
              "path": "manifest.require_material_executor_budget_coverage.min_observed_field_count",
              "expected": {">=": 2},
              "actual": {"count": 1, "observed_fields": ["draw_calls"]}
            },
            {
              "path": "manifest.require_material_resource_bound_coverage.min_bound_key_count",
              "expected": {">=": 2},
              "actual": {"count": 1, "bound_keys": ["max_plan_sample_taps_lte"]}
            },
            {
              "path": "manifest.require_material_resource_bound_coverage.min_guarded_field_count",
              "expected": {">=": 2},
              "actual": {"count": 1, "guarded_fields": ["max_plan_sample_taps"]}
            },
            {
              "path": "manifest.require_material_quality_policy_coverage.min_bound_key_count",
              "expected": {">=": 2},
              "actual": {"count": 1, "bound_keys": ["max_blur_radius_lte"]}
            }
          ]
        })json");
    auto many_minimum_json =
        verifier_failure_summary_json(many_minimum_report);
    assert(contains_text(
        many_minimum_json,
        "\"total_count\":6,\"shown_count\":5,\"omitted_count\":1,\"truncated\":true"));
    assert(!contains_text(
        many_minimum_json,
        "\"label\":\"quality.min_bound_key_count\""));

    auto many_missing_report = json::parse(
        R"json({
          "failure_summary": {
            "count": 1,
            "top_likely_layer": "artifact-manifest",
            "top_likely_pass": "resource-budget"
          },
          "failures": [
            {
              "path": "manifest.require_material_resource_bound_coverage.required_fields",
              "expected": ["a", "b", "c", "d", "e", "f"],
              "actual": {
                "missing_fields": ["a", "b", "c", "d", "e", "f"],
                "missing_field_sources": {
                  "a": {"metric": "a", "value": 1, "source_path": "debug.a", "likely_pass": "resource-budget"},
                  "b": {"metric": "b", "value": 2, "source_path": "debug.b", "likely_pass": "resource-budget"},
                  "c": {"metric": "c", "value": 3, "source_path": "debug.c", "likely_pass": "resource-budget"},
                  "d": {"metric": "d", "value": 4, "source_path": "debug.d", "likely_pass": "resource-budget"},
                  "e": {"metric": "e", "value": 5, "source_path": "debug.e", "likely_pass": "resource-budget"},
                  "f": {"metric": "f", "value": 6, "source_path": "debug.f", "likely_pass": "resource-budget"}
                }
              }
            }
          ]
        })json");
    auto many_missing_json = verifier_failure_summary_json(many_missing_report);
    assert(contains_text(
        many_missing_json,
        "\"missing_field_source_details\":{\"entries\":[{\"field\":\"a\""));
    assert(contains_text(
        many_missing_json,
        "\"text\":\"a=1 pass=resource-budget path=debug.a\""));
    assert(contains_text(
        many_missing_json,
        "\"total_count\":6,\"shown_count\":5,\"omitted_count\":1,\"truncated\":true"));
    assert(contains_text(
        many_missing_json,
        "\"total_count\":6,\"shown_count\":3,\"omitted_count\":3,\"truncated\":true"));
    assert(!contains_text(many_missing_json, "\"field\":\"f\""));

    auto passing_report = json::parse(
        R"json({"failure_summary":{"count":0},"failures":[]})json");
    assert(verifier_tightest_bound_results_json(passing_report) == "null");
    assert(verifier_tightest_bound_results_text(passing_report).empty());
    assert(verifier_nearest_headroom_results_json(passing_report) == "null");
    assert(verifier_nearest_headroom_results_text(passing_report).empty());
    assert(verifier_bound_pressure_json(passing_report) == "null");
    assert(verifier_bound_pressure_text(passing_report).empty());
    assert(verifier_failure_summary_json(passing_report) == "null");
    assert(verifier_failure_summary_lines(passing_report).empty());
}

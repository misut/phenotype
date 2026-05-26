module;
#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>

export module phenotype.material.executor_summary;

import phenotype.material.plan;
import phenotype.material.types;
import phenotype.types;

export namespace phenotype {

struct MaterialExecutorSummary {
    std::uint32_t plan_count = 0;
    std::uint32_t material_instance_count = 0;
    std::uint32_t fallback_instance_count = 0;
    std::uint32_t sampled_backdrop_instance_count = 0;
    std::uint32_t standard_fill_instance_count = 0;
    std::uint32_t deterministic_fallback_instance_count = 0;
    std::uint32_t material_draw_calls = 0;
    std::uint32_t backdrop_copy_count = 0;
    std::uint32_t backdrop_copy_skipped_count = 0;
    std::uint32_t execution_stage_count = 0;
    std::uint32_t active_execution_stage_count = 0;
    std::uint32_t backdrop_execution_stage_count = 0;
    std::uint32_t primary_execution_stage_count = 0;
    std::uint32_t dropped_execution_stage_count = 0;
    std::uint32_t paint_layer_count = 0;
    std::uint32_t active_paint_layer_count = 0;
    std::uint32_t dropped_paint_layer_count = 0;
    std::uint32_t shadow_paint_layer_count = 0;
    std::uint32_t fill_paint_layer_count = 0;
    std::uint32_t edge_paint_layer_count = 0;
    float max_paint_layer_inflate = 0.0f;
    std::uint32_t backdrop_filter_stage_count = 0;
    std::uint32_t fallback_fill_stage_count = 0;
    std::uint32_t shadow_stage_count = 0;
    std::uint32_t edge_highlight_stage_count = 0;
    std::uint32_t noise_dither_stage_count = 0;
    std::uint32_t backdrop_access_plan_count = 0;
    std::uint32_t next_frame_capture_plan_count = 0;
    std::uint32_t planned_frame_capture_count = 0;
    std::int64_t planned_frame_capture_pixels = 0;
    std::int64_t planned_surface_sample_pixels = 0;
    std::uint32_t material_max_sample_taps = 0;
    std::int64_t material_total_sample_taps = 0;
    std::int64_t backdrop_copy_pixels = 0;
    bool backdrop_copy_excludes_foreground_text = false;
    bool foreground_pass_after_backdrop_copy = false;
    std::int64_t material_upload_bytes = 0;
    std::int64_t material_buffer_capacity_bytes = 0;
    std::uint32_t material_buffer_reallocations = 0;
    std::uint32_t execution_contract_satisfied_count = 0;
    std::uint32_t execution_contract_mismatch_count = 0;
    std::uint32_t execution_contract_mismatch_total = 0;
    std::uint32_t stage_order_match_count = 0;
    std::uint32_t stage_order_mismatch_count = 0;
    char const* first_execution_contract_mismatch = "none";
    char const* first_stage_order_mismatch = "none";
    std::uint32_t foreground_text_candidate_count = 0;
    std::uint32_t foreground_text_remap_count = 0;
    std::uint32_t interaction_enabled_count = 0;
    std::uint32_t interaction_active_count = 0;
    std::uint32_t interaction_specular_highlight_count = 0;
    std::uint32_t interaction_pointer_lens_count = 0;
    std::uint32_t refraction_active_count = 0;
    float max_interaction_response_strength = 0.0f;
    float max_interaction_specular_radius = 0.0f;
    float max_interaction_specular_intensity = 0.0f;
    float max_interaction_pointer_lens_radius = 0.0f;
    float max_interaction_pointer_lens_strength = 0.0f;
    float max_refraction_strength = 0.0f;
    float max_refraction_edge_bias = 0.0f;
    float max_refraction_offset_pixels = 0.0f;
    bool backdrop_descriptor_color_available = false;
    Color backdrop_descriptor_color_mean = {255, 255, 255, 255};
    std::uint32_t backdrop_descriptor_color_sample_count = 0;
    char const* backdrop_descriptor_color_status = "not-sampled";
    bool backdrop_descriptor_luma_available = false;
    float backdrop_descriptor_luma_min = 0.0f;
    float backdrop_descriptor_luma_max = 1.0f;
    float backdrop_descriptor_luma_mean = 0.5f;
    std::uint32_t backdrop_descriptor_luma_sample_count = 0;
    std::uint32_t backdrop_descriptor_luma_grid_width = 0;
    std::uint32_t backdrop_descriptor_luma_grid_height = 0;
    std::uint64_t backdrop_descriptor_luma_frame = 0;
    char const* backdrop_descriptor_luma_status = "not-sampled";
    char const* backdrop_descriptor_source = "none";
    bool material_pipeline_ready = false;
    bool material_backdrop_source_ready = false;
    bool material_sampled_backdrop_upload_required = false;
    bool material_sampled_backdrop_draw_required = false;
    bool material_sampled_backdrop_uploaded = false;
    bool material_sampled_backdrop_drawn = false;
    char const* material_upload_status = "not-needed";
    char const* material_draw_status = "not-needed";
    std::uint32_t backdrop_luma_sampling_skipped_count = 0;
    char const* backdrop_luma_sampling_skip_reason = "none";
    char const* backdrop_copy_skip_reason = "none";
    MaterialContainerGroupRuntimeSummary container_groups{};
    float material_shader_content_scale = 1.0f;
    float material_max_shader_blur_step_pixels = 0.0f;
    std::int64_t cpu_decode_ns = 0;
    std::int64_t cpu_material_upload_ns = 0;
    std::int64_t cpu_total_ns = 0;
};

inline char const* material_backdrop_copy_policy() noexcept {
    return "copy_only_when_material_plan_requires_shared_or_next_frame_capture";
}

inline bool material_executor_requires_frame_capture(
        MaterialExecutorSummary const& summary) noexcept {
    return summary.planned_frame_capture_count > 0
        && summary.planned_frame_capture_pixels > 0;
}

inline char const* material_executor_sampled_upload_status(
        MaterialExecutorSummary const& summary) noexcept {
    if (summary.sampled_backdrop_instance_count == 0u)
        return "not-needed";
    if (summary.material_upload_bytes > 0)
        return "uploaded";
    if (!summary.material_pipeline_ready)
        return "skipped-material-pipeline-unavailable";
    return "skipped-material-buffer-upload-not-recorded";
}

inline char const* material_executor_sampled_draw_status(
        MaterialExecutorSummary const& summary) noexcept {
    if (summary.sampled_backdrop_instance_count == 0u)
        return "not-needed";
    if (summary.material_draw_calls > 0u)
        return "drawn";
    if (!summary.material_pipeline_ready)
        return "skipped-material-pipeline-unavailable";
    if (!summary.material_backdrop_source_ready)
        return "skipped-material-backdrop-source-unavailable";
    if (!summary.material_sampled_backdrop_uploaded)
        return "skipped-material-buffer-upload-not-recorded";
    return "skipped-material-draw-not-recorded";
}

inline void finalize_material_executor_execution_status(
        MaterialExecutorSummary& summary) noexcept {
    summary.material_sampled_backdrop_upload_required =
        summary.sampled_backdrop_instance_count > 0u;
    summary.material_sampled_backdrop_draw_required =
        summary.sampled_backdrop_instance_count > 0u;
    summary.material_sampled_backdrop_uploaded =
        summary.material_upload_bytes > 0;
    summary.material_sampled_backdrop_drawn =
        summary.material_draw_calls > 0u;
    summary.material_upload_status =
        material_executor_sampled_upload_status(summary);
    summary.material_draw_status =
        material_executor_sampled_draw_status(summary);
}

inline void set_material_executor_backdrop_descriptor_summary(
        MaterialExecutorSummary& summary,
        MaterialBackdropDescriptor const& backdrop) noexcept {
    summary.backdrop_descriptor_color_available =
        backdrop.available && backdrop.color_sample_count > 0;
    summary.backdrop_descriptor_color_mean = backdrop.color_mean;
    summary.backdrop_descriptor_color_sample_count =
        backdrop.color_sample_count;
    summary.backdrop_descriptor_color_status =
        backdrop.color_sample_status && backdrop.color_sample_status[0]
            ? backdrop.color_sample_status
            : "not-sampled";
    summary.backdrop_descriptor_luma_available =
        backdrop.available && backdrop.luma_sample_count > 0;
    summary.backdrop_descriptor_luma_min =
        material_safe_luma(backdrop.luma_min, 0.0f);
    summary.backdrop_descriptor_luma_max =
        std::max(summary.backdrop_descriptor_luma_min,
                 material_safe_luma(backdrop.luma_max, 1.0f));
    summary.backdrop_descriptor_luma_mean = std::clamp(
        material_safe_luma(backdrop.luma_mean, 0.5f),
        summary.backdrop_descriptor_luma_min,
        summary.backdrop_descriptor_luma_max);
    summary.backdrop_descriptor_luma_sample_count =
        backdrop.luma_sample_count;
    summary.backdrop_descriptor_luma_grid_width =
        backdrop.luma_sample_grid_width;
    summary.backdrop_descriptor_luma_grid_height =
        backdrop.luma_sample_grid_height;
    summary.backdrop_descriptor_luma_frame = backdrop.luma_sample_frame;
    summary.backdrop_descriptor_luma_status =
        backdrop.luma_sample_status && backdrop.luma_sample_status[0]
            ? backdrop.luma_sample_status
            : "not-sampled";
    summary.backdrop_descriptor_source =
        backdrop.source && backdrop.source[0] ? backdrop.source : "none";
}

inline void accumulate_material_executor_plan_summary(
        MaterialExecutorSummary& summary,
        MaterialPlan const& plan) noexcept {
    ++summary.plan_count;
    if (plan.fallback()) {
        ++summary.fallback_instance_count;
    } else {
        ++summary.material_instance_count;
        summary.material_max_sample_taps = std::max(
            summary.material_max_sample_taps,
            plan.sample_taps);
        summary.material_total_sample_taps += plan.sample_taps;
    }
    if (material_plan_uses_sampled_backdrop_executor(plan))
        ++summary.sampled_backdrop_instance_count;
    if (material_plan_uses_standard_fill_executor(plan))
        ++summary.standard_fill_instance_count;
    if (material_plan_uses_deterministic_fallback_executor(plan))
        ++summary.deterministic_fallback_instance_count;
    for (unsigned int i = 0; i < plan.execution_stage_count; ++i) {
        auto const& stage = plan.execution_stages[i];
        ++summary.execution_stage_count;
        if (stage.active)
            ++summary.active_execution_stage_count;
        if (stage.requires_backdrop)
            ++summary.backdrop_execution_stage_count;
        if (plan.primary_pass.active) {
            auto const primary_stage =
                material_stage_matches(stage.name, plan.primary_pass.name)
                && stage.requires_backdrop == plan.primary_pass.requires_backdrop
                && stage.sample_taps == plan.primary_pass.sample_taps
                && material_stage_matches(stage.likely_layer,
                                          plan.primary_pass.likely_layer)
                && material_stage_matches(stage.executor,
                                          plan.primary_pass.executor)
                && stage.max_texture_copy_pixels
                    == plan.primary_pass.max_texture_copy_pixels;
            if (primary_stage)
                ++summary.primary_execution_stage_count;
        }
        if (material_stage_matches(stage.executor, "backdrop-filter"))
            ++summary.backdrop_filter_stage_count;
        if (material_stage_matches(stage.executor, "fallback-fill"))
            ++summary.fallback_fill_stage_count;
        if (material_stage_matches(stage.name, "shape-shadow"))
            ++summary.shadow_stage_count;
        if (material_stage_matches(stage.name, "edge-highlight"))
            ++summary.edge_highlight_stage_count;
        if (material_stage_matches(stage.name, "noise-dither"))
            ++summary.noise_dither_stage_count;
    }
    summary.dropped_execution_stage_count +=
        plan.dropped_execution_stage_count;
    summary.paint_layer_count += plan.paint_layer_count;
    summary.dropped_paint_layer_count += plan.dropped_paint_layer_count;
    for (unsigned int i = 0; i < plan.paint_layer_count; ++i) {
        auto const& layer = plan.paint_layers[i];
        if (layer.active)
            ++summary.active_paint_layer_count;
        if (material_paint_layer_matches(layer.executor, "rounded-shadow"))
            ++summary.shadow_paint_layer_count;
        if (material_paint_layer_matches(layer.executor, "rounded-fill"))
            ++summary.fill_paint_layer_count;
        if (material_paint_layer_matches(layer.executor, "rounded-edge"))
            ++summary.edge_paint_layer_count;
        summary.max_paint_layer_inflate = std::max(
            summary.max_paint_layer_inflate,
            layer.inflate);
    }
    if (plan.backdrop_access.required)
        ++summary.backdrop_access_plan_count;
    if (plan.backdrop_access.next_frame_capture_required)
        ++summary.next_frame_capture_plan_count;
    if (plan.interaction.enabled)
        ++summary.interaction_enabled_count;
    if (plan.interaction.active)
        ++summary.interaction_active_count;
    if (plan.interaction.specular_highlight_active)
        ++summary.interaction_specular_highlight_count;
    if (plan.interaction.pointer_lens_active)
        ++summary.interaction_pointer_lens_count;
    if (plan.refraction.active)
        ++summary.refraction_active_count;
    summary.max_interaction_response_strength = std::max(
        summary.max_interaction_response_strength,
        plan.interaction.response_strength);
    summary.max_interaction_specular_radius = std::max(
        summary.max_interaction_specular_radius,
        plan.interaction.specular_radius);
    summary.max_interaction_specular_intensity = std::max(
        summary.max_interaction_specular_intensity,
        plan.interaction.specular_intensity);
    summary.max_interaction_pointer_lens_radius = std::max(
        summary.max_interaction_pointer_lens_radius,
        plan.interaction.pointer_lens_radius);
    summary.max_interaction_pointer_lens_strength = std::max(
        summary.max_interaction_pointer_lens_strength,
        plan.interaction.pointer_lens_strength);
    summary.max_refraction_strength = std::max(
        summary.max_refraction_strength,
        plan.refraction.strength);
    summary.max_refraction_edge_bias = std::max(
        summary.max_refraction_edge_bias,
        plan.refraction.edge_bias);
    summary.max_refraction_offset_pixels = std::max(
        summary.max_refraction_offset_pixels,
        plan.refraction.max_offset_pixels);
    if (plan.backdrop_access.shared_frame_capture
        || plan.backdrop_access.next_frame_capture_required) {
        summary.planned_frame_capture_count = std::max(
            summary.planned_frame_capture_count,
            plan.backdrop_access.max_frame_capture_count);
        summary.planned_frame_capture_pixels = std::max(
            summary.planned_frame_capture_pixels,
            plan.backdrop_access.max_frame_capture_pixels);
    }
    summary.planned_surface_sample_pixels +=
        plan.backdrop_access.max_surface_sample_pixels;
    if (plan.execution_audit.contract_satisfied) {
        ++summary.execution_contract_satisfied_count;
    } else {
        ++summary.execution_contract_mismatch_count;
        if (std::string_view{summary.first_execution_contract_mismatch}
                == "none") {
            summary.first_execution_contract_mismatch =
                plan.execution_audit.first_mismatch;
        }
    }
    summary.execution_contract_mismatch_total +=
        plan.execution_audit.mismatch_count;
    if (plan.execution_audit.stage_order_match) {
        ++summary.stage_order_match_count;
    } else {
        ++summary.stage_order_mismatch_count;
        if (std::string_view{summary.first_stage_order_mismatch} == "none") {
            summary.first_stage_order_mismatch =
                plan.execution_audit.actual_stage_order;
        }
    }
}

inline void finalize_material_executor_summary(
        MaterialExecutorSummary& summary,
        std::span<MaterialRuntimeRecord const> records) {
    summary.container_groups = summarize_material_container_groups(records);
    auto const surface_samples =
        summarize_material_execution_surface_samples(records);
    summary.planned_surface_sample_pixels =
        surface_samples.total_surface_sample_pixels;
    finalize_material_executor_execution_status(summary);
}

} // namespace phenotype

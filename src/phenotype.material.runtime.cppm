module;
#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>

export module phenotype.material.runtime;

import phenotype.material.plan;
import phenotype.material.types;

export namespace phenotype {

struct MaterialRuntimeSummary {
    std::uint32_t plan_count = 0;
    std::uint32_t fallback_count = 0;
    std::uint32_t backdrop_sampling_count = 0;
    std::uint32_t total_runtime_passes = 0;
    std::uint32_t active_runtime_passes = 0;
    std::uint32_t backdrop_runtime_passes = 0;
    std::uint32_t total_execution_stages = 0;
    std::uint32_t active_execution_stages = 0;
    std::uint32_t backdrop_execution_stages = 0;
    std::uint32_t dropped_execution_stages = 0;
    unsigned int max_execution_stage_count = 0;
    unsigned int max_execution_stages = 0;
    unsigned int max_execution_stage_capacity = 0;
    std::uint32_t total_paint_layers = 0;
    std::uint32_t active_paint_layers = 0;
    std::uint32_t dropped_paint_layers = 0;
    std::uint32_t shadow_paint_layers = 0;
    std::uint32_t fill_paint_layers = 0;
    std::uint32_t edge_paint_layers = 0;
    unsigned int max_paint_layer_count = 0;
    unsigned int max_paint_layers = 0;
    unsigned int max_paint_layer_capacity = 0;
    float max_paint_layer_inflate = 0.0f;
    std::int64_t max_pass_texture_copy_pixels = 0;
    std::int64_t total_pass_texture_copy_pixels = 0;
    std::uint32_t backdrop_access_count = 0;
    std::uint32_t shared_frame_capture_plan_count = 0;
    std::uint32_t next_frame_capture_plan_count = 0;
    std::uint32_t max_frame_capture_count = 0;
    std::int64_t max_frame_capture_pixels = 0;
    std::int64_t total_surface_sample_pixels = 0;
    std::int64_t max_surface_sample_pixels = 0;
    float max_plan_blur_radius = 0.0f;
    unsigned int max_plan_sample_taps = 0;
    std::int64_t total_plan_sample_taps = 0;
    float max_budget_blur_radius = 0.0f;
    unsigned int max_sample_taps = 0;
    unsigned int max_sampling_kernel_radius = 0;
    unsigned int max_pass_count = 0;
    std::int64_t max_backdrop_pixels = 0;
    float max_container_spacing = 0.0f;
    std::uint32_t containered_count = 0;
    std::uint32_t unioned_count = 0;
    std::uint32_t interactive_count = 0;
    std::uint32_t morph_transition_count = 0;
    std::uint32_t valid_shape_count = 0;
    std::uint32_t rounded_shape_count = 0;
    std::uint32_t capsule_shape_count = 0;
    std::uint32_t radius_clamped_count = 0;
    std::uint32_t foreground_backdrop_driven_count = 0;
    std::uint32_t foreground_high_contrast_count = 0;
    std::uint32_t foreground_vibrant_count = 0;
    std::uint32_t interaction_enabled_count = 0;
    std::uint32_t interaction_active_count = 0;
    std::uint32_t interaction_reduce_motion_count = 0;
    std::uint32_t interaction_specular_highlight_count = 0;
    std::uint32_t interaction_pointer_lens_count = 0;
    std::uint32_t refraction_active_count = 0;
    std::uint32_t theme_default_glass_token_count = 0;
    std::uint32_t theme_custom_token_count = 0;
    MaterialContainerGroupRuntimeSummary container_groups{};
    float max_surface_area = 0.0f;
    float max_effective_radius = 0.0f;
    float max_radius_limit = 0.0f;
    float max_normalized_radius = 0.0f;
    float max_saturation = 0.0f;
    float max_edge_highlight = 0.0f;
    float max_edge_width = 0.0f;
    float max_noise_opacity = 0.0f;
    float max_shadow_alpha = 0.0f;
    float max_shadow_radius = 0.0f;
    float max_interaction_response_strength = 0.0f;
    float max_interaction_specular_radius = 0.0f;
    float max_interaction_specular_intensity = 0.0f;
    float max_interaction_pointer_lens_radius = 0.0f;
    float max_interaction_pointer_lens_strength = 0.0f;
    float max_refraction_strength = 0.0f;
    float max_refraction_edge_bias = 0.0f;
    float max_refraction_offset_pixels = 0.0f;
    float min_foreground_contrast_ratio = 0.0f;
    std::uint32_t unbounded_texture_copy = 0;
    std::uint32_t non_deterministic_fallback = 0;
    std::uint32_t execution_contract_satisfied_count = 0;
    std::uint32_t execution_contract_mismatch_count = 0;
    std::uint32_t execution_contract_mismatch_total = 0;
    std::uint32_t stage_order_match_count = 0;
    std::uint32_t stage_order_mismatch_count = 0;
    char const* first_execution_contract_mismatch = "none";
    char const* first_stage_order_mismatch = "none";
};

inline void accumulate_material_runtime_summary(
        MaterialRuntimeSummary& summary,
        MaterialRuntimeRecord const& record) noexcept {
    auto const& plan = record.plan;
    ++summary.plan_count;
    if (plan.fallback())
        ++summary.fallback_count;
    if (plan.backdrop_sampling)
        ++summary.backdrop_sampling_count;

    ++summary.total_runtime_passes;
    if (plan.primary_pass.active)
        ++summary.active_runtime_passes;
    if (plan.primary_pass.requires_backdrop)
        ++summary.backdrop_runtime_passes;
    summary.total_execution_stages += plan.execution_stage_count;
    summary.dropped_execution_stages +=
        plan.dropped_execution_stage_count;
    summary.total_paint_layers += plan.paint_layer_count;
    summary.dropped_paint_layers += plan.dropped_paint_layer_count;
    summary.max_execution_stage_count =
        std::max(summary.max_execution_stage_count, plan.execution_stage_count);
    summary.max_execution_stages =
        std::max(summary.max_execution_stages,
                 plan.resource_budget.max_execution_stages);
    summary.max_paint_layers =
        std::max(summary.max_paint_layers,
                 plan.resource_budget.max_paint_layers);
    summary.max_execution_stage_capacity =
        std::max(summary.max_execution_stage_capacity,
                 plan.execution_stage_capacity);
    summary.max_paint_layer_count =
        std::max(summary.max_paint_layer_count, plan.paint_layer_count);
    summary.max_paint_layer_capacity =
        std::max(summary.max_paint_layer_capacity, plan.paint_layer_capacity);
    summary.max_paint_layer_inflate = std::max(
        summary.max_paint_layer_inflate,
        plan.resource_budget.max_paint_layer_inflate);
    for (unsigned int i = 0; i < plan.execution_stage_count; ++i) {
        auto const& stage = plan.execution_stages[i];
        if (stage.active)
            ++summary.active_execution_stages;
        if (stage.requires_backdrop)
            ++summary.backdrop_execution_stages;
    }
    for (unsigned int i = 0; i < plan.paint_layer_count; ++i) {
        auto const& layer = plan.paint_layers[i];
        if (layer.active)
            ++summary.active_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-shadow"))
            ++summary.shadow_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-fill"))
            ++summary.fill_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-edge"))
            ++summary.edge_paint_layers;
    }
    summary.max_pass_texture_copy_pixels = std::max(
        summary.max_pass_texture_copy_pixels,
        plan.primary_pass.max_texture_copy_pixels);
    summary.total_pass_texture_copy_pixels +=
        plan.primary_pass.max_texture_copy_pixels;
    if (plan.backdrop_access.required)
        ++summary.backdrop_access_count;
    if (plan.backdrop_access.shared_frame_capture)
        ++summary.shared_frame_capture_plan_count;
    if (plan.backdrop_access.next_frame_capture_required)
        ++summary.next_frame_capture_plan_count;
    summary.max_frame_capture_count = std::max(
        summary.max_frame_capture_count,
        plan.backdrop_access.max_frame_capture_count);
    summary.max_frame_capture_pixels = std::max(
        summary.max_frame_capture_pixels,
        plan.backdrop_access.max_frame_capture_pixels);
    summary.total_surface_sample_pixels +=
        plan.backdrop_access.max_surface_sample_pixels;
    summary.max_surface_sample_pixels = std::max(
        summary.max_surface_sample_pixels,
        plan.backdrop_access.max_surface_sample_pixels);

    summary.max_plan_blur_radius =
        std::max(summary.max_plan_blur_radius, plan.blur_radius);
    summary.max_plan_sample_taps =
        std::max(summary.max_plan_sample_taps, plan.sample_taps);
    summary.total_plan_sample_taps += plan.sample_taps;
    summary.max_budget_blur_radius = std::max(
        summary.max_budget_blur_radius,
        plan.resource_budget.max_blur_radius);
    summary.max_sample_taps = std::max(
        summary.max_sample_taps,
        plan.resource_budget.max_sample_taps);
    summary.max_sampling_kernel_radius = std::max(
        summary.max_sampling_kernel_radius,
        plan.resource_budget.max_sampling_kernel_radius);
    summary.max_pass_count =
        std::max(summary.max_pass_count, plan.resource_budget.max_pass_count);
    summary.max_backdrop_pixels = std::max(
        summary.max_backdrop_pixels,
        plan.resource_budget.max_backdrop_pixels);
    summary.max_frame_capture_count = std::max(
        summary.max_frame_capture_count,
        plan.resource_budget.max_frame_capture_count);
    summary.max_frame_capture_pixels = std::max(
        summary.max_frame_capture_pixels,
        plan.resource_budget.max_frame_capture_pixels);
    summary.max_surface_sample_pixels = std::max(
        summary.max_surface_sample_pixels,
        plan.resource_budget.max_surface_sample_pixels);
    summary.max_refraction_offset_pixels = std::max(
        summary.max_refraction_offset_pixels,
        plan.resource_budget.max_refraction_offset_pixels);
    summary.max_container_spacing = std::max(
        summary.max_container_spacing,
        plan.resource_budget.max_container_spacing);
    if (plan.container.participates)
        ++summary.containered_count;
    if (plan.container.shape_union_expected)
        ++summary.unioned_count;
    if (plan.container.interactive)
        ++summary.interactive_count;
    if (plan.container.morph_transitions)
        ++summary.morph_transition_count;
    if (plan.shape.valid)
        ++summary.valid_shape_count;
    if (plan.shape.rounded)
        ++summary.rounded_shape_count;
    if (plan.shape.capsule)
        ++summary.capsule_shape_count;
    if (plan.shape.radius_clamped)
        ++summary.radius_clamped_count;
    if (plan.foreground.backdrop_driven)
        ++summary.foreground_backdrop_driven_count;
    if (plan.foreground.high_contrast)
        ++summary.foreground_high_contrast_count;
    if (plan.foreground.uses_vibrancy)
        ++summary.foreground_vibrant_count;
    if (plan.interaction.enabled)
        ++summary.interaction_enabled_count;
    if (plan.interaction.active)
        ++summary.interaction_active_count;
    if (plan.interaction.reduce_motion)
        ++summary.interaction_reduce_motion_count;
    if (plan.interaction.specular_highlight_active)
        ++summary.interaction_specular_highlight_count;
    if (plan.interaction.pointer_lens_active)
        ++summary.interaction_pointer_lens_count;
    if (plan.refraction.active)
        ++summary.refraction_active_count;
    if (plan.theme.default_glass_tokens)
        ++summary.theme_default_glass_token_count;
    else
        ++summary.theme_custom_token_count;
    summary.max_surface_area = std::max(
        summary.max_surface_area,
        plan.shape.surface_area);
    summary.max_effective_radius = std::max(
        summary.max_effective_radius,
        plan.shape.effective_radius);
    summary.max_radius_limit = std::max(
        summary.max_radius_limit,
        plan.shape.radius_limit);
    summary.max_normalized_radius = std::max(
        summary.max_normalized_radius,
        plan.shape.normalized_radius);
    summary.max_saturation =
        std::max(summary.max_saturation, plan.saturation);
    summary.max_edge_highlight =
        std::max(summary.max_edge_highlight, plan.edge_highlight);
    summary.max_edge_width =
        std::max(summary.max_edge_width, plan.edge_width);
    summary.max_noise_opacity =
        std::max(summary.max_noise_opacity, plan.noise_opacity);
    summary.max_shadow_alpha =
        std::max(summary.max_shadow_alpha, plan.shadow_alpha);
    summary.max_shadow_radius =
        std::max(summary.max_shadow_radius, plan.shadow_radius);
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
    if (plan.foreground.primary_contrast_ratio > 0.0f) {
        summary.min_foreground_contrast_ratio =
            summary.min_foreground_contrast_ratio == 0.0f
                ? plan.foreground.primary_contrast_ratio
                : std::min(summary.min_foreground_contrast_ratio,
                           plan.foreground.primary_contrast_ratio);
    }
    if (!plan.resource_budget.bounded_texture_copy)
        ++summary.unbounded_texture_copy;
    if (!plan.resource_budget.deterministic_fallback)
        ++summary.non_deterministic_fallback;
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

inline void finalize_material_runtime_summary(
        MaterialRuntimeSummary& summary,
        std::span<MaterialRuntimeRecord const> records) {
    summary.container_groups = summarize_material_container_groups(records);
    auto const surface_samples =
        summarize_material_execution_surface_samples(records);
    summary.total_surface_sample_pixels =
        surface_samples.total_surface_sample_pixels;
    summary.max_surface_sample_pixels =
        surface_samples.max_surface_sample_pixels;
}

} // namespace phenotype

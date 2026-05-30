module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

export module phenotype.material.runtime;

import phenotype.material.plan;
import phenotype.material.types;

export namespace phenotype {

struct MaterialContainerGroupRuntimeSummary {
    std::uint32_t group_count = 0;
    std::uint32_t multi_surface_group_count = 0;
    std::uint32_t union_group_count = 0;
    std::uint32_t morph_group_count = 0;
    std::uint32_t interactive_group_count = 0;
    std::uint32_t shared_backdrop_scope_group_count = 0;
    std::uint32_t shared_capture_surface_count = 0;
    std::uint32_t shared_capture_saved_surface_count = 0;
    std::uint32_t max_shared_capture_group_surfaces = 0;
    std::uint32_t shape_blend_execution_group_count = 0;
    std::uint32_t shape_blend_execution_surface_count = 0;
    std::uint32_t fallback_mixed_group_count = 0;
    std::uint32_t max_group_size = 0;
    std::uint32_t max_active_surfaces = 0;
    std::uint32_t max_sampled_backdrop_surfaces = 0;
    std::uint32_t max_fallback_surfaces = 0;
    std::uint32_t total_shape_pair_count = 0;
    std::uint32_t blend_candidate_pair_count = 0;
    std::uint32_t union_candidate_pair_count = 0;
    std::uint32_t morph_candidate_pair_count = 0;
    std::uint32_t separated_pair_count = 0;
    float min_shape_gap = 0.0f;
    float max_shape_gap = 0.0f;
    float max_blend_distance = 0.0f;
    float max_group_bounds_width = 0.0f;
    float max_group_bounds_height = 0.0f;
    float max_group_bounds_area = 0.0f;
    float max_shape_blend_strength = 0.0f;
};

struct MaterialContainerExecutionDescriptor {
    std::uint32_t command_index = 0;
    std::uint32_t container_id = 0;
    std::uint32_t surface_count = 0;
    std::uint32_t glass_namespace_id = 0;
    std::uint32_t glass_effect_id = 0;
    std::uint32_t glass_effect_surface_count = 0;
    bool active = false;
    bool group_bounds_valid = false;
    bool shared_backdrop_scope = false;
    bool shape_blend_execution = false;
    bool group_surface_execution = false;
    bool union_execution = false;
    bool surface_leader = true;
    bool paint_layer_leader = true;
    bool morph_execution = false;
    bool glass_effect_match_execution = false;
    bool glass_effect_materialize_execution = false;
    char const* execution_policy = "isolated";
    char const* fusion_model = "none";
    bool fusion_optics_active = false;
    float group_x = 0.0f;
    float group_y = 0.0f;
    float group_w = 0.0f;
    float group_h = 0.0f;
    float group_radius = 0.0f;
    float shape_blend_strength = 0.0f;
    float inner_edge_alpha_blend_strength = 0.0f;
    float fusion_strength = 0.0f;
    float fusion_lensing_gain = 1.0f;
    float fusion_edge_lift = 0.0f;
    float fusion_shadow_gain = 1.0f;
    char const* cohesion_model = "none";
    bool cohesion_active = false;
    bool cohesion_spacing_driven = false;
    bool cohesion_morph_driven = false;
    bool cohesion_union_driven = false;
    bool cohesion_overlap_driven = false;
    float cohesion_strength = 0.0f;
    float cohesion_pressure = 0.0f;
    float cohesion_falloff = 0.0f;
    float cohesion_stabilization = 0.0f;
    char const* union_response_model = "none";
    bool union_response_active = false;
    bool union_response_spacing_driven = false;
    bool union_response_member_driven = false;
    bool union_response_overlap_driven = false;
    float union_response_strength = 0.0f;
    float union_edge_continuity = 0.0f;
    float union_shape_coalescence = 0.0f;
    float union_luma_stability = 0.0f;
    bool overlap_response_active = false;
    std::uint32_t overlap_pair_count = 0;
    float overlap_response_strength = 0.0f;
    float overlap_max_fraction = 0.0f;
    float overlap_density = 0.0f;
    float glass_effect_match_progress = 1.0f;
    float glass_effect_match_blend_strength = 0.0f;
    float glass_effect_materialize_progress = 1.0f;
    float glass_effect_materialize_opacity_gain = 1.0f;
    float glass_effect_materialize_optical_gain = 1.0f;
    float glass_effect_materialize_shadow_gain = 1.0f;
    float glass_effect_materialize_refraction_gain = 1.0f;
    float glass_effect_materialize_wave_strength = 0.0f;
    bool glass_effect_match_source_valid = false;
    float glass_effect_match_source_gap = 0.0f;
    float glass_effect_match_source_spacing = 0.0f;
    float glass_effect_match_source_proximity = 0.0f;
    bool glass_effect_match_source_effect_id_match = false;
    float glass_effect_match_source_x = 0.0f;
    float glass_effect_match_source_y = 0.0f;
    float glass_effect_match_source_w = 0.0f;
    float glass_effect_match_source_h = 0.0f;
    float glass_effect_match_source_radius = 0.0f;
    float glass_effect_match_rect_x = 0.0f;
    float glass_effect_match_rect_y = 0.0f;
    float glass_effect_match_rect_w = 0.0f;
    float glass_effect_match_rect_h = 0.0f;
    float glass_effect_match_rect_radius = 0.0f;
    bool glass_effect_match_appearance_active = false;
    std::uint32_t glass_effect_match_appearance_source_command_index = 0;
    float glass_effect_match_appearance_blend = 1.0f;
    bool glass_effect_match_appearance_tint_active = false;
    float glass_effect_match_appearance_tint_r = 0.0f;
    float glass_effect_match_appearance_tint_g = 0.0f;
    float glass_effect_match_appearance_tint_b = 0.0f;
    float glass_effect_match_appearance_tint_a = 0.0f;
    bool glass_effect_match_appearance_spectral_tint_active = false;
    float glass_effect_match_appearance_spectral_tint_warmth = 0.0f;
    float glass_effect_match_appearance_spectral_tint_coolness = 0.0f;
    float glass_effect_match_appearance_spectral_tint_dispersion = 0.0f;
    float glass_effect_match_appearance_spectral_tint_rim = 0.0f;
    bool glass_effect_match_appearance_prominent_glass_active = false;
    float glass_effect_match_appearance_prominent_glass_intensity = 0.0f;
    float glass_effect_match_appearance_prominent_glass_tint_weight = 0.0f;
    float glass_effect_match_appearance_prominent_glass_edge_lift = 0.0f;
    float glass_effect_match_appearance_prominent_glass_lensing_gain = 1.0f;
    bool glass_effect_match_appearance_clear_glass_active = false;
    float glass_effect_match_appearance_clear_glass_dimming = 0.0f;
    float glass_effect_match_appearance_clear_glass_contrast = 0.0f;
    float glass_effect_match_appearance_clear_glass_brightness_response = 0.0f;
    float glass_effect_match_appearance_clear_glass_detail_response = 0.0f;
    bool glass_effect_match_appearance_refraction_active = false;
    float glass_effect_match_appearance_refraction_strength = 0.0f;
    float glass_effect_match_appearance_refraction_edge_bias = 0.0f;
    float glass_effect_match_appearance_refraction_max_offset_pixels = 0.0f;
    float glass_effect_match_appearance_refraction_edge_caustic_intensity = 0.0f;
    bool glass_effect_match_appearance_dynamic_lighting_active = false;
    float glass_effect_match_appearance_dynamic_light_direction_x = 0.0f;
    float glass_effect_match_appearance_dynamic_light_direction_y = 0.0f;
    float glass_effect_match_appearance_dynamic_light_highlight = 0.0f;
    float glass_effect_match_appearance_dynamic_light_shadow = 0.0f;
    bool glass_effect_match_appearance_glass_thickness_active = false;
    float glass_effect_match_appearance_glass_thickness = 0.0f;
    float glass_effect_match_appearance_glass_lensing_gain = 1.0f;
    float glass_effect_match_appearance_glass_shadow_gain = 1.0f;
    float glass_effect_match_appearance_glass_scattering_gain = 1.0f;
    bool glass_effect_match_appearance_glass_dispersion_active = false;
    float glass_effect_match_appearance_glass_dispersion_axial_offset = 0.0f;
    float glass_effect_match_appearance_glass_dispersion_tangential_offset = 0.0f;
    float glass_effect_match_appearance_glass_dispersion_prismatic_gain = 1.0f;
    float glass_effect_match_appearance_glass_dispersion_caustic_spread = 0.0f;
    bool group_interaction_source_valid = false;
    std::uint32_t group_interaction_source_command_index = 0;
    float group_interaction_specular_anchor_x = 0.5f;
    float group_interaction_specular_anchor_y = 0.5f;
    float group_interaction_specular_radius = 0.0f;
    float group_interaction_specular_intensity = 0.0f;
    bool group_interaction_pointer_lens_active = false;
    float group_interaction_pointer_lens_anchor_x = 0.5f;
    float group_interaction_pointer_lens_anchor_y = 0.5f;
    float group_interaction_pointer_lens_radius = 0.0f;
    float group_interaction_pointer_lens_strength = 0.0f;
    bool group_interaction_control_morph_active = false;
    float group_interaction_control_morph_scale_delta = 0.0f;
    float group_interaction_control_morph_depth = 0.0f;
    float group_interaction_control_morph_edge = 0.0f;
    float group_interaction_control_morph_shadow = 0.0f;
    bool group_interaction_refraction_active = false;
    float group_interaction_refraction_strength = 0.0f;
    float group_interaction_refraction_edge_bias = 0.0f;
    float group_interaction_refraction_max_offset_pixels = 0.0f;
    float group_interaction_refraction_edge_caustic_intensity = 0.0f;
    bool group_interaction_dynamic_lighting_active = false;
    float group_interaction_dynamic_light_direction_x = 0.0f;
    float group_interaction_dynamic_light_direction_y = 0.0f;
    float group_interaction_dynamic_light_highlight = 0.0f;
    float group_interaction_dynamic_light_shadow = 0.0f;
    bool group_interaction_glass_thickness_active = false;
    float group_interaction_glass_thickness = 0.0f;
    float group_interaction_glass_lensing_gain = 1.0f;
    float group_interaction_glass_shadow_gain = 1.0f;
    float group_interaction_glass_scattering_gain = 1.0f;
    bool group_interaction_glass_dispersion_active = false;
    float group_interaction_glass_dispersion_axial_offset = 0.0f;
    float group_interaction_glass_dispersion_tangential_offset = 0.0f;
    float group_interaction_glass_dispersion_prismatic_gain = 1.0f;
    float group_interaction_glass_dispersion_caustic_spread = 0.0f;
    bool group_appearance_source_valid = false;
    std::uint32_t group_appearance_source_command_index = 0;
    bool group_appearance_tint_active = false;
    float group_appearance_tint_r = 0.0f;
    float group_appearance_tint_g = 0.0f;
    float group_appearance_tint_b = 0.0f;
    float group_appearance_tint_a = 0.0f;
    bool group_appearance_spectral_tint_active = false;
    float group_appearance_spectral_tint_warmth = 0.0f;
    float group_appearance_spectral_tint_coolness = 0.0f;
    float group_appearance_spectral_tint_dispersion = 0.0f;
    float group_appearance_spectral_tint_rim = 0.0f;
    bool group_appearance_prominent_glass_active = false;
    float group_appearance_prominent_glass_intensity = 0.0f;
    float group_appearance_prominent_glass_tint_weight = 0.0f;
    float group_appearance_prominent_glass_edge_lift = 0.0f;
    float group_appearance_prominent_glass_lensing_gain = 1.0f;
    bool group_appearance_clear_glass_active = false;
    float group_appearance_clear_glass_dimming = 0.0f;
    float group_appearance_clear_glass_contrast = 0.0f;
    float group_appearance_clear_glass_brightness_response = 0.0f;
    float group_appearance_clear_glass_detail_response = 0.0f;
    bool group_appearance_refraction_active = false;
    float group_appearance_refraction_strength = 0.0f;
    float group_appearance_refraction_edge_bias = 0.0f;
    float group_appearance_refraction_max_offset_pixels = 0.0f;
    float group_appearance_refraction_edge_caustic_intensity = 0.0f;
    bool group_appearance_dynamic_lighting_active = false;
    float group_appearance_dynamic_light_direction_x = 0.0f;
    float group_appearance_dynamic_light_direction_y = 0.0f;
    float group_appearance_dynamic_light_highlight = 0.0f;
    float group_appearance_dynamic_light_shadow = 0.0f;
    bool group_appearance_glass_thickness_active = false;
    float group_appearance_glass_thickness = 0.0f;
    float group_appearance_glass_lensing_gain = 1.0f;
    float group_appearance_glass_shadow_gain = 1.0f;
    float group_appearance_glass_scattering_gain = 1.0f;
    bool group_appearance_glass_dispersion_active = false;
    float group_appearance_glass_dispersion_axial_offset = 0.0f;
    float group_appearance_glass_dispersion_tangential_offset = 0.0f;
    float group_appearance_glass_dispersion_prismatic_gain = 1.0f;
    float group_appearance_glass_dispersion_caustic_spread = 0.0f;
};

struct MaterialPaintLayerExecutionGeometry {
    bool active = false;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float radius = 0.0f;
};

struct MaterialGlassEffectMotionOptics {
    bool active = false;
    float strength = 0.0f;
    float direction_x = 0.0f;
    float direction_y = 0.0f;
    float specular_anchor_x = 0.5f;
    float specular_anchor_y = 0.5f;
    float refraction_gain = 1.0f;
    float caustic_gain = 1.0f;
    float specular_intensity_gain = 1.0f;
    float flow_offset_gain = 0.0f;
    float ribbon_width = 0.0f;
    float highlight_gain = 0.0f;
};

inline MaterialTransitionAnalysis material_execution_transition(
        MaterialTransitionAnalysis transition,
        MaterialContainerExecutionDescriptor const* execution) noexcept {
    if (execution && execution->glass_effect_materialize_execution)
        return material_transition_as_materialize(transition);
    return transition;
}

inline MaterialPaintLayerExecutionGeometry
material_paint_layer_execution_geometry(
        MaterialPlan const& plan,
        MaterialPaintLayer const& layer,
        MaterialContainerExecutionDescriptor const* execution = nullptr)
        noexcept {
    if (!layer.active)
        return {};
    if (execution
        && (execution->union_execution
            || execution->group_surface_execution
            || execution->glass_effect_match_execution)
        && !execution->paint_layer_leader) {
        return {};
    }

    auto x = plan.geometry.x;
    auto y = plan.geometry.y;
    auto w = plan.geometry.w;
    auto h = plan.geometry.h;
    auto radius = plan.shape.effective_radius;
    if (execution
        && execution->glass_effect_match_execution
        && execution->glass_effect_match_source_valid) {
        x = execution->glass_effect_match_rect_x;
        y = execution->glass_effect_match_rect_y;
        w = execution->glass_effect_match_rect_w;
        h = execution->glass_effect_match_rect_h;
        radius = execution->glass_effect_match_rect_radius;
    } else if (execution
        && (execution->union_execution || execution->group_surface_execution)
        && execution->group_bounds_valid) {
        x = execution->group_x;
        y = execution->group_y;
        w = execution->group_w;
        h = execution->group_h;
        radius = execution->group_radius;
    }
    auto const transition = material_execution_transition(
        plan.transition,
        execution);
    material_apply_materialize_execution_geometry(
        transition,
        x,
        y,
        w,
        h,
        radius);

    auto const inflate = std::max(layer.inflate, 0.0f);
    MaterialPaintLayerExecutionGeometry geometry{};
    geometry.x = x + layer.x_offset - inflate;
    geometry.y = y + layer.y_offset - inflate;
    geometry.w = w + inflate * 2.0f;
    geometry.h = h + inflate * 2.0f;
    if (geometry.w <= 0.0f || geometry.h <= 0.0f)
        return {};
    geometry.radius = std::max(0.0f, radius + layer.radius_delta);
    geometry.active = true;
    return geometry;
}

struct MaterialGlassEffectMatchSource {
    MaterialRuntimeRecord const* record = nullptr;
    float gap = 0.0f;
    float spacing = 0.0f;
    float proximity = 0.0f;
    bool effect_id_match = false;

    constexpr bool valid() const noexcept {
        return record != nullptr;
    }
};

struct MaterialSurfaceExecutionGeometry {
    bool active = false;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float radius = 0.0f;
};

inline MaterialSurfaceExecutionGeometry material_surface_execution_geometry(
        MaterialPlan const& plan,
        MaterialContainerExecutionDescriptor const* execution = nullptr)
        noexcept {
    if (!plan.primary_pass.active)
        return {};
    if (execution
        && (execution->union_execution
            || execution->group_surface_execution
            || execution->glass_effect_match_execution)
        && !execution->surface_leader) {
        return {};
    }

    auto x = plan.geometry.x;
    auto y = plan.geometry.y;
    auto w = plan.geometry.w;
    auto h = plan.geometry.h;
    auto radius = plan.shape.effective_radius;
    if (execution
        && execution->glass_effect_match_execution
        && execution->glass_effect_match_source_valid) {
        x = execution->glass_effect_match_rect_x;
        y = execution->glass_effect_match_rect_y;
        w = execution->glass_effect_match_rect_w;
        h = execution->glass_effect_match_rect_h;
        radius = execution->glass_effect_match_rect_radius;
    } else if (execution
        && (execution->union_execution || execution->group_surface_execution)
        && execution->group_bounds_valid) {
        x = execution->group_x;
        y = execution->group_y;
        w = execution->group_w;
        h = execution->group_h;
        radius = execution->group_radius;
    }
    auto const transition = material_execution_transition(
        plan.transition,
        execution);
    material_apply_materialize_execution_geometry(
        transition,
        x,
        y,
        w,
        h,
        radius);

    auto const inflate = material_background_paint_inflate(plan);
    MaterialSurfaceExecutionGeometry geometry{};
    geometry.x = x - inflate;
    geometry.y = y - inflate;
    geometry.w = w + inflate * 2.0f;
    geometry.h = h + inflate * 2.0f;
    if (geometry.w <= 0.0f || geometry.h <= 0.0f)
        return {};
    geometry.radius = std::min(
        std::max(0.0f, radius + inflate),
        std::max(0.0f, std::min(geometry.w, geometry.h) * 0.5f));
    geometry.active = true;
    return geometry;
}

inline std::int64_t material_estimate_surface_sample_pixels(
        MaterialPlan const& plan,
        MaterialContainerExecutionDescriptor const* execution) noexcept {
    if (!plan.backdrop_sampling)
        return 0;
    auto const geometry =
        material_surface_execution_geometry(plan, execution);
    if (!geometry.active)
        return 0;
    return material_estimate_surface_sample_pixels_from_area(
        static_cast<double>(geometry.w) * static_cast<double>(geometry.h),
        plan.render_target);
}

inline float material_surface_execution_anchor(
        float anchor,
        float base_origin,
        float base_extent,
        float execution_origin,
        float execution_extent) noexcept {
    auto const clamped_anchor =
        std::isfinite(anchor) ? std::clamp(anchor, 0.0f, 1.0f) : 0.5f;
    if (execution_extent <= 0.0f)
        return clamped_anchor;
    auto const absolute = base_origin
        + clamped_anchor * std::max(base_extent, 0.0f);
    return std::clamp(
        (absolute - execution_origin) / execution_extent,
        0.0f,
        1.0f);
}

inline float material_surface_execution_anchor_x(
        MaterialPlan const& plan,
        MaterialSurfaceExecutionGeometry const& geometry,
        float anchor) noexcept {
    return material_surface_execution_anchor(
        anchor,
        plan.geometry.x,
        plan.geometry.w,
        geometry.x,
        geometry.w);
}

inline float material_surface_execution_anchor_y(
        MaterialPlan const& plan,
        MaterialSurfaceExecutionGeometry const& geometry,
        float anchor) noexcept {
    return material_surface_execution_anchor(
        anchor,
        plan.geometry.y,
        plan.geometry.h,
        geometry.y,
        geometry.h);
}

struct MaterialContainerGroupAccumulator {
    std::uint32_t container_id = 0;
    std::uint32_t surface_count = 0;
    std::uint32_t active_surfaces = 0;
    std::uint32_t sampled_backdrop_surfaces = 0;
    std::uint32_t fallback_surfaces = 0;
    std::uint32_t union_surfaces = 0;
    std::uint32_t morph_surfaces = 0;
    std::uint32_t interactive_surfaces = 0;
    std::uint32_t shared_backdrop_scope_surfaces = 0;
    std::uint32_t shape_pair_count = 0;
    std::uint32_t blend_candidate_pair_count = 0;
    std::uint32_t union_candidate_pair_count = 0;
    std::uint32_t morph_candidate_pair_count = 0;
    std::uint32_t separated_pair_count = 0;
    std::uint32_t overlap_pair_count = 0;
    bool has_bounds = false;
    bool has_shape_gap = false;
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    float min_shape_gap = 0.0f;
    float max_shape_gap = 0.0f;
    float max_shape_overlap = 0.0f;
    float max_blend_distance = 0.0f;
    float max_effective_radius = 0.0f;
};

struct MaterialGlassEffectMatchAccumulator {
    std::uint32_t namespace_id = 0;
    std::uint32_t effect_id = 0;
    std::uint32_t container_id = 0;
    std::uint32_t surface_count = 0;
    std::uint32_t matched_geometry_surfaces = 0;
    bool has_bounds = false;
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    float max_progress = 0.0f;
    float max_effective_radius = 0.0f;
};

inline bool material_plan_in_container(
        MaterialPlan const& plan,
        std::uint32_t container_id) noexcept {
    return plan.container.participates
        && plan.container.container_id == container_id;
}

inline bool material_plans_share_glass_effect_namespace(
        MaterialPlan const& a,
        MaterialPlan const& b) noexcept {
    return a.glass_identity.participates
        && b.glass_identity.participates
        && a.glass_identity.namespace_id == b.glass_identity.namespace_id;
}

inline bool material_plan_in_glass_effect_match_group(
        MaterialPlan const& plan,
        MaterialPlan const& anchor,
        std::uint32_t container_id) noexcept {
    return material_plans_share_glass_effect_namespace(plan, anchor)
        && material_plan_in_container(plan, container_id);
}

inline bool material_plans_share_glass_effect_id(
        MaterialPlan const& a,
        MaterialPlan const& b) noexcept {
    return material_plans_share_glass_effect_namespace(a, b)
        && a.glass_identity.effect_id != 0u
        && a.glass_identity.effect_id == b.glass_identity.effect_id;
}

inline bool material_plans_share_glass_effect_union_namespace(
        MaterialPlan const& a,
        MaterialPlan const& b) noexcept {
    if (a.glass_identity.namespace_id == 0u
        && b.glass_identity.namespace_id == 0u) {
        return true;
    }
    return a.glass_identity.namespace_id != 0u
        && a.glass_identity.namespace_id == b.glass_identity.namespace_id;
}

inline bool material_plans_share_glass_effect_union_shape(
        MaterialPlan const& a,
        MaterialPlan const& b) noexcept {
    if (!a.shape.valid
        || !b.shape.valid
        || a.shape.kind != b.shape.kind) {
        return false;
    }
    if (a.shape.kind == MaterialShapeKind::RoundedRectangle) {
        return std::fabs(a.shape.effective_radius - b.shape.effective_radius)
            <= 0.0001f;
    }
    return true;
}

inline bool material_plans_share_glass_effect_union(
        MaterialPlan const& a,
        MaterialPlan const& b) noexcept {
    return a.container.participates
        && b.container.participates
        && a.container.container_id == b.container.container_id
        && a.container.union_id != 0u
        && a.container.union_id == b.container.union_id
        && material_plans_share_glass_effect_union_namespace(a, b)
        && a.kind == b.kind
        && material_plans_share_glass_effect_union_shape(a, b);
}

inline bool material_plan_in_glass_effect_union_group(
        MaterialPlan const& plan,
        MaterialPlan const& anchor) noexcept {
    return material_plans_share_glass_effect_union(plan, anchor);
}

inline float material_rect_gap(MaterialGeometry const& a,
                               MaterialGeometry const& b) noexcept {
    auto const ax2 = a.x + std::max(0.0f, a.w);
    auto const ay2 = a.y + std::max(0.0f, a.h);
    auto const bx2 = b.x + std::max(0.0f, b.w);
    auto const by2 = b.y + std::max(0.0f, b.h);
    auto const dx = std::max(std::max(b.x - ax2, a.x - bx2), 0.0f);
    auto const dy = std::max(std::max(b.y - ay2, a.y - by2), 0.0f);
    return std::sqrt(dx * dx + dy * dy);
}

inline float material_rect_overlap_fraction(
        MaterialGeometry const& a,
        MaterialGeometry const& b) noexcept {
    auto const aw = std::max(0.0f, a.w);
    auto const ah = std::max(0.0f, a.h);
    auto const bw = std::max(0.0f, b.w);
    auto const bh = std::max(0.0f, b.h);
    auto const min_area = std::min(aw * ah, bw * bh);
    if (min_area <= 0.0001f)
        return 0.0f;
    auto const ix0 = std::max(a.x, b.x);
    auto const iy0 = std::max(a.y, b.y);
    auto const ix1 = std::min(a.x + aw, b.x + bw);
    auto const iy1 = std::min(a.y + ah, b.y + bh);
    auto const iw = std::max(0.0f, ix1 - ix0);
    auto const ih = std::max(0.0f, iy1 - iy0);
    if (iw <= 0.0001f || ih <= 0.0001f)
        return 0.0f;
    return std::clamp((iw * ih) / min_area, 0.0f, 1.0f);
}

inline void accumulate_material_container_bounds(
        MaterialContainerGroupAccumulator& group,
        MaterialPlan const& plan) noexcept {
    if (!plan.shape.valid)
        return;
    group.max_blend_distance =
        std::max(group.max_blend_distance, plan.container.blend_distance);
    group.max_effective_radius =
        std::max(group.max_effective_radius, plan.shape.effective_radius);
    auto const x0 = plan.geometry.x;
    auto const y0 = plan.geometry.y;
    auto const x1 = plan.geometry.x + std::max(0.0f, plan.geometry.w);
    auto const y1 = plan.geometry.y + std::max(0.0f, plan.geometry.h);
    if (!group.has_bounds) {
        group.has_bounds = true;
        group.min_x = x0;
        group.min_y = y0;
        group.max_x = x1;
        group.max_y = y1;
        return;
    }
    group.min_x = std::min(group.min_x, x0);
    group.min_y = std::min(group.min_y, y0);
    group.max_x = std::max(group.max_x, x1);
    group.max_y = std::max(group.max_y, y1);
}

inline void accumulate_material_glass_effect_match_bounds(
        MaterialGlassEffectMatchAccumulator& group,
        MaterialPlan const& plan) noexcept {
    if (!plan.shape.valid)
        return;
    group.max_effective_radius =
        std::max(group.max_effective_radius, plan.shape.effective_radius);
    auto const x0 = plan.geometry.x;
    auto const y0 = plan.geometry.y;
    auto const x1 = plan.geometry.x + std::max(0.0f, plan.geometry.w);
    auto const y1 = plan.geometry.y + std::max(0.0f, plan.geometry.h);
    if (!group.has_bounds) {
        group.has_bounds = true;
        group.min_x = x0;
        group.min_y = y0;
        group.max_x = x1;
        group.max_y = y1;
        return;
    }
    group.min_x = std::min(group.min_x, x0);
    group.min_y = std::min(group.min_y, y0);
    group.max_x = std::max(group.max_x, x1);
    group.max_y = std::max(group.max_y, y1);
}

inline float material_container_bounds_span(float min, float max) noexcept {
    return std::max(0.0f, max - min);
}

inline float material_container_group_radius_for_bounds(
        float radius,
        float w,
        float h) noexcept {
    if (!std::isfinite(radius) || radius <= 0.0f)
        return 0.0f;
    auto const limit = std::max(0.0f, std::min(w, h) * 0.5f);
    return std::clamp(radius, 0.0f, limit);
}

inline float material_container_group_bounds_width(
        MaterialContainerGroupAccumulator const& group) noexcept {
    return group.has_bounds
        ? material_container_bounds_span(group.min_x, group.max_x)
        : 0.0f;
}

inline float material_container_group_bounds_height(
        MaterialContainerGroupAccumulator const& group) noexcept {
    return group.has_bounds
        ? material_container_bounds_span(group.min_y, group.max_y)
        : 0.0f;
}

inline float material_container_group_execution_radius(
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!group.has_bounds)
        return 0.0f;
    return material_container_group_radius_for_bounds(
        group.max_effective_radius,
        material_container_group_bounds_width(group),
        material_container_group_bounds_height(group));
}

inline float material_glass_effect_match_group_bounds_width(
        MaterialGlassEffectMatchAccumulator const& group) noexcept {
    return group.has_bounds
        ? material_container_bounds_span(group.min_x, group.max_x)
        : 0.0f;
}

inline float material_glass_effect_match_group_bounds_height(
        MaterialGlassEffectMatchAccumulator const& group) noexcept {
    return group.has_bounds
        ? material_container_bounds_span(group.min_y, group.max_y)
        : 0.0f;
}

inline float material_glass_effect_match_group_execution_radius(
        MaterialGlassEffectMatchAccumulator const& group) noexcept {
    if (!group.has_bounds)
        return 0.0f;
    return material_container_group_radius_for_bounds(
        group.max_effective_radius,
        material_glass_effect_match_group_bounds_width(group),
        material_glass_effect_match_group_bounds_height(group));
}

inline void material_apply_container_group_execution_bounds(
        MaterialContainerExecutionDescriptor& descriptor,
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!group.has_bounds)
        return;
    descriptor.group_bounds_valid = true;
    descriptor.group_x = group.min_x;
    descriptor.group_y = group.min_y;
    descriptor.group_w = material_container_group_bounds_width(group);
    descriptor.group_h = material_container_group_bounds_height(group);
    descriptor.group_radius = material_container_group_execution_radius(group);
}

inline void material_apply_glass_effect_match_group_execution_bounds(
        MaterialContainerExecutionDescriptor& descriptor,
        MaterialGlassEffectMatchAccumulator const& group) noexcept {
    if (!group.has_bounds)
        return;
    descriptor.group_bounds_valid = true;
    descriptor.group_x = group.min_x;
    descriptor.group_y = group.min_y;
    descriptor.group_w = material_glass_effect_match_group_bounds_width(group);
    descriptor.group_h = material_glass_effect_match_group_bounds_height(group);
    descriptor.group_radius =
        material_glass_effect_match_group_execution_radius(group);
}

inline void accumulate_material_container_pair(
        MaterialContainerGroupAccumulator& group,
        MaterialPlan const& a,
        MaterialPlan const& b) noexcept {
    if (!a.shape.valid || !b.shape.valid)
        return;
    ++group.shape_pair_count;
    auto const gap = material_rect_gap(a.geometry, b.geometry);
    if (!group.has_shape_gap) {
        group.has_shape_gap = true;
        group.min_shape_gap = gap;
        group.max_shape_gap = gap;
    } else {
        group.min_shape_gap = std::min(group.min_shape_gap, gap);
        group.max_shape_gap = std::max(group.max_shape_gap, gap);
    }
    auto const blend_distance =
        std::min(a.container.blend_distance, b.container.blend_distance);
    group.max_blend_distance =
        std::max(group.max_blend_distance, blend_distance);
    auto const overlap = material_rect_overlap_fraction(a.geometry, b.geometry);
    if (overlap > 0.0001f) {
        ++group.overlap_pair_count;
        group.max_shape_overlap = std::max(group.max_shape_overlap, overlap);
    }
    auto const blend_candidate = gap <= blend_distance;
    if (blend_candidate)
        ++group.blend_candidate_pair_count;
    else
        ++group.separated_pair_count;
    if (blend_candidate && material_plans_share_glass_effect_union(a, b)) {
        ++group.union_candidate_pair_count;
    }
    if (blend_candidate
        && (a.container.morph_transitions
            || b.container.morph_transitions)) {
        ++group.morph_candidate_pair_count;
    }
}

inline bool material_container_group_shape_blend_execution_active(
        MaterialContainerGroupAccumulator const& group) noexcept {
    return group.active_surfaces > 1u
        && group.blend_candidate_pair_count > 0u
        && (group.union_surfaces > 0u
            || group.morph_surfaces > 0u
            || group.shared_backdrop_scope_surfaces > 1u
            || group.interactive_surfaces > 0u);
}

inline float material_container_group_shape_blend_strength(
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!material_container_group_shape_blend_execution_active(group)
        || !group.has_shape_gap) {
        return 0.0f;
    }
    if (group.max_blend_distance <= 0.0f)
        return group.min_shape_gap <= 0.5f ? 1.0f : 0.0f;
    auto const proximity = std::clamp(
        (group.max_blend_distance - group.min_shape_gap)
            / group.max_blend_distance,
        0.0f,
        1.0f);
    return std::clamp(std::max(0.25f, proximity), 0.0f, 1.0f);
}

inline bool material_glass_effect_match_execution_active(
        MaterialGlassEffectMatchAccumulator const& group) noexcept {
    return group.surface_count > 1u
        && group.matched_geometry_surfaces > 0u
        && group.has_bounds;
}

inline bool material_glass_effect_union_execution_active(
        MaterialContainerGroupAccumulator const& group) noexcept {
    return group.active_surfaces > 1u
        && group.union_surfaces > 1u
        && group.has_bounds;
}

inline float material_lerp(float a, float b, float t) noexcept {
    return a + (b - a) * t;
}

inline float material_glass_effect_match_progress_gain(
        float progress) noexcept {
    progress = std::isfinite(progress)
        ? std::clamp(progress, 0.0f, 1.0f)
        : 1.0f;
    return progress * progress * (3.0f - 2.0f * progress);
}

inline float material_glass_effect_match_blend_strength(
        MaterialGlassEffectMatchAccumulator const& group) noexcept {
    if (!material_glass_effect_match_execution_active(group))
        return 0.0f;
    auto const progress = std::clamp(group.max_progress, 0.0f, 1.0f);
    auto const smooth = material_glass_effect_match_progress_gain(progress);
    return std::clamp(std::max(0.25f, smooth), 0.0f, 1.0f);
}

inline float material_container_group_blend_continuity(
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (group.union_surfaces > 0u)
        return 1.0f;
    if (group.morph_surfaces > 0u)
        return 0.85f;
    if (group.shared_backdrop_scope_surfaces > 1u)
        return 0.65f;
    if (group.interactive_surfaces > 0u)
        return 0.50f;
    return 0.0f;
}

inline float material_container_overlap_density(
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (group.shape_pair_count == 0u || group.overlap_pair_count == 0u)
        return 0.0f;
    return std::clamp(
        static_cast<float>(group.overlap_pair_count)
            / static_cast<float>(group.shape_pair_count),
        0.0f,
        1.0f);
}

inline float material_container_overlap_response_strength(
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (group.overlap_pair_count == 0u)
        return 0.0f;
    auto const overlap = std::clamp(group.max_shape_overlap, 0.0f, 1.0f);
    auto const density = material_container_overlap_density(group);
    auto const continuity = material_container_group_blend_continuity(group);
    return std::clamp(
        0.32f * std::sqrt(overlap)
            + 0.38f * overlap
            + 0.20f * density
            + 0.10f * continuity,
        0.0f,
        1.0f);
}

inline char const* material_container_cohesion_model_name(
        MaterialContainerExecutionDescriptor const& execution) noexcept {
    if (execution.glass_effect_match_execution)
        return "matched-liquid-glass-cohesion";
    if (execution.union_execution)
        return "union-liquid-glass-cohesion";
    if (execution.group_surface_execution)
        return "proximity-liquid-glass-cohesion";
    if (execution.morph_execution)
        return "morph-liquid-glass-cohesion";
    if (execution.shape_blend_execution)
        return "spacing-liquid-glass-cohesion";
    return "none";
}

inline float material_container_spacing_proximity(
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!group.has_shape_gap)
        return 0.0f;
    if (group.max_blend_distance <= 0.0f)
        return group.min_shape_gap <= 0.5f ? 1.0f : 0.0f;
    return std::clamp(
        (group.max_blend_distance - group.min_shape_gap)
            / group.max_blend_distance,
        0.0f,
        1.0f);
}

inline float material_container_spacing_falloff(
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!group.has_shape_gap || group.max_blend_distance <= 0.0f)
        return 0.0f;
    return std::clamp(
        group.min_shape_gap / group.max_blend_distance,
        0.0f,
        1.0f);
}

inline char const* material_container_fusion_model_name(
        MaterialContainerExecutionDescriptor const& execution) noexcept {
    if (execution.glass_effect_match_execution)
        return "matched-liquid-glass-fusion";
    if (execution.union_execution)
        return "union-liquid-glass-fusion";
    if (execution.group_surface_execution)
        return "proximity-liquid-glass-fusion";
    if (execution.morph_execution)
        return "morph-liquid-glass-fusion";
    if (execution.shape_blend_execution)
        return "container-liquid-glass-fusion";
    return "none";
}

inline float material_container_union_fusion_density(
        MaterialContainerGroupAccumulator const& group,
        bool union_execution) noexcept {
    if (!union_execution)
        return 0.0f;
    auto const member_density = std::clamp(
        (static_cast<float>(group.surface_count) - 2.0f) / 4.0f,
        0.0f,
        1.0f);
    auto aspect_density = 0.0f;
    if (group.has_bounds) {
        auto const group_w = material_container_group_bounds_width(group);
        auto const group_h = material_container_group_bounds_height(group);
        auto const min_extent = std::max(std::min(group_w, group_h), 1.0f);
        auto const max_extent = std::max(group_w, group_h);
        aspect_density = std::clamp(
            (max_extent / min_extent - 1.0f) / 4.0f,
            0.0f,
            1.0f);
    }
    return std::clamp(
        0.50f * member_density + 0.50f * aspect_density,
        0.0f,
        1.0f);
}

inline void material_apply_container_fusion_optics(
        MaterialContainerExecutionDescriptor& execution,
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!execution.shape_blend_execution
        || execution.shape_blend_strength <= 0.0001f) {
        execution.fusion_model = "none";
        execution.fusion_optics_active = false;
        execution.fusion_strength = 0.0f;
        execution.fusion_lensing_gain = 1.0f;
        execution.fusion_edge_lift = 0.0f;
        execution.fusion_shadow_gain = 1.0f;
        execution.overlap_response_active = false;
        execution.overlap_pair_count = 0u;
        execution.overlap_response_strength = 0.0f;
        execution.overlap_max_fraction = 0.0f;
        execution.overlap_density = 0.0f;
        return;
    }

    auto const continuity = material_container_group_blend_continuity(group);
    auto const match_bias = execution.glass_effect_match_execution ? 0.10f : 0.0f;
    auto const union_bias = execution.union_execution ? 0.08f : 0.0f;
    auto const fusion_strength = std::clamp(
        execution.shape_blend_strength
            * (0.72f + 0.28f * continuity + match_bias + union_bias),
        0.0f,
        1.0f);
    execution.fusion_model = material_container_fusion_model_name(execution);
    execution.fusion_optics_active = fusion_strength > 0.0001f;
    execution.fusion_strength = fusion_strength;
    auto const union_density =
        material_container_union_fusion_density(
            group,
            execution.union_execution);
    execution.fusion_lensing_gain = 1.0f + 0.18f * fusion_strength
        + 0.07f * union_density;
    execution.fusion_edge_lift = std::clamp(
        0.075f * fusion_strength + 0.025f * union_bias
            + 0.04f * union_density,
        0.0f,
        0.16f);
    execution.fusion_shadow_gain = 1.0f + 0.16f * fusion_strength
        + 0.07f * union_density;
    execution.overlap_pair_count = group.overlap_pair_count;
    execution.overlap_max_fraction =
        std::clamp(group.max_shape_overlap, 0.0f, 1.0f);
    execution.overlap_density = material_container_overlap_density(group);
    execution.overlap_response_strength =
        material_container_overlap_response_strength(group);
    execution.overlap_response_active =
        execution.overlap_response_strength > 0.0001f;
}

inline void material_apply_container_cohesion_profile(
        MaterialContainerExecutionDescriptor& execution,
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!execution.shape_blend_execution
        || execution.shape_blend_strength <= 0.0001f) {
        execution.cohesion_model = "none";
        execution.cohesion_active = false;
        execution.cohesion_spacing_driven = false;
        execution.cohesion_morph_driven = false;
        execution.cohesion_union_driven = false;
        execution.cohesion_overlap_driven = false;
        execution.cohesion_strength = 0.0f;
        execution.cohesion_pressure = 0.0f;
        execution.cohesion_falloff = 0.0f;
        execution.cohesion_stabilization = 0.0f;
        return;
    }

    auto const continuity = material_container_group_blend_continuity(group);
    auto const overlap = material_container_overlap_response_strength(group);
    auto const overlap_density = material_container_overlap_density(group);
    auto const spacing_proximity =
        material_container_spacing_proximity(group);
    auto const spacing_falloff = material_container_spacing_falloff(group);
    auto const member_density =
        group.surface_count > 1u
            ? std::clamp(
                (static_cast<float>(group.surface_count) - 2.0f) / 4.0f,
                0.0f,
                1.0f)
            : 0.0f;
    auto const morph = execution.morph_execution ? 1.0f : 0.0f;
    auto const union_bias = execution.union_execution ? 1.0f : 0.0f;
    auto const match = execution.glass_effect_match_execution ? 1.0f : 0.0f;
    auto const shared = execution.shared_backdrop_scope ? 1.0f : 0.0f;

    execution.cohesion_model = material_container_cohesion_model_name(execution);
    execution.cohesion_active = true;
    execution.cohesion_spacing_driven = spacing_proximity > 0.0001f;
    execution.cohesion_morph_driven = morph > 0.0f || match > 0.0f;
    execution.cohesion_union_driven = union_bias > 0.0f;
    execution.cohesion_overlap_driven = overlap > 0.0001f;
    execution.cohesion_strength = std::clamp(
        execution.shape_blend_strength
                * (0.50f
                   + 0.22f * continuity
                   + 0.14f * spacing_proximity
                   + 0.08f * overlap
                   + 0.06f * member_density)
            + 0.050f * match
            + 0.040f * union_bias,
        0.0f,
        1.0f);
    execution.cohesion_pressure = std::clamp(
        0.35f * spacing_proximity
            + 0.20f * overlap
            + 0.15f * continuity
            + 0.12f * morph
            + 0.10f * shared
            + 0.08f * member_density,
        0.0f,
        1.0f);
    execution.cohesion_falloff = spacing_falloff;
    execution.cohesion_stabilization = std::clamp(
        0.30f * execution.cohesion_strength
            + 0.24f * execution.fusion_strength
            + 0.18f * execution.cohesion_pressure
            + 0.12f * overlap
            + 0.10f * match
            + 0.06f * union_bias
            + 0.04f * overlap_density,
        0.0f,
        1.0f);
}

inline void material_apply_glass_union_response_profile(
        MaterialContainerExecutionDescriptor& execution,
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!execution.union_execution
        || !execution.shape_blend_execution
        || execution.shape_blend_strength <= 0.0001f) {
        execution.union_response_model = "none";
        execution.union_response_active = false;
        execution.union_response_spacing_driven = false;
        execution.union_response_member_driven = false;
        execution.union_response_overlap_driven = false;
        execution.union_response_strength = 0.0f;
        execution.union_edge_continuity = 0.0f;
        execution.union_shape_coalescence = 0.0f;
        execution.union_luma_stability = 0.0f;
        return;
    }

    auto const spacing_proximity =
        material_container_spacing_proximity(group);
    auto const spacing_falloff = material_container_spacing_falloff(group);
    auto const overlap = material_container_overlap_response_strength(group);
    auto const overlap_density = material_container_overlap_density(group);
    auto const member_density =
        group.surface_count > 1u
            ? std::clamp(
                (static_cast<float>(group.surface_count) - 2.0f) / 4.0f,
                0.0f,
                1.0f)
            : 0.0f;
    auto aspect_spread = 0.0f;
    if (group.has_bounds) {
        auto const width =
            std::max(material_container_group_bounds_width(group), 1.0f);
        auto const height =
            std::max(material_container_group_bounds_height(group), 1.0f);
        auto const min_extent = std::max(std::min(width, height), 1.0f);
        auto const max_extent = std::max(width, height);
        aspect_spread = std::clamp(
            (max_extent / min_extent - 1.0f) / 4.0f,
            0.0f,
            1.0f);
    }
    auto const edge_lift =
        std::clamp(execution.fusion_edge_lift / 0.16f, 0.0f, 1.0f);
    auto const shadow_gain =
        std::clamp((execution.fusion_shadow_gain - 1.0f) / 0.32f,
                   0.0f,
                   1.0f);

    execution.union_response_model = "glass-effect-union-response";
    execution.union_response_active = true;
    execution.union_response_spacing_driven = spacing_proximity > 0.0001f;
    execution.union_response_member_driven = member_density > 0.0001f
        || group.surface_count > 1u;
    execution.union_response_overlap_driven = overlap > 0.0001f
        || overlap_density > 0.0001f;
    execution.union_response_strength = std::clamp(
        0.36f
            + 0.28f * execution.shape_blend_strength
            + 0.12f * spacing_proximity
            + 0.10f * execution.fusion_strength
            + 0.08f * member_density
            + 0.06f * overlap,
        0.0f,
        1.0f);
    execution.union_edge_continuity = std::clamp(
        0.30f
            + 0.26f * execution.shape_blend_strength
            + 0.18f * spacing_proximity
            + 0.14f * (1.0f - spacing_falloff)
            + 0.10f * edge_lift
            + 0.08f * overlap,
        0.0f,
        1.0f);
    execution.union_shape_coalescence = std::clamp(
        0.28f
            + 0.34f * execution.fusion_strength
            + 0.18f * execution.cohesion_strength
            + 0.12f * member_density
            + 0.08f * (1.0f - aspect_spread),
        0.0f,
        1.0f);
    execution.union_luma_stability = std::clamp(
        0.32f
            + 0.26f * execution.cohesion_stabilization
            + 0.14f * shadow_gain
            + 0.10f * (1.0f - overlap_density)
            + 0.08f * (1.0f - spacing_falloff),
        0.0f,
        1.0f);
}

inline float material_container_shape_blend_strength_for_gap(
        float gap,
        float blend_distance) noexcept {
    if (blend_distance <= 0.0f)
        return gap <= 0.5f ? 1.0f : 0.0f;
    if (gap > blend_distance)
        return 0.0f;
    auto const proximity = std::clamp(
        (blend_distance - gap) / blend_distance,
        0.0f,
        1.0f);
    return std::clamp(std::max(0.25f, proximity), 0.0f, 1.0f);
}

inline float material_container_member_shape_blend_strength(
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records,
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!material_container_group_shape_blend_execution_active(group))
        return 0.0f;
    auto const& plan = record.plan;
    if (!plan.primary_pass.active
        || !plan.shape.valid
        || !material_plan_in_container(plan, group.container_id)) {
        return 0.0f;
    }
    auto best = 0.0f;
    for (auto const& candidate_record : records) {
        if (&candidate_record == &record)
            continue;
        auto const& candidate = candidate_record.plan;
        if (!candidate.primary_pass.active
            || !candidate.shape.valid
            || !material_plan_in_container(candidate, group.container_id)) {
            continue;
        }
        auto const gap = material_rect_gap(plan.geometry, candidate.geometry);
        auto const blend_distance = std::min(
            plan.container.blend_distance,
            candidate.container.blend_distance);
        best = std::max(
            best,
            material_container_shape_blend_strength_for_gap(
                gap,
                blend_distance));
    }
    return std::min(
        material_container_group_shape_blend_strength(group),
        best);
}

inline float material_glass_effect_union_member_shape_blend_strength(
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records,
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!material_glass_effect_union_execution_active(group))
        return 0.0f;
    auto const& plan = record.plan;
    if (!plan.primary_pass.active
        || !material_plan_in_glass_effect_union_group(plan, plan)) {
        return 0.0f;
    }
    for (auto const& candidate_record : records) {
        if (&candidate_record == &record)
            continue;
        auto const& candidate = candidate_record.plan;
        if (!candidate.primary_pass.active
            || !material_plan_in_glass_effect_union_group(candidate, plan)) {
            continue;
        }
        return 1.0f;
    }
    return 0.0f;
}

inline std::uint32_t material_container_group_shape_blend_surface_count(
        std::span<MaterialRuntimeRecord const> records,
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (!material_container_group_shape_blend_execution_active(group))
        return 0u;
    std::uint32_t count = 0;
    for (auto const& record : records) {
        if (material_container_member_shape_blend_strength(
                record,
                records,
                group) > 0.0f) {
            ++count;
        }
    }
    return count;
}

inline bool material_container_shape_blend_edge(
        MaterialPlan const& a,
        MaterialPlan const& b,
        std::uint32_t container_id) noexcept {
    if (!a.primary_pass.active
        || !b.primary_pass.active
        || !a.shape.valid
        || !b.shape.valid
        || !material_plan_in_container(a, container_id)
        || !material_plan_in_container(b, container_id)) {
        return false;
    }
    auto const gap = material_rect_gap(a.geometry, b.geometry);
    auto const blend_distance = std::min(
        a.container.blend_distance,
        b.container.blend_distance);
    return material_container_shape_blend_strength_for_gap(gap, blend_distance)
        > 0.0f;
}

inline MaterialContainerGroupAccumulator
accumulate_material_container_shape_blend_cluster(
        MaterialRuntimeRecord const& anchor,
        std::span<MaterialRuntimeRecord const> records,
        MaterialContainerGroupAccumulator const& group) {
    MaterialContainerGroupAccumulator cluster{
        .container_id = group.container_id,
    };
    if (!material_container_group_shape_blend_execution_active(group)
        || !anchor.plan.primary_pass.active
        || !anchor.plan.shape.valid
        || !material_plan_in_container(anchor.plan, group.container_id)) {
        return cluster;
    }

    std::vector<unsigned char> selected(records.size(), 0u);
    auto anchor_found = false;
    for (std::size_t index = 0; index < records.size(); ++index) {
        if (records[index].command_index == anchor.command_index) {
            selected[index] = 1u;
            anchor_found = true;
            break;
        }
    }
    if (!anchor_found)
        return cluster;

    auto changed = true;
    while (changed) {
        changed = false;
        for (std::size_t index = 0; index < records.size(); ++index) {
            if (selected[index] != 0u)
                continue;
            auto const& candidate = records[index];
            if (!material_plan_in_container(candidate.plan, group.container_id))
                continue;
            for (std::size_t selected_index = 0;
                 selected_index < records.size();
                 ++selected_index) {
                if (selected[selected_index] == 0u)
                    continue;
                if (material_container_shape_blend_edge(
                        candidate.plan,
                        records[selected_index].plan,
                        group.container_id)) {
                    selected[index] = 1u;
                    changed = true;
                    break;
                }
            }
        }
    }

    for (std::size_t index = 0; index < records.size(); ++index) {
        if (selected[index] == 0u)
            continue;
        auto const& candidate = records[index].plan;
        ++cluster.surface_count;
        accumulate_material_container_bounds(cluster, candidate);
        if (candidate.primary_pass.active)
            ++cluster.active_surfaces;
        if (candidate.backdrop_sampling)
            ++cluster.sampled_backdrop_surfaces;
        if (candidate.fallback())
            ++cluster.fallback_surfaces;
        if (candidate.container.shape_union_expected)
            ++cluster.union_surfaces;
        if (candidate.container.morph_transitions)
            ++cluster.morph_surfaces;
        if (candidate.container.interactive)
            ++cluster.interactive_surfaces;
        if (candidate.container.shared_backdrop_scope)
            ++cluster.shared_backdrop_scope_surfaces;
    }
    for (std::size_t left = 0; left < records.size(); ++left) {
        if (selected[left] == 0u)
            continue;
        for (std::size_t right = left + 1; right < records.size(); ++right) {
            if (selected[right] == 0u)
                continue;
            accumulate_material_container_pair(
                cluster,
                records[left].plan,
                records[right].plan);
        }
    }
    return cluster;
}

inline bool material_container_plan_inside_cluster_bounds(
        MaterialPlan const& plan,
        MaterialContainerGroupAccumulator const& cluster) noexcept {
    if (!cluster.has_bounds || !plan.shape.valid)
        return false;
    auto const x1 = plan.geometry.x + std::max(0.0f, plan.geometry.w);
    auto const y1 = plan.geometry.y + std::max(0.0f, plan.geometry.h);
    return plan.geometry.x >= cluster.min_x
        && plan.geometry.y >= cluster.min_y
        && x1 <= cluster.max_x
        && y1 <= cluster.max_y;
}

inline bool material_container_shape_blend_surface_leader(
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records,
        MaterialContainerGroupAccumulator const& cluster) {
    if (!material_plan_uses_group_surface_merge_executor(record.plan))
        return true;
    auto found = false;
    auto leader_command_index = record.command_index;
    for (auto const& candidate : records) {
        if (!material_plan_uses_group_surface_merge_executor(candidate.plan)
            || !material_plan_in_container(candidate.plan, cluster.container_id)
            || !material_container_plan_inside_cluster_bounds(
                candidate.plan,
                cluster)) {
            continue;
        }
        if (!found || candidate.command_index < leader_command_index) {
            leader_command_index = candidate.command_index;
            found = true;
        }
    }
    return !found || record.command_index == leader_command_index;
}

inline bool material_glass_effect_match_surface_leader(
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records,
        std::uint32_t container_id) noexcept {
    auto const& plan = record.plan;
    if (!plan.glass_identity.matched_geometry_candidate)
        return true;

    auto found = false;
    auto leader_command_index = record.command_index;
    auto leader_appearing = false;
    auto leader_progress = 0.0f;
    for (auto const& candidate_record : records) {
        auto const& candidate = candidate_record.plan;
        if (!candidate.primary_pass.active
            || !candidate.glass_identity.matched_geometry_candidate
            || !material_plan_in_glass_effect_match_group(
                candidate,
                plan,
                container_id)) {
            continue;
        }
        auto const candidate_appearing = candidate.transition.appearing;
        auto const candidate_progress = std::isfinite(
            candidate.transition.progress)
            ? std::clamp(candidate.transition.progress, 0.0f, 1.0f)
            : 1.0f;
        auto const better =
            !found
            || (candidate_appearing && !leader_appearing)
            || (candidate_appearing == leader_appearing
                && candidate_progress > leader_progress + 0.0001f)
            || (candidate_appearing == leader_appearing
                && std::fabs(candidate_progress - leader_progress) <= 0.0001f
                && candidate_record.command_index < leader_command_index);
        if (better) {
            found = true;
            leader_command_index = candidate_record.command_index;
            leader_appearing = candidate_appearing;
            leader_progress = candidate_progress;
        }
    }
    return !found || record.command_index == leader_command_index;
}

inline float material_container_inner_edge_alpha_blend_strength(
        MaterialContainerGroupAccumulator const& group,
        float shape_blend) noexcept {
    if (shape_blend <= 0.0f)
        return 0.0f;
    auto const continuity = material_container_group_blend_continuity(group);
    return std::clamp(shape_blend * continuity, 0.0f, 1.0f);
}

inline float material_container_group_inner_edge_alpha_blend_strength(
        MaterialContainerGroupAccumulator const& group) noexcept {
    return material_container_inner_edge_alpha_blend_strength(
        group,
        material_container_group_shape_blend_strength(group));
}

inline char const* material_container_execution_policy_name(
        MaterialContainerGroupAccumulator const& group) noexcept {
    if (material_container_group_shape_blend_execution_active(group))
        return "group-edge-continuity";
    if (group.surface_count > 0u)
        return "group-isolated";
    return "isolated";
}

inline MaterialContainerGroupAccumulator accumulate_material_container_group(
        std::span<MaterialRuntimeRecord const> records,
        std::uint32_t container_id) noexcept {
    MaterialContainerGroupAccumulator group{
        .container_id = container_id,
    };
    if (container_id == 0u)
        return group;
    for (auto const& candidate_record : records) {
        auto const& candidate = candidate_record.plan;
        if (!candidate.container.participates
            || candidate.container.container_id != container_id) {
            continue;
        }
        ++group.surface_count;
        accumulate_material_container_bounds(group, candidate);
        if (candidate.primary_pass.active)
            ++group.active_surfaces;
        if (candidate.backdrop_sampling)
            ++group.sampled_backdrop_surfaces;
        if (candidate.fallback())
            ++group.fallback_surfaces;
        if (candidate.container.shape_union_expected)
            ++group.union_surfaces;
        if (candidate.container.morph_transitions)
            ++group.morph_surfaces;
        if (candidate.container.interactive)
            ++group.interactive_surfaces;
        if (candidate.container.shared_backdrop_scope)
            ++group.shared_backdrop_scope_surfaces;
    }
    for (std::size_t left = 0; left < records.size(); ++left) {
        auto const& a = records[left].plan;
        if (!material_plan_in_container(a, container_id))
            continue;
        for (std::size_t right = left + 1; right < records.size(); ++right) {
            auto const& b = records[right].plan;
            if (!material_plan_in_container(b, container_id))
                continue;
            accumulate_material_container_pair(group, a, b);
        }
    }
    return group;
}

inline MaterialContainerGroupAccumulator
accumulate_material_glass_effect_union_group(
        std::span<MaterialRuntimeRecord const> records,
        MaterialPlan const& anchor) noexcept {
    MaterialContainerGroupAccumulator group{
        .container_id = anchor.container.container_id,
    };
    if (!material_plan_in_glass_effect_union_group(anchor, anchor))
        return group;
    for (auto const& candidate_record : records) {
        auto const& candidate = candidate_record.plan;
        if (!material_plan_in_glass_effect_union_group(candidate, anchor))
            continue;
        ++group.surface_count;
        accumulate_material_container_bounds(group, candidate);
        if (candidate.primary_pass.active)
            ++group.active_surfaces;
        if (candidate.backdrop_sampling)
            ++group.sampled_backdrop_surfaces;
        if (candidate.fallback())
            ++group.fallback_surfaces;
        if (candidate.container.shape_union_expected)
            ++group.union_surfaces;
        if (candidate.container.morph_transitions)
            ++group.morph_surfaces;
        if (candidate.container.interactive)
            ++group.interactive_surfaces;
        if (candidate.container.shared_backdrop_scope)
            ++group.shared_backdrop_scope_surfaces;
    }
    for (std::size_t left = 0; left < records.size(); ++left) {
        auto const& a = records[left].plan;
        if (!material_plan_in_glass_effect_union_group(a, anchor))
            continue;
        for (std::size_t right = left + 1; right < records.size(); ++right) {
            auto const& b = records[right].plan;
            if (!material_plan_in_glass_effect_union_group(b, anchor))
                continue;
            accumulate_material_container_pair(group, a, b);
        }
    }
    return group;
}

inline bool material_glass_effect_union_paint_layer_leader(
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records) noexcept {
    auto const& plan = record.plan;
    if (!material_plan_in_glass_effect_union_group(plan, plan)
        || material_plan_uses_sampled_backdrop_executor(plan)) {
        return true;
    }

    auto found = false;
    auto leader_command_index = record.command_index;
    for (auto const& candidate_record : records) {
        auto const& candidate = candidate_record.plan;
        if (!candidate.primary_pass.active
            || material_plan_uses_sampled_backdrop_executor(candidate)
            || !material_plan_in_glass_effect_union_group(candidate, plan)) {
            continue;
        }
        if (!found || candidate_record.command_index < leader_command_index) {
            leader_command_index = candidate_record.command_index;
            found = true;
        }
    }
    return !found || record.command_index == leader_command_index;
}

inline bool material_glass_effect_union_surface_leader(
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records) noexcept {
    auto const& plan = record.plan;
    if (!material_plan_in_glass_effect_union_group(plan, plan)
        || !material_plan_uses_sampled_backdrop_executor(plan)) {
        return true;
    }

    auto found = false;
    auto leader_command_index = record.command_index;
    for (auto const& candidate_record : records) {
        auto const& candidate = candidate_record.plan;
        if (!candidate.primary_pass.active
            || !material_plan_uses_sampled_backdrop_executor(candidate)
            || !material_plan_in_glass_effect_union_group(candidate, plan)) {
            continue;
        }
        if (!found || candidate_record.command_index < leader_command_index) {
            leader_command_index = candidate_record.command_index;
            found = true;
        }
    }
    return !found || record.command_index == leader_command_index;
}

inline MaterialGlassEffectMatchAccumulator
accumulate_material_glass_effect_match_group(
        std::span<MaterialRuntimeRecord const> records,
        MaterialPlan const& anchor,
        std::uint32_t container_id) noexcept {
    MaterialGlassEffectMatchAccumulator group{
        .namespace_id = anchor.glass_identity.namespace_id,
        .effect_id = anchor.glass_identity.effect_id,
        .container_id = container_id,
    };
    if (!anchor.glass_identity.participates
        || !anchor.glass_identity.matched_geometry_candidate
        || container_id == 0u) {
        return group;
    }
    for (auto const& candidate_record : records) {
        auto const& candidate = candidate_record.plan;
        if (!material_plan_in_glass_effect_match_group(
                candidate,
                anchor,
                container_id)) {
            continue;
        }
        ++group.surface_count;
        accumulate_material_glass_effect_match_bounds(group, candidate);
        if (candidate.glass_identity.matched_geometry_candidate) {
            ++group.matched_geometry_surfaces;
            group.max_progress = std::max(
                group.max_progress,
                candidate.transition.progress);
        }
    }
    return group;
}

inline bool material_glass_effect_match_source_candidate(
        MaterialRuntimeRecord const& record,
        MaterialRuntimeRecord const& candidate,
        std::uint32_t container_id) noexcept {
    if (&record == &candidate)
        return false;
    return material_plan_in_glass_effect_match_group(
        candidate.plan,
        record.plan,
        container_id);
}

inline MaterialGlassEffectMatchSource material_nearest_glass_effect_match_source(
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records,
        std::uint32_t container_id) noexcept {
    auto constexpr gap_epsilon = 0.0001f;
    MaterialGlassEffectMatchSource best{};
    for (auto const& candidate : records) {
        if (!material_glass_effect_match_source_candidate(
                record,
                candidate,
                container_id)) {
            continue;
        }
        auto const gap = material_rect_gap(
            record.plan.geometry,
            candidate.plan.geometry);
        auto const spacing = std::min(
            record.plan.container.blend_distance,
            candidate.plan.container.blend_distance);
        auto const proximity =
            material_container_shape_blend_strength_for_gap(gap, spacing);
        if (proximity <= 0.0f)
            continue;
        auto const effect_id_match =
            material_plans_share_glass_effect_id(candidate.plan, record.plan);
        auto const better = !best.valid()
            || (effect_id_match && !best.effect_id_match)
            || (effect_id_match == best.effect_id_match
                && (gap < best.gap - gap_epsilon
                    || (std::fabs(gap - best.gap) <= gap_epsilon
                        && candidate.command_index
                            < best.record->command_index)));
        if (better) {
            best.record = &candidate;
            best.gap = gap;
            best.spacing = spacing;
            best.proximity = proximity;
            best.effect_id_match = effect_id_match;
        }
    }
    return best;
}

inline MaterialGeometry material_glass_effect_match_rect(
        MaterialGeometry const& source,
        MaterialGeometry const& target,
        MaterialTransitionAnalysis const& transition) noexcept {
    auto const progress = transition.appearing
        ? transition.progress
        : 1.0f - transition.progress;
    auto const t = material_glass_effect_match_progress_gain(progress);
    return MaterialGeometry{
        material_lerp(source.x, target.x, t),
        material_lerp(source.y, target.y, t),
        std::max(0.0f, material_lerp(source.w, target.w, t)),
        std::max(0.0f, material_lerp(source.h, target.h, t)),
        std::max(0.0f, material_lerp(source.radius, target.radius, t)),
    };
}

inline float material_glass_effect_match_appearance_blend(
        MaterialTransitionAnalysis const& transition) noexcept {
    auto const progress = transition.appearing
        ? transition.progress
        : 1.0f - transition.progress;
    return material_glass_effect_match_progress_gain(progress);
}

inline void material_apply_glass_effect_match_appearance_source(
        MaterialContainerExecutionDescriptor& execution,
        MaterialRuntimeRecord const& target,
        MaterialRuntimeRecord const& source) noexcept {
    auto const& source_plan = source.plan;
    auto const& target_plan = target.plan;
    auto const blend =
        material_glass_effect_match_appearance_blend(target_plan.transition);
    if (!target_plan.transition.active || blend >= 0.999f)
        return;

    execution.glass_effect_match_appearance_active = true;
    execution.glass_effect_match_appearance_source_command_index =
        source.command_index;
    execution.glass_effect_match_appearance_blend = blend;

    execution.glass_effect_match_appearance_tint_active =
        source_plan.tint != target_plan.tint;
    execution.glass_effect_match_appearance_tint_r =
        static_cast<float>(source_plan.tint.r) / 255.0f;
    execution.glass_effect_match_appearance_tint_g =
        static_cast<float>(source_plan.tint.g) / 255.0f;
    execution.glass_effect_match_appearance_tint_b =
        static_cast<float>(source_plan.tint.b) / 255.0f;
    execution.glass_effect_match_appearance_tint_a =
        static_cast<float>(source_plan.tint.a) / 255.0f;

    execution.glass_effect_match_appearance_spectral_tint_active =
        source_plan.spectral_tint.active || target_plan.spectral_tint.active;
    execution.glass_effect_match_appearance_spectral_tint_warmth =
        source_plan.spectral_tint.warmth;
    execution.glass_effect_match_appearance_spectral_tint_coolness =
        source_plan.spectral_tint.coolness;
    execution.glass_effect_match_appearance_spectral_tint_dispersion =
        source_plan.spectral_tint.dispersion;
    execution.glass_effect_match_appearance_spectral_tint_rim =
        source_plan.spectral_tint.rim_tint;

    execution.glass_effect_match_appearance_prominent_glass_active =
        source_plan.prominent_glass.active || target_plan.prominent_glass.active;
    execution.glass_effect_match_appearance_prominent_glass_intensity =
        source_plan.prominent_glass.intensity;
    execution.glass_effect_match_appearance_prominent_glass_tint_weight =
        source_plan.prominent_glass.tint_weight;
    execution.glass_effect_match_appearance_prominent_glass_edge_lift =
        source_plan.prominent_glass.edge_lift;
    execution.glass_effect_match_appearance_prominent_glass_lensing_gain =
        source_plan.prominent_glass.lensing_gain;

    execution.glass_effect_match_appearance_clear_glass_active =
        source_plan.clear_glass_legibility.active
        || target_plan.clear_glass_legibility.active;
    execution.glass_effect_match_appearance_clear_glass_dimming =
        source_plan.clear_glass_legibility.dimming_strength;
    execution.glass_effect_match_appearance_clear_glass_contrast =
        source_plan.clear_glass_legibility.contrast_lift;
    execution.glass_effect_match_appearance_clear_glass_brightness_response =
        source_plan.clear_glass_legibility.brightness_response;
    execution.glass_effect_match_appearance_clear_glass_detail_response =
        source_plan.clear_glass_legibility.detail_response;

    execution.glass_effect_match_appearance_refraction_active =
        source_plan.refraction.active || target_plan.refraction.active;
    execution.glass_effect_match_appearance_refraction_strength =
        source_plan.refraction.strength;
    execution.glass_effect_match_appearance_refraction_edge_bias =
        source_plan.refraction.edge_bias;
    execution.glass_effect_match_appearance_refraction_max_offset_pixels =
        source_plan.refraction.max_offset_pixels;
    execution
        .glass_effect_match_appearance_refraction_edge_caustic_intensity =
            source_plan.refraction.edge_caustic_intensity;

    execution.glass_effect_match_appearance_dynamic_lighting_active =
        source_plan.dynamic_lighting.active
        || target_plan.dynamic_lighting.active;
    execution.glass_effect_match_appearance_dynamic_light_direction_x =
        source_plan.dynamic_lighting.direction_x;
    execution.glass_effect_match_appearance_dynamic_light_direction_y =
        source_plan.dynamic_lighting.direction_y;
    execution.glass_effect_match_appearance_dynamic_light_highlight =
        source_plan.dynamic_lighting.highlight_strength;
    execution.glass_effect_match_appearance_dynamic_light_shadow =
        source_plan.dynamic_lighting.shadow_strength;

    execution.glass_effect_match_appearance_glass_thickness_active =
        source_plan.glass_thickness.active
        || target_plan.glass_thickness.active;
    execution.glass_effect_match_appearance_glass_thickness =
        source_plan.glass_thickness.thickness;
    execution.glass_effect_match_appearance_glass_lensing_gain =
        source_plan.glass_thickness.lensing_gain;
    execution.glass_effect_match_appearance_glass_shadow_gain =
        source_plan.glass_thickness.shadow_gain;
    execution.glass_effect_match_appearance_glass_scattering_gain =
        source_plan.glass_thickness.scattering_gain;

    execution.glass_effect_match_appearance_glass_dispersion_active =
        source_plan.glass_dispersion.active
        || target_plan.glass_dispersion.active;
    execution.glass_effect_match_appearance_glass_dispersion_axial_offset =
        source_plan.glass_dispersion.axial_offset_pixels;
    execution
        .glass_effect_match_appearance_glass_dispersion_tangential_offset =
            source_plan.glass_dispersion.tangential_offset_pixels;
    execution.glass_effect_match_appearance_glass_dispersion_prismatic_gain =
        source_plan.glass_dispersion.prismatic_gain;
    execution.glass_effect_match_appearance_glass_dispersion_caustic_spread =
        source_plan.glass_dispersion.caustic_spread;
}

inline MaterialGlassEffectMotionOptics material_glass_effect_match_motion_optics(
        MaterialContainerExecutionDescriptor const& execution) noexcept {
    MaterialGlassEffectMotionOptics optics{};
    if (!execution.glass_effect_match_execution
        || !execution.surface_leader
        || !execution.glass_effect_match_source_valid
        || execution.glass_effect_match_rect_w <= 0.0f
        || execution.glass_effect_match_rect_h <= 0.0f) {
        return optics;
    }

    auto const source_center_x =
        execution.glass_effect_match_source_x
        + execution.glass_effect_match_source_w * 0.5f;
    auto const source_center_y =
        execution.glass_effect_match_source_y
        + execution.glass_effect_match_source_h * 0.5f;
    auto const match_center_x =
        execution.glass_effect_match_rect_x
        + execution.glass_effect_match_rect_w * 0.5f;
    auto const match_center_y =
        execution.glass_effect_match_rect_y
        + execution.glass_effect_match_rect_h * 0.5f;
    auto const dx = match_center_x - source_center_x;
    auto const dy = match_center_y - source_center_y;
    auto const length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f)
        return optics;

    auto const proximity = std::clamp(
        execution.glass_effect_match_source_proximity,
        0.0f,
        1.0f);
    auto const blend = std::clamp(
        execution.glass_effect_match_blend_strength,
        0.0f,
        1.0f);
    auto const identity_coherence =
        execution.glass_effect_match_source_effect_id_match ? blend : 0.0f;
    auto const strength = std::clamp(
        blend * (0.65f + 0.35f * proximity)
            + 0.10f * identity_coherence,
        0.0f,
        1.0f);
    if (strength <= 0.0001f)
        return optics;

    optics.active = true;
    optics.strength = strength;
    optics.direction_x = dx / length;
    optics.direction_y = dy / length;
    optics.specular_anchor_x = std::clamp(
        0.5f + optics.direction_x * 0.22f * strength,
        0.18f,
        0.82f);
    optics.specular_anchor_y = std::clamp(
        0.5f + optics.direction_y * 0.22f * strength,
        0.18f,
        0.82f);
    optics.refraction_gain =
        1.0f + 0.45f * strength + 0.08f * identity_coherence;
    optics.caustic_gain =
        1.0f + 0.70f * strength + 0.12f * identity_coherence;
    optics.specular_intensity_gain =
        1.0f + 0.55f * strength + 0.10f * identity_coherence;
    optics.flow_offset_gain =
        0.18f + 0.34f * strength + 0.06f * identity_coherence;
    optics.ribbon_width =
        0.12f + 0.18f * strength + 0.04f * identity_coherence;
    optics.highlight_gain =
        0.08f + 0.16f * strength + 0.05f * identity_coherence;
    return optics;
}

inline bool material_container_bridge_motion_candidate(
        MaterialRuntimeRecord const& record,
        MaterialRuntimeRecord const& candidate,
        MaterialContainerExecutionDescriptor const& execution) noexcept {
    if (&record == &candidate)
        return false;
    if (!candidate.plan.primary_pass.active
        || !candidate.plan.shape.valid
        || !material_plan_in_container(
            candidate.plan,
            execution.container_id)) {
        return false;
    }
    if (execution.union_execution) {
        return material_plan_in_glass_effect_union_group(
            candidate.plan,
            record.plan);
    }
    return material_container_shape_blend_edge(
        record.plan,
        candidate.plan,
        execution.container_id);
}

inline MaterialGlassEffectMotionOptics material_container_bridge_motion_optics(
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records,
        MaterialContainerExecutionDescriptor const& execution) noexcept {
    MaterialGlassEffectMotionOptics optics{};
    if (execution.glass_effect_match_execution
        || !execution.shape_blend_execution
        || !execution.fusion_optics_active
        || !record.plan.primary_pass.active
        || !record.plan.shape.valid) {
        return optics;
    }

    auto const geometry =
        material_surface_execution_geometry(record.plan, &execution);
    if (!geometry.active || geometry.w <= 0.0f || geometry.h <= 0.0f)
        return optics;

    auto const record_center_x =
        record.plan.geometry.x + record.plan.geometry.w * 0.5f;
    auto const record_center_y =
        record.plan.geometry.y + record.plan.geometry.h * 0.5f;
    auto const geometry_center_x = geometry.x + geometry.w * 0.5f;
    auto const geometry_center_y = geometry.y + geometry.h * 0.5f;

    auto best_pair_strength = 0.0f;
    auto best_gap = 0.0f;
    auto best_candidate_center_x = record_center_x;
    auto best_candidate_center_y = record_center_y;
    auto const record_weight = std::max(
        std::max(record.plan.geometry.w, 0.0f)
            * std::max(record.plan.geometry.h, 0.0f),
        1.0f);
    auto union_weight = execution.union_execution ? record_weight : 0.0f;
    auto union_center_x = execution.union_execution
        ? record_center_x * record_weight
        : 0.0f;
    auto union_center_y = execution.union_execution
        ? record_center_y * record_weight
        : 0.0f;
    auto found = false;
    for (auto const& candidate : records) {
        if (!material_container_bridge_motion_candidate(
                record,
                candidate,
                execution)) {
            continue;
        }
        auto const gap = material_rect_gap(record.plan.geometry,
                                           candidate.plan.geometry);
        auto const blend_distance = std::min(
            record.plan.container.blend_distance,
            candidate.plan.container.blend_distance);
        auto const pair_strength = execution.union_execution
            ? 1.0f
            : material_container_shape_blend_strength_for_gap(
                gap,
                blend_distance);
        if (pair_strength <= 0.0f)
            continue;
        if (execution.union_execution) {
            auto const candidate_w = std::max(candidate.plan.geometry.w, 0.0f);
            auto const candidate_h = std::max(candidate.plan.geometry.h, 0.0f);
            auto const candidate_weight =
                std::max(candidate_w * candidate_h, 1.0f)
                * pair_strength;
            auto const candidate_center_x =
                candidate.plan.geometry.x + candidate_w * 0.5f;
            auto const candidate_center_y =
                candidate.plan.geometry.y + candidate_h * 0.5f;
            union_weight += candidate_weight;
            union_center_x += candidate_center_x * candidate_weight;
            union_center_y += candidate_center_y * candidate_weight;
            found = true;
            best_pair_strength = std::max(
                best_pair_strength,
                pair_strength);
            continue;
        }
        auto const better =
            !found
            || pair_strength > best_pair_strength + 0.0001f
            || (std::fabs(pair_strength - best_pair_strength) <= 0.0001f
                && gap < best_gap);
        if (!better)
            continue;
        found = true;
        best_pair_strength = pair_strength;
        best_gap = gap;
        best_candidate_center_x =
            candidate.plan.geometry.x + candidate.plan.geometry.w * 0.5f;
        best_candidate_center_y =
            candidate.plan.geometry.y + candidate.plan.geometry.h * 0.5f;
    }
    if (!found)
        return optics;
    if (execution.union_execution && union_weight > 0.0001f) {
        best_candidate_center_x = union_center_x / union_weight;
        best_candidate_center_y = union_center_y / union_weight;
    }

    auto const bridge_x = execution.union_execution
        ? best_candidate_center_x
        : (record_center_x + best_candidate_center_x) * 0.5f;
    auto const bridge_y = execution.union_execution
        ? best_candidate_center_y
        : (record_center_y + best_candidate_center_y) * 0.5f;
    auto dx = bridge_x - geometry_center_x;
    auto dy = bridge_y - geometry_center_y;
    auto length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f) {
        dx = best_candidate_center_x - record_center_x;
        dy = best_candidate_center_y - record_center_y;
        length = std::sqrt(dx * dx + dy * dy);
    }
    if (length <= 0.0001f)
        return optics;

    auto const fusion = std::clamp(execution.fusion_strength, 0.0f, 1.0f);
    auto const strength = std::clamp(
        fusion * (0.55f + 0.45f * best_pair_strength),
        0.0f,
        1.0f);
    if (strength <= 0.0001f)
        return optics;
    auto const member_spread = execution.union_execution
        ? std::clamp(
            (static_cast<float>(execution.surface_count) - 2.0f) / 4.0f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const group_min_extent =
        std::max(std::min(geometry.w, geometry.h), 1.0f);
    auto const group_max_extent = std::max(geometry.w, geometry.h);
    auto const aspect_spread = execution.union_execution
        ? std::clamp(
            (group_max_extent / group_min_extent - 1.0f) / 4.0f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const union_flow_spread =
        std::clamp(0.50f * member_spread + 0.50f * aspect_spread,
                   0.0f,
                   1.0f);

    optics.active = true;
    optics.strength = strength;
    optics.direction_x = dx / length;
    optics.direction_y = dy / length;
    optics.specular_anchor_x = std::clamp(
        (bridge_x - geometry.x) / geometry.w,
        0.16f,
        0.84f);
    optics.specular_anchor_y = std::clamp(
        (bridge_y - geometry.y) / geometry.h,
        0.16f,
        0.84f);
    optics.refraction_gain = 1.0f + 0.32f * strength
        + 0.06f * union_flow_spread;
    optics.caustic_gain = 1.0f + 0.48f * strength
        + 0.10f * union_flow_spread;
    optics.specular_intensity_gain = 1.0f + 0.42f * strength
        + 0.08f * union_flow_spread;
    optics.flow_offset_gain = 0.14f + 0.30f * strength
        + 0.08f * union_flow_spread;
    optics.ribbon_width = 0.10f + 0.16f * strength
        + 0.05f * union_flow_spread;
    optics.highlight_gain = 0.06f + 0.14f * strength
        + 0.05f * union_flow_spread;
    return optics;
}

inline bool material_container_group_interaction_candidate(
        MaterialRuntimeRecord const& record,
        MaterialRuntimeRecord const& candidate,
        MaterialContainerExecutionDescriptor const& execution,
        MaterialContainerGroupAccumulator const& shape_blend_cluster)
        noexcept {
    auto const& plan = candidate.plan;
    if (!plan.primary_pass.active
        || !plan.shape.valid
        || !material_plan_in_container(plan, execution.container_id)
        || (!plan.specular.interaction_driven
            && !plan.interaction.pointer_lens_active)) {
        return false;
    }
    if (execution.union_execution) {
        return material_plan_in_glass_effect_union_group(
            plan,
            record.plan);
    }
    if (execution.group_surface_execution) {
        return material_plan_uses_group_surface_merge_executor(plan)
            && material_container_plan_inside_cluster_bounds(
                plan,
                shape_blend_cluster);
    }
    return false;
}

inline float material_container_group_interaction_score(
        MaterialPlan const& plan) noexcept {
    auto score = 0.0f;
    if (plan.specular.interaction_driven) {
        score += 1.0f
            + std::clamp(plan.specular.intensity, 0.0f, 1.0f)
            + std::clamp(plan.interaction.response_strength, 0.0f, 1.0f)
                * 0.25f;
    }
    if (plan.interaction.pointer_lens_active) {
        score += 2.0f
            + std::clamp(
                plan.interaction.pointer_lens_strength,
                0.0f,
                1.0f);
    }
    return score;
}

inline void material_apply_container_group_interaction_source(
        MaterialContainerExecutionDescriptor& execution,
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records,
        MaterialContainerGroupAccumulator const& shape_blend_cluster)
        noexcept {
    if (!execution.shape_blend_execution
        || !execution.surface_leader
        || !execution.group_bounds_valid
        || (!execution.union_execution
            && !execution.group_surface_execution)) {
        return;
    }

    auto const geometry =
        material_surface_execution_geometry(record.plan, &execution);
    if (!geometry.active)
        return;

    MaterialRuntimeRecord const* source = nullptr;
    auto best_score = 0.0f;
    for (auto const& candidate : records) {
        if (!material_container_group_interaction_candidate(
                record,
                candidate,
                execution,
                shape_blend_cluster)) {
            continue;
        }
        auto const score =
            material_container_group_interaction_score(candidate.plan);
        auto const better =
            source == nullptr
            || score > best_score + 0.0001f
            || (std::fabs(score - best_score) <= 0.0001f
                && candidate.command_index < source->command_index);
        if (!better)
            continue;
        source = &candidate;
        best_score = score;
    }
    if (!source)
        return;

    auto const& source_plan = source->plan;
    execution.group_interaction_source_valid = true;
    execution.group_interaction_source_command_index =
        source->command_index;
    if (source_plan.specular.interaction_driven) {
        execution.group_interaction_specular_anchor_x =
            material_surface_execution_anchor_x(
                source_plan,
                geometry,
                source_plan.specular.anchor_x);
        execution.group_interaction_specular_anchor_y =
            material_surface_execution_anchor_y(
                source_plan,
                geometry,
                source_plan.specular.anchor_y);
        execution.group_interaction_specular_radius =
            source_plan.specular.radius;
        execution.group_interaction_specular_intensity =
            source_plan.specular.intensity;
    }
    if (source_plan.interaction.pointer_lens_active) {
        execution.group_interaction_pointer_lens_active = true;
        execution.group_interaction_pointer_lens_anchor_x =
            material_surface_execution_anchor_x(
                source_plan,
                geometry,
                source_plan.interaction.pointer_lens_anchor_x);
        execution.group_interaction_pointer_lens_anchor_y =
            material_surface_execution_anchor_y(
                source_plan,
                geometry,
                source_plan.interaction.pointer_lens_anchor_y);
        execution.group_interaction_pointer_lens_radius =
            source_plan.interaction.pointer_lens_radius;
        execution.group_interaction_pointer_lens_strength =
            source_plan.interaction.pointer_lens_strength;
    }
    if (source_plan.interaction.control_morph_active) {
        execution.group_interaction_control_morph_active = true;
        execution.group_interaction_control_morph_scale_delta =
            source_plan.interaction.control_morph_scale_delta;
        execution.group_interaction_control_morph_depth =
            source_plan.interaction.control_morph_depth;
        execution.group_interaction_control_morph_edge =
            source_plan.interaction.control_morph_edge;
        execution.group_interaction_control_morph_shadow =
            source_plan.interaction.control_morph_shadow;
    }
    if (source_plan.refraction.active
        && source_plan.refraction.interaction_driven) {
        execution.group_interaction_refraction_active = true;
        execution.group_interaction_refraction_strength =
            source_plan.refraction.strength;
        execution.group_interaction_refraction_edge_bias =
            source_plan.refraction.edge_bias;
        execution.group_interaction_refraction_max_offset_pixels =
            source_plan.refraction.max_offset_pixels;
        execution.group_interaction_refraction_edge_caustic_intensity =
            source_plan.refraction.edge_caustic_intensity;
    }
    if (source_plan.dynamic_lighting.active
        && source_plan.dynamic_lighting.interaction_driven) {
        execution.group_interaction_dynamic_lighting_active = true;
        execution.group_interaction_dynamic_light_direction_x =
            source_plan.dynamic_lighting.direction_x;
        execution.group_interaction_dynamic_light_direction_y =
            source_plan.dynamic_lighting.direction_y;
        execution.group_interaction_dynamic_light_highlight =
            source_plan.dynamic_lighting.highlight_strength;
        execution.group_interaction_dynamic_light_shadow =
            source_plan.dynamic_lighting.shadow_strength;
    }
    if (source_plan.glass_thickness.active
        && source_plan.glass_thickness.interaction_driven) {
        execution.group_interaction_glass_thickness_active = true;
        execution.group_interaction_glass_thickness =
            source_plan.glass_thickness.thickness;
        execution.group_interaction_glass_lensing_gain =
            source_plan.glass_thickness.lensing_gain;
        execution.group_interaction_glass_shadow_gain =
            source_plan.glass_thickness.shadow_gain;
        execution.group_interaction_glass_scattering_gain =
            source_plan.glass_thickness.scattering_gain;
    }
    if (source_plan.glass_dispersion.active
        && source_plan.glass_dispersion.interaction_driven) {
        execution.group_interaction_glass_dispersion_active = true;
        execution.group_interaction_glass_dispersion_axial_offset =
            source_plan.glass_dispersion.axial_offset_pixels;
        execution.group_interaction_glass_dispersion_tangential_offset =
            source_plan.glass_dispersion.tangential_offset_pixels;
        execution.group_interaction_glass_dispersion_prismatic_gain =
            source_plan.glass_dispersion.prismatic_gain;
        execution.group_interaction_glass_dispersion_caustic_spread =
            source_plan.glass_dispersion.caustic_spread;
    }
}

inline bool material_container_group_appearance_candidate(
        MaterialRuntimeRecord const& record,
        MaterialRuntimeRecord const& candidate,
        MaterialContainerExecutionDescriptor const& execution,
        MaterialContainerGroupAccumulator const& shape_blend_cluster)
        noexcept {
    auto const& plan = candidate.plan;
    if (!plan.primary_pass.active
        || !plan.shape.valid
        || !material_plan_in_container(plan, execution.container_id)) {
        return false;
    }
    if (execution.union_execution) {
        return material_plan_in_glass_effect_union_group(
            plan,
            record.plan);
    }
    if (execution.group_surface_execution) {
        return material_plan_uses_group_surface_merge_executor(plan)
            && material_container_plan_inside_cluster_bounds(
                plan,
                shape_blend_cluster);
    }
    return false;
}

inline float material_container_appearance_tint_score(
        MaterialPlan const& plan) noexcept {
    auto const red = static_cast<float>(plan.tint.r) / 255.0f;
    auto const green = static_cast<float>(plan.tint.g) / 255.0f;
    auto const blue = static_cast<float>(plan.tint.b) / 255.0f;
    auto const chroma = std::max(
        std::max(std::fabs(red - green), std::fabs(green - blue)),
        std::fabs(blue - red));
    auto const alpha = material_alpha_fraction(plan.tint);
    return alpha * (0.20f + chroma)
        + (plan.theme.tint_matches_surface ? 0.0f : 1.0f);
}

inline float material_container_group_appearance_score(
        MaterialPlan const& plan) noexcept {
    auto score = material_container_appearance_tint_score(plan);
    if (plan.spectral_tint.active) {
        score += std::max(
            plan.spectral_tint.warmth,
            plan.spectral_tint.coolness)
            + plan.spectral_tint.dispersion
            + 0.75f * plan.spectral_tint.rim_tint
            + (plan.spectral_tint.tint_driven ? 0.50f : 0.0f);
    }
    if (plan.prominent_glass.active) {
        score += 1.25f
            + 1.50f * std::clamp(plan.prominent_glass.intensity, 0.0f, 1.0f)
            + plan.prominent_glass.tint_weight
            + plan.prominent_glass.edge_lift
            + std::max(0.0f, plan.prominent_glass.lensing_gain - 1.0f);
    }
    if (plan.clear_glass_legibility.active) {
        score += 0.75f
            + plan.clear_glass_legibility.dimming_strength
            + plan.clear_glass_legibility.contrast_lift
            + 0.25f * plan.clear_glass_legibility.brightness_response
            + 0.25f * plan.clear_glass_legibility.detail_response;
    }
    return score;
}

inline void material_apply_container_group_appearance_source(
        MaterialContainerExecutionDescriptor& execution,
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records,
        MaterialContainerGroupAccumulator const& shape_blend_cluster)
        noexcept {
    if (!execution.shape_blend_execution
        || !execution.surface_leader
        || !execution.group_bounds_valid
        || (!execution.union_execution
            && !execution.group_surface_execution)) {
        return;
    }

    auto const geometry =
        material_surface_execution_geometry(record.plan, &execution);
    if (!geometry.active)
        return;

    MaterialRuntimeRecord const* source = nullptr;
    auto best_score = 0.0f;
    auto best_tint_score = 0.0f;
    for (auto const& candidate : records) {
        if (candidate.command_index == record.command_index
            || !material_container_group_appearance_candidate(
                record,
                candidate,
                execution,
                shape_blend_cluster)) {
            continue;
        }
        auto const& plan = candidate.plan;
        auto const score = material_container_group_appearance_score(plan);
        auto const better_source =
            score > 0.0001f
            && (source == nullptr
                || score > best_score + 0.0001f
                || (std::fabs(score - best_score) <= 0.0001f
                    && candidate.command_index < source->command_index));
        if (better_source) {
            source = &candidate;
            best_score = score;
        }

        auto const tint_score =
            material_container_appearance_tint_score(plan);
        if (tint_score > best_tint_score + 0.0001f) {
            best_tint_score = tint_score;
            execution.group_appearance_tint_active = true;
            execution.group_appearance_tint_r =
                static_cast<float>(plan.tint.r) / 255.0f;
            execution.group_appearance_tint_g =
                static_cast<float>(plan.tint.g) / 255.0f;
            execution.group_appearance_tint_b =
                static_cast<float>(plan.tint.b) / 255.0f;
            execution.group_appearance_tint_a =
                static_cast<float>(plan.tint.a) / 255.0f;
        }
        if (plan.spectral_tint.active) {
            execution.group_appearance_spectral_tint_active = true;
            execution.group_appearance_spectral_tint_warmth = std::max(
                execution.group_appearance_spectral_tint_warmth,
                plan.spectral_tint.warmth);
            execution.group_appearance_spectral_tint_coolness = std::max(
                execution.group_appearance_spectral_tint_coolness,
                plan.spectral_tint.coolness);
            execution.group_appearance_spectral_tint_dispersion = std::max(
                execution.group_appearance_spectral_tint_dispersion,
                plan.spectral_tint.dispersion);
            execution.group_appearance_spectral_tint_rim = std::max(
                execution.group_appearance_spectral_tint_rim,
                plan.spectral_tint.rim_tint);
        }
        if (plan.prominent_glass.active) {
            execution.group_appearance_prominent_glass_active = true;
            execution.group_appearance_prominent_glass_intensity = std::max(
                execution.group_appearance_prominent_glass_intensity,
                plan.prominent_glass.intensity);
            execution.group_appearance_prominent_glass_tint_weight = std::max(
                execution.group_appearance_prominent_glass_tint_weight,
                plan.prominent_glass.tint_weight);
            execution.group_appearance_prominent_glass_edge_lift = std::max(
                execution.group_appearance_prominent_glass_edge_lift,
                plan.prominent_glass.edge_lift);
            execution.group_appearance_prominent_glass_lensing_gain =
                std::max(
                    execution.group_appearance_prominent_glass_lensing_gain,
                    plan.prominent_glass.lensing_gain);
        }
        if (plan.clear_glass_legibility.active) {
            execution.group_appearance_clear_glass_active = true;
            execution.group_appearance_clear_glass_dimming = std::max(
                execution.group_appearance_clear_glass_dimming,
                plan.clear_glass_legibility.dimming_strength);
            execution.group_appearance_clear_glass_contrast = std::max(
                execution.group_appearance_clear_glass_contrast,
                plan.clear_glass_legibility.contrast_lift);
            execution.group_appearance_clear_glass_brightness_response =
                std::max(
                    execution.group_appearance_clear_glass_brightness_response,
                    plan.clear_glass_legibility.brightness_response);
            execution.group_appearance_clear_glass_detail_response = std::max(
                execution.group_appearance_clear_glass_detail_response,
                plan.clear_glass_legibility.detail_response);
        }
        if (plan.refraction.active) {
            execution.group_appearance_refraction_active = true;
            execution.group_appearance_refraction_strength = std::max(
                execution.group_appearance_refraction_strength,
                plan.refraction.strength);
            execution.group_appearance_refraction_edge_bias = std::max(
                execution.group_appearance_refraction_edge_bias,
                plan.refraction.edge_bias);
            execution.group_appearance_refraction_max_offset_pixels = std::max(
                execution.group_appearance_refraction_max_offset_pixels,
                plan.refraction.max_offset_pixels);
            execution.group_appearance_refraction_edge_caustic_intensity =
                std::max(
                    execution
                        .group_appearance_refraction_edge_caustic_intensity,
                    plan.refraction.edge_caustic_intensity);
        }
        if (plan.dynamic_lighting.active) {
            auto const current_lighting_energy =
                execution.group_appearance_dynamic_light_highlight
                + execution.group_appearance_dynamic_light_shadow;
            auto const candidate_lighting_energy =
                plan.dynamic_lighting.highlight_strength
                + plan.dynamic_lighting.shadow_strength;
            if (!execution.group_appearance_dynamic_lighting_active
                || candidate_lighting_energy
                    > current_lighting_energy + 0.0001f) {
                execution.group_appearance_dynamic_light_direction_x =
                    plan.dynamic_lighting.direction_x;
                execution.group_appearance_dynamic_light_direction_y =
                    plan.dynamic_lighting.direction_y;
            }
            execution.group_appearance_dynamic_lighting_active = true;
            execution.group_appearance_dynamic_light_highlight = std::max(
                execution.group_appearance_dynamic_light_highlight,
                plan.dynamic_lighting.highlight_strength);
            execution.group_appearance_dynamic_light_shadow = std::max(
                execution.group_appearance_dynamic_light_shadow,
                plan.dynamic_lighting.shadow_strength);
        }
        if (plan.glass_thickness.active) {
            execution.group_appearance_glass_thickness_active = true;
            execution.group_appearance_glass_thickness = std::max(
                execution.group_appearance_glass_thickness,
                plan.glass_thickness.thickness);
            execution.group_appearance_glass_lensing_gain = std::max(
                execution.group_appearance_glass_lensing_gain,
                plan.glass_thickness.lensing_gain);
            execution.group_appearance_glass_shadow_gain = std::max(
                execution.group_appearance_glass_shadow_gain,
                plan.glass_thickness.shadow_gain);
            execution.group_appearance_glass_scattering_gain = std::max(
                execution.group_appearance_glass_scattering_gain,
                plan.glass_thickness.scattering_gain);
        }
        if (plan.glass_dispersion.active) {
            execution.group_appearance_glass_dispersion_active = true;
            execution.group_appearance_glass_dispersion_axial_offset =
                std::max(
                    execution.group_appearance_glass_dispersion_axial_offset,
                    plan.glass_dispersion.axial_offset_pixels);
            execution.group_appearance_glass_dispersion_tangential_offset =
                std::max(
                    execution
                        .group_appearance_glass_dispersion_tangential_offset,
                    plan.glass_dispersion.tangential_offset_pixels);
            execution.group_appearance_glass_dispersion_prismatic_gain =
                std::max(
                    execution.group_appearance_glass_dispersion_prismatic_gain,
                    plan.glass_dispersion.prismatic_gain);
            execution.group_appearance_glass_dispersion_caustic_spread =
                std::max(
                    execution.group_appearance_glass_dispersion_caustic_spread,
                    plan.glass_dispersion.caustic_spread);
        }
    }
    if (!source)
        return;

    execution.group_appearance_source_valid = true;
    execution.group_appearance_source_command_index = source->command_index;
}

inline MaterialContainerExecutionDescriptor
material_container_execution_descriptor_from_group(
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records,
        MaterialContainerGroupAccumulator const& group) noexcept {
    MaterialContainerExecutionDescriptor descriptor{};
    descriptor.command_index = record.command_index;
    auto const& plan = record.plan;
    if (!plan.container.participates
        || plan.container.container_id == 0u
        || plan.container.container_id != group.container_id) {
        return descriptor;
    }

    descriptor.container_id = group.container_id;
    descriptor.surface_count = group.surface_count;
    descriptor.glass_namespace_id = plan.glass_identity.namespace_id;
    descriptor.glass_effect_id = plan.glass_identity.effect_id;
    descriptor.active = plan.primary_pass.active && group.surface_count > 0u;
    descriptor.group_bounds_valid = group.has_bounds;
    descriptor.shared_backdrop_scope = plan.container.shared_backdrop_scope
        && group.shared_backdrop_scope_surfaces > 1u;
    auto const glass_group = accumulate_material_glass_effect_match_group(
        records,
        plan,
        group.container_id);
    auto const match_source =
        material_glass_effect_match_execution_active(glass_group)
            ? material_nearest_glass_effect_match_source(
                record,
                records,
                group.container_id)
            : MaterialGlassEffectMatchSource{};
    descriptor.glass_effect_surface_count = glass_group.surface_count;
    descriptor.glass_effect_match_progress =
        glass_group.max_progress > 0.0f ? glass_group.max_progress : 1.0f;
    descriptor.glass_effect_match_blend_strength =
        match_source.valid()
            ? material_glass_effect_match_blend_strength(glass_group)
            : 0.0f;
    descriptor.glass_effect_match_execution =
        descriptor.active
        && match_source.valid();
    descriptor.glass_effect_materialize_execution =
        descriptor.active
        && !descriptor.glass_effect_match_execution
        && material_glass_effect_match_execution_active(glass_group)
        && plan.glass_identity.matched_geometry_candidate
        && plan.container.morph_transitions
        && plan.transition.active
        && plan.transition.matched_geometry;
    if (descriptor.glass_effect_materialize_execution) {
        auto const transition =
            material_transition_as_materialize(plan.transition);
        descriptor.glass_effect_materialize_progress = transition.progress;
        descriptor.glass_effect_materialize_opacity_gain =
            transition.opacity_gain;
        descriptor.glass_effect_materialize_optical_gain =
            transition.optical_gain;
        descriptor.glass_effect_materialize_shadow_gain =
            transition.shadow_gain;
        descriptor.glass_effect_materialize_refraction_gain =
            transition.refraction_gain;
        descriptor.glass_effect_materialize_wave_strength =
            transition.materialize_wave_strength;
    }
    auto const group_shape_blend_execution =
        material_container_group_shape_blend_execution_active(group);
    auto const shape_blend_cluster =
        accumulate_material_container_shape_blend_cluster(
            record,
            records,
            group);
    auto const union_group = accumulate_material_glass_effect_union_group(
        records,
        plan);
    auto const union_group_shape_blend_execution =
        material_glass_effect_union_execution_active(union_group);
    descriptor.shape_blend_execution =
        descriptor.active
        && !descriptor.glass_effect_materialize_execution
        && (union_group_shape_blend_execution
            || group_shape_blend_execution
            || descriptor.glass_effect_match_execution);
    descriptor.union_execution =
        descriptor.shape_blend_execution
        && union_group_shape_blend_execution
        && !descriptor.glass_effect_match_execution;
    descriptor.group_surface_execution =
        descriptor.shape_blend_execution
        && !descriptor.union_execution
        && !descriptor.glass_effect_match_execution
        && material_container_group_shape_blend_execution_active(
            shape_blend_cluster)
        && (shape_blend_cluster.sampled_backdrop_surfaces
                + shape_blend_cluster.fallback_surfaces) > 1u
        && material_plan_uses_group_surface_merge_executor(plan);
    if (descriptor.glass_effect_match_execution) {
        descriptor.surface_leader = material_glass_effect_match_surface_leader(
            record,
            records,
            group.container_id);
    } else if (descriptor.group_surface_execution) {
        descriptor.surface_leader = material_container_shape_blend_surface_leader(
            record,
            records,
            shape_blend_cluster);
    } else {
        descriptor.surface_leader =
            !descriptor.union_execution
            || material_glass_effect_union_surface_leader(record, records);
    }
    descriptor.paint_layer_leader =
        (descriptor.glass_effect_match_execution
            || descriptor.group_surface_execution)
            ? descriptor.surface_leader
            : (!descriptor.union_execution
                || material_glass_effect_union_paint_layer_leader(
                    record,
                    records));
    descriptor.morph_execution =
        descriptor.shape_blend_execution
        && (group.morph_surfaces > 0u
            || descriptor.glass_effect_match_execution);
    descriptor.execution_policy =
        descriptor.glass_effect_match_execution
            ? "glass-effect-matched-geometry"
            : (descriptor.glass_effect_materialize_execution
                ? "glass-effect-materialize"
                : (descriptor.union_execution
                    ? "glass-effect-union"
                    : material_container_execution_policy_name(group)));
    descriptor.shape_blend_strength =
        std::max(
            std::max(
                material_container_member_shape_blend_strength(
                    record,
                    records,
                    group),
                material_glass_effect_union_member_shape_blend_strength(
                    record,
                    records,
                    union_group)),
            descriptor.glass_effect_match_blend_strength);
    descriptor.inner_edge_alpha_blend_strength =
        descriptor.glass_effect_match_execution
            ? std::max(
                descriptor.glass_effect_match_blend_strength,
                material_container_inner_edge_alpha_blend_strength(
                    group,
                    descriptor.shape_blend_strength))
            : material_container_inner_edge_alpha_blend_strength(
                group,
                descriptor.shape_blend_strength);
    auto const& fusion_group =
        descriptor.union_execution ? union_group : group;
    material_apply_container_fusion_optics(descriptor, fusion_group);
    material_apply_container_cohesion_profile(descriptor, fusion_group);
    material_apply_glass_union_response_profile(descriptor, fusion_group);
    if (descriptor.glass_effect_match_execution) {
        if (auto const* source = match_source.record) {
            descriptor.glass_effect_match_source_valid = true;
            descriptor.glass_effect_match_source_gap = match_source.gap;
            descriptor.glass_effect_match_source_spacing = match_source.spacing;
            descriptor.glass_effect_match_source_proximity =
                match_source.proximity;
            descriptor.glass_effect_match_source_effect_id_match =
                match_source.effect_id_match;
            descriptor.glass_effect_match_source_x = source->plan.geometry.x;
            descriptor.glass_effect_match_source_y = source->plan.geometry.y;
            descriptor.glass_effect_match_source_w = source->plan.geometry.w;
            descriptor.glass_effect_match_source_h = source->plan.geometry.h;
            descriptor.glass_effect_match_source_radius =
                source->plan.geometry.radius;
            auto const match_rect = material_glass_effect_match_rect(
                source->plan.geometry,
                plan.geometry,
                plan.transition);
            descriptor.glass_effect_match_rect_x = match_rect.x;
            descriptor.glass_effect_match_rect_y = match_rect.y;
            descriptor.glass_effect_match_rect_w = match_rect.w;
            descriptor.glass_effect_match_rect_h = match_rect.h;
            descriptor.glass_effect_match_rect_radius = match_rect.radius;
            material_apply_glass_effect_match_appearance_source(
                descriptor,
                record,
                *source);
        }
    }
    if (descriptor.glass_effect_match_execution && glass_group.has_bounds) {
        material_apply_glass_effect_match_group_execution_bounds(
            descriptor,
            glass_group);
    } else if (descriptor.union_execution && union_group.has_bounds) {
        material_apply_container_group_execution_bounds(
            descriptor,
            union_group);
    } else if (descriptor.group_surface_execution
               && shape_blend_cluster.has_bounds) {
        material_apply_container_group_execution_bounds(
            descriptor,
            shape_blend_cluster);
    } else if (group.has_bounds) {
        material_apply_container_group_execution_bounds(descriptor, group);
    }
    material_apply_container_group_interaction_source(
        descriptor,
        record,
        records,
        shape_blend_cluster);
    material_apply_container_group_appearance_source(
        descriptor,
        record,
        records,
        shape_blend_cluster);
    return descriptor;
}

inline MaterialContainerExecutionDescriptor
material_container_execution_descriptor(
        MaterialRuntimeRecord const& record,
        std::span<MaterialRuntimeRecord const> records) {
    MaterialContainerExecutionDescriptor descriptor{};
    descriptor.command_index = record.command_index;
    auto const& plan = record.plan;
    if (!plan.container.participates || plan.container.container_id == 0u)
        return descriptor;

    auto const group = accumulate_material_container_group(
        records,
        plan.container.container_id);
    return material_container_execution_descriptor_from_group(
        record,
        records,
        group);
}

inline MaterialContainerGroupRuntimeSummary summarize_material_container_groups(
        std::span<MaterialRuntimeRecord const> records) {
    MaterialContainerGroupRuntimeSummary summary{};
    for (std::size_t index = 0; index < records.size(); ++index) {
        auto const& plan = records[index].plan;
        if (!plan.container.participates || plan.container.container_id == 0u)
            continue;
        auto const container_id = plan.container.container_id;
        auto seen = false;
        for (std::size_t prior = 0; prior < index; ++prior) {
            auto const& prior_plan = records[prior].plan;
            if (prior_plan.container.participates
                && prior_plan.container.container_id == container_id) {
                seen = true;
                break;
            }
        }
        if (seen)
            continue;

        MaterialContainerGroupAccumulator group{
            .container_id = container_id,
        };
        for (auto const& candidate_record : records) {
            auto const& candidate = candidate_record.plan;
            if (!candidate.container.participates
                || candidate.container.container_id != container_id) {
                continue;
            }
            ++group.surface_count;
            accumulate_material_container_bounds(group, candidate);
            if (candidate.primary_pass.active)
                ++group.active_surfaces;
            if (candidate.backdrop_sampling)
                ++group.sampled_backdrop_surfaces;
            if (candidate.fallback())
                ++group.fallback_surfaces;
            if (candidate.container.shape_union_expected)
                ++group.union_surfaces;
            if (candidate.container.morph_transitions)
                ++group.morph_surfaces;
            if (candidate.container.interactive)
                ++group.interactive_surfaces;
            if (candidate.container.shared_backdrop_scope)
                ++group.shared_backdrop_scope_surfaces;
        }
        for (std::size_t left = 0; left < records.size(); ++left) {
            auto const& a = records[left].plan;
            if (!material_plan_in_container(a, container_id))
                continue;
            for (std::size_t right = left + 1; right < records.size(); ++right) {
                auto const& b = records[right].plan;
                if (!material_plan_in_container(b, container_id))
                    continue;
                accumulate_material_container_pair(group, a, b);
            }
        }
        ++summary.group_count;
        summary.max_group_size =
            std::max(summary.max_group_size, group.surface_count);
        summary.max_active_surfaces =
            std::max(summary.max_active_surfaces, group.active_surfaces);
        summary.max_sampled_backdrop_surfaces = std::max(
            summary.max_sampled_backdrop_surfaces,
            group.sampled_backdrop_surfaces);
        summary.max_fallback_surfaces =
            std::max(summary.max_fallback_surfaces, group.fallback_surfaces);
        if (group.surface_count > 1u)
            ++summary.multi_surface_group_count;
        if (group.union_surfaces > 0u)
            ++summary.union_group_count;
        if (group.morph_surfaces > 0u)
            ++summary.morph_group_count;
        if (group.interactive_surfaces > 0u)
            ++summary.interactive_group_count;
        if (group.shared_backdrop_scope_surfaces > 0u)
            ++summary.shared_backdrop_scope_group_count;
        if (group.shared_backdrop_scope_surfaces > 0u) {
            summary.shared_capture_surface_count +=
                group.shared_backdrop_scope_surfaces;
            summary.shared_capture_saved_surface_count +=
                group.shared_backdrop_scope_surfaces > 1u
                    ? group.shared_backdrop_scope_surfaces - 1u
                    : 0u;
            summary.max_shared_capture_group_surfaces = std::max(
                summary.max_shared_capture_group_surfaces,
                group.shared_backdrop_scope_surfaces);
        }
        if (material_container_group_shape_blend_execution_active(group)) {
            ++summary.shape_blend_execution_group_count;
            summary.shape_blend_execution_surface_count +=
                material_container_group_shape_blend_surface_count(
                    records,
                    group);
            summary.max_shape_blend_strength = std::max(
                summary.max_shape_blend_strength,
                material_container_group_shape_blend_strength(group));
        }
        if (group.fallback_surfaces > 0u
            && group.active_surfaces > group.fallback_surfaces) {
            ++summary.fallback_mixed_group_count;
        }
        summary.total_shape_pair_count += group.shape_pair_count;
        summary.blend_candidate_pair_count += group.blend_candidate_pair_count;
        summary.union_candidate_pair_count += group.union_candidate_pair_count;
        summary.morph_candidate_pair_count += group.morph_candidate_pair_count;
        summary.separated_pair_count += group.separated_pair_count;
        if (group.has_shape_gap) {
            summary.min_shape_gap =
                summary.total_shape_pair_count == group.shape_pair_count
                    ? group.min_shape_gap
                    : std::min(summary.min_shape_gap, group.min_shape_gap);
            summary.max_shape_gap =
                std::max(summary.max_shape_gap, group.max_shape_gap);
        }
        summary.max_blend_distance =
            std::max(summary.max_blend_distance, group.max_blend_distance);
        if (group.has_bounds) {
            auto const bounds_width =
                std::max(0.0f, group.max_x - group.min_x);
            auto const bounds_height =
                std::max(0.0f, group.max_y - group.min_y);
            summary.max_group_bounds_width = std::max(
                summary.max_group_bounds_width,
                bounds_width);
            summary.max_group_bounds_height = std::max(
                summary.max_group_bounds_height,
                bounds_height);
            summary.max_group_bounds_area = std::max(
                summary.max_group_bounds_area,
                bounds_width * bounds_height);
        }
    }
    return summary;
}

struct MaterialSurfaceSampleRuntimeSummary {
    std::int64_t total_surface_sample_pixels = 0;
    std::int64_t max_surface_sample_pixels = 0;
};

inline MaterialSurfaceSampleRuntimeSummary
summarize_material_execution_surface_samples(
        std::span<MaterialRuntimeRecord const> records) {
    MaterialSurfaceSampleRuntimeSummary summary{};
    for (auto const& record : records) {
        auto const execution =
            material_container_execution_descriptor(record, records);
        auto const pixels =
            material_estimate_surface_sample_pixels(record.plan, &execution);
        summary.total_surface_sample_pixels += pixels;
        summary.max_surface_sample_pixels =
            std::max(summary.max_surface_sample_pixels, pixels);
    }
    return summary;
}

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

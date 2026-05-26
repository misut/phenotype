module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <span>
#include <string_view>
#include <vector>

export module phenotype.material.plan;

import phenotype.material.types;
import phenotype.types;
import phenotype.theme_contract;

export namespace phenotype {

inline float material_alpha_fraction(Color color) noexcept {
    return static_cast<float>(color.a) / 255.0f;
}

inline MaterialStageOptics material_primary_stage_optics(
        MaterialPlan const& plan) noexcept {
    auto const& composition = plan.optical_composition;
    MaterialStageOptics optics{};
    optics.channel = plan.primary_pass.executor;
    optics.opacity = composition.opacity;
    optics.blur_radius = composition.blur_radius;
    optics.tint_alpha = composition.tint_alpha;
    optics.saturation = composition.saturation;
    optics.luminance_floor = composition.luminance_floor;
    optics.luminance_gain = composition.luminance_gain;
    optics.refraction_model = plan.refraction.model;
    optics.refraction_strength = plan.refraction.strength;
    optics.refraction_edge_bias = plan.refraction.edge_bias;
    optics.refraction_offset_pixels = plan.refraction.max_offset_pixels;
    optics.refraction_edge_caustic_intensity =
        plan.refraction.edge_caustic_intensity;
    optics.edge_optics_model = plan.edge_optics.model;
    optics.edge_bevel_width = plan.edge_optics.bevel_width;
    optics.edge_inner_highlight = plan.edge_optics.inner_highlight;
    optics.edge_outer_shadow = plan.edge_optics.outer_shadow;
    optics.edge_chromatic_fringe = plan.edge_optics.chromatic_fringe;
    optics.spectral_tint_model = plan.spectral_tint.model;
    optics.spectral_tint_warmth = plan.spectral_tint.warmth;
    optics.spectral_tint_coolness = plan.spectral_tint.coolness;
    optics.spectral_dispersion = plan.spectral_tint.dispersion;
    optics.spectral_rim_tint = plan.spectral_tint.rim_tint;
    optics.dynamic_lighting_model = plan.dynamic_lighting.model;
    optics.dynamic_light_direction_x = plan.dynamic_lighting.direction_x;
    optics.dynamic_light_direction_y = plan.dynamic_lighting.direction_y;
    optics.dynamic_light_highlight =
        plan.dynamic_lighting.highlight_strength;
    optics.dynamic_light_shadow = plan.dynamic_lighting.shadow_strength;
    optics.glass_caustic_flow_model = plan.glass_caustic_flow.model;
    optics.glass_caustic_flow_strength =
        plan.glass_caustic_flow.flow_strength;
    optics.glass_caustic_chroma_shear =
        plan.glass_caustic_flow.chroma_shear;
    optics.glass_caustic_highlight_drift =
        plan.glass_caustic_flow.highlight_drift;
    optics.glass_caustic_focus =
        plan.glass_caustic_flow.caustic_focus;
    optics.glass_meniscus_model = plan.glass_meniscus.model;
    optics.glass_meniscus_rim_width =
        plan.glass_meniscus.rim_width;
    optics.glass_meniscus_edge_pull =
        plan.glass_meniscus.edge_pull;
    optics.glass_meniscus_highlight_gain =
        plan.glass_meniscus.highlight_gain;
    optics.glass_meniscus_refraction_gain =
        plan.glass_meniscus.refraction_gain;
    optics.glass_transmission_model = plan.glass_transmission.model;
    optics.glass_internal_transmission =
        plan.glass_transmission.internal_transmission;
    optics.glass_subsurface_scatter =
        plan.glass_transmission.subsurface_scatter;
    optics.glass_volume_absorption =
        plan.glass_transmission.volume_absorption;
    optics.glass_interlayer_refraction =
        plan.glass_transmission.interlayer_refraction;
    optics.glass_surface_cohesion_model =
        plan.glass_surface_cohesion.model;
    optics.glass_surface_response =
        plan.glass_surface_cohesion.surface_response;
    optics.glass_edge_adhesion =
        plan.glass_surface_cohesion.edge_adhesion;
    optics.glass_shape_coalescence =
        plan.glass_surface_cohesion.shape_coalescence;
    optics.glass_luma_stability =
        plan.glass_surface_cohesion.luma_stability;
    optics.glass_substrate_adhesion_model =
        plan.glass_substrate_adhesion.model;
    optics.glass_substrate_contact =
        plan.glass_substrate_adhesion.contact_strength;
    optics.glass_substrate_settle =
        plan.glass_substrate_adhesion.settle_depth;
    optics.glass_substrate_shadow =
        plan.glass_substrate_adhesion.contact_shadow;
    optics.glass_substrate_refraction =
        plan.glass_substrate_adhesion.refraction_crawl;
    optics.glass_ambient_reflection_model =
        plan.glass_ambient_reflection.model;
    optics.glass_ambient_reflection_gain =
        plan.glass_ambient_reflection.reflection_gain;
    optics.glass_ambient_color_bleed =
        plan.glass_ambient_reflection.color_bleed;
    optics.glass_ambient_luma_polarization =
        plan.glass_ambient_reflection.luma_polarization;
    optics.glass_ambient_sheen_coherence =
        plan.glass_ambient_reflection.sheen_coherence;
    optics.glass_thickness_model = plan.glass_thickness.model;
    optics.glass_thickness = plan.glass_thickness.thickness;
    optics.glass_lensing_gain = plan.glass_thickness.lensing_gain;
    optics.glass_shadow_gain = plan.glass_thickness.shadow_gain;
    optics.glass_scattering_gain = plan.glass_thickness.scattering_gain;
    optics.glass_dispersion_model = plan.glass_dispersion.model;
    optics.glass_dispersion_axial_offset =
        plan.glass_dispersion.axial_offset_pixels;
    optics.glass_dispersion_tangential_offset =
        plan.glass_dispersion.tangential_offset_pixels;
    optics.glass_dispersion_prismatic_gain =
        plan.glass_dispersion.prismatic_gain;
    optics.glass_dispersion_caustic_spread =
        plan.glass_dispersion.caustic_spread;
    optics.glass_stabilization_model = plan.glass_stabilization.model;
    optics.glass_stabilization_strength =
        plan.glass_stabilization.strength;
    optics.glass_stabilization_damping =
        plan.glass_stabilization.damping;
    optics.glass_stabilization_shimmer_reduction =
        plan.glass_stabilization.shimmer_reduction;
    optics.glass_stabilization_transmission_bias =
        plan.glass_stabilization.transmission_bias;
    optics.glass_environment_model = plan.glass_environment.model;
    optics.glass_environment_reflection_strength =
        plan.glass_environment.reflection_strength;
    optics.glass_environment_color_pickup =
        plan.glass_environment.color_pickup;
    optics.glass_environment_luminance_balance =
        plan.glass_environment.luminance_balance;
    optics.glass_environment_transmission_balance =
        plan.glass_environment.transmission_balance;
    optics.glass_depth_model = plan.glass_depth.model;
    optics.glass_depth_separation = plan.glass_depth.depth_separation;
    optics.glass_depth_inner_shadow = plan.glass_depth.inner_shadow;
    optics.glass_depth_surface_lift = plan.glass_depth.surface_lift;
    optics.glass_depth_parallax_gain = plan.glass_depth.parallax_gain;
    optics.scroll_edge_model = plan.scroll_edge.model;
    optics.scroll_edge_extent = plan.scroll_edge.fade_extent_pixels;
    optics.scroll_edge_dissolve = plan.scroll_edge.dissolve_strength;
    optics.scroll_edge_dimming = plan.scroll_edge.dimming_strength;
    optics.scroll_edge_hard_style = plan.scroll_edge.hard_style_strength;
    optics.clear_glass_legibility_model =
        plan.clear_glass_legibility.model;
    optics.clear_glass_dimming =
        plan.clear_glass_legibility.dimming_strength;
    optics.clear_glass_contrast =
        plan.clear_glass_legibility.contrast_lift;
    optics.clear_glass_brightness_response =
        plan.clear_glass_legibility.brightness_response;
    optics.clear_glass_detail_response =
        plan.clear_glass_legibility.detail_response;
    optics.control_morph_model = plan.interaction.control_morph_model;
    optics.control_morph_scale_delta =
        plan.interaction.control_morph_scale_delta;
    optics.control_morph_depth = plan.interaction.control_morph_depth;
    optics.control_morph_edge = plan.interaction.control_morph_edge;
    optics.control_morph_shadow = plan.interaction.control_morph_shadow;
    optics.prominent_glass_model = plan.prominent_glass.model;
    optics.prominent_glass_intensity = plan.prominent_glass.intensity;
    optics.prominent_glass_tint_weight = plan.prominent_glass.tint_weight;
    optics.prominent_glass_edge_lift = plan.prominent_glass.edge_lift;
    optics.prominent_glass_lensing_gain = plan.prominent_glass.lensing_gain;
    return optics;
}

inline MaterialStageOptics material_shadow_stage_optics(
        MaterialPlan const& plan) noexcept {
    auto const& composition = plan.optical_composition;
    MaterialStageOptics optics{};
    optics.channel = "shape-shadow";
    optics.edge_width = composition.edge_width;
    optics.shadow_alpha = composition.shadow_alpha;
    optics.shadow_radius = composition.shadow_radius;
    return optics;
}

inline MaterialStageOptics material_edge_stage_optics(
        MaterialPlan const& plan) noexcept {
    auto const& composition = plan.optical_composition;
    MaterialStageOptics optics{};
    optics.channel = "edge-highlight";
    optics.edge_highlight = composition.edge_highlight;
    optics.edge_width = composition.edge_width;
    optics.specular_model = plan.specular.model;
    optics.specular_anchor_x = plan.specular.anchor_x;
    optics.specular_anchor_y = plan.specular.anchor_y;
    optics.specular_radius = plan.specular.radius;
    optics.specular_intensity = plan.specular.intensity;
    optics.pointer_lens_model = plan.interaction.pointer_lens_model;
    optics.pointer_lens_anchor_x = plan.interaction.pointer_lens_anchor_x;
    optics.pointer_lens_anchor_y = plan.interaction.pointer_lens_anchor_y;
    optics.pointer_lens_radius = plan.interaction.pointer_lens_radius;
    optics.pointer_lens_strength = plan.interaction.pointer_lens_strength;
    optics.edge_optics_model = plan.edge_optics.model;
    optics.edge_bevel_width = plan.edge_optics.bevel_width;
    optics.edge_inner_highlight = plan.edge_optics.inner_highlight;
    optics.edge_outer_shadow = plan.edge_optics.outer_shadow;
    optics.edge_chromatic_fringe = plan.edge_optics.chromatic_fringe;
    optics.spectral_tint_model = plan.spectral_tint.model;
    optics.spectral_tint_warmth = plan.spectral_tint.warmth;
    optics.spectral_tint_coolness = plan.spectral_tint.coolness;
    optics.spectral_dispersion = plan.spectral_tint.dispersion;
    optics.spectral_rim_tint = plan.spectral_tint.rim_tint;
    optics.dynamic_lighting_model = plan.dynamic_lighting.model;
    optics.dynamic_light_direction_x = plan.dynamic_lighting.direction_x;
    optics.dynamic_light_direction_y = plan.dynamic_lighting.direction_y;
    optics.dynamic_light_highlight =
        plan.dynamic_lighting.highlight_strength;
    optics.dynamic_light_shadow = plan.dynamic_lighting.shadow_strength;
    optics.glass_caustic_flow_model = plan.glass_caustic_flow.model;
    optics.glass_caustic_flow_strength =
        plan.glass_caustic_flow.flow_strength;
    optics.glass_caustic_chroma_shear =
        plan.glass_caustic_flow.chroma_shear;
    optics.glass_caustic_highlight_drift =
        plan.glass_caustic_flow.highlight_drift;
    optics.glass_caustic_focus =
        plan.glass_caustic_flow.caustic_focus;
    optics.glass_meniscus_model = plan.glass_meniscus.model;
    optics.glass_meniscus_rim_width =
        plan.glass_meniscus.rim_width;
    optics.glass_meniscus_edge_pull =
        plan.glass_meniscus.edge_pull;
    optics.glass_meniscus_highlight_gain =
        plan.glass_meniscus.highlight_gain;
    optics.glass_meniscus_refraction_gain =
        plan.glass_meniscus.refraction_gain;
    optics.glass_transmission_model = plan.glass_transmission.model;
    optics.glass_internal_transmission =
        plan.glass_transmission.internal_transmission;
    optics.glass_subsurface_scatter =
        plan.glass_transmission.subsurface_scatter;
    optics.glass_volume_absorption =
        plan.glass_transmission.volume_absorption;
    optics.glass_interlayer_refraction =
        plan.glass_transmission.interlayer_refraction;
    optics.glass_surface_cohesion_model =
        plan.glass_surface_cohesion.model;
    optics.glass_surface_response =
        plan.glass_surface_cohesion.surface_response;
    optics.glass_edge_adhesion =
        plan.glass_surface_cohesion.edge_adhesion;
    optics.glass_shape_coalescence =
        plan.glass_surface_cohesion.shape_coalescence;
    optics.glass_luma_stability =
        plan.glass_surface_cohesion.luma_stability;
    optics.glass_substrate_adhesion_model =
        plan.glass_substrate_adhesion.model;
    optics.glass_substrate_contact =
        plan.glass_substrate_adhesion.contact_strength;
    optics.glass_substrate_settle =
        plan.glass_substrate_adhesion.settle_depth;
    optics.glass_substrate_shadow =
        plan.glass_substrate_adhesion.contact_shadow;
    optics.glass_substrate_refraction =
        plan.glass_substrate_adhesion.refraction_crawl;
    optics.glass_ambient_reflection_model =
        plan.glass_ambient_reflection.model;
    optics.glass_ambient_reflection_gain =
        plan.glass_ambient_reflection.reflection_gain;
    optics.glass_ambient_color_bleed =
        plan.glass_ambient_reflection.color_bleed;
    optics.glass_ambient_luma_polarization =
        plan.glass_ambient_reflection.luma_polarization;
    optics.glass_ambient_sheen_coherence =
        plan.glass_ambient_reflection.sheen_coherence;
    optics.glass_thickness_model = plan.glass_thickness.model;
    optics.glass_thickness = plan.glass_thickness.thickness;
    optics.glass_lensing_gain = plan.glass_thickness.lensing_gain;
    optics.glass_shadow_gain = plan.glass_thickness.shadow_gain;
    optics.glass_scattering_gain = plan.glass_thickness.scattering_gain;
    optics.glass_dispersion_model = plan.glass_dispersion.model;
    optics.glass_dispersion_axial_offset =
        plan.glass_dispersion.axial_offset_pixels;
    optics.glass_dispersion_tangential_offset =
        plan.glass_dispersion.tangential_offset_pixels;
    optics.glass_dispersion_prismatic_gain =
        plan.glass_dispersion.prismatic_gain;
    optics.glass_dispersion_caustic_spread =
        plan.glass_dispersion.caustic_spread;
    optics.glass_stabilization_model = plan.glass_stabilization.model;
    optics.glass_stabilization_strength =
        plan.glass_stabilization.strength;
    optics.glass_stabilization_damping =
        plan.glass_stabilization.damping;
    optics.glass_stabilization_shimmer_reduction =
        plan.glass_stabilization.shimmer_reduction;
    optics.glass_stabilization_transmission_bias =
        plan.glass_stabilization.transmission_bias;
    optics.glass_environment_model = plan.glass_environment.model;
    optics.glass_environment_reflection_strength =
        plan.glass_environment.reflection_strength;
    optics.glass_environment_color_pickup =
        plan.glass_environment.color_pickup;
    optics.glass_environment_luminance_balance =
        plan.glass_environment.luminance_balance;
    optics.glass_environment_transmission_balance =
        plan.glass_environment.transmission_balance;
    optics.glass_depth_model = plan.glass_depth.model;
    optics.glass_depth_separation = plan.glass_depth.depth_separation;
    optics.glass_depth_inner_shadow = plan.glass_depth.inner_shadow;
    optics.glass_depth_surface_lift = plan.glass_depth.surface_lift;
    optics.glass_depth_parallax_gain = plan.glass_depth.parallax_gain;
    optics.scroll_edge_model = plan.scroll_edge.model;
    optics.scroll_edge_extent = plan.scroll_edge.fade_extent_pixels;
    optics.scroll_edge_dissolve = plan.scroll_edge.dissolve_strength;
    optics.scroll_edge_dimming = plan.scroll_edge.dimming_strength;
    optics.scroll_edge_hard_style = plan.scroll_edge.hard_style_strength;
    optics.clear_glass_legibility_model =
        plan.clear_glass_legibility.model;
    optics.clear_glass_dimming =
        plan.clear_glass_legibility.dimming_strength;
    optics.clear_glass_contrast =
        plan.clear_glass_legibility.contrast_lift;
    optics.clear_glass_brightness_response =
        plan.clear_glass_legibility.brightness_response;
    optics.clear_glass_detail_response =
        plan.clear_glass_legibility.detail_response;
    optics.control_morph_model = plan.interaction.control_morph_model;
    optics.control_morph_scale_delta =
        plan.interaction.control_morph_scale_delta;
    optics.control_morph_depth = plan.interaction.control_morph_depth;
    optics.control_morph_edge = plan.interaction.control_morph_edge;
    optics.control_morph_shadow = plan.interaction.control_morph_shadow;
    optics.prominent_glass_model = plan.prominent_glass.model;
    optics.prominent_glass_intensity = plan.prominent_glass.intensity;
    optics.prominent_glass_tint_weight = plan.prominent_glass.tint_weight;
    optics.prominent_glass_edge_lift = plan.prominent_glass.edge_lift;
    optics.prominent_glass_lensing_gain = plan.prominent_glass.lensing_gain;
    return optics;
}

inline MaterialStageOptics material_noise_stage_optics(
        MaterialPlan const& plan) noexcept {
    auto const& composition = plan.optical_composition;
    MaterialStageOptics optics{};
    optics.channel = "noise-dither";
    optics.noise_opacity = composition.noise_opacity;
    return optics;
}

inline bool material_plan_uses_standard_content_layer(
        MaterialPlan const& plan) noexcept {
    return plan.kind != MaterialKind::None
        && material_role_uses_standard_content_layer(plan.role);
}

inline void append_material_execution_stage(
        MaterialPlan& plan,
        MaterialExecutionStage stage) noexcept {
    if (!stage.active)
        return;
    auto const capacity =
        std::min(plan.execution_stage_capacity, material_max_execution_stages);
    if (plan.execution_stage_count >= capacity) {
        ++plan.dropped_execution_stage_count;
        return;
    }
    plan.execution_stages[plan.execution_stage_count++] = stage;
}

inline bool material_paint_layer_matches(
        char const* actual,
        std::string_view expected) noexcept {
    return actual && std::string_view{actual} == expected;
}

inline void append_material_paint_layer(
        MaterialPlan& plan,
        MaterialPaintLayer layer) noexcept {
    if (!layer.active)
        return;
    auto const capacity =
        std::min(plan.paint_layer_capacity, material_max_paint_layers);
    if (plan.paint_layer_count >= capacity) {
        ++plan.dropped_paint_layer_count;
        return;
    }
    layer.inflate = std::max(0.0f, layer.inflate);
    layer.stroke_width = std::max(0.0f, layer.stroke_width);
    layer.opacity = std::clamp(layer.opacity, 0.0f, 1.0f);
    layer.soft_edge_radius = std::max(0.0f, layer.soft_edge_radius);
    plan.resource_budget.max_paint_layer_inflate = std::max(
        plan.resource_budget.max_paint_layer_inflate,
        layer.inflate);
    plan.paint_layers[plan.paint_layer_count++] = layer;
}

inline MaterialObservationContract material_observation_contract(
        MaterialPlan const& plan) noexcept {
    MaterialObservationContract contract{};
    contract.schema_version = plan.contract_version;
    contract.semantic_material_required = plan.kind != MaterialKind::None;
    contract.runtime_plan_required = plan.kind != MaterialKind::None;
    contract.fallback_expected = plan.fallback();
    contract.backdrop_sampling_expected = plan.backdrop_sampling;
    contract.stable_backdrop_required = plan.backdrop_sampling;
    contract.shared_frame_capture_required =
        plan.backdrop_access.shared_frame_capture;
    contract.next_frame_capture_required =
        plan.backdrop_access.next_frame_capture_required;
    contract.backdrop_capture_excludes_foreground_text =
        plan.backdrop_access.excludes_foreground_text;
    contract.bounded_texture_copy_required =
        plan.resource_budget.bounded_texture_copy;
    contract.deterministic_fallback_required =
        plan.resource_budget.deterministic_fallback;
    contract.backdrop_capture_scope = plan.backdrop_access.capture_scope;
    contract.backdrop_capture_reason = plan.backdrop_access.capture_reason;
    contract.fallback_path =
        material_fallback_path_name(plan.fallback_path);
    contract.fallback_reason = plan.fallback_reason;
    contract.primary_pass = plan.primary_pass.name;
    contract.primary_executor = plan.primary_pass.executor;
    contract.expected_stage_order = plan.optical_composition.stage_order;
    contract.expected_runtime_passes = 1;
    contract.expected_active_runtime_passes =
        plan.primary_pass.active ? 1u : 0u;
    contract.expected_backdrop_runtime_passes =
        plan.primary_pass.requires_backdrop ? 1u : 0u;
    contract.expected_execution_stages = plan.execution_stage_count;
    contract.expected_paint_layers = plan.paint_layer_count;
    contract.max_frame_capture_count =
        plan.backdrop_access.max_frame_capture_count;
    contract.max_frame_capture_pixels =
        plan.backdrop_access.max_frame_capture_pixels;
    contract.max_surface_sample_pixels =
        plan.backdrop_access.max_surface_sample_pixels;
    contract.max_texture_copy_pixels =
        plan.primary_pass.max_texture_copy_pixels;
    contract.region_name = plan.verifier.region_name;
    contract.likely_layer = plan.verifier.likely_layer;
    contract.likely_pass = plan.verifier.likely_pass;
    for (unsigned int i = 0; i < plan.execution_stage_count; ++i) {
        auto const& stage = plan.execution_stages[i];
        if (stage.active)
            ++contract.expected_active_execution_stages;
        if (stage.requires_backdrop)
            ++contract.expected_backdrop_execution_stages;
    }
    for (unsigned int i = 0; i < plan.paint_layer_count; ++i) {
        auto const& layer = plan.paint_layers[i];
        if (layer.active)
            ++contract.expected_active_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-shadow"))
            ++contract.expected_shadow_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-fill"))
            ++contract.expected_fill_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-edge"))
            ++contract.expected_edge_paint_layers;
    }
    return contract;
}

inline void material_record_execution_mismatch(
        MaterialExecutionAudit& audit,
        bool condition,
        char const* mismatch) noexcept {
    if (condition)
        return;
    audit.contract_satisfied = false;
    ++audit.mismatch_count;
    if (audit.first_mismatch == nullptr
        || std::string_view{audit.first_mismatch} == "none") {
        audit.first_mismatch =
            mismatch != nullptr && mismatch[0] ? mismatch : "unknown";
    }
}

inline char const* material_execution_stage_order_name(
        MaterialPlan const& plan) noexcept;

inline MaterialExecutionAudit material_execution_audit(
        MaterialPlan const& plan) noexcept {
    auto const& observation = plan.observation_contract;
    MaterialExecutionAudit audit{};
    audit.schema_version = plan.contract_version;
    audit.actual_runtime_passes = 1;
    audit.actual_active_runtime_passes =
        plan.primary_pass.active ? 1u : 0u;
    audit.actual_backdrop_runtime_passes =
        plan.primary_pass.requires_backdrop ? 1u : 0u;
    audit.actual_execution_stages = plan.execution_stage_count;
    audit.actual_paint_layers = plan.paint_layer_count;
    audit.expected_stage_order = observation.expected_stage_order;
    audit.actual_stage_order = material_execution_stage_order_name(plan);
    audit.bounded_texture_copy = plan.resource_budget.bounded_texture_copy;
    audit.deterministic_fallback =
        plan.resource_budget.deterministic_fallback;
    audit.likely_layer =
        observation.likely_layer != nullptr && observation.likely_layer[0]
            ? observation.likely_layer
            : "material-execution-contract";
    audit.likely_pass =
        observation.likely_pass != nullptr && observation.likely_pass[0]
            ? observation.likely_pass
            : "none";

    for (unsigned int i = 0; i < plan.execution_stage_count; ++i) {
        auto const& stage = plan.execution_stages[i];
        if (stage.active)
            ++audit.actual_active_execution_stages;
        if (stage.requires_backdrop)
            ++audit.actual_backdrop_execution_stages;
    }
    for (unsigned int i = 0; i < plan.paint_layer_count; ++i) {
        auto const& layer = plan.paint_layers[i];
        if (layer.active)
            ++audit.actual_active_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-shadow"))
            ++audit.actual_shadow_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-fill"))
            ++audit.actual_fill_paint_layers;
        if (material_paint_layer_matches(layer.executor, "rounded-edge"))
            ++audit.actual_edge_paint_layers;
    }

    audit.runtime_passes_match =
        observation.expected_runtime_passes == audit.actual_runtime_passes;
    audit.active_runtime_passes_match =
        observation.expected_active_runtime_passes
            == audit.actual_active_runtime_passes;
    audit.backdrop_runtime_passes_match =
        observation.expected_backdrop_runtime_passes
            == audit.actual_backdrop_runtime_passes;
    audit.execution_stages_match =
        observation.expected_execution_stages == audit.actual_execution_stages;
    audit.active_execution_stages_match =
        observation.expected_active_execution_stages
            == audit.actual_active_execution_stages;
    audit.backdrop_execution_stages_match =
        observation.expected_backdrop_execution_stages
            == audit.actual_backdrop_execution_stages;
    audit.paint_layers_match =
        observation.expected_paint_layers == audit.actual_paint_layers;
    audit.active_paint_layers_match =
        observation.expected_active_paint_layers
            == audit.actual_active_paint_layers;
    audit.shadow_paint_layers_match =
        observation.expected_shadow_paint_layers
            == audit.actual_shadow_paint_layers;
    audit.fill_paint_layers_match =
        observation.expected_fill_paint_layers
            == audit.actual_fill_paint_layers;
    audit.edge_paint_layers_match =
        observation.expected_edge_paint_layers
            == audit.actual_edge_paint_layers;
    audit.stage_order_match =
        std::string_view{audit.expected_stage_order
            ? audit.expected_stage_order
            : "none"}
        == std::string_view{audit.actual_stage_order
            ? audit.actual_stage_order
            : "none"};

    material_record_execution_mismatch(
        audit, audit.runtime_passes_match, "runtime-pass-count");
    material_record_execution_mismatch(
        audit, audit.active_runtime_passes_match, "active-runtime-pass-count");
    material_record_execution_mismatch(
        audit, audit.backdrop_runtime_passes_match, "backdrop-runtime-pass-count");
    material_record_execution_mismatch(
        audit, audit.execution_stages_match, "execution-stage-count");
    material_record_execution_mismatch(
        audit, audit.active_execution_stages_match, "active-execution-stage-count");
    material_record_execution_mismatch(
        audit, audit.backdrop_execution_stages_match, "backdrop-execution-stage-count");
    material_record_execution_mismatch(
        audit, audit.paint_layers_match, "paint-layer-count");
    material_record_execution_mismatch(
        audit, audit.active_paint_layers_match, "active-paint-layer-count");
    material_record_execution_mismatch(
        audit, audit.shadow_paint_layers_match, "shadow-paint-layer-count");
    material_record_execution_mismatch(
        audit, audit.fill_paint_layers_match, "fill-paint-layer-count");
    material_record_execution_mismatch(
        audit, audit.edge_paint_layers_match, "edge-paint-layer-count");
    material_record_execution_mismatch(
        audit, audit.stage_order_match, "stage-order");
    material_record_execution_mismatch(
        audit,
        !observation.bounded_texture_copy_required
            || audit.bounded_texture_copy,
        "bounded-texture-copy");
    material_record_execution_mismatch(
        audit,
        !observation.deterministic_fallback_required
            || audit.deterministic_fallback,
        "deterministic-fallback");
    return audit;
}

struct MaterialForegroundTextResolution {
    Color color{};
    char const* role = "none";
    std::uint32_t material_command_index = 0;
    bool has_material = false;
    bool remapped = false;
};

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

inline float material_surface_materialize_geometry_scale(
        MaterialTransitionAnalysis const& transition) noexcept {
    if (!transition.active || !transition.materialize)
        return 1.0f;
    return std::clamp(0.72f + 0.28f * transition.optical_gain, 0.72f, 1.0f);
}

inline float material_transition_materialize_wave(float material_gain) noexcept {
    material_gain = std::clamp(material_gain, 0.0f, 1.0f);
    return std::clamp(
        4.0f * material_gain * (1.0f - material_gain),
        0.0f,
        1.0f);
}

inline MaterialTransitionAnalysis material_transition_as_materialize(
        MaterialTransitionAnalysis transition) noexcept {
    if (!transition.active || transition.reduced_motion_suppressed)
        return transition;
    auto const progress = std::clamp(transition.progress, 0.0f, 1.0f);
    auto const gain = progress * progress * (3.0f - 2.0f * progress);
    auto const material_gain = transition.appearing ? gain : 1.0f - gain;
    transition.kind = MaterialGlassTransitionKind::Materialize;
    transition.kind_name = "materialize";
    transition.materialize = true;
    transition.matched_geometry = false;
    transition.opacity_gain = material_gain;
    transition.optical_gain =
        std::clamp(0.22f + 0.78f * material_gain, 0.0f, 1.0f);
    transition.shadow_gain =
        std::clamp(0.10f + 0.90f * material_gain, 0.0f, 1.0f);
    transition.refraction_gain = std::clamp(material_gain, 0.0f, 1.0f);
    auto const wave = material_transition_materialize_wave(material_gain);
    transition.materialize_optics_model = "materialize-liquid-glass-optics";
    transition.materialize_optics_active = wave > 0.0001f;
    transition.materialize_wave_strength = wave;
    transition.materialize_edge_lift =
        std::clamp(0.11f * wave + 0.035f * transition.optical_gain,
                   0.0f,
                   0.16f);
    transition.materialize_lensing_gain =
        1.0f + 0.20f * wave + 0.08f * transition.refraction_gain;
    transition.materialize_rim_position = transition.appearing
        ? std::clamp(0.18f + 0.82f * material_gain, 0.0f, 1.0f)
        : std::clamp(0.18f + 0.82f * (1.0f - material_gain),
                     0.0f,
                     1.0f);
    transition.policy = transition.appearing
        ? "materialize-in"
        : "materialize-out";
    return transition;
}

inline MaterialTransitionAnalysis material_execution_transition(
        MaterialTransitionAnalysis transition,
        MaterialContainerExecutionDescriptor const* execution) noexcept {
    if (execution && execution->glass_effect_materialize_execution)
        return material_transition_as_materialize(transition);
    return transition;
}

inline void material_apply_centered_geometry_scale(float& x,
                                                   float& y,
                                                   float& w,
                                                   float& h,
                                                   float& radius,
                                                   float scale) noexcept {
    scale = std::isfinite(scale) ? std::clamp(scale, 0.0f, 1.0f) : 1.0f;
    if (scale >= 0.999f)
        return;
    w = std::max(0.0f, w);
    h = std::max(0.0f, h);
    auto const cx = x + w * 0.5f;
    auto const cy = y + h * 0.5f;
    w *= scale;
    h *= scale;
    x = cx - w * 0.5f;
    y = cy - h * 0.5f;
    radius = std::max(0.0f, radius * scale);
}

inline void material_apply_materialize_execution_geometry(
        MaterialTransitionAnalysis const& transition,
        float& x,
        float& y,
        float& w,
        float& h,
        float& radius) noexcept {
    material_apply_centered_geometry_scale(
        x,
        y,
        w,
        h,
        radius,
        material_surface_materialize_geometry_scale(transition));
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

inline float material_safe_luma(float value, float fallback) noexcept;

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

inline bool material_stage_matches(
        char const* actual,
        std::string_view expected) noexcept {
    return actual && std::string_view{actual} == expected;
}

inline char const* material_execution_stage_role(
        MaterialPlan const& plan,
        MaterialExecutionStage const& stage) noexcept {
    if (material_stage_matches(stage.name, "shape-shadow"))
        return "shadow";
    if (material_stage_matches(stage.name, "edge-highlight"))
        return "edge";
    if (material_stage_matches(stage.name, "noise-dither"))
        return "noise";
    if (plan.primary_pass.active
        && material_stage_matches(stage.name, plan.primary_pass.name)) {
        return "primary";
    }
    return "unknown";
}

inline bool material_execution_stage_role_at(
        MaterialPlan const& plan,
        unsigned int index,
        std::string_view expected) noexcept {
    if (index >= plan.execution_stage_count)
        return false;
    return std::string_view{
        material_execution_stage_role(plan, plan.execution_stages[index])
    } == expected;
}

inline char const* material_execution_stage_order_name(
        MaterialPlan const& plan) noexcept {
    switch (plan.execution_stage_count) {
        case 0:
            return "none";
        case 1:
            return material_execution_stage_role_at(plan, 0, "primary")
                ? "primary"
                : "unexpected-stage-order";
        case 2:
            if (material_execution_stage_role_at(plan, 0, "shadow")
                && material_execution_stage_role_at(plan, 1, "primary")) {
                return "shadow-primary";
            }
            if (material_execution_stage_role_at(plan, 0, "primary")
                && material_execution_stage_role_at(plan, 1, "edge")) {
                return "primary-edge";
            }
            if (material_execution_stage_role_at(plan, 0, "primary")
                && material_execution_stage_role_at(plan, 1, "noise")) {
                return "primary-noise";
            }
            return "unexpected-stage-order";
        case 3:
            if (material_execution_stage_role_at(plan, 0, "shadow")
                && material_execution_stage_role_at(plan, 1, "primary")
                && material_execution_stage_role_at(plan, 2, "edge")) {
                return "shadow-primary-edge";
            }
            if (material_execution_stage_role_at(plan, 0, "shadow")
                && material_execution_stage_role_at(plan, 1, "primary")
                && material_execution_stage_role_at(plan, 2, "noise")) {
                return "shadow-primary-noise";
            }
            if (material_execution_stage_role_at(plan, 0, "primary")
                && material_execution_stage_role_at(plan, 1, "edge")
                && material_execution_stage_role_at(plan, 2, "noise")) {
                return "primary-edge-noise";
            }
            return "unexpected-stage-order";
        case 4:
            if (material_execution_stage_role_at(plan, 0, "shadow")
                && material_execution_stage_role_at(plan, 1, "primary")
                && material_execution_stage_role_at(plan, 2, "edge")
                && material_execution_stage_role_at(plan, 3, "noise")) {
                return "shadow-primary-edge-noise";
            }
            return "unexpected-stage-order";
    }
    return "unexpected-stage-order";
}

inline bool material_plan_uses_sampled_backdrop_executor(
        MaterialPlan const& plan) noexcept {
    return plan.primary_pass.active
        && plan.primary_pass.requires_backdrop
        && material_stage_matches(plan.primary_pass.executor, "backdrop-filter");
}

inline bool material_plan_uses_standard_fill_executor(
        MaterialPlan const& plan) noexcept {
    return plan.primary_pass.active
        && !plan.primary_pass.requires_backdrop
        && material_stage_matches(plan.primary_pass.executor, "standard-fill");
}

inline bool material_plan_uses_deterministic_fallback_executor(
        MaterialPlan const& plan) noexcept {
    return plan.primary_pass.active
        && !plan.primary_pass.requires_backdrop
        && material_stage_matches(plan.primary_pass.executor, "fallback-fill");
}

inline bool material_plan_uses_group_surface_merge_executor(
        MaterialPlan const& plan) noexcept {
    return material_plan_uses_sampled_backdrop_executor(plan)
        || material_plan_uses_deterministic_fallback_executor(plan);
}

inline Color material_paint_layer_color_with_alpha(
        Color color,
        float alpha) noexcept {
    color.a = static_cast<unsigned char>(
        std::clamp(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f),
                   0l,
                   255l));
    return color;
}

inline float material_shadow_paint_inflate(
        MaterialPlan const& plan) noexcept {
    return std::min(std::max(plan.shadow_radius * 0.5f, 1.0f), 18.0f);
}

inline float material_shadow_paint_y_offset(
        MaterialPlan const& plan) noexcept {
    return std::min(std::max(plan.shadow_radius * 0.18f, 1.0f), 6.0f);
}

inline float material_background_paint_inflate(
        MaterialPlan const& plan) noexcept {
    if (!plan.glass_background.feathered)
        return 0.0f;
    return std::max(0.0f, plan.glass_background.feather_padding)
        + std::max(0.0f, plan.glass_background.soft_edge_radius);
}

inline std::int64_t material_estimate_surface_sample_pixels_from_area(
        double surface_area,
        MaterialRenderTargetAnalysis target) noexcept {
    if (!target.ready)
        return 0;
    auto const scaled =
        surface_area
        * static_cast<double>(target.scale)
        * static_cast<double>(target.scale);
    if (!std::isfinite(scaled) || scaled <= 0.0)
        return 0;
    auto const bounded = std::min(
        scaled,
        static_cast<double>(target.pixel_count));
    return static_cast<std::int64_t>(std::ceil(bounded));
}

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

inline char const* material_background_fill_layer_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_background.plate)
        return "plate-glass-fill";
    if (plan.glass_background.feathered)
        return "feathered-glass-fill";
    return plan.primary_pass.name;
}

inline char const* material_background_effect_layer_name(
        MaterialPlan const& plan) noexcept {
    return plan.glass_background.active
        ? plan.glass_background.kind_name
        : "none";
}

inline bool material_interaction_paint_highlight_active(
        MaterialPlan const& plan) noexcept {
    return plan.primary_pass.active
        && !plan.primary_pass.requires_backdrop
        && plan.interaction.specular_highlight_active
        && plan.interaction.specular_intensity > 0.0f;
}

inline float material_interaction_paint_extent(
        MaterialPlan const& plan) noexcept {
    return std::min(
        std::max(plan.geometry.w, 0.0f),
        std::max(plan.geometry.h, 0.0f));
}

inline float material_interaction_paint_offset(
        float anchor,
        float extent,
        float response_strength) noexcept {
    auto const scale = std::clamp(
        0.12f + 0.10f * response_strength,
        0.12f,
        0.24f);
    return std::clamp((anchor - 0.5f) * extent * scale, -24.0f, 24.0f);
}

inline float material_interaction_paint_inflate(
        MaterialPlan const& plan) noexcept {
    auto const extent = material_interaction_paint_extent(plan);
    auto const lens_radius = plan.interaction.pointer_lens_active
        ? plan.interaction.pointer_lens_radius
        : plan.interaction.specular_radius;
    return std::clamp(
        extent * std::clamp(lens_radius, 0.0f, 1.0f) * 0.16f,
        4.0f,
        18.0f);
}

inline float material_interaction_paint_opacity(
        MaterialPlan const& plan) noexcept {
    return std::clamp(
        0.04f
            + plan.interaction.specular_intensity * 0.32f
            + plan.interaction.pointer_lens_strength * 0.20f
            + plan.interaction.response_strength * 0.02f,
        0.0f,
        0.26f);
}

inline float material_interaction_paint_soft_edge(
        MaterialPlan const& plan) noexcept {
    auto const extent = material_interaction_paint_extent(plan);
    return std::clamp(
        extent * std::clamp(plan.interaction.specular_radius, 0.0f, 1.0f)
            * 0.40f,
        8.0f,
        30.0f);
}

inline void resolve_material_paint_layers(MaterialPlan& plan) noexcept {
    if (!plan.primary_pass.active || plan.primary_pass.requires_backdrop)
        return;

    auto const background_inflate = material_background_paint_inflate(plan);
    auto const background_effect = material_background_effect_layer_name(plan);
    auto const soft_edge_radius = plan.glass_background.feathered
        ? plan.glass_background.soft_edge_radius
        : 0.0f;

    if (plan.shadow_alpha > 0.0f) {
        auto const inflate = material_shadow_paint_inflate(plan);
        append_material_paint_layer(
            plan,
            MaterialPaintLayer{
                "fallback-shadow",
                true,
                "rounded-shadow",
                background_effect,
                0.0f,
                material_shadow_paint_y_offset(plan),
                inflate + background_inflate,
                inflate + background_inflate,
                0.0f,
                Color{0, 0, 0, 255},
                plan.shadow_alpha,
                soft_edge_radius,
                true,
            });
    }

    append_material_paint_layer(
        plan,
        MaterialPaintLayer{
            material_background_fill_layer_name(plan),
            true,
            "rounded-fill",
            background_effect,
            0.0f,
            0.0f,
            background_inflate,
            background_inflate,
            0.0f,
            plan.tint,
            plan.opacity,
            soft_edge_radius,
            true,
        });

    if (material_interaction_paint_highlight_active(plan)) {
        auto const highlight_inflate =
            material_interaction_paint_inflate(plan);
        append_material_paint_layer(
            plan,
            MaterialPaintLayer{
                "pointer-specular-highlight",
                true,
                "rounded-fill",
                background_effect,
                material_interaction_paint_offset(
                    plan.interaction.specular_anchor_x,
                    plan.geometry.w,
                    plan.interaction.response_strength),
                material_interaction_paint_offset(
                    plan.interaction.specular_anchor_y,
                    plan.geometry.h,
                    plan.interaction.response_strength),
                background_inflate + highlight_inflate,
                background_inflate + highlight_inflate,
                0.0f,
                Color{255, 255, 255, 255},
                material_interaction_paint_opacity(plan),
                std::max(
                    soft_edge_radius,
                    material_interaction_paint_soft_edge(plan)),
                true,
            });
    }

    if (plan.edge_highlight > 0.0f) {
        append_material_paint_layer(
            plan,
            MaterialPaintLayer{
                "fallback-edge-highlight",
                true,
                "rounded-edge",
                background_effect,
                0.0f,
                0.0f,
                background_inflate,
                background_inflate,
                plan.edge_width,
                material_paint_layer_color_with_alpha(
                    Color{255, 255, 255, 255},
                    plan.edge_highlight),
                1.0f,
                soft_edge_radius,
                true,
            });
    }
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
    execution.glass_effect_match_appearance_active = true;
    execution.glass_effect_match_appearance_source_command_index =
        source.command_index;
    execution.glass_effect_match_appearance_blend =
        material_glass_effect_match_appearance_blend(target_plan.transition);

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

struct MaterialEnvironment {
    MaterialCapabilityInput capabilities{};
    MaterialBackdropDescriptor backdrop{};
    MaterialRenderTargetMetadata render_target{};
    MaterialDebugSeed debug_seed{};
    MaterialQualityPolicy quality{};
};

inline MaterialKind material_kind_from_wire(unsigned int raw) noexcept {
    switch (static_cast<MaterialKind>(raw)) {
        case MaterialKind::Clear: return MaterialKind::Clear;
        case MaterialKind::Thin: return MaterialKind::Thin;
        case MaterialKind::Regular: return MaterialKind::Regular;
        case MaterialKind::Thick: return MaterialKind::Thick;
        case MaterialKind::None:
        default: return MaterialKind::None;
    }
}

inline Color material_with_alpha(Color color, unsigned char alpha) noexcept {
    color.a = alpha;
    return color;
}

inline MaterialStyle material_style_for_kind(MaterialKind kind,
                                             Theme const& theme) noexcept {
    MaterialStyle style{};
    style.kind = kind;
    style.fallback = kind != MaterialKind::None;
    style.fallback_reason = kind == MaterialKind::None
        ? ""
        : "fallback path is available when backdrop blur is unavailable";
    style.foreground = theme.foreground;
    style.secondary_foreground = theme.muted;
    style.accent_foreground = theme.accent;
    style.strong_accent_foreground = theme.accent_strong;

    switch (kind) {
        case MaterialKind::Clear:
            style.opacity = 0.28f;
            style.blur_radius = 10.0f;
            style.tint = material_with_alpha(theme.surface, 72);
            style.border = material_with_alpha(theme.border, 120);
            style.contrast_intent = "context";
            style.saturation = 1.22f;
            style.luminance_floor = 0.02f;
            style.luminance_gain = 1.02f;
            style.edge_highlight = 0.22f;
            style.noise_opacity = 0.010f;
            style.shadow_alpha = 0.08f;
            style.shadow_radius = 8.0f;
            style.plan_id = "material.clear.base";
            style.verifier_profile = "clear-over-rich-backdrop";
            break;
        case MaterialKind::Thin:
            style.opacity = 0.42f;
            style.blur_radius = 16.0f;
            style.tint = material_with_alpha(theme.surface, 108);
            style.border = material_with_alpha(theme.border, 150);
            style.contrast_intent = "balanced";
            style.saturation = 1.16f;
            style.luminance_floor = 0.04f;
            style.luminance_gain = 1.05f;
            style.edge_highlight = 0.28f;
            style.noise_opacity = 0.012f;
            style.shadow_alpha = 0.10f;
            style.shadow_radius = 10.0f;
            style.plan_id = "material.thin.base";
            style.verifier_profile = "thin-balanced-backdrop";
            break;
        case MaterialKind::Regular:
            style.opacity = 0.58f;
            style.blur_radius = 22.0f;
            style.tint = material_with_alpha(theme.surface, 148);
            style.border = material_with_alpha(theme.border, 190);
            style.contrast_intent = "legible";
            style.saturation = 1.08f;
            style.luminance_floor = 0.08f;
            style.luminance_gain = 1.08f;
            style.edge_highlight = 0.34f;
            style.noise_opacity = 0.014f;
            style.shadow_alpha = 0.14f;
            style.shadow_radius = 14.0f;
            style.plan_id = "material.regular.base";
            style.verifier_profile = "regular-legibility-backdrop";
            break;
        case MaterialKind::Thick:
            style.opacity = 0.78f;
            style.blur_radius = 30.0f;
            style.tint = material_with_alpha(theme.surface, 198);
            style.border = material_with_alpha(theme.border, 230);
            style.contrast_intent = "high-contrast";
            style.saturation = 1.00f;
            style.luminance_floor = 0.12f;
            style.luminance_gain = 1.12f;
            style.edge_highlight = 0.42f;
            style.noise_opacity = 0.016f;
            style.shadow_alpha = 0.18f;
            style.shadow_radius = 18.0f;
            style.plan_id = "material.thick.base";
            style.verifier_profile = "thick-high-contrast-backdrop";
            break;
        case MaterialKind::None:
        default:
            style.opacity = 0.0f;
            style.blur_radius = 0.0f;
            style.tint = theme.transparent;
            style.border = theme.transparent;
            style.fallback = false;
            style.contrast_intent = "standard";
            style.plan_id = "material.none";
            style.verifier_profile = "none";
            break;
    }
    return style;
}

inline MaterialStyle material_style_for_command(MaterialKind kind,
                                                float opacity,
                                                float blur_radius,
                                                Color tint,
                                                Theme const& theme) noexcept {
    auto style = material_style_for_kind(kind, theme);
    style.opacity = opacity;
    style.blur_radius = blur_radius;
    style.tint = tint;
    return style;
}

inline MaterialCommandDescriptor material_command_descriptor(
        MaterialStyle const& style) noexcept {
    return MaterialCommandDescriptor{
        style.kind,
        style.role,
        style.container,
        style.opacity,
        style.blur_radius,
        style.tint,
        style.saturation,
        style.luminance_floor,
        style.luminance_gain,
        style.edge_highlight,
        style.edge_width,
        style.noise_opacity,
        style.shadow_alpha,
        style.shadow_radius,
        style.interaction,
        style.transition,
        style.glass_identity,
        style.glass_background,
        style.prominence};
}

inline MaterialStyle material_style_for_command(MaterialKind kind,
                                                MaterialSurfaceRole role,
                                                float opacity,
                                                float blur_radius,
                                                Color tint,
                                                Theme const& theme) noexcept {
    auto style = material_style_for_command(
        kind,
        opacity,
        blur_radius,
        tint,
        theme);
    style.role = role;
    return style;
}

inline MaterialStyle material_style_for_command(MaterialKind kind,
                                                float opacity,
                                                float blur_radius,
                                                Color tint,
                                                float saturation,
                                                float luminance_floor,
                                                float luminance_gain,
                                                float edge_highlight,
                                                float edge_width,
                                                float noise_opacity,
                                                float shadow_alpha,
                                                float shadow_radius,
                                                Theme const& theme) noexcept {
    auto style = material_style_for_command(
        kind,
        opacity,
        blur_radius,
        tint,
        theme);
    style.saturation = saturation;
    style.luminance_floor = luminance_floor;
    style.luminance_gain = luminance_gain;
    style.edge_highlight = edge_highlight;
    style.edge_width = edge_width;
    style.noise_opacity = noise_opacity;
    style.shadow_alpha = shadow_alpha;
    style.shadow_radius = shadow_radius;
    return style;
}

inline MaterialStyle material_style_for_command(
        MaterialCommandDescriptor const& descriptor,
        Theme const& theme) noexcept {
    auto style = material_style_for_command(
        descriptor.kind,
        descriptor.opacity,
        descriptor.blur_radius,
        descriptor.tint,
        descriptor.saturation,
        descriptor.luminance_floor,
        descriptor.luminance_gain,
        descriptor.edge_highlight,
        descriptor.edge_width,
        descriptor.noise_opacity,
        descriptor.shadow_alpha,
        descriptor.shadow_radius,
        theme);
    style.role = descriptor.role;
    style.container = descriptor.container;
    style.interaction = descriptor.interaction;
    style.transition = descriptor.transition;
    style.glass_identity = descriptor.glass_identity;
    style.glass_background = descriptor.glass_background;
    style.prominence = descriptor.prominence;
    return style;
}

inline MaterialRequest material_request_for_command(MaterialKind kind,
                                                    float opacity,
                                                    float blur_radius,
                                                    Color tint,
                                                    MaterialGeometry geometry,
                                                    Theme const& theme) noexcept {
    return MaterialRequest{
        material_style_for_command(kind, opacity, blur_radius, tint, theme),
        geometry,
    };
}

inline MaterialRequest material_request_for_command(
        MaterialCommandDescriptor const& descriptor,
        MaterialGeometry geometry,
        Theme const& theme) noexcept {
    return MaterialRequest{
        material_style_for_command(descriptor, theme),
        geometry,
    };
}

inline MaterialRequest material_request_for_command(MaterialKind kind,
                                                    float opacity,
                                                    float blur_radius,
                                                    Color tint,
                                                    float saturation,
                                                    float luminance_floor,
                                                    float luminance_gain,
                                                    float edge_highlight,
                                                    float edge_width,
                                                    float noise_opacity,
                                                    float shadow_alpha,
                                                    float shadow_radius,
                                                    MaterialGeometry geometry,
                                                    Theme const& theme) noexcept {
    return material_request_for_command(
        MaterialCommandDescriptor{
            kind,
            MaterialSurfaceRole::Surface,
            {},
            opacity,
            blur_radius,
            tint,
            saturation,
            luminance_floor,
            luminance_gain,
            edge_highlight,
            edge_width,
            noise_opacity,
            shadow_alpha,
            shadow_radius},
        geometry,
        theme);
}

inline std::uint32_t material_debug_seed(MaterialDebugSeed seed,
                                         MaterialKind kind) noexcept {
    auto value = seed.salt ^ (seed.frame * 0x45d9f3bu);
    value ^= seed.node + 0x9e3779b9u + (value << 6u) + (value >> 2u);
    value ^= static_cast<std::uint32_t>(kind) * 0x27d4eb2du;
    return value;
}

inline unsigned int material_resolve_sample_taps(
        unsigned int max_sample_taps) noexcept {
    auto const bounded = std::min(max_sample_taps, material_max_sample_taps);
    if (bounded == 0u)
        return 0u;
    if (bounded < 5u)
        return 1u;
    if (bounded < 9u)
        return 5u;
    if (bounded < 13u)
        return 9u;
    if (bounded < material_max_sample_taps)
        return 13u;
    return material_max_sample_taps;
}

inline MaterialSamplingKernel material_resolve_sampling_kernel(
        bool backdrop_sampling,
        unsigned int sample_taps) noexcept {
    if (!backdrop_sampling || sample_taps == 0u)
        return {};
    if (sample_taps <= 1u)
        return MaterialSamplingKernel{
            "weighted-center",
            0u,
            sample_taps,
            0.0f,
            "center4",
            true,
            true,
        };
    if (sample_taps <= 5u)
        return MaterialSamplingKernel{
            "weighted-cross-5",
            1u,
            sample_taps,
            0.35f,
            "gaussian-cross-5-separable",
            true,
            true,
        };
    if (sample_taps <= 9u)
        return MaterialSamplingKernel{
            "weighted-3x3-grid",
            1u,
            sample_taps,
            0.35f,
            "gaussian-3x3-separable",
            true,
            true,
        };
    return MaterialSamplingKernel{
        "gaussian-5x5",
        2u,
        sample_taps,
        0.35f,
        "gaussian-5x5-separable",
        true,
        true,
    };
}

inline float material_safe_luma(float value, float fallback) noexcept {
    return std::isfinite(value)
        ? std::clamp(value, 0.0f, 1.0f)
        : fallback;
}

inline float material_srgb_channel_luma(unsigned char channel) noexcept {
    auto const value = static_cast<float>(channel) / 255.0f;
    return value <= 0.04045f
        ? value / 12.92f
        : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

inline float material_color_luma(Color color) noexcept {
    return 0.2126f * material_srgb_channel_luma(color.r)
        + 0.7152f * material_srgb_channel_luma(color.g)
        + 0.0722f * material_srgb_channel_luma(color.b);
}

inline float material_contrast_ratio(float a, float b) noexcept {
    auto const lighter = std::max(a, b);
    auto const darker = std::min(a, b);
    return (lighter + 0.05f) / (darker + 0.05f);
}

inline float material_contrast_ratio(Color foreground,
                                     float background_luma) noexcept {
    return material_contrast_ratio(
        material_color_luma(foreground),
        material_safe_luma(background_luma, 1.0f));
}

inline Color material_with_opaque_alpha(Color color) noexcept {
    color.a = 255;
    return color;
}

inline Color material_pick_readable_color(Color preferred,
                                          float background_luma,
                                          float minimum_contrast) noexcept {
    auto best = material_with_opaque_alpha(preferred);
    auto best_contrast = material_contrast_ratio(best, background_luma);
    if (best_contrast >= minimum_contrast)
        return best;
    auto const dark = Color{17, 24, 39, 255};
    auto const light = Color{255, 255, 255, 255};
    auto const absolute_dark = Color{0, 0, 0, 255};
    Color const candidates[3] = {dark, light, absolute_dark};
    for (auto candidate : candidates) {
        auto const contrast = material_contrast_ratio(candidate, background_luma);
        if (contrast > best_contrast) {
            best = candidate;
            best_contrast = contrast;
        }
    }
    return best;
}

inline float material_estimated_surface_luma(MaterialPlan const& plan) noexcept {
    auto const backdrop_luma = plan.backdrop.available
        ? plan.backdrop.luma_mean
        : material_color_luma(plan.tint);
    auto const tint_luma = material_color_luma(plan.tint);
    auto const alpha = std::clamp(
        plan.opacity * (static_cast<float>(plan.tint.a) / 255.0f),
        0.0f,
        1.0f);
    auto mixed = backdrop_luma * (1.0f - alpha) + tint_luma * alpha;
    if (plan.clear_glass_legibility.active) {
        auto const bright_gate =
            std::clamp((mixed - 0.54f) / 0.38f, 0.0f, 1.0f);
        auto const dimming =
            plan.clear_glass_legibility.dimming_strength
            * (0.38f + 0.62f * std::max(
                bright_gate,
                plan.clear_glass_legibility.brightness_response));
        mixed *= 1.0f - std::clamp(dimming, 0.0f, 0.34f);
        auto const contrast = std::clamp(
            plan.clear_glass_legibility.contrast_lift,
            0.0f,
            0.28f);
        mixed = (mixed - 0.50f) * (1.0f + contrast) + 0.50f;
    }
    return std::clamp(mixed + plan.luminance_floor * 0.08f, 0.0f, 1.0f);
}

inline char const* material_foreground_scheme_name(
        MaterialPlan const& plan,
        float background_luma,
        MaterialCapabilityInput capabilities) noexcept {
    if (plan.kind == MaterialKind::None)
        return "none";
    if (capabilities.increase_contrast)
        return "high-contrast";
    if (capabilities.reduce_transparency || plan.fallback())
        return "solid-fallback";
    if (!plan.backdrop_sampling)
        return "standard";
    if (background_luma < 0.48f)
        return "vibrant-light";
    if (background_luma > 0.68f)
        return "vibrant-dark";
    return "vibrant-balanced";
}

inline char const* material_foreground_source_name(
        MaterialPlan const& plan,
        MaterialCapabilityInput capabilities) noexcept {
    if (plan.kind == MaterialKind::None)
        return "none";
    if (capabilities.increase_contrast)
        return "accessibility";
    if (plan.backdrop_sampling)
        return "backdrop-analysis";
    if (plan.fallback())
        return "fallback-material";
    return "theme";
}

inline MaterialForegroundRecommendation material_resolve_foreground(
        MaterialPlan const& plan,
        MaterialStyle const& style,
        MaterialCapabilityInput capabilities) noexcept {
    MaterialForegroundRecommendation foreground{};
    foreground.background_luma = material_estimated_surface_luma(plan);
    foreground.minimum_contrast_ratio =
        capabilities.increase_contrast ? 7.0f : 4.5f;
    foreground.scheme = material_foreground_scheme_name(
        plan,
        foreground.background_luma,
        capabilities);
    foreground.source = material_foreground_source_name(plan, capabilities);
    foreground.contrast_policy = capabilities.increase_contrast
        ? "enhanced-contrast"
        : "standard-contrast";
    foreground.remap_policy = plan.kind == MaterialKind::None
        ? "none"
        : capabilities.increase_contrast
            ? "strict-theme-role-remap"
            : "theme-role-remap-if-needed";
    foreground.backdrop_driven = plan.backdrop_sampling;
    foreground.high_contrast = capabilities.increase_contrast;
    foreground.uses_vibrancy = plan.backdrop_sampling
        && !capabilities.reduce_transparency
        && !plan.fallback();
    foreground.deterministic = true;

    foreground.primary = material_pick_readable_color(
        style.foreground,
        foreground.background_luma,
        foreground.minimum_contrast_ratio);
    auto const primary_light =
        material_color_luma(foreground.primary) > foreground.background_luma;
    auto secondary_preferred = style.secondary_foreground;
    if (material_contrast_ratio(secondary_preferred,
                                foreground.background_luma)
            < foreground.minimum_contrast_ratio) {
        secondary_preferred = primary_light
            ? Color{226, 232, 240, 255}
            : Color{71, 85, 105, 255};
    }
    foreground.secondary = material_pick_readable_color(
        secondary_preferred,
        foreground.background_luma,
        foreground.minimum_contrast_ratio);

    auto accent_preferred = style.accent_foreground;
    if (material_contrast_ratio(accent_preferred,
                                foreground.background_luma)
            < foreground.minimum_contrast_ratio) {
        accent_preferred = style.strong_accent_foreground;
    }
    foreground.accent = material_pick_readable_color(
        accent_preferred,
        foreground.background_luma,
        foreground.minimum_contrast_ratio);

    foreground.primary_contrast_ratio =
        material_contrast_ratio(foreground.primary, foreground.background_luma);
    foreground.secondary_contrast_ratio =
        material_contrast_ratio(foreground.secondary, foreground.background_luma);
    foreground.accent_contrast_ratio =
        material_contrast_ratio(foreground.accent, foreground.background_luma);
    foreground.primary_contrast_margin =
        foreground.primary_contrast_ratio - foreground.minimum_contrast_ratio;
    foreground.secondary_contrast_margin =
        foreground.secondary_contrast_ratio - foreground.minimum_contrast_ratio;
    foreground.accent_contrast_margin =
        foreground.accent_contrast_ratio - foreground.minimum_contrast_ratio;
    return foreground;
}

inline bool material_color_rgb_equal(Color a, Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

inline Color material_color_from_contract_token(
        theme_contract::ColorToken color) noexcept {
    return Color{color.r, color.g, color.b, color.a};
}

inline MaterialThemeSnapshot material_resolve_theme_snapshot(
        MaterialStyle const& style) noexcept {
    MaterialThemeSnapshot snapshot{};
    auto const contract = theme_contract::default_glass_theme_contract();
    auto const default_foreground =
        material_color_from_contract_token(contract.foreground);
    auto const default_muted =
        material_color_from_contract_token(contract.muted);
    auto const default_accent =
        material_color_from_contract_token(contract.accent);
    auto const default_accent_strong =
        material_color_from_contract_token(contract.accent_strong);
    auto const default_surface =
        material_color_from_contract_token(contract.surface);
    auto const default_border =
        material_color_from_contract_token(contract.border);
    snapshot.foreground = style.foreground;
    snapshot.secondary_foreground = style.secondary_foreground;
    snapshot.accent_foreground = style.accent_foreground;
    snapshot.strong_accent_foreground = style.strong_accent_foreground;
    snapshot.tint = style.tint;
    snapshot.border = style.border;
    snapshot.foreground_matches_theme =
        style.foreground == default_foreground
        && style.secondary_foreground == default_muted;
    snapshot.accent_matches_theme =
        style.accent_foreground == default_accent
        && style.strong_accent_foreground == default_accent_strong;
    snapshot.tint_matches_surface =
        style.kind == MaterialKind::None
        || material_color_rgb_equal(style.tint, default_surface);
    snapshot.border_matches_theme =
        style.kind == MaterialKind::None
        || material_color_rgb_equal(style.border, default_border);
    snapshot.default_glass_tokens =
        snapshot.foreground_matches_theme
        && snapshot.accent_matches_theme
        && snapshot.tint_matches_surface
        && snapshot.border_matches_theme
        && contract.profile_name == "apple-glass-light"
        && contract.typography.default_font_family == "Pretendard";
    snapshot.profile_name = snapshot.default_glass_tokens
        ? "apple-glass-light"
        : "custom";
    return snapshot;
}

inline Color material_preserve_text_alpha(Color resolved,
                                          Color original) noexcept {
    resolved.a = original.a;
    return resolved;
}

inline bool material_point_inside_geometry(MaterialGeometry geometry,
                                           float x,
                                           float y) noexcept {
    return geometry.w > 0.0f
        && geometry.h > 0.0f
        && x >= geometry.x
        && y >= geometry.y
        && x <= geometry.x + geometry.w
        && y <= geometry.y + geometry.h;
}

inline MaterialForegroundTextResolution material_resolve_text_foreground(
        std::span<MaterialRuntimeRecord const> records,
        std::uint32_t text_command_index,
        float text_x,
        float text_y,
        Color text_color,
        Theme const& theme) noexcept {
    MaterialForegroundTextResolution result{};
    result.color = text_color;

    for (auto it = records.rbegin(); it != records.rend(); ++it) {
        auto const& record = *it;
        auto const& plan = record.plan;
        if (record.command_index >= text_command_index)
            continue;
        if (plan.kind == MaterialKind::None)
            continue;
        if (!material_point_inside_geometry(plan.geometry, text_x, text_y))
            continue;

        result.has_material = true;
        result.material_command_index = record.command_index;
        auto const style = material_style_for_kind(plan.kind, theme);

        if (material_color_rgb_equal(text_color, style.foreground)) {
            result.color = material_preserve_text_alpha(
                plan.foreground.primary,
                text_color);
            result.role = "primary";
            result.remapped = true;
            return result;
        }
        if (material_color_rgb_equal(text_color, style.secondary_foreground)) {
            result.color = material_preserve_text_alpha(
                plan.foreground.secondary,
                text_color);
            result.role = "secondary";
            result.remapped = true;
            return result;
        }
        if (material_color_rgb_equal(text_color, style.accent_foreground)
            || material_color_rgb_equal(
                text_color,
                style.strong_accent_foreground)) {
            result.color = material_preserve_text_alpha(
                plan.foreground.accent,
                text_color);
            result.role = "accent";
            result.remapped = true;
            return result;
        }
        return result;
    }

    return result;
}

inline MaterialBackdropAnalysis analyze_material_backdrop(
        MaterialBackdropDescriptor backdrop) noexcept {
    MaterialBackdropAnalysis analysis{};
    analysis.available = backdrop.available;
    analysis.stable = backdrop.stable;
    analysis.excludes_foreground_text = backdrop.excludes_foreground_text;
    analysis.color_mean = backdrop.color_mean;
    analysis.color_sample_count = backdrop.color_sample_count;
    analysis.color_sample_status =
        backdrop.color_sample_status && backdrop.color_sample_status[0]
            ? backdrop.color_sample_status
            : "not-sampled";
    analysis.luma_min = material_safe_luma(backdrop.luma_min, 0.0f);
    analysis.luma_max = std::max(
        analysis.luma_min,
        material_safe_luma(backdrop.luma_max, 1.0f));
    analysis.luma_mean = std::clamp(
        material_safe_luma(backdrop.luma_mean, 0.5f),
        analysis.luma_min,
        analysis.luma_max);
    analysis.luma_span = analysis.luma_max - analysis.luma_min;
    analysis.luma_sample_count = backdrop.luma_sample_count;
    analysis.luma_sample_grid_width = backdrop.luma_sample_grid_width;
    analysis.luma_sample_grid_height = backdrop.luma_sample_grid_height;
    analysis.luma_sample_frame = backdrop.luma_sample_frame;
    analysis.luma_sample_status =
        backdrop.luma_sample_status && backdrop.luma_sample_status[0]
            ? backdrop.luma_sample_status
            : "not-sampled";
    analysis.source = backdrop.source && backdrop.source[0]
        ? backdrop.source
        : "none";
    return analysis;
}

inline MaterialRenderTargetAnalysis analyze_material_render_target(
        MaterialRenderTargetMetadata target,
        std::int64_t max_backdrop_pixels) noexcept {
    MaterialRenderTargetAnalysis analysis{};
    analysis.width = std::max(0, target.width);
    analysis.height = std::max(0, target.height);
    analysis.scale = std::isfinite(target.scale) && target.scale > 0.0f
        ? target.scale
        : 1.0f;
    analysis.pixel_format = target.pixel_format && target.pixel_format[0]
        ? target.pixel_format
        : "unknown";
    analysis.ready = analysis.width > 0 && analysis.height > 0;
    analysis.pixel_count = analysis.ready
        ? static_cast<std::int64_t>(analysis.width)
            * static_cast<std::int64_t>(analysis.height)
        : std::int64_t{0};
    analysis.within_backdrop_budget =
        analysis.pixel_count <= max_backdrop_pixels;
    return analysis;
}

inline std::int64_t material_estimate_surface_sample_pixels(
        MaterialShapeAnalysis shape,
        MaterialRenderTargetAnalysis target) noexcept {
    if (!shape.valid)
        return 0;
    return material_estimate_surface_sample_pixels_from_area(
        static_cast<double>(shape.surface_area),
        target);
}

inline std::int64_t material_estimate_surface_sample_pixels(
        MaterialPlan const& plan) noexcept {
    if (!plan.shape.valid)
        return 0;
    auto x = plan.geometry.x;
    auto y = plan.geometry.y;
    auto w = plan.geometry.w;
    auto h = plan.geometry.h;
    auto radius = plan.shape.effective_radius;
    material_apply_materialize_execution_geometry(
        plan.transition,
        x,
        y,
        w,
        h,
        radius);
    auto const inflate = material_background_paint_inflate(plan);
    w = std::max(0.0f, w + inflate * 2.0f);
    h = std::max(0.0f, h + inflate * 2.0f);
    return material_estimate_surface_sample_pixels_from_area(
        static_cast<double>(w) * static_cast<double>(h),
        plan.render_target);
}

inline MaterialShapeAnalysis analyze_material_shape(
        MaterialGeometry geometry) noexcept {
    MaterialShapeAnalysis analysis{};
    bool const width_ready = std::isfinite(geometry.w) && geometry.w > 0.0f;
    bool const height_ready = std::isfinite(geometry.h) && geometry.h > 0.0f;
    analysis.valid = width_ready && height_ready;
    if (!analysis.valid)
        return analysis;

    auto const surface_area = geometry.w * geometry.h;
    analysis.surface_area = std::isfinite(surface_area) ? surface_area : 0.0f;
    analysis.min_extent = std::min(geometry.w, geometry.h);
    analysis.max_extent = std::max(geometry.w, geometry.h);
    analysis.radius_limit = analysis.min_extent * 0.5f;
    auto const requested_radius = std::isfinite(geometry.radius)
        ? geometry.radius
        : 0.0f;
    analysis.effective_radius =
        std::clamp(requested_radius, 0.0f, analysis.radius_limit);
    analysis.rounded = analysis.effective_radius > 0.0f;
    analysis.radius_clamped =
        std::fabs(analysis.effective_radius - requested_radius) > 0.0001f;
    analysis.normalized_radius = analysis.radius_limit > 0.0f
        ? analysis.effective_radius / analysis.radius_limit
        : 0.0f;
    analysis.capsule = analysis.rounded
        && analysis.normalized_radius >= 0.999f;
    analysis.kind = analysis.capsule
        ? MaterialShapeKind::Capsule
        : analysis.rounded
            ? MaterialShapeKind::RoundedRectangle
            : MaterialShapeKind::Rectangle;
    return analysis;
}

inline float material_sanitize_container_spacing(float spacing) noexcept {
    return std::isfinite(spacing) ? std::max(0.0f, spacing) : 0.0f;
}

inline char const* material_container_blend_policy_name(
        MaterialContainerMode mode,
        float spacing) noexcept {
    if (mode == MaterialContainerMode::Isolated)
        return "isolated";
    if (spacing <= 0.0f)
        return "touching-only";
    if (mode == MaterialContainerMode::Union)
        return "union-shape-proximity";
    return "container-shape-proximity";
}

inline char const* material_container_morph_policy_name(
        MaterialContainerMode mode,
        bool morph_transitions,
        bool reduced_motion_suppressed_morph) noexcept {
    if (mode == MaterialContainerMode::Isolated)
        return "isolated";
    if (reduced_motion_suppressed_morph)
        return "reduced-motion-static";
    if (!morph_transitions)
        return "static-container";
    if (mode == MaterialContainerMode::Union)
        return "union-morph";
    return "container-morph";
}

inline char const* material_container_performance_policy_name(
        MaterialContainerMode mode) noexcept {
    if (mode == MaterialContainerMode::Union)
        return "shared-union-capture";
    if (mode == MaterialContainerMode::Container)
        return "shared-container-capture";
    return "single-surface";
}

inline MaterialContainerAnalysis analyze_material_container(
        MaterialContainerDescriptor descriptor,
        bool reduce_motion) noexcept {
    MaterialContainerAnalysis analysis{};
    analysis.container_id = descriptor.container_id;
    analysis.union_id = descriptor.union_id;
    analysis.spacing = material_sanitize_container_spacing(descriptor.spacing);
    analysis.blend_distance = analysis.spacing;
    analysis.interactive = descriptor.interactive;
    analysis.participates = descriptor.participates();
    analysis.mode = descriptor.mode();
    analysis.mode_name = material_container_mode_name(analysis.mode);
    analysis.shared_backdrop_scope = analysis.participates;
    analysis.shape_union_expected =
        analysis.mode == MaterialContainerMode::Union;
    analysis.requested_morph_transitions =
        descriptor.morph_transitions && analysis.participates;
    analysis.reduced_motion_suppressed_morph =
        analysis.requested_morph_transitions && reduce_motion;
    analysis.morph_transitions =
        analysis.requested_morph_transitions
        && !analysis.reduced_motion_suppressed_morph;
    analysis.shape_blending_expected =
        analysis.participates && analysis.blend_distance > 0.0f;
    analysis.spacing_clamped =
        !std::isfinite(descriptor.spacing)
        || std::fabs(analysis.spacing - descriptor.spacing) > 0.0001f;
    analysis.blend_policy = material_container_blend_policy_name(
        analysis.mode,
        analysis.blend_distance);
    analysis.morph_policy = material_container_morph_policy_name(
        analysis.mode,
        analysis.morph_transitions,
        analysis.reduced_motion_suppressed_morph);
    analysis.performance_policy =
        material_container_performance_policy_name(analysis.mode);
    return analysis;
}

inline float material_transition_progress(float progress) noexcept {
    return std::isfinite(progress)
        ? std::clamp(progress, 0.0f, 1.0f)
        : 1.0f;
}

inline float material_smooth_transition_gain(float progress) noexcept {
    progress = material_transition_progress(progress);
    return progress * progress * (3.0f - 2.0f * progress);
}

inline MaterialTransitionAnalysis analyze_material_transition(
        MaterialTransitionDescriptor descriptor,
        bool reduce_motion) noexcept {
    MaterialTransitionAnalysis analysis{};
    analysis.kind = descriptor.kind;
    analysis.kind_name = material_glass_transition_kind_name(descriptor.kind);
    analysis.appearing = descriptor.appearing;
    analysis.progress = material_transition_progress(descriptor.progress);
    analysis.materialize =
        descriptor.kind == MaterialGlassTransitionKind::Materialize;
    analysis.matched_geometry =
        descriptor.kind == MaterialGlassTransitionKind::MatchedGeometry;
    analysis.reduced_motion_suppressed =
        reduce_motion && descriptor.kind != MaterialGlassTransitionKind::Identity;
    if (analysis.reduced_motion_suppressed) {
        analysis.progress = 1.0f;
        analysis.policy = "reduced-motion-static";
        return analysis;
    }
    analysis.active =
        descriptor.kind != MaterialGlassTransitionKind::Identity
        && analysis.progress < 0.999f;
    if (!analysis.active)
        return analysis;
    auto const gain = material_smooth_transition_gain(analysis.progress);
    if (analysis.materialize) {
        analysis = material_transition_as_materialize(analysis);
    } else if (analysis.matched_geometry) {
        analysis.opacity_gain = 1.0f;
        analysis.optical_gain = std::clamp(0.65f + 0.35f * gain, 0.0f, 1.0f);
        analysis.shadow_gain = 1.0f;
        analysis.refraction_gain =
            std::clamp(0.70f + 0.30f * gain, 0.0f, 1.0f);
        analysis.policy = "matched-geometry";
    }
    return analysis;
}

inline MaterialGlassIdentityAnalysis analyze_material_glass_identity(
        MaterialGlassIdentityDescriptor descriptor,
        MaterialContainerAnalysis const& container,
        MaterialTransitionAnalysis const& transition) noexcept {
    MaterialGlassIdentityAnalysis analysis{};
    analysis.namespace_id = descriptor.namespace_id;
    analysis.effect_id = descriptor.effect_id;
    analysis.namespace_scoped = descriptor.namespace_id != 0u;
    analysis.effect_identified = descriptor.effect_id != 0u;
    analysis.participates = descriptor.participates();
    analysis.container_scoped = analysis.participates && container.participates;
    analysis.matched_geometry_candidate =
        analysis.participates && transition.matched_geometry;
    if (!analysis.namespace_scoped && !analysis.effect_identified) {
        analysis.source = "none";
        analysis.policy = "unidentified";
    } else if (!analysis.participates) {
        analysis.source = "partial-glass-effect-id";
        analysis.policy = "incomplete-effect-id";
    } else if (analysis.matched_geometry_candidate && analysis.container_scoped) {
        analysis.source = "glass-effect-id";
        analysis.policy = "matched-geometry-container";
    } else if (analysis.matched_geometry_candidate) {
        analysis.source = "glass-effect-id";
        analysis.policy = "matched-geometry";
    } else if (analysis.container_scoped) {
        analysis.source = "glass-effect-id";
        analysis.policy = "container-identity";
    } else {
        analysis.source = "glass-effect-id";
        analysis.policy = "view-identity";
    }
    return analysis;
}

inline MaterialGlassBackgroundAnalysis analyze_material_glass_background(
        MaterialGlassBackgroundDescriptor descriptor) noexcept {
    MaterialGlassBackgroundAnalysis analysis{};
    analysis.kind = descriptor.kind;
    analysis.kind_name =
        material_glass_background_effect_kind_name(descriptor.kind);
    analysis.active = descriptor.active();
    analysis.automatic =
        descriptor.kind == MaterialGlassBackgroundEffectKind::Automatic;
    analysis.plate =
        descriptor.kind == MaterialGlassBackgroundEffectKind::Plate;
    analysis.feathered =
        descriptor.kind == MaterialGlassBackgroundEffectKind::Feathered;
    analysis.feather_padding =
        material_glass_background_non_negative(descriptor.feather_padding);
    analysis.soft_edge_radius =
        material_glass_background_non_negative(descriptor.soft_edge_radius);
    if (!analysis.active) {
        analysis.optical_profile = "none";
        analysis.edge_policy = "none";
    } else if (analysis.plate) {
        analysis.optical_profile = "plate";
        analysis.edge_policy = "bounded-plate";
    } else if (analysis.feathered) {
        analysis.optical_profile = "feathered";
        analysis.edge_policy = "soft-feathered-edge";
    } else {
        analysis.optical_profile = "automatic";
        analysis.edge_policy = "container-relative";
    }
    return analysis;
}

inline unsigned char material_scale_alpha(unsigned char alpha,
                                          float gain) noexcept {
    return static_cast<unsigned char>(std::clamp(
        std::lround(
            static_cast<float>(alpha)
            * std::clamp(gain, 0.0f, 1.0f)),
        0l,
        255l));
}

inline void apply_material_transition_policy(MaterialPlan& plan) noexcept {
    if (!plan.transition.active)
        return;
    auto const opacity_gain = plan.transition.opacity_gain;
    auto const optical_gain = plan.transition.optical_gain;
    auto const shadow_gain = plan.transition.shadow_gain;
    plan.opacity = std::clamp(plan.opacity * opacity_gain, 0.0f, 1.0f);
    plan.tint.a = material_scale_alpha(plan.tint.a, opacity_gain);
    plan.blur_radius = std::clamp(
        plan.blur_radius * optical_gain,
        0.0f,
        plan.resource_budget.max_blur_radius);
    plan.saturation = 1.0f + (plan.saturation - 1.0f) * optical_gain;
    plan.edge_highlight = std::clamp(
        plan.edge_highlight * optical_gain,
        0.0f,
        1.0f);
    plan.noise_opacity = std::clamp(
        plan.noise_opacity * optical_gain,
        0.0f,
        0.05f);
    plan.shadow_alpha = std::clamp(
        plan.shadow_alpha * shadow_gain,
        0.0f,
        0.4f);
    plan.shadow_radius = std::max(0.0f, plan.shadow_radius * shadow_gain);
}

inline char const* material_luminance_response_name(
        bool dark,
        bool bright,
        bool flat) noexcept {
    if (dark && flat)
        return "dark-flat";
    if (bright && flat)
        return "bright-flat";
    if (dark)
        return "dark";
    if (bright)
        return "bright";
    if (flat)
        return "flat";
    return "neutral";
}

inline char const* material_frosting_response_name(
        bool dark,
        bool bright,
        bool flat) noexcept {
    if (dark)
        return "dark-frost-lift";
    if (bright)
        return "bright-frost-thin";
    if (flat)
        return "flat-edge-frost";
    return "balanced";
}

inline char const* material_tint_response_name(
        bool dark,
        bool bright,
        bool flat) noexcept {
    if (dark)
        return "lift-dark-backdrop";
    if (bright)
        return "thin-bright-backdrop";
    if (flat)
        return "stabilize-flat-backdrop";
    return "preserve";
}

inline float material_color_channel_fraction(unsigned char value) noexcept {
    return static_cast<float>(value) / 255.0f;
}

inline float material_rgb_distance(Color a, Color b) noexcept {
    auto const red = std::fabs(
        material_color_channel_fraction(a.r)
        - material_color_channel_fraction(b.r));
    auto const green = std::fabs(
        material_color_channel_fraction(a.g)
        - material_color_channel_fraction(b.g));
    auto const blue = std::fabs(
        material_color_channel_fraction(a.b)
        - material_color_channel_fraction(b.b));
    return std::max(red, std::max(green, blue));
}

inline float material_colorfulness(Color color) noexcept {
    auto const red = material_color_channel_fraction(color.r);
    auto const green = material_color_channel_fraction(color.g);
    auto const blue = material_color_channel_fraction(color.b);
    auto const high = std::max(red, std::max(green, blue));
    auto const low = std::min(red, std::min(green, blue));
    return high - low;
}

inline char const* material_color_response_name(float colorfulness,
                                                float distance) noexcept {
    if (distance <= 0.025f)
        return "preserve";
    if (colorfulness <= 0.025f)
        return "neutral-backdrop-color";
    return "sampled-backdrop-color";
}

inline char const* material_saturation_response_name(
        bool dark,
        bool bright,
        bool flat) noexcept {
    if (dark)
        return "lift-dark-color";
    if (bright)
        return "compress-bright-color";
    if (flat)
        return "restore-flat-color";
    return "preserve";
}

inline char const* material_depth_response_name(
        bool dark,
        bool bright,
        bool flat) noexcept {
    if (dark)
        return "soften-dark-depth";
    if (bright)
        return "restore-bright-depth";
    if (flat)
        return "restore-flat-depth";
    return "standard";
}

inline float material_apply_scalar_delta(float& value,
                                         float delta,
                                         float min_value,
                                         float max_value) noexcept {
    if (std::fabs(delta) <= 0.000001f)
        return 0.0f;
    auto const before = value;
    auto const lower = std::min(min_value, before);
    auto const upper = std::max(max_value, before);
    value = std::clamp(value + delta, lower, upper);
    return value - before;
}

inline float material_apply_tint_alpha_delta(Color& tint,
                                             float delta) noexcept {
    auto const before = tint.a;
    auto const adjusted =
        static_cast<int>(before)
        + static_cast<int>(std::round(delta * 255.0f));
    tint.a = static_cast<unsigned char>(std::clamp(adjusted, 0, 255));
    return static_cast<float>(
        static_cast<int>(tint.a) - static_cast<int>(before)) / 255.0f;
}

inline void apply_backdrop_luminance_policy(MaterialPlan& plan) noexcept {
    auto const luma_mean = plan.backdrop.luma_mean;
    auto const luma_span = plan.backdrop.luma_span;
    auto const floor_before = plan.luminance_floor;
    auto const gain_before = plan.luminance_gain;
    auto const edge_before = plan.edge_highlight;
    auto const opacity_before = plan.opacity;
    auto const saturation_before = plan.saturation;
    auto const shadow_alpha_before = plan.shadow_alpha;
    auto const shadow_radius_before = plan.shadow_radius;
    bool dark = false;
    bool bright = false;
    bool flat = false;
    float darkness = 0.0f;
    float brightness = 0.0f;
    float flatness = 0.0f;

    if (luma_mean < 0.35f) {
        dark = true;
        darkness = std::clamp((0.35f - luma_mean) / 0.35f, 0.0f, 1.0f);
        plan.luminance_floor = std::max(
            plan.luminance_floor,
            0.08f + 0.08f * darkness);
        plan.luminance_gain = std::max(
            plan.luminance_gain,
            1.08f + 0.10f * darkness);
        plan.edge_highlight = std::min(
            1.0f,
            plan.edge_highlight + 0.06f * darkness);
    } else if (luma_mean > 0.72f) {
        bright = true;
        brightness = std::clamp((luma_mean - 0.72f) / 0.28f, 0.0f, 1.0f);
        plan.luminance_gain = std::max(
            1.0f,
            plan.luminance_gain - 0.04f * brightness);
        plan.edge_highlight = std::min(
            1.0f,
            plan.edge_highlight + 0.04f * brightness);
    }

    if (luma_span < 0.12f) {
        flat = true;
        flatness = std::clamp((0.12f - luma_span) / 0.12f, 0.0f, 1.0f);
        plan.edge_highlight = std::min(
            1.0f,
            plan.edge_highlight + 0.04f * flatness);
    }

    plan.backdrop.opacity_delta = material_apply_scalar_delta(
        plan.opacity,
        0.035f * darkness - 0.020f * brightness + 0.015f * flatness,
        0.0f,
        1.0f);
    plan.backdrop.tint_alpha_delta = material_apply_tint_alpha_delta(
        plan.tint,
        0.045f * darkness - 0.030f * brightness + 0.015f * flatness);
    plan.backdrop.saturation_delta = material_apply_scalar_delta(
        plan.saturation,
        0.040f * darkness - 0.045f * brightness + 0.055f * flatness,
        0.0f,
        1.3f);
    plan.backdrop.shadow_alpha_delta = material_apply_scalar_delta(
        plan.shadow_alpha,
        -0.015f * darkness + 0.025f * brightness + 0.015f * flatness,
        0.0f,
        0.4f);
    plan.backdrop.shadow_radius_delta = material_apply_scalar_delta(
        plan.shadow_radius,
        -0.50f * darkness + 1.50f * brightness + 0.75f * flatness,
        0.0f,
        40.0f);

    plan.backdrop.luminance_response =
        material_luminance_response_name(dark, bright, flat);
    plan.backdrop.frosting_response =
        material_frosting_response_name(dark, bright, flat);
    plan.backdrop.tint_response =
        material_tint_response_name(dark, bright, flat);
    plan.backdrop.saturation_response =
        material_saturation_response_name(dark, bright, flat);
    plan.backdrop.depth_response =
        material_depth_response_name(dark, bright, flat);
    plan.backdrop.luminance_floor_delta =
        plan.luminance_floor - floor_before;
    plan.backdrop.luminance_gain_delta =
        plan.luminance_gain - gain_before;
    plan.backdrop.edge_highlight_delta =
        plan.edge_highlight - edge_before;
    plan.backdrop.response_strength =
        std::max(std::max(darkness, brightness), flatness);
    if (plan.backdrop.response_strength < 0.0001f) {
        plan.backdrop.opacity_delta = plan.opacity - opacity_before;
        plan.backdrop.saturation_delta = plan.saturation - saturation_before;
        plan.backdrop.shadow_alpha_delta =
            plan.shadow_alpha - shadow_alpha_before;
        plan.backdrop.shadow_radius_delta =
            plan.shadow_radius - shadow_radius_before;
    }
}

inline void apply_backdrop_color_policy(MaterialPlan& plan) noexcept {
    if (!plan.backdrop_sampling || plan.backdrop.color_sample_count == 0) {
        plan.backdrop.color_response = "not-sampled";
        return;
    }

    auto const source = plan.backdrop.color_mean;
    auto const colorfulness = material_colorfulness(source);
    auto const distance = material_rgb_distance(plan.tint, source);
    plan.backdrop.color_response =
        material_color_response_name(colorfulness, distance);
    if (std::string_view{plan.backdrop.color_response}
        != "sampled-backdrop-color") {
        return;
    }

    auto const before = plan.tint;
    auto target = source;
    target.a = before.a;
    auto const response_strength = std::clamp(
        0.06f
            + 0.10f * colorfulness
            + 0.04f * std::clamp(plan.backdrop.response_strength, 0.0f, 1.0f),
        0.0f,
        0.18f);
    plan.tint = lerp_color(before, target, response_strength);
    plan.tint.a = before.a;
    plan.backdrop.tint_color_delta =
        material_rgb_distance(before, plan.tint);
}

inline float material_large_surface_legibility_role_scale(
        MaterialSurfaceRole role) noexcept {
    switch (role) {
        case MaterialSurfaceRole::Sidebar: return 1.18f;
        case MaterialSurfaceRole::Navigation: return 1.08f;
        case MaterialSurfaceRole::Toolbar: return 0.92f;
        case MaterialSurfaceRole::StatusBar: return 0.82f;
        case MaterialSurfaceRole::Overlay: return 0.76f;
        case MaterialSurfaceRole::Surface: return 0.56f;
        case MaterialSurfaceRole::Control:
        case MaterialSurfaceRole::Content:
            return 0.0f;
    }
    return 0.0f;
}

inline char const* material_large_surface_legibility_source_name(
        MaterialSurfaceRole role,
        bool brightness_driven,
        bool contrast_driven) noexcept {
    if (role == MaterialSurfaceRole::Sidebar)
        return "sidebar-large-glass-legibility";
    if (role == MaterialSurfaceRole::Navigation)
        return "navigation-large-glass-legibility";
    if (brightness_driven && contrast_driven)
        return "bright-detail-large-glass-legibility";
    if (contrast_driven)
        return "detailed-large-glass-legibility";
    if (brightness_driven)
        return "bright-large-glass-legibility";
    return "large-glass-legibility";
}

inline MaterialLargeSurfaceLegibilityProfile
apply_material_large_surface_legibility_policy(MaterialPlan& plan) noexcept {
    MaterialLargeSurfaceLegibilityProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None) {
        return profile;
    }

    auto const role_scale =
        material_large_surface_legibility_role_scale(plan.role);
    if (role_scale <= 0.0001f)
        return profile;

    auto const width = std::max(plan.geometry.w, 1.0f);
    auto const height = std::max(plan.geometry.h, 1.0f);
    auto const area_root = std::sqrt(width * height);
    auto const min_dim = std::min(width, height);
    auto const area_response =
        std::clamp((area_root - 180.0f) / 440.0f, 0.0f, 1.0f);
    auto const span_response =
        std::clamp((min_dim - 88.0f) / 260.0f, 0.0f, 1.0f);
    auto const size_response =
        std::clamp(area_response + 0.42f * span_response, 0.0f, 1.0f);
    if (size_response <= 0.0500f)
        return profile;

    auto const luma_mean = std::clamp(plan.backdrop.luma_mean, 0.0f, 1.0f);
    auto const luma_span = std::clamp(plan.backdrop.luma_span, 0.0f, 1.0f);
    auto const bright_response =
        std::clamp((luma_mean - 0.62f) / 0.38f, 0.0f, 1.0f);
    auto const dark_response =
        std::clamp((0.38f - luma_mean) / 0.38f, 0.0f, 1.0f);
    auto const contrast_response =
        std::clamp((luma_span - 0.18f) / 0.50f, 0.0f, 1.0f);
    auto const flat_response =
        std::clamp((0.14f - luma_span) / 0.14f, 0.0f, 1.0f);
    auto const response = std::clamp(
        role_scale
            * (0.52f * size_response
               + 0.20f * contrast_response
               + 0.14f * bright_response
               + 0.08f * dark_response
               + 0.06f * flat_response),
        0.0f,
        1.0f);
    if (response <= 0.0001f)
        return profile;

    profile.active = true;
    profile.role_driven = role_scale > 0.0001f;
    profile.size_driven = size_response > 0.0001f;
    profile.backdrop_driven = true;
    profile.contrast_driven =
        contrast_response > 0.0001f || flat_response > 0.0001f;
    profile.brightness_driven = bright_response > 0.0001f;
    profile.response_strength = response;
    profile.model = "large-surface-liquid-glass-legibility";
    profile.source = material_large_surface_legibility_source_name(
        plan.role,
        profile.brightness_driven,
        profile.contrast_driven);

    profile.opacity_delta = material_apply_scalar_delta(
        plan.opacity,
        response
            * (0.030f
               + 0.055f * size_response
               + 0.026f * contrast_response
               + 0.020f * bright_response),
        0.0f,
        0.96f);
    profile.tint_alpha_delta = material_apply_tint_alpha_delta(
        plan.tint,
        response
            * (0.040f
               + 0.060f * size_response
               + 0.032f * contrast_response
               + 0.018f * bright_response));
    profile.luminance_floor_delta = material_apply_scalar_delta(
        plan.luminance_floor,
        response
            * (0.014f
               + 0.034f * dark_response
               + 0.022f * contrast_response
               + 0.016f * flat_response),
        0.0f,
        0.30f);
    profile.edge_highlight_delta = material_apply_scalar_delta(
        plan.edge_highlight,
        response
            * (0.020f
               + 0.026f * contrast_response
               + 0.018f * bright_response
               + 0.014f * dark_response),
        0.0f,
        1.0f);
    if (plan.quality_policy.allow_shadow) {
        profile.shadow_alpha_delta = material_apply_scalar_delta(
            plan.shadow_alpha,
            response
                * (0.008f
                   + 0.015f * size_response
                   + 0.010f * contrast_response),
            0.0f,
            0.4f);
        profile.shadow_radius_delta = material_apply_scalar_delta(
            plan.shadow_radius,
            response
                * (0.60f
                   + 1.20f * size_response
                   + 0.55f * contrast_response),
            0.0f,
            40.0f);
    }
    return profile;
}

inline float material_clamped_pointer_coordinate(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.5f;
}

inline char const* material_interaction_state_name(
        MaterialInteractionDescriptor input) noexcept {
    if (input.pressed)
        return "pressed";
    if (input.hovered || input.pointer_inside)
        return "hovered";
    if (input.focused)
        return "focused";
    return "idle";
}

inline float material_interaction_strength(
        MaterialInteractionDescriptor input) noexcept {
    float strength = 0.0f;
    if (input.focused)
        strength = std::max(strength, 0.24f);
    if (input.hovered || input.pointer_inside)
        strength = std::max(strength, 0.48f);
    if (input.pressed)
        strength = std::max(strength, 1.0f);
    return strength;
}

inline char const* material_interaction_enablement_reason(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "inactive-material";
    if (!plan.container.interactive)
        return "noninteractive-container";
    return "interactive-container";
}

inline MaterialInteractionResponse material_resolve_interaction_response(
        MaterialPlan const& plan,
        MaterialInteractionDescriptor input,
        bool reduce_motion) noexcept {
    MaterialInteractionResponse response{};
    response.enablement_reason =
        material_interaction_enablement_reason(plan);
    response.enabled = plan.kind != MaterialKind::None
        && plan.container.interactive;
    response.hovered = input.hovered;
    response.pressed = input.pressed;
    response.focused = input.focused;
    response.pointer_inside = input.pointer_inside;
    response.reduce_motion = reduce_motion;
    response.pointer_x =
        material_clamped_pointer_coordinate(input.pointer_x);
    response.pointer_y =
        material_clamped_pointer_coordinate(input.pointer_y);
    response.deterministic = true;
    if (!response.enabled)
        return response;

    response.state = material_interaction_state_name(input);
    response.response_model = "liquid-glass-interaction";
    response.motion_policy = reduce_motion
        ? "reduced-motion-static"
        : "animated-optical-response";
    auto strength = material_interaction_strength(input);
    if (reduce_motion)
        strength = std::min(strength, 0.36f);
    response.response_strength = strength;
    response.active = strength > 0.0f;
    auto const press = response.pressed ? strength : 0.0f;
    auto const hover = response.pressed
        ? 0.0f
        : ((response.hovered || response.pointer_inside) ? strength : 0.0f);
    auto const focus = (!response.pressed
        && !(response.hovered || response.pointer_inside)
        && response.focused) ? strength : 0.0f;
    auto const motion_scale = reduce_motion ? 0.35f : 1.0f;
    if (response.active) {
        response.control_morph_model = response.pressed
            ? "pressed-liquid-control-morph"
            : ((response.hovered || response.pointer_inside)
                ? "hover-liquid-control-morph"
                : "focused-liquid-control-morph");
        response.control_morph_active = true;
        response.control_morph_reduced_motion_suppressed = reduce_motion;
        response.control_morph_scale_delta = std::clamp(
            motion_scale
                * (0.014f * hover + 0.006f * focus - 0.018f * press),
            -0.024f,
            0.020f);
        response.control_morph_depth = std::clamp(
            motion_scale * (0.15f * hover + 0.30f * press + 0.08f * focus),
            0.0f,
            0.34f);
        response.control_morph_edge = std::clamp(
            motion_scale * (0.08f * hover + 0.16f * press + 0.04f * focus),
            0.0f,
            0.26f);
        response.control_morph_shadow = std::clamp(
            motion_scale * (0.08f * hover + 0.20f * press),
            0.0f,
            0.28f);
    }
    auto const pointer_driven =
        response.pointer_inside || response.hovered || response.pressed;
    if (pointer_driven && response.active) {
        auto const press_scale = response.pressed ? 1.0f : 0.0f;
        auto const pointer_motion_scale = reduce_motion ? 0.45f : 1.0f;
        response.specular_model = "pointer-specular";
        response.specular_highlight_active = true;
        response.specular_anchor_x = response.pointer_x;
        response.specular_anchor_y = response.pointer_y;
        response.specular_radius = std::clamp(
            0.34f - 0.10f * press_scale,
            0.20f,
            0.38f);
        response.specular_intensity = std::clamp(
            pointer_motion_scale * (0.42f + 0.18f * press_scale) * strength,
            0.0f,
            0.75f);
        response.pointer_lens_model = response.pressed
            ? "pressed-pointer-lens"
            : "hover-pointer-lens";
        response.pointer_lens_active = true;
        response.pointer_lens_anchor_x = response.pointer_x;
        response.pointer_lens_anchor_y = response.pointer_y;
        response.pointer_lens_radius = std::clamp(
            0.42f - 0.10f * press_scale,
            0.26f,
            0.44f);
        response.pointer_lens_strength = std::clamp(
            pointer_motion_scale * (0.16f + 0.10f * press_scale) * strength,
            0.0f,
            0.32f);
    }
    auto const focus_only =
        response.focused && !pointer_driven && response.active;
    if (focus_only) {
        auto const focus_motion_scale = reduce_motion ? 0.55f : 1.0f;
        response.specular_model = "focus-specular";
        response.specular_highlight_active = true;
        response.specular_anchor_x = 0.5f;
        response.specular_anchor_y = 0.5f;
        response.specular_radius = 0.52f;
        response.specular_intensity = std::clamp(
            focus_motion_scale * 0.30f * strength,
            0.0f,
            0.22f);
    }
    return response;
}

inline void apply_material_interaction_policy(MaterialPlan& plan) noexcept {
    auto response = material_resolve_interaction_response(
        plan,
        plan.command_descriptor.interaction,
        plan.decision_trace.reduce_motion);
    if (!response.enabled || !response.active) {
        plan.interaction = response;
        return;
    }

    auto const before_opacity = plan.opacity;
    auto const before_blur = plan.blur_radius;
    auto const before_saturation = plan.saturation;
    auto const before_edge = plan.edge_highlight;
    auto const before_shadow_alpha = plan.shadow_alpha;
    auto const before_shadow_radius = plan.shadow_radius;
    auto const strength = response.response_strength;
    auto const press = response.pressed ? strength : 0.0f;
    auto const hover = response.pressed
        ? 0.0f
        : ((response.hovered || response.pointer_inside) ? strength : 0.0f);
    auto const focus = (!response.pressed
        && !(response.hovered || response.pointer_inside)
        && response.focused) ? strength : 0.0f;
    auto const motion_scale = response.reduce_motion ? 0.35f : 1.0f;

    plan.opacity = std::clamp(
        plan.opacity + 0.026f * hover + 0.050f * press,
        0.0f,
        1.0f);
    if (plan.backdrop_sampling) {
        auto const blur_delta =
            motion_scale * (0.85f * hover - 1.15f * press);
        plan.blur_radius = std::clamp(
            plan.blur_radius + blur_delta,
            0.0f,
            plan.resource_budget.max_blur_radius);
        plan.saturation = std::clamp(
            plan.saturation
                + motion_scale * (0.025f * hover + 0.035f * press),
            0.0f,
            1.35f);
    }
    plan.edge_highlight = std::clamp(
        plan.edge_highlight
            + 0.055f * hover + 0.090f * press + 0.050f * focus,
        0.0f,
        1.0f);
    plan.shadow_alpha = std::clamp(
        plan.shadow_alpha + 0.012f * hover + 0.022f * press,
        0.0f,
        0.4f);
    plan.shadow_radius = std::clamp(
        plan.shadow_radius
            + motion_scale * (0.80f * hover + 1.20f * press),
        0.0f,
        40.0f);

    response.opacity_delta = plan.opacity - before_opacity;
    response.blur_radius_delta = plan.blur_radius - before_blur;
    response.saturation_delta = plan.saturation - before_saturation;
    response.edge_highlight_delta = plan.edge_highlight - before_edge;
    response.shadow_alpha_delta = plan.shadow_alpha - before_shadow_alpha;
    response.shadow_radius_delta = plan.shadow_radius - before_shadow_radius;
    plan.interaction = response;
}

inline char const* material_prominent_glass_source_name(
        bool control_driven,
        bool tint_driven,
        bool interaction_driven) noexcept {
    if (interaction_driven && control_driven && tint_driven)
        return "interactive-prominent-control-accent-glass";
    if (interaction_driven && control_driven)
        return "interactive-prominent-control-glass";
    if (interaction_driven && tint_driven)
        return "interactive-prominent-accent-glass";
    if (interaction_driven)
        return "interactive-prominent-glass";
    if (control_driven && tint_driven)
        return "prominent-control-accent-glass";
    if (control_driven)
        return "prominent-control-glass";
    if (tint_driven)
        return "prominent-accent-glass";
    return "prominent-glass";
}

inline MaterialProminentGlassProfile material_resolve_prominent_glass_profile(
        MaterialPlan const& plan) noexcept {
    MaterialProminentGlassProfile profile{};
    profile.bounded = true;
    auto const requested = plan.command_descriptor.prominence;
    if (!requested.enabled
        || !plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None) {
        return profile;
    }

    auto const requested_intensity = std::isfinite(requested.intensity)
        ? requested.intensity
        : 1.0f;
    auto const base_intensity =
        std::clamp(requested_intensity, 0.0f, 1.25f);
    if (base_intensity <= 0.0001f)
        return profile;

    auto const motion_scale = plan.decision_trace.reduce_motion ? 0.74f : 1.0f;
    auto const intensity = std::clamp(base_intensity * motion_scale, 0.0f, 1.0f);
    auto const tint_alpha = material_alpha_fraction(plan.tint);
    auto const contrast_response =
        plan.decision_trace.increase_contrast ? 1.0f : 0.0f;
    auto const interaction_response = plan.interaction.active
        ? std::clamp(plan.interaction.response_strength, 0.0f, 1.0f)
        : 0.0f;
    auto const pointer_lens_response = plan.interaction.pointer_lens_active
        ? std::clamp(
            plan.interaction.pointer_lens_strength / 0.32f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const press_response =
        plan.interaction.pressed ? interaction_response : 0.0f;
    profile.active = true;
    profile.control_driven = plan.role == MaterialSurfaceRole::Control;
    profile.tint_driven = !plan.theme.tint_matches_surface;
    profile.backdrop_driven = plan.backdrop_sampling;
    profile.interaction_driven = interaction_response > 0.0001f;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && motion_scale < 1.0f;
    profile.model = "prominent-liquid-glass-action";
    profile.source = material_prominent_glass_source_name(
        profile.control_driven,
        profile.tint_driven,
        profile.interaction_driven);
    profile.intensity = std::clamp(
        intensity + 0.10f * interaction_response,
        0.0f,
        1.0f);
    profile.tint_weight = std::clamp(
        0.12f
            + 0.28f * profile.intensity
            + 0.18f * tint_alpha
            + 0.08f * contrast_response
            + 0.050f * interaction_response
            + 0.040f * press_response
            + 0.030f * pointer_lens_response,
        0.0f,
        0.74f);
    profile.edge_lift = std::clamp(
        0.035f
            + 0.095f * profile.intensity
            + 0.045f * contrast_response
            + 0.040f * interaction_response
            + 0.035f * press_response
            + 0.025f * pointer_lens_response,
        0.0f,
        0.30f);
    profile.lensing_gain = std::clamp(
        1.0f
            + 0.16f * profile.intensity
            + 0.040f * contrast_response
            + 0.050f * interaction_response
            + 0.050f * press_response
            + 0.060f * pointer_lens_response,
        1.0f,
        1.36f);
    return profile;
}

inline float material_base_refraction_strength(MaterialKind kind) noexcept {
    switch (kind) {
        case MaterialKind::Clear: return 0.075f;
        case MaterialKind::Thin: return 0.095f;
        case MaterialKind::Regular: return 0.120f;
        case MaterialKind::Thick: return 0.150f;
        case MaterialKind::None: return 0.0f;
    }
    return 0.0f;
}

inline MaterialRefractionProfile material_resolve_refraction_profile(
        MaterialPlan const& plan) noexcept {
    MaterialRefractionProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling || plan.fallback() || plan.kind == MaterialKind::None)
        return profile;

    auto const thickness_strength =
        material_base_refraction_strength(plan.kind);
    auto const radius_strength =
        0.020f * std::clamp(plan.shape.normalized_radius, 0.0f, 1.0f);
    auto const interaction_boost =
        plan.interaction.active
            ? 0.035f * std::clamp(
                plan.interaction.response_strength,
                0.0f,
                1.0f)
            : 0.0f;
    auto const motion_scale = plan.decision_trace.reduce_motion ? 0.48f : 1.0f;
    auto const transition_gain =
        plan.transition.active ? plan.transition.refraction_gain : 1.0f;
    auto const strength = std::clamp(
        transition_gain
            * motion_scale
            * (thickness_strength + radius_strength + interaction_boost),
        0.0f,
        0.22f);
    if (strength <= 0.0001f)
        return profile;

    profile.active = true;
    profile.backdrop_driven = true;
    profile.interaction_driven = plan.interaction.active;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && motion_scale < 1.0f;
    profile.model = plan.interaction.pointer_lens_active
        ? "interactive-pointer-lens"
        : (profile.interaction_driven ? "interactive-edge-lens" : "edge-lens");
    profile.source = plan.interaction.pointer_lens_active
        ? "sampled-backdrop-pointer-refraction"
        : "sampled-backdrop-edge-refraction";
    profile.strength = strength;
    profile.edge_bias = std::clamp(
        0.32f + 0.28f * plan.shape.normalized_radius,
        0.20f,
        0.72f);
    auto const blur_offset =
        0.18f * std::clamp(plan.blur_radius, 0.0f, material_max_blur_radius);
    auto const edge_offset =
        0.65f * std::clamp(plan.edge_width, 0.5f, 8.0f);
    profile.max_offset_pixels = std::clamp(
        motion_scale * (blur_offset + edge_offset) * profile.strength,
        0.0f,
        3.50f);
    auto const backdrop_response =
        plan.backdrop_sampling
            ? std::clamp(plan.backdrop.response_strength, 0.0f, 1.0f)
            : 0.0f;
    auto const interaction_light =
        plan.interaction.active
            ? 0.10f * std::clamp(
                plan.interaction.response_strength,
                0.0f,
                1.0f)
            : 0.0f;
    auto const caustic_shape =
        0.18f
        + 0.22f * std::clamp(plan.edge_highlight, 0.0f, 1.0f)
        + 0.14f * std::clamp(plan.shape.normalized_radius, 0.0f, 1.0f)
        + 0.08f * backdrop_response
        + interaction_light;
    profile.edge_caustic_intensity = std::clamp(
        motion_scale * profile.strength * caustic_shape,
        0.0f,
        0.20f);
    return profile;
}

inline MaterialEdgeOpticsProfile material_resolve_edge_optics_profile(
        MaterialPlan const& plan) noexcept {
    MaterialEdgeOpticsProfile profile{};
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None
        || plan.edge_highlight <= 0.0001f) {
        return profile;
    }

    auto const shape_roundness =
        std::clamp(plan.shape.normalized_radius, 0.0f, 1.0f);
    auto const edge_highlight =
        std::clamp(plan.edge_highlight, 0.0f, 1.0f);
    auto const refraction_strength =
        plan.refraction.active
            ? std::clamp(plan.refraction.strength, 0.0f, 0.35f)
            : 0.0f;
    auto const caustic_response =
        plan.refraction.active
            ? std::clamp(plan.refraction.edge_caustic_intensity, 0.0f, 0.35f)
            : 0.0f;
    auto const backdrop_response =
        std::clamp(plan.backdrop.response_strength, 0.0f, 1.0f);
    auto const motion_scale = plan.decision_trace.reduce_motion ? 0.78f : 1.0f;
    auto const transition_gain =
        plan.transition.active ? plan.transition.optical_gain : 1.0f;

    profile.active = true;
    profile.backdrop_driven = true;
    profile.caustic_driven = caustic_response > 0.0001f;
    profile.model = "beveled-liquid-glass-edge";
    profile.source = profile.caustic_driven
        ? "sampled-backdrop-bevel-caustics"
        : "sampled-backdrop-bevel-lighting";
    profile.bevel_width = std::clamp(
        plan.edge_width * (1.35f + 1.20f * shape_roundness)
            + 0.035f * std::clamp(plan.blur_radius, 0.0f,
                                   material_max_blur_radius),
        0.75f,
        14.0f);
    profile.inner_highlight = std::clamp(
        transition_gain
            * motion_scale
            * (0.10f
               + 0.40f * edge_highlight
               + 0.75f * caustic_response
               + 0.08f * backdrop_response
               + 0.08f * shape_roundness),
        0.0f,
        0.65f);
    profile.outer_shadow = std::clamp(
        transition_gain
            * motion_scale
            * (0.06f
               + 0.22f * edge_highlight
               + 0.40f * caustic_response
               + 0.08f * std::clamp(plan.shadow_alpha, 0.0f, 0.4f)),
        0.0f,
        0.42f);
    profile.chromatic_fringe = std::clamp(
        motion_scale
            * (0.015f
               + 0.38f * caustic_response
               + 0.08f * refraction_strength),
        0.0f,
        0.12f);
    return profile;
}

inline char const* material_spectral_tint_source_name(
        bool tint_driven,
        bool color_driven,
        bool caustic_driven) noexcept {
    if (tint_driven && color_driven && caustic_driven)
        return "configured-tint-sampled-backdrop-spectral-caustics";
    if (tint_driven && color_driven)
        return "configured-tint-sampled-backdrop-spectral-tint";
    if (tint_driven && caustic_driven)
        return "configured-tint-spectral-caustics";
    if (tint_driven)
        return "configured-tint-spectral-rim";
    if (color_driven && caustic_driven)
        return "sampled-backdrop-spectral-caustics";
    if (color_driven)
        return "sampled-backdrop-spectral-tint";
    if (caustic_driven)
        return "sampled-backdrop-spectral-rim";
    return "sampled-backdrop-spectral-neutral";
}

inline MaterialSpectralTintProfile material_resolve_spectral_tint_profile(
        MaterialPlan const& plan) noexcept {
    MaterialSpectralTintProfile profile{};
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None
        || plan.backdrop.color_sample_count == 0u) {
        return profile;
    }

    auto const source = plan.backdrop.color_mean;
    auto const red = material_color_channel_fraction(source.r);
    auto const green = material_color_channel_fraction(source.g);
    auto const blue = material_color_channel_fraction(source.b);
    auto const luma = std::clamp(material_color_luma(source), 0.0f, 1.0f);
    auto const colorfulness = material_colorfulness(source);
    auto const configured_tint = plan.theme.tint;
    auto const tint_alpha = material_alpha_fraction(configured_tint);
    auto const tint_colorfulness = material_colorfulness(configured_tint);
    auto const tint_influence =
        std::clamp((tint_alpha - 0.12f) / 0.42f, 0.0f, 1.0f)
        * std::clamp((tint_colorfulness - 0.04f) / 0.56f, 0.0f, 1.0f);
    auto const tint_red = material_color_channel_fraction(configured_tint.r);
    auto const tint_green = material_color_channel_fraction(configured_tint.g);
    auto const tint_blue = material_color_channel_fraction(configured_tint.b);
    auto const tint_warm_bias = std::clamp(
        tint_red - tint_blue + 0.28f * std::max(0.0f, tint_red - tint_green),
        0.0f,
        1.0f);
    auto const tint_cool_bias = std::clamp(
        tint_blue - tint_red + 0.22f * std::max(0.0f, tint_green - tint_red),
        0.0f,
        1.0f);
    auto const warm_bias = std::clamp(
        red - blue + 0.25f * std::max(0.0f, red - green),
        0.0f,
        1.0f);
    auto const cool_bias = std::clamp(
        blue - red + 0.20f * std::max(0.0f, green - red),
        0.0f,
        1.0f);
    auto const caustic = std::clamp(
        plan.refraction.edge_caustic_intensity
            + plan.edge_optics.chromatic_fringe,
        0.0f,
        0.35f);
    auto const backdrop_response =
        std::clamp(plan.backdrop.response_strength, 0.0f, 1.0f);
    auto const motion_scale = plan.decision_trace.reduce_motion ? 0.72f : 1.0f;
    auto const transition_gain =
        plan.transition.active ? plan.transition.optical_gain : 1.0f;
    auto const base = std::clamp(
        transition_gain
            * motion_scale
            * (0.012f
               + 0.060f * colorfulness
               + 0.052f * tint_influence
               + 0.50f * caustic
               + 0.025f * backdrop_response),
        0.0f,
        0.20f);

    profile.warmth = std::clamp(
        base * (0.44f + 1.20f * warm_bias + 0.10f * luma)
            + 0.085f * tint_influence * (0.34f + 1.15f * tint_warm_bias),
        0.0f,
        0.22f);
    profile.coolness = std::clamp(
        base * (0.44f + 1.20f * cool_bias + 0.10f * (1.0f - luma))
            + 0.085f * tint_influence * (0.34f + 1.15f * tint_cool_bias),
        0.0f,
        0.22f);
    profile.dispersion = std::clamp(
        base * (0.52f + 0.70f * std::clamp(plan.refraction.strength,
                                           0.0f,
                                           0.35f))
            + 0.55f * plan.edge_optics.chromatic_fringe
            + 0.24f * plan.refraction.edge_caustic_intensity
            + 0.040f * tint_influence,
        0.0f,
        0.22f);
    profile.rim_tint = std::clamp(
        0.040f
            + 0.45f * base
            + 0.35f * std::max(profile.warmth, profile.coolness)
            + 0.050f * tint_influence,
        0.0f,
        0.28f);

    auto const tint_total = profile.warmth + profile.coolness;
    profile.balance = tint_total > 0.0001f
        ? std::clamp(
            0.5f + 0.5f * ((profile.warmth - profile.coolness) / tint_total),
            0.0f,
            1.0f)
        : 0.5f;

    auto const strongest =
        std::max(
            std::max(profile.warmth, profile.coolness),
            std::max(profile.dispersion, profile.rim_tint));
    if (strongest <= 0.0001f)
        return MaterialSpectralTintProfile{};

    profile.active = true;
    profile.backdrop_driven = true;
    profile.color_driven = colorfulness > 0.025f;
    profile.tint_driven = tint_influence > 0.0001f;
    profile.caustic_driven = caustic > 0.0001f;
    profile.bounded = true;
    profile.tint_influence = tint_influence;
    profile.model = "spectral-liquid-glass-tint";
    profile.source = material_spectral_tint_source_name(
        profile.tint_driven,
        profile.color_driven,
        profile.caustic_driven);
    return profile;
}

inline char const* material_dynamic_lighting_source_name(
        bool color_driven,
        bool caustic_driven,
        bool interaction_driven) noexcept {
    if (interaction_driven && color_driven)
        return "sampled-backdrop-interactive-color-lighting";
    if (interaction_driven)
        return "sampled-backdrop-interactive-lighting";
    if (color_driven && caustic_driven)
        return "sampled-backdrop-color-caustic-lighting";
    if (color_driven)
        return "sampled-backdrop-color-lighting";
    if (caustic_driven)
        return "sampled-backdrop-caustic-lighting";
    return "sampled-backdrop-dynamic-lighting";
}

inline MaterialDynamicLightingProfile material_resolve_dynamic_lighting_profile(
        MaterialPlan const& plan) noexcept {
    MaterialDynamicLightingProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None
        || plan.backdrop.color_sample_count == 0u) {
        return profile;
    }

    auto const source = plan.backdrop.color_mean;
    auto const red = material_color_channel_fraction(source.r);
    auto const green = material_color_channel_fraction(source.g);
    auto const blue = material_color_channel_fraction(source.b);
    auto const colorfulness = material_colorfulness(source);
    auto const sample_luma = plan.backdrop.luma_sample_count > 0u
        ? plan.backdrop.luma_mean
        : material_color_luma(source);
    auto const luma_mean = std::clamp(sample_luma, 0.0f, 1.0f);
    auto const luma_span = std::clamp(plan.backdrop.luma_span, 0.0f, 1.0f);
    auto const backdrop_response =
        std::clamp(plan.backdrop.response_strength, 0.0f, 1.0f);
    auto const edge_highlight =
        std::clamp(plan.edge_highlight, 0.0f, 1.0f);
    auto const caustic =
        std::clamp(plan.refraction.edge_caustic_intensity, 0.0f, 0.35f);
    auto const spectral_energy = std::clamp(
        std::max(plan.spectral_tint.warmth, plan.spectral_tint.coolness)
            + 0.45f * plan.spectral_tint.rim_tint,
        0.0f,
        0.32f);
    auto const interaction_response = plan.interaction.active
        ? std::clamp(plan.interaction.response_strength, 0.0f, 1.0f)
        : 0.0f;
    auto const motion_scale = plan.decision_trace.reduce_motion ? 0.66f : 1.0f;
    auto const shift_scale = plan.decision_trace.reduce_motion ? 0.42f : 1.0f;
    auto const transition_gain =
        plan.transition.active ? plan.transition.optical_gain : 1.0f;

    profile.highlight_strength = std::clamp(
        transition_gain
            * motion_scale
            * (0.045f
               + 0.150f * edge_highlight
               + 0.58f * caustic
               + 0.055f * backdrop_response
               + 0.18f * spectral_energy
               + 0.045f * interaction_response),
        0.0f,
        0.38f);
    profile.shadow_strength = std::clamp(
        transition_gain
            * motion_scale
            * (0.035f
               + 0.28f * std::clamp(plan.shadow_alpha, 0.0f, 0.45f)
               + 0.090f * luma_span
               + 0.46f * caustic
               + 0.025f * interaction_response),
        0.0f,
        0.30f);

    if (profile.highlight_strength <= 0.0001f
        && profile.shadow_strength <= 0.0001f) {
        return MaterialDynamicLightingProfile{};
    }

    auto const color_shift = std::clamp(
        (blue - red) * 0.16f + (green - red) * 0.06f,
        -0.22f,
        0.22f);
    auto const luma_shift = std::clamp(
        (luma_mean - 0.5f) * 0.18f,
        -0.12f,
        0.12f);
    auto const pointer_shift_x = plan.interaction.pointer_lens_active
        ? (plan.interaction.pointer_x - 0.5f) * 0.20f
        : 0.0f;
    auto const pointer_shift_y = plan.interaction.pointer_lens_active
        ? (plan.interaction.pointer_y - 0.5f) * 0.16f
        : 0.0f;
    auto const raw_x =
        -0.58f + shift_scale * (color_shift + pointer_shift_x);
    auto const raw_y =
        -0.82f + shift_scale * (luma_shift + pointer_shift_y);
    auto const length = std::sqrt(raw_x * raw_x + raw_y * raw_y);
    if (length > 0.0001f) {
        profile.direction_x = std::clamp(raw_x / length, -0.98f, 0.98f);
        profile.direction_y = std::clamp(raw_y / length, -0.98f, 0.98f);
    } else {
        profile.direction_x = -0.58f;
        profile.direction_y = -0.82f;
    }

    profile.active = true;
    profile.backdrop_driven = true;
    profile.color_driven = colorfulness > 0.025f;
    profile.caustic_driven = caustic > 0.0001f;
    profile.interaction_driven = plan.interaction.active;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && motion_scale < 1.0f;
    profile.model = profile.interaction_driven
        ? "interactive-dynamic-glass-light"
        : "dynamic-liquid-glass-light";
    profile.source = material_dynamic_lighting_source_name(
        profile.color_driven,
        profile.caustic_driven,
        profile.interaction_driven);
    return profile;
}

inline float material_base_glass_thickness(MaterialKind kind) noexcept {
    switch (kind) {
        case MaterialKind::Clear: return 0.10f;
        case MaterialKind::Thin: return 0.16f;
        case MaterialKind::Regular: return 0.24f;
        case MaterialKind::Thick: return 0.34f;
        case MaterialKind::None: return 0.0f;
    }
    return 0.0f;
}

inline char const* material_glass_thickness_source_name(
        bool size_driven,
        bool transition_driven,
        bool interaction_driven) noexcept {
    if (transition_driven && interaction_driven)
        return "transition-interactive-thickness-lensing";
    if (transition_driven)
        return "transition-thickness-lensing";
    if (interaction_driven)
        return "interactive-thickness-lensing";
    if (size_driven)
        return "size-adaptive-thickness-lensing";
    return "material-thickness-lensing";
}

inline MaterialGlassThicknessProfile material_resolve_glass_thickness_profile(
        MaterialPlan const& plan) noexcept {
    MaterialGlassThicknessProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None) {
        return profile;
    }

    auto const width = std::max(plan.geometry.w, 1.0f);
    auto const height = std::max(plan.geometry.h, 1.0f);
    auto const min_dim = std::min(width, height);
    auto const area_root = std::sqrt(width * height);
    auto const area_scale =
        std::clamp((area_root - 96.0f) / 420.0f, 0.0f, 1.0f);
    auto const span_scale =
        std::clamp((min_dim - 44.0f) / 240.0f, 0.0f, 1.0f);
    auto const size_response =
        std::clamp(area_scale + 0.35f * span_scale, 0.0f, 1.0f);
    auto const shape_response =
        std::clamp(plan.shape.normalized_radius, 0.0f, 1.0f);
    auto const transition_response = plan.transition.active
        ? std::clamp(
            0.22f
                + 0.36f * (1.0f - plan.transition.optical_gain)
                + 0.24f * (1.0f - plan.transition.refraction_gain)
                + (plan.transition.matched_geometry ? 0.18f : 0.0f),
            0.0f,
            1.0f)
        : 0.0f;
    auto const interaction_response = plan.interaction.active
        ? std::clamp(plan.interaction.response_strength, 0.0f, 1.0f)
        : 0.0f;
    auto const motion_scale = plan.decision_trace.reduce_motion ? 0.72f : 1.0f;

    profile.thickness = std::clamp(
        motion_scale
            * (material_base_glass_thickness(plan.kind)
               + 0.20f * size_response
               + 0.08f * shape_response
               + 0.12f * transition_response
               + 0.06f * interaction_response),
        0.0f,
        0.72f);
    if (profile.thickness <= 0.0001f)
        return MaterialGlassThicknessProfile{};

    profile.active = true;
    profile.size_driven = size_response > 0.0001f;
    profile.transition_driven = transition_response > 0.0001f;
    profile.interaction_driven = interaction_response > 0.0001f;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && motion_scale < 1.0f;
    profile.model = "adaptive-glass-thickness";
    profile.source = material_glass_thickness_source_name(
        profile.size_driven,
        profile.transition_driven,
        profile.interaction_driven);
    profile.lensing_gain = std::clamp(
        1.0f + 0.34f * profile.thickness + 0.12f * size_response,
        1.0f,
        1.42f);
    profile.shadow_gain = std::clamp(
        1.0f + 0.28f * profile.thickness + 0.18f * transition_response,
        1.0f,
        1.38f);
    profile.scattering_gain = std::clamp(
        1.0f
            + 0.30f * profile.thickness
            + (profile.size_driven ? 0.12f : 0.0f)
            + 0.08f * interaction_response,
        1.0f,
        1.36f);
    return profile;
}

inline char const* material_glass_dispersion_source_name(
        bool spectral_driven,
        bool thickness_driven,
        bool transition_driven,
        bool lighting_driven,
        bool interaction_driven) noexcept {
    if (transition_driven && interaction_driven)
        return "transition-interactive-prismatic-dispersion";
    if (interaction_driven)
        return "interactive-prismatic-glass-dispersion";
    if (transition_driven && lighting_driven)
        return "transition-directional-prismatic-dispersion";
    if (transition_driven)
        return "transition-prismatic-dispersion";
    if (thickness_driven && lighting_driven)
        return "directional-thickness-prismatic-dispersion";
    if (thickness_driven)
        return "thickness-prismatic-dispersion";
    if (spectral_driven)
        return "spectral-prismatic-dispersion";
    return "edge-prismatic-dispersion";
}

inline MaterialGlassDispersionProfile material_resolve_glass_dispersion_profile(
        MaterialPlan const& plan) noexcept {
    MaterialGlassDispersionProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None
        || !plan.refraction.active) {
        return profile;
    }

    auto const spectral_energy = plan.spectral_tint.active
        ? std::clamp(
            plan.spectral_tint.dispersion
                + 0.35f * plan.spectral_tint.rim_tint
                + 0.20f * std::max(plan.spectral_tint.warmth,
                                    plan.spectral_tint.coolness),
            0.0f,
            0.36f)
        : 0.0f;
    auto const thickness = plan.glass_thickness.active
        ? std::clamp(plan.glass_thickness.thickness, 0.0f, 1.0f)
        : 0.0f;
    auto const lensing = plan.glass_thickness.active
        ? std::clamp(plan.glass_thickness.lensing_gain - 1.0f, 0.0f, 0.60f)
        : 0.0f;
    auto const caustic = std::clamp(
        plan.refraction.edge_caustic_intensity
            + plan.edge_optics.chromatic_fringe,
        0.0f,
        0.42f);
    auto const lighting = plan.dynamic_lighting.active
        ? std::clamp(
            plan.dynamic_lighting.highlight_strength
                + plan.dynamic_lighting.shadow_strength,
            0.0f,
            0.74f)
        : 0.0f;
    auto const interaction_response = plan.interaction.active
        ? std::clamp(plan.interaction.response_strength, 0.0f, 1.0f)
        : 0.0f;
    auto const transition_response = plan.transition.active
        ? std::clamp(
            0.24f
                + 0.34f * (1.0f - plan.transition.optical_gain)
                + 0.28f * (1.0f - plan.transition.refraction_gain)
                + (plan.transition.matched_geometry ? 0.20f : 0.0f),
            0.0f,
            1.0f)
        : 0.0f;
    auto const motion_scale = plan.decision_trace.reduce_motion ? 0.58f : 1.0f;
    auto const transition_gain = plan.transition.active
        ? std::clamp(
            1.0f
                + 0.16f * (1.0f - plan.transition.optical_gain)
                + 0.12f * (1.0f - plan.transition.refraction_gain)
                + (plan.transition.matched_geometry ? 0.10f : 0.0f),
            1.0f,
            1.28f)
        : 1.0f;
    auto const response = std::clamp(
        spectral_energy
            + 0.26f * thickness
            + 0.58f * caustic
            + 0.22f * lensing
            + 0.12f * transition_response
            + 0.10f * lighting
            + 0.08f * interaction_response,
        0.0f,
        1.0f);
    if (response <= 0.0001f)
        return profile;

    profile.axial_offset_pixels = std::clamp(
        motion_scale
            * transition_gain
            * (0.10f
               + plan.refraction.max_offset_pixels
                   * (0.20f + 0.90f * spectral_energy)
               + 1.45f * thickness * caustic
               + 0.64f * transition_response
               + 0.38f * lensing),
        0.0f,
        2.80f);
    profile.tangential_offset_pixels = std::clamp(
        motion_scale
            * (0.08f
               + 0.50f * profile.axial_offset_pixels
               + 0.80f * caustic
               + 0.26f * transition_response
               + 0.22f * lighting),
        0.0f,
        2.10f);
    profile.prismatic_gain = std::clamp(
        1.0f
            + motion_scale
                * (0.52f * spectral_energy
                   + 0.34f * thickness
                   + 0.62f * caustic
                   + 0.20f * transition_response
                   + 0.18f * lighting
                   + 0.12f * interaction_response),
        1.0f,
        1.66f);
    profile.caustic_spread = std::clamp(
        motion_scale
            * (0.18f * caustic
               + 0.12f * spectral_energy
               + 0.08f * thickness
               + 0.08f * transition_response
               + 0.06f * interaction_response),
        0.0f,
        0.34f);

    if (profile.axial_offset_pixels <= 0.0001f
        && profile.tangential_offset_pixels <= 0.0001f
        && profile.caustic_spread <= 0.0001f) {
        return MaterialGlassDispersionProfile{};
    }

    profile.active = true;
    profile.spectral_driven = spectral_energy > 0.0001f;
    profile.thickness_driven = thickness > 0.0001f;
    profile.transition_driven = transition_response > 0.0001f;
    profile.lighting_driven = lighting > 0.0001f;
    profile.interaction_driven = interaction_response > 0.0001f;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && motion_scale < 1.0f;
    profile.model = "prismatic-glass-dispersion";
    profile.source = material_glass_dispersion_source_name(
        profile.spectral_driven,
        profile.thickness_driven,
        profile.transition_driven,
        profile.lighting_driven,
        profile.interaction_driven);
    return profile;
}

inline bool material_role_supports_scroll_edge(
        MaterialSurfaceRole role) noexcept {
    switch (role) {
        case MaterialSurfaceRole::Toolbar:
        case MaterialSurfaceRole::Sidebar:
        case MaterialSurfaceRole::StatusBar:
        case MaterialSurfaceRole::Navigation:
            return true;
        case MaterialSurfaceRole::Surface:
        case MaterialSurfaceRole::Content:
        case MaterialSurfaceRole::Overlay:
        case MaterialSurfaceRole::Control:
            return false;
    }
    return false;
}

inline char const* material_scroll_edge_source_name(
        bool hard_style,
        bool contrast_driven,
        bool backdrop_driven) noexcept {
    if (hard_style)
        return "sampled-backdrop-hard-scroll-edge";
    if (contrast_driven)
        return "sampled-backdrop-contrast-scroll-edge";
    if (backdrop_driven)
        return "sampled-backdrop-dissolving-scroll-edge";
    return "role-scroll-edge";
}

inline MaterialScrollEdgeProfile material_resolve_scroll_edge_profile(
        MaterialPlan const& plan) noexcept {
    MaterialScrollEdgeProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None
        || !material_role_supports_scroll_edge(plan.role)
        || plan.backdrop.luma_sample_count == 0u) {
        return profile;
    }

    auto const luma_mean = std::clamp(plan.backdrop.luma_mean, 0.0f, 1.0f);
    auto const luma_span = std::clamp(plan.backdrop.luma_span, 0.0f, 1.0f);
    auto const dark_response =
        std::clamp((0.46f - luma_mean) / 0.46f, 0.0f, 1.0f);
    auto const bright_response =
        std::clamp((luma_mean - 0.70f) / 0.30f, 0.0f, 1.0f);
    auto const content_detail =
        std::clamp((luma_span - 0.10f) / 0.52f, 0.0f, 1.0f);
    auto const flat_mid_luma =
        std::clamp((0.14f - luma_span) / 0.14f, 0.0f, 1.0f)
        * (1.0f - std::max(dark_response, bright_response));
    auto const backdrop_response =
        std::clamp(plan.backdrop.response_strength, 0.0f, 1.0f);
    auto const shape_response =
        std::clamp(plan.shape.normalized_radius, 0.0f, 1.0f);
    auto const thickness = plan.glass_thickness.active
        ? std::clamp(plan.glass_thickness.thickness, 0.0f, 1.0f)
        : 0.0f;
    auto const motion_scale = plan.decision_trace.reduce_motion ? 0.74f : 1.0f;
    auto const role_scale = plan.role == MaterialSurfaceRole::Sidebar
        ? 1.18f
        : (plan.role == MaterialSurfaceRole::Navigation ? 1.08f : 1.0f);

    profile.fade_extent_pixels = std::clamp(
        role_scale
            * (plan.edge_width * 3.5f
               + 0.42f * std::clamp(plan.blur_radius, 0.0f,
                                     material_max_blur_radius)
               + 0.12f * std::min(plan.geometry.w, plan.geometry.h)
               + 8.0f * content_detail
               + 6.0f * flat_mid_luma),
        8.0f,
        72.0f);
    profile.dissolve_strength = std::clamp(
        motion_scale
            * (0.08f
               + 0.24f * content_detail
               + 0.12f * backdrop_response
               + 0.08f * thickness
               + 0.04f * shape_response),
        0.0f,
        0.46f);
    profile.dimming_strength = std::clamp(
        0.02f
            + 0.20f * dark_response
            + 0.06f * bright_response
            + 0.10f * content_detail
            + 0.08f * backdrop_response,
        0.0f,
        0.36f);
    profile.hard_style_strength = std::clamp(
        flat_mid_luma * (0.30f + 0.30f * role_scale),
        0.0f,
        0.56f);
    profile.response_strength = std::clamp(
        0.36f * (profile.dissolve_strength / 0.46f)
            + 0.40f * (profile.dimming_strength / 0.36f)
            + 0.30f * (profile.hard_style_strength / 0.56f)
            + 0.10f * backdrop_response,
        0.0f,
        1.0f);

    if (profile.dissolve_strength <= 0.0001f
        && profile.dimming_strength <= 0.0001f
        && profile.hard_style_strength <= 0.0001f) {
        return MaterialScrollEdgeProfile{};
    }

    profile.active = true;
    profile.role_driven = true;
    profile.backdrop_driven = true;
    profile.contrast_driven =
        dark_response > 0.0001f
        || bright_response > 0.0001f
        || content_detail > 0.0001f;
    profile.hard_style = profile.hard_style_strength > 0.0001f;
    profile.model = "adaptive-glass-scroll-edge";
    profile.source = material_scroll_edge_source_name(
        profile.hard_style,
        profile.contrast_driven,
        profile.backdrop_driven);
    return profile;
}

inline void apply_material_scroll_edge_legibility_policy(
        MaterialPlan& plan) noexcept {
    auto& profile = plan.scroll_edge;
    if (!profile.active
        || !plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None) {
        return;
    }

    auto const response =
        std::clamp(profile.response_strength, 0.0f, 1.0f);
    if (response <= 0.0001f)
        return;

    auto const dissolve =
        std::clamp(profile.dissolve_strength / 0.46f, 0.0f, 1.0f);
    auto const dimming =
        std::clamp(profile.dimming_strength / 0.36f, 0.0f, 1.0f);
    auto const hard =
        std::clamp(profile.hard_style_strength / 0.56f, 0.0f, 1.0f);
    auto const contrast =
        profile.contrast_driven ? 1.0f : 0.0f;

    profile.opacity_delta = material_apply_scalar_delta(
        plan.opacity,
        response * (0.010f + 0.020f * dimming + 0.014f * hard),
        0.0f,
        0.96f);
    profile.tint_alpha_delta = material_apply_tint_alpha_delta(
        plan.tint,
        response
            * (0.012f
               + 0.018f * dissolve
               + 0.016f * dimming
               + 0.010f * hard));
    profile.luminance_floor_delta = material_apply_scalar_delta(
        plan.luminance_floor,
        response * (0.006f + 0.016f * dimming + 0.014f * hard),
        0.0f,
        0.32f);
    profile.edge_highlight_delta = material_apply_scalar_delta(
        plan.edge_highlight,
        response
            * (0.008f
               + 0.014f * dissolve
               + 0.016f * hard
               + 0.006f * contrast),
        0.0f,
        1.0f);
    profile.legibility_driven =
        std::fabs(profile.opacity_delta) > 0.0001f
        || std::fabs(profile.tint_alpha_delta) > 0.0001f
        || std::fabs(profile.luminance_floor_delta) > 0.0001f
        || std::fabs(profile.edge_highlight_delta) > 0.0001f;
}

inline char const* material_clear_glass_legibility_source_name(
        bool accessibility_driven,
        bool brightness_driven,
        bool detail_driven,
        bool contrast_driven) noexcept {
    if (accessibility_driven)
        return "accessibility-clear-glass-legibility-dimming";
    if (brightness_driven && detail_driven)
        return "sampled-clear-glass-bright-detail-dimming";
    if (brightness_driven)
        return "sampled-clear-glass-brightness-dimming";
    if (detail_driven || contrast_driven)
        return "sampled-clear-glass-detail-dimming";
    return "clear-glass-legibility-dimming";
}

inline MaterialClearGlassLegibilityProfile
material_resolve_clear_glass_legibility_profile(
        MaterialPlan const& plan) noexcept {
    MaterialClearGlassLegibilityProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind != MaterialKind::Clear
        || plan.backdrop.luma_sample_count == 0u) {
        return profile;
    }

    auto const luma_mean = std::clamp(plan.backdrop.luma_mean, 0.0f, 1.0f);
    auto const luma_span = std::clamp(plan.backdrop.luma_span, 0.0f, 1.0f);
    auto const brightness_response =
        std::clamp((luma_mean - 0.62f) / 0.38f, 0.0f, 1.0f);
    auto const detail_response =
        std::clamp((luma_span - 0.18f) / 0.46f, 0.0f, 1.0f);
    auto const flat_contrast_response =
        std::clamp((0.16f - luma_span) / 0.16f, 0.0f, 1.0f)
        * (1.0f - brightness_response);
    auto const accessibility_response =
        plan.decision_trace.increase_contrast ? 1.0f : 0.0f;
    auto const response = std::clamp(
        0.22f
            + 0.44f * brightness_response
            + 0.26f * detail_response
            + 0.20f * flat_contrast_response
            + 0.28f * accessibility_response,
        0.0f,
        1.0f);

    profile.dimming_strength = std::clamp(
        0.050f
            + 0.145f * brightness_response
            + 0.060f * detail_response
            + 0.038f * flat_contrast_response
            + 0.055f * accessibility_response,
        0.0f,
        0.30f);
    profile.contrast_lift = std::clamp(
        0.030f
            + 0.075f * detail_response
            + 0.060f * brightness_response
            + 0.045f * flat_contrast_response
            + 0.060f * accessibility_response,
        0.0f,
        0.24f);
    if (response <= 0.0001f
        || (profile.dimming_strength <= 0.0001f
            && profile.contrast_lift <= 0.0001f)) {
        return MaterialClearGlassLegibilityProfile{};
    }

    profile.active = true;
    profile.backdrop_driven = true;
    profile.brightness_driven = brightness_response > 0.0001f;
    profile.detail_driven = detail_response > 0.0001f;
    profile.contrast_driven = flat_contrast_response > 0.0001f;
    profile.accessibility_driven = accessibility_response > 0.0001f;
    profile.brightness_response = brightness_response;
    profile.detail_response =
        std::max(detail_response, flat_contrast_response);
    profile.model = "adaptive-clear-glass-legibility";
    profile.source = material_clear_glass_legibility_source_name(
        profile.accessibility_driven,
        profile.brightness_driven,
        profile.detail_driven,
        profile.contrast_driven);
    return profile;
}

inline char const* material_glass_stabilization_source_name(
        bool transition_driven,
        bool container_driven,
        bool interaction_driven,
        bool backdrop_driven) noexcept {
    if (transition_driven && container_driven)
        return "container-transition-glass-stabilization";
    if (transition_driven)
        return "transition-glass-stabilization";
    if (container_driven)
        return "container-glass-stabilization";
    if (interaction_driven)
        return "interactive-glass-stabilization";
    if (backdrop_driven)
        return "sampled-backdrop-glass-stabilization";
    return "glass-stabilization";
}

inline MaterialGlassStabilizationProfile
material_resolve_glass_stabilization_profile(
        MaterialPlan const& plan) noexcept {
    MaterialGlassStabilizationProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None
        || plan.backdrop.luma_sample_count == 0u) {
        return profile;
    }

    auto const luma_span = std::clamp(plan.backdrop.luma_span, 0.0f, 1.0f);
    auto const luma_mean = std::clamp(plan.backdrop.luma_mean, 0.0f, 1.0f);
    auto const backdrop_detail =
        std::clamp((luma_span - 0.10f) / 0.48f, 0.0f, 1.0f);
    auto const backdrop_flat =
        std::clamp((0.18f - luma_span) / 0.18f, 0.0f, 1.0f);
    auto const bright_response =
        std::clamp((luma_mean - 0.66f) / 0.34f, 0.0f, 1.0f);
    auto const dark_response =
        std::clamp((0.38f - luma_mean) / 0.38f, 0.0f, 1.0f);
    auto const backdrop_busy = std::clamp(
        backdrop_detail * 0.62f
            + plan.backdrop.response_strength * 0.20f
            + std::max(bright_response, dark_response) * 0.18f,
        0.0f,
        1.0f);
    auto const thickness_response = plan.glass_thickness.active
        ? std::clamp(plan.glass_thickness.thickness, 0.0f, 1.0f)
        : 0.0f;
    auto const dispersion_response = plan.glass_dispersion.active
        ? std::clamp(
            plan.glass_dispersion.axial_offset_pixels / 2.80f * 0.30f
                + plan.glass_dispersion.tangential_offset_pixels / 2.10f
                    * 0.28f
                + (plan.glass_dispersion.prismatic_gain - 1.0f) / 0.66f
                    * 0.22f
                + plan.glass_dispersion.caustic_spread / 0.34f * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const transition_response = plan.transition.active
        ? std::clamp(
            0.18f
                + 0.28f * plan.transition.materialize_wave_strength
                + 0.22f * (1.0f - plan.transition.optical_gain)
                + 0.20f * (1.0f - plan.transition.refraction_gain)
                + 0.14f * std::clamp(1.0f - plan.transition.progress,
                                      0.0f,
                                      1.0f)
                + (plan.transition.matched_geometry ? 0.14f : 0.0f),
            0.0f,
            1.0f)
        : 0.0f;
    auto const container_response = plan.container.participates
        ? std::clamp(
            0.18f
                + (plan.container.shape_blending_expected ? 0.20f : 0.0f)
                + (plan.container.morph_transitions ? 0.18f : 0.0f)
                + (plan.container.shared_backdrop_scope ? 0.16f : 0.0f)
                + std::clamp(plan.container.blend_distance / 96.0f,
                             0.0f,
                             1.0f)
                    * 0.16f
                + std::clamp(plan.container.spacing / 64.0f, 0.0f, 1.0f)
                    * 0.08f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const interaction_response = plan.interaction.active
        ? std::clamp(
            plan.interaction.response_strength * 0.58f
                + plan.interaction.pointer_lens_strength * 0.24f
                + plan.interaction.control_morph_depth * 0.18f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const clear_glass_response = plan.clear_glass_legibility.active
        ? std::clamp(
            plan.clear_glass_legibility.detail_response * 0.42f
                + plan.clear_glass_legibility.brightness_response * 0.28f
                + plan.clear_glass_legibility.contrast_lift / 0.24f * 0.30f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const reduced_motion_scale =
        plan.decision_trace.reduce_motion ? 0.62f : 1.0f;
    auto const motion_response = std::clamp(
        transition_response * 0.32f
            + container_response * 0.24f
            + interaction_response * 0.20f
            + dispersion_response * 0.14f
            + thickness_response * 0.10f,
        0.0f,
        1.0f);
    auto const response = std::clamp(
        0.16f
            + 0.24f * backdrop_busy
            + 0.16f * clear_glass_response
            + 0.16f * motion_response
            + 0.12f * thickness_response
            + 0.10f * dispersion_response
            + 0.06f * backdrop_flat,
        0.0f,
        1.0f);

    profile.strength = std::clamp(
        reduced_motion_scale
            * (0.12f
               + 0.30f * response
               + 0.16f * motion_response
               + 0.12f * backdrop_busy),
        0.0f,
        1.0f);
    profile.damping = std::clamp(
        0.28f
            + 0.26f * backdrop_busy
            + 0.18f * motion_response
            + 0.14f * clear_glass_response
            + 0.12f * thickness_response
            + (plan.decision_trace.reduce_motion ? 0.10f : 0.0f),
        0.0f,
        1.0f);
    profile.shimmer_reduction = std::clamp(
        0.10f
            + 0.34f * backdrop_busy
            + 0.20f * dispersion_response
            + 0.16f * motion_response
            + 0.12f * clear_glass_response
            + (plan.decision_trace.reduce_motion ? 0.18f : 0.0f),
        0.0f,
        1.0f);
    profile.transmission_bias = std::clamp(
        0.035f * backdrop_flat
            + 0.030f * thickness_response
            + 0.024f * container_response
            - 0.075f * backdrop_busy
            - 0.030f * transition_response,
        -0.18f,
        0.18f);

    if (profile.strength <= 0.0001f
        && profile.damping <= 0.0001f
        && profile.shimmer_reduction <= 0.0001f) {
        return MaterialGlassStabilizationProfile{};
    }

    profile.active = true;
    profile.backdrop_driven = backdrop_busy > 0.0001f
        || clear_glass_response > 0.0001f;
    profile.motion_driven = motion_response > 0.0001f;
    profile.interaction_driven = interaction_response > 0.0001f;
    profile.transition_driven = transition_response > 0.0001f;
    profile.container_driven = container_response > 0.0001f;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && reduced_motion_scale < 1.0f;
    profile.model = "adaptive-glass-stabilization";
    profile.source = material_glass_stabilization_source_name(
        profile.transition_driven,
        profile.container_driven,
        profile.interaction_driven,
        profile.backdrop_driven);
    return profile;
}

inline char const* material_glass_environment_source_name(
        bool color_driven,
        bool luminance_driven,
        bool reflection_driven,
        bool stabilization_driven,
        bool container_driven) noexcept {
    if (container_driven && reflection_driven)
        return "container-backdrop-glass-environment";
    if (stabilization_driven && reflection_driven)
        return "stabilized-backdrop-glass-environment";
    if (color_driven && luminance_driven && reflection_driven)
        return "sampled-backdrop-color-luminance-environment";
    if (color_driven && reflection_driven)
        return "sampled-backdrop-color-reflection-environment";
    if (luminance_driven && reflection_driven)
        return "sampled-backdrop-luminance-reflection-environment";
    if (reflection_driven)
        return "sampled-backdrop-reflection-environment";
    if (color_driven)
        return "sampled-backdrop-color-environment";
    return "sampled-backdrop-environment";
}

inline MaterialGlassEnvironmentProfile
material_resolve_glass_environment_profile(
        MaterialPlan const& plan) noexcept {
    MaterialGlassEnvironmentProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None
        || plan.backdrop.color_sample_count == 0u
        || plan.backdrop.luma_sample_count == 0u) {
        return profile;
    }

    auto const source = plan.backdrop.color_mean;
    auto const red = material_color_channel_fraction(source.r);
    auto const green = material_color_channel_fraction(source.g);
    auto const blue = material_color_channel_fraction(source.b);
    auto const colorfulness = material_colorfulness(source);
    auto const chroma = std::clamp(
        std::max(std::max(std::fabs(red - green), std::fabs(green - blue)),
                 std::fabs(blue - red)),
        0.0f,
        1.0f);
    auto const luma_mean = std::clamp(plan.backdrop.luma_mean, 0.0f, 1.0f);
    auto const luma_span = std::clamp(plan.backdrop.luma_span, 0.0f, 1.0f);
    auto const bright_response =
        std::clamp((luma_mean - 0.56f) / 0.44f, 0.0f, 1.0f);
    auto const dark_response =
        std::clamp((0.44f - luma_mean) / 0.44f, 0.0f, 1.0f);
    auto const detail_response =
        std::clamp((luma_span - 0.12f) / 0.52f, 0.0f, 1.0f);
    auto const backdrop_response =
        std::clamp(plan.backdrop.response_strength, 0.0f, 1.0f);
    auto const lighting_response = plan.dynamic_lighting.active
        ? std::clamp(
            plan.dynamic_lighting.highlight_strength
                + plan.dynamic_lighting.shadow_strength,
            0.0f,
            1.0f)
        : 0.0f;
    auto const stabilization_response = plan.glass_stabilization.active
        ? std::clamp(
            plan.glass_stabilization.strength * 0.42f
                + plan.glass_stabilization.damping * 0.26f
                + plan.glass_stabilization.shimmer_reduction * 0.22f
                + std::fabs(plan.glass_stabilization.transmission_bias)
                    * 0.10f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const container_response = plan.container.participates
        ? std::clamp(
            0.16f
                + (plan.container.shared_backdrop_scope ? 0.22f : 0.0f)
                + (plan.container.shape_blending_expected ? 0.18f : 0.0f)
                + (plan.container.morph_transitions ? 0.16f : 0.0f)
                + std::clamp(plan.container.blend_distance / 120.0f,
                             0.0f,
                             1.0f)
                    * 0.18f
                + std::clamp(plan.container.spacing / 72.0f, 0.0f, 1.0f)
                    * 0.10f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const clear_glass_response = plan.clear_glass_legibility.active
        ? std::clamp(
            plan.clear_glass_legibility.detail_response * 0.44f
                + plan.clear_glass_legibility.brightness_response * 0.30f
                + plan.clear_glass_legibility.contrast_lift / 0.24f * 0.26f,
            0.0f,
            1.0f)
        : 0.0f;

    profile.reflection_strength = std::clamp(
        0.08f
            + 0.30f * detail_response
            + 0.18f * lighting_response
            + 0.16f * stabilization_response
            + 0.12f * container_response
            + 0.10f * std::max(bright_response, dark_response),
        0.0f,
        1.0f);
    profile.color_pickup = std::clamp(
        0.04f
            + 0.42f * std::max(colorfulness, chroma)
            + 0.14f * detail_response
            + 0.12f * backdrop_response
            + 0.08f * lighting_response,
        0.0f,
        1.0f);
    profile.luminance_balance = std::clamp(
        0.50f
            + 0.18f * (bright_response - dark_response)
            + 0.08f * stabilization_response
            + 0.05f * detail_response
            - 0.06f * clear_glass_response,
        0.0f,
        1.0f);
    profile.transmission_balance = std::clamp(
        0.48f
            + 0.14f * stabilization_response
            + 0.10f * container_response
            + 0.06f * (1.0f - std::max(colorfulness, chroma))
            - 0.10f * detail_response
            - 0.05f * std::max(bright_response, dark_response),
        0.0f,
        1.0f);

    if (profile.reflection_strength <= 0.0001f
        && profile.color_pickup <= 0.0001f
        && std::fabs(profile.luminance_balance - 0.5f) <= 0.0001f
        && std::fabs(profile.transmission_balance - 0.5f) <= 0.0001f) {
        return MaterialGlassEnvironmentProfile{};
    }

    profile.active = true;
    profile.backdrop_driven = true;
    profile.color_driven = std::max(colorfulness, chroma) > 0.025f;
    profile.luminance_driven =
        detail_response > 0.0001f
        || bright_response > 0.0001f
        || dark_response > 0.0001f;
    profile.reflection_driven = profile.reflection_strength > 0.0001f;
    profile.stabilization_driven = stabilization_response > 0.0001f;
    profile.container_driven = container_response > 0.0001f;
    profile.model = "adaptive-glass-environment-response";
    profile.source = material_glass_environment_source_name(
        profile.color_driven,
        profile.luminance_driven,
        profile.reflection_driven,
        profile.stabilization_driven,
        profile.container_driven);
    return profile;
}

inline char const* material_glass_depth_source_name(
        bool thickness_driven,
        bool environment_driven,
        bool geometry_driven,
        bool luminance_driven,
        bool stabilization_driven) noexcept {
    if (environment_driven && luminance_driven && stabilization_driven)
        return "stabilized-environment-glass-depth";
    if (environment_driven && luminance_driven)
        return "environment-luminance-glass-depth";
    if (thickness_driven && geometry_driven)
        return "thickness-geometry-glass-depth";
    if (environment_driven)
        return "environment-glass-depth";
    if (luminance_driven)
        return "sampled-luminance-glass-depth";
    return "glass-depth";
}

inline MaterialGlassDepthResponseProfile
material_resolve_glass_depth_profile(MaterialPlan const& plan) noexcept {
    MaterialGlassDepthResponseProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None
        || !plan.shape.valid) {
        return profile;
    }

    auto const thickness_response = plan.glass_thickness.active
        ? std::clamp(
            plan.glass_thickness.thickness / 0.78f * 0.40f
                + (plan.glass_thickness.lensing_gain - 1.0f) / 0.48f
                    * 0.24f
                + (plan.glass_thickness.shadow_gain - 1.0f) / 0.44f
                    * 0.20f
                + (plan.glass_thickness.scattering_gain - 1.0f) / 0.40f
                    * 0.16f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const environment_response = plan.glass_environment.active
        ? std::clamp(
            plan.glass_environment.reflection_strength * 0.34f
                + plan.glass_environment.transmission_balance * 0.26f
                + std::fabs(plan.glass_environment.luminance_balance - 0.5f)
                    * 0.60f * 0.22f
                + plan.glass_environment.color_pickup * 0.18f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const stabilization_response = plan.glass_stabilization.active
        ? std::clamp(
            plan.glass_stabilization.strength * 0.36f
                + plan.glass_stabilization.damping * 0.28f
                + plan.glass_stabilization.shimmer_reduction * 0.24f
                + std::fabs(plan.glass_stabilization.transmission_bias)
                    * 0.12f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const luma_span = std::clamp(plan.backdrop.luma_span, 0.0f, 1.0f);
    auto const luma_response = plan.backdrop.luma_sample_count > 0u
        ? std::clamp(
            (luma_span - 0.08f) / 0.54f * 0.72f
                + std::fabs(plan.backdrop.luma_mean - 0.5f) * 0.56f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const size_response = std::clamp(
        (std::sqrt(std::max(plan.shape.surface_area, 0.0f)) - 96.0f)
            / 420.0f,
        0.0f,
        1.0f);
    auto const roundness_response =
        std::clamp(plan.shape.normalized_radius, 0.0f, 1.0f);
    auto const geometry_response = std::clamp(
        roundness_response * 0.55f + size_response * 0.45f,
        0.0f,
        1.0f);
    auto const response = std::clamp(
        0.14f
            + 0.22f * thickness_response
            + 0.20f * environment_response
            + 0.18f * luma_response
            + 0.14f * geometry_response
            + 0.12f * stabilization_response,
        0.0f,
        1.0f);

    profile.depth_separation = std::clamp(
        0.10f
            + 0.46f * response
            + 0.12f * thickness_response
            + 0.08f * environment_response,
        0.0f,
        1.0f);
    profile.inner_shadow = std::clamp(
        0.030f
            + 0.115f * response
            + 0.060f * luma_response
            + 0.040f * stabilization_response,
        0.0f,
        0.36f);
    profile.surface_lift = std::clamp(
        0.035f
            + 0.160f * response
            + 0.070f * environment_response
            + 0.050f * geometry_response,
        0.0f,
        0.42f);
    profile.parallax_gain = std::clamp(
        0.045f
            + 0.170f * thickness_response
            + 0.115f * environment_response
            + 0.080f * geometry_response
            + 0.050f * stabilization_response,
        0.0f,
        0.46f);
    if (profile.depth_separation <= 0.0001f
        && profile.inner_shadow <= 0.0001f
        && profile.surface_lift <= 0.0001f
        && profile.parallax_gain <= 0.0001f) {
        return MaterialGlassDepthResponseProfile{};
    }

    profile.active = true;
    profile.thickness_driven = thickness_response > 0.0001f;
    profile.environment_driven = environment_response > 0.0001f;
    profile.geometry_driven = geometry_response > 0.0001f;
    profile.luminance_driven = luma_response > 0.0001f;
    profile.stabilization_driven = stabilization_response > 0.0001f;
    profile.model = "adaptive-glass-depth-response";
    profile.source = material_glass_depth_source_name(
        profile.thickness_driven,
        profile.environment_driven,
        profile.geometry_driven,
        profile.luminance_driven,
        profile.stabilization_driven);
    return profile;
}

inline char const* material_glass_caustic_flow_source_name(
        bool lighting_driven,
        bool dispersion_driven,
        bool depth_driven,
        bool interaction_driven) noexcept {
    if (interaction_driven && depth_driven)
        return "interactive-depth-caustic-flow";
    if (depth_driven && dispersion_driven && lighting_driven)
        return "depth-prismatic-caustic-flow";
    if (dispersion_driven && lighting_driven)
        return "prismatic-light-caustic-flow";
    if (depth_driven)
        return "depth-caustic-flow";
    if (dispersion_driven)
        return "prismatic-caustic-flow";
    return "sampled-backdrop-caustic-flow";
}

inline MaterialGlassCausticFlowProfile
material_resolve_glass_caustic_flow_profile(MaterialPlan const& plan) noexcept {
    MaterialGlassCausticFlowProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None) {
        return profile;
    }

    auto const source = plan.backdrop.color_mean;
    auto const colorfulness = material_colorfulness(source);
    auto const luma_span = std::clamp(plan.backdrop.luma_span, 0.0f, 1.0f);
    auto const backdrop_response = std::clamp(
        plan.backdrop.response_strength * 0.34f
            + luma_span * 0.28f
            + colorfulness * 0.20f
            + (plan.glass_environment.active
                   ? plan.glass_environment.reflection_strength * 0.18f
                   : 0.0f),
        0.0f,
        1.0f);
    auto const lighting_response = plan.dynamic_lighting.active
        ? std::clamp(
            plan.dynamic_lighting.highlight_strength / 0.45f * 0.48f
                + plan.dynamic_lighting.shadow_strength / 0.36f * 0.30f
                + std::fabs(plan.dynamic_lighting.direction_x) * 0.12f
                + std::fabs(plan.dynamic_lighting.direction_y) * 0.10f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const dispersion_response = plan.glass_dispersion.active
        ? std::clamp(
            plan.glass_dispersion.axial_offset_pixels / 3.20f * 0.24f
                + plan.glass_dispersion.tangential_offset_pixels / 2.45f
                    * 0.22f
                + (plan.glass_dispersion.prismatic_gain - 1.0f) / 0.75f
                    * 0.24f
                + plan.glass_dispersion.caustic_spread / 0.40f * 0.30f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const depth_response = plan.glass_depth.active
        ? std::clamp(
            plan.glass_depth.depth_separation * 0.32f
                + plan.glass_depth.surface_lift / 0.42f * 0.24f
                + plan.glass_depth.parallax_gain / 0.46f * 0.24f
                + plan.glass_depth.inner_shadow / 0.36f * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const interaction_response = plan.interaction.active
        ? std::clamp(
            plan.interaction.response_strength * 0.56f
                + plan.interaction.pointer_lens_strength * 0.24f
                + plan.interaction.control_morph_depth * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const reduced_motion_scale =
        plan.decision_trace.reduce_motion ? 0.58f : 1.0f;
    auto const response = std::clamp(
        0.12f
            + 0.24f * lighting_response
            + 0.22f * dispersion_response
            + 0.20f * depth_response
            + 0.16f * backdrop_response
            + 0.06f * interaction_response,
        0.0f,
        1.0f);

    profile.flow_strength = std::clamp(
        reduced_motion_scale
            * (0.10f
               + 0.46f * response
               + 0.14f * dispersion_response
               + 0.10f * depth_response),
        0.0f,
        1.0f);
    profile.chroma_shear = std::clamp(
        reduced_motion_scale
            * (0.024f
               + 0.105f * dispersion_response
               + 0.060f * colorfulness
               + 0.050f * backdrop_response),
        0.0f,
        0.32f);
    profile.highlight_drift = std::clamp(
        reduced_motion_scale
            * (0.034f
               + 0.125f * lighting_response
               + 0.075f * depth_response
               + 0.050f * interaction_response),
        0.0f,
        0.36f);
    profile.caustic_focus = std::clamp(
        0.044f
            + 0.145f * dispersion_response
            + 0.110f * lighting_response
            + 0.085f * depth_response
            + 0.055f * backdrop_response,
        0.0f,
        0.40f);
    if (profile.flow_strength <= 0.0001f
        && profile.chroma_shear <= 0.0001f
        && profile.highlight_drift <= 0.0001f
        && profile.caustic_focus <= 0.0001f) {
        return MaterialGlassCausticFlowProfile{};
    }

    profile.active = true;
    profile.backdrop_driven = backdrop_response > 0.0001f;
    profile.lighting_driven = lighting_response > 0.0001f;
    profile.dispersion_driven = dispersion_response > 0.0001f;
    profile.depth_driven = depth_response > 0.0001f;
    profile.interaction_driven = interaction_response > 0.0001f;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && reduced_motion_scale < 1.0f;
    profile.model = "adaptive-glass-caustic-flow";
    profile.source = material_glass_caustic_flow_source_name(
        profile.lighting_driven,
        profile.dispersion_driven,
        profile.depth_driven,
        profile.interaction_driven);
    return profile;
}

inline char const* material_glass_meniscus_source_name(
        bool depth_driven,
        bool caustic_driven,
        bool environment_driven,
        bool interaction_driven) noexcept {
    if (interaction_driven && caustic_driven)
        return "interactive-caustic-meniscus-rim";
    if (depth_driven && caustic_driven && environment_driven)
        return "depth-caustic-environment-meniscus-rim";
    if (depth_driven && caustic_driven)
        return "depth-caustic-meniscus-rim";
    if (caustic_driven && environment_driven)
        return "caustic-environment-meniscus-rim";
    if (depth_driven)
        return "depth-meniscus-rim";
    return "sampled-backdrop-meniscus-rim";
}

inline MaterialGlassMeniscusProfile
material_resolve_glass_meniscus_profile(MaterialPlan const& plan) noexcept {
    MaterialGlassMeniscusProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None) {
        return profile;
    }

    auto const shape_roundness =
        std::clamp(plan.shape.normalized_radius, 0.0f, 1.0f);
    auto const edge_response = std::clamp(
        plan.edge_highlight * 0.45f
            + shape_roundness * 0.25f
            + plan.edge_width / 12.0f * 0.30f,
        0.0f,
        1.0f);
    auto const depth_response = plan.glass_depth.active
        ? std::clamp(
            plan.glass_depth.depth_separation * 0.34f
                + plan.glass_depth.surface_lift / 0.42f * 0.28f
                + plan.glass_depth.parallax_gain / 0.46f * 0.22f
                + plan.glass_depth.inner_shadow / 0.36f * 0.16f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const caustic_response = plan.glass_caustic_flow.active
        ? std::clamp(
            plan.glass_caustic_flow.flow_strength * 0.32f
                + plan.glass_caustic_flow.caustic_focus / 0.40f * 0.30f
                + plan.glass_caustic_flow.chroma_shear / 0.32f * 0.20f
                + plan.glass_caustic_flow.highlight_drift / 0.36f * 0.18f,
            0.0f,
            1.0f)
        : std::clamp(plan.refraction.edge_caustic_intensity / 0.20f, 0.0f, 1.0f);
    auto const environment_response = plan.glass_environment.active
        ? std::clamp(
            plan.glass_environment.reflection_strength * 0.40f
                + plan.glass_environment.color_pickup * 0.22f
                + plan.glass_environment.transmission_balance * 0.22f
                + std::fabs(plan.glass_environment.luminance_balance - 0.5f)
                    * 0.16f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const thickness_response = plan.glass_thickness.active
        ? std::clamp(
            plan.glass_thickness.thickness / 0.78f * 0.48f
                + (plan.glass_thickness.lensing_gain - 1.0f) / 0.48f * 0.28f
                + (plan.glass_thickness.scattering_gain - 1.0f) / 0.40f
                    * 0.24f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const interaction_response = plan.interaction.active
        ? std::clamp(
            plan.interaction.response_strength * 0.50f
                + plan.interaction.pointer_lens_strength * 0.28f
                + plan.interaction.control_morph_edge * 0.22f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const reduced_motion_scale =
        plan.decision_trace.reduce_motion ? 0.70f : 1.0f;
    auto const response = std::clamp(
        0.10f
            + edge_response * 0.24f
            + depth_response * 0.22f
            + caustic_response * 0.22f
            + environment_response * 0.14f
            + thickness_response * 0.12f
            + interaction_response * 0.06f,
        0.0f,
        1.0f);

    profile.rim_width = std::clamp(
        0.45f
            + 3.20f * response
            + 1.45f * depth_response
            + 0.85f * thickness_response,
        0.0f,
        6.0f);
    profile.edge_pull = std::clamp(
        reduced_motion_scale
            * (0.020f
               + 0.110f * depth_response
               + 0.095f * caustic_response
               + 0.055f * environment_response
               + 0.040f * interaction_response),
        0.0f,
        0.32f);
    profile.highlight_gain = std::clamp(
        1.0f
            + 0.20f * edge_response
            + 0.18f * caustic_response
            + 0.14f * environment_response
            + 0.10f * depth_response,
        1.0f,
        1.60f);
    profile.refraction_gain = std::clamp(
        1.0f
            + reduced_motion_scale
                * (0.110f * depth_response
                   + 0.095f * caustic_response
                   + 0.055f * thickness_response
                   + 0.040f * interaction_response),
        1.0f,
        1.38f);
    if (profile.rim_width <= 0.0001f
        && profile.edge_pull <= 0.0001f
        && std::fabs(profile.highlight_gain - 1.0f) <= 0.0001f
        && std::fabs(profile.refraction_gain - 1.0f) <= 0.0001f) {
        return MaterialGlassMeniscusProfile{};
    }

    profile.active = true;
    profile.edge_driven = edge_response > 0.0001f;
    profile.depth_driven = depth_response > 0.0001f;
    profile.caustic_driven = caustic_response > 0.0001f;
    profile.environment_driven = environment_response > 0.0001f;
    profile.interaction_driven = interaction_response > 0.0001f;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && reduced_motion_scale < 1.0f;
    profile.model = "adaptive-glass-meniscus-rim";
    profile.source = material_glass_meniscus_source_name(
        profile.depth_driven,
        profile.caustic_driven,
        profile.environment_driven,
        profile.interaction_driven);
    return profile;
}

inline char const* material_glass_transmission_source_name(
        bool thickness_driven,
        bool depth_driven,
        bool environment_driven,
        bool caustic_driven,
        bool interaction_driven) noexcept {
    if (interaction_driven && depth_driven)
        return "interactive-depth-volume-transmission";
    if (depth_driven && caustic_driven && environment_driven)
        return "depth-caustic-environment-volume-transmission";
    if (thickness_driven && depth_driven && environment_driven)
        return "thickness-depth-environment-volume-transmission";
    if (depth_driven && environment_driven)
        return "depth-environment-volume-transmission";
    if (thickness_driven && environment_driven)
        return "thickness-environment-volume-transmission";
    return "sampled-backdrop-volume-transmission";
}

inline MaterialGlassTransmissionProfile
material_resolve_glass_transmission_profile(MaterialPlan const& plan) noexcept {
    MaterialGlassTransmissionProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None) {
        return profile;
    }

    auto const thickness_response = plan.glass_thickness.active
        ? std::clamp(
            plan.glass_thickness.thickness / 0.78f * 0.34f
                + (plan.glass_thickness.scattering_gain - 1.0f) / 0.40f
                    * 0.26f
                + (plan.glass_thickness.lensing_gain - 1.0f) / 0.48f
                    * 0.20f
                + (plan.glass_thickness.shadow_gain - 1.0f) / 0.38f
                    * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const depth_response = plan.glass_depth.active
        ? std::clamp(
            plan.glass_depth.depth_separation * 0.30f
                + plan.glass_depth.inner_shadow / 0.36f * 0.26f
                + plan.glass_depth.surface_lift / 0.42f * 0.24f
                + plan.glass_depth.parallax_gain / 0.46f * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const environment_response = plan.glass_environment.active
        ? std::clamp(
            plan.glass_environment.transmission_balance * 0.36f
                + plan.glass_environment.reflection_strength * 0.22f
                + plan.glass_environment.color_pickup * 0.18f
                + std::fabs(plan.glass_environment.luminance_balance - 0.5f)
                    * 0.24f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const caustic_response = plan.glass_caustic_flow.active
        ? std::clamp(
            plan.glass_caustic_flow.flow_strength * 0.30f
                + plan.glass_caustic_flow.caustic_focus / 0.40f * 0.26f
                + plan.glass_caustic_flow.chroma_shear / 0.32f * 0.22f
                + plan.glass_caustic_flow.highlight_drift / 0.36f * 0.22f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const stabilization_response = plan.glass_stabilization.active
        ? std::clamp(
            plan.glass_stabilization.strength * 0.36f
                + plan.glass_stabilization.damping * 0.20f
                + plan.glass_stabilization.shimmer_reduction * 0.18f
                + -plan.glass_stabilization.transmission_bias / 0.20f
                    * 0.26f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const interaction_response = plan.interaction.active
        ? std::clamp(
            plan.interaction.response_strength * 0.48f
                + plan.interaction.pointer_lens_strength * 0.26f
                + plan.interaction.control_morph_depth * 0.26f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const reduced_motion_scale =
        plan.decision_trace.reduce_motion ? 0.72f : 1.0f;
    auto const response = std::clamp(
        0.08f
            + 0.22f * thickness_response
            + 0.22f * depth_response
            + 0.20f * environment_response
            + 0.16f * caustic_response
            + 0.12f * stabilization_response
            + 0.08f * interaction_response,
        0.0f,
        1.0f);

    profile.internal_transmission = std::clamp(
        0.045f
            + 0.34f * response
            + 0.070f * environment_response
            + 0.050f * thickness_response,
        0.0f,
        0.52f);
    profile.subsurface_scatter = std::clamp(
        0.020f
            + 0.150f * thickness_response
            + 0.090f * caustic_response
            + 0.070f * environment_response
            + 0.050f * depth_response,
        0.0f,
        0.34f);
    profile.volume_absorption = std::clamp(
        0.016f
            + 0.120f * depth_response
            + 0.095f * environment_response
            + 0.070f * thickness_response
            + 0.055f * stabilization_response,
        0.0f,
        0.30f);
    profile.interlayer_refraction = std::clamp(
        reduced_motion_scale
            * (0.020f
               + 0.135f * depth_response
               + 0.110f * caustic_response
               + 0.075f * thickness_response
               + 0.055f * interaction_response),
        0.0f,
        0.32f);
    if (profile.internal_transmission <= 0.0001f
        && profile.subsurface_scatter <= 0.0001f
        && profile.volume_absorption <= 0.0001f
        && profile.interlayer_refraction <= 0.0001f) {
        return MaterialGlassTransmissionProfile{};
    }

    profile.active = true;
    profile.thickness_driven = thickness_response > 0.0001f;
    profile.depth_driven = depth_response > 0.0001f;
    profile.environment_driven = environment_response > 0.0001f;
    profile.caustic_driven = caustic_response > 0.0001f;
    profile.stabilization_driven = stabilization_response > 0.0001f;
    profile.interaction_driven = interaction_response > 0.0001f;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && reduced_motion_scale < 1.0f;
    profile.model = "adaptive-glass-volume-transmission";
    profile.source = material_glass_transmission_source_name(
        profile.thickness_driven,
        profile.depth_driven,
        profile.environment_driven,
        profile.caustic_driven,
        profile.interaction_driven);
    return profile;
}

inline char const* material_glass_surface_cohesion_source_name(
        bool transmission_driven,
        bool meniscus_driven,
        bool caustic_driven,
        bool interaction_driven) noexcept {
    if (interaction_driven && meniscus_driven)
        return "interactive-meniscus-glass-cohesion";
    if (transmission_driven && meniscus_driven && caustic_driven)
        return "transmission-meniscus-caustic-glass-cohesion";
    if (transmission_driven && meniscus_driven)
        return "transmission-meniscus-glass-cohesion";
    if (meniscus_driven && caustic_driven)
        return "meniscus-caustic-glass-cohesion";
    if (transmission_driven)
        return "transmission-glass-cohesion";
    return "sampled-backdrop-glass-cohesion";
}

inline MaterialGlassSurfaceCohesionProfile
material_resolve_glass_surface_cohesion_profile(
        MaterialPlan const& plan) noexcept {
    MaterialGlassSurfaceCohesionProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None) {
        return profile;
    }

    auto const thickness_response = plan.glass_thickness.active
        ? std::clamp(
            plan.glass_thickness.thickness / 0.78f * 0.34f
                + (plan.glass_thickness.lensing_gain - 1.0f) / 0.48f
                    * 0.28f
                + (plan.glass_thickness.scattering_gain - 1.0f) / 0.40f
                    * 0.22f
                + (plan.glass_thickness.shadow_gain - 1.0f) / 0.38f
                    * 0.16f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const transmission_response = plan.glass_transmission.active
        ? std::clamp(
            plan.glass_transmission.internal_transmission / 0.52f * 0.34f
                + plan.glass_transmission.subsurface_scatter / 0.34f
                    * 0.24f
                + plan.glass_transmission.interlayer_refraction / 0.32f
                    * 0.22f
                + plan.glass_transmission.volume_absorption / 0.30f
                    * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const meniscus_response = plan.glass_meniscus.active
        ? std::clamp(
            plan.glass_meniscus.rim_width / 6.0f * 0.30f
                + plan.glass_meniscus.edge_pull / 0.32f * 0.24f
                + (plan.glass_meniscus.highlight_gain - 1.0f) / 0.60f
                    * 0.22f
                + (plan.glass_meniscus.refraction_gain - 1.0f) / 0.38f
                    * 0.24f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const caustic_response = plan.glass_caustic_flow.active
        ? std::clamp(
            plan.glass_caustic_flow.flow_strength * 0.30f
                + plan.glass_caustic_flow.caustic_focus / 0.40f * 0.28f
                + plan.glass_caustic_flow.chroma_shear / 0.32f * 0.22f
                + plan.glass_caustic_flow.highlight_drift / 0.36f * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const interaction_response = plan.interaction.active
        ? std::clamp(
            plan.interaction.response_strength * 0.42f
                + plan.interaction.pointer_lens_strength * 0.30f
                + plan.interaction.control_morph_depth * 0.18f
                + plan.interaction.control_morph_edge * 0.10f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const reduced_motion_scale =
        plan.decision_trace.reduce_motion ? 0.70f : 1.0f;
    auto const response = std::clamp(
        0.07f
            + 0.22f * thickness_response
            + 0.24f * transmission_response
            + 0.22f * meniscus_response
            + 0.20f * caustic_response
            + 0.12f * interaction_response,
        0.0f,
        1.0f);

    profile.surface_response = std::clamp(
        reduced_motion_scale
            * (0.08f
               + 0.42f * response
               + 0.10f * transmission_response
               + 0.08f * caustic_response),
        0.0f,
        0.64f);
    profile.edge_adhesion = std::clamp(
        reduced_motion_scale
            * (0.06f
               + 0.30f * meniscus_response
               + 0.18f * transmission_response
               + 0.12f * thickness_response),
        0.0f,
        0.56f);
    profile.shape_coalescence = std::clamp(
        reduced_motion_scale
            * (0.05f
               + 0.28f * caustic_response
               + 0.22f * meniscus_response
               + 0.16f * transmission_response),
        0.0f,
        0.54f);
    profile.luma_stability = std::clamp(
        0.12f
            + 0.24f * transmission_response
            + 0.20f * thickness_response
            + 0.16f * meniscus_response,
        0.0f,
        0.62f);
    if (profile.surface_response <= 0.0001f
        && profile.edge_adhesion <= 0.0001f
        && profile.shape_coalescence <= 0.0001f
        && profile.luma_stability <= 0.0001f) {
        return MaterialGlassSurfaceCohesionProfile{};
    }

    profile.active = true;
    profile.thickness_driven = thickness_response > 0.0001f;
    profile.transmission_driven = transmission_response > 0.0001f;
    profile.meniscus_driven = meniscus_response > 0.0001f;
    profile.caustic_driven = caustic_response > 0.0001f;
    profile.interaction_driven = interaction_response > 0.0001f;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && reduced_motion_scale < 1.0f;
    profile.model = "adaptive-glass-surface-cohesion";
    profile.source = material_glass_surface_cohesion_source_name(
        profile.transmission_driven,
        profile.meniscus_driven,
        profile.caustic_driven,
        profile.interaction_driven);
    return profile;
}

inline char const* material_glass_substrate_adhesion_source_name(
        bool surface_driven,
        bool transmission_driven,
        bool depth_driven,
        bool environment_driven,
        bool interaction_driven) noexcept {
    if (interaction_driven && surface_driven)
        return "interactive-surface-substrate-adhesion";
    if (surface_driven && transmission_driven && depth_driven)
        return "surface-transmission-depth-substrate-adhesion";
    if (surface_driven && environment_driven && depth_driven)
        return "surface-environment-depth-substrate-adhesion";
    if (surface_driven && transmission_driven)
        return "surface-transmission-substrate-adhesion";
    if (surface_driven)
        return "surface-substrate-adhesion";
    return "sampled-backdrop-substrate-adhesion";
}

inline MaterialGlassSubstrateAdhesionProfile
material_resolve_glass_substrate_adhesion_profile(
        MaterialPlan const& plan) noexcept {
    MaterialGlassSubstrateAdhesionProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None) {
        return profile;
    }

    auto const surface_response = plan.glass_surface_cohesion.active
        ? std::clamp(
            plan.glass_surface_cohesion.surface_response / 0.64f * 0.30f
                + plan.glass_surface_cohesion.edge_adhesion / 0.56f
                    * 0.28f
                + plan.glass_surface_cohesion.shape_coalescence / 0.54f
                    * 0.22f
                + plan.glass_surface_cohesion.luma_stability / 0.62f
                    * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const transmission_response = plan.glass_transmission.active
        ? std::clamp(
            plan.glass_transmission.internal_transmission / 0.52f * 0.30f
                + plan.glass_transmission.subsurface_scatter / 0.34f
                    * 0.26f
                + plan.glass_transmission.volume_absorption / 0.30f
                    * 0.24f
                + plan.glass_transmission.interlayer_refraction / 0.32f
                    * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const depth_response = plan.glass_depth.active
        ? std::clamp(
            plan.glass_depth.depth_separation * 0.30f
                + plan.glass_depth.inner_shadow / 0.36f * 0.28f
                + plan.glass_depth.surface_lift / 0.42f * 0.22f
                + plan.glass_depth.parallax_gain / 0.46f * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const environment_response = plan.glass_environment.active
        ? std::clamp(
            plan.glass_environment.reflection_strength * 0.28f
                + plan.glass_environment.transmission_balance * 0.26f
                + plan.glass_environment.color_pickup * 0.18f
                + std::fabs(plan.glass_environment.luminance_balance - 0.5f)
                    * 0.28f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const interaction_response = plan.interaction.active
        ? std::clamp(
            plan.interaction.response_strength * 0.44f
                + plan.interaction.pointer_lens_strength * 0.30f
                + plan.interaction.control_morph_shadow * 0.16f
                + plan.interaction.control_morph_depth * 0.10f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const reduced_motion_scale =
        plan.decision_trace.reduce_motion ? 0.74f : 1.0f;
    auto const response = std::clamp(
        0.08f
            + 0.26f * surface_response
            + 0.22f * transmission_response
            + 0.20f * depth_response
            + 0.18f * environment_response
            + 0.10f * interaction_response,
        0.0f,
        1.0f);

    profile.contact_strength = std::clamp(
        reduced_motion_scale
            * (0.045f
               + 0.30f * response
               + 0.10f * surface_response
               + 0.06f * environment_response),
        0.0f,
        0.46f);
    profile.settle_depth = std::clamp(
        0.030f
            + 0.18f * depth_response
            + 0.16f * surface_response
            + 0.10f * transmission_response,
        0.0f,
        0.34f);
    profile.contact_shadow = std::clamp(
        0.020f
            + 0.16f * depth_response
            + 0.13f * transmission_response
            + 0.10f * environment_response,
        0.0f,
        0.30f);
    profile.refraction_crawl = std::clamp(
        reduced_motion_scale
            * (0.018f
               + 0.14f * surface_response
               + 0.12f * transmission_response
               + 0.08f * interaction_response),
        0.0f,
        0.28f);
    if (profile.contact_strength <= 0.0001f
        && profile.settle_depth <= 0.0001f
        && profile.contact_shadow <= 0.0001f
        && profile.refraction_crawl <= 0.0001f) {
        return MaterialGlassSubstrateAdhesionProfile{};
    }

    profile.active = true;
    profile.surface_driven = surface_response > 0.0001f;
    profile.transmission_driven = transmission_response > 0.0001f;
    profile.depth_driven = depth_response > 0.0001f;
    profile.environment_driven = environment_response > 0.0001f;
    profile.interaction_driven = interaction_response > 0.0001f;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && reduced_motion_scale < 1.0f;
    profile.model = "adaptive-glass-substrate-adhesion";
    profile.source = material_glass_substrate_adhesion_source_name(
        profile.surface_driven,
        profile.transmission_driven,
        profile.depth_driven,
        profile.environment_driven,
        profile.interaction_driven);
    return profile;
}

inline char const* material_glass_ambient_reflection_source_name(
        bool environment_driven,
        bool transmission_driven,
        bool substrate_driven,
        bool surface_driven,
        bool lighting_driven) noexcept {
    if (environment_driven && transmission_driven && substrate_driven)
        return "environment-transmission-substrate-ambient-reflection";
    if (environment_driven && surface_driven && lighting_driven)
        return "environment-surface-light-ambient-reflection";
    if (environment_driven && transmission_driven)
        return "environment-transmission-ambient-reflection";
    if (environment_driven && substrate_driven)
        return "environment-substrate-ambient-reflection";
    if (environment_driven)
        return "environment-ambient-reflection";
    return "sampled-backdrop-ambient-reflection";
}

inline MaterialGlassAmbientReflectionProfile
material_resolve_glass_ambient_reflection_profile(
        MaterialPlan const& plan) noexcept {
    MaterialGlassAmbientReflectionProfile profile{};
    profile.bounded = true;
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None) {
        return profile;
    }

    auto const environment_response = plan.glass_environment.active
        ? std::clamp(
            plan.glass_environment.reflection_strength * 0.34f
                + plan.glass_environment.color_pickup * 0.24f
                + plan.glass_environment.transmission_balance * 0.22f
                + std::fabs(plan.glass_environment.luminance_balance - 0.5f)
                    * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const transmission_response = plan.glass_transmission.active
        ? std::clamp(
            plan.glass_transmission.internal_transmission / 0.52f * 0.30f
                + plan.glass_transmission.interlayer_refraction / 0.32f
                    * 0.26f
                + plan.glass_transmission.subsurface_scatter / 0.34f
                    * 0.24f
                + plan.glass_transmission.volume_absorption / 0.30f
                    * 0.20f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const substrate_response = plan.glass_substrate_adhesion.active
        ? std::clamp(
            plan.glass_substrate_adhesion.contact_strength / 0.46f * 0.32f
                + plan.glass_substrate_adhesion.settle_depth / 0.34f
                    * 0.24f
                + plan.glass_substrate_adhesion.contact_shadow / 0.30f
                    * 0.22f
                + plan.glass_substrate_adhesion.refraction_crawl / 0.28f
                    * 0.22f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const surface_response = plan.glass_surface_cohesion.active
        ? std::clamp(
            plan.glass_surface_cohesion.surface_response / 0.64f * 0.30f
                + plan.glass_surface_cohesion.edge_adhesion / 0.56f
                    * 0.24f
                + plan.glass_surface_cohesion.shape_coalescence / 0.54f
                    * 0.22f
                + plan.glass_surface_cohesion.luma_stability / 0.62f
                    * 0.24f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const lighting_response = plan.dynamic_lighting.active
        ? std::clamp(
            plan.dynamic_lighting.highlight_strength / 0.45f * 0.46f
                + plan.dynamic_lighting.shadow_strength / 0.36f * 0.26f
                + std::fabs(plan.dynamic_lighting.direction_x) * 0.14f
                + std::fabs(plan.dynamic_lighting.direction_y) * 0.14f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const interaction_response = plan.interaction.active
        ? std::clamp(
            plan.interaction.response_strength * 0.44f
                + plan.interaction.pointer_lens_strength * 0.32f
                + plan.interaction.control_morph_edge * 0.14f
                + plan.interaction.control_morph_depth * 0.10f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const reduced_motion_scale =
        plan.decision_trace.reduce_motion ? 0.80f : 1.0f;
    auto const response = std::clamp(
        0.07f
            + 0.30f * environment_response
            + 0.22f * transmission_response
            + 0.18f * substrate_response
            + 0.14f * surface_response
            + 0.10f * lighting_response
            + 0.06f * interaction_response,
        0.0f,
        1.0f);

    profile.reflection_gain = std::clamp(
        reduced_motion_scale
            * (0.045f
               + 0.24f * response
               + 0.10f * environment_response
               + 0.06f * substrate_response),
        0.0f,
        0.42f);
    profile.color_bleed = std::clamp(
        0.030f
            + 0.20f * environment_response
            + 0.12f * transmission_response
            + 0.08f * surface_response,
        0.0f,
        0.34f);
    profile.luma_polarization = std::clamp(
        0.024f
            + 0.16f * environment_response
            + 0.12f * substrate_response
            + 0.08f * lighting_response,
        0.0f,
        0.30f);
    profile.sheen_coherence = std::clamp(
        reduced_motion_scale
            * (0.040f
               + 0.18f * surface_response
               + 0.16f * substrate_response
               + 0.14f * transmission_response
               + 0.08f * lighting_response),
        0.0f,
        0.36f);
    if (profile.reflection_gain <= 0.0001f
        && profile.color_bleed <= 0.0001f
        && profile.luma_polarization <= 0.0001f
        && profile.sheen_coherence <= 0.0001f) {
        return MaterialGlassAmbientReflectionProfile{};
    }

    profile.active = true;
    profile.environment_driven = environment_response > 0.0001f;
    profile.transmission_driven = transmission_response > 0.0001f;
    profile.substrate_driven = substrate_response > 0.0001f;
    profile.surface_driven = surface_response > 0.0001f;
    profile.lighting_driven = lighting_response > 0.0001f;
    profile.interaction_driven = interaction_response > 0.0001f;
    profile.reduced_motion_suppressed =
        plan.decision_trace.reduce_motion && reduced_motion_scale < 1.0f;
    profile.model = "adaptive-glass-ambient-reflection";
    profile.source = material_glass_ambient_reflection_source_name(
        profile.environment_driven,
        profile.transmission_driven,
        profile.substrate_driven,
        profile.surface_driven,
        profile.lighting_driven);
    return profile;
}

inline float material_base_specular_intensity(MaterialKind kind) noexcept {
    switch (kind) {
        case MaterialKind::Clear: return 0.050f;
        case MaterialKind::Thin: return 0.070f;
        case MaterialKind::Regular: return 0.090f;
        case MaterialKind::Thick: return 0.115f;
        case MaterialKind::None: return 0.0f;
    }
    return 0.0f;
}

inline char const* material_glass_specular_source_name(
        bool caustic_flow_driven,
        bool depth_driven,
        bool environment_driven,
        bool lighting_driven) noexcept {
    if (caustic_flow_driven && depth_driven && environment_driven)
        return "caustic-depth-environment-glass-glint";
    if (caustic_flow_driven && depth_driven)
        return "caustic-depth-glass-glint";
    if (caustic_flow_driven && environment_driven)
        return "caustic-environment-glass-glint";
    if (caustic_flow_driven)
        return "caustic-flow-glass-glint";
    if (depth_driven && lighting_driven)
        return "depth-light-glass-glint";
    return "sampled-backdrop-edge-lighting";
}

inline MaterialSpecularProfile material_resolve_specular_profile(
        MaterialPlan const& plan) noexcept {
    MaterialSpecularProfile profile{};
    if (!plan.backdrop_sampling
        || plan.fallback()
        || plan.kind == MaterialKind::None) {
        return profile;
    }

    auto const shape_roundness =
        std::clamp(plan.shape.normalized_radius, 0.0f, 1.0f);
    auto const backdrop_response =
        std::clamp(plan.backdrop.response_strength, 0.0f, 1.0f);
    auto const caustic_response =
        std::clamp(plan.refraction.edge_caustic_intensity, 0.0f, 0.20f);
    auto const caustic_flow_response = plan.glass_caustic_flow.active
        ? std::clamp(
            plan.glass_caustic_flow.flow_strength * 0.36f
                + plan.glass_caustic_flow.caustic_focus / 0.40f * 0.28f
                + plan.glass_caustic_flow.highlight_drift / 0.36f * 0.22f
                + plan.glass_caustic_flow.chroma_shear / 0.32f * 0.14f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const depth_response = plan.glass_depth.active
        ? std::clamp(
            plan.glass_depth.depth_separation * 0.34f
                + plan.glass_depth.surface_lift / 0.42f * 0.28f
                + plan.glass_depth.parallax_gain / 0.46f * 0.22f
                + plan.glass_depth.inner_shadow / 0.36f * 0.16f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const environment_response = plan.glass_environment.active
        ? std::clamp(
            plan.glass_environment.reflection_strength * 0.40f
                + plan.glass_environment.color_pickup * 0.22f
                + plan.glass_environment.transmission_balance * 0.22f
                + std::fabs(plan.glass_environment.luminance_balance - 0.5f)
                    * 0.16f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const ambient_reflection_response =
        plan.glass_ambient_reflection.active
        ? std::clamp(
            plan.glass_ambient_reflection.reflection_gain / 0.42f * 0.32f
                + plan.glass_ambient_reflection.color_bleed / 0.34f
                    * 0.22f
                + plan.glass_ambient_reflection.luma_polarization / 0.30f
                    * 0.22f
                + plan.glass_ambient_reflection.sheen_coherence / 0.36f
                    * 0.24f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const lighting_response = plan.dynamic_lighting.active
        ? std::clamp(
            plan.dynamic_lighting.highlight_strength / 0.45f * 0.46f
                + plan.dynamic_lighting.shadow_strength / 0.36f * 0.28f
                + std::fabs(plan.dynamic_lighting.direction_x) * 0.14f
                + std::fabs(plan.dynamic_lighting.direction_y) * 0.12f,
            0.0f,
            1.0f)
        : 0.0f;
    auto const glint_response = std::clamp(
        caustic_flow_response * 0.40f
            + depth_response * 0.24f
            + environment_response * 0.16f
            + ambient_reflection_response * 0.12f
            + lighting_response * 0.08f,
        0.0f,
        1.0f);
    auto const ambient_intensity = std::clamp(
        (material_base_specular_intensity(plan.kind)
             + 0.030f * shape_roundness
             + 0.045f * std::clamp(plan.edge_highlight, 0.0f, 1.0f)
             + 0.020f * backdrop_response
             + 0.50f * caustic_response
             + 0.060f * glint_response
             + 0.030f * ambient_reflection_response
             + 0.025f * caustic_flow_response)
            * (plan.transition.active ? plan.transition.optical_gain : 1.0f),
        0.0f,
        0.32f);
    if (ambient_intensity <= 0.0001f)
        return profile;

    profile.active = true;
    profile.ambient = true;
    bool const caustic_flow_driven = caustic_flow_response > 0.0001f;
    bool const depth_driven = depth_response > 0.0001f;
    bool const environment_driven = environment_response > 0.0001f;
    bool const lighting_driven = lighting_response > 0.0001f;
    profile.model = glint_response > 0.0001f
        ? "adaptive-liquid-glass-glint"
        : "ambient-glass-sheen";
    profile.source = material_glass_specular_source_name(
        caustic_flow_driven,
        depth_driven,
        environment_driven,
        lighting_driven);
    profile.anchor_x = std::clamp(
        0.30f
            + 0.06f * shape_roundness
            + plan.dynamic_lighting.direction_x * 0.045f
            + plan.glass_caustic_flow.chroma_shear * 0.16f,
        0.0f,
        1.0f);
    profile.anchor_y = std::clamp(
        0.18f
            + 0.04f * backdrop_response
            + plan.dynamic_lighting.direction_y * 0.035f
            - plan.glass_depth.surface_lift * 0.050f,
        0.0f,
        1.0f);
    profile.radius = std::clamp(
        0.58f
            + 0.18f * shape_roundness
            + 0.045f * depth_response
            + 0.030f * ambient_reflection_response
            - 0.070f * caustic_flow_response,
        0.48f,
        0.82f);
    profile.intensity = ambient_intensity;

    if (plan.interaction.specular_highlight_active) {
        profile.model = plan.interaction.specular_model;
        profile.source = std::string_view{plan.interaction.specular_model}
                == "focus-specular"
            ? "sampled-backdrop-focus-lighting"
            : "sampled-backdrop-pointer-lighting";
        profile.interaction_driven = true;
        profile.anchor_x = plan.interaction.specular_anchor_x;
        profile.anchor_y = plan.interaction.specular_anchor_y;
        profile.radius = plan.interaction.specular_radius;
        profile.intensity = std::clamp(
            plan.interaction.specular_intensity + ambient_intensity * 0.35f,
            0.0f,
            0.85f);
    }
    return profile;
}

inline MaterialBackdropAccess material_resolve_backdrop_access(
        MaterialPlan const& plan) noexcept {
    MaterialBackdropAccess access{};
    if (!plan.backdrop_sampling
        && !plan.decision_trace.next_frame_capture_required)
        return access;
    access.required = plan.backdrop_sampling;
    access.stable_required = plan.backdrop_sampling;
    access.frame_history_required = plan.backdrop_sampling;
    access.shared_frame_capture = true;
    access.next_frame_capture_required = true;
    access.excludes_foreground_text = true;
    bool const has_named_backdrop_source =
        plan.backdrop.source
        && plan.backdrop.source[0]
        && std::string_view{plan.backdrop.source} != "none";
    access.source = has_named_backdrop_source
        ? plan.backdrop.source
        : "previous-presented-frame";
    access.capture_scope = "shared-frame";
    access.capture_reason = plan.backdrop_sampling
        ? "sample-current-frame"
        : "warmup-next-frame";
    access.max_frame_capture_count = 1;
    access.max_frame_capture_pixels = plan.render_target.pixel_count;
    access.max_surface_sample_pixels = plan.backdrop_sampling
        ? material_estimate_surface_sample_pixels(plan)
        : 0;
    access.bounded = true;
    return access;
}

inline MaterialLuminanceCurve material_resolve_luminance_curve(
        bool backdrop_sampling,
        MaterialBackdropAnalysis backdrop,
        float luminance_floor,
        float luminance_gain,
        float edge_highlight) noexcept {
    MaterialLuminanceCurve curve{};
    curve.floor = std::clamp(luminance_floor, 0.0f, 1.0f);
    curve.gain = std::max(0.0f, luminance_gain);
    curve.edge_lift = std::clamp(edge_highlight, 0.0f, 1.0f);
    if (!backdrop_sampling)
        return curve;

    auto const luma_mean = std::clamp(backdrop.luma_mean, 0.0f, 1.0f);
    auto const luma_span = std::clamp(backdrop.luma_span, 0.0f, 1.0f);
    curve.name = "adaptive-backdrop-luma";
    curve.backdrop_driven = true;
    curve.midpoint = std::clamp(luma_mean, 0.25f, 0.75f);

    if (luma_mean < 0.35f) {
        auto const darkness =
            std::clamp((0.35f - luma_mean) / 0.35f, 0.0f, 1.0f);
        curve.gamma = 0.92f - 0.06f * darkness;
    } else if (luma_mean > 0.72f) {
        auto const brightness =
            std::clamp((luma_mean - 0.72f) / 0.28f, 0.0f, 1.0f);
        curve.gamma = 1.04f + 0.06f * brightness;
    }

    if (luma_span < 0.12f) {
        auto const flatness =
            std::clamp((0.12f - luma_span) / 0.12f, 0.0f, 1.0f);
        curve.contrast = 1.0f + 0.12f * flatness;
    }
    return curve;
}

inline char const* material_plan_id(MaterialKind kind,
                                    MaterialSurfaceRole role,
                                    bool backdrop_sampling) noexcept {
    if (material_role_uses_standard_content_layer(role)) {
        switch (kind) {
            case MaterialKind::Clear:
                return "material.clear.standard-material";
            case MaterialKind::Thin:
                return "material.thin.standard-material";
            case MaterialKind::Regular:
                return "material.regular.standard-material";
            case MaterialKind::Thick:
                return "material.thick.standard-material";
            case MaterialKind::None:
            default:
                return "material.none";
        }
    }
    switch (kind) {
        case MaterialKind::Clear:
            return backdrop_sampling ? "material.clear.liquid-glass"
                                     : "material.clear.fallback";
        case MaterialKind::Thin:
            return backdrop_sampling ? "material.thin.liquid-glass"
                                     : "material.thin.fallback";
        case MaterialKind::Regular:
            return backdrop_sampling ? "material.regular.liquid-glass"
                                     : "material.regular.fallback";
        case MaterialKind::Thick:
            return backdrop_sampling ? "material.thick.liquid-glass"
                                     : "material.thick.fallback";
        case MaterialKind::None:
        default:
            return "material.none";
    }
}

inline char const* material_reference_shape_name(
        MaterialShapeAnalysis shape) noexcept {
    return material_shape_kind_name(shape.kind);
}

inline char const* material_reference_blending_scope(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "none";
    if (plan.backdrop_sampling)
        return "sampled-backdrop";
    if (plan.fallback())
        return "deterministic-fallback";
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-fill";
    return "none";
}

inline char const* material_reference_technology(
        MaterialPlan const& plan) noexcept {
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-material";
    return "liquid-glass";
}

inline char const* material_reference_layer(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "inactive";
    if (material_plan_uses_standard_content_layer(plan))
        return "content-layer";
    return "functional-layer";
}

inline char const* material_reference_policy(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "inactive";
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-material-content-layer";
    return "liquid-glass-functional-layer";
}

inline char const* material_reference_accessibility_response(
        MaterialPlan const& plan) noexcept {
    unsigned int accessibility_adjustment_count = 0;
    if (plan.decision_trace.reduced_transparency)
        ++accessibility_adjustment_count;
    if (plan.decision_trace.increase_contrast)
        ++accessibility_adjustment_count;
    if (plan.decision_trace.reduce_motion)
        ++accessibility_adjustment_count;
    if (accessibility_adjustment_count > 1u)
        return "combined-accessibility";
    if (plan.decision_trace.reduced_transparency)
        return "reduced-transparency";
    if (plan.decision_trace.increase_contrast)
        return "increased-contrast";
    if (plan.decision_trace.reduce_motion)
        return "reduced-motion";
    return "standard";
}

inline bool material_reference_uses_budgeted_effects(
        MaterialPlan const& plan) noexcept {
    return plan.backdrop_sampling
        && (plan.resource_budget.max_blur_radius < material_max_blur_radius
            || plan.resource_budget.max_sample_taps < material_max_sample_taps
            || !plan.quality_policy.allow_noise
            || !plan.quality_policy.allow_shadow);
}

inline char const* material_reference_performance_response(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "inactive";
    if (plan.backdrop_access.next_frame_capture_required
        && !plan.backdrop_sampling
        && plan.fallback_path == MaterialFallbackPath::NoBackdropSource)
        return "warmup-capture";
    if (plan.fallback())
        return "deterministic-fallback";
    if (material_reference_uses_budgeted_effects(plan))
        return "budgeted-effects";
    return "standard";
}

inline MaterialReferenceModel material_resolve_reference_model(
        MaterialPlan const& plan) noexcept {
    MaterialReferenceModel model{};
    model.technology = material_reference_technology(plan);
    model.layer = material_reference_layer(plan);
    model.material_policy = material_reference_policy(plan);
    model.variant = material_kind_name(plan.kind);
    model.shape = material_reference_shape_name(plan.shape);
    model.blending_scope = material_reference_blending_scope(plan);
    model.semantic_thickness = material_kind_name(plan.kind);
    model.accessibility_response =
        material_reference_accessibility_response(plan);
    model.performance_response =
        material_reference_performance_response(plan);
    model.view_bounds_anchored = plan.kind != MaterialKind::None
        && plan.shape.valid;
    model.shape_matches_geometry = model.view_bounds_anchored
        && plan.shape.effective_radius <= plan.shape.radius_limit;
    model.tint_applied = plan.kind != MaterialKind::None
        && plan.tint.a > 0;
    model.interactive_response = plan.interaction.enabled;
    model.container_grouped = plan.container.participates;
    model.container_union = plan.container.shape_union_expected;
    model.container_morphing = plan.container.morph_transitions;
    model.glass_effect_identified = plan.glass_identity.participates;
    model.glass_effect_matched_geometry =
        plan.glass_identity.matched_geometry_candidate;
    model.glass_background_effect = plan.glass_background.kind_name;
    model.glass_background_plate = plan.glass_background.plate;
    model.glass_background_feathered = plan.glass_background.feathered;
    model.legibility_preserved =
        plan.foreground.primary_contrast_ratio
            >= plan.foreground.minimum_contrast_ratio
        && plan.foreground.secondary_contrast_ratio
            >= plan.foreground.minimum_contrast_ratio
        && plan.foreground.accent_contrast_ratio
            >= plan.foreground.minimum_contrast_ratio;
    model.vibrancy_expected = plan.foreground.uses_vibrancy;
    model.deterministic_degradation =
        plan.resource_budget.deterministic_fallback;
    return model;
}

inline char const* material_optical_response_model_name(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "inactive";
    if (plan.backdrop_sampling)
        return "sampled-backdrop";
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-content";
    return "deterministic-fallback";
}

inline char const* material_optical_blur_strategy_name(
        MaterialPlan const& plan) noexcept {
    if (!plan.primary_pass.active)
        return "none";
    if (plan.primary_pass.requires_backdrop)
        return "backdrop-sample-blur";
    if (material_stage_matches(plan.primary_pass.executor, "standard-fill"))
        return "standard-fill";
    if (material_stage_matches(plan.primary_pass.executor, "fallback-fill"))
        return "fallback-fill";
    return "none";
}

inline char const* material_optical_color_strategy_name(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None || plan.tint.a == 0)
        return "none";
    if (plan.backdrop_sampling)
        return "adaptive-backdrop-color";
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-content-color";
    return "fallback-solid-color";
}

inline char const* material_optical_depth_strategy_name(
        MaterialPlan const& plan) noexcept {
    if (!plan.primary_pass.active)
        return "none";
    bool const edge = plan.edge_highlight > 0.0f;
    bool const shadow = plan.shadow_alpha > 0.0f;
    bool const noise = plan.noise_opacity > 0.0f && plan.backdrop_sampling;
    if (plan.glass_depth.active)
        return "adaptive-glass-depth";
    if (plan.backdrop_sampling && shadow && edge && noise)
        return "layered-shadow-edge-noise";
    if (plan.backdrop_sampling && (shadow || edge))
        return "layered-shadow-edge";
    if (material_plan_uses_standard_content_layer(plan) && edge)
        return "standard-content-edge";
    if ((plan.fallback() || !plan.backdrop_sampling) && shadow && edge)
        return "fallback-shadow-edge";
    if ((plan.fallback() || !plan.backdrop_sampling) && edge)
        return "fallback-edge";
    return "none";
}

inline char const* material_optical_frosting_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None)
        return "none";
    if (plan.backdrop_sampling)
        return "sampled-backdrop-frosting";
    if (plan.fallback())
        return "solid-fallback-frosting";
    if (material_plan_uses_standard_content_layer(plan))
        return "standard-material-fill";
    return "none";
}

inline char const* material_optical_tint_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.kind == MaterialKind::None || plan.tint.a == 0)
        return "none";
    if (plan.backdrop_sampling)
        return "adaptive-backdrop-tint";
    return "style-tint";
}

inline char const* material_optical_interaction_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.interaction.active
        && plan.interaction.response_model
        && plan.interaction.response_model[0])
        return plan.interaction.response_model;
    return "none";
}

inline char const* material_optical_transition_source_name(
        MaterialPlan const& plan) noexcept {
    if (!plan.transition.active)
        return plan.transition.policy;
    if (plan.transition.materialize)
        return "glass-effect-materialize";
    if (plan.transition.matched_geometry)
        return "glass-effect-matched-geometry";
    return plan.transition.policy;
}

inline char const* material_optical_refraction_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.refraction.active
        && plan.refraction.source
        && plan.refraction.source[0])
        return plan.refraction.source;
    return "none";
}

inline char const* material_optical_spectral_tint_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.spectral_tint.active
        && plan.spectral_tint.source
        && plan.spectral_tint.source[0])
        return plan.spectral_tint.source;
    return "none";
}

inline char const* material_optical_dynamic_lighting_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.dynamic_lighting.active
        && plan.dynamic_lighting.source
        && plan.dynamic_lighting.source[0])
        return plan.dynamic_lighting.source;
    return "none";
}

inline char const* material_optical_glass_caustic_flow_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_caustic_flow.active
        && plan.glass_caustic_flow.source
        && plan.glass_caustic_flow.source[0])
        return plan.glass_caustic_flow.source;
    return "none";
}

inline char const* material_optical_glass_meniscus_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_meniscus.active
        && plan.glass_meniscus.source
        && plan.glass_meniscus.source[0])
        return plan.glass_meniscus.source;
    return "none";
}

inline char const* material_optical_glass_transmission_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_transmission.active
        && plan.glass_transmission.source
        && plan.glass_transmission.source[0])
        return plan.glass_transmission.source;
    return "none";
}

inline char const* material_optical_glass_surface_cohesion_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_surface_cohesion.active
        && plan.glass_surface_cohesion.source
        && plan.glass_surface_cohesion.source[0])
        return plan.glass_surface_cohesion.source;
    return "none";
}

inline char const* material_optical_glass_substrate_adhesion_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_substrate_adhesion.active
        && plan.glass_substrate_adhesion.source
        && plan.glass_substrate_adhesion.source[0])
        return plan.glass_substrate_adhesion.source;
    return "none";
}

inline char const* material_optical_glass_ambient_reflection_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_ambient_reflection.active
        && plan.glass_ambient_reflection.source
        && plan.glass_ambient_reflection.source[0])
        return plan.glass_ambient_reflection.source;
    return "none";
}

inline char const* material_optical_glass_thickness_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_thickness.active
        && plan.glass_thickness.source
        && plan.glass_thickness.source[0])
        return plan.glass_thickness.source;
    return "none";
}

inline char const* material_optical_glass_dispersion_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_dispersion.active
        && plan.glass_dispersion.source
        && plan.glass_dispersion.source[0])
        return plan.glass_dispersion.source;
    return "none";
}

inline char const* material_optical_glass_stabilization_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_stabilization.active
        && plan.glass_stabilization.source
        && plan.glass_stabilization.source[0])
        return plan.glass_stabilization.source;
    return "none";
}

inline char const* material_optical_glass_environment_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_environment.active
        && plan.glass_environment.source
        && plan.glass_environment.source[0])
        return plan.glass_environment.source;
    return "none";
}

inline char const* material_optical_glass_depth_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.glass_depth.active
        && plan.glass_depth.source
        && plan.glass_depth.source[0])
        return plan.glass_depth.source;
    return "none";
}

inline char const* material_optical_scroll_edge_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.scroll_edge.active
        && plan.scroll_edge.source
        && plan.scroll_edge.source[0])
        return plan.scroll_edge.source;
    return "none";
}

inline char const* material_optical_prominent_glass_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.prominent_glass.active
        && plan.prominent_glass.source
        && plan.prominent_glass.source[0])
        return plan.prominent_glass.source;
    return "none";
}

inline char const* material_optical_clear_glass_legibility_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.clear_glass_legibility.active
        && plan.clear_glass_legibility.source
        && plan.clear_glass_legibility.source[0])
        return plan.clear_glass_legibility.source;
    return "none";
}

inline char const* material_optical_large_surface_legibility_source_name(
        MaterialPlan const& plan) noexcept {
    if (plan.large_surface_legibility.active
        && plan.large_surface_legibility.source
        && plan.large_surface_legibility.source[0])
        return plan.large_surface_legibility.source;
    return "none";
}

inline char const* material_optical_stage_order_name(
        MaterialPlan const& plan) noexcept {
    if (!plan.primary_pass.active)
        return "none";
    bool const shadow = plan.shadow_alpha > 0.0f;
    bool const edge = plan.edge_highlight > 0.0f;
    bool const noise = plan.backdrop_sampling && plan.noise_opacity > 0.0f;
    if (shadow && edge && noise)
        return "shadow-primary-edge-noise";
    if (shadow && edge)
        return "shadow-primary-edge";
    if (shadow && noise)
        return "shadow-primary-noise";
    if (edge && noise)
        return "primary-edge-noise";
    if (shadow)
        return "shadow-primary";
    if (edge)
        return "primary-edge";
    if (noise)
        return "primary-noise";
    return "primary";
}

inline char const* material_optical_backdrop_capture_policy_name(
        MaterialPlan const& plan) noexcept {
    if (!plan.backdrop_access.shared_frame_capture
        && !plan.backdrop_access.next_frame_capture_required
        && !plan.backdrop_access.required) {
        return "no-capture";
    }
    if (plan.backdrop_sampling)
        return "sample-current-frame";
    if (plan.backdrop_access.next_frame_capture_required)
        return "warmup-next-frame";
    return "shared-frame-capture";
}

inline char const* material_optical_foreground_sampling_policy_name(
        MaterialPlan const& plan) noexcept {
    bool const captures_backdrop =
        plan.backdrop_access.shared_frame_capture
        || plan.backdrop_access.next_frame_capture_required
        || plan.backdrop_access.required;
    if (!captures_backdrop)
        return "not-applicable";
    if (plan.backdrop_access.excludes_foreground_text
        && plan.backdrop_sampling) {
        return "foreground-excluded-from-sample";
    }
    if (plan.backdrop_access.excludes_foreground_text)
        return "foreground-excluded-from-warmup";
    return "foreground-inclusion-risk";
}

inline MaterialOpticalComposition material_resolve_optical_composition(
        MaterialPlan const& plan) noexcept {
    MaterialOpticalComposition composition{};
    composition.schema_version = plan.contract_version;
    composition.model = material_optical_response_model_name(plan);
    composition.blur_source = material_optical_blur_strategy_name(plan);
    composition.frosting_source = material_optical_frosting_source_name(plan);
    composition.tint_source = material_optical_tint_source_name(plan);
    composition.luminance_source = plan.luminance_curve.name;
    composition.depth_source = material_optical_depth_strategy_name(plan);
    composition.refraction_source =
        material_optical_refraction_source_name(plan);
    composition.spectral_tint_source =
        material_optical_spectral_tint_source_name(plan);
    composition.dynamic_lighting_source =
        material_optical_dynamic_lighting_source_name(plan);
    composition.glass_caustic_flow_source =
        material_optical_glass_caustic_flow_source_name(plan);
    composition.glass_meniscus_source =
        material_optical_glass_meniscus_source_name(plan);
    composition.glass_transmission_source =
        material_optical_glass_transmission_source_name(plan);
    composition.glass_surface_cohesion_source =
        material_optical_glass_surface_cohesion_source_name(plan);
    composition.glass_substrate_adhesion_source =
        material_optical_glass_substrate_adhesion_source_name(plan);
    composition.glass_ambient_reflection_source =
        material_optical_glass_ambient_reflection_source_name(plan);
    composition.glass_thickness_source =
        material_optical_glass_thickness_source_name(plan);
    composition.glass_dispersion_source =
        material_optical_glass_dispersion_source_name(plan);
    composition.glass_stabilization_source =
        material_optical_glass_stabilization_source_name(plan);
    composition.glass_environment_source =
        material_optical_glass_environment_source_name(plan);
    composition.glass_depth_source =
        material_optical_glass_depth_source_name(plan);
    composition.scroll_edge_source =
        material_optical_scroll_edge_source_name(plan);
    composition.prominent_glass_source =
        material_optical_prominent_glass_source_name(plan);
    composition.clear_glass_legibility_source =
        material_optical_clear_glass_legibility_source_name(plan);
    composition.large_surface_legibility_source =
        material_optical_large_surface_legibility_source_name(plan);
    composition.interaction_source =
        material_optical_interaction_source_name(plan);
    composition.transition_source =
        material_optical_transition_source_name(plan);
    composition.fallback_source = plan.fallback()
        ? material_fallback_path_name(plan.fallback_path)
        : "none";
    composition.stage_order = material_optical_stage_order_name(plan);
    composition.backdrop_capture_policy =
        material_optical_backdrop_capture_policy_name(plan);
    composition.foreground_sampling_policy =
        material_optical_foreground_sampling_policy_name(plan);
    composition.backdrop_sampled = plan.backdrop_sampling;
    composition.blur_required = plan.primary_pass.requires_backdrop;
    composition.frosting_required = plan.backdrop_sampling;
    composition.tint_required =
        plan.kind != MaterialKind::None && plan.tint.a > 0;
    composition.saturation_required =
        plan.backdrop_sampling && std::fabs(plan.saturation - 1.0f) > 0.0001f;
    composition.luminance_required =
        plan.kind != MaterialKind::None
        && (plan.luminance_curve.bounded
            || plan.foreground.primary_contrast_ratio
                >= plan.foreground.minimum_contrast_ratio);
    composition.edge_required =
        plan.primary_pass.active && plan.edge_highlight > 0.0f;
    composition.shadow_required =
        plan.primary_pass.active && plan.shadow_alpha > 0.0f;
    composition.noise_required =
        plan.backdrop_sampling && plan.noise_opacity > 0.0f;
    composition.refraction_required = plan.refraction.active;
    composition.spectral_tint_required = plan.spectral_tint.active;
    composition.dynamic_lighting_required = plan.dynamic_lighting.active;
    composition.glass_caustic_flow_required =
        plan.glass_caustic_flow.active;
    composition.glass_meniscus_required = plan.glass_meniscus.active;
    composition.glass_transmission_required =
        plan.glass_transmission.active;
    composition.glass_surface_cohesion_required =
        plan.glass_surface_cohesion.active;
    composition.glass_substrate_adhesion_required =
        plan.glass_substrate_adhesion.active;
    composition.glass_ambient_reflection_required =
        plan.glass_ambient_reflection.active;
    composition.glass_thickness_required = plan.glass_thickness.active;
    composition.glass_dispersion_required = plan.glass_dispersion.active;
    composition.glass_stabilization_required =
        plan.glass_stabilization.active;
    composition.glass_environment_required =
        plan.glass_environment.active;
    composition.glass_depth_required = plan.glass_depth.active;
    composition.scroll_edge_required = plan.scroll_edge.active;
    composition.prominent_glass_required = plan.prominent_glass.active;
    composition.clear_glass_legibility_required =
        plan.clear_glass_legibility.active;
    composition.large_surface_legibility_required =
        plan.large_surface_legibility.active;
    composition.interaction_required = plan.interaction.active;
    composition.transition_required = plan.transition.active;
    composition.fallback_required = plan.fallback();
    composition.backdrop_capture_required =
        plan.backdrop_access.shared_frame_capture
        || plan.backdrop_access.next_frame_capture_required
        || plan.backdrop_access.required;
    composition.foreground_excluded_from_backdrop =
        plan.backdrop_access.excludes_foreground_text;
    composition.stage_order_stable = true;
    composition.bounded =
        plan.resource_budget.bounded_texture_copy
        && plan.sampling_kernel.bounded
        && plan.luminance_curve.bounded
        && plan.backdrop_access.bounded
        && plan.refraction.bounded
        && plan.edge_optics.bounded
        && plan.spectral_tint.bounded
        && plan.dynamic_lighting.bounded
        && plan.glass_caustic_flow.bounded
        && plan.glass_meniscus.bounded
        && plan.glass_transmission.bounded
        && plan.glass_surface_cohesion.bounded
        && plan.glass_substrate_adhesion.bounded
        && plan.glass_ambient_reflection.bounded
        && plan.glass_thickness.bounded
        && plan.glass_dispersion.bounded
        && plan.glass_stabilization.bounded
        && plan.glass_environment.bounded
        && plan.glass_depth.bounded
        && plan.scroll_edge.bounded
        && plan.prominent_glass.bounded
        && plan.clear_glass_legibility.bounded
        && plan.large_surface_legibility.bounded
        && plan.transition.bounded;
    composition.deterministic =
        plan.resource_budget.deterministic_fallback
        && plan.foreground.deterministic
        && plan.interaction.deterministic;
    composition.opacity = plan.opacity;
    composition.blur_radius = plan.blur_radius;
    composition.tint_alpha = material_alpha_fraction(plan.tint);
    composition.saturation = plan.saturation;
    composition.luminance_floor = plan.luminance_floor;
    composition.luminance_gain = plan.luminance_gain;
    composition.edge_highlight = plan.edge_highlight;
    composition.edge_width = plan.edge_width;
    composition.noise_opacity = plan.noise_opacity;
    composition.shadow_alpha = plan.shadow_alpha;
    composition.shadow_radius = plan.shadow_radius;
    composition.refraction_strength = plan.refraction.strength;
    composition.refraction_edge_bias = plan.refraction.edge_bias;
    composition.refraction_offset_pixels = plan.refraction.max_offset_pixels;
    composition.refraction_edge_caustic_intensity =
        plan.refraction.edge_caustic_intensity;
    composition.edge_bevel_width = plan.edge_optics.bevel_width;
    composition.edge_inner_highlight = plan.edge_optics.inner_highlight;
    composition.edge_outer_shadow = plan.edge_optics.outer_shadow;
    composition.edge_chromatic_fringe = plan.edge_optics.chromatic_fringe;
    composition.spectral_tint_warmth = plan.spectral_tint.warmth;
    composition.spectral_tint_coolness = plan.spectral_tint.coolness;
    composition.spectral_dispersion = plan.spectral_tint.dispersion;
    composition.spectral_rim_tint = plan.spectral_tint.rim_tint;
    composition.spectral_tint_balance = plan.spectral_tint.balance;
    composition.spectral_tint_influence =
        plan.spectral_tint.tint_influence;
    composition.dynamic_light_direction_x =
        plan.dynamic_lighting.direction_x;
    composition.dynamic_light_direction_y =
        plan.dynamic_lighting.direction_y;
    composition.dynamic_light_highlight =
        plan.dynamic_lighting.highlight_strength;
    composition.dynamic_light_shadow =
        plan.dynamic_lighting.shadow_strength;
    composition.glass_caustic_flow_strength =
        plan.glass_caustic_flow.flow_strength;
    composition.glass_caustic_chroma_shear =
        plan.glass_caustic_flow.chroma_shear;
    composition.glass_caustic_highlight_drift =
        plan.glass_caustic_flow.highlight_drift;
    composition.glass_caustic_focus =
        plan.glass_caustic_flow.caustic_focus;
    composition.glass_meniscus_rim_width =
        plan.glass_meniscus.rim_width;
    composition.glass_meniscus_edge_pull =
        plan.glass_meniscus.edge_pull;
    composition.glass_meniscus_highlight_gain =
        plan.glass_meniscus.highlight_gain;
    composition.glass_meniscus_refraction_gain =
        plan.glass_meniscus.refraction_gain;
    composition.glass_internal_transmission =
        plan.glass_transmission.internal_transmission;
    composition.glass_subsurface_scatter =
        plan.glass_transmission.subsurface_scatter;
    composition.glass_volume_absorption =
        plan.glass_transmission.volume_absorption;
    composition.glass_interlayer_refraction =
        plan.glass_transmission.interlayer_refraction;
    composition.glass_surface_response =
        plan.glass_surface_cohesion.surface_response;
    composition.glass_edge_adhesion =
        plan.glass_surface_cohesion.edge_adhesion;
    composition.glass_shape_coalescence =
        plan.glass_surface_cohesion.shape_coalescence;
    composition.glass_luma_stability =
        plan.glass_surface_cohesion.luma_stability;
    composition.glass_substrate_contact =
        plan.glass_substrate_adhesion.contact_strength;
    composition.glass_substrate_settle =
        plan.glass_substrate_adhesion.settle_depth;
    composition.glass_substrate_shadow =
        plan.glass_substrate_adhesion.contact_shadow;
    composition.glass_substrate_refraction =
        plan.glass_substrate_adhesion.refraction_crawl;
    composition.glass_ambient_reflection_gain =
        plan.glass_ambient_reflection.reflection_gain;
    composition.glass_ambient_color_bleed =
        plan.glass_ambient_reflection.color_bleed;
    composition.glass_ambient_luma_polarization =
        plan.glass_ambient_reflection.luma_polarization;
    composition.glass_ambient_sheen_coherence =
        plan.glass_ambient_reflection.sheen_coherence;
    composition.glass_thickness = plan.glass_thickness.thickness;
    composition.glass_lensing_gain = plan.glass_thickness.lensing_gain;
    composition.glass_shadow_gain = plan.glass_thickness.shadow_gain;
    composition.glass_scattering_gain = plan.glass_thickness.scattering_gain;
    composition.glass_dispersion_axial_offset =
        plan.glass_dispersion.axial_offset_pixels;
    composition.glass_dispersion_tangential_offset =
        plan.glass_dispersion.tangential_offset_pixels;
    composition.glass_dispersion_prismatic_gain =
        plan.glass_dispersion.prismatic_gain;
    composition.glass_dispersion_caustic_spread =
        plan.glass_dispersion.caustic_spread;
    composition.glass_stabilization_strength =
        plan.glass_stabilization.strength;
    composition.glass_stabilization_damping =
        plan.glass_stabilization.damping;
    composition.glass_stabilization_shimmer_reduction =
        plan.glass_stabilization.shimmer_reduction;
    composition.glass_stabilization_transmission_bias =
        plan.glass_stabilization.transmission_bias;
    composition.glass_environment_reflection_strength =
        plan.glass_environment.reflection_strength;
    composition.glass_environment_color_pickup =
        plan.glass_environment.color_pickup;
    composition.glass_environment_luminance_balance =
        plan.glass_environment.luminance_balance;
    composition.glass_environment_transmission_balance =
        plan.glass_environment.transmission_balance;
    composition.glass_depth_separation =
        plan.glass_depth.depth_separation;
    composition.glass_depth_inner_shadow =
        plan.glass_depth.inner_shadow;
    composition.glass_depth_surface_lift =
        plan.glass_depth.surface_lift;
    composition.glass_depth_parallax_gain =
        plan.glass_depth.parallax_gain;
    composition.scroll_edge_response = plan.scroll_edge.response_strength;
    composition.scroll_edge_extent = plan.scroll_edge.fade_extent_pixels;
    composition.scroll_edge_dissolve = plan.scroll_edge.dissolve_strength;
    composition.scroll_edge_dimming = plan.scroll_edge.dimming_strength;
    composition.scroll_edge_hard_style =
        plan.scroll_edge.hard_style_strength;
    composition.scroll_edge_opacity_delta =
        plan.scroll_edge.opacity_delta;
    composition.scroll_edge_tint_alpha_delta =
        plan.scroll_edge.tint_alpha_delta;
    composition.scroll_edge_luminance_floor_delta =
        plan.scroll_edge.luminance_floor_delta;
    composition.scroll_edge_edge_highlight_delta =
        plan.scroll_edge.edge_highlight_delta;
    composition.prominent_glass_intensity =
        plan.prominent_glass.intensity;
    composition.prominent_glass_tint_weight =
        plan.prominent_glass.tint_weight;
    composition.prominent_glass_edge_lift =
        plan.prominent_glass.edge_lift;
    composition.prominent_glass_lensing_gain =
        plan.prominent_glass.lensing_gain;
    composition.clear_glass_dimming =
        plan.clear_glass_legibility.dimming_strength;
    composition.clear_glass_contrast =
        plan.clear_glass_legibility.contrast_lift;
    composition.clear_glass_brightness_response =
        plan.clear_glass_legibility.brightness_response;
    composition.clear_glass_detail_response =
        plan.clear_glass_legibility.detail_response;
    composition.large_surface_legibility_response =
        plan.large_surface_legibility.response_strength;
    composition.large_surface_opacity_delta =
        plan.large_surface_legibility.opacity_delta;
    composition.large_surface_tint_alpha_delta =
        plan.large_surface_legibility.tint_alpha_delta;
    composition.large_surface_luminance_floor_delta =
        plan.large_surface_legibility.luminance_floor_delta;
    composition.large_surface_edge_highlight_delta =
        plan.large_surface_legibility.edge_highlight_delta;
    composition.interaction_response_strength =
        plan.interaction.response_strength;
    composition.interaction_control_morph_scale_delta =
        plan.interaction.control_morph_scale_delta;
    composition.interaction_control_morph_depth =
        plan.interaction.control_morph_depth;
    composition.interaction_control_morph_edge =
        plan.interaction.control_morph_edge;
    composition.interaction_control_morph_shadow =
        plan.interaction.control_morph_shadow;
    composition.transition_progress = plan.transition.progress;
    composition.transition_opacity_gain = plan.transition.opacity_gain;
    composition.transition_optical_gain = plan.transition.optical_gain;
    composition.transition_refraction_gain =
        plan.transition.refraction_gain;
    composition.transition_materialize_wave_strength =
        plan.transition.materialize_wave_strength;
    composition.transition_materialize_edge_lift =
        plan.transition.materialize_edge_lift;
    composition.transition_materialize_lensing_gain =
        plan.transition.materialize_lensing_gain;
    composition.transition_materialize_rim_position =
        plan.transition.materialize_rim_position;
    composition.sample_taps = plan.sample_taps;
    composition.max_texture_copy_pixels =
        plan.primary_pass.max_texture_copy_pixels;
    composition.max_surface_sample_pixels =
        plan.backdrop_access.max_surface_sample_pixels;
    return composition;
}

inline MaterialOpticalResponseContract material_resolve_optical_response(
        MaterialPlan const& plan) noexcept {
    auto const& composition = plan.optical_composition;
    MaterialOpticalResponseContract response{};
    response.response_model = composition.model;
    response.blur_strategy = composition.blur_source;
    response.color_strategy = material_optical_color_strategy_name(plan);
    response.depth_strategy = composition.depth_source;
    response.backdrop_driven = composition.backdrop_sampled;
    response.blur_active = composition.blur_required;
    response.frosting_active = composition.frosting_required;
    response.tint_active = composition.tint_required;
    response.saturation_active = composition.saturation_required;
    response.luminance_preservation_active =
        composition.luminance_required;
    response.edge_highlight_active = composition.edge_required;
    response.depth_shadow_active = composition.shadow_required;
    response.noise_dither_active = composition.noise_required;
    response.refraction_active = composition.refraction_required;
    response.spectral_tint_active = composition.spectral_tint_required;
    response.dynamic_lighting_active =
        composition.dynamic_lighting_required;
    response.glass_caustic_flow_active =
        composition.glass_caustic_flow_required;
    response.glass_meniscus_active =
        composition.glass_meniscus_required;
    response.glass_transmission_active =
        composition.glass_transmission_required;
    response.glass_surface_cohesion_active =
        composition.glass_surface_cohesion_required;
    response.glass_substrate_adhesion_active =
        composition.glass_substrate_adhesion_required;
    response.glass_ambient_reflection_active =
        composition.glass_ambient_reflection_required;
    response.glass_thickness_active =
        composition.glass_thickness_required;
    response.glass_dispersion_active =
        composition.glass_dispersion_required;
    response.glass_stabilization_active =
        composition.glass_stabilization_required;
    response.glass_environment_active =
        composition.glass_environment_required;
    response.glass_depth_active =
        composition.glass_depth_required;
    response.scroll_edge_active =
        composition.scroll_edge_required;
    response.prominent_glass_active =
        composition.prominent_glass_required;
    response.clear_glass_legibility_active =
        composition.clear_glass_legibility_required;
    response.large_surface_legibility_active =
        composition.large_surface_legibility_required;
    response.foreground_vibrancy_active = plan.foreground.uses_vibrancy;
    response.interaction_active = composition.interaction_required;
    response.interaction_modulates_optics =
        composition.interaction_required
        && (std::fabs(plan.interaction.opacity_delta) > 0.0001f
            || std::fabs(plan.interaction.blur_radius_delta) > 0.0001f
            || std::fabs(plan.interaction.saturation_delta) > 0.0001f
            || std::fabs(plan.interaction.edge_highlight_delta) > 0.0001f
            || std::fabs(plan.interaction.shadow_alpha_delta) > 0.0001f
            || std::fabs(plan.interaction.shadow_radius_delta) > 0.0001f
            || plan.interaction.control_morph_active
            || plan.interaction.pointer_lens_active);
    response.deterministic_fallback = composition.deterministic;
    return response;
}

inline void apply_material_reduced_transparency_policy(
        MaterialPlan& plan) noexcept {
    if (!plan.decision_trace.reduced_transparency
        || plan.kind == MaterialKind::None) {
        return;
    }

    plan.opacity = std::clamp(std::max(plan.opacity, 0.88f), 0.0f, 1.0f);
    plan.tint.a = static_cast<unsigned char>(
        std::max(static_cast<int>(plan.tint.a), 224));
    plan.luminance_floor = std::clamp(
        std::max(plan.luminance_floor, 0.14f),
        0.0f,
        1.0f);
    plan.edge_highlight = std::clamp(
        std::max(plan.edge_highlight, 0.46f),
        0.0f,
        1.0f);
    plan.edge_width = std::max(plan.edge_width, 1.35f);
    if (plan.quality_policy.allow_shadow) {
        plan.shadow_alpha = std::clamp(
            std::max(plan.shadow_alpha, 0.16f),
            0.0f,
            0.4f);
        plan.shadow_radius = std::max(plan.shadow_radius, 12.0f);
    }
}

inline void apply_material_increased_contrast_policy(
        MaterialPlan& plan) noexcept {
    if (!plan.decision_trace.increase_contrast
        || plan.kind == MaterialKind::None) {
        return;
    }

    plan.opacity = std::clamp(plan.opacity + 0.12f, 0.0f, 1.0f);
    plan.luminance_floor = std::clamp(
        plan.luminance_floor + 0.05f,
        0.0f,
        1.0f);
    plan.saturation = std::min(plan.saturation, 1.0f);
    plan.edge_highlight = std::clamp(plan.edge_highlight + 0.10f, 0.0f, 1.0f);
    plan.edge_width = std::clamp(plan.edge_width + 0.35f, 0.5f, 8.0f);
    if (plan.quality_policy.allow_shadow)
        plan.shadow_alpha = std::clamp(plan.shadow_alpha + 0.02f, 0.0f, 0.4f);
}

inline MaterialPlan plan_material_surface(MaterialRequest request,
                                          MaterialEnvironment environment) noexcept {
    MaterialPlan plan{};
    auto const& style = request.style;
    auto const capability_snapshot = analyze_material_capabilities(
        environment.capabilities,
        environment.render_target);
    MaterialQualityPolicy resolved_quality =
        resolve_material_quality_policy(
            environment.quality,
            capability_snapshot);
    auto const max_blur_radius = std::max(
        0.0f,
        resolved_quality.max_blur_radius);
    plan.kind = style.kind;
    plan.role = style.role;
    plan.command_descriptor = material_command_descriptor(style);
    plan.geometry = request.geometry;
    plan.shape = analyze_material_shape(request.geometry);
    plan.capability_snapshot = capability_snapshot;
    plan.quality_policy = resolved_quality;
    plan.render_target = analyze_material_render_target(
        environment.render_target,
        resolved_quality.max_backdrop_pixels);
    plan.container = analyze_material_container(
        style.container,
        environment.capabilities.reduce_motion);
    plan.transition = analyze_material_transition(
        style.transition,
        environment.capabilities.reduce_motion);
    plan.glass_identity = analyze_material_glass_identity(
        style.glass_identity,
        plan.container,
        plan.transition);
    plan.glass_background = analyze_material_glass_background(
        style.glass_background);
    plan.opacity = std::clamp(style.opacity, 0.0f, 1.0f);
    plan.blur_radius = std::clamp(
        style.blur_radius, 0.0f, max_blur_radius);
    plan.tint = style.tint;
    plan.theme = material_resolve_theme_snapshot(style);
    plan.saturation = std::max(0.0f, style.saturation);
    plan.luminance_floor = std::clamp(style.luminance_floor, 0.0f, 1.0f);
    plan.luminance_gain = std::max(0.0f, style.luminance_gain);
    plan.edge_highlight = std::clamp(style.edge_highlight, 0.0f, 1.0f);
    plan.edge_width = std::max(0.5f, style.edge_width);
    plan.noise_opacity = resolved_quality.allow_noise
        ? std::clamp(style.noise_opacity, 0.0f, 0.05f)
        : 0.0f;
    plan.shadow_alpha = resolved_quality.allow_shadow
        ? std::clamp(style.shadow_alpha, 0.0f, 0.4f)
        : 0.0f;
    plan.shadow_radius = resolved_quality.allow_shadow
        ? std::max(0.0f, style.shadow_radius)
        : 0.0f;
    plan.contrast_intent = style.contrast_intent;
    plan.debug_seed = material_debug_seed(environment.debug_seed, style.kind);
    plan.sample_taps =
        material_resolve_sample_taps(resolved_quality.max_sample_taps);
    if (environment.capabilities.reduce_motion)
        plan.sample_taps = std::min(plan.sample_taps, 9u);
    plan.resource_budget.max_blur_radius = max_blur_radius;
    plan.resource_budget.max_sample_taps = plan.sample_taps;
    plan.resource_budget.max_sampling_kernel_radius = 0u;
    plan.resource_budget.max_pass_count = 1;
    plan.resource_budget.max_execution_stages = material_max_execution_stages;
    plan.resource_budget.max_paint_layers = material_max_paint_layers;
    plan.resource_budget.max_backdrop_pixels =
        resolved_quality.max_backdrop_pixels;
    plan.resource_budget.max_frame_capture_count = 0;
    plan.resource_budget.max_frame_capture_pixels = 0;
    plan.resource_budget.max_surface_sample_pixels = 0;
    plan.resource_budget.max_container_spacing = plan.container.spacing;
    plan.resource_budget.max_paint_layer_inflate = 0.0f;
    plan.resource_budget.bounded_texture_copy = true;
    plan.resource_budget.deterministic_fallback = true;
    plan.backdrop = analyze_material_backdrop(environment.backdrop);

    bool const has_geometry = plan.shape.valid;
    bool const has_material = style.kind != MaterialKind::None
        && style.tint.a > 0
        && plan.opacity > 0.0f
        && has_geometry;
    bool const role_allows_liquid_glass =
        material_role_allows_liquid_glass(style.role);
    bool const content_layer_standard_material =
        has_material && !role_allows_liquid_glass;
    bool const liquid_glass_backdrop_candidate =
        has_material && role_allows_liquid_glass;
    bool const target_ready = plan.render_target.ready;
    bool const backdrop_pixels_within_budget =
        plan.render_target.within_backdrop_budget
        && plan.capability_snapshot.target_within_texture_limits
        && plan.capability_snapshot.target_within_backdrop_budget;
    bool const quality_switches_allow_backdrop =
        resolved_quality.allow_backdrop_sampling
        && max_blur_radius > 0.0f
        && plan.sample_taps > 0u;
    bool const quality_allows_backdrop =
        quality_switches_allow_backdrop
        && backdrop_pixels_within_budget;
    bool const backend_supports_backdrop =
        environment.capabilities.material_surfaces
        && environment.capabilities.material_backdrop_blur
        && environment.capabilities.shader_blur;
    bool const backdrop_source_ready =
        environment.backdrop.available
        && environment.backdrop.stable;
    bool const can_sample_backdrop =
        liquid_glass_backdrop_candidate
        && target_ready
        && quality_allows_backdrop
        && backend_supports_backdrop
        && environment.capabilities.frame_history
        && backdrop_source_ready
        && !environment.capabilities.reduce_transparency;
    bool const next_frame_capture_required =
        liquid_glass_backdrop_candidate
        && target_ready
        && quality_allows_backdrop
        && backend_supports_backdrop
        && !environment.capabilities.reduce_transparency;
    plan.decision_trace.has_geometry = has_geometry;
    plan.decision_trace.has_material = has_material;
    plan.decision_trace.role_allows_liquid_glass =
        role_allows_liquid_glass;
    plan.decision_trace.content_layer_standard_material =
        content_layer_standard_material;
    plan.decision_trace.liquid_glass_backdrop_candidate =
        liquid_glass_backdrop_candidate;
    plan.decision_trace.target_ready = target_ready;
    plan.decision_trace.quality_switches_allow_backdrop =
        quality_switches_allow_backdrop;
    plan.decision_trace.backdrop_pixels_within_budget =
        backdrop_pixels_within_budget;
    plan.decision_trace.quality_allows_backdrop = quality_allows_backdrop;
    plan.decision_trace.capability_material_surfaces =
        environment.capabilities.material_surfaces;
    plan.decision_trace.capability_material_backdrop_blur =
        environment.capabilities.material_backdrop_blur;
    plan.decision_trace.capability_shader_blur =
        environment.capabilities.shader_blur;
    plan.decision_trace.capability_frame_history =
        environment.capabilities.frame_history;
    plan.decision_trace.capability_texture_limits_known =
        plan.capability_snapshot.texture_limits_known;
    plan.decision_trace.capability_backdrop_budget_known =
        plan.capability_snapshot.backdrop_budget_known;
    plan.decision_trace.capability_target_within_texture_limits =
        plan.capability_snapshot.target_within_texture_limits;
    plan.decision_trace.capability_target_within_backdrop_budget =
        plan.capability_snapshot.target_within_backdrop_budget;
    plan.decision_trace.backend_supports_backdrop = backend_supports_backdrop;
    plan.decision_trace.backdrop_available = environment.backdrop.available;
    plan.decision_trace.backdrop_stable = environment.backdrop.stable;
    plan.decision_trace.backdrop_source_ready = backdrop_source_ready;
    plan.decision_trace.next_frame_capture_required =
        next_frame_capture_required;
    plan.decision_trace.reduced_transparency =
        environment.capabilities.reduce_transparency;
    plan.decision_trace.increase_contrast =
        environment.capabilities.increase_contrast;
    plan.decision_trace.reduce_motion =
        environment.capabilities.reduce_motion;
    plan.decision_trace.can_sample_backdrop = can_sample_backdrop;

    if (!has_material) {
        plan.fallback_path = style.kind == MaterialKind::None
            ? MaterialFallbackPath::NoMaterial
            : MaterialFallbackPath::InvalidGeometry;
        plan.fallback_reason = style.kind == MaterialKind::None
            ? "material kind is none"
            : "material geometry or opacity is invalid";
        plan.blur_radius = 0.0f;
        plan.opacity = 0.0f;
        plan.tint.a = 0;
    } else if (liquid_glass_backdrop_candidate
               && environment.capabilities.reduce_transparency) {
        plan.fallback_path = MaterialFallbackPath::ReducedTransparency;
        plan.fallback_reason = "reduced transparency disables backdrop sampling";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    } else if (liquid_glass_backdrop_candidate
               && !quality_switches_allow_backdrop) {
        plan.fallback_path = MaterialFallbackPath::QualityPolicy;
        plan.fallback_reason = "quality policy disables material backdrop sampling";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    } else if (liquid_glass_backdrop_candidate
               && (!environment.capabilities.material_surfaces
                   || !environment.capabilities.material_backdrop_blur
                   || !environment.capabilities.shader_blur)) {
        plan.fallback_path = MaterialFallbackPath::UnsupportedBackend;
        plan.fallback_reason = "backend reports no material backdrop blur support";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    } else if (liquid_glass_backdrop_candidate
               && !backdrop_pixels_within_budget) {
        plan.fallback_path = MaterialFallbackPath::QualityPolicy;
        plan.fallback_reason = "quality policy backdrop pixel budget exceeded";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    } else if (liquid_glass_backdrop_candidate
               && (!target_ready || !environment.capabilities.frame_history
                   || !environment.backdrop.available
                   || !environment.backdrop.stable)) {
        plan.fallback_path = MaterialFallbackPath::NoBackdropSource;
        plan.fallback_reason = "stable backdrop source is not available for this frame";
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    }
    if (content_layer_standard_material) {
        plan.blur_radius = 0.0f;
        plan.saturation = 1.0f;
        plan.noise_opacity = 0.0f;
    }
    if (has_material)
        apply_material_transition_policy(plan);
    apply_material_reduced_transparency_policy(plan);
    plan.decision_trace.first_blocker =
        material_fallback_path_name(plan.fallback_path);

    plan.backdrop_sampling = can_sample_backdrop;
    if (!plan.backdrop_sampling) {
        plan.sample_taps = 0u;
    } else {
        apply_backdrop_luminance_policy(plan);
        apply_backdrop_color_policy(plan);
        plan.large_surface_legibility =
            apply_material_large_surface_legibility_policy(plan);
    }
    plan.sampling_kernel = material_resolve_sampling_kernel(
        plan.backdrop_sampling,
        plan.sample_taps);
    plan.resource_budget.max_sampling_kernel_radius =
        plan.sampling_kernel.radius;
    plan.backdrop_access = material_resolve_backdrop_access(plan);
    plan.resource_budget.max_frame_capture_count =
        plan.backdrop_access.max_frame_capture_count;
    plan.resource_budget.max_frame_capture_pixels =
        plan.backdrop_access.max_frame_capture_pixels;
    plan.resource_budget.max_surface_sample_pixels =
        plan.backdrop_access.max_surface_sample_pixels;
    if (environment.capabilities.reduce_motion) {
        plan.noise_opacity = 0.0f;
    }
    apply_material_increased_contrast_policy(plan);
    apply_material_interaction_policy(plan);
    plan.prominent_glass = material_resolve_prominent_glass_profile(plan);
    plan.refraction = material_resolve_refraction_profile(plan);
    plan.resource_budget.max_refraction_offset_pixels =
        plan.refraction.max_offset_pixels;
    plan.glass_thickness = material_resolve_glass_thickness_profile(plan);
    plan.scroll_edge = material_resolve_scroll_edge_profile(plan);
    apply_material_scroll_edge_legibility_policy(plan);
    plan.edge_optics = material_resolve_edge_optics_profile(plan);
    plan.spectral_tint = material_resolve_spectral_tint_profile(plan);
    plan.dynamic_lighting = material_resolve_dynamic_lighting_profile(plan);
    plan.glass_dispersion = material_resolve_glass_dispersion_profile(plan);
    plan.clear_glass_legibility =
        material_resolve_clear_glass_legibility_profile(plan);
    plan.glass_stabilization =
        material_resolve_glass_stabilization_profile(plan);
    plan.glass_environment =
        material_resolve_glass_environment_profile(plan);
    plan.glass_depth =
        material_resolve_glass_depth_profile(plan);
    plan.glass_caustic_flow =
        material_resolve_glass_caustic_flow_profile(plan);
    plan.glass_meniscus =
        material_resolve_glass_meniscus_profile(plan);
    plan.glass_transmission =
        material_resolve_glass_transmission_profile(plan);
    plan.glass_surface_cohesion =
        material_resolve_glass_surface_cohesion_profile(plan);
    plan.glass_substrate_adhesion =
        material_resolve_glass_substrate_adhesion_profile(plan);
    plan.glass_ambient_reflection =
        material_resolve_glass_ambient_reflection_profile(plan);
    plan.specular = material_resolve_specular_profile(plan);
    plan.luminance_curve = material_resolve_luminance_curve(
        plan.backdrop_sampling,
        plan.backdrop,
        plan.luminance_floor,
        plan.luminance_gain,
        plan.edge_highlight);
    plan.foreground = material_resolve_foreground(
        plan,
        style,
        environment.capabilities);
    plan.reference_model = material_resolve_reference_model(plan);

    plan.plan_id = material_plan_id(
        style.kind,
        style.role,
        plan.backdrop_sampling);
    char const* const non_backdrop_material_layer =
        material_plan_uses_standard_content_layer(plan)
            ? "material-standard-pass"
            : "material-fallback-pass";
    if (has_material && plan.backdrop_sampling && !plan.fallback()) {
        plan.primary_pass = MaterialPassExpectation{
            "backdrop-sample-blur",
            true,
            true,
            plan.sample_taps,
            "material-blur-pass",
            "backdrop-filter",
            plan.render_target.pixel_count,
        };
    } else if (content_layer_standard_material && !plan.fallback()) {
        plan.primary_pass = MaterialPassExpectation{
            "standard-material-fill",
            true,
            false,
            0u,
            "material-standard-pass",
            "standard-fill",
            0,
        };
    } else if (has_material && plan.fallback_path != MaterialFallbackPath::InvalidGeometry) {
        plan.primary_pass = MaterialPassExpectation{
            "translucent-rounded-rect",
            true,
            false,
            0u,
            "material-fallback-pass",
            "fallback-fill",
            0,
        };
    } else {
        plan.primary_pass = MaterialPassExpectation{
            "none",
            false,
            false,
            0u,
            "material-fallback-pass",
            "none",
            0,
        };
    }
    plan.optical_composition = material_resolve_optical_composition(plan);
    if (has_material && plan.shadow_alpha > 0.0f) {
        append_material_execution_stage(
            plan,
            MaterialExecutionStage{
                "shape-shadow",
                true,
                false,
                0u,
                "material-shadow-pass",
                "shape-shadow",
                0,
                true,
                material_shadow_stage_optics(plan),
            });
    }
    if (plan.primary_pass.active) {
        append_material_execution_stage(
            plan,
            MaterialExecutionStage{
                plan.primary_pass.name,
                true,
                plan.primary_pass.requires_backdrop,
                plan.primary_pass.sample_taps,
                plan.primary_pass.likely_layer,
                plan.primary_pass.executor,
                plan.primary_pass.max_texture_copy_pixels,
                true,
                material_primary_stage_optics(plan),
            });
    }
    if (has_material && plan.edge_highlight > 0.0f && plan.primary_pass.active) {
        append_material_execution_stage(
            plan,
            MaterialExecutionStage{
                "edge-highlight",
                true,
                false,
                0u,
                plan.backdrop_sampling ? "material-edge-pass"
                                       : non_backdrop_material_layer,
                "edge-highlight",
                0,
                true,
                material_edge_stage_optics(plan),
            });
    }
    if (has_material && plan.noise_opacity > 0.0f && plan.backdrop_sampling) {
        append_material_execution_stage(
            plan,
            MaterialExecutionStage{
                "noise-dither",
                true,
                false,
                0u,
                "material-noise-pass",
                "ordered-dither",
                0,
                true,
                material_noise_stage_optics(plan),
            });
    }
    resolve_material_paint_layers(plan);
    plan.verifier.require_backdrop_source = plan.backdrop_sampling;
    plan.verifier.require_edge_highlight = plan.edge_highlight > 0.0f
        && !plan.fallback();
    plan.verifier.require_container_identity = plan.container.participates;
    plan.verifier.require_container_morph_contract =
        plan.container.morph_transitions;
    plan.verifier.min_luma_delta = plan.backdrop_sampling ? 8.0f : 3.0f;
    plan.verifier.min_unique_colors = plan.backdrop_sampling ? 4 : 2;
    plan.verifier.region_name = style.verifier_profile;
    plan.verifier.likely_layer = plan.backdrop_sampling
        ? "material-blur-pass"
        : non_backdrop_material_layer;
    plan.verifier.likely_pass = plan.primary_pass.name;
    plan.optical_response = material_resolve_optical_response(plan);
    plan.observation_contract = material_observation_contract(plan);
    plan.execution_audit = material_execution_audit(plan);
    return plan;
}

} // namespace phenotype

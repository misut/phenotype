#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

import phenotype.material;
import phenotype.types;

using namespace phenotype;

namespace {

MaterialEnvironment sampled_environment() {
    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.backdrop.excludes_foreground_text = true;
    env.backdrop.source = "previous-presented-frame";
    env.backdrop.color_mean = Color{255, 255, 255, 255};
    env.backdrop.color_sample_count = 25;
    env.backdrop.color_sample_status = "sampled-async-grid";
    env.backdrop.luma_min = 0.2f;
    env.backdrop.luma_max = 0.8f;
    env.backdrop.luma_mean = 0.5f;
    env.backdrop.luma_sample_count = 25;
    env.backdrop.luma_sample_grid_width = 5;
    env.backdrop.luma_sample_grid_height = 5;
    env.backdrop.luma_sample_frame = 7;
    env.backdrop.luma_sample_status = "sampled-async-grid";
    env.render_target.width = 320;
    env.render_target.height = 240;
    env.render_target.scale = 1.0f;
    env.render_target.pixel_format = "bgra8unorm";
    env.quality = default_material_quality_policy();
    return env;
}

MaterialRequest regular_request() {
    return MaterialRequest{
        material_style_for_kind(MaterialKind::Regular, Theme{}),
        MaterialGeometry{12.0f, 20.0f, 240.0f, 96.0f, 10.0f},
    };
}

MaterialRequest clear_request() {
    return MaterialRequest{
        material_style_for_kind(MaterialKind::Clear, Theme{}),
        MaterialGeometry{12.0f, 20.0f, 240.0f, 96.0f, 10.0f},
    };
}

MaterialRequest large_surface_request() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{8.0f, 12.0f, 520.0f, 260.0f, 28.0f};
    return request;
}

MaterialRequest toolbar_request() {
    auto request = regular_request();
    request.style.role = MaterialSurfaceRole::Toolbar;
    request.geometry = MaterialGeometry{0.0f, 0.0f, 320.0f, 64.0f, 16.0f};
    return request;
}

MaterialRequest content_request() {
    auto style = material_style_for_kind(MaterialKind::Regular, Theme{});
    style.role = MaterialSurfaceRole::Content;
    return MaterialRequest{
        style,
        MaterialGeometry{12.0f, 20.0f, 240.0f, 96.0f, 10.0f},
    };
}

void test_sampled_backdrop_access_contract() {
    auto plan = plan_material_surface(regular_request(), sampled_environment());

    assert(plan.contract_version == material_plan_contract_version);
    assert(material_plan_contract_version == 75);
    assert(plan.capability_snapshot.material_surfaces);
    assert(plan.capability_snapshot.material_backdrop_blur);
    assert(plan.capability_snapshot.shader_blur);
    assert(plan.capability_snapshot.frame_history);
    assert(plan.capability_snapshot.max_shader_sample_taps
           == material_max_sample_taps);
    assert(plan.capability_snapshot.target_within_texture_limits);
    assert(plan.capability_snapshot.target_within_backdrop_budget);
    assert(plan.decision_trace.capability_target_within_texture_limits);
    assert(plan.decision_trace.capability_target_within_backdrop_budget);
    assert(plan.execution_audit.contract_satisfied);
    assert(plan.execution_audit.mismatch_count == 0u);
    assert(std::string_view(plan.execution_audit.first_mismatch) == "none");
    assert(plan.execution_audit.actual_runtime_passes
           == plan.observation_contract.expected_runtime_passes);
    assert(plan.execution_audit.actual_execution_stages
           == plan.observation_contract.expected_execution_stages);
    assert(std::string_view(plan.observation_contract.expected_stage_order)
           == std::string_view(plan.optical_composition.stage_order));
    assert(plan.execution_audit.stage_order_match);
    assert(std::string_view(plan.execution_audit.expected_stage_order)
           == "shadow-primary-edge-noise");
    assert(std::string_view(plan.execution_audit.actual_stage_order)
           == "shadow-primary-edge-noise");
    assert(std::string_view(material_execution_stage_order_name(plan))
           == "shadow-primary-edge-noise");
    assert(std::string_view(plan.interaction.enablement_reason)
        == "noninteractive-container");
    assert(!plan.transition.active);
    assert(!plan.transition.materialize);
    assert(std::string_view(plan.transition.kind_name) == "identity");
    assert(std::string_view(plan.transition.policy) == "identity");
    assert(plan.transition.progress == 1.0f);
    assert(plan.glass_identity.namespace_id == 0u);
    assert(plan.glass_identity.effect_id == 0u);
    assert(!plan.glass_identity.participates);
    assert(!plan.glass_identity.container_scoped);
    assert(!plan.glass_identity.matched_geometry_candidate);
    assert(std::string_view(plan.glass_identity.source) == "none");
    assert(std::string_view(plan.glass_identity.policy) == "unidentified");
    assert(!plan.reference_model.glass_effect_identified);
    assert(!plan.reference_model.glass_effect_matched_geometry);
    assert(!plan.prominent_glass.active);
    assert(!plan.clear_glass_legibility.active);
    assert(plan.shape.kind == MaterialShapeKind::RoundedRectangle);
    assert(!plan.shape.capsule);
    assert(plan.theme.default_glass_tokens);
    assert(std::string(plan.theme.profile_name) == "apple-glass-light");
    assert(std::string(plan.theme.source) == "material-style");
    assert(plan.theme.tint_matches_surface);
    assert(plan.theme.border_matches_theme);
    assert(plan.backdrop_sampling);
    assert(!plan.fallback());
    assert(plan.backdrop_access.required);
    assert(plan.backdrop_access.stable_required);
    assert(plan.backdrop_access.frame_history_required);
    assert(plan.backdrop_access.shared_frame_capture);
    assert(plan.backdrop_access.next_frame_capture_required);
    assert(plan.backdrop_access.excludes_foreground_text);
    assert(std::string_view(plan.backdrop_access.source)
        == "previous-presented-frame");
    assert(std::string_view(plan.backdrop_access.capture_scope)
        == "shared-frame");
    assert(std::string_view(plan.backdrop_access.capture_reason)
        == "sample-current-frame");
    assert(plan.backdrop_access.max_frame_capture_count == 1);
    assert(plan.backdrop_access.max_frame_capture_pixels == 320 * 240);
    assert(plan.backdrop_access.max_surface_sample_pixels == 240 * 96);
    assert(plan.resource_budget.max_frame_capture_count == 1);
    assert(plan.resource_budget.max_frame_capture_pixels == 320 * 240);
    assert(plan.resource_budget.max_surface_sample_pixels == 240 * 96);
    assert(plan.refraction.active);
    assert(plan.refraction.backdrop_driven);
    assert(!plan.refraction.interaction_driven);
    assert(!plan.refraction.reduced_motion_suppressed);
    assert(plan.refraction.bounded);
    assert(std::string_view(plan.refraction.model) == "edge-lens");
    assert(std::string_view(plan.refraction.source)
        == "sampled-backdrop-edge-refraction");
    assert(plan.refraction.strength > 0.0f);
    assert(plan.refraction.edge_bias > 0.0f);
    assert(plan.refraction.max_offset_pixels > 0.0f);
    assert(plan.refraction.edge_caustic_intensity > 0.0f);
    assert(plan.resource_budget.max_refraction_offset_pixels
           == plan.refraction.max_offset_pixels);
    assert(plan.edge_optics.active);
    assert(plan.edge_optics.backdrop_driven);
    assert(plan.edge_optics.caustic_driven);
    assert(plan.edge_optics.bounded);
    assert(std::string_view(plan.edge_optics.model)
        == "beveled-liquid-glass-edge");
    assert(std::string_view(plan.edge_optics.source)
        == "sampled-backdrop-bevel-caustics");
    assert(plan.edge_optics.bevel_width > plan.edge_width);
    assert(plan.edge_optics.inner_highlight > 0.0f);
    assert(plan.edge_optics.outer_shadow > 0.0f);
    assert(plan.edge_optics.chromatic_fringe > 0.0f);
    assert(plan.spectral_tint.active);
    assert(plan.spectral_tint.backdrop_driven);
    assert(!plan.spectral_tint.color_driven);
    assert(!plan.spectral_tint.tint_driven);
    assert(plan.spectral_tint.caustic_driven);
    assert(plan.spectral_tint.bounded);
    assert(std::string_view(plan.spectral_tint.model)
        == "spectral-liquid-glass-tint");
    assert(std::string_view(plan.spectral_tint.source)
        == "sampled-backdrop-spectral-rim");
    assert(plan.spectral_tint.warmth > 0.0f);
    assert(plan.spectral_tint.coolness > 0.0f);
    assert(plan.spectral_tint.dispersion > 0.0f);
    assert(plan.spectral_tint.rim_tint > 0.0f);
    assert(plan.dynamic_lighting.active);
    assert(plan.dynamic_lighting.backdrop_driven);
    assert(!plan.dynamic_lighting.color_driven);
    assert(plan.dynamic_lighting.caustic_driven);
    assert(!plan.dynamic_lighting.interaction_driven);
    assert(!plan.dynamic_lighting.reduced_motion_suppressed);
    assert(plan.dynamic_lighting.bounded);
    assert(std::string_view(plan.dynamic_lighting.model)
        == "dynamic-liquid-glass-light");
    assert(std::string_view(plan.dynamic_lighting.source)
        == "sampled-backdrop-caustic-lighting");
    assert(plan.dynamic_lighting.direction_x < 0.0f);
    assert(plan.dynamic_lighting.direction_y < 0.0f);
    assert(plan.dynamic_lighting.highlight_strength > 0.0f);
    assert(plan.dynamic_lighting.shadow_strength > 0.0f);
    assert(plan.glass_thickness.active);
    assert(plan.glass_thickness.size_driven);
    assert(!plan.glass_thickness.transition_driven);
    assert(!plan.glass_thickness.interaction_driven);
    assert(!plan.glass_thickness.reduced_motion_suppressed);
    assert(plan.glass_thickness.bounded);
    assert(std::string_view(plan.glass_thickness.model)
        == "adaptive-glass-thickness");
    assert(std::string_view(plan.glass_thickness.source)
        == "size-adaptive-thickness-lensing");
    assert(plan.glass_thickness.thickness > 0.0f);
    assert(plan.glass_thickness.lensing_gain > 1.0f);
    assert(plan.glass_thickness.shadow_gain > 1.0f);
    assert(plan.glass_thickness.scattering_gain > 1.0f);
    assert(plan.glass_dispersion.active);
    assert(plan.glass_dispersion.spectral_driven);
    assert(plan.glass_dispersion.thickness_driven);
    assert(!plan.glass_dispersion.transition_driven);
    assert(plan.glass_dispersion.lighting_driven);
    assert(!plan.glass_dispersion.interaction_driven);
    assert(!plan.glass_dispersion.reduced_motion_suppressed);
    assert(plan.glass_dispersion.bounded);
    assert(std::string_view(plan.glass_dispersion.model)
        == "prismatic-glass-dispersion");
    assert(std::string_view(plan.glass_dispersion.source)
        == "directional-thickness-prismatic-dispersion");
    assert(plan.glass_dispersion.axial_offset_pixels > 0.0f);
    assert(plan.glass_dispersion.tangential_offset_pixels > 0.0f);
    assert(plan.glass_dispersion.prismatic_gain > 1.0f);
    assert(plan.glass_dispersion.caustic_spread > 0.0f);
    assert(!plan.scroll_edge.active);
    assert(std::string_view(plan.scroll_edge.model) == "none");
    assert(std::string_view(plan.scroll_edge.source) == "none");
    assert(plan.scroll_edge.fade_extent_pixels == 0.0f);
    assert(plan.scroll_edge.dissolve_strength == 0.0f);
    assert(plan.scroll_edge.dimming_strength == 0.0f);
    assert(plan.scroll_edge.hard_style_strength == 0.0f);
    assert(!plan.clear_glass_legibility.active);
    assert(std::string_view(plan.clear_glass_legibility.model) == "none");
    assert(std::string_view(plan.clear_glass_legibility.source) == "none");
    assert(plan.clear_glass_legibility.dimming_strength == 0.0f);
    assert(plan.clear_glass_legibility.contrast_lift == 0.0f);
    assert(plan.specular.active);
    assert(plan.specular.ambient);
    assert(!plan.specular.interaction_driven);
    assert(plan.specular.bounded);
    assert(std::string_view(plan.specular.model) == "ambient-glass-sheen");
    assert(std::string_view(plan.specular.source)
        == "sampled-backdrop-edge-lighting");
    assert(plan.specular.anchor_x > 0.0f);
    assert(plan.specular.anchor_x < 1.0f);
    assert(plan.specular.anchor_y > 0.0f);
    assert(plan.specular.anchor_y < 1.0f);
    assert(plan.specular.radius > 0.0f);
    assert(plan.specular.intensity > 0.0f);
    assert(!plan.interaction.specular_highlight_active);
    assert(plan.observation_contract.shared_frame_capture_required);
    assert(plan.observation_contract.next_frame_capture_required);
    assert(plan.observation_contract.backdrop_capture_excludes_foreground_text);
    assert(std::string_view(plan.observation_contract.backdrop_capture_scope)
        == "shared-frame");
    assert(std::string_view(plan.observation_contract.backdrop_capture_reason)
        == "sample-current-frame");
    assert(plan.observation_contract.max_frame_capture_count == 1);
    assert(plan.observation_contract.max_frame_capture_pixels == 320 * 240);
    assert(plan.observation_contract.max_surface_sample_pixels == 240 * 96);
    assert(plan.observation_contract.expected_active_runtime_passes == 1u);
    assert(plan.observation_contract.expected_backdrop_runtime_passes == 1u);
    assert(std::string_view(plan.optical_response.response_model)
        == "sampled-backdrop");
    assert(std::string_view(plan.optical_response.blur_strategy)
        == "backdrop-sample-blur");
    assert(std::string_view(plan.optical_response.color_strategy)
        == "adaptive-backdrop-color");
    assert(std::string_view(plan.optical_response.depth_strategy)
        == "layered-shadow-edge-noise");
    assert(plan.optical_composition.schema_version
           == material_plan_contract_version);
    assert(std::string_view(plan.optical_composition.model)
        == "sampled-backdrop");
    assert(std::string_view(plan.optical_composition.blur_source)
        == "backdrop-sample-blur");
    assert(std::string_view(plan.optical_composition.frosting_source)
        == "sampled-backdrop-frosting");
    assert(std::string_view(plan.optical_composition.tint_source)
        == "adaptive-backdrop-tint");
    assert(std::string_view(plan.optical_composition.luminance_source)
        == "adaptive-backdrop-luma");
    assert(std::string_view(plan.optical_composition.depth_source)
        == "layered-shadow-edge-noise");
    assert(std::string_view(plan.optical_composition.refraction_source)
        == "sampled-backdrop-edge-refraction");
    assert(std::string_view(plan.optical_composition.spectral_tint_source)
        == "sampled-backdrop-spectral-rim");
    assert(std::string_view(plan.optical_composition.dynamic_lighting_source)
        == "sampled-backdrop-caustic-lighting");
    assert(std::string_view(plan.optical_composition.glass_thickness_source)
        == "size-adaptive-thickness-lensing");
    assert(std::string_view(plan.optical_composition.glass_dispersion_source)
        == "directional-thickness-prismatic-dispersion");
    assert(std::string_view(plan.optical_composition.scroll_edge_source)
        == "none");
    assert(std::string_view(
        plan.optical_composition.clear_glass_legibility_source) == "none");
    assert(std::string_view(plan.optical_composition.fallback_source)
        == "none");
    assert(std::string_view(plan.optical_composition.stage_order)
        == "shadow-primary-edge-noise");
    assert(std::string_view(plan.optical_composition.backdrop_capture_policy)
        == "sample-current-frame");
    assert(std::string_view(plan.optical_composition.foreground_sampling_policy)
        == "foreground-excluded-from-sample");
    assert(plan.optical_composition.backdrop_sampled);
    assert(plan.optical_composition.blur_required);
    assert(plan.optical_composition.frosting_required);
    assert(plan.optical_composition.tint_required);
    assert(plan.optical_composition.saturation_required);
    assert(plan.optical_composition.luminance_required);
    assert(plan.optical_composition.edge_required);
    assert(plan.optical_composition.shadow_required);
    assert(plan.optical_composition.noise_required);
    assert(plan.optical_composition.refraction_required);
    assert(plan.optical_composition.spectral_tint_required);
    assert(plan.optical_composition.dynamic_lighting_required);
    assert(plan.optical_composition.glass_thickness_required);
    assert(plan.optical_composition.glass_dispersion_required);
    assert(!plan.optical_composition.scroll_edge_required);
    assert(!plan.optical_composition.clear_glass_legibility_required);
    assert(!plan.optical_composition.fallback_required);
    assert(plan.optical_composition.backdrop_capture_required);
    assert(plan.optical_composition.foreground_excluded_from_backdrop);
    assert(plan.optical_composition.stage_order_stable);
    assert(plan.optical_composition.bounded);
    assert(plan.optical_composition.deterministic);
    assert(plan.optical_composition.sample_taps == plan.sample_taps);
    assert(plan.optical_composition.max_texture_copy_pixels
           == plan.primary_pass.max_texture_copy_pixels);
    assert(plan.optical_composition.max_surface_sample_pixels
           == plan.backdrop_access.max_surface_sample_pixels);
    assert(plan.optical_composition.refraction_strength
           == plan.refraction.strength);
    assert(plan.optical_composition.refraction_edge_bias
           == plan.refraction.edge_bias);
    assert(plan.optical_composition.refraction_offset_pixels
           == plan.refraction.max_offset_pixels);
    assert(plan.optical_composition.refraction_edge_caustic_intensity
           == plan.refraction.edge_caustic_intensity);
    assert(plan.optical_composition.edge_bevel_width
           == plan.edge_optics.bevel_width);
    assert(plan.optical_composition.edge_inner_highlight
           == plan.edge_optics.inner_highlight);
    assert(plan.optical_composition.edge_outer_shadow
           == plan.edge_optics.outer_shadow);
    assert(plan.optical_composition.edge_chromatic_fringe
           == plan.edge_optics.chromatic_fringe);
    assert(plan.optical_composition.spectral_tint_warmth
           == plan.spectral_tint.warmth);
    assert(plan.optical_composition.spectral_tint_coolness
           == plan.spectral_tint.coolness);
    assert(plan.optical_composition.spectral_dispersion
           == plan.spectral_tint.dispersion);
    assert(plan.optical_composition.spectral_rim_tint
           == plan.spectral_tint.rim_tint);
    assert(plan.optical_composition.spectral_tint_balance
           == plan.spectral_tint.balance);
    assert(plan.optical_composition.spectral_tint_influence
           == plan.spectral_tint.tint_influence);
    assert(plan.optical_composition.dynamic_light_direction_x
           == plan.dynamic_lighting.direction_x);
    assert(plan.optical_composition.dynamic_light_direction_y
           == plan.dynamic_lighting.direction_y);
    assert(plan.optical_composition.dynamic_light_highlight
           == plan.dynamic_lighting.highlight_strength);
    assert(plan.optical_composition.dynamic_light_shadow
           == plan.dynamic_lighting.shadow_strength);
    assert(plan.optical_composition.glass_thickness
           == plan.glass_thickness.thickness);
    assert(plan.optical_composition.glass_lensing_gain
           == plan.glass_thickness.lensing_gain);
    assert(plan.optical_composition.glass_shadow_gain
           == plan.glass_thickness.shadow_gain);
    assert(plan.optical_composition.glass_scattering_gain
           == plan.glass_thickness.scattering_gain);
    assert(plan.optical_composition.glass_dispersion_axial_offset
           == plan.glass_dispersion.axial_offset_pixels);
    assert(plan.optical_composition.glass_dispersion_tangential_offset
           == plan.glass_dispersion.tangential_offset_pixels);
    assert(plan.optical_composition.glass_dispersion_prismatic_gain
           == plan.glass_dispersion.prismatic_gain);
    assert(plan.optical_composition.glass_dispersion_caustic_spread
           == plan.glass_dispersion.caustic_spread);
    assert(plan.optical_composition.clear_glass_dimming == 0.0f);
    assert(plan.optical_composition.clear_glass_contrast == 0.0f);
    assert(plan.optical_response.backdrop_driven);
    assert(plan.optical_response.blur_active);
    assert(plan.optical_response.frosting_active);
    assert(plan.optical_response.tint_active);
    assert(plan.optical_response.saturation_active);
    assert(plan.optical_response.luminance_preservation_active);
    assert(plan.optical_response.edge_highlight_active);
    assert(plan.optical_response.depth_shadow_active);
    assert(plan.optical_response.noise_dither_active);
    assert(plan.optical_response.refraction_active);
    assert(plan.optical_response.spectral_tint_active);
    assert(plan.optical_response.dynamic_lighting_active);
    assert(plan.optical_response.glass_thickness_active);
    assert(plan.optical_response.glass_dispersion_active);
    assert(!plan.optical_response.scroll_edge_active);
    assert(!plan.optical_response.clear_glass_legibility_active);
    assert(plan.optical_response.foreground_vibrancy_active);
    assert(plan.optical_response.deterministic_fallback);
    assert(material_plan_uses_sampled_backdrop_executor(plan));
    assert(!material_plan_uses_standard_fill_executor(plan));
    assert(!material_plan_uses_deterministic_fallback_executor(plan));
    assert(plan.execution_stage_count == 4u);
    assert(std::string_view(plan.execution_stages[2].name) == "edge-highlight");
    assert(std::string_view(plan.execution_stages[2].optics.specular_model)
        == "ambient-glass-sheen");
    assert(plan.execution_stages[2].optics.specular_anchor_x
           == plan.specular.anchor_x);
    assert(plan.execution_stages[2].optics.specular_anchor_y
           == plan.specular.anchor_y);
    assert(plan.execution_stages[2].optics.specular_radius
           == plan.specular.radius);
    assert(plan.execution_stages[2].optics.specular_intensity
           == plan.specular.intensity);
    assert(std::string_view(plan.execution_stages[2].optics.edge_optics_model)
        == "beveled-liquid-glass-edge");
    assert(plan.execution_stages[2].optics.edge_bevel_width
           == plan.edge_optics.bevel_width);
    assert(plan.execution_stages[2].optics.edge_inner_highlight
           == plan.edge_optics.inner_highlight);
    assert(plan.execution_stages[2].optics.edge_outer_shadow
           == plan.edge_optics.outer_shadow);
    assert(plan.execution_stages[2].optics.edge_chromatic_fringe
           == plan.edge_optics.chromatic_fringe);
    assert(std::string_view(plan.execution_stages[2].optics.spectral_tint_model)
        == "spectral-liquid-glass-tint");
    assert(plan.execution_stages[2].optics.spectral_tint_warmth
           == plan.spectral_tint.warmth);
    assert(plan.execution_stages[2].optics.spectral_tint_coolness
           == plan.spectral_tint.coolness);
    assert(plan.execution_stages[2].optics.spectral_dispersion
           == plan.spectral_tint.dispersion);
    assert(plan.execution_stages[2].optics.spectral_rim_tint
           == plan.spectral_tint.rim_tint);
    assert(std::string_view(
               plan.execution_stages[2].optics.dynamic_lighting_model)
        == "dynamic-liquid-glass-light");
    assert(plan.execution_stages[2].optics.dynamic_light_direction_x
           == plan.dynamic_lighting.direction_x);
    assert(plan.execution_stages[2].optics.dynamic_light_direction_y
           == plan.dynamic_lighting.direction_y);
    assert(plan.execution_stages[2].optics.dynamic_light_highlight
           == plan.dynamic_lighting.highlight_strength);
    assert(plan.execution_stages[2].optics.dynamic_light_shadow
           == plan.dynamic_lighting.shadow_strength);
    assert(std::string_view(plan.execution_stages[2].optics.glass_thickness_model)
        == "adaptive-glass-thickness");
    assert(plan.execution_stages[2].optics.glass_thickness
           == plan.glass_thickness.thickness);
    assert(plan.execution_stages[2].optics.glass_lensing_gain
           == plan.glass_thickness.lensing_gain);
    assert(plan.execution_stages[2].optics.glass_shadow_gain
           == plan.glass_thickness.shadow_gain);
    assert(plan.execution_stages[2].optics.glass_scattering_gain
           == plan.glass_thickness.scattering_gain);
    assert(std::string_view(
               plan.execution_stages[2].optics.glass_dispersion_model)
        == "prismatic-glass-dispersion");
    assert(plan.execution_stages[2].optics.glass_dispersion_axial_offset
           == plan.glass_dispersion.axial_offset_pixels);
    assert(plan.execution_stages[2].optics.glass_dispersion_tangential_offset
           == plan.glass_dispersion.tangential_offset_pixels);
    assert(plan.execution_stages[2].optics.glass_dispersion_prismatic_gain
           == plan.glass_dispersion.prismatic_gain);
    assert(plan.execution_stages[2].optics.glass_dispersion_caustic_spread
           == plan.glass_dispersion.caustic_spread);
    assert(plan.paint_layer_count == 0u);
    assert(plan.dropped_paint_layer_count == 0u);
    assert(plan.resource_budget.max_paint_layers == material_max_paint_layers);
    assert(plan.resource_budget.max_paint_layer_inflate == 0.0f);
    assert(plan.observation_contract.expected_paint_layers == 0u);
    assert(plan.observation_contract.expected_active_paint_layers == 0u);
    assert(plan.backdrop.luma_sample_count == 25u);
    assert(plan.backdrop.color_sample_count == 25u);
    assert(std::string_view(plan.backdrop.color_sample_status)
        == "sampled-async-grid");
    assert(std::string_view(plan.backdrop.color_response) == "preserve");
    assert(std::fabs(plan.backdrop.tint_color_delta) < 0.0001f);
    assert((plan.backdrop.color_mean == Color{255, 255, 255, 255}));
    assert(plan.backdrop.luma_sample_grid_width == 5u);
    assert(plan.backdrop.luma_sample_grid_height == 5u);
    assert(plan.backdrop.luma_sample_frame == 7u);
    assert(std::string_view(plan.backdrop.luma_sample_status)
        == "sampled-async-grid");
    std::puts("PASS: sampled backdrop access contract");
}

void test_backdrop_optical_response_contract() {
    auto env = sampled_environment();

    auto neutral_plan = plan_material_surface(regular_request(), env);
    assert(std::string_view(neutral_plan.backdrop.frosting_response)
        == "balanced");
    assert(std::string_view(neutral_plan.backdrop.tint_response)
        == "preserve");
    assert(std::string_view(neutral_plan.backdrop.saturation_response)
        == "preserve");
    assert(std::string_view(neutral_plan.backdrop.depth_response)
        == "standard");
    assert(std::fabs(neutral_plan.backdrop.opacity_delta) < 0.0001f);
    assert(std::fabs(neutral_plan.backdrop.tint_alpha_delta) < 0.0001f);
    assert(std::fabs(neutral_plan.backdrop.tint_color_delta) < 0.0001f);
    assert(std::string_view(neutral_plan.backdrop.color_response)
        == "preserve");
    assert(std::fabs(neutral_plan.backdrop.saturation_delta) < 0.0001f);
    assert(std::fabs(neutral_plan.backdrop.shadow_alpha_delta) < 0.0001f);
    assert(std::fabs(neutral_plan.backdrop.shadow_radius_delta) < 0.0001f);
    assert(std::fabs(neutral_plan.backdrop.response_strength) < 0.0001f);

    env.backdrop.luma_min = 0.02f;
    env.backdrop.luma_max = 0.28f;
    env.backdrop.luma_mean = 0.12f;
    auto dark_plan = plan_material_surface(regular_request(), env);
    assert(std::string_view(dark_plan.backdrop.frosting_response)
        == "dark-frost-lift");
    assert(std::string_view(dark_plan.backdrop.tint_response)
        == "lift-dark-backdrop");
    assert(std::string_view(dark_plan.backdrop.saturation_response)
        == "lift-dark-color");
    assert(std::string_view(dark_plan.backdrop.depth_response)
        == "soften-dark-depth");
    assert(dark_plan.backdrop.opacity_delta > 0.0f);
    assert(dark_plan.backdrop.tint_alpha_delta > 0.0f);
    assert(std::fabs(dark_plan.backdrop.tint_color_delta) < 0.0001f);
    assert(dark_plan.backdrop.saturation_delta > 0.0f);
    assert(dark_plan.backdrop.shadow_alpha_delta < 0.0f);
    assert(dark_plan.backdrop.shadow_radius_delta < 0.0f);
    assert(dark_plan.backdrop.response_strength > 0.0f);

    env.backdrop.luma_min = 0.84f;
    env.backdrop.luma_max = 0.97f;
    env.backdrop.luma_mean = 0.90f;
    auto bright_plan = plan_material_surface(regular_request(), env);
    assert(std::string_view(bright_plan.backdrop.frosting_response)
        == "bright-frost-thin");
    assert(std::string_view(bright_plan.backdrop.tint_response)
        == "thin-bright-backdrop");
    assert(std::string_view(bright_plan.backdrop.saturation_response)
        == "compress-bright-color");
    assert(std::string_view(bright_plan.backdrop.depth_response)
        == "restore-bright-depth");
    assert(bright_plan.backdrop.opacity_delta < 0.0f);
    assert(bright_plan.backdrop.tint_alpha_delta < 0.0f);
    assert(bright_plan.backdrop.saturation_delta < 0.0f);
    assert(bright_plan.backdrop.shadow_alpha_delta > 0.0f);
    assert(bright_plan.backdrop.shadow_radius_delta > 0.0f);

    env.backdrop.luma_min = 0.46f;
    env.backdrop.luma_max = 0.50f;
    env.backdrop.luma_mean = 0.48f;
    auto flat_plan = plan_material_surface(regular_request(), env);
    assert(std::string_view(flat_plan.backdrop.frosting_response)
        == "flat-edge-frost");
    assert(std::string_view(flat_plan.backdrop.tint_response)
        == "stabilize-flat-backdrop");
    assert(std::string_view(flat_plan.backdrop.saturation_response)
        == "restore-flat-color");
    assert(std::string_view(flat_plan.backdrop.depth_response)
        == "restore-flat-depth");
    assert(flat_plan.backdrop.opacity_delta > 0.0f);
    assert(flat_plan.backdrop.tint_alpha_delta > 0.0f);
    assert(flat_plan.backdrop.saturation_delta > 0.0f);
    assert(flat_plan.backdrop.shadow_alpha_delta > 0.0f);
    assert(flat_plan.backdrop.shadow_radius_delta > 0.0f);

    env.backdrop.luma_min = 0.25f;
    env.backdrop.luma_max = 0.75f;
    env.backdrop.luma_mean = 0.50f;
    env.backdrop.color_mean = Color{126, 191, 255, 255};
    auto color_plan = plan_material_surface(regular_request(), env);
    assert(std::string_view(color_plan.backdrop.color_response)
        == "sampled-backdrop-color");
    assert(std::string_view(color_plan.spectral_tint.source)
        == "sampled-backdrop-spectral-caustics");
    assert(color_plan.spectral_tint.color_driven);
    assert(color_plan.spectral_tint.coolness
           > color_plan.spectral_tint.warmth);
    assert(color_plan.optical_composition.spectral_tint_coolness
           == color_plan.spectral_tint.coolness);
    assert(color_plan.dynamic_lighting.color_driven);
    assert(std::string_view(color_plan.dynamic_lighting.source)
        == "sampled-backdrop-color-caustic-lighting");
    assert(color_plan.dynamic_lighting.direction_x
           > neutral_plan.dynamic_lighting.direction_x);
    assert(color_plan.optical_composition.dynamic_light_direction_x
           == color_plan.dynamic_lighting.direction_x);
    assert(color_plan.backdrop.tint_color_delta > 0.0f);
    assert(color_plan.tint.r < 255);
    assert(color_plan.tint.g < 255);
    assert(color_plan.tint.b == 255);
    assert(color_plan.tint.a == neutral_plan.tint.a);

    env.backdrop.color_mean = Color{255, 176, 118, 255};
    auto warm_plan = plan_material_surface(regular_request(), env);
    assert(warm_plan.spectral_tint.color_driven);
    assert(warm_plan.spectral_tint.warmth
           > warm_plan.spectral_tint.coolness);
    assert(warm_plan.spectral_tint.balance > 0.5f);
    assert(warm_plan.dynamic_lighting.color_driven);
    assert(warm_plan.dynamic_lighting.direction_x
           < neutral_plan.dynamic_lighting.direction_x);
    std::puts("PASS: backdrop optical response contract");
}

void test_toolbar_scroll_edge_separates_scrolled_content_contract() {
    auto env = sampled_environment();
    env.backdrop.luma_min = 0.04f;
    env.backdrop.luma_max = 0.82f;
    env.backdrop.luma_mean = 0.30f;
    env.backdrop.color_mean = Color{74, 86, 118, 255};

    auto plan = plan_material_surface(toolbar_request(), env);

    assert(plan.role == MaterialSurfaceRole::Toolbar);
    assert(plan.backdrop_sampling);
    assert(plan.scroll_edge.active);
    assert(plan.scroll_edge.role_driven);
    assert(plan.scroll_edge.backdrop_driven);
    assert(plan.scroll_edge.contrast_driven);
    assert(!plan.scroll_edge.hard_style);
    assert(plan.scroll_edge.bounded);
    assert(std::string_view(plan.scroll_edge.model)
        == "adaptive-glass-scroll-edge");
    assert(std::string_view(plan.scroll_edge.source)
        == "sampled-backdrop-contrast-scroll-edge");
    assert(plan.scroll_edge.fade_extent_pixels > plan.edge_width);
    assert(plan.scroll_edge.dissolve_strength > 0.0f);
    assert(plan.scroll_edge.dimming_strength > 0.0f);
    assert(plan.scroll_edge.hard_style_strength == 0.0f);
    assert(std::string_view(plan.optical_composition.scroll_edge_source)
        == "sampled-backdrop-contrast-scroll-edge");
    assert(plan.optical_composition.scroll_edge_required);
    assert(plan.optical_composition.scroll_edge_extent
           == plan.scroll_edge.fade_extent_pixels);
    assert(plan.optical_composition.scroll_edge_dissolve
           == plan.scroll_edge.dissolve_strength);
    assert(plan.optical_composition.scroll_edge_dimming
           == plan.scroll_edge.dimming_strength);
    assert(plan.optical_composition.scroll_edge_hard_style
           == plan.scroll_edge.hard_style_strength);
    assert(plan.optical_response.scroll_edge_active);
    assert(std::string_view(plan.execution_stages[1].optics.scroll_edge_model)
        == "adaptive-glass-scroll-edge");
    assert(plan.execution_stages[1].optics.scroll_edge_extent
           == plan.scroll_edge.fade_extent_pixels);
    assert(plan.execution_stages[1].optics.scroll_edge_dissolve
           == plan.scroll_edge.dissolve_strength);
    assert(plan.execution_stages[1].optics.scroll_edge_dimming
           == plan.scroll_edge.dimming_strength);

    env.backdrop.luma_min = 0.48f;
    env.backdrop.luma_max = 0.54f;
    env.backdrop.luma_mean = 0.51f;
    auto hard_plan = plan_material_surface(toolbar_request(), env);
    assert(hard_plan.scroll_edge.active);
    assert(hard_plan.scroll_edge.hard_style);
    assert(std::string_view(hard_plan.scroll_edge.source)
        == "sampled-backdrop-hard-scroll-edge");
    assert(hard_plan.scroll_edge.hard_style_strength > 0.0f);
    assert(hard_plan.optical_composition.scroll_edge_hard_style
           == hard_plan.scroll_edge.hard_style_strength);

    std::puts("PASS: toolbar scroll edge separates scrolled content contract");
}

void test_configured_tint_drives_glass_chromatics_contract() {
    auto env = sampled_environment();
    env.backdrop.color_mean = Color{242, 242, 247, 255};
    env.backdrop.luma_min = 0.42f;
    env.backdrop.luma_max = 0.62f;
    env.backdrop.luma_mean = 0.52f;

    auto neutral = regular_request();
    neutral.style.tint = Color{255, 255, 255, 96};
    auto neutral_plan = plan_material_surface(neutral, env);
    assert(!neutral_plan.spectral_tint.tint_driven);
    assert(neutral_plan.spectral_tint.tint_influence == 0.0f);

    auto cool = regular_request();
    cool.style.tint = Color{64, 156, 255, 128};
    auto cool_plan = plan_material_surface(cool, env);
    assert(cool_plan.backdrop_sampling);
    assert(cool_plan.spectral_tint.active);
    assert(cool_plan.spectral_tint.tint_driven);
    assert(!cool_plan.spectral_tint.color_driven);
    assert(cool_plan.spectral_tint.caustic_driven);
    assert(cool_plan.spectral_tint.tint_influence > 0.0f);
    assert(std::string_view(cool_plan.spectral_tint.source)
        == "configured-tint-spectral-caustics");
    assert(cool_plan.spectral_tint.coolness
           > neutral_plan.spectral_tint.coolness);
    assert(cool_plan.spectral_tint.coolness
           > cool_plan.spectral_tint.warmth);
    assert(cool_plan.spectral_tint.rim_tint
           > neutral_plan.spectral_tint.rim_tint);
    assert(cool_plan.spectral_tint.dispersion
           > neutral_plan.spectral_tint.dispersion);
    assert(std::string_view(cool_plan.optical_composition.spectral_tint_source)
           == cool_plan.spectral_tint.source);
    assert(cool_plan.optical_composition.spectral_tint_influence
           == cool_plan.spectral_tint.tint_influence);
    assert(cool_plan.execution_stages[2].optics.spectral_tint_coolness
           == cool_plan.spectral_tint.coolness);

    auto warm = regular_request();
    warm.style.tint = Color{255, 128, 48, 128};
    auto warm_plan = plan_material_surface(warm, env);
    assert(warm_plan.spectral_tint.tint_driven);
    assert(warm_plan.spectral_tint.warmth
           > warm_plan.spectral_tint.coolness);
    assert(warm_plan.spectral_tint.balance > 0.5f);

    std::puts("PASS: configured tint drives glass chromatics contract");
}

void test_glass_thickness_scales_lensing_contract() {
    auto baseline = plan_material_surface(regular_request(), sampled_environment());
    auto large = plan_material_surface(large_surface_request(), sampled_environment());

    assert(baseline.glass_thickness.active);
    assert(large.glass_thickness.active);
    assert(large.glass_thickness.size_driven);
    assert(std::string_view(large.glass_thickness.model)
        == "adaptive-glass-thickness");
    assert(std::string_view(large.glass_thickness.source)
        == "size-adaptive-thickness-lensing");
    assert(large.glass_thickness.thickness
           > baseline.glass_thickness.thickness);
    assert(large.glass_thickness.lensing_gain
           > baseline.glass_thickness.lensing_gain);
    assert(large.glass_thickness.shadow_gain
           > baseline.glass_thickness.shadow_gain);
    assert(large.glass_thickness.scattering_gain
           > baseline.glass_thickness.scattering_gain);
    assert(baseline.glass_dispersion.active);
    assert(large.glass_dispersion.active);
    assert(large.glass_dispersion.thickness_driven);
    assert(std::string_view(large.glass_dispersion.model)
        == "prismatic-glass-dispersion");
    assert(large.glass_dispersion.axial_offset_pixels
           > baseline.glass_dispersion.axial_offset_pixels);
    assert(large.glass_dispersion.tangential_offset_pixels
           > baseline.glass_dispersion.tangential_offset_pixels);
    assert(large.glass_dispersion.prismatic_gain
           > baseline.glass_dispersion.prismatic_gain);
    assert(large.glass_dispersion.caustic_spread
           > baseline.glass_dispersion.caustic_spread);
    assert(large.optical_composition.glass_thickness_required);
    assert(large.optical_composition.glass_lensing_gain
           == large.glass_thickness.lensing_gain);
    assert(large.optical_composition.glass_dispersion_required);
    assert(large.optical_composition.glass_dispersion_prismatic_gain
           == large.glass_dispersion.prismatic_gain);
    assert(large.optical_response.glass_thickness_active);
    assert(large.optical_response.glass_dispersion_active);

    auto transition_request = large_surface_request();
    transition_request.style.transition = MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::Materialize,
        .progress = 0.5f,
        .appearing = true,
    };
    auto transition =
        plan_material_surface(transition_request, sampled_environment());
    assert(transition.transition.active);
    assert(transition.glass_thickness.active);
    assert(transition.glass_thickness.transition_driven);
    assert(std::string_view(transition.glass_thickness.source)
        == "transition-thickness-lensing");
    assert(transition.glass_thickness.thickness
           > large.glass_thickness.thickness);
    assert(transition.optical_composition.glass_shadow_gain
           == transition.glass_thickness.shadow_gain);
    assert(transition.glass_dispersion.active);
    assert(transition.glass_dispersion.transition_driven);
    assert(std::string_view(transition.glass_dispersion.source)
        == "transition-directional-prismatic-dispersion");
    assert(transition.glass_dispersion.axial_offset_pixels
           >= large.glass_dispersion.axial_offset_pixels);
    assert(transition.glass_dispersion.prismatic_gain
           >= large.glass_dispersion.prismatic_gain);

    std::puts("PASS: glass thickness scales lensing contract");
}

void test_content_layer_stays_standard_material_contract() {
    auto plan = plan_material_surface(content_request(), sampled_environment());

    assert(!plan.fallback());
    assert(plan.role == MaterialSurfaceRole::Content);
    assert(plan.command_descriptor.role == MaterialSurfaceRole::Content);
    assert(!plan.decision_trace.role_allows_liquid_glass);
    assert(plan.decision_trace.content_layer_standard_material);
    assert(!plan.decision_trace.liquid_glass_backdrop_candidate);
    assert(plan.decision_trace.backend_supports_backdrop);
    assert(!plan.decision_trace.can_sample_backdrop);
    assert(!plan.decision_trace.next_frame_capture_required);
    assert(!plan.backdrop_sampling);
    assert(plan.blur_radius == 0.0f);
    assert(plan.saturation == 1.0f);
    assert(plan.noise_opacity == 0.0f);
    assert(plan.sample_taps == 0u);
    assert(std::string_view(plan.plan_id)
        == "material.regular.standard-material");
    assert(std::string_view(plan.reference_model.technology)
        == "standard-material");
    assert(std::string_view(plan.reference_model.layer)
        == "content-layer");
    assert(std::string_view(plan.reference_model.material_policy)
        == "standard-material-content-layer");
    assert(std::string_view(plan.reference_model.blending_scope)
        == "standard-fill");
    assert(!plan.backdrop_access.required);
    assert(!plan.backdrop_access.shared_frame_capture);
    assert(!plan.backdrop_access.next_frame_capture_required);
    assert(std::string_view(plan.backdrop_access.capture_scope) == "none");
    assert(std::string_view(plan.backdrop_access.capture_reason)
        == "not-required");
    assert(std::string_view(plan.sampling_kernel.name) == "none");
    assert(plan.sampling_kernel.sample_taps == 0u);
    assert(!plan.sampling_kernel.requires_backdrop);
    assert(std::string_view(plan.primary_pass.name)
        == "standard-material-fill");
    assert(plan.primary_pass.active);
    assert(!plan.primary_pass.requires_backdrop);
    assert(plan.primary_pass.sample_taps == 0u);
    assert(std::string_view(plan.primary_pass.likely_layer)
        == "material-standard-pass");
    assert(std::string_view(plan.primary_pass.executor)
        == "standard-fill");
    assert(plan.observation_contract.runtime_plan_required);
    assert(!plan.observation_contract.fallback_expected);
    assert(!plan.observation_contract.backdrop_sampling_expected);
    assert(!plan.observation_contract.shared_frame_capture_required);
    assert(!plan.observation_contract.next_frame_capture_required);
    assert(plan.observation_contract.expected_backdrop_execution_stages == 0u);
    assert(plan.observation_contract.expected_active_runtime_passes == 1u);
    assert(plan.observation_contract.expected_backdrop_runtime_passes == 0u);
    assert(std::string_view(plan.observation_contract.likely_layer)
        == "material-standard-pass");
    assert(std::string_view(plan.observation_contract.likely_pass)
        == "standard-material-fill");
    assert(std::string_view(plan.optical_response.response_model)
        == "standard-content");
    assert(std::string_view(plan.optical_response.blur_strategy)
        == "standard-fill");
    assert(std::string_view(plan.optical_response.color_strategy)
        == "standard-content-color");
    assert(std::string_view(plan.optical_response.depth_strategy)
        == "standard-content-edge");
    assert(std::string_view(plan.optical_composition.stage_order)
        == "shadow-primary-edge");
    assert(std::string_view(plan.optical_composition.backdrop_capture_policy)
        == "no-capture");
    assert(std::string_view(plan.optical_composition.foreground_sampling_policy)
        == "not-applicable");
    assert(!plan.optical_composition.backdrop_capture_required);
    assert(!plan.optical_composition.foreground_excluded_from_backdrop);
    assert(plan.optical_composition.stage_order_stable);
    assert(!plan.refraction.active);
    assert(plan.refraction.bounded);
    assert(std::string_view(plan.refraction.model) == "none");
    assert(std::string_view(plan.refraction.source) == "none");
    assert(plan.refraction.edge_caustic_intensity == 0.0f);
    assert(plan.resource_budget.max_refraction_offset_pixels == 0.0f);
    assert(!plan.specular.active);
    assert(std::string_view(plan.specular.model) == "none");
    assert(std::string_view(plan.specular.source) == "none");
    assert(plan.specular.intensity == 0.0f);
    assert(!plan.spectral_tint.active);
    assert(std::string_view(plan.spectral_tint.model) == "none");
    assert(std::string_view(plan.spectral_tint.source) == "none");
    assert(plan.spectral_tint.dispersion == 0.0f);
    assert(!plan.dynamic_lighting.active);
    assert(std::string_view(plan.dynamic_lighting.model) == "none");
    assert(std::string_view(plan.dynamic_lighting.source) == "none");
    assert(plan.dynamic_lighting.highlight_strength == 0.0f);
    assert(plan.dynamic_lighting.shadow_strength == 0.0f);
    assert(!plan.glass_thickness.active);
    assert(std::string_view(plan.glass_thickness.model) == "none");
    assert(std::string_view(plan.glass_thickness.source) == "none");
    assert(plan.glass_thickness.thickness == 0.0f);
    assert(plan.glass_thickness.lensing_gain == 1.0f);
    assert(!plan.glass_dispersion.active);
    assert(std::string_view(plan.glass_dispersion.model) == "none");
    assert(std::string_view(plan.glass_dispersion.source) == "none");
    assert(plan.glass_dispersion.axial_offset_pixels == 0.0f);
    assert(plan.glass_dispersion.prismatic_gain == 1.0f);
    assert(!plan.optical_response.backdrop_driven);
    assert(!plan.optical_response.blur_active);
    assert(!plan.optical_response.frosting_active);
    assert(plan.optical_response.tint_active);
    assert(!plan.optical_response.saturation_active);
    assert(plan.optical_response.luminance_preservation_active);
    assert(plan.optical_response.edge_highlight_active);
    assert(plan.optical_response.depth_shadow_active);
    assert(!plan.optical_response.noise_dither_active);
    assert(!plan.optical_response.refraction_active);
    assert(!plan.optical_response.spectral_tint_active);
    assert(!plan.optical_response.dynamic_lighting_active);
    assert(!plan.optical_response.glass_thickness_active);
    assert(!plan.optical_response.glass_dispersion_active);
    assert(!plan.optical_response.foreground_vibrancy_active);
    assert(plan.optical_response.deterministic_fallback);
    assert(!material_plan_uses_sampled_backdrop_executor(plan));
    assert(material_plan_uses_standard_fill_executor(plan));
    assert(!material_plan_uses_deterministic_fallback_executor(plan));
    assert(plan.paint_layer_count == 3u);
    assert(plan.dropped_paint_layer_count == 0u);
    assert(std::string_view(plan.paint_layers[0].name) == "fallback-shadow");
    assert(std::string_view(plan.paint_layers[0].executor) == "rounded-shadow");
    assert(plan.paint_layers[0].inflate > 0.0f);
    assert(std::string_view(plan.paint_layers[1].name) == "standard-material-fill");
    assert(std::string_view(plan.paint_layers[1].executor) == "rounded-fill");
    assert(plan.paint_layers[1].color.a == plan.tint.a);
    assert(std::fabs(plan.paint_layers[1].opacity - plan.opacity) < 0.0001f);
    assert(std::string_view(plan.paint_layers[2].name) == "fallback-edge-highlight");
    assert(std::string_view(plan.paint_layers[2].executor) == "rounded-edge");
    assert(std::fabs(plan.paint_layers[2].stroke_width - plan.edge_width) < 0.0001f);
    assert(plan.observation_contract.expected_paint_layers == 3u);
    assert(plan.observation_contract.expected_shadow_paint_layers == 1u);
    assert(plan.observation_contract.expected_fill_paint_layers == 1u);
    assert(plan.observation_contract.expected_edge_paint_layers == 1u);
    std::puts("PASS: content layer stays standard material contract");
}

void test_fallback_backdrop_access_contract() {
    auto env = sampled_environment();
    env.capabilities.material_backdrop_blur = false;
    env.capabilities.shader_blur = false;
    env.capabilities.frame_history = false;
    env.backdrop.available = false;
    env.backdrop.stable = false;
    env.backdrop.source = "none";

    auto plan = plan_material_surface(regular_request(), env);

    assert(!plan.backdrop_sampling);
    assert(plan.fallback());
    assert(plan.fallback_path == MaterialFallbackPath::UnsupportedBackend);
    assert(!plan.backdrop_access.required);
    assert(!plan.backdrop_access.shared_frame_capture);
    assert(!plan.backdrop_access.next_frame_capture_required);
    assert(!plan.backdrop_access.excludes_foreground_text);
    assert(std::string_view(plan.backdrop_access.capture_scope) == "none");
    assert(std::string_view(plan.backdrop_access.capture_reason)
        == "not-required");
    assert(plan.backdrop_access.max_frame_capture_count == 0);
    assert(plan.backdrop_access.max_frame_capture_pixels == 0);
    assert(plan.backdrop_access.max_surface_sample_pixels == 0);
    assert(plan.resource_budget.max_frame_capture_count == 0);
    assert(plan.resource_budget.max_frame_capture_pixels == 0);
    assert(plan.resource_budget.max_surface_sample_pixels == 0);
    assert(!plan.observation_contract.shared_frame_capture_required);
    assert(!plan.observation_contract.next_frame_capture_required);
    assert(plan.observation_contract.expected_active_runtime_passes == 1u);
    assert(plan.observation_contract.expected_backdrop_runtime_passes == 0u);
    assert(std::string_view(plan.observation_contract.backdrop_capture_scope)
        == "none");
    assert(std::string_view(plan.observation_contract.backdrop_capture_reason)
        == "not-required");
    assert(std::string_view(plan.optical_response.response_model)
        == "deterministic-fallback");
    assert(std::string_view(plan.optical_response.blur_strategy)
        == "fallback-fill");
    assert(std::string_view(plan.optical_response.color_strategy)
        == "fallback-solid-color");
    assert(std::string_view(plan.optical_response.depth_strategy)
        == "fallback-shadow-edge");
    assert(std::string_view(plan.optical_composition.model)
        == "deterministic-fallback");
    assert(std::string_view(plan.optical_composition.blur_source)
        == "fallback-fill");
    assert(std::string_view(plan.optical_composition.frosting_source)
        == "solid-fallback-frosting");
    assert(std::string_view(plan.optical_composition.tint_source)
        == "style-tint");
    assert(std::string_view(plan.optical_composition.luminance_source)
        == "fallback-flat");
    assert(std::string_view(plan.optical_composition.depth_source)
        == "fallback-shadow-edge");
    assert(std::string_view(plan.optical_composition.refraction_source)
        == "none");
    assert(std::string_view(plan.optical_composition.dynamic_lighting_source)
        == "none");
    assert(std::string_view(plan.optical_composition.glass_thickness_source)
        == "none");
    assert(std::string_view(plan.optical_composition.glass_dispersion_source)
        == "none");
    assert(std::string_view(plan.optical_composition.fallback_source)
        == "unsupported-backend");
    assert(std::string_view(plan.optical_composition.stage_order)
        == "shadow-primary-edge");
    assert(std::string_view(plan.optical_composition.backdrop_capture_policy)
        == "no-capture");
    assert(std::string_view(plan.optical_composition.foreground_sampling_policy)
        == "not-applicable");
    assert(!plan.optical_composition.backdrop_sampled);
    assert(!plan.optical_composition.blur_required);
    assert(!plan.optical_composition.frosting_required);
    assert(plan.optical_composition.tint_required);
    assert(!plan.optical_composition.saturation_required);
    assert(plan.optical_composition.luminance_required);
    assert(plan.optical_composition.edge_required);
    assert(plan.optical_composition.shadow_required);
    assert(!plan.optical_composition.noise_required);
    assert(!plan.optical_composition.refraction_required);
    assert(!plan.optical_composition.spectral_tint_required);
    assert(!plan.optical_composition.dynamic_lighting_required);
    assert(!plan.optical_composition.glass_thickness_required);
    assert(!plan.optical_composition.glass_dispersion_required);
    assert(plan.optical_composition.fallback_required);
    assert(!plan.optical_composition.backdrop_capture_required);
    assert(!plan.optical_composition.foreground_excluded_from_backdrop);
    assert(plan.optical_composition.stage_order_stable);
    assert(plan.optical_composition.bounded);
    assert(plan.optical_composition.deterministic);
    assert(!plan.optical_response.backdrop_driven);
    assert(!plan.optical_response.blur_active);
    assert(!plan.optical_response.frosting_active);
    assert(plan.optical_response.tint_active);
    assert(!plan.optical_response.saturation_active);
    assert(plan.optical_response.luminance_preservation_active);
    assert(plan.optical_response.edge_highlight_active);
    assert(plan.optical_response.depth_shadow_active);
    assert(!plan.optical_response.noise_dither_active);
    assert(!plan.optical_response.refraction_active);
    assert(!plan.optical_response.spectral_tint_active);
    assert(!plan.optical_response.glass_thickness_active);
    assert(!plan.optical_response.glass_dispersion_active);
    assert(!plan.optical_response.foreground_vibrancy_active);
    assert(!plan.refraction.active);
    assert(plan.refraction.bounded);
    assert(std::string_view(plan.refraction.model) == "none");
    assert(std::string_view(plan.refraction.source) == "none");
    assert(plan.refraction.edge_caustic_intensity == 0.0f);
    assert(plan.resource_budget.max_refraction_offset_pixels == 0.0f);
    assert(!plan.spectral_tint.active);
    assert(std::string_view(plan.spectral_tint.source) == "none");
    assert(!plan.dynamic_lighting.active);
    assert(std::string_view(plan.dynamic_lighting.source) == "none");
    assert(!plan.glass_thickness.active);
    assert(std::string_view(plan.glass_thickness.source) == "none");
    assert(plan.glass_thickness.lensing_gain == 1.0f);
    assert(!plan.glass_dispersion.active);
    assert(std::string_view(plan.glass_dispersion.source) == "none");
    assert(plan.glass_dispersion.prismatic_gain == 1.0f);
    assert(plan.optical_response.deterministic_fallback);
    assert(!material_plan_uses_sampled_backdrop_executor(plan));
    assert(!material_plan_uses_standard_fill_executor(plan));
    assert(material_plan_uses_deterministic_fallback_executor(plan));
    assert(plan.paint_layer_count == 3u);
    assert(plan.dropped_paint_layer_count == 0u);
    assert(std::string_view(plan.paint_layers[0].executor) == "rounded-shadow");
    assert(std::string_view(plan.paint_layers[1].name)
        == "translucent-rounded-rect");
    assert(std::string_view(plan.paint_layers[1].executor) == "rounded-fill");
    assert(std::string_view(plan.paint_layers[2].executor) == "rounded-edge");
    assert(plan.resource_budget.max_paint_layer_inflate
        == plan.paint_layers[0].inflate);
    assert(plan.observation_contract.expected_paint_layers == 3u);
    assert(plan.observation_contract.expected_active_paint_layers == 3u);
    assert(plan.observation_contract.expected_shadow_paint_layers == 1u);
    assert(plan.observation_contract.expected_fill_paint_layers == 1u);
    assert(plan.observation_contract.expected_edge_paint_layers == 1u);
    std::puts("PASS: fallback backdrop access contract");
}

void test_glass_background_variants_shape_fallback_paint_policy() {
    auto env = sampled_environment();
    env.capabilities.material_backdrop_blur = false;
    env.capabilities.shader_blur = false;
    env.capabilities.frame_history = false;
    env.backdrop.available = false;
    env.backdrop.stable = false;
    env.backdrop.source = "none";

    auto plate_request = regular_request();
    plate_request.style.glass_background =
        MaterialGlassBackgroundDescriptor{
            .kind = MaterialGlassBackgroundEffectKind::Plate,
        };
    auto plate_plan = plan_material_surface(plate_request, env);
    assert(plate_plan.fallback());
    assert(plate_plan.glass_background.plate);
    assert(plate_plan.paint_layer_count == 3u);
    assert(std::string_view{plate_plan.paint_layers[1].name}
           == "plate-glass-fill");
    assert(std::string_view{plate_plan.paint_layers[1].executor}
           == "rounded-fill");
    assert(std::string_view{plate_plan.paint_layers[1].background_effect}
           == "plate");
    assert(plate_plan.paint_layers[1].inflate == 0.0f);
    assert(plate_plan.paint_layers[1].soft_edge_radius == 0.0f);

    auto feathered_request = regular_request();
    feathered_request.style.glass_background =
        material_glass_background_from_wire(
            static_cast<unsigned int>(
                MaterialGlassBackgroundEffectKind::Feathered),
            14.0f,
            6.0f);
    auto feathered_plan = plan_material_surface(feathered_request, env);
    assert(feathered_plan.fallback());
    assert(feathered_plan.glass_background.feathered);
    assert(feathered_plan.paint_layer_count == 3u);
    assert(std::string_view{feathered_plan.paint_layers[1].name}
           == "feathered-glass-fill");
    assert(std::string_view{feathered_plan.paint_layers[1].background_effect}
           == "feathered");
    assert(feathered_plan.paint_layers[1].inflate == 20.0f);
    assert(feathered_plan.paint_layers[1].radius_delta == 20.0f);
    assert(feathered_plan.paint_layers[1].soft_edge_radius == 6.0f);
    assert(feathered_plan.paint_layers[0].inflate
           > plate_plan.paint_layers[0].inflate);
    assert(feathered_plan.resource_budget.max_paint_layer_inflate
           == feathered_plan.paint_layers[0].inflate);
    assert(feathered_plan.observation_contract.expected_paint_layers == 3u);
    assert(feathered_plan.observation_contract.expected_fill_paint_layers == 1u);
    assert(feathered_plan.execution_audit.contract_satisfied);

    std::puts("PASS: glass background variants shape fallback paint policy");
}

void test_glass_background_sample_bounds_follow_execution_geometry() {
    auto baseline = plan_material_surface(regular_request(), sampled_environment());
    assert(baseline.backdrop_sampling);
    assert(baseline.backdrop_access.max_surface_sample_pixels == 240 * 96);

    auto plate_request = regular_request();
    plate_request.style.glass_background =
        MaterialGlassBackgroundDescriptor{
            .kind = MaterialGlassBackgroundEffectKind::Plate,
        };
    auto plate_plan = plan_material_surface(plate_request, sampled_environment());
    assert(plate_plan.backdrop_sampling);
    assert(plate_plan.glass_background.plate);
    assert(plate_plan.backdrop_access.max_surface_sample_pixels
           == baseline.backdrop_access.max_surface_sample_pixels);

    auto feathered_request = regular_request();
    feathered_request.style.glass_background =
        material_glass_background_from_wire(
            static_cast<unsigned int>(
                MaterialGlassBackgroundEffectKind::Feathered),
            14.0f,
            6.0f);
    auto feathered_plan =
        plan_material_surface(feathered_request, sampled_environment());
    auto const feathered_geometry =
        material_surface_execution_geometry(feathered_plan);
    assert(feathered_plan.backdrop_sampling);
    assert(feathered_plan.glass_background.feathered);
    assert(feathered_geometry.active);
    assert(std::fabs(feathered_geometry.x - (baseline.geometry.x - 20.0f))
           < 0.0001f);
    assert(std::fabs(feathered_geometry.y - (baseline.geometry.y - 20.0f))
           < 0.0001f);
    assert(std::fabs(feathered_geometry.w - 280.0f) < 0.0001f);
    assert(std::fabs(feathered_geometry.h - 136.0f) < 0.0001f);
    auto const feathered_pixels = static_cast<std::int64_t>(
        std::ceil(feathered_geometry.w * feathered_geometry.h));
    assert(feathered_pixels == 38'080);
    assert(feathered_plan.backdrop_access.max_surface_sample_pixels
           == feathered_pixels);
    assert(feathered_plan.resource_budget.max_surface_sample_pixels
           == feathered_pixels);
    assert(feathered_plan.backdrop_access.max_surface_sample_pixels
           > baseline.backdrop_access.max_surface_sample_pixels);

    auto materialize_request = feathered_request;
    materialize_request.style.transition = MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::Materialize,
        .progress = 0.5f,
        .appearing = true,
    };
    auto materialize_plan =
        plan_material_surface(materialize_request, sampled_environment());
    auto const materialize_geometry =
        material_surface_execution_geometry(materialize_plan);
    assert(materialize_plan.backdrop_sampling);
    assert(materialize_plan.transition.materialize);
    assert(materialize_geometry.active);
    assert(materialize_geometry.w < feathered_geometry.w);
    assert(materialize_geometry.h < feathered_geometry.h);
    auto const materialize_pixels = static_cast<std::int64_t>(
        std::ceil(materialize_geometry.w * materialize_geometry.h));
    assert(materialize_plan.backdrop_access.max_surface_sample_pixels
           == materialize_pixels);
    assert(materialize_plan.resource_budget.max_surface_sample_pixels
           == materialize_pixels);

    std::puts("PASS: glass background sample bounds follow execution geometry");
}

void test_custom_theme_snapshot_contract() {
    Theme theme{};
    theme.accent = {255, 45, 85, 255};
    theme.accent_strong = {196, 24, 56, 255};
    theme.surface = {250, 250, 252, 238};
    auto request = MaterialRequest{
        material_style_for_kind(MaterialKind::Thin, theme),
        MaterialGeometry{4.0f, 8.0f, 120.0f, 64.0f, 12.0f},
    };

    auto plan = plan_material_surface(request, sampled_environment());

    assert(!plan.theme.default_glass_tokens);
    assert(std::string(plan.theme.profile_name) == "custom");
    assert(plan.theme.foreground_matches_theme);
    assert(!plan.theme.accent_matches_theme);
    assert(!plan.theme.tint_matches_surface);
    assert(plan.theme.border_matches_theme);
    std::puts("PASS: custom theme snapshot contract");
}

void test_command_material_preserves_theme_snapshot_contract() {
    auto const descriptor = material_command_descriptor(
        material_style_for_kind(MaterialKind::Regular, Theme{}));
    auto request = MaterialRequest{
        material_style_for_command(descriptor, Theme{}),
        MaterialGeometry{4.0f, 8.0f, 120.0f, 64.0f, 12.0f},
    };

    auto plan = plan_material_surface(request, sampled_environment());

    assert(plan.theme.default_glass_tokens);
    assert(plan.theme.border_matches_theme);
    assert(plan.theme.foreground_matches_theme);
    assert(plan.theme.accent_matches_theme);
    assert(plan.theme.tint_matches_surface);
    std::puts("PASS: command material preserves theme snapshot contract");
}

void test_foreground_contrast_gap_uses_absolute_contrast_candidate() {
    auto dark_theme = apply_dark_color_scheme(Theme{});
    auto request = MaterialRequest{
        material_style_for_kind(MaterialKind::Regular, dark_theme),
        MaterialGeometry{12.0f, 20.0f, 240.0f, 96.0f, 10.0f},
    };
    auto env = sampled_environment();
    env.backdrop.luma_min = 0.26f;
    env.backdrop.luma_max = 0.38f;
    env.backdrop.luma_mean = 0.32f;

    auto plan = plan_material_surface(request, env);

    assert(plan.backdrop_sampling);
    assert(plan.foreground.primary_contrast_ratio
           >= plan.foreground.minimum_contrast_ratio);
    assert(plan.foreground.secondary_contrast_ratio
           >= plan.foreground.minimum_contrast_ratio);
    assert(plan.foreground.accent_contrast_ratio
           >= plan.foreground.minimum_contrast_ratio);
    assert(plan.reference_model.legibility_preserved);
    assert((plan.foreground.primary == Color{0, 0, 0, 255}));
    std::puts("PASS: foreground contrast gap uses absolute contrast candidate");
}

void test_clear_glass_legibility_dimming_contract() {
    auto env = sampled_environment();
    env.backdrop.luma_min = 0.70f;
    env.backdrop.luma_max = 0.98f;
    env.backdrop.luma_mean = 0.90f;
    env.backdrop.color_mean = Color{236, 244, 255, 255};

    auto plan = plan_material_surface(clear_request(), env);

    assert(plan.kind == MaterialKind::Clear);
    assert(plan.backdrop_sampling);
    assert(plan.clear_glass_legibility.active);
    assert(plan.clear_glass_legibility.backdrop_driven);
    assert(plan.clear_glass_legibility.brightness_driven);
    assert(plan.clear_glass_legibility.detail_driven);
    assert(!plan.clear_glass_legibility.accessibility_driven);
    assert(plan.clear_glass_legibility.bounded);
    assert(std::string_view(plan.clear_glass_legibility.model)
        == "adaptive-clear-glass-legibility");
    assert(std::string_view(plan.clear_glass_legibility.source)
        == "sampled-clear-glass-bright-detail-dimming");
    assert(plan.clear_glass_legibility.dimming_strength > 0.15f);
    assert(plan.clear_glass_legibility.contrast_lift > 0.07f);
    assert(plan.clear_glass_legibility.brightness_response > 0.70f);
    assert(plan.clear_glass_legibility.detail_response > 0.0f);
    assert(plan.foreground.background_luma < env.backdrop.luma_mean);
    assert(plan.foreground.primary_contrast_ratio
           >= plan.foreground.minimum_contrast_ratio);
    assert(plan.reference_model.legibility_preserved);
    assert(std::string_view(
        plan.optical_composition.clear_glass_legibility_source)
        == "sampled-clear-glass-bright-detail-dimming");
    assert(plan.optical_composition.clear_glass_legibility_required);
    assert(plan.optical_composition.clear_glass_dimming
           == plan.clear_glass_legibility.dimming_strength);
    assert(plan.optical_composition.clear_glass_contrast
           == plan.clear_glass_legibility.contrast_lift);
    assert(plan.optical_composition.clear_glass_brightness_response
           == plan.clear_glass_legibility.brightness_response);
    assert(plan.optical_composition.clear_glass_detail_response
           == plan.clear_glass_legibility.detail_response);
    assert(plan.optical_response.clear_glass_legibility_active);
    assert(plan.execution_stages[1].optics.clear_glass_dimming
           == plan.clear_glass_legibility.dimming_strength);
    assert(plan.execution_stages[1].optics.clear_glass_contrast
           == plan.clear_glass_legibility.contrast_lift);
    assert(std::string_view(
        plan.execution_stages[1].optics.clear_glass_legibility_model)
        == "adaptive-clear-glass-legibility");

    auto regular_plan = plan_material_surface(regular_request(), env);
    assert(!regular_plan.clear_glass_legibility.active);
    assert(!regular_plan.optical_response.clear_glass_legibility_active);

    auto high_contrast_env = env;
    high_contrast_env.capabilities.increase_contrast = true;
    auto high_contrast_plan =
        plan_material_surface(clear_request(), high_contrast_env);
    assert(high_contrast_plan.clear_glass_legibility.accessibility_driven);
    assert(high_contrast_plan.clear_glass_legibility.dimming_strength
           > plan.clear_glass_legibility.dimming_strength);
    assert(high_contrast_plan.clear_glass_legibility.contrast_lift
           > plan.clear_glass_legibility.contrast_lift);

    std::puts("PASS: clear glass legibility dimming contract");
}

void test_increase_contrast_raises_foreground_readability_contract() {
    auto baseline_env = sampled_environment();
    baseline_env.backdrop.luma_min = 0.42f;
    baseline_env.backdrop.luma_max = 0.64f;
    baseline_env.backdrop.luma_mean = 0.54f;

    auto baseline = plan_material_surface(regular_request(), baseline_env);
    auto env = baseline_env;
    env.capabilities.increase_contrast = true;
    auto plan = plan_material_surface(regular_request(), env);

    assert(plan.backdrop_sampling);
    assert(plan.decision_trace.increase_contrast);
    assert(plan.opacity > baseline.opacity);
    assert(plan.edge_highlight > baseline.edge_highlight);
    assert(plan.edge_width > baseline.edge_width);
    assert(plan.shadow_alpha > baseline.shadow_alpha);
    assert(plan.optical_composition.edge_highlight == plan.edge_highlight);
    assert(plan.optical_composition.edge_width == plan.edge_width);
    assert(plan.execution_stages[2].optics.edge_width == plan.edge_width);
    assert(plan.foreground.high_contrast);
    assert(std::string_view(plan.foreground.scheme) == "high-contrast");
    assert(std::string_view(plan.foreground.source) == "accessibility");
    assert(std::string_view(plan.foreground.contrast_policy)
        == "enhanced-contrast");
    assert(std::string_view(plan.foreground.remap_policy)
        == "strict-theme-role-remap");
    assert(plan.foreground.minimum_contrast_ratio >= 7.0f);
    assert(plan.foreground.primary_contrast_ratio
           >= plan.foreground.minimum_contrast_ratio);
    assert(plan.foreground.secondary_contrast_ratio
           >= plan.foreground.minimum_contrast_ratio);
    assert(plan.foreground.accent_contrast_ratio
           >= plan.foreground.minimum_contrast_ratio);
    assert(std::fabs(
        plan.foreground.primary_contrast_margin
        - (plan.foreground.primary_contrast_ratio
           - plan.foreground.minimum_contrast_ratio)) < 0.0001f);
    assert(std::fabs(
        plan.foreground.secondary_contrast_margin
        - (plan.foreground.secondary_contrast_ratio
           - plan.foreground.minimum_contrast_ratio)) < 0.0001f);
    assert(std::fabs(
        plan.foreground.accent_contrast_margin
        - (plan.foreground.accent_contrast_ratio
           - plan.foreground.minimum_contrast_ratio)) < 0.0001f);
    assert(plan.reference_model.legibility_preserved);
    assert(std::string_view(plan.reference_model.accessibility_response)
        == "increased-contrast");
    std::puts("PASS: increase contrast raises foreground readability contract");
}

void test_reduced_transparency_uses_solid_accessible_glass_fallback_contract() {
    auto env = sampled_environment();
    auto baseline = plan_material_surface(regular_request(), env);
    env.capabilities.reduce_transparency = true;

    auto plan = plan_material_surface(regular_request(), env);

    assert(plan.fallback());
    assert(plan.fallback_path == MaterialFallbackPath::ReducedTransparency);
    assert(!plan.backdrop_sampling);
    assert(plan.blur_radius == 0.0f);
    assert(plan.saturation == 1.0f);
    assert(plan.noise_opacity == 0.0f);
    assert(plan.opacity > baseline.opacity);
    assert(plan.tint.a > baseline.tint.a);
    assert(plan.luminance_floor > baseline.luminance_floor);
    assert(plan.edge_highlight > baseline.edge_highlight);
    assert(plan.edge_width > baseline.edge_width);
    assert(plan.shadow_alpha > baseline.shadow_alpha);
    assert(plan.shadow_radius >= baseline.shadow_radius);
    assert(material_alpha_fraction(plan.tint) * plan.opacity > 0.76f);
    assert(std::string_view(plan.foreground.scheme) == "solid-fallback");
    assert(std::string_view(plan.reference_model.accessibility_response)
        == "reduced-transparency");
    assert(std::string_view(plan.reference_model.performance_response)
        == "deterministic-fallback");
    assert(std::string_view(plan.optical_response.response_model)
        == "deterministic-fallback");
    assert(std::string_view(plan.optical_composition.fallback_source)
        == "reduced-transparency");
    assert(plan.paint_layer_count == 3u);
    assert(plan.paint_layers[1].color.a == plan.tint.a);
    assert(plan.paint_layers[1].opacity == plan.opacity);

    std::puts(
        "PASS: reduced transparency uses solid accessible glass fallback contract");
}

void test_interactive_material_modulates_optics_contract() {
    auto request = regular_request();
    request.style.container = MaterialContainerDescriptor{
        .container_id = 88u,
        .union_id = 0u,
        .spacing = 18.0f,
        .interactive = true,
        .morph_transitions = true,
    };
    request.style.interaction = MaterialInteractionDescriptor{
        .hovered = true,
        .pressed = false,
        .focused = true,
        .pointer_inside = true,
        .pointer_x = 0.75f,
        .pointer_y = 0.25f,
    };

    auto baseline = regular_request();
    baseline.style.container = request.style.container;
    auto baseline_plan =
        plan_material_surface(baseline, sampled_environment());
    auto plan = plan_material_surface(request, sampled_environment());

    assert(plan.container.interactive);
    assert(plan.interaction.enabled);
    assert(plan.interaction.active);
    assert(plan.interaction.hovered);
    assert(plan.interaction.focused);
    assert(plan.interaction.pointer_inside);
    assert(!plan.interaction.pressed);
    assert(std::string_view(plan.interaction.state) == "hovered");
    assert(std::string_view(plan.interaction.enablement_reason)
        == "interactive-container");
    assert(std::string_view(plan.interaction.response_model)
        == "liquid-glass-interaction");
    assert(std::string_view(plan.interaction.motion_policy)
        == "animated-optical-response");
    assert(std::string_view(plan.interaction.specular_model)
        == "pointer-specular");
    assert(plan.interaction.specular_highlight_active);
    assert(std::fabs(plan.interaction.specular_anchor_x - 0.75f) < 0.0001f);
    assert(std::fabs(plan.interaction.specular_anchor_y - 0.25f) < 0.0001f);
    assert(plan.interaction.specular_radius > 0.0f);
    assert(plan.interaction.specular_intensity > 0.0f);
    assert(std::string_view(plan.interaction.pointer_lens_model)
        == "hover-pointer-lens");
    assert(plan.interaction.pointer_lens_active);
    assert(std::fabs(plan.interaction.pointer_lens_anchor_x - 0.75f)
           < 0.0001f);
    assert(std::fabs(plan.interaction.pointer_lens_anchor_y - 0.25f)
           < 0.0001f);
    assert(plan.interaction.pointer_lens_radius > 0.0f);
    assert(plan.interaction.pointer_lens_strength > 0.0f);
    assert(std::string_view(plan.interaction.control_morph_model)
        == "hover-liquid-control-morph");
    assert(plan.interaction.control_morph_active);
    assert(!plan.interaction.control_morph_reduced_motion_suppressed);
    assert(plan.interaction.control_morph_scale_delta > 0.0f);
    assert(plan.interaction.control_morph_depth > 0.0f);
    assert(plan.interaction.control_morph_edge > 0.0f);
    assert(plan.interaction.control_morph_shadow > 0.0f);
    assert(plan.interaction.response_strength > 0.0f);
    assert(plan.opacity > baseline_plan.opacity);
    assert(plan.blur_radius > baseline_plan.blur_radius);
    assert(plan.saturation > baseline_plan.saturation);
    assert(plan.edge_highlight > baseline_plan.edge_highlight);
    assert(plan.shadow_alpha > baseline_plan.shadow_alpha);
    assert(plan.shadow_radius > baseline_plan.shadow_radius);
    assert(plan.refraction.active);
    assert(plan.refraction.interaction_driven);
    assert(std::string_view(plan.refraction.model)
        == "interactive-pointer-lens");
    assert(std::string_view(plan.refraction.source)
        == "sampled-backdrop-pointer-refraction");
    assert(plan.refraction.strength > baseline_plan.refraction.strength);
    assert(plan.refraction.max_offset_pixels
           > baseline_plan.refraction.max_offset_pixels);
    assert(plan.refraction.edge_caustic_intensity
           > baseline_plan.refraction.edge_caustic_intensity);
    assert(plan.dynamic_lighting.active);
    assert(plan.dynamic_lighting.interaction_driven);
    assert(std::string_view(plan.dynamic_lighting.model)
        == "interactive-dynamic-glass-light");
    assert(std::string_view(plan.dynamic_lighting.source)
        == "sampled-backdrop-interactive-lighting");
    assert(plan.dynamic_lighting.direction_x
           > baseline_plan.dynamic_lighting.direction_x);
    assert(plan.dynamic_lighting.highlight_strength
           > baseline_plan.dynamic_lighting.highlight_strength);
    assert(plan.glass_thickness.active);
    assert(plan.glass_thickness.interaction_driven);
    assert(std::string_view(plan.glass_thickness.source)
        == "interactive-thickness-lensing");
    assert(plan.glass_thickness.thickness
           > baseline_plan.glass_thickness.thickness);
    assert(plan.glass_thickness.scattering_gain
           > baseline_plan.glass_thickness.scattering_gain);
    assert(plan.glass_dispersion.active);
    assert(plan.glass_dispersion.interaction_driven);
    assert(std::string_view(plan.glass_dispersion.source)
        == "interactive-prismatic-glass-dispersion");
    assert(plan.glass_dispersion.axial_offset_pixels
           > baseline_plan.glass_dispersion.axial_offset_pixels);
    assert(plan.glass_dispersion.prismatic_gain
           > baseline_plan.glass_dispersion.prismatic_gain);
    assert(plan.specular.active);
    assert(plan.specular.ambient);
    assert(plan.specular.interaction_driven);
    assert(std::string_view(plan.specular.model) == "pointer-specular");
    assert(std::string_view(plan.specular.source)
        == "sampled-backdrop-pointer-lighting");
    assert(plan.specular.anchor_x == plan.interaction.specular_anchor_x);
    assert(plan.specular.anchor_y == plan.interaction.specular_anchor_y);
    assert(plan.specular.radius == plan.interaction.specular_radius);
    assert(plan.specular.intensity > plan.interaction.specular_intensity);
    assert(plan.specular.intensity > baseline_plan.specular.intensity);
    assert(plan.interaction.opacity_delta
           == plan.opacity - baseline_plan.opacity);
    assert(plan.interaction.blur_radius_delta
           == plan.blur_radius - baseline_plan.blur_radius);
    assert(plan.reference_model.interactive_response);
    assert(plan.optical_composition.interaction_required);
    assert(std::string_view(plan.optical_composition.interaction_source)
        == "liquid-glass-interaction");
    assert(plan.optical_composition.interaction_response_strength
           == plan.interaction.response_strength);
    assert(plan.optical_composition.interaction_control_morph_scale_delta
           == plan.interaction.control_morph_scale_delta);
    assert(plan.optical_composition.interaction_control_morph_depth
           == plan.interaction.control_morph_depth);
    assert(plan.optical_composition.interaction_control_morph_edge
           == plan.interaction.control_morph_edge);
    assert(plan.optical_composition.interaction_control_morph_shadow
           == plan.interaction.control_morph_shadow);
    assert(plan.optical_composition.refraction_required);
    assert(plan.optical_composition.refraction_strength
           == plan.refraction.strength);
    assert(plan.optical_composition.dynamic_lighting_required);
    assert(plan.optical_composition.dynamic_light_highlight
           == plan.dynamic_lighting.highlight_strength);
    assert(plan.optical_composition.glass_thickness_required);
    assert(plan.optical_composition.glass_scattering_gain
           == plan.glass_thickness.scattering_gain);
    assert(plan.optical_composition.glass_dispersion_required);
    assert(plan.optical_composition.glass_dispersion_axial_offset
           == plan.glass_dispersion.axial_offset_pixels);
    assert(plan.optical_response.interaction_active);
    assert(plan.optical_response.interaction_modulates_optics);
    assert(plan.optical_response.refraction_active);
    assert(plan.optical_response.dynamic_lighting_active);
    assert(plan.optical_response.glass_thickness_active);
    assert(plan.optical_response.glass_dispersion_active);
    assert(std::string_view(
               plan.execution_stages[1].optics.control_morph_model)
        == "hover-liquid-control-morph");
    assert(plan.execution_stages[1].optics.control_morph_scale_delta
           == plan.interaction.control_morph_scale_delta);
    assert(plan.execution_stages[2].optics.control_morph_depth
           == plan.interaction.control_morph_depth);

    auto focused_request = regular_request();
    focused_request.style.container = request.style.container;
    focused_request.style.interaction = MaterialInteractionDescriptor{
        .hovered = false,
        .pressed = false,
        .focused = true,
        .pointer_inside = false,
        .pointer_x = 0.15f,
        .pointer_y = 0.85f,
    };
    auto focused_plan =
        plan_material_surface(focused_request, sampled_environment());
    assert(focused_plan.container.interactive);
    assert(focused_plan.interaction.enabled);
    assert(focused_plan.interaction.active);
    assert(focused_plan.interaction.focused);
    assert(!focused_plan.interaction.hovered);
    assert(!focused_plan.interaction.pressed);
    assert(!focused_plan.interaction.pointer_inside);
    assert(std::string_view(focused_plan.interaction.state) == "focused");
    assert(std::string_view(focused_plan.interaction.specular_model)
        == "focus-specular");
    assert(focused_plan.interaction.specular_highlight_active);
    assert(std::fabs(focused_plan.interaction.specular_anchor_x - 0.5f)
           < 0.0001f);
    assert(std::fabs(focused_plan.interaction.specular_anchor_y - 0.5f)
           < 0.0001f);
    assert(focused_plan.interaction.specular_radius > 0.0f);
    assert(focused_plan.interaction.specular_intensity > 0.0f);
    assert(!focused_plan.interaction.pointer_lens_active);
    assert(std::string_view(focused_plan.interaction.control_morph_model)
        == "focused-liquid-control-morph");
    assert(focused_plan.interaction.control_morph_active);
    assert(focused_plan.interaction.control_morph_scale_delta > 0.0f);
    assert(focused_plan.interaction.control_morph_depth > 0.0f);
    assert(focused_plan.interaction.control_morph_edge > 0.0f);
    assert(focused_plan.edge_highlight > baseline_plan.edge_highlight);
    assert(focused_plan.specular.active);
    assert(focused_plan.specular.ambient);
    assert(focused_plan.specular.interaction_driven);
    assert(std::string_view(focused_plan.specular.model)
        == "focus-specular");
    assert(std::string_view(focused_plan.specular.source)
        == "sampled-backdrop-focus-lighting");
    assert(focused_plan.specular.anchor_x
           == focused_plan.interaction.specular_anchor_x);
    assert(focused_plan.specular.anchor_y
           == focused_plan.interaction.specular_anchor_y);
    assert(focused_plan.specular.radius
           == focused_plan.interaction.specular_radius);
    assert(focused_plan.specular.intensity
           > focused_plan.interaction.specular_intensity);
    assert(focused_plan.optical_response.interaction_active);
    assert(focused_plan.optical_response.interaction_modulates_optics);
    assert(std::string_view(
               focused_plan.execution_stages[2].optics.specular_model)
        == "focus-specular");
    assert(focused_plan.execution_stages[2].optics.specular_intensity
           == focused_plan.specular.intensity);

    auto reduced_focus_env = sampled_environment();
    reduced_focus_env.capabilities.reduce_motion = true;
    auto reduced_focus_plan =
        plan_material_surface(focused_request, reduced_focus_env);
    assert(reduced_focus_plan.interaction.specular_highlight_active);
    assert(reduced_focus_plan.interaction.specular_intensity
           < focused_plan.interaction.specular_intensity);
    assert(!reduced_focus_plan.interaction.pointer_lens_active);
    assert(reduced_focus_plan.specular.interaction_driven);
    assert(reduced_focus_plan.specular.intensity
           < focused_plan.specular.intensity);

    auto pressed_request = request;
    pressed_request.style.interaction.hovered = false;
    pressed_request.style.interaction.pressed = true;
    auto pressed_plan =
        plan_material_surface(pressed_request, sampled_environment());
    assert(pressed_plan.interaction.enabled);
    assert(pressed_plan.interaction.active);
    assert(pressed_plan.interaction.pressed);
    assert(std::string_view(pressed_plan.interaction.control_morph_model)
        == "pressed-liquid-control-morph");
    assert(pressed_plan.interaction.control_morph_scale_delta < 0.0f);
    assert(pressed_plan.interaction.control_morph_depth
           > plan.interaction.control_morph_depth);
    assert(pressed_plan.interaction.control_morph_shadow
           > plan.interaction.control_morph_shadow);

    auto reduced_env = sampled_environment();
    reduced_env.capabilities.reduce_motion = true;
    auto reduced_plan = plan_material_surface(request, reduced_env);
    assert(reduced_plan.interaction.enabled);
    assert(reduced_plan.interaction.active);
    assert(reduced_plan.interaction.reduce_motion);
    assert(std::string_view(reduced_plan.interaction.enablement_reason)
        == "interactive-container");
    assert(std::string_view(reduced_plan.interaction.motion_policy)
        == "reduced-motion-static");
    assert(reduced_plan.interaction.specular_highlight_active);
    assert(reduced_plan.interaction.specular_intensity
           < plan.interaction.specular_intensity);
    assert(reduced_plan.interaction.pointer_lens_active);
    assert(reduced_plan.interaction.pointer_lens_strength
           < plan.interaction.pointer_lens_strength);
    assert(reduced_plan.interaction.control_morph_active);
    assert(reduced_plan.interaction.control_morph_reduced_motion_suppressed);
    assert(reduced_plan.interaction.control_morph_scale_delta
           < plan.interaction.control_morph_scale_delta);
    assert(reduced_plan.interaction.control_morph_depth
           < plan.interaction.control_morph_depth);
    assert(reduced_plan.interaction.control_morph_edge
           < plan.interaction.control_morph_edge);
    assert(reduced_plan.interaction.control_morph_shadow
           < plan.interaction.control_morph_shadow);
    assert(reduced_plan.interaction.response_strength
           <= plan.interaction.response_strength);
    assert(reduced_plan.refraction.active);
    assert(reduced_plan.refraction.reduced_motion_suppressed);
    assert(reduced_plan.refraction.strength < plan.refraction.strength);
    assert(reduced_plan.refraction.max_offset_pixels
           < plan.refraction.max_offset_pixels);
    assert(reduced_plan.refraction.edge_caustic_intensity
           < plan.refraction.edge_caustic_intensity);
    assert(reduced_plan.dynamic_lighting.active);
    assert(reduced_plan.dynamic_lighting.reduced_motion_suppressed);
    assert(reduced_plan.dynamic_lighting.highlight_strength
           < plan.dynamic_lighting.highlight_strength);
    assert(reduced_plan.dynamic_lighting.shadow_strength
           < plan.dynamic_lighting.shadow_strength);
    assert(reduced_plan.glass_thickness.active);
    assert(reduced_plan.glass_thickness.reduced_motion_suppressed);
    assert(reduced_plan.glass_thickness.thickness
           < plan.glass_thickness.thickness);
    assert(reduced_plan.glass_dispersion.active);
    assert(reduced_plan.glass_dispersion.reduced_motion_suppressed);
    assert(reduced_plan.glass_dispersion.axial_offset_pixels
           < plan.glass_dispersion.axial_offset_pixels);
    assert(reduced_plan.glass_dispersion.prismatic_gain
           < plan.glass_dispersion.prismatic_gain);
    assert(reduced_plan.specular.active);
    assert(reduced_plan.specular.interaction_driven);
    assert(reduced_plan.specular.intensity < plan.specular.intensity);
    assert(!reduced_plan.container.morph_transitions);
    std::puts("PASS: interactive material modulates optics contract");
}

void test_prominent_glass_action_optics_contract() {
    auto request = regular_request();
    request.style.role = MaterialSurfaceRole::Control;
    request.style.tint = Color{64, 156, 255, 178};
    request.style.prominence = MaterialProminenceDescriptor{
        .enabled = true,
        .intensity = 1.0f,
    };

    auto plan = plan_material_surface(request, sampled_environment());

    assert(plan.backdrop_sampling);
    assert(plan.command_descriptor.prominence.enabled);
    assert(plan.prominent_glass.active);
    assert(plan.prominent_glass.control_driven);
    assert(plan.prominent_glass.tint_driven);
    assert(plan.prominent_glass.backdrop_driven);
    assert(!plan.prominent_glass.interaction_driven);
    assert(plan.prominent_glass.bounded);
    assert(std::string_view(plan.prominent_glass.model)
        == "prominent-liquid-glass-action");
    assert(std::string_view(plan.prominent_glass.source)
        == "prominent-control-accent-glass");
    assert(plan.prominent_glass.intensity > 0.99f);
    assert(plan.prominent_glass.tint_weight > 0.0f);
    assert(plan.prominent_glass.edge_lift > 0.0f);
    assert(plan.prominent_glass.lensing_gain > 1.0f);
    assert(plan.optical_composition.prominent_glass_required);
    assert(std::string_view(plan.optical_composition.prominent_glass_source)
        == "prominent-control-accent-glass");
    assert(plan.optical_composition.prominent_glass_intensity
           == plan.prominent_glass.intensity);
    assert(plan.optical_composition.prominent_glass_tint_weight
           == plan.prominent_glass.tint_weight);
    assert(plan.optical_composition.prominent_glass_edge_lift
           == plan.prominent_glass.edge_lift);
    assert(plan.optical_composition.prominent_glass_lensing_gain
           == plan.prominent_glass.lensing_gain);
    assert(plan.optical_response.prominent_glass_active);

    bool saw_primary_stage = false;
    for (unsigned int i = 0; i < plan.execution_stage_count; ++i) {
        auto const& stage = plan.execution_stages[i];
        if (std::string_view(stage.executor) != "backdrop-filter")
            continue;
        saw_primary_stage = true;
        assert(std::string_view(stage.optics.prominent_glass_model)
            == "prominent-liquid-glass-action");
        assert(stage.optics.prominent_glass_intensity
               == plan.prominent_glass.intensity);
        assert(stage.optics.prominent_glass_lensing_gain
               == plan.prominent_glass.lensing_gain);
    }
    assert(saw_primary_stage);

    auto reduced_env = sampled_environment();
    reduced_env.capabilities.reduce_motion = true;
    auto reduced_plan = plan_material_surface(request, reduced_env);
    assert(reduced_plan.prominent_glass.active);
    assert(reduced_plan.prominent_glass.reduced_motion_suppressed);
    assert(reduced_plan.prominent_glass.intensity
           < plan.prominent_glass.intensity);

    auto high_contrast_env = sampled_environment();
    high_contrast_env.capabilities.increase_contrast = true;
    auto high_contrast_plan =
        plan_material_surface(request, high_contrast_env);
    assert(high_contrast_plan.prominent_glass.active);
    assert(high_contrast_plan.prominent_glass.intensity
           == plan.prominent_glass.intensity);
    assert(high_contrast_plan.prominent_glass.tint_weight
           > plan.prominent_glass.tint_weight);
    assert(high_contrast_plan.prominent_glass.edge_lift
           > plan.prominent_glass.edge_lift);
    assert(high_contrast_plan.prominent_glass.lensing_gain
           > plan.prominent_glass.lensing_gain);
    assert(high_contrast_plan.optical_composition.prominent_glass_tint_weight
           == high_contrast_plan.prominent_glass.tint_weight);
    assert(high_contrast_plan.optical_response.prominent_glass_active);
    assert(std::string_view(
               high_contrast_plan.reference_model.accessibility_response)
           == "increased-contrast");

    auto interactive = request;
    interactive.style.container.interactive = true;
    interactive.style.interaction = MaterialInteractionDescriptor{
        .hovered = true,
        .pressed = false,
        .focused = true,
        .pointer_inside = true,
        .pointer_x = 0.82f,
        .pointer_y = 0.18f,
    };
    auto interactive_plan =
        plan_material_surface(interactive, sampled_environment());
    assert(interactive_plan.interaction.active);
    assert(interactive_plan.interaction.pointer_lens_active);
    assert(interactive_plan.prominent_glass.active);
    assert(interactive_plan.prominent_glass.interaction_driven);
    assert(std::string_view(interactive_plan.prominent_glass.source)
        == "interactive-prominent-control-accent-glass");
    assert(interactive_plan.prominent_glass.tint_weight
           > plan.prominent_glass.tint_weight);
    assert(interactive_plan.prominent_glass.edge_lift
           > plan.prominent_glass.edge_lift);
    assert(interactive_plan.prominent_glass.lensing_gain
           > plan.prominent_glass.lensing_gain);
    assert(std::string_view(
               interactive_plan.optical_composition.prominent_glass_source)
           == interactive_plan.prominent_glass.source);
    auto const& interactive_primary_stage =
        interactive_plan.execution_stages[1];
    assert(interactive_primary_stage.optics.prominent_glass_tint_weight
           == interactive_plan.prominent_glass.tint_weight);

    auto pressed = interactive;
    pressed.style.interaction.hovered = false;
    pressed.style.interaction.pressed = true;
    auto pressed_plan = plan_material_surface(pressed, sampled_environment());
    assert(pressed_plan.prominent_glass.interaction_driven);
    assert(pressed_plan.prominent_glass.tint_weight
           > interactive_plan.prominent_glass.tint_weight);
    assert(pressed_plan.prominent_glass.edge_lift
           > interactive_plan.prominent_glass.edge_lift);
    assert(pressed_plan.prominent_glass.lensing_gain
           > interactive_plan.prominent_glass.lensing_gain);

    auto disabled = request;
    disabled.style.prominence.enabled = false;
    auto disabled_plan = plan_material_surface(disabled, sampled_environment());
    assert(!disabled_plan.prominent_glass.active);
    assert(!disabled_plan.optical_response.prominent_glass_active);

    std::puts("PASS: prominent glass action optics contract");
}

void test_interactive_fallback_material_adds_pointer_highlight_layer() {
    auto env = sampled_environment();
    env.capabilities.material_backdrop_blur = false;
    env.capabilities.shader_blur = false;
    env.capabilities.frame_history = false;
    env.backdrop.available = false;
    env.backdrop.stable = false;
    env.backdrop.source = "none";

    auto baseline = regular_request();
    baseline.style.container = MaterialContainerDescriptor{
        .container_id = 89u,
        .union_id = 0u,
        .spacing = 18.0f,
        .interactive = true,
        .morph_transitions = true,
    };
    auto request = baseline;
    request.style.interaction = MaterialInteractionDescriptor{
        .hovered = true,
        .pressed = false,
        .focused = false,
        .pointer_inside = true,
        .pointer_x = 0.75f,
        .pointer_y = 0.25f,
    };

    auto const baseline_plan = plan_material_surface(baseline, env);
    auto const plan = plan_material_surface(request, env);

    assert(plan.fallback());
    assert(plan.interaction.enabled);
    assert(plan.interaction.active);
    assert(plan.interaction.specular_highlight_active);
    assert(plan.interaction.pointer_lens_active);
    assert(!plan.refraction.active);
    assert(plan.paint_layer_count == 4u);
    assert(plan.dropped_paint_layer_count == 0u);
    assert(baseline_plan.paint_layer_count == 3u);

    auto const& highlight = plan.paint_layers[2];
    assert(std::string_view(highlight.name) == "pointer-specular-highlight");
    assert(std::string_view(highlight.executor) == "rounded-fill");
    assert(highlight.x_offset > 0.0f);
    assert(highlight.y_offset < 0.0f);
    assert(highlight.inflate > plan.paint_layers[1].inflate);
    assert(highlight.radius_delta == highlight.inflate);
    assert(highlight.opacity > 0.0f);
    assert(highlight.opacity <= 0.26f);
    assert(highlight.soft_edge_radius > 0.0f);
    assert(highlight.color.r == 255);
    assert(highlight.color.g == 255);
    assert(highlight.color.b == 255);
    assert(highlight.color.a == 255);
    assert(std::string_view(plan.paint_layers[3].name)
        == "fallback-edge-highlight");
    assert(plan.observation_contract.expected_paint_layers == 4u);
    assert(plan.observation_contract.expected_active_paint_layers == 4u);
    assert(plan.observation_contract.expected_shadow_paint_layers == 1u);
    assert(plan.observation_contract.expected_fill_paint_layers == 2u);
    assert(plan.observation_contract.expected_edge_paint_layers == 1u);
    assert(plan.resource_budget.max_paint_layer_inflate >= highlight.inflate);

    auto const geometry =
        material_paint_layer_execution_geometry(plan, highlight);
    assert(geometry.active);
    assert(geometry.x < plan.geometry.x + highlight.x_offset);
    assert(geometry.y < plan.geometry.y + highlight.y_offset);
    assert(geometry.w > plan.geometry.w);
    assert(geometry.h > plan.geometry.h);

    std::puts("PASS: interactive fallback material adds pointer highlight layer");
}

void test_materialize_transition_modulates_glass_optics_contract() {
    auto request = regular_request();
    request.style.transition = MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::Materialize,
        .progress = 0.5f,
        .appearing = true,
    };

    auto baseline = plan_material_surface(regular_request(), sampled_environment());
    auto plan = plan_material_surface(request, sampled_environment());

    assert(plan.transition.active);
    assert(plan.transition.materialize);
    assert(!plan.transition.matched_geometry);
    assert(plan.transition.appearing);
    assert(!plan.transition.reduced_motion_suppressed);
    assert(plan.transition.bounded);
    assert(std::string_view(plan.transition.kind_name) == "materialize");
    assert(std::string_view(plan.transition.policy) == "materialize-in");
    assert(std::fabs(plan.transition.progress - 0.5f) < 0.0001f);
    assert(std::fabs(plan.transition.opacity_gain - 0.5f) < 0.0001f);
    assert(plan.transition.optical_gain > plan.transition.opacity_gain);
    assert(plan.transition.optical_gain < 1.0f);
    assert(plan.transition.shadow_gain > plan.transition.opacity_gain);
    assert(plan.transition.shadow_gain < 1.0f);
    assert(std::fabs(plan.transition.refraction_gain - 0.5f) < 0.0001f);
    assert(std::string_view(plan.transition.materialize_optics_model)
           == "materialize-liquid-glass-optics");
    assert(plan.transition.materialize_optics_active);
    assert(std::fabs(plan.transition.materialize_wave_strength - 1.0f)
           < 0.0001f);
    assert(plan.transition.materialize_edge_lift > 0.13f);
    assert(plan.transition.materialize_edge_lift < 0.14f);
    assert(std::fabs(plan.transition.materialize_lensing_gain - 1.24f)
           < 0.0001f);
    assert(std::fabs(plan.transition.materialize_rim_position - 0.59f)
           < 0.0001f);

    assert(plan.backdrop_sampling);
    assert(!plan.fallback());
    assert(plan.opacity < baseline.opacity);
    assert(plan.tint.a < baseline.tint.a);
    assert(plan.blur_radius < baseline.blur_radius);
    assert(plan.saturation < baseline.saturation);
    assert(plan.edge_highlight < baseline.edge_highlight);
    assert(plan.noise_opacity < baseline.noise_opacity);
    assert(plan.shadow_alpha < baseline.shadow_alpha);
    assert(plan.shadow_radius < baseline.shadow_radius);
    assert(plan.refraction.active);
    assert(plan.refraction.strength < baseline.refraction.strength);
    assert(plan.refraction.max_offset_pixels
           < baseline.refraction.max_offset_pixels);
    assert(plan.refraction.edge_caustic_intensity
           < baseline.refraction.edge_caustic_intensity);
    assert(plan.specular.active);
    assert(plan.specular.intensity < baseline.specular.intensity);
    assert(plan.optical_composition.transition_required);
    assert(std::string_view(plan.optical_composition.transition_source)
        == "glass-effect-materialize");
    assert(std::fabs(
               plan.optical_composition.transition_progress
                   - plan.transition.progress)
           < 0.0001f);
    assert(plan.optical_composition.transition_opacity_gain
           == plan.transition.opacity_gain);
    assert(plan.optical_composition.transition_optical_gain
           == plan.transition.optical_gain);
    assert(plan.optical_composition.transition_refraction_gain
           == plan.transition.refraction_gain);
    assert(plan.optical_composition.transition_materialize_wave_strength
           == plan.transition.materialize_wave_strength);
    assert(plan.optical_composition.transition_materialize_edge_lift
           == plan.transition.materialize_edge_lift);
    assert(plan.optical_composition.transition_materialize_lensing_gain
           == plan.transition.materialize_lensing_gain);
    assert(plan.optical_composition.transition_materialize_rim_position
           == plan.transition.materialize_rim_position);
    assert(plan.optical_composition.opacity == plan.opacity);
    assert(plan.optical_composition.blur_radius == plan.blur_radius);
    assert(plan.optical_composition.refraction_strength
           == plan.refraction.strength);
    auto const baseline_geometry = material_surface_execution_geometry(baseline);
    auto const transition_geometry = material_surface_execution_geometry(plan);
    auto const geometry_scale =
        material_surface_materialize_geometry_scale(plan.transition);
    assert(baseline_geometry.active);
    assert(transition_geometry.active);
    assert(geometry_scale > 0.72f);
    assert(geometry_scale < 1.0f);
    assert(std::fabs(transition_geometry.x
                     - (baseline_geometry.x
                        + baseline_geometry.w * (1.0f - geometry_scale) * 0.5f))
           < 0.0001f);
    assert(std::fabs(transition_geometry.y
                     - (baseline_geometry.y
                        + baseline_geometry.h * (1.0f - geometry_scale) * 0.5f))
           < 0.0001f);
    assert(std::fabs(transition_geometry.w - baseline_geometry.w * geometry_scale)
           < 0.0001f);
    assert(std::fabs(transition_geometry.h - baseline_geometry.h * geometry_scale)
           < 0.0001f);
    assert(transition_geometry.radius < baseline_geometry.radius);
    auto interactive_request = request;
    interactive_request.style.container = MaterialContainerDescriptor{
        .container_id = 940u,
        .union_id = 0u,
        .spacing = 20.0f,
        .interactive = true,
        .morph_transitions = true,
    };
    interactive_request.style.interaction = MaterialInteractionDescriptor{
        .hovered = true,
        .pressed = false,
        .focused = false,
        .pointer_inside = true,
        .pointer_x = 0.75f,
        .pointer_y = 0.25f,
    };
    auto interactive_plan =
        plan_material_surface(interactive_request, sampled_environment());
    auto const interactive_geometry =
        material_surface_execution_geometry(interactive_plan);
    assert(interactive_geometry.active);
    assert(interactive_plan.specular.interaction_driven);
    assert(interactive_plan.interaction.pointer_lens_active);
    auto const specular_anchor_x = material_surface_execution_anchor_x(
        interactive_plan,
        interactive_geometry,
        interactive_plan.specular.anchor_x);
    auto const specular_anchor_y = material_surface_execution_anchor_y(
        interactive_plan,
        interactive_geometry,
        interactive_plan.specular.anchor_y);
    auto const lens_anchor_x = material_surface_execution_anchor_x(
        interactive_plan,
        interactive_geometry,
        interactive_plan.interaction.pointer_lens_anchor_x);
    auto const lens_anchor_y = material_surface_execution_anchor_y(
        interactive_plan,
        interactive_geometry,
        interactive_plan.interaction.pointer_lens_anchor_y);
    assert(specular_anchor_x > interactive_plan.specular.anchor_x);
    assert(specular_anchor_y < interactive_plan.specular.anchor_y);
    assert(specular_anchor_x == lens_anchor_x);
    assert(specular_anchor_y == lens_anchor_y);
    auto const absolute_pointer_x =
        interactive_plan.geometry.x
        + interactive_plan.specular.anchor_x * interactive_plan.geometry.w;
    auto const absolute_pointer_y =
        interactive_plan.geometry.y
        + interactive_plan.specular.anchor_y * interactive_plan.geometry.h;
    assert(std::fabs(
               (interactive_geometry.x + specular_anchor_x * interactive_geometry.w)
                   - absolute_pointer_x)
           < 0.0001f);
    assert(std::fabs(
               (interactive_geometry.y + specular_anchor_y * interactive_geometry.h)
                   - absolute_pointer_y)
           < 0.0001f);

    auto disappearing_request = regular_request();
    disappearing_request.style.transition = MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::Materialize,
        .progress = 0.75f,
        .appearing = false,
    };
    auto disappearing_plan =
        plan_material_surface(disappearing_request, sampled_environment());
    constexpr auto disappearing_gain = 0.15625f;
    assert(disappearing_plan.transition.active);
    assert(disappearing_plan.transition.materialize);
    assert(!disappearing_plan.transition.matched_geometry);
    assert(!disappearing_plan.transition.appearing);
    assert(std::string_view(disappearing_plan.transition.policy)
        == "materialize-out");
    assert(std::fabs(disappearing_plan.transition.progress - 0.75f)
           < 0.0001f);
    assert(std::fabs(
               disappearing_plan.transition.opacity_gain - disappearing_gain)
           < 0.0001f);
    assert(std::fabs(
               disappearing_plan.transition.optical_gain - 0.341875f)
           < 0.0001f);
    assert(std::fabs(
               disappearing_plan.transition.shadow_gain - 0.240625f)
           < 0.0001f);
    assert(std::fabs(
               disappearing_plan.transition.refraction_gain
                   - disappearing_gain)
           < 0.0001f);
    assert(std::string_view(
               disappearing_plan.transition.materialize_optics_model)
           == "materialize-liquid-glass-optics");
    assert(disappearing_plan.transition.materialize_optics_active);
    assert(disappearing_plan.transition.materialize_wave_strength > 0.52f);
    assert(disappearing_plan.transition.materialize_wave_strength < 0.54f);
    assert(disappearing_plan.transition.materialize_edge_lift > 0.06f);
    assert(disappearing_plan.transition.materialize_lensing_gain > 1.11f);
    assert(disappearing_plan.transition.materialize_lensing_gain < 1.12f);
    assert(std::fabs(
               disappearing_plan.transition.materialize_rim_position
                   - 0.871875f)
           < 0.0001f);
    assert(disappearing_plan.opacity < baseline.opacity);
    assert(disappearing_plan.tint.a < baseline.tint.a);
    assert(disappearing_plan.blur_radius < baseline.blur_radius);
    assert(disappearing_plan.refraction.strength
           < baseline.refraction.strength);
    assert(disappearing_plan.specular.intensity
           < baseline.specular.intensity);
    assert(disappearing_plan.optical_composition.transition_required);
    assert(std::string_view(
               disappearing_plan.optical_composition.transition_source)
        == "glass-effect-materialize");
    assert(disappearing_plan.optical_composition.transition_opacity_gain
           == disappearing_plan.transition.opacity_gain);
    assert(disappearing_plan.optical_composition.transition_optical_gain
           == disappearing_plan.transition.optical_gain);
    assert(disappearing_plan.optical_composition.transition_refraction_gain
           == disappearing_plan.transition.refraction_gain);
    assert(disappearing_plan
               .optical_composition
               .transition_materialize_wave_strength
           == disappearing_plan.transition.materialize_wave_strength);
    assert(disappearing_plan
               .optical_composition
               .transition_materialize_lensing_gain
           == disappearing_plan.transition.materialize_lensing_gain);
    auto const disappearing_geometry =
        material_surface_execution_geometry(disappearing_plan);
    assert(disappearing_geometry.active);
    assert(disappearing_geometry.w < transition_geometry.w);
    assert(disappearing_geometry.h < transition_geometry.h);
    assert(std::fabs(
               (disappearing_geometry.x + disappearing_geometry.w * 0.5f)
                   - (baseline_geometry.x + baseline_geometry.w * 0.5f))
           < 0.0001f);
    assert(std::fabs(
               (disappearing_geometry.y + disappearing_geometry.h * 0.5f)
                   - (baseline_geometry.y + baseline_geometry.h * 0.5f))
           < 0.0001f);

    auto fallback_env = sampled_environment();
    fallback_env.capabilities.material_backdrop_blur = false;
    fallback_env.capabilities.shader_blur = false;
    fallback_env.capabilities.frame_history = false;
    fallback_env.backdrop.available = false;
    fallback_env.backdrop.stable = false;
    fallback_env.backdrop.source = "none";
    auto fallback_baseline = plan_material_surface(regular_request(), fallback_env);
    auto fallback_plan = plan_material_surface(request, fallback_env);
    assert(fallback_baseline.fallback());
    assert(fallback_plan.fallback());
    assert(fallback_plan.paint_layer_count > 1u);
    auto const& fallback_layer = fallback_plan.paint_layers[1];
    auto const fallback_baseline_geometry =
        material_paint_layer_execution_geometry(
            fallback_baseline,
            fallback_baseline.paint_layers[1]);
    auto const fallback_transition_geometry =
        material_paint_layer_execution_geometry(
            fallback_plan,
            fallback_layer);
    assert(fallback_baseline_geometry.active);
    assert(fallback_transition_geometry.active);
    assert(fallback_transition_geometry.w < fallback_baseline_geometry.w);
    assert(fallback_transition_geometry.h < fallback_baseline_geometry.h);
    assert(std::fabs(
               (fallback_transition_geometry.x
                + fallback_transition_geometry.w * 0.5f)
                   - (fallback_baseline_geometry.x
                      + fallback_baseline_geometry.w * 0.5f))
           < 0.0001f);
    assert(std::fabs(
               (fallback_transition_geometry.y
                + fallback_transition_geometry.h * 0.5f)
                   - (fallback_baseline_geometry.y
                      + fallback_baseline_geometry.h * 0.5f))
           < 0.0001f);

    auto reduced_env = sampled_environment();
    reduced_env.capabilities.reduce_motion = true;
    auto reduced_baseline = plan_material_surface(regular_request(), reduced_env);
    auto reduced_plan = plan_material_surface(request, reduced_env);
    assert(!reduced_plan.transition.active);
    assert(reduced_plan.transition.reduced_motion_suppressed);
    assert(reduced_plan.transition.progress == 1.0f);
    assert(std::string_view(reduced_plan.transition.policy)
        == "reduced-motion-static");
    assert(std::string_view(reduced_plan.transition.materialize_optics_model)
           == "none");
    assert(!reduced_plan.transition.materialize_optics_active);
    assert(reduced_plan.transition.materialize_wave_strength == 0.0f);
    assert(reduced_plan.transition.materialize_lensing_gain == 1.0f);
    assert(reduced_plan.opacity == reduced_baseline.opacity);
    assert(reduced_plan.tint.a == reduced_baseline.tint.a);
    assert(reduced_plan.blur_radius == reduced_baseline.blur_radius);
    auto const reduced_geometry =
        material_surface_execution_geometry(reduced_plan);
    auto const reduced_baseline_geometry =
        material_surface_execution_geometry(reduced_baseline);
    assert(reduced_geometry.active);
    assert(reduced_baseline_geometry.active);
    assert(reduced_geometry.x == reduced_baseline_geometry.x);
    assert(reduced_geometry.y == reduced_baseline_geometry.y);
    assert(reduced_geometry.w == reduced_baseline_geometry.w);
    assert(reduced_geometry.h == reduced_baseline_geometry.h);
    assert(reduced_geometry.radius == reduced_baseline_geometry.radius);
    assert(!reduced_plan.optical_composition.transition_required);
    assert(std::string_view(reduced_plan.optical_composition.transition_source)
        == "reduced-motion-static");
    std::puts("PASS: materialize transition modulates glass optics contract");
}

void test_glass_effect_identity_marks_matched_transition_contract() {
    auto request = regular_request();
    request.style.container = MaterialContainerDescriptor{
        .container_id = 55u,
        .union_id = 0u,
        .spacing = 16.0f,
        .interactive = false,
        .morph_transitions = true,
    };
    request.style.transition = MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::MatchedGeometry,
        .progress = 0.5f,
        .appearing = true,
    };
    request.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 77u,
        .effect_id = 901u,
    };

    auto descriptor = material_command_descriptor(request.style);
    auto roundtrip_style = material_style_for_command(descriptor, Theme{});
    auto plan = plan_material_surface(request, sampled_environment());

    assert(descriptor.glass_identity.namespace_id == 77u);
    assert(descriptor.glass_identity.effect_id == 901u);
    assert(roundtrip_style.glass_identity.namespace_id == 77u);
    assert(roundtrip_style.glass_identity.effect_id == 901u);
    assert(plan.command_descriptor.glass_identity.namespace_id == 77u);
    assert(plan.command_descriptor.glass_identity.effect_id == 901u);

    assert(plan.container.participates);
    assert(plan.transition.active);
    assert(plan.transition.matched_geometry);
    assert(plan.glass_identity.namespace_id == 77u);
    assert(plan.glass_identity.effect_id == 901u);
    assert(plan.glass_identity.namespace_scoped);
    assert(plan.glass_identity.effect_identified);
    assert(plan.glass_identity.participates);
    assert(plan.glass_identity.container_scoped);
    assert(plan.glass_identity.matched_geometry_candidate);
    assert(plan.glass_identity.bounded);
    assert(std::string_view(plan.glass_identity.source) == "glass-effect-id");
    assert(std::string_view(plan.glass_identity.policy)
           == "matched-geometry-container");
    assert(plan.reference_model.container_grouped);
    assert(plan.reference_model.container_morphing);
    assert(plan.reference_model.glass_effect_identified);
    assert(plan.reference_model.glass_effect_matched_geometry);

    auto partial = regular_request();
    partial.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 77u,
        .effect_id = 0u,
    };
    auto partial_plan = plan_material_surface(partial, sampled_environment());
    assert(partial_plan.glass_identity.namespace_scoped);
    assert(!partial_plan.glass_identity.effect_identified);
    assert(!partial_plan.glass_identity.participates);
    assert(std::string_view(partial_plan.glass_identity.source)
           == "partial-glass-effect-id");
    assert(std::string_view(partial_plan.glass_identity.policy)
           == "incomplete-effect-id");
    assert(!partial_plan.reference_model.glass_effect_identified);

    std::puts("PASS: glass effect identity marks matched transition contract");
}

void test_glass_effect_identity_drives_matched_execution_contract() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 12.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 88u,
        .union_id = 0u,
        .spacing = 128.0f,
        .interactive = false,
        .morph_transitions = true,
    };
    request.style.transition = MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::MatchedGeometry,
        .progress = 0.5f,
        .appearing = false,
    };
    request.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 19u,
        .effect_id = 700u,
    };

    auto peer = request;
    peer.geometry = MaterialGeometry{120.0f, 0.0f, 40.0f, 40.0f, 24.0f};
    peer.style.transition.appearing = true;

    auto unrelated = request;
    unrelated.geometry = MaterialGeometry{240.0f, 0.0f, 40.0f, 40.0f, 12.0f};
    unrelated.style.container.container_id = 89u;
    unrelated.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 19u,
        .effect_id = 701u,
    };

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(peer, sampled_environment()), 2u},
        {plan_material_surface(unrelated, sampled_environment()), 3u},
    };

    auto const group = accumulate_material_container_group(records, 88u);
    assert(group.shape_pair_count == 1u);
    assert(group.blend_candidate_pair_count == 1u);
    assert(material_container_group_shape_blend_execution_active(group));

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const second_execution =
        material_container_execution_descriptor(records[1], records);
    auto const unrelated_execution =
        material_container_execution_descriptor(records[2], records);

    assert(first_execution.active);
    assert(first_execution.shape_blend_execution);
    assert(!first_execution.union_execution);
    assert(first_execution.morph_execution);
    assert(first_execution.glass_effect_match_execution);
    assert(first_execution.glass_namespace_id == 19u);
    assert(first_execution.glass_effect_id == 700u);
    assert(first_execution.glass_effect_surface_count == 2u);
    assert(std::string_view(first_execution.execution_policy)
           == "glass-effect-matched-geometry");
    assert(first_execution.group_bounds_valid);
    assert(first_execution.group_x == 0.0f);
    assert(first_execution.group_y == 0.0f);
    assert(first_execution.group_w == 160.0f);
    assert(first_execution.group_h == 40.0f);
    assert(first_execution.glass_effect_match_source_valid);
    assert(first_execution.glass_effect_match_source_effect_id_match);
    assert(first_execution.glass_effect_match_source_x == 120.0f);
    assert(first_execution.glass_effect_match_source_y == 0.0f);
    assert(first_execution.glass_effect_match_source_w == 40.0f);
    assert(first_execution.glass_effect_match_source_h == 40.0f);
    assert(first_execution.glass_effect_match_source_radius == 24.0f);
    assert(std::fabs(first_execution.glass_effect_match_source_gap - 80.0f)
           < 0.0001f);
    assert(std::fabs(first_execution.glass_effect_match_source_spacing - 128.0f)
           < 0.0001f);
    assert(std::fabs(
               first_execution.glass_effect_match_source_proximity - 0.375f)
           < 0.0001f);
    assert(std::fabs(first_execution.glass_effect_match_rect_x - 60.0f)
           < 0.0001f);
    assert(first_execution.glass_effect_match_rect_y == 0.0f);
    assert(first_execution.glass_effect_match_rect_w == 40.0f);
    assert(first_execution.glass_effect_match_rect_h == 40.0f);
    assert(std::fabs(first_execution.glass_effect_match_rect_radius - 18.0f)
           < 0.0001f);
    assert(std::fabs(first_execution.glass_effect_match_progress - 0.5f)
           < 0.0001f);
    assert(std::fabs(first_execution.glass_effect_match_blend_strength - 0.5f)
           < 0.0001f);
    assert(std::fabs(first_execution.shape_blend_strength - 0.5f)
           < 0.0001f);
    assert(std::fabs(
               first_execution.inner_edge_alpha_blend_strength - 0.5f)
           < 0.0001f);
    assert(std::string_view(first_execution.fusion_model)
           == "matched-liquid-glass-fusion");
    assert(first_execution.fusion_optics_active);
    assert(first_execution.fusion_strength > first_execution.shape_blend_strength);
    assert(first_execution.fusion_lensing_gain > 1.0f);
    assert(first_execution.fusion_edge_lift > 0.0f);
    assert(first_execution.fusion_shadow_gain > 1.0f);
    assert(!first_execution.surface_leader);
    assert(!first_execution.paint_layer_leader);

    assert(second_execution.glass_effect_match_execution);
    assert(second_execution.glass_effect_surface_count == 2u);
    assert(second_execution.glass_effect_match_source_valid);
    assert(second_execution.glass_effect_match_source_effect_id_match);
    assert(second_execution.glass_effect_match_source_x == 0.0f);
    assert(second_execution.glass_effect_match_source_radius == 12.0f);
    assert(std::fabs(second_execution.glass_effect_match_source_gap - 80.0f)
           < 0.0001f);
    assert(std::fabs(second_execution.glass_effect_match_source_spacing - 128.0f)
           < 0.0001f);
    assert(std::fabs(
               second_execution.glass_effect_match_source_proximity - 0.375f)
           < 0.0001f);
    assert(std::fabs(second_execution.glass_effect_match_rect_x - 60.0f)
           < 0.0001f);
    assert(std::fabs(second_execution.glass_effect_match_rect_radius - 18.0f)
           < 0.0001f);
    assert(std::fabs(second_execution.shape_blend_strength - 0.5f)
           < 0.0001f);
    assert(std::string_view(second_execution.fusion_model)
           == "matched-liquid-glass-fusion");
    assert(second_execution.fusion_optics_active);
    assert(second_execution.fusion_strength == first_execution.fusion_strength);
    assert(second_execution.surface_leader);
    assert(second_execution.paint_layer_leader);

    auto const source_motion =
        material_glass_effect_match_motion_optics(first_execution);
    auto const target_motion =
        material_glass_effect_match_motion_optics(second_execution);
    assert(!source_motion.active);
    assert(target_motion.active);
    assert(std::fabs(target_motion.strength - 0.440625f) < 0.0001f);
    assert(target_motion.direction_x > 0.999f);
    assert(std::fabs(target_motion.direction_y) < 0.0001f);
    assert(target_motion.specular_anchor_x > 0.58f);
    assert(target_motion.specular_anchor_y == 0.5f);
    assert(target_motion.refraction_gain > 1.23f);
    assert(target_motion.caustic_gain > target_motion.refraction_gain);
    assert(target_motion.specular_intensity_gain > 1.29f);
    assert(target_motion.flow_offset_gain > 0.35f);
    assert(target_motion.highlight_gain > 0.17f);

    auto const first_geometry =
        material_surface_execution_geometry(records[0].plan, &first_execution);
    auto const second_geometry =
        material_surface_execution_geometry(records[1].plan, &second_execution);
    assert(!first_geometry.active);
    assert(second_geometry.active);
    assert(std::fabs(second_geometry.x - 60.0f) < 0.0001f);
    assert(second_geometry.w == 40.0f);

    MaterialRuntimeSummary runtime_summary{};
    for (auto const& record : records)
        accumulate_material_runtime_summary(runtime_summary, record);
    finalize_material_runtime_summary(runtime_summary, records);
    assert(runtime_summary.total_surface_sample_pixels == 40 * 40 + 40 * 40);
    assert(runtime_summary.max_surface_sample_pixels == 40 * 40);

    assert(!unrelated_execution.glass_effect_match_execution);
    assert(unrelated_execution.glass_effect_surface_count == 1u);
    assert(!unrelated_execution.shape_blend_execution);

    std::puts("PASS: glass effect identity drives matched execution contract");
}

void test_glass_effect_matched_geometry_respects_container_spacing() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 12.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 90u,
        .union_id = 0u,
        .spacing = 8.0f,
        .interactive = false,
        .morph_transitions = true,
    };
    request.style.transition = MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::MatchedGeometry,
        .progress = 0.5f,
        .appearing = true,
    };
    request.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 19u,
        .effect_id = 800u,
    };

    auto far_peer = request;
    far_peer.geometry =
        MaterialGeometry{120.0f, 0.0f, 40.0f, 40.0f, 24.0f};

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(far_peer, sampled_environment()), 2u},
    };

    auto const group = accumulate_material_container_group(records, 90u);
    assert(group.shape_pair_count == 1u);
    assert(group.blend_candidate_pair_count == 0u);
    assert(!material_container_group_shape_blend_execution_active(group));

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const second_execution =
        material_container_execution_descriptor(records[1], records);

    assert(first_execution.active);
    assert(first_execution.glass_effect_surface_count == 2u);
    assert(!first_execution.glass_effect_match_execution);
    assert(first_execution.glass_effect_materialize_execution);
    assert(!first_execution.glass_effect_match_source_valid);
    assert(first_execution.glass_effect_match_blend_strength == 0.0f);
    assert(first_execution.glass_effect_materialize_opacity_gain == 0.5f);
    assert(first_execution.glass_effect_materialize_optical_gain
           < records[0].plan.transition.optical_gain);
    assert(first_execution.glass_effect_materialize_shadow_gain
           < records[0].plan.transition.shadow_gain);
    assert(first_execution.glass_effect_materialize_refraction_gain
           < records[0].plan.transition.refraction_gain);
    assert(first_execution.glass_effect_materialize_wave_strength == 1.0f);
    assert(!first_execution.shape_blend_execution);
    assert(std::string_view(first_execution.execution_policy)
           == "glass-effect-materialize");

    assert(second_execution.active);
    assert(second_execution.glass_effect_surface_count == 2u);
    assert(!second_execution.glass_effect_match_execution);
    assert(second_execution.glass_effect_materialize_execution);
    assert(!second_execution.glass_effect_match_source_valid);
    assert(second_execution.glass_effect_match_blend_strength == 0.0f);
    assert(second_execution.glass_effect_materialize_opacity_gain == 0.5f);
    assert(second_execution.glass_effect_materialize_optical_gain
           < records[1].plan.transition.optical_gain);
    assert(second_execution.glass_effect_materialize_shadow_gain
           < records[1].plan.transition.shadow_gain);
    assert(second_execution.glass_effect_materialize_refraction_gain
           < records[1].plan.transition.refraction_gain);
    assert(second_execution.glass_effect_materialize_wave_strength == 1.0f);
    assert(!second_execution.shape_blend_execution);
    assert(std::string_view(second_execution.execution_policy)
           == "glass-effect-materialize");

    auto const first_transition =
        material_execution_transition(records[0].plan.transition,
                                      &first_execution);
    assert(first_transition.materialize);
    assert(!first_transition.matched_geometry);
    assert(std::string_view(first_transition.policy) == "materialize-in");
    assert(first_transition.opacity_gain == 0.5f);
    assert(first_execution.glass_effect_materialize_opacity_gain
           == first_transition.opacity_gain);
    assert(first_execution.glass_effect_materialize_optical_gain
           == first_transition.optical_gain);
    assert(first_execution.glass_effect_materialize_shadow_gain
           == first_transition.shadow_gain);
    assert(first_execution.glass_effect_materialize_refraction_gain
           == first_transition.refraction_gain);
    assert(first_transition.materialize_lensing_gain > 1.23f);

    auto const first_base_geometry =
        material_surface_execution_geometry(records[0].plan);
    auto const first_materialize_geometry =
        material_surface_execution_geometry(records[0].plan, &first_execution);
    assert(first_base_geometry.active);
    assert(first_materialize_geometry.active);
    assert(first_materialize_geometry.w < first_base_geometry.w);
    assert(first_materialize_geometry.h < first_base_geometry.h);
    assert(first_materialize_geometry.radius < first_base_geometry.radius);

    std::puts("PASS: glass effect matched geometry respects container spacing");
}

void test_glass_effect_matched_geometry_uses_nearby_namespace_source() {
    auto source = clear_request();
    source.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 12.0f};
    source.style.role = MaterialSurfaceRole::Control;
    source.style.tint = Color{64, 156, 255, 178};
    source.style.prominence = MaterialProminenceDescriptor{
        .enabled = true,
        .intensity = 1.0f,
    };
    source.style.container = MaterialContainerDescriptor{
        .container_id = 91u,
        .union_id = 0u,
        .spacing = 128.0f,
        .interactive = false,
        .morph_transitions = true,
    };
    source.style.transition = MaterialTransitionDescriptor{};
    source.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 33u,
        .effect_id = 801u,
    };

    auto target = regular_request();
    target.geometry = MaterialGeometry{120.0f, 0.0f, 40.0f, 40.0f, 24.0f};
    target.style.container = source.style.container;
    target.style.transition = MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::MatchedGeometry,
        .progress = 0.5f,
        .appearing = true,
    };
    target.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 33u,
        .effect_id = 802u,
    };

    auto env = sampled_environment();
    env.backdrop.luma_min = 0.70f;
    env.backdrop.luma_max = 0.98f;
    env.backdrop.luma_mean = 0.90f;
    env.backdrop.color_mean = Color{236, 244, 255, 255};

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(source, env), 1u},
        {plan_material_surface(target, env), 2u},
    };
    assert(records[0].plan.spectral_tint.tint_driven);
    assert(records[0].plan.prominent_glass.active);
    assert(records[0].plan.clear_glass_legibility.active);
    assert(!records[1].plan.prominent_glass.active);
    assert(!records[1].plan.clear_glass_legibility.active);

    auto const source_execution =
        material_container_execution_descriptor(records[0], records);
    auto const target_execution =
        material_container_execution_descriptor(records[1], records);

    assert(!source_execution.glass_effect_match_execution);
    assert(source_execution.glass_effect_surface_count == 0u);

    assert(target_execution.glass_effect_match_execution);
    assert(target_execution.glass_namespace_id == 33u);
    assert(target_execution.glass_effect_id == 802u);
    assert(target_execution.glass_effect_surface_count == 2u);
    assert(target_execution.glass_effect_match_source_valid);
    assert(!target_execution.glass_effect_match_source_effect_id_match);
    assert(target_execution.glass_effect_match_source_x == 0.0f);
    assert(target_execution.glass_effect_match_source_radius == 12.0f);
    assert(std::fabs(target_execution.glass_effect_match_source_gap - 80.0f)
           < 0.0001f);
    assert(std::fabs(target_execution.glass_effect_match_source_spacing - 128.0f)
           < 0.0001f);
    assert(std::fabs(target_execution.glass_effect_match_rect_x - 60.0f)
           < 0.0001f);
    assert(std::fabs(target_execution.glass_effect_match_rect_radius - 18.0f)
           < 0.0001f);
    assert(target_execution.glass_effect_match_appearance_active);
    assert(target_execution.glass_effect_match_appearance_source_command_index
           == 1u);
    assert(std::fabs(target_execution.glass_effect_match_appearance_blend
                     - 0.5f)
           < 0.0001f);
    assert(target_execution.glass_effect_match_appearance_tint_active);
    assert(std::fabs(
               target_execution.glass_effect_match_appearance_tint_r
                   - static_cast<float>(records[0].plan.tint.r) / 255.0f)
           < 0.0001f);
    assert(std::fabs(
               target_execution.glass_effect_match_appearance_tint_g
                   - static_cast<float>(records[0].plan.tint.g) / 255.0f)
           < 0.0001f);
    assert(std::fabs(
               target_execution.glass_effect_match_appearance_tint_b
                   - static_cast<float>(records[0].plan.tint.b) / 255.0f)
           < 0.0001f);
    assert(std::fabs(
               target_execution.glass_effect_match_appearance_tint_a
                   - static_cast<float>(records[0].plan.tint.a) / 255.0f)
           < 0.0001f);
    assert(target_execution
               .glass_effect_match_appearance_spectral_tint_active);
    assert(target_execution
               .glass_effect_match_appearance_spectral_tint_coolness
           == records[0].plan.spectral_tint.coolness);
    assert(target_execution
               .glass_effect_match_appearance_spectral_tint_dispersion
           == records[0].plan.spectral_tint.dispersion);
    assert(target_execution
               .glass_effect_match_appearance_prominent_glass_active);
    assert(target_execution
               .glass_effect_match_appearance_prominent_glass_intensity
           == records[0].plan.prominent_glass.intensity);
    assert(target_execution
               .glass_effect_match_appearance_prominent_glass_lensing_gain
           == records[0].plan.prominent_glass.lensing_gain);
    assert(target_execution
               .glass_effect_match_appearance_clear_glass_active);
    assert(target_execution.glass_effect_match_appearance_clear_glass_dimming
           == records[0].plan.clear_glass_legibility.dimming_strength);
    assert(target_execution.glass_effect_match_appearance_clear_glass_contrast
           == records[0].plan.clear_glass_legibility.contrast_lift);
    assert(target_execution.glass_effect_match_appearance_refraction_active);
    assert(target_execution.glass_effect_match_appearance_refraction_strength
           == records[0].plan.refraction.strength);
    assert(target_execution.glass_effect_match_appearance_refraction_edge_bias
           == records[0].plan.refraction.edge_bias);
    assert(target_execution
               .glass_effect_match_appearance_refraction_max_offset_pixels
           == records[0].plan.refraction.max_offset_pixels);
    assert(target_execution
               .glass_effect_match_appearance_refraction_edge_caustic_intensity
           == records[0].plan.refraction.edge_caustic_intensity);
    assert(target_execution
               .glass_effect_match_appearance_dynamic_lighting_active);
    assert(target_execution
               .glass_effect_match_appearance_dynamic_light_direction_x
           == records[0].plan.dynamic_lighting.direction_x);
    assert(target_execution
               .glass_effect_match_appearance_dynamic_light_direction_y
           == records[0].plan.dynamic_lighting.direction_y);
    assert(target_execution
               .glass_effect_match_appearance_dynamic_light_highlight
           == records[0].plan.dynamic_lighting.highlight_strength);
    assert(target_execution.glass_effect_match_appearance_dynamic_light_shadow
           == records[0].plan.dynamic_lighting.shadow_strength);
    assert(target_execution
               .glass_effect_match_appearance_glass_thickness_active);
    assert(target_execution.glass_effect_match_appearance_glass_thickness
           == records[0].plan.glass_thickness.thickness);
    assert(target_execution.glass_effect_match_appearance_glass_lensing_gain
           == records[0].plan.glass_thickness.lensing_gain);
    assert(target_execution.glass_effect_match_appearance_glass_shadow_gain
           == records[0].plan.glass_thickness.shadow_gain);
    assert(target_execution.glass_effect_match_appearance_glass_scattering_gain
           == records[0].plan.glass_thickness.scattering_gain);
    assert(target_execution
               .glass_effect_match_appearance_glass_dispersion_active);
    assert(target_execution
               .glass_effect_match_appearance_glass_dispersion_axial_offset
           == records[0].plan.glass_dispersion.axial_offset_pixels);
    assert(target_execution
               .glass_effect_match_appearance_glass_dispersion_tangential_offset
           == records[0].plan.glass_dispersion.tangential_offset_pixels);
    assert(target_execution
               .glass_effect_match_appearance_glass_dispersion_prismatic_gain
           == records[0].plan.glass_dispersion.prismatic_gain);
    assert(target_execution
               .glass_effect_match_appearance_glass_dispersion_caustic_spread
           == records[0].plan.glass_dispersion.caustic_spread);
    assert(!target_execution.group_appearance_source_valid);
    assert(std::string_view{target_execution.execution_policy}
           == "glass-effect-matched-geometry");

    auto const target_motion =
        material_glass_effect_match_motion_optics(target_execution);
    assert(target_motion.active);
    assert(std::fabs(target_motion.strength - 0.390625f) < 0.0001f);
    assert(target_motion.refraction_gain < 1.18f);
    assert(target_motion.specular_intensity_gain < 1.22f);

    std::puts("PASS: glass effect matched geometry uses nearby namespace source");
}

void test_glass_effect_matched_geometry_prefers_effect_id_source() {
    auto target = regular_request();
    target.geometry = MaterialGeometry{120.0f, 0.0f, 40.0f, 40.0f, 20.0f};
    target.style.container = MaterialContainerDescriptor{
        .container_id = 94u,
        .union_id = 0u,
        .spacing = 128.0f,
        .interactive = false,
        .morph_transitions = true,
    };
    target.style.transition = MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::MatchedGeometry,
        .progress = 0.5f,
        .appearing = true,
    };
    target.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 44u,
        .effect_id = 944u,
    };

    auto same_id_source = regular_request();
    same_id_source.geometry =
        MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 12.0f};
    same_id_source.style.container = target.style.container;
    same_id_source.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 44u,
        .effect_id = 944u,
    };

    auto nearby_different_id = regular_request();
    nearby_different_id.geometry =
        MaterialGeometry{70.0f, 0.0f, 40.0f, 40.0f, 28.0f};
    nearby_different_id.style.container = target.style.container;
    nearby_different_id.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 44u,
        .effect_id = 945u,
    };

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(same_id_source, sampled_environment()), 1u},
        {plan_material_surface(nearby_different_id, sampled_environment()), 2u},
        {plan_material_surface(target, sampled_environment()), 3u},
    };

    auto const target_execution =
        material_container_execution_descriptor(records[2], records);
    assert(target_execution.glass_effect_match_execution);
    assert(target_execution.glass_effect_match_source_valid);
    assert(target_execution.glass_effect_match_source_effect_id_match);
    assert(target_execution.glass_effect_match_source_x == 0.0f);
    assert(target_execution.glass_effect_match_source_radius == 12.0f);
    assert(std::fabs(target_execution.glass_effect_match_source_gap - 80.0f)
           < 0.0001f);
    assert(std::fabs(target_execution.glass_effect_match_rect_x - 60.0f)
           < 0.0001f);
    assert(std::fabs(target_execution.glass_effect_match_rect_radius - 16.0f)
           < 0.0001f);

    auto const target_motion =
        material_glass_effect_match_motion_optics(target_execution);
    assert(target_motion.active);
    assert(std::fabs(target_motion.strength - 0.440625f) < 0.0001f);
    assert(target_motion.refraction_gain > 1.23f);
    assert(target_motion.caustic_gain > 1.36f);
    assert(target_motion.specular_intensity_gain > 1.29f);

    std::puts(
        "PASS: glass effect matched geometry prefers effect id source");
}

void test_warmup_backdrop_access_contract() {
    auto env = sampled_environment();
    env.capabilities.frame_history = false;
    env.backdrop.available = false;
    env.backdrop.stable = false;
    env.backdrop.source = "none";

    auto plan = plan_material_surface(regular_request(), env);

    assert(!plan.backdrop_sampling);
    assert(plan.fallback());
    assert(plan.fallback_path == MaterialFallbackPath::NoBackdropSource);
    assert(plan.decision_trace.backend_supports_backdrop);
    assert(plan.decision_trace.next_frame_capture_required);
    assert(!plan.backdrop_access.required);
    assert(!plan.backdrop_access.stable_required);
    assert(!plan.backdrop_access.frame_history_required);
    assert(plan.backdrop_access.shared_frame_capture);
    assert(plan.backdrop_access.next_frame_capture_required);
    assert(plan.backdrop_access.excludes_foreground_text);
    assert(std::string_view(plan.backdrop_access.source)
        == "previous-presented-frame");
    assert(std::string_view(plan.backdrop_access.capture_scope)
        == "shared-frame");
    assert(std::string_view(plan.backdrop_access.capture_reason)
        == "warmup-next-frame");
    assert(plan.backdrop_access.max_frame_capture_count == 1);
    assert(plan.backdrop_access.max_frame_capture_pixels == 320 * 240);
    assert(plan.backdrop_access.max_surface_sample_pixels == 0);
    assert(plan.resource_budget.max_frame_capture_count == 1);
    assert(plan.resource_budget.max_frame_capture_pixels == 320 * 240);
    assert(plan.resource_budget.max_surface_sample_pixels == 0);
    assert(plan.observation_contract.shared_frame_capture_required);
    assert(plan.observation_contract.next_frame_capture_required);
    assert(plan.observation_contract.backdrop_capture_excludes_foreground_text);
    assert(std::string_view(plan.observation_contract.backdrop_capture_scope)
        == "shared-frame");
    assert(std::string_view(plan.observation_contract.backdrop_capture_reason)
        == "warmup-next-frame");
    assert(std::string_view(plan.optical_composition.stage_order)
        == "shadow-primary-edge");
    assert(std::string_view(plan.optical_composition.backdrop_capture_policy)
        == "warmup-next-frame");
    assert(std::string_view(plan.optical_composition.foreground_sampling_policy)
        == "foreground-excluded-from-warmup");
    assert(plan.optical_composition.backdrop_capture_required);
    assert(plan.optical_composition.foreground_excluded_from_backdrop);
    assert(plan.optical_composition.stage_order_stable);
    std::puts("PASS: warmup backdrop access contract");
}

void test_surface_sample_pixels_are_scaled_and_bounded() {
    auto env = sampled_environment();
    env.render_target.width = 100;
    env.render_target.height = 80;
    env.render_target.scale = 2.0f;
    auto request = regular_request();
    request.geometry.w = 70.0f;
    request.geometry.h = 30.0f;

    auto plan = plan_material_surface(request, env);

    assert(plan.backdrop_sampling);
    assert(plan.render_target.pixel_count == 8'000);
    assert(plan.backdrop_access.max_frame_capture_pixels == 8'000);
    assert(plan.backdrop_access.max_surface_sample_pixels == 8'000);
    std::puts("PASS: surface sample pixels are scaled and bounded");
}

void test_executor_frame_capture_policy_contract() {
    assert(std::string_view(material_backdrop_copy_policy())
        == "copy_only_when_material_plan_requires_shared_or_next_frame_capture");

    MaterialExecutorSummary empty_summary{};
    assert(!material_executor_requires_frame_capture(empty_summary));

    MaterialExecutorSummary standard_summary{};
    accumulate_material_executor_plan_summary(
        standard_summary,
        plan_material_surface(content_request(), sampled_environment()));
    assert(standard_summary.plan_count == 1u);
    assert(standard_summary.planned_frame_capture_count == 0u);
    assert(standard_summary.planned_frame_capture_pixels == 0);
    assert(!material_executor_requires_frame_capture(standard_summary));

    auto warmup_env = sampled_environment();
    warmup_env.capabilities.frame_history = false;
    warmup_env.backdrop.available = false;
    warmup_env.backdrop.stable = false;
    warmup_env.backdrop.source = "none";
    MaterialExecutorSummary warmup_summary{};
    accumulate_material_executor_plan_summary(
        warmup_summary,
        plan_material_surface(regular_request(), warmup_env));
    assert(warmup_summary.plan_count == 1u);
    assert(warmup_summary.planned_frame_capture_count == 1u);
    assert(warmup_summary.planned_frame_capture_pixels == 320 * 240);
    assert(material_executor_requires_frame_capture(warmup_summary));

    MaterialExecutorSummary sampled_summary{};
    accumulate_material_executor_plan_summary(
        sampled_summary,
        plan_material_surface(regular_request(), sampled_environment()));
    assert(sampled_summary.planned_frame_capture_count == 1u);
    assert(sampled_summary.planned_frame_capture_pixels == 320 * 240);
    assert(material_executor_requires_frame_capture(sampled_summary));
    std::puts("PASS: executor frame capture policy contract");
}

void test_executor_sampled_status_contract() {
    MaterialExecutorSummary standard_summary{};
    accumulate_material_executor_plan_summary(
        standard_summary,
        plan_material_surface(content_request(), sampled_environment()));
    finalize_material_executor_execution_status(standard_summary);
    assert(!standard_summary.material_sampled_backdrop_upload_required);
    assert(!standard_summary.material_sampled_backdrop_draw_required);
    assert(std::string_view(standard_summary.material_upload_status)
        == "not-needed");
    assert(std::string_view(standard_summary.material_draw_status)
        == "not-needed");

    MaterialExecutorSummary pipeline_missing{};
    accumulate_material_executor_plan_summary(
        pipeline_missing,
        plan_material_surface(regular_request(), sampled_environment()));
    finalize_material_executor_execution_status(pipeline_missing);
    assert(pipeline_missing.material_sampled_backdrop_upload_required);
    assert(pipeline_missing.material_sampled_backdrop_draw_required);
    assert(!pipeline_missing.material_sampled_backdrop_uploaded);
    assert(!pipeline_missing.material_sampled_backdrop_drawn);
    assert(std::string_view(pipeline_missing.material_upload_status)
        == "skipped-material-pipeline-unavailable");
    assert(std::string_view(pipeline_missing.material_draw_status)
        == "skipped-material-pipeline-unavailable");

    MaterialExecutorSummary backdrop_missing = pipeline_missing;
    backdrop_missing.material_pipeline_ready = true;
    backdrop_missing.material_upload_bytes = 64;
    finalize_material_executor_execution_status(backdrop_missing);
    assert(backdrop_missing.material_sampled_backdrop_uploaded);
    assert(std::string_view(backdrop_missing.material_upload_status)
        == "uploaded");
    assert(std::string_view(backdrop_missing.material_draw_status)
        == "skipped-material-backdrop-source-unavailable");

    MaterialExecutorSummary drawn = backdrop_missing;
    drawn.material_backdrop_source_ready = true;
    drawn.material_draw_calls = 1;
    finalize_material_executor_execution_status(drawn);
    assert(drawn.material_sampled_backdrop_drawn);
    assert(std::string_view(drawn.material_draw_status) == "drawn");

    std::puts("PASS: executor sampled status contract");
}

void test_container_group_runtime_summary_contract() {
    auto request = regular_request();
    request.style.container = MaterialContainerDescriptor{
        .container_id = 42u,
        .union_id = 0u,
        .spacing = 28.0f,
        .interactive = false,
        .morph_transitions = true,
    };
    auto union_request = request;
    union_request.style.container.union_id = 1u;
    auto fallback_request = request;
    auto interactive_request = regular_request();
    interactive_request.style.container = MaterialContainerDescriptor{
        .container_id = 77u,
        .union_id = 0u,
        .spacing = 20.0f,
        .interactive = true,
        .morph_transitions = false,
    };

    auto fallback_env = sampled_environment();
    fallback_env.capabilities.material_backdrop_blur = false;
    fallback_env.capabilities.shader_blur = false;
    fallback_env.backdrop.available = false;
    fallback_env.backdrop.stable = false;

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(union_request, sampled_environment()), 2u},
        {plan_material_surface(fallback_request, fallback_env), 3u},
        {plan_material_surface(interactive_request, sampled_environment()), 4u},
    };

    auto groups = summarize_material_container_groups(records);
    assert(groups.group_count == 2u);
    assert(groups.multi_surface_group_count == 1u);
    assert(groups.union_group_count == 1u);
    assert(groups.morph_group_count == 1u);
    assert(groups.interactive_group_count == 1u);
    assert(groups.shared_backdrop_scope_group_count == 2u);
    assert(groups.shared_capture_surface_count == 4u);
    assert(groups.shared_capture_saved_surface_count == 2u);
    assert(groups.max_shared_capture_group_surfaces == 3u);
    assert(groups.shape_blend_execution_group_count == 1u);
    assert(groups.shape_blend_execution_surface_count == 3u);
    assert(groups.fallback_mixed_group_count == 1u);
    assert(groups.max_group_size == 3u);
    assert(groups.max_active_surfaces == 3u);
    assert(groups.max_sampled_backdrop_surfaces == 2u);
    assert(groups.max_fallback_surfaces == 1u);
    assert(groups.total_shape_pair_count == 3u);
    assert(groups.blend_candidate_pair_count == 3u);
    assert(groups.union_candidate_pair_count == 0u);
    assert(groups.morph_candidate_pair_count == 3u);
    assert(groups.separated_pair_count == 0u);
    assert(std::fabs(groups.min_shape_gap) < 0.0001f);
    assert(std::fabs(groups.max_shape_gap) < 0.0001f);
    assert(groups.max_blend_distance == 28.0f);
    assert(groups.max_group_bounds_width == 240.0f);
    assert(groups.max_group_bounds_height == 96.0f);
    assert(groups.max_group_bounds_area == 240.0f * 96.0f);
    assert(groups.max_shape_blend_strength == 1.0f);
    auto container_group = accumulate_material_container_group(records, 42u);
    assert(material_container_group_inner_edge_alpha_blend_strength(
               container_group) == 1.0f);

    auto execution =
        material_container_execution_descriptor(records[0], records);
    assert(execution.command_index == 1u);
    assert(execution.container_id == 42u);
    assert(execution.surface_count == 3u);
    assert(execution.active);
    assert(execution.group_bounds_valid);
    assert(execution.shape_blend_execution);
    assert(!execution.union_execution);
    assert(execution.morph_execution);
    assert(execution.shared_backdrop_scope);
    assert(std::string_view(execution.execution_policy)
           == "group-edge-continuity");
    assert(execution.group_x == 12.0f);
    assert(execution.group_y == 20.0f);
    assert(execution.group_w == 240.0f);
    assert(execution.group_h == 96.0f);
    assert(execution.shape_blend_strength == 1.0f);
    assert(execution.inner_edge_alpha_blend_strength == 1.0f);

    MaterialRuntimeSummary runtime_summary{};
    for (auto const& record : records)
        accumulate_material_runtime_summary(runtime_summary, record);
    finalize_material_runtime_summary(runtime_summary, records);
    assert(runtime_summary.container_groups.group_count == groups.group_count);
    assert(runtime_summary.container_groups.max_group_size == groups.max_group_size);
    assert(runtime_summary.container_groups.total_shape_pair_count
           == groups.total_shape_pair_count);
    assert(runtime_summary.container_groups.blend_candidate_pair_count
           == groups.blend_candidate_pair_count);
    assert(runtime_summary.container_groups.shared_capture_surface_count
           == groups.shared_capture_surface_count);
    assert(runtime_summary.container_groups.shared_capture_saved_surface_count
           == groups.shared_capture_saved_surface_count);
    assert(runtime_summary.container_groups.max_shared_capture_group_surfaces
           == groups.max_shared_capture_group_surfaces);
    assert(runtime_summary.container_groups.shape_blend_execution_group_count
           == groups.shape_blend_execution_group_count);
    assert(runtime_summary.container_groups.shape_blend_execution_surface_count
           == groups.shape_blend_execution_surface_count);
    assert(runtime_summary.container_groups.max_group_bounds_area
           == groups.max_group_bounds_area);
    assert(runtime_summary.container_groups.max_shape_blend_strength
           == groups.max_shape_blend_strength);
    assert(runtime_summary.max_saturation >= 1.0f);
    assert(runtime_summary.max_edge_highlight > 0.0f);
    assert(runtime_summary.max_edge_width > 0.0f);
    assert(runtime_summary.max_noise_opacity > 0.0f);
    assert(runtime_summary.max_shadow_alpha > 0.0f);
    assert(runtime_summary.max_shadow_radius > 0.0f);
    assert(runtime_summary.total_paint_layers == 3u);
    assert(runtime_summary.active_paint_layers == 3u);
    assert(runtime_summary.shadow_paint_layers == 1u);
    assert(runtime_summary.fill_paint_layers == 1u);
    assert(runtime_summary.edge_paint_layers == 1u);
    assert(runtime_summary.max_paint_layer_count == 3u);
    assert(runtime_summary.max_paint_layers == material_max_paint_layers);
    assert(runtime_summary.max_paint_layer_inflate > 0.0f);
    assert(runtime_summary.stage_order_match_count == records.size());
    assert(runtime_summary.stage_order_mismatch_count == 0u);
    assert(std::string_view(runtime_summary.first_stage_order_mismatch)
           == "none");

    MaterialExecutorSummary executor_summary{};
    for (auto const& record : records)
        accumulate_material_executor_plan_summary(executor_summary, record.plan);
    finalize_material_executor_summary(executor_summary, records);
    assert(executor_summary.container_groups.group_count == groups.group_count);
    assert(executor_summary.container_groups.fallback_mixed_group_count == 1u);
    assert(executor_summary.container_groups.union_candidate_pair_count == 0u);
    assert(executor_summary.container_groups.morph_candidate_pair_count == 3u);
    assert(executor_summary.container_groups.shared_capture_surface_count == 4u);
    assert(executor_summary.container_groups.shared_capture_saved_surface_count == 2u);
    assert(executor_summary.container_groups.max_shared_capture_group_surfaces == 3u);
    assert(executor_summary.container_groups.shape_blend_execution_group_count == 1u);
    assert(executor_summary.container_groups.shape_blend_execution_surface_count == 3u);
    assert(executor_summary.container_groups.max_shape_blend_strength == 1.0f);
    assert(executor_summary.paint_layer_count == 3u);
    assert(executor_summary.active_paint_layer_count == 3u);
    assert(executor_summary.shadow_paint_layer_count == 1u);
    assert(executor_summary.fill_paint_layer_count == 1u);
    assert(executor_summary.edge_paint_layer_count == 1u);
    assert(executor_summary.max_paint_layer_inflate
           == runtime_summary.max_paint_layer_inflate);
    assert(executor_summary.stage_order_match_count == records.size());
    assert(executor_summary.stage_order_mismatch_count == 0u);
    assert(std::string_view(executor_summary.first_stage_order_mismatch)
           == "none");
    assert(runtime_summary.execution_contract_satisfied_count == records.size());
    assert(runtime_summary.execution_contract_mismatch_count == 0u);
    assert(runtime_summary.execution_contract_mismatch_total == 0u);
    assert(std::string_view(runtime_summary.first_execution_contract_mismatch)
           == "none");
    assert(executor_summary.execution_contract_satisfied_count == records.size());
    assert(executor_summary.execution_contract_mismatch_count == 0u);
    assert(executor_summary.execution_contract_mismatch_total == 0u);
    assert(std::string_view(executor_summary.first_execution_contract_mismatch)
           == "none");
    std::puts("PASS: container group runtime summary contract");
}

void test_container_member_shape_blend_uses_spacing_falloff() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 16.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 900u,
        .union_id = 0u,
        .spacing = 20.0f,
        .interactive = false,
        .morph_transitions = true,
    };
    auto nearby = request;
    nearby.geometry.x = 50.0f;
    auto distant = request;
    distant.geometry.x = 200.0f;

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(nearby, sampled_environment()), 2u},
        {plan_material_surface(distant, sampled_environment()), 3u},
    };
    auto const group = accumulate_material_container_group(records, 900u);
    assert(group.shape_pair_count == 3u);
    assert(group.blend_candidate_pair_count == 1u);
    assert(group.separated_pair_count == 2u);
    assert(std::fabs(group.min_shape_gap - 10.0f) < 0.0001f);
    assert(material_container_group_shape_blend_execution_active(group));
    assert(std::fabs(
               material_container_group_shape_blend_strength(group) - 0.5f)
           < 0.0001f);
    assert(material_container_group_shape_blend_surface_count(records, group)
           == 2u);

    auto first_execution =
        material_container_execution_descriptor(records[0], records);
    auto second_execution =
        material_container_execution_descriptor(records[1], records);
    auto distant_execution =
        material_container_execution_descriptor(records[2], records);
    assert(std::fabs(first_execution.shape_blend_strength - 0.5f)
           < 0.0001f);
    assert(std::fabs(second_execution.shape_blend_strength - 0.5f)
           < 0.0001f);
    assert(std::fabs(distant_execution.shape_blend_strength) < 0.0001f);
    assert(std::fabs(distant_execution.inner_edge_alpha_blend_strength)
           < 0.0001f);
    assert(first_execution.group_surface_execution);
    assert(second_execution.group_surface_execution);
    assert(!distant_execution.group_surface_execution);
    assert(std::string_view(first_execution.fusion_model)
           == "proximity-liquid-glass-fusion");
    assert(first_execution.fusion_optics_active);
    assert(first_execution.fusion_strength > 0.45f);
    assert(first_execution.fusion_strength < 0.50f);
    assert(first_execution.fusion_lensing_gain > 1.0f);
    assert(first_execution.fusion_edge_lift > 0.0f);
    assert(first_execution.fusion_shadow_gain > 1.0f);
    assert(!distant_execution.fusion_optics_active);
    assert(std::string_view(distant_execution.fusion_model) == "none");

    auto const first_bridge_motion =
        material_container_bridge_motion_optics(
            records[0],
            records,
            first_execution);
    auto const second_bridge_motion =
        material_container_bridge_motion_optics(
            records[1],
            records,
            second_execution);
    auto const distant_bridge_motion =
        material_container_bridge_motion_optics(
            records[2],
            records,
            distant_execution);
    assert(first_bridge_motion.active);
    assert(first_bridge_motion.strength > 0.35f);
    assert(first_bridge_motion.direction_x > 0.999f);
    assert(std::fabs(first_bridge_motion.direction_y) < 0.0001f);
    assert(std::fabs(first_bridge_motion.specular_anchor_x - 0.5f)
           < 0.0001f);
    assert(std::fabs(first_bridge_motion.specular_anchor_y - 0.5f)
           < 0.0001f);
    assert(first_bridge_motion.refraction_gain > 1.0f);
    assert(first_bridge_motion.caustic_gain
           > first_bridge_motion.refraction_gain);
    assert(first_bridge_motion.specular_intensity_gain > 1.0f);
    assert(first_bridge_motion.flow_offset_gain > 0.0f);
    assert(first_bridge_motion.ribbon_width > 0.10f);
    assert(first_bridge_motion.highlight_gain > 0.0f);
    assert(!second_bridge_motion.active);
    assert(!distant_bridge_motion.active);
    assert(first_execution.surface_leader);
    assert(!second_execution.surface_leader);
    assert(first_execution.group_bounds_valid);
    assert(first_execution.group_x == 0.0f);
    assert(first_execution.group_y == 0.0f);
    assert(first_execution.group_w == 90.0f);
    assert(first_execution.group_h == 40.0f);
    assert(first_execution.group_radius == 16.0f);

    auto const first_surface_geometry =
        material_surface_execution_geometry(records[0].plan, &first_execution);
    auto const second_surface_geometry =
        material_surface_execution_geometry(records[1].plan, &second_execution);
    auto const distant_surface_geometry =
        material_surface_execution_geometry(records[2].plan, &distant_execution);
    assert(first_surface_geometry.active);
    assert(first_surface_geometry.x == 0.0f);
    assert(first_surface_geometry.w == 90.0f);
    assert(first_surface_geometry.h == 40.0f);
    assert(first_surface_geometry.radius == 16.0f);
    assert(!second_surface_geometry.active);
    assert(distant_surface_geometry.active);
    assert(distant_surface_geometry.x == 200.0f);
    assert(distant_surface_geometry.w == 40.0f);

    MaterialRuntimeSummary runtime_summary{};
    for (auto const& record : records)
        accumulate_material_runtime_summary(runtime_summary, record);
    finalize_material_runtime_summary(runtime_summary, records);
    assert(runtime_summary.total_surface_sample_pixels == 90 * 40 + 40 * 40);
    assert(runtime_summary.max_surface_sample_pixels == 90 * 40);

    auto summary = summarize_material_container_groups(records);
    assert(summary.shape_blend_execution_group_count == 1u);
    assert(summary.shape_blend_execution_surface_count == 2u);
    assert(std::fabs(summary.max_shape_blend_strength - 0.5f) < 0.0001f);
    std::puts("PASS: container member shape blend uses spacing falloff");
}

void test_container_group_surface_adds_overlap_response() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 16.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 922u,
        .union_id = 0u,
        .spacing = 20.0f,
        .interactive = false,
        .morph_transitions = true,
    };
    auto overlapping = request;
    overlapping.geometry.x = 30.0f;

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(overlapping, sampled_environment()), 2u},
    };

    auto const group = accumulate_material_container_group(records, 922u);
    assert(group.shape_pair_count == 1u);
    assert(group.blend_candidate_pair_count == 1u);
    assert(group.overlap_pair_count == 1u);
    assert(std::fabs(group.max_shape_overlap - 0.25f) < 0.0001f);
    assert(std::fabs(material_container_overlap_density(group) - 1.0f)
           < 0.0001f);
    auto const overlap_strength =
        material_container_overlap_response_strength(group);
    assert(overlap_strength > 0.53f);
    assert(overlap_strength < 0.55f);

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const second_execution =
        material_container_execution_descriptor(records[1], records);
    assert(first_execution.group_surface_execution);
    assert(second_execution.group_surface_execution);
    assert(first_execution.surface_leader);
    assert(!second_execution.surface_leader);
    assert(first_execution.overlap_response_active);
    assert(first_execution.overlap_pair_count == 1u);
    assert(std::fabs(
               first_execution.overlap_max_fraction
                   - group.max_shape_overlap)
           < 0.0001f);
    assert(std::fabs(first_execution.overlap_density - 1.0f) < 0.0001f);
    assert(std::fabs(
               first_execution.overlap_response_strength
                   - overlap_strength)
           < 0.0001f);

    auto const first_surface_geometry =
        material_surface_execution_geometry(records[0].plan, &first_execution);
    auto const second_surface_geometry =
        material_surface_execution_geometry(records[1].plan, &second_execution);
    assert(first_surface_geometry.active);
    assert(first_surface_geometry.w == 70.0f);
    assert(first_surface_geometry.h == 40.0f);
    assert(!second_surface_geometry.active);

    std::puts("PASS: container group surface adds overlap response");
}

void test_container_group_surface_preserves_radius_continuity() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 8.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 906u,
        .union_id = 0u,
        .spacing = 20.0f,
        .interactive = false,
        .morph_transitions = true,
    };
    auto capsule = request;
    capsule.geometry.x = 50.0f;
    capsule.geometry.radius = 20.0f;

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(capsule, sampled_environment()), 2u},
    };

    auto const group = accumulate_material_container_group(records, 906u);
    assert(group.has_bounds);
    assert(group.max_effective_radius == 20.0f);
    assert(material_container_group_execution_radius(group) == 20.0f);

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const second_execution =
        material_container_execution_descriptor(records[1], records);
    assert(first_execution.group_surface_execution);
    assert(second_execution.group_surface_execution);
    assert(first_execution.surface_leader);
    assert(!second_execution.surface_leader);
    assert(first_execution.group_x == 0.0f);
    assert(first_execution.group_w == 90.0f);
    assert(first_execution.group_h == 40.0f);
    assert(first_execution.group_radius == 20.0f);
    assert(second_execution.group_radius == 20.0f);

    auto const first_surface_geometry =
        material_surface_execution_geometry(records[0].plan, &first_execution);
    auto const second_surface_geometry =
        material_surface_execution_geometry(records[1].plan, &second_execution);
    assert(first_surface_geometry.active);
    assert(first_surface_geometry.w == 90.0f);
    assert(first_surface_geometry.h == 40.0f);
    assert(first_surface_geometry.radius == 20.0f);
    assert(!second_surface_geometry.active);

    std::puts("PASS: container group surface preserves radius continuity");
}

void test_container_group_surface_aggregates_member_interaction() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 16.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 907u,
        .union_id = 0u,
        .spacing = 20.0f,
        .interactive = true,
        .morph_transitions = true,
    };

    auto peer = request;
    peer.geometry.x = 50.0f;
    peer.style.role = MaterialSurfaceRole::Control;
    peer.style.tint = Color{64, 156, 255, 178};
    peer.style.prominence = MaterialProminenceDescriptor{
        .enabled = true,
        .intensity = 1.0f,
    };
    peer.style.interaction = MaterialInteractionDescriptor{
        .hovered = true,
        .pressed = false,
        .focused = false,
        .pointer_inside = true,
        .pointer_x = 0.75f,
        .pointer_y = 0.25f,
    };

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(peer, sampled_environment()), 2u},
    };
    assert(!records[0].plan.specular.interaction_driven);
    assert(!records[0].plan.interaction.pointer_lens_active);
    assert(records[1].plan.specular.interaction_driven);
    assert(records[1].plan.interaction.pointer_lens_active);
    assert(records[1].plan.refraction.interaction_driven);
    assert(records[1].plan.dynamic_lighting.interaction_driven);
    assert(records[1].plan.glass_thickness.interaction_driven);
    assert(records[1].plan.glass_dispersion.interaction_driven);
    assert(records[1].plan.spectral_tint.tint_driven);
    assert(records[1].plan.prominent_glass.active);

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const peer_execution =
        material_container_execution_descriptor(records[1], records);
    assert(first_execution.group_surface_execution);
    assert(peer_execution.group_surface_execution);
    assert(first_execution.surface_leader);
    assert(!peer_execution.surface_leader);
    assert(first_execution.group_interaction_source_valid);
    assert(first_execution.group_interaction_source_command_index == 2u);
    assert(first_execution.group_interaction_pointer_lens_active);
    assert(first_execution.group_appearance_source_valid);
    assert(first_execution.group_appearance_source_command_index == 2u);
    assert(first_execution.group_appearance_tint_active);
    assert(std::fabs(
               first_execution.group_appearance_tint_r
                   - static_cast<float>(peer.style.tint.r) / 255.0f)
           < 0.0001f);
    assert(std::fabs(
               first_execution.group_appearance_tint_g
                   - static_cast<float>(peer.style.tint.g) / 255.0f)
           < 0.0001f);
    assert(std::fabs(
               first_execution.group_appearance_tint_b
                   - static_cast<float>(peer.style.tint.b) / 255.0f)
           < 0.0001f);
    assert(std::fabs(
               first_execution.group_appearance_tint_a
                   - static_cast<float>(peer.style.tint.a) / 255.0f)
           < 0.0001f);
    assert(first_execution.group_appearance_spectral_tint_active);
    assert(first_execution.group_appearance_spectral_tint_warmth
           == records[1].plan.spectral_tint.warmth);
    assert(first_execution.group_appearance_spectral_tint_coolness
           == records[1].plan.spectral_tint.coolness);
    assert(first_execution.group_appearance_spectral_tint_dispersion
           == records[1].plan.spectral_tint.dispersion);
    assert(first_execution.group_appearance_spectral_tint_rim
           == records[1].plan.spectral_tint.rim_tint);
    assert(first_execution.group_appearance_prominent_glass_active);
    assert(first_execution.group_appearance_prominent_glass_intensity
           == records[1].plan.prominent_glass.intensity);
    assert(first_execution.group_appearance_prominent_glass_tint_weight
           == records[1].plan.prominent_glass.tint_weight);
    assert(first_execution.group_appearance_prominent_glass_edge_lift
           == records[1].plan.prominent_glass.edge_lift);
    assert(first_execution.group_appearance_prominent_glass_lensing_gain
           == records[1].plan.prominent_glass.lensing_gain);

    auto const leader_geometry =
        material_surface_execution_geometry(records[0].plan, &first_execution);
    assert(leader_geometry.active);
    auto const expected_specular_x =
        material_surface_execution_anchor_x(
            records[1].plan,
            leader_geometry,
            records[1].plan.specular.anchor_x);
    auto const expected_specular_y =
        material_surface_execution_anchor_y(
            records[1].plan,
            leader_geometry,
            records[1].plan.specular.anchor_y);
    auto const expected_lens_x =
        material_surface_execution_anchor_x(
            records[1].plan,
            leader_geometry,
            records[1].plan.interaction.pointer_lens_anchor_x);
    auto const expected_lens_y =
        material_surface_execution_anchor_y(
            records[1].plan,
            leader_geometry,
            records[1].plan.interaction.pointer_lens_anchor_y);
    assert(std::fabs(
               first_execution.group_interaction_specular_anchor_x
                   - expected_specular_x)
           < 0.0001f);
    assert(std::fabs(
               first_execution.group_interaction_specular_anchor_y
                   - expected_specular_y)
           < 0.0001f);
    assert(std::fabs(
               first_execution.group_interaction_pointer_lens_anchor_x
                   - expected_lens_x)
           < 0.0001f);
    assert(std::fabs(
               first_execution.group_interaction_pointer_lens_anchor_y
                   - expected_lens_y)
           < 0.0001f);
    assert(first_execution.group_interaction_specular_anchor_x > 0.85f);
    assert(first_execution.group_interaction_specular_anchor_y < 0.30f);
    assert(first_execution.group_interaction_specular_radius
           == records[1].plan.specular.radius);
    assert(first_execution.group_interaction_specular_intensity
           == records[1].plan.specular.intensity);
    assert(first_execution.group_interaction_pointer_lens_radius
           == records[1].plan.interaction.pointer_lens_radius);
    assert(first_execution.group_interaction_pointer_lens_strength
           == records[1].plan.interaction.pointer_lens_strength);
    assert(first_execution.group_interaction_control_morph_active);
    assert(first_execution.group_interaction_control_morph_scale_delta
           == records[1].plan.interaction.control_morph_scale_delta);
    assert(first_execution.group_interaction_control_morph_depth
           == records[1].plan.interaction.control_morph_depth);
    assert(first_execution.group_interaction_control_morph_edge
           == records[1].plan.interaction.control_morph_edge);
    assert(first_execution.group_interaction_control_morph_shadow
           == records[1].plan.interaction.control_morph_shadow);
    assert(first_execution.group_interaction_refraction_active);
    assert(first_execution.group_interaction_refraction_strength
           == records[1].plan.refraction.strength);
    assert(first_execution.group_interaction_refraction_edge_bias
           == records[1].plan.refraction.edge_bias);
    assert(first_execution.group_interaction_refraction_max_offset_pixels
           == records[1].plan.refraction.max_offset_pixels);
    assert(first_execution.group_interaction_refraction_edge_caustic_intensity
           == records[1].plan.refraction.edge_caustic_intensity);
    assert(first_execution.group_interaction_dynamic_lighting_active);
    assert(first_execution.group_interaction_dynamic_light_direction_x
           == records[1].plan.dynamic_lighting.direction_x);
    assert(first_execution.group_interaction_dynamic_light_direction_y
           == records[1].plan.dynamic_lighting.direction_y);
    assert(first_execution.group_interaction_dynamic_light_highlight
           == records[1].plan.dynamic_lighting.highlight_strength);
    assert(first_execution.group_interaction_dynamic_light_shadow
           == records[1].plan.dynamic_lighting.shadow_strength);
    assert(first_execution.group_interaction_glass_thickness_active);
    assert(first_execution.group_interaction_glass_thickness
           == records[1].plan.glass_thickness.thickness);
    assert(first_execution.group_interaction_glass_lensing_gain
           == records[1].plan.glass_thickness.lensing_gain);
    assert(first_execution.group_interaction_glass_shadow_gain
           == records[1].plan.glass_thickness.shadow_gain);
    assert(first_execution.group_interaction_glass_scattering_gain
           == records[1].plan.glass_thickness.scattering_gain);
    assert(first_execution.group_interaction_glass_dispersion_active);
    assert(first_execution.group_interaction_glass_dispersion_axial_offset
           == records[1].plan.glass_dispersion.axial_offset_pixels);
    assert(first_execution.group_interaction_glass_dispersion_tangential_offset
           == records[1].plan.glass_dispersion.tangential_offset_pixels);
    assert(first_execution.group_interaction_glass_dispersion_prismatic_gain
           == records[1].plan.glass_dispersion.prismatic_gain);
    assert(first_execution.group_interaction_glass_dispersion_caustic_spread
           == records[1].plan.glass_dispersion.caustic_spread);

    std::puts("PASS: container group surface aggregates member interaction");
}

void test_container_group_surface_aggregates_clear_member_appearance() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 16.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 908u,
        .union_id = 0u,
        .spacing = 20.0f,
        .interactive = true,
        .morph_transitions = true,
    };

    auto peer = clear_request();
    peer.geometry = request.geometry;
    peer.geometry.x = 50.0f;
    peer.style.container = request.style.container;

    auto env = sampled_environment();
    env.backdrop.luma_min = 0.70f;
    env.backdrop.luma_max = 0.98f;
    env.backdrop.luma_mean = 0.90f;
    env.backdrop.color_mean = Color{236, 244, 255, 255};

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, env), 1u},
        {plan_material_surface(peer, env), 2u},
    };
    assert(!records[0].plan.clear_glass_legibility.active);
    assert(records[1].plan.clear_glass_legibility.active);

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const peer_execution =
        material_container_execution_descriptor(records[1], records);
    assert(first_execution.group_surface_execution);
    assert(peer_execution.group_surface_execution);
    assert(first_execution.surface_leader);
    assert(!peer_execution.surface_leader);
    assert(first_execution.group_appearance_source_valid);
    assert(first_execution.group_appearance_source_command_index == 2u);
    assert(first_execution.group_appearance_clear_glass_active);
    assert(first_execution.group_appearance_clear_glass_dimming
           == records[1].plan.clear_glass_legibility.dimming_strength);
    assert(first_execution.group_appearance_clear_glass_contrast
           == records[1].plan.clear_glass_legibility.contrast_lift);
    assert(first_execution.group_appearance_clear_glass_brightness_response
           == records[1].plan.clear_glass_legibility.brightness_response);
    assert(first_execution.group_appearance_clear_glass_detail_response
           == records[1].plan.clear_glass_legibility.detail_response);

    std::puts(
        "PASS: container group surface aggregates clear member appearance");
}

void test_container_member_fallback_paint_uses_proximity_cluster() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 16.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 905u,
        .union_id = 0u,
        .spacing = 20.0f,
        .interactive = false,
        .morph_transitions = true,
    };
    auto nearby = request;
    nearby.geometry.x = 50.0f;
    auto distant = request;
    distant.geometry.x = 200.0f;

    auto env = sampled_environment();
    env.capabilities.material_backdrop_blur = false;
    env.capabilities.shader_blur = false;
    env.capabilities.frame_history = false;
    env.backdrop.available = false;
    env.backdrop.stable = false;
    env.backdrop.source = "none";

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, env), 1u},
        {plan_material_surface(nearby, env), 2u},
        {plan_material_surface(distant, env), 3u},
    };
    assert(records[0].plan.fallback());
    assert(records[1].plan.fallback());
    assert(records[0].plan.paint_layer_count > 1u);
    assert(records[1].plan.paint_layer_count > 1u);

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const second_execution =
        material_container_execution_descriptor(records[1], records);
    auto const distant_execution =
        material_container_execution_descriptor(records[2], records);
    assert(first_execution.group_surface_execution);
    assert(second_execution.group_surface_execution);
    assert(!distant_execution.group_surface_execution);
    assert(first_execution.surface_leader);
    assert(!second_execution.surface_leader);
    assert(first_execution.paint_layer_leader);
    assert(!second_execution.paint_layer_leader);
    assert(first_execution.group_x == 0.0f);
    assert(first_execution.group_w == 90.0f);

    auto const& first_layer = records[0].plan.paint_layers[1];
    auto const first_inflate = std::max(first_layer.inflate, 0.0f);
    auto const first_geometry =
        material_paint_layer_execution_geometry(
            records[0].plan,
            first_layer,
            &first_execution);
    assert(first_geometry.active);
    assert(std::fabs(first_geometry.x
                     - (first_execution.group_x
                        + first_layer.x_offset
                        - first_inflate))
           < 0.0001f);
    assert(std::fabs(first_geometry.w
                     - (first_execution.group_w + first_inflate * 2.0f))
           < 0.0001f);

    auto const& second_layer = records[1].plan.paint_layers[1];
    auto const second_base_geometry =
        material_paint_layer_execution_geometry(records[1].plan, second_layer);
    auto const second_cluster_geometry =
        material_paint_layer_execution_geometry(
            records[1].plan,
            second_layer,
            &second_execution);
    assert(second_base_geometry.active);
    assert(!second_cluster_geometry.active);

    auto const& distant_layer = records[2].plan.paint_layers[1];
    auto const distant_geometry =
        material_paint_layer_execution_geometry(
            records[2].plan,
            distant_layer,
            &distant_execution);
    assert(distant_geometry.active);
    assert(std::fabs(distant_geometry.x - 200.0f) < 0.0001f);
    assert(std::fabs(distant_geometry.w - 40.0f) < 0.0001f);

    std::puts("PASS: container member fallback paint uses proximity cluster");
}

void test_glass_effect_union_uses_compatible_render_bounds() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 20.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 910u,
        .union_id = 1u,
        .spacing = 24.0f,
        .interactive = false,
        .morph_transitions = false,
    };
    request.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 55u,
        .effect_id = 0u,
    };

    auto peer = request;
    peer.geometry.x = 50.0f;

    auto different_union = request;
    different_union.geometry.x = 160.0f;
    different_union.style.container.union_id = 2u;

    auto different_variant = request;
    different_variant.geometry.x = 210.0f;
    different_variant.style = material_style_for_kind(MaterialKind::Clear, Theme{});
    different_variant.style.container = request.style.container;

    auto different_shape = request;
    different_shape.geometry.x = 260.0f;
    different_shape.geometry.radius = 8.0f;

    auto different_namespace = request;
    different_namespace.geometry.x = 310.0f;
    different_namespace.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 56u,
        .effect_id = 0u,
    };

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(peer, sampled_environment()), 2u},
        {plan_material_surface(different_union, sampled_environment()), 3u},
        {plan_material_surface(different_variant, sampled_environment()), 4u},
        {plan_material_surface(different_shape, sampled_environment()), 5u},
        {plan_material_surface(different_namespace, sampled_environment()), 6u},
    };

    auto const union_group =
        accumulate_material_glass_effect_union_group(records, records[0].plan);
    assert(union_group.surface_count == 2u);
    assert(union_group.shape_pair_count == 1u);
    assert(union_group.union_candidate_pair_count == 1u);
    assert(union_group.min_x == 0.0f);
    assert(union_group.max_x == 90.0f);
    assert(material_container_group_shape_blend_execution_active(union_group));

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const peer_execution =
        material_container_execution_descriptor(records[1], records);
    auto const different_union_execution =
        material_container_execution_descriptor(records[2], records);
    auto const different_variant_execution =
        material_container_execution_descriptor(records[3], records);
    auto const different_shape_execution =
        material_container_execution_descriptor(records[4], records);
    auto const different_namespace_execution =
        material_container_execution_descriptor(records[5], records);

    assert(first_execution.union_execution);
    assert(peer_execution.union_execution);
    assert(std::string_view(first_execution.execution_policy)
           == "glass-effect-union");
    assert(first_execution.group_bounds_valid);
    assert(first_execution.group_x == 0.0f);
    assert(first_execution.group_y == 0.0f);
    assert(first_execution.group_w == 90.0f);
    assert(first_execution.group_h == 40.0f);
    assert(first_execution.group_radius == 20.0f);
    assert(first_execution.shape_blend_strength == 1.0f);
    assert(std::string_view(first_execution.fusion_model)
           == "union-liquid-glass-fusion");
    assert(first_execution.fusion_optics_active);
    assert(first_execution.fusion_strength == 1.0f);
    assert(first_execution.fusion_lensing_gain > 1.17f);
    assert(first_execution.fusion_lensing_gain < 1.20f);
    assert(first_execution.fusion_edge_lift > 0.07f);
    assert(first_execution.fusion_edge_lift < 0.09f);
    assert(first_execution.fusion_shadow_gain > 1.15f);
    assert(first_execution.fusion_shadow_gain < 1.18f);

    auto const first_bridge_motion =
        material_container_bridge_motion_optics(
            records[0],
            records,
            first_execution);
    auto const peer_bridge_motion =
        material_container_bridge_motion_optics(
            records[1],
            records,
            peer_execution);
    assert(first_bridge_motion.active);
    assert(first_bridge_motion.strength > 0.99f);
    assert(first_bridge_motion.direction_x > 0.999f);
    assert(std::fabs(first_bridge_motion.direction_y) < 0.0001f);
    assert(std::fabs(first_bridge_motion.specular_anchor_x - 0.5f)
           < 0.0001f);
    assert(std::fabs(first_bridge_motion.specular_anchor_y - 0.5f)
           < 0.0001f);
    assert(first_bridge_motion.refraction_gain > 1.30f);
    assert(first_bridge_motion.caustic_gain
           > first_bridge_motion.refraction_gain);
    assert(first_bridge_motion.specular_intensity_gain > 1.40f);
    assert(first_bridge_motion.flow_offset_gain > 0.40f);
    assert(first_bridge_motion.ribbon_width > 0.25f);
    assert(first_bridge_motion.highlight_gain > 0.19f);
    assert(!peer_bridge_motion.active);
    assert(first_execution.surface_leader);
    assert(!peer_execution.surface_leader);
    assert(first_execution.paint_layer_leader);
    assert(peer_execution.paint_layer_leader);
    auto const first_surface_geometry =
        material_surface_execution_geometry(
            records[0].plan,
            &first_execution);
    auto const peer_surface_geometry =
        material_surface_execution_geometry(
            records[1].plan,
            &peer_execution);
    assert(first_surface_geometry.active);
    assert(first_surface_geometry.x == 0.0f);
    assert(first_surface_geometry.y == 0.0f);
    assert(first_surface_geometry.w == 90.0f);
    assert(first_surface_geometry.h == 40.0f);
    assert(first_surface_geometry.radius == 20.0f);
    assert(!peer_surface_geometry.active);
    assert(!different_union_execution.union_execution);
    assert(!different_variant_execution.union_execution);
    assert(!different_shape_execution.union_execution);
    assert(!different_namespace_execution.union_execution);

    std::puts("PASS: glass effect union uses compatible render bounds");
}

void test_glass_effect_union_separates_rounded_radius_variants() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 48.0f, 48.0f, 8.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 913u,
        .union_id = 1u,
        .spacing = 24.0f,
        .interactive = false,
        .morph_transitions = false,
    };
    request.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 57u,
        .effect_id = 0u,
    };

    auto peer = request;
    peer.geometry.x = 56.0f;

    auto different_radius = request;
    different_radius.geometry.x = 112.0f;
    different_radius.geometry.radius = 12.0f;

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(peer, sampled_environment()), 2u},
        {plan_material_surface(different_radius, sampled_environment()), 3u},
    };
    assert(records[0].plan.shape.kind == MaterialShapeKind::RoundedRectangle);
    assert(records[1].plan.shape.kind == MaterialShapeKind::RoundedRectangle);
    assert(records[2].plan.shape.kind == MaterialShapeKind::RoundedRectangle);
    assert(records[0].plan.shape.effective_radius
           == records[1].plan.shape.effective_radius);
    assert(records[0].plan.shape.effective_radius
           != records[2].plan.shape.effective_radius);

    auto const union_group =
        accumulate_material_glass_effect_union_group(records, records[0].plan);
    assert(union_group.surface_count == 2u);
    assert(union_group.shape_pair_count == 1u);
    assert(union_group.union_candidate_pair_count == 1u);
    assert(union_group.min_x == 0.0f);
    assert(union_group.max_x == 104.0f);

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const peer_execution =
        material_container_execution_descriptor(records[1], records);
    auto const different_radius_execution =
        material_container_execution_descriptor(records[2], records);
    assert(first_execution.union_execution);
    assert(peer_execution.union_execution);
    assert(!different_radius_execution.union_execution);
    assert(first_execution.group_w == 104.0f);
    assert(first_execution.group_radius == 8.0f);

    std::puts(
        "PASS: glass effect union separates rounded radius variants");
}

void test_glass_effect_union_aggregates_member_interaction() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 20.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 921u,
        .union_id = 1u,
        .spacing = 24.0f,
        .interactive = true,
        .morph_transitions = false,
    };

    auto peer = request;
    peer.geometry.x = 50.0f;
    peer.style.role = MaterialSurfaceRole::Control;
    peer.style.tint = Color{64, 156, 255, 178};
    peer.style.prominence = MaterialProminenceDescriptor{
        .enabled = true,
        .intensity = 1.0f,
    };
    peer.style.interaction = MaterialInteractionDescriptor{
        .hovered = true,
        .pressed = false,
        .focused = false,
        .pointer_inside = true,
        .pointer_x = 0.20f,
        .pointer_y = 0.80f,
    };

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(peer, sampled_environment()), 2u},
    };
    assert(!records[0].plan.specular.interaction_driven);
    assert(records[1].plan.specular.interaction_driven);
    assert(records[1].plan.interaction.pointer_lens_active);
    assert(records[1].plan.refraction.interaction_driven);
    assert(records[1].plan.dynamic_lighting.interaction_driven);
    assert(records[1].plan.glass_thickness.interaction_driven);
    assert(records[1].plan.glass_dispersion.interaction_driven);
    assert(records[1].plan.spectral_tint.tint_driven);
    assert(records[1].plan.prominent_glass.active);

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const peer_execution =
        material_container_execution_descriptor(records[1], records);
    assert(first_execution.union_execution);
    assert(peer_execution.union_execution);
    assert(first_execution.surface_leader);
    assert(!peer_execution.surface_leader);
    assert(first_execution.group_interaction_source_valid);
    assert(first_execution.group_interaction_source_command_index == 2u);
    assert(first_execution.group_interaction_pointer_lens_active);
    assert(first_execution.group_appearance_source_valid);
    assert(first_execution.group_appearance_source_command_index == 2u);
    assert(first_execution.group_appearance_tint_active);
    assert(std::fabs(
               first_execution.group_appearance_tint_r
                   - static_cast<float>(peer.style.tint.r) / 255.0f)
           < 0.0001f);
    assert(std::fabs(
               first_execution.group_appearance_tint_g
                   - static_cast<float>(peer.style.tint.g) / 255.0f)
           < 0.0001f);
    assert(std::fabs(
               first_execution.group_appearance_tint_b
                   - static_cast<float>(peer.style.tint.b) / 255.0f)
           < 0.0001f);
    assert(std::fabs(
               first_execution.group_appearance_tint_a
                   - static_cast<float>(peer.style.tint.a) / 255.0f)
           < 0.0001f);
    assert(first_execution.group_appearance_prominent_glass_active);
    assert(first_execution.group_appearance_prominent_glass_intensity
           == records[1].plan.prominent_glass.intensity);
    assert(first_execution.group_appearance_prominent_glass_tint_weight
           == records[1].plan.prominent_glass.tint_weight);

    auto const leader_geometry =
        material_surface_execution_geometry(records[0].plan, &first_execution);
    assert(leader_geometry.active);
    auto const expected_specular_x =
        material_surface_execution_anchor_x(
            records[1].plan,
            leader_geometry,
            records[1].plan.specular.anchor_x);
    auto const expected_specular_y =
        material_surface_execution_anchor_y(
            records[1].plan,
            leader_geometry,
            records[1].plan.specular.anchor_y);
    assert(std::fabs(
               first_execution.group_interaction_specular_anchor_x
                   - expected_specular_x)
           < 0.0001f);
    assert(std::fabs(
               first_execution.group_interaction_specular_anchor_y
                   - expected_specular_y)
           < 0.0001f);
    assert(first_execution.group_interaction_specular_anchor_x > 0.60f);
    assert(first_execution.group_interaction_specular_anchor_y > 0.75f);
    assert(first_execution.group_interaction_pointer_lens_strength
           == records[1].plan.interaction.pointer_lens_strength);
    assert(first_execution.group_interaction_control_morph_active);
    assert(first_execution.group_interaction_control_morph_scale_delta
           == records[1].plan.interaction.control_morph_scale_delta);
    assert(first_execution.group_interaction_control_morph_depth
           == records[1].plan.interaction.control_morph_depth);
    assert(first_execution.group_interaction_control_morph_edge
           == records[1].plan.interaction.control_morph_edge);
    assert(first_execution.group_interaction_control_morph_shadow
           == records[1].plan.interaction.control_morph_shadow);
    assert(first_execution.group_interaction_refraction_active);
    assert(first_execution.group_interaction_refraction_strength
           == records[1].plan.refraction.strength);
    assert(first_execution.group_interaction_dynamic_lighting_active);
    assert(first_execution.group_interaction_dynamic_light_direction_x
           == records[1].plan.dynamic_lighting.direction_x);
    assert(first_execution.group_interaction_dynamic_light_direction_y
           == records[1].plan.dynamic_lighting.direction_y);
    assert(first_execution.group_interaction_glass_thickness_active);
    assert(first_execution.group_interaction_glass_thickness
           == records[1].plan.glass_thickness.thickness);
    assert(first_execution.group_interaction_glass_dispersion_active);
    assert(first_execution.group_interaction_glass_dispersion_axial_offset
           == records[1].plan.glass_dispersion.axial_offset_pixels);
    assert(first_execution.group_interaction_glass_dispersion_prismatic_gain
           == records[1].plan.glass_dispersion.prismatic_gain);

    std::puts("PASS: glass effect union aggregates member interaction");
}

void test_glass_effect_union_combines_at_rest_without_spacing() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 20.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 912u,
        .union_id = 1u,
        .spacing = 0.0f,
        .interactive = false,
        .morph_transitions = false,
    };

    auto peer = request;
    peer.geometry.x = 160.0f;

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(peer, sampled_environment()), 2u},
    };

    auto const union_group =
        accumulate_material_glass_effect_union_group(records, records[0].plan);
    assert(union_group.surface_count == 2u);
    assert(union_group.shape_pair_count == 1u);
    assert(union_group.blend_candidate_pair_count == 0u);
    assert(union_group.union_candidate_pair_count == 0u);
    assert(material_glass_effect_union_execution_active(union_group));

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const peer_execution =
        material_container_execution_descriptor(records[1], records);
    assert(first_execution.union_execution);
    assert(peer_execution.union_execution);
    assert(first_execution.surface_leader);
    assert(!peer_execution.surface_leader);
    assert(first_execution.group_bounds_valid);
    assert(first_execution.group_x == 0.0f);
    assert(first_execution.group_y == 0.0f);
    assert(first_execution.group_w == 200.0f);
    assert(first_execution.group_h == 40.0f);
    assert(first_execution.group_radius == 20.0f);
    assert(first_execution.shape_blend_strength == 1.0f);
    assert(first_execution.inner_edge_alpha_blend_strength == 1.0f);
    assert(std::string_view(first_execution.fusion_model)
           == "union-liquid-glass-fusion");
    assert(first_execution.fusion_optics_active);
    assert(first_execution.fusion_strength == 1.0f);

    auto const first_surface_geometry =
        material_surface_execution_geometry(
            records[0].plan,
            &first_execution);
    auto const peer_surface_geometry =
        material_surface_execution_geometry(
            records[1].plan,
            &peer_execution);
    assert(first_surface_geometry.active);
    assert(first_surface_geometry.w == 200.0f);
    assert(first_surface_geometry.h == 40.0f);
    assert(first_surface_geometry.radius == 20.0f);
    assert(!peer_surface_geometry.active);

    MaterialRuntimeSummary runtime_summary{};
    for (auto const& record : records)
        accumulate_material_runtime_summary(runtime_summary, record);
    finalize_material_runtime_summary(runtime_summary, records);
    assert(runtime_summary.total_surface_sample_pixels == 200 * 40);
    assert(runtime_summary.max_surface_sample_pixels == 200 * 40);

    std::puts("PASS: glass effect union combines at rest without spacing");
}

void test_glass_effect_union_aggregates_member_optics() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 80.0f, 80.0f, 20.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 922u,
        .union_id = 1u,
        .spacing = 24.0f,
        .interactive = false,
        .morph_transitions = false,
    };

    auto peer = request;
    peer.geometry = MaterialGeometry{92.0f, 0.0f, 160.0f, 160.0f, 20.0f};
    peer.style.tint = Color{64, 156, 255, 178};

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(peer, sampled_environment()), 2u},
    };
    assert(records[1].plan.refraction.active);
    assert(records[1].plan.dynamic_lighting.active);
    assert(records[1].plan.glass_thickness.active);
    assert(records[1].plan.glass_thickness.thickness
           > records[0].plan.glass_thickness.thickness);
    assert(records[1].plan.glass_dispersion.active);
    assert(records[1].plan.glass_dispersion.axial_offset_pixels
           > records[0].plan.glass_dispersion.axial_offset_pixels);

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const peer_execution =
        material_container_execution_descriptor(records[1], records);
    assert(first_execution.union_execution);
    assert(peer_execution.union_execution);
    assert(first_execution.surface_leader);
    assert(!peer_execution.surface_leader);
    assert(first_execution.group_appearance_source_valid);
    assert(first_execution.group_appearance_source_command_index == 2u);
    assert(first_execution.group_appearance_refraction_active);
    assert(first_execution.group_appearance_refraction_strength
           == records[1].plan.refraction.strength);
    assert(first_execution.group_appearance_refraction_edge_bias
           == records[1].plan.refraction.edge_bias);
    assert(first_execution.group_appearance_refraction_max_offset_pixels
           == records[1].plan.refraction.max_offset_pixels);
    assert(first_execution.group_appearance_refraction_edge_caustic_intensity
           == records[1].plan.refraction.edge_caustic_intensity);
    assert(first_execution.group_appearance_dynamic_lighting_active);
    assert(first_execution.group_appearance_dynamic_light_direction_x
           == records[1].plan.dynamic_lighting.direction_x);
    assert(first_execution.group_appearance_dynamic_light_direction_y
           == records[1].plan.dynamic_lighting.direction_y);
    assert(first_execution.group_appearance_dynamic_light_highlight
           == records[1].plan.dynamic_lighting.highlight_strength);
    assert(first_execution.group_appearance_dynamic_light_shadow
           == records[1].plan.dynamic_lighting.shadow_strength);
    assert(first_execution.group_appearance_glass_thickness_active);
    assert(first_execution.group_appearance_glass_thickness
           == records[1].plan.glass_thickness.thickness);
    assert(first_execution.group_appearance_glass_lensing_gain
           == records[1].plan.glass_thickness.lensing_gain);
    assert(first_execution.group_appearance_glass_shadow_gain
           == records[1].plan.glass_thickness.shadow_gain);
    assert(first_execution.group_appearance_glass_scattering_gain
           == records[1].plan.glass_thickness.scattering_gain);
    assert(first_execution.group_appearance_glass_dispersion_active);
    assert(first_execution.group_appearance_glass_dispersion_axial_offset
           == records[1].plan.glass_dispersion.axial_offset_pixels);
    assert(first_execution.group_appearance_glass_dispersion_tangential_offset
           == records[1].plan.glass_dispersion.tangential_offset_pixels);
    assert(first_execution.group_appearance_glass_dispersion_prismatic_gain
           == records[1].plan.glass_dispersion.prismatic_gain);
    assert(first_execution.group_appearance_glass_dispersion_caustic_spread
           == records[1].plan.glass_dispersion.caustic_spread);

    std::puts("PASS: glass effect union aggregates member optics");
}

void test_glass_effect_union_bridge_motion_uses_member_centroid() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 20.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 923u,
        .union_id = 1u,
        .spacing = 24.0f,
        .interactive = false,
        .morph_transitions = false,
    };

    auto peer = request;
    peer.geometry.x = 50.0f;

    auto wide_peer = request;
    wide_peer.geometry = MaterialGeometry{120.0f, 0.0f, 120.0f, 40.0f, 20.0f};

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(peer, sampled_environment()), 2u},
        {plan_material_surface(wide_peer, sampled_environment()), 3u},
    };

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const peer_execution =
        material_container_execution_descriptor(records[1], records);
    auto const wide_peer_execution =
        material_container_execution_descriptor(records[2], records);
    assert(first_execution.union_execution);
    assert(peer_execution.union_execution);
    assert(wide_peer_execution.union_execution);
    assert(first_execution.surface_leader);
    assert(!peer_execution.surface_leader);
    assert(!wide_peer_execution.surface_leader);
    assert(first_execution.group_w == 240.0f);
    assert(first_execution.group_h == 40.0f);
    assert(first_execution.fusion_lensing_gain > 1.22f);
    assert(first_execution.fusion_edge_lift > 0.10f);
    assert(first_execution.fusion_shadow_gain > 1.20f);

    auto const first_bridge_motion =
        material_container_bridge_motion_optics(
            records[0],
            records,
            first_execution);
    assert(first_bridge_motion.active);
    assert(first_bridge_motion.strength > 0.99f);
    assert(first_bridge_motion.direction_x > 0.999f);
    assert(std::fabs(first_bridge_motion.direction_y) < 0.0001f);
    assert(first_bridge_motion.specular_anchor_x > 0.50f);
    assert(first_bridge_motion.specular_anchor_x < 0.55f);
    assert(std::fabs(first_bridge_motion.specular_anchor_y - 0.5f)
           < 0.0001f);
    assert(first_bridge_motion.refraction_gain > 1.35f);
    assert(first_bridge_motion.caustic_gain > 1.54f);
    assert(first_bridge_motion.specular_intensity_gain > 1.45f);
    assert(first_bridge_motion.flow_offset_gain > 0.48f);
    assert(first_bridge_motion.ribbon_width > 0.29f);
    assert(first_bridge_motion.highlight_gain > 0.23f);

    std::puts("PASS: glass effect union bridge motion uses member centroid");
}

void test_glass_effect_union_sample_budget_uses_leader_bounds() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 20.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 915u,
        .union_id = 1u,
        .spacing = 24.0f,
        .interactive = false,
        .morph_transitions = false,
    };

    auto peer = request;
    peer.geometry.x = 50.0f;

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, sampled_environment()), 1u},
        {plan_material_surface(peer, sampled_environment()), 2u},
    };
    assert(records[0].plan.backdrop_access.max_surface_sample_pixels
           == 40 * 40);
    assert(records[1].plan.backdrop_access.max_surface_sample_pixels
           == 40 * 40);

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const peer_execution =
        material_container_execution_descriptor(records[1], records);
    assert(first_execution.union_execution);
    assert(peer_execution.union_execution);
    assert(first_execution.surface_leader);
    assert(!peer_execution.surface_leader);

    auto const first_pixels =
        material_estimate_surface_sample_pixels(
            records[0].plan,
            &first_execution);
    auto const peer_pixels =
        material_estimate_surface_sample_pixels(
            records[1].plan,
            &peer_execution);
    assert(first_pixels == 90 * 40);
    assert(peer_pixels == 0);

    MaterialRuntimeSummary runtime_summary{};
    for (auto const& record : records)
        accumulate_material_runtime_summary(runtime_summary, record);
    assert(runtime_summary.total_surface_sample_pixels == 2 * 40 * 40);
    finalize_material_runtime_summary(runtime_summary, records);
    assert(runtime_summary.total_surface_sample_pixels == 90 * 40);
    assert(runtime_summary.max_surface_sample_pixels == 90 * 40);

    MaterialExecutorSummary executor_summary{};
    for (auto const& record : records)
        accumulate_material_executor_plan_summary(executor_summary, record.plan);
    assert(executor_summary.planned_surface_sample_pixels == 2 * 40 * 40);
    finalize_material_executor_summary(executor_summary, records);
    assert(executor_summary.planned_surface_sample_pixels == 90 * 40);

    std::puts("PASS: glass effect union sample budget uses leader bounds");
}

void test_glass_effect_union_fallback_paint_uses_group_leader() {
    auto request = regular_request();
    request.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 20.0f};
    request.style.container = MaterialContainerDescriptor{
        .container_id = 920u,
        .union_id = 1u,
        .spacing = 24.0f,
        .interactive = false,
        .morph_transitions = false,
    };

    auto peer = request;
    peer.geometry.x = 50.0f;

    auto env = sampled_environment();
    env.capabilities.material_backdrop_blur = false;
    env.capabilities.shader_blur = false;
    env.capabilities.frame_history = false;
    env.backdrop.available = false;
    env.backdrop.stable = false;
    env.backdrop.source = "none";

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(request, env), 1u},
        {plan_material_surface(peer, env), 2u},
    };
    assert(records[0].plan.paint_layer_count > 1u);
    assert(records[1].plan.paint_layer_count > 1u);

    auto const first_execution =
        material_container_execution_descriptor(records[0], records);
    auto const peer_execution =
        material_container_execution_descriptor(records[1], records);

    assert(first_execution.union_execution);
    assert(peer_execution.union_execution);
    assert(first_execution.paint_layer_leader);
    assert(!peer_execution.paint_layer_leader);
    assert(first_execution.group_x == 0.0f);
    assert(first_execution.group_w == 90.0f);
    assert(first_execution.group_radius == 20.0f);

    auto const& layer = records[0].plan.paint_layers[1];
    auto const inflate = std::max(layer.inflate, 0.0f);
    auto const first_geometry =
        material_paint_layer_execution_geometry(
            records[0].plan,
            layer,
            &first_execution);
    assert(first_geometry.active);
    assert(std::fabs(first_geometry.x
                     - (first_execution.group_x + layer.x_offset - inflate))
           < 0.0001f);
    assert(std::fabs(first_geometry.w
                     - (first_execution.group_w + inflate * 2.0f))
           < 0.0001f);
    assert(std::fabs(first_geometry.radius
                     - (first_execution.group_radius + layer.radius_delta))
           < 0.0001f);

    auto const peer_base_geometry =
        material_paint_layer_execution_geometry(records[1].plan, layer);
    auto const peer_union_geometry =
        material_paint_layer_execution_geometry(
            records[1].plan,
            layer,
            &peer_execution);
    assert(peer_base_geometry.active);
    assert(!peer_union_geometry.active);

    std::puts("PASS: glass effect union fallback paint uses group leader");
}

void test_glass_effect_matched_fallback_paint_uses_match_rect() {
    auto source = regular_request();
    source.geometry = MaterialGeometry{0.0f, 0.0f, 40.0f, 40.0f, 12.0f};
    source.style.container = MaterialContainerDescriptor{
        .container_id = 930u,
        .union_id = 0u,
        .spacing = 128.0f,
        .interactive = false,
        .morph_transitions = true,
    };
    source.style.transition = MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::MatchedGeometry,
        .progress = 0.5f,
        .appearing = false,
    };
    source.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 41u,
        .effect_id = 930u,
    };

    auto target = source;
    target.geometry = MaterialGeometry{120.0f, 0.0f, 40.0f, 40.0f, 24.0f};
    target.style.transition = MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::MatchedGeometry,
        .progress = 0.5f,
        .appearing = true,
    };
    target.style.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 41u,
        .effect_id = 931u,
    };

    auto env = sampled_environment();
    env.capabilities.material_backdrop_blur = false;
    env.capabilities.shader_blur = false;
    env.capabilities.frame_history = false;
    env.backdrop.available = false;
    env.backdrop.stable = false;
    env.backdrop.source = "none";

    std::vector<MaterialRuntimeRecord> records{
        {plan_material_surface(source, env), 1u},
        {plan_material_surface(target, env), 2u},
    };
    assert(records[1].plan.paint_layer_count > 1u);

    auto const source_execution =
        material_container_execution_descriptor(records[0], records);
    auto const target_execution =
        material_container_execution_descriptor(records[1], records);
    assert(source_execution.glass_effect_match_execution);
    assert(!source_execution.surface_leader);
    assert(!source_execution.paint_layer_leader);
    assert(target_execution.glass_effect_match_execution);
    assert(target_execution.surface_leader);
    assert(target_execution.paint_layer_leader);
    assert(target_execution.glass_effect_match_source_valid);
    assert(std::fabs(target_execution.glass_effect_match_rect_x - 60.0f)
           < 0.0001f);
    assert(std::fabs(target_execution.glass_effect_match_rect_radius - 18.0f)
           < 0.0001f);

    auto const& layer = records[1].plan.paint_layers[1];
    auto const inflate = std::max(layer.inflate, 0.0f);
    auto const& source_layer = records[0].plan.paint_layers[1];
    auto const source_cluster_geometry =
        material_paint_layer_execution_geometry(
            records[0].plan,
            source_layer,
            &source_execution);
    auto const base_geometry =
        material_paint_layer_execution_geometry(records[1].plan, layer);
    auto const matched_geometry =
        material_paint_layer_execution_geometry(
            records[1].plan,
            layer,
            &target_execution);
    assert(!source_cluster_geometry.active);
    assert(base_geometry.active);
    assert(matched_geometry.active);
    assert(std::fabs(base_geometry.x - (target.geometry.x + layer.x_offset - inflate))
           < 0.0001f);
    assert(std::fabs(matched_geometry.x
                     - (target_execution.glass_effect_match_rect_x
                        + layer.x_offset
                        - inflate))
           < 0.0001f);
    assert(std::fabs(matched_geometry.radius
                     - (target_execution.glass_effect_match_rect_radius
                        + layer.radius_delta))
           < 0.0001f);

    std::puts("PASS: glass effect matched fallback paint uses match rect");
}

} // namespace

int main() {
    test_sampled_backdrop_access_contract();
    test_backdrop_optical_response_contract();
    test_toolbar_scroll_edge_separates_scrolled_content_contract();
    test_configured_tint_drives_glass_chromatics_contract();
    test_glass_thickness_scales_lensing_contract();
    test_content_layer_stays_standard_material_contract();
    test_fallback_backdrop_access_contract();
    test_glass_background_variants_shape_fallback_paint_policy();
    test_glass_background_sample_bounds_follow_execution_geometry();
    test_custom_theme_snapshot_contract();
    test_command_material_preserves_theme_snapshot_contract();
    test_foreground_contrast_gap_uses_absolute_contrast_candidate();
    test_clear_glass_legibility_dimming_contract();
    test_increase_contrast_raises_foreground_readability_contract();
    test_reduced_transparency_uses_solid_accessible_glass_fallback_contract();
    test_interactive_material_modulates_optics_contract();
    test_prominent_glass_action_optics_contract();
    test_interactive_fallback_material_adds_pointer_highlight_layer();
    test_materialize_transition_modulates_glass_optics_contract();
    test_glass_effect_identity_marks_matched_transition_contract();
    test_glass_effect_identity_drives_matched_execution_contract();
    test_glass_effect_matched_geometry_respects_container_spacing();
    test_glass_effect_matched_geometry_uses_nearby_namespace_source();
    test_glass_effect_matched_geometry_prefers_effect_id_source();
    test_warmup_backdrop_access_contract();
    test_surface_sample_pixels_are_scaled_and_bounded();
    test_executor_frame_capture_policy_contract();
    test_executor_sampled_status_contract();
    test_container_group_runtime_summary_contract();
    test_container_member_shape_blend_uses_spacing_falloff();
    test_container_group_surface_adds_overlap_response();
    test_container_group_surface_preserves_radius_continuity();
    test_container_group_surface_aggregates_member_interaction();
    test_container_group_surface_aggregates_clear_member_appearance();
    test_container_member_fallback_paint_uses_proximity_cluster();
    test_glass_effect_union_uses_compatible_render_bounds();
    test_glass_effect_union_separates_rounded_radius_variants();
    test_glass_effect_union_aggregates_member_interaction();
    test_glass_effect_union_combines_at_rest_without_spacing();
    test_glass_effect_union_aggregates_member_optics();
    test_glass_effect_union_bridge_motion_uses_member_centroid();
    test_glass_effect_union_sample_budget_uses_leader_bounds();
    test_glass_effect_union_fallback_paint_uses_group_leader();
    test_glass_effect_matched_fallback_paint_uses_match_rect();
    std::puts("\nAll material tests passed.");
    return 0;
}

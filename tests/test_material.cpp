#include <cassert>
#include <cmath>
#include <cstdio>
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
    assert(material_plan_contract_version == 36);
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
    assert(std::string_view(plan.interaction.enablement_reason)
        == "noninteractive-container");
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
    assert(std::string_view(plan.optical_composition.fallback_source)
        == "none");
    assert(plan.optical_composition.backdrop_sampled);
    assert(plan.optical_composition.blur_required);
    assert(plan.optical_composition.frosting_required);
    assert(plan.optical_composition.tint_required);
    assert(plan.optical_composition.saturation_required);
    assert(plan.optical_composition.luminance_required);
    assert(plan.optical_composition.edge_required);
    assert(plan.optical_composition.shadow_required);
    assert(plan.optical_composition.noise_required);
    assert(!plan.optical_composition.fallback_required);
    assert(plan.optical_composition.bounded);
    assert(plan.optical_composition.deterministic);
    assert(plan.optical_composition.sample_taps == plan.sample_taps);
    assert(plan.optical_composition.max_texture_copy_pixels
           == plan.primary_pass.max_texture_copy_pixels);
    assert(plan.optical_composition.max_surface_sample_pixels
           == plan.backdrop_access.max_surface_sample_pixels);
    assert(plan.optical_response.backdrop_driven);
    assert(plan.optical_response.blur_active);
    assert(plan.optical_response.frosting_active);
    assert(plan.optical_response.tint_active);
    assert(plan.optical_response.saturation_active);
    assert(plan.optical_response.luminance_preservation_active);
    assert(plan.optical_response.edge_highlight_active);
    assert(plan.optical_response.depth_shadow_active);
    assert(plan.optical_response.noise_dither_active);
    assert(plan.optical_response.foreground_vibrancy_active);
    assert(plan.optical_response.deterministic_fallback);
    assert(material_plan_uses_sampled_backdrop_executor(plan));
    assert(!material_plan_uses_standard_fill_executor(plan));
    assert(!material_plan_uses_deterministic_fallback_executor(plan));
    assert(plan.paint_layer_count == 0u);
    assert(plan.dropped_paint_layer_count == 0u);
    assert(plan.resource_budget.max_paint_layers == material_max_paint_layers);
    assert(plan.resource_budget.max_paint_layer_inflate == 0.0f);
    assert(plan.observation_contract.expected_paint_layers == 0u);
    assert(plan.observation_contract.expected_active_paint_layers == 0u);
    assert(plan.backdrop.luma_sample_count == 25u);
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
    std::puts("PASS: backdrop optical response contract");
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
    assert(!plan.optical_response.backdrop_driven);
    assert(!plan.optical_response.blur_active);
    assert(!plan.optical_response.frosting_active);
    assert(plan.optical_response.tint_active);
    assert(!plan.optical_response.saturation_active);
    assert(plan.optical_response.luminance_preservation_active);
    assert(plan.optical_response.edge_highlight_active);
    assert(plan.optical_response.depth_shadow_active);
    assert(!plan.optical_response.noise_dither_active);
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
    assert(std::string_view(plan.optical_composition.fallback_source)
        == "unsupported-backend");
    assert(!plan.optical_composition.backdrop_sampled);
    assert(!plan.optical_composition.blur_required);
    assert(!plan.optical_composition.frosting_required);
    assert(plan.optical_composition.tint_required);
    assert(!plan.optical_composition.saturation_required);
    assert(plan.optical_composition.luminance_required);
    assert(plan.optical_composition.edge_required);
    assert(plan.optical_composition.shadow_required);
    assert(!plan.optical_composition.noise_required);
    assert(plan.optical_composition.fallback_required);
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
    assert(!plan.optical_response.foreground_vibrancy_active);
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

void test_increase_contrast_raises_foreground_readability_contract() {
    auto env = sampled_environment();
    env.capabilities.increase_contrast = true;
    env.backdrop.luma_min = 0.42f;
    env.backdrop.luma_max = 0.64f;
    env.backdrop.luma_mean = 0.54f;

    auto plan = plan_material_surface(regular_request(), env);

    assert(plan.backdrop_sampling);
    assert(plan.decision_trace.increase_contrast);
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
    assert(plan.interaction.response_strength > 0.0f);
    assert(plan.opacity > baseline_plan.opacity);
    assert(plan.blur_radius > baseline_plan.blur_radius);
    assert(plan.saturation > baseline_plan.saturation);
    assert(plan.edge_highlight > baseline_plan.edge_highlight);
    assert(plan.shadow_alpha > baseline_plan.shadow_alpha);
    assert(plan.shadow_radius > baseline_plan.shadow_radius);
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
    assert(plan.optical_response.interaction_active);
    assert(plan.optical_response.interaction_modulates_optics);

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
    assert(reduced_plan.interaction.response_strength
           <= plan.interaction.response_strength);
    assert(!reduced_plan.container.morph_transitions);
    std::puts("PASS: interactive material modulates optics contract");
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
    assert(groups.fallback_mixed_group_count == 1u);
    assert(groups.max_group_size == 3u);
    assert(groups.max_active_surfaces == 3u);
    assert(groups.max_sampled_backdrop_surfaces == 2u);
    assert(groups.max_fallback_surfaces == 1u);
    assert(groups.total_shape_pair_count == 3u);
    assert(groups.blend_candidate_pair_count == 3u);
    assert(groups.union_candidate_pair_count == 2u);
    assert(groups.morph_candidate_pair_count == 3u);
    assert(groups.separated_pair_count == 0u);
    assert(std::fabs(groups.min_shape_gap) < 0.0001f);
    assert(std::fabs(groups.max_shape_gap) < 0.0001f);
    assert(groups.max_blend_distance == 28.0f);
    assert(groups.max_group_bounds_width == 240.0f);
    assert(groups.max_group_bounds_height == 96.0f);
    assert(groups.max_group_bounds_area == 240.0f * 96.0f);

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
    assert(runtime_summary.container_groups.max_group_bounds_area
           == groups.max_group_bounds_area);
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

    MaterialExecutorSummary executor_summary{};
    for (auto const& record : records)
        accumulate_material_executor_plan_summary(executor_summary, record.plan);
    finalize_material_executor_summary(executor_summary, records);
    assert(executor_summary.container_groups.group_count == groups.group_count);
    assert(executor_summary.container_groups.fallback_mixed_group_count == 1u);
    assert(executor_summary.container_groups.union_candidate_pair_count == 2u);
    assert(executor_summary.container_groups.morph_candidate_pair_count == 3u);
    assert(executor_summary.paint_layer_count == 3u);
    assert(executor_summary.active_paint_layer_count == 3u);
    assert(executor_summary.shadow_paint_layer_count == 1u);
    assert(executor_summary.fill_paint_layer_count == 1u);
    assert(executor_summary.edge_paint_layer_count == 1u);
    assert(executor_summary.max_paint_layer_inflate
        == runtime_summary.max_paint_layer_inflate);
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

} // namespace

int main() {
    test_sampled_backdrop_access_contract();
    test_backdrop_optical_response_contract();
    test_content_layer_stays_standard_material_contract();
    test_fallback_backdrop_access_contract();
    test_custom_theme_snapshot_contract();
    test_command_material_preserves_theme_snapshot_contract();
    test_foreground_contrast_gap_uses_absolute_contrast_candidate();
    test_increase_contrast_raises_foreground_readability_contract();
    test_interactive_material_modulates_optics_contract();
    test_warmup_backdrop_access_contract();
    test_surface_sample_pixels_are_scaled_and_bounded();
    test_executor_frame_capture_policy_contract();
    test_executor_sampled_status_contract();
    test_container_group_runtime_summary_contract();
    std::puts("\nAll material tests passed.");
    return 0;
}

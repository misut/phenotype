#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
import phenotype;
import phenotype.svg;

using namespace phenotype;

#if !defined(__wasi__) && !defined(__ANDROID__)
static null_host host;
#define LAYOUT_NODE(h, w)                detail::layout_node(host, h, w)
#define PAINT_NODE(h, ox, oy, sy, vh)    detail::paint_node(host, host, h, ox, oy, 0.0f, sy, 800.0f, vh)
#define CMD_BUF                          host.buf()
#define CMD_LEN                          host.buf_len()
#else
extern "C" {
    extern unsigned char phenotype_cmd_buf[];
    extern unsigned int phenotype_cmd_len;
    // Stubs for WASM host imports — wasmtime has no JS shim.
    void phenotype_flush() {}
    float phenotype_measure_text(float fs, unsigned int /*flags*/,
                                  char const* /*family*/, unsigned int /*family_len*/,
                                  char const* /*text*/, unsigned int len) {
        return static_cast<float>(len) * fs * 0.6f;
    }
    float phenotype_get_canvas_width() { return 800.0f; }
    float phenotype_get_canvas_height() { return 600.0f; }
    void phenotype_open_url(char const*, unsigned int) {}
}
#define LAYOUT_NODE(h, w)                detail::layout_node(h, w)
#define PAINT_NODE(h, ox, oy, sy, vh)    detail::wasi_paint_node(h, ox, oy, 0.0f, sy, 800.0f, vh)
#define CMD_BUF                          phenotype_cmd_buf
#define CMD_LEN                          phenotype_cmd_len
#endif

static std::unique_ptr<MaterialPlan> make_test_material_plan(
        MaterialRequest const& request,
        MaterialEnvironment environment) {
    auto owner = std::make_unique<MaterialPlan>();
    *owner = plan_material_surface(request, environment);
    return owner;
}

void test_material_props_invalidate_diff_cache() {
    auto make_tree = [](float blur_radius) {
        auto root_h = detail::alloc_node();
        auto material_h = detail::alloc_node();
        auto& material = detail::node_at(material_h);
        material.material = layout::material_style(MaterialKind::Regular);
        material.material.blur_radius = blur_radius;
        material.background = Color{255, 255, 255, 128};
        material.border_color = Color{229, 231, 235, 190};
        material.border_width = 1.0f;
        material.border_radius = 4.0f;
        material.style.fixed_height = 40.0f;
        detail::node_at(root_h).children.push_back(material_h);
        return root_h;
    };

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    auto old_root = make_tree(16.0f);
    detail::g_app().prev_root = old_root;
    std::swap(detail::g_app().arena, detail::g_app().prev_arena);
    detail::g_app().arena.reset();

    auto new_root = make_tree(30.0f);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app().prev_root,
        new_root,
        detail::g_app().prev_arena,
        detail::g_app().arena);

    assert(!matched);
    auto const& material = detail::node_at(detail::node_at(new_root).children[0]);
    assert(!material.layout_valid);
    assert(!material.paint_valid);

    std::puts("PASS: material prop changes invalidate diff/paint cache");
}

void test_material_planner_backdrop_and_fallback_paths() {
    Theme theme{};
    assert(material_resolve_sample_taps(0) == 0);
    assert(material_resolve_sample_taps(4) == 1);
    assert(material_resolve_sample_taps(7) == 5);
    assert(material_resolve_sample_taps(10) == 9);
    assert(material_resolve_sample_taps(24) == 13);
    assert(material_resolve_sample_taps(25) == 25);
    auto const kernel1 = material_resolve_sampling_kernel(true, 1);
    assert(std::string(kernel1.name) == "weighted-center");
    assert(kernel1.radius == 0);
    assert(kernel1.blur_step_scale == 0.0f);
    auto const kernel5 = material_resolve_sampling_kernel(true, 5);
    assert(std::string(kernel5.name) == "weighted-cross-5");
    assert(kernel5.radius == 1);
    assert(std::string(kernel5.weight_profile)
           == "gaussian-cross-5-separable");
    auto const kernel9 = material_resolve_sampling_kernel(true, 9);
    assert(std::string(kernel9.name) == "weighted-3x3-grid");
    assert(kernel9.radius == 1);
    assert(std::string(kernel9.weight_profile)
           == "gaussian-3x3-separable");
    auto const kernel13 = material_resolve_sampling_kernel(true, 13);
    assert(std::string(kernel13.name) == "gaussian-5x5");
    assert(kernel13.radius == 2);

    auto default_quality = default_material_quality_policy();
    assert(default_quality.allow_backdrop_sampling);
    assert(default_quality.allow_noise);
    assert(default_quality.allow_shadow);
    assert(default_quality.max_blur_radius == material_max_blur_radius);
    assert(default_quality.max_sample_taps == material_max_sample_taps);
    assert(default_quality.max_backdrop_pixels == 4'000'000);

    MaterialQualityPolicy raw_quality{};
    raw_quality.max_blur_radius = -4.0f;
    raw_quality.max_sample_taps = 99;
    raw_quality.max_backdrop_pixels = -8;
    auto sanitized_quality = sanitize_material_quality_policy(raw_quality);
    assert(sanitized_quality.max_blur_radius == 0.0f);
    assert(sanitized_quality.max_sample_taps == material_max_sample_taps);
    assert(sanitized_quality.max_backdrop_pixels == 0);

    raw_quality.max_blur_radius = 999.0f;
    raw_quality.max_sample_taps = 999;
    raw_quality.max_backdrop_pixels = 12;
    sanitized_quality = sanitize_material_quality_policy(raw_quality);
    assert(sanitized_quality.max_blur_radius == material_max_blur_radius);
    assert(sanitized_quality.max_sample_taps == material_max_sample_taps);
    assert(sanitized_quality.max_backdrop_pixels == 12);

    raw_quality.max_blur_radius = std::numeric_limits<float>::quiet_NaN();
    sanitized_quality = sanitize_material_quality_policy(raw_quality);
    assert(sanitized_quality.max_blur_radius == 0.0f);

    raw_quality.max_blur_radius = std::numeric_limits<float>::infinity();
    sanitized_quality = sanitize_material_quality_policy(raw_quality);
    assert(sanitized_quality.max_blur_radius == 0.0f);

    auto style = material_style_for_kind(MaterialKind::Regular, theme);
    MaterialRequest request{
        style,
        MaterialGeometry{12.0f, 20.0f, 240.0f, 96.0f, 10.0f},
    };

    MaterialEnvironment fallback_env{};
    fallback_env.capabilities.material_surfaces = true;
    fallback_env.render_target.width = 520;
    fallback_env.render_target.height = 760;
    fallback_env.render_target.scale = 2.0f;
    // Keep large MaterialPlan values out of this wasm test stack frame.
    auto fallback_plan_owner =
        make_test_material_plan(request, fallback_env);
    auto& fallback_plan = *fallback_plan_owner;
    assert(fallback_plan.contract_version == material_plan_contract_version);
    assert(fallback_plan.kind == MaterialKind::Regular);
    assert(fallback_plan.role == MaterialSurfaceRole::Surface);
    assert(fallback_plan.theme.default_glass_tokens);
    assert(std::string(fallback_plan.theme.profile_name) == "apple-glass-light");
    assert(std::string(fallback_plan.theme.source) == "material-style");
    assert(fallback_plan.theme.foreground_matches_theme);
    assert(fallback_plan.theme.accent_matches_theme);
    assert(fallback_plan.theme.tint_matches_surface);
    assert(fallback_plan.theme.border_matches_theme);
    assert(fallback_plan.command_descriptor.role == MaterialSurfaceRole::Surface);
    assert(fallback_plan.container.mode == MaterialContainerMode::Isolated);
    assert(std::string(fallback_plan.container.mode_name) == "isolated");
    assert(!fallback_plan.container.participates);
    assert(fallback_plan.shape.valid);
    assert(fallback_plan.shape.kind == MaterialShapeKind::RoundedRectangle);
    assert(fallback_plan.shape.rounded);
    assert(!fallback_plan.shape.capsule);
    assert(!fallback_plan.shape.radius_clamped);
    assert(fallback_plan.shape.surface_area == 240.0f * 96.0f);
    assert(fallback_plan.shape.min_extent == 96.0f);
    assert(fallback_plan.shape.max_extent == 240.0f);
    assert(fallback_plan.shape.radius_limit == 48.0f);
    assert(fallback_plan.shape.effective_radius == 10.0f);
    assert(std::fabs(fallback_plan.shape.normalized_radius
                     - (10.0f / 48.0f)) < 0.0001f);
    assert(fallback_plan.fallback());
    assert(!fallback_plan.backdrop_sampling);
    assert(fallback_plan.render_target.width == 520);
    assert(fallback_plan.render_target.height == 760);
    assert(fallback_plan.render_target.pixel_count == 395'200);
    assert(fallback_plan.render_target.ready);
    assert(fallback_plan.render_target.within_backdrop_budget);
    assert(std::string(fallback_plan.render_target.pixel_format)
           == "unknown");
    assert(fallback_plan.decision_trace.has_material);
    assert(fallback_plan.decision_trace.role_allows_liquid_glass);
    assert(!fallback_plan.decision_trace.content_layer_standard_material);
    assert(fallback_plan.decision_trace.liquid_glass_backdrop_candidate);
    assert(fallback_plan.decision_trace.target_ready);
    assert(!fallback_plan.decision_trace.backend_supports_backdrop);
    assert(!fallback_plan.decision_trace.can_sample_backdrop);
    assert(!fallback_plan.decision_trace.next_frame_capture_required);
    assert(std::string(fallback_plan.decision_trace.first_blocker)
           == "unsupported-backend");
    assert(fallback_plan.fallback_path == MaterialFallbackPath::UnsupportedBackend);
    assert(fallback_plan.blur_radius == 0.0f);
    assert(std::string(fallback_plan.primary_pass.name)
           == "translucent-rounded-rect");
    assert(fallback_plan.primary_pass.active);
    assert(!fallback_plan.primary_pass.requires_backdrop);
    assert(fallback_plan.sample_taps == 0);
    assert(fallback_plan.primary_pass.sample_taps == 0);
    assert(std::string(fallback_plan.primary_pass.executor)
           == "fallback-fill");
    assert(fallback_plan.primary_pass.max_texture_copy_pixels == 0);
    assert(fallback_plan.resource_budget.max_sample_taps
           == material_max_sample_taps);
    assert(fallback_plan.resource_budget.max_sampling_kernel_radius == 0);
    assert(fallback_plan.resource_budget.deterministic_fallback);
    assert(!fallback_plan.backdrop_access.required);
    assert(!fallback_plan.backdrop_access.shared_frame_capture);
    assert(!fallback_plan.backdrop_access.next_frame_capture_required);
    assert(!fallback_plan.backdrop_access.excludes_foreground_text);
    assert(std::string(fallback_plan.backdrop_access.capture_scope) == "none");
    assert(std::string(fallback_plan.backdrop_access.capture_reason)
           == "not-required");
    assert(std::string(fallback_plan.sampling_kernel.name) == "none");
    assert(fallback_plan.sampling_kernel.radius == 0);
    assert(fallback_plan.sampling_kernel.sample_taps == 0);
    assert(fallback_plan.sampling_kernel.blur_step_scale == 0.0f);
    assert(std::string(fallback_plan.sampling_kernel.weight_profile)
           == "none");
    assert(!fallback_plan.sampling_kernel.requires_backdrop);
    assert(fallback_plan.sampling_kernel.bounded);
    assert(std::string(fallback_plan.luminance_curve.name)
           == "fallback-flat");
    assert(std::fabs(fallback_plan.luminance_curve.floor
                     - fallback_plan.luminance_floor) < 0.0001f);
    assert(std::fabs(fallback_plan.luminance_curve.gain
                     - fallback_plan.luminance_gain) < 0.0001f);
    assert(std::fabs(fallback_plan.luminance_curve.gamma - 1.0f)
           < 0.0001f);
    assert(std::fabs(fallback_plan.luminance_curve.midpoint - 0.5f)
           < 0.0001f);
    assert(std::fabs(fallback_plan.luminance_curve.contrast - 1.0f)
           < 0.0001f);
    assert(std::fabs(fallback_plan.luminance_curve.edge_lift
                     - fallback_plan.edge_highlight) < 0.0001f);
    assert(!fallback_plan.luminance_curve.backdrop_driven);
    assert(fallback_plan.luminance_curve.bounded);
    assert(std::string(fallback_plan.backdrop.source) == "none");
    assert(std::string(fallback_plan.backdrop.luminance_response)
           == "not-sampled");
    assert(std::string(fallback_plan.backdrop.frosting_response)
           == "not-sampled");
    assert(std::string(fallback_plan.backdrop.tint_response)
           == "not-sampled");
    assert(std::string(fallback_plan.backdrop.saturation_response)
           == "not-sampled");
    assert(std::string(fallback_plan.backdrop.depth_response)
           == "not-sampled");
    assert(std::fabs(fallback_plan.backdrop.opacity_delta) < 0.0001f);
    assert(std::fabs(fallback_plan.backdrop.tint_alpha_delta) < 0.0001f);
    assert(std::fabs(fallback_plan.backdrop.saturation_delta) < 0.0001f);
    assert(std::fabs(fallback_plan.backdrop.shadow_alpha_delta) < 0.0001f);
    assert(std::fabs(fallback_plan.backdrop.shadow_radius_delta) < 0.0001f);
    assert(std::fabs(fallback_plan.backdrop.response_strength) < 0.0001f);
    assert(std::string(fallback_plan.foreground.scheme)
           == "solid-fallback");
    assert(std::string(fallback_plan.foreground.source)
           == "fallback-material");
    assert(!fallback_plan.foreground.backdrop_driven);
    assert(!fallback_plan.foreground.uses_vibrancy);
    assert(fallback_plan.foreground.deterministic);
    assert(fallback_plan.foreground.primary_contrast_ratio
           >= fallback_plan.foreground.minimum_contrast_ratio);
    assert(fallback_plan.foreground.secondary_contrast_ratio
           >= fallback_plan.foreground.minimum_contrast_ratio);
    assert(fallback_plan.foreground.accent_contrast_ratio
           >= fallback_plan.foreground.minimum_contrast_ratio);
    assert(std::string(fallback_plan.reference_model.technology)
           == "liquid-glass");
    assert(std::string(fallback_plan.reference_model.layer)
           == "functional-layer");
    assert(std::string(fallback_plan.reference_model.material_policy)
           == "liquid-glass-functional-layer");
    assert(std::string(fallback_plan.reference_model.variant) == "regular");
    assert(std::string(fallback_plan.reference_model.shape)
           == "rounded-rectangle");
    assert(std::string(fallback_plan.reference_model.shape_scope)
           == "view-bounds");
    assert(std::string(fallback_plan.reference_model.blending_scope)
           == "deterministic-fallback");
    assert(std::string(fallback_plan.reference_model.accessibility_response)
           == "standard");
    assert(std::string(fallback_plan.reference_model.performance_response)
           == "deterministic-fallback");
    assert(fallback_plan.reference_model.view_bounds_anchored);
    assert(fallback_plan.reference_model.shape_matches_geometry);
    assert(fallback_plan.reference_model.tint_applied);
    assert(!fallback_plan.reference_model.interactive_response);
    assert(!fallback_plan.reference_model.container_grouped);
    assert(!fallback_plan.reference_model.container_union);
    assert(!fallback_plan.reference_model.container_morphing);
    assert(fallback_plan.reference_model.legibility_preserved);
    assert(!fallback_plan.reference_model.vibrancy_expected);
    assert(fallback_plan.reference_model.deterministic_degradation);
    assert(std::string(fallback_plan.verifier.likely_layer)
           == "material-fallback-pass");
    assert(std::string(fallback_plan.verifier.likely_pass)
           == "translucent-rounded-rect");
    assert(fallback_plan.observation_contract.schema_version
           == material_plan_contract_version);
    assert(fallback_plan.observation_contract.semantic_material_required);
    assert(fallback_plan.observation_contract.runtime_plan_required);
    assert(fallback_plan.observation_contract.fallback_expected);
    assert(!fallback_plan.observation_contract.backdrop_sampling_expected);
    assert(!fallback_plan.observation_contract.stable_backdrop_required);
    assert(!fallback_plan.observation_contract.shared_frame_capture_required);
    assert(!fallback_plan.observation_contract.next_frame_capture_required);
    assert(!fallback_plan.observation_contract
                .backdrop_capture_excludes_foreground_text);
    assert(fallback_plan.observation_contract.bounded_texture_copy_required);
    assert(fallback_plan.observation_contract.deterministic_fallback_required);
    assert(std::string(fallback_plan.observation_contract.fallback_path)
           == "unsupported-backend");
    assert(std::string(fallback_plan.observation_contract.fallback_reason)
           == fallback_plan.fallback_reason);
    assert(std::string(fallback_plan.observation_contract.backdrop_capture_scope)
           == "none");
    assert(std::string(fallback_plan.observation_contract.backdrop_capture_reason)
           == "not-required");
    assert(std::string(fallback_plan.observation_contract.primary_pass)
           == "translucent-rounded-rect");
    assert(std::string(fallback_plan.observation_contract.primary_executor)
           == "fallback-fill");
    assert(fallback_plan.observation_contract.expected_runtime_passes == 1);
    assert(fallback_plan.observation_contract.expected_active_runtime_passes
           == 1);
    assert(fallback_plan.observation_contract.expected_backdrop_runtime_passes
           == 0);
    assert(fallback_plan.observation_contract.expected_execution_stages
           == fallback_plan.execution_stage_count);
    assert(fallback_plan.observation_contract.expected_backdrop_execution_stages
           == 0);
    assert(fallback_plan.observation_contract.max_texture_copy_pixels == 0);
    assert(std::string(fallback_plan.observation_contract.region_name)
           == fallback_plan.verifier.region_name);
    assert(std::string(fallback_plan.observation_contract.likely_layer)
           == fallback_plan.verifier.likely_layer);
    assert(std::string(fallback_plan.observation_contract.likely_pass)
           == fallback_plan.verifier.likely_pass);

    MaterialEnvironment unsupported_large_env = fallback_env;
    unsupported_large_env.render_target.width = 2400;
    unsupported_large_env.render_target.height = 2400;
    auto unsupported_large_plan_owner =
        make_test_material_plan(request, unsupported_large_env);
    auto& unsupported_large_plan = *unsupported_large_plan_owner;
    assert(unsupported_large_plan.fallback());
    assert(unsupported_large_plan.fallback_path
           == MaterialFallbackPath::UnsupportedBackend);
    assert(!unsupported_large_plan.render_target.within_backdrop_budget);

    MaterialEnvironment warmup_env = fallback_env;
    warmup_env.capabilities.material_backdrop_blur = true;
    warmup_env.capabilities.shader_blur = true;
    warmup_env.capabilities.frame_history = false;
    warmup_env.backdrop.available = false;
    warmup_env.backdrop.stable = false;
    warmup_env.backdrop.source = "none";
    auto warmup_plan_owner = make_test_material_plan(request, warmup_env);
    auto& warmup_plan = *warmup_plan_owner;
    assert(warmup_plan.fallback());
    assert(warmup_plan.fallback_path == MaterialFallbackPath::NoBackdropSource);
    assert(warmup_plan.decision_trace.backend_supports_backdrop);
    assert(!warmup_plan.decision_trace.backdrop_source_ready);
    assert(warmup_plan.decision_trace.next_frame_capture_required);
    assert(!warmup_plan.backdrop_access.required);
    assert(!warmup_plan.backdrop_access.stable_required);
    assert(!warmup_plan.backdrop_access.frame_history_required);
    assert(warmup_plan.backdrop_access.shared_frame_capture);
    assert(warmup_plan.backdrop_access.next_frame_capture_required);
    assert(warmup_plan.backdrop_access.excludes_foreground_text);
    assert(std::string(warmup_plan.backdrop_access.capture_scope)
           == "shared-frame");
    assert(std::string(warmup_plan.backdrop_access.capture_reason)
           == "warmup-next-frame");
    assert(warmup_plan.backdrop_access.max_frame_capture_count == 1);
    assert(warmup_plan.backdrop_access.max_frame_capture_pixels
           == warmup_plan.render_target.pixel_count);
    assert(warmup_plan.backdrop_access.max_surface_sample_pixels == 0);
    assert(warmup_plan.resource_budget.max_frame_capture_count == 1);
    assert(warmup_plan.resource_budget.max_frame_capture_pixels
           == warmup_plan.render_target.pixel_count);
    assert(warmup_plan.resource_budget.max_surface_sample_pixels == 0);
    assert(warmup_plan.observation_contract.shared_frame_capture_required);
    assert(warmup_plan.observation_contract.next_frame_capture_required);
    assert(warmup_plan.observation_contract
                .backdrop_capture_excludes_foreground_text);
    assert(std::string(warmup_plan.reference_model.performance_response)
           == "warmup-capture");
    assert(std::string(warmup_plan.observation_contract.backdrop_capture_scope)
           == "shared-frame");
    assert(std::string(warmup_plan.observation_contract.backdrop_capture_reason)
           == "warmup-next-frame");
    assert(std::string(unsupported_large_plan.decision_trace.first_blocker)
           == "unsupported-backend");

    MaterialEnvironment glass_env = fallback_env;
    glass_env.capabilities.material_backdrop_blur = true;
    glass_env.capabilities.shader_blur = true;
    glass_env.capabilities.frame_history = true;
    glass_env.backdrop.available = true;
    glass_env.backdrop.stable = true;
    glass_env.backdrop.excludes_foreground_text = true;
    glass_env.backdrop.source = "previous-presented-frame";
    auto glass_plan_owner = make_test_material_plan(request, glass_env);
    auto& glass_plan = *glass_plan_owner;
    assert(glass_plan.contract_version == material_plan_contract_version);
    assert(glass_plan.role == MaterialSurfaceRole::Surface);
    assert(glass_plan.container.mode == MaterialContainerMode::Isolated);
    assert(!glass_plan.fallback());
    assert(glass_plan.backdrop_sampling);
    assert(glass_plan.decision_trace.backend_supports_backdrop);
    assert(glass_plan.decision_trace.role_allows_liquid_glass);
    assert(!glass_plan.decision_trace.content_layer_standard_material);
    assert(glass_plan.decision_trace.liquid_glass_backdrop_candidate);
    assert(glass_plan.decision_trace.backdrop_source_ready);
    assert(glass_plan.decision_trace.can_sample_backdrop);
    assert(glass_plan.decision_trace.next_frame_capture_required);
    assert(std::string(glass_plan.decision_trace.first_blocker) == "none");
    assert(glass_plan.render_target.ready);
    assert(glass_plan.render_target.within_backdrop_budget);
    assert(glass_plan.blur_radius >= 20.0f);
    assert(glass_plan.saturation > 1.0f);
    assert(glass_plan.edge_highlight > 0.0f);
    assert(glass_plan.shadow_alpha > 0.0f);
    assert(std::string(glass_plan.primary_pass.name)
           == "backdrop-sample-blur");
    assert(glass_plan.primary_pass.active);
    assert(glass_plan.primary_pass.requires_backdrop);
    assert(glass_plan.primary_pass.sample_taps == glass_plan.sample_taps);
    assert(std::string(glass_plan.primary_pass.executor)
           == "backdrop-filter");
    assert(glass_plan.primary_pass.max_texture_copy_pixels
           == glass_plan.render_target.pixel_count);
    assert(glass_plan.resource_budget.max_execution_stages == 4);
    assert(glass_plan.execution_stage_capacity == 4);
    assert(glass_plan.execution_stage_count == 4);
    assert(glass_plan.dropped_execution_stage_count == 0);
    assert(std::string(glass_plan.execution_stages[0].name)
           == "shape-shadow");
    assert(!glass_plan.execution_stages[0].requires_backdrop);
    assert(std::string(glass_plan.execution_stages[0].optics.channel)
           == "shape-shadow");
    assert(glass_plan.execution_stages[0].optics.shadow_alpha
           == glass_plan.shadow_alpha);
    assert(glass_plan.execution_stages[0].optics.shadow_radius
           == glass_plan.shadow_radius);
    assert(std::string(glass_plan.execution_stages[1].name)
           == "backdrop-sample-blur");
    assert(glass_plan.execution_stages[1].requires_backdrop);
    assert(glass_plan.execution_stages[1].sample_taps
           == glass_plan.sample_taps);
    assert(glass_plan.execution_stages[1].max_texture_copy_pixels
           == glass_plan.render_target.pixel_count);
    assert(std::string(glass_plan.execution_stages[1].optics.channel)
           == "backdrop-filter");
    assert(glass_plan.execution_stages[1].optics.blur_radius
           == glass_plan.blur_radius);
    assert(glass_plan.execution_stages[1].optics.saturation
           == glass_plan.saturation);
    assert(std::fabs(glass_plan.execution_stages[1].optics.tint_alpha
                     - static_cast<float>(glass_plan.tint.a) / 255.0f)
           < 0.0001f);
    assert(std::string(glass_plan.execution_stages[2].name)
           == "edge-highlight");
    assert(glass_plan.execution_stages[2].optics.edge_highlight
           == glass_plan.edge_highlight);
    assert(glass_plan.execution_stages[2].optics.edge_width
           == glass_plan.edge_width);
    assert(std::string(glass_plan.execution_stages[3].name)
           == "noise-dither");
    assert(glass_plan.execution_stages[3].optics.noise_opacity
           == glass_plan.noise_opacity);
    for (unsigned int i = 0; i < glass_plan.execution_stage_count; ++i) {
        assert(glass_plan.execution_stages[i].active);
        assert(glass_plan.execution_stages[i].bounded);
    }
    assert(std::string(glass_plan.sampling_kernel.name)
           == "gaussian-5x5");
    assert(glass_plan.sampling_kernel.radius == 2);
    assert(glass_plan.sampling_kernel.sample_taps == glass_plan.sample_taps);
    assert(std::fabs(glass_plan.sampling_kernel.blur_step_scale - 0.35f)
           < 0.0001f);
    assert(std::string(glass_plan.sampling_kernel.weight_profile)
           == "gaussian-5x5-separable");
    assert(glass_plan.sampling_kernel.requires_backdrop);
    assert(glass_plan.sampling_kernel.bounded);
    assert(std::string(glass_plan.luminance_curve.name)
           == "adaptive-backdrop-luma");
    assert(std::fabs(glass_plan.luminance_curve.floor
                     - glass_plan.luminance_floor) < 0.0001f);
    assert(std::fabs(glass_plan.luminance_curve.gain
                     - glass_plan.luminance_gain) < 0.0001f);
    assert(std::fabs(glass_plan.luminance_curve.gamma - 1.0f)
           < 0.0001f);
    assert(std::fabs(glass_plan.luminance_curve.midpoint - 0.5f)
           < 0.0001f);
    assert(std::fabs(glass_plan.luminance_curve.contrast - 1.0f)
           < 0.0001f);
    assert(std::fabs(glass_plan.luminance_curve.edge_lift
                     - glass_plan.edge_highlight) < 0.0001f);
    assert(glass_plan.luminance_curve.backdrop_driven);
    assert(glass_plan.luminance_curve.bounded);
    assert(glass_plan.resource_budget.max_sampling_kernel_radius == 2);
    assert(glass_plan.backdrop_access.required);
    assert(glass_plan.backdrop_access.shared_frame_capture);
    assert(glass_plan.backdrop_access.next_frame_capture_required);
    assert(glass_plan.backdrop_access.excludes_foreground_text);
    assert(std::string(glass_plan.backdrop_access.capture_scope)
           == "shared-frame");
    assert(std::string(glass_plan.backdrop_access.capture_reason)
           == "sample-current-frame");
    assert(glass_plan.backdrop.available);
    assert(glass_plan.backdrop.stable);
    assert(glass_plan.backdrop.excludes_foreground_text);
    assert(std::string(glass_plan.backdrop.source)
           == "previous-presented-frame");
    assert(std::string(glass_plan.backdrop.luminance_response)
           == "neutral");
    assert(std::string(glass_plan.backdrop.frosting_response)
           == "balanced");
    assert(std::string(glass_plan.backdrop.tint_response)
           == "preserve");
    assert(std::string(glass_plan.backdrop.saturation_response)
           == "preserve");
    assert(std::string(glass_plan.backdrop.depth_response)
           == "standard");
    assert(std::fabs(glass_plan.backdrop.luminance_floor_delta) < 0.0001f);
    assert(std::fabs(glass_plan.backdrop.luminance_gain_delta) < 0.0001f);
    assert(std::fabs(glass_plan.backdrop.edge_highlight_delta) < 0.0001f);
    assert(std::fabs(glass_plan.backdrop.opacity_delta) < 0.0001f);
    assert(std::fabs(glass_plan.backdrop.tint_alpha_delta) < 0.0001f);
    assert(std::fabs(glass_plan.backdrop.saturation_delta) < 0.0001f);
    assert(std::fabs(glass_plan.backdrop.shadow_alpha_delta) < 0.0001f);
    assert(std::fabs(glass_plan.backdrop.shadow_radius_delta) < 0.0001f);
    assert(std::fabs(glass_plan.backdrop.response_strength) < 0.0001f);
    assert(std::string(glass_plan.foreground.scheme)
           == "vibrant-balanced");
    assert(std::string(glass_plan.foreground.source)
           == "backdrop-analysis");
    assert(glass_plan.foreground.backdrop_driven);
    assert(glass_plan.foreground.uses_vibrancy);
    assert(!glass_plan.foreground.high_contrast);
    assert(glass_plan.foreground.deterministic);
    assert(glass_plan.foreground.background_luma > 0.60f);
    assert(glass_plan.foreground.background_luma < 0.75f);
    assert(glass_plan.foreground.primary_contrast_ratio
           >= glass_plan.foreground.minimum_contrast_ratio);
    assert(glass_plan.foreground.secondary_contrast_ratio
           >= glass_plan.foreground.minimum_contrast_ratio);
    assert(glass_plan.foreground.accent_contrast_ratio
           >= glass_plan.foreground.minimum_contrast_ratio);
    assert(std::string(glass_plan.reference_model.blending_scope)
           == "sampled-backdrop");
    assert(std::string(glass_plan.reference_model.technology)
           == "liquid-glass");
    assert(std::string(glass_plan.reference_model.layer)
           == "functional-layer");
    assert(std::string(glass_plan.reference_model.material_policy)
           == "liquid-glass-functional-layer");
    assert(std::string(glass_plan.reference_model.semantic_thickness)
           == "regular");
    assert(std::string(glass_plan.reference_model.accessibility_response)
           == "standard");
    assert(std::string(glass_plan.reference_model.performance_response)
           == "standard");
    assert(glass_plan.reference_model.view_bounds_anchored);
    assert(glass_plan.reference_model.shape_matches_geometry);
    assert(glass_plan.reference_model.tint_applied);
    assert(!glass_plan.reference_model.interactive_response);
    assert(!glass_plan.reference_model.container_grouped);
    assert(!glass_plan.reference_model.container_union);
    assert(!glass_plan.reference_model.container_morphing);
    assert(glass_plan.reference_model.legibility_preserved);
    assert(glass_plan.reference_model.vibrancy_expected);
    assert(glass_plan.reference_model.deterministic_degradation);
    assert(std::string(glass_plan.verifier.likely_layer)
           == "material-blur-pass");
    assert(std::string(glass_plan.verifier.likely_pass)
           == "backdrop-sample-blur");
    assert(glass_plan.observation_contract.schema_version
           == material_plan_contract_version);
    assert(glass_plan.observation_contract.semantic_material_required);
    assert(glass_plan.observation_contract.runtime_plan_required);
    assert(!glass_plan.observation_contract.fallback_expected);
    assert(glass_plan.observation_contract.backdrop_sampling_expected);
    assert(glass_plan.observation_contract.stable_backdrop_required);
    assert(glass_plan.observation_contract.shared_frame_capture_required);
    assert(glass_plan.observation_contract.next_frame_capture_required);
    assert(glass_plan.observation_contract
                .backdrop_capture_excludes_foreground_text);
    assert(std::string(glass_plan.observation_contract.backdrop_capture_scope)
           == "shared-frame");
    assert(std::string(glass_plan.observation_contract.backdrop_capture_reason)
           == "sample-current-frame");
    assert(glass_plan.observation_contract.expected_runtime_passes == 1);
    assert(glass_plan.observation_contract.expected_active_runtime_passes == 1);
    assert(glass_plan.observation_contract.expected_backdrop_runtime_passes
           == 1);
    assert(glass_plan.observation_contract.expected_execution_stages == 4);
    assert(glass_plan.observation_contract.expected_active_execution_stages == 4);
    assert(glass_plan.observation_contract.expected_backdrop_execution_stages == 1);
    assert(glass_plan.observation_contract.max_texture_copy_pixels
           == glass_plan.render_target.pixel_count);
    assert(std::string(glass_plan.observation_contract.primary_pass)
           == "backdrop-sample-blur");
    assert(std::string(glass_plan.observation_contract.primary_executor)
           == "backdrop-filter");

    MaterialRequest container_request = request;
    container_request.style.container = MaterialContainerDescriptor{
        41u,
        7u,
        24.0f,
        true,
        true};
    auto container_plan_owner =
        make_test_material_plan(container_request, glass_env);
    auto& container_plan = *container_plan_owner;
    assert(!container_plan.fallback());
    assert(container_plan.container.mode == MaterialContainerMode::Union);
    assert(std::string(container_plan.container.mode_name) == "union");
    assert(container_plan.container.container_id == 41u);
    assert(container_plan.container.union_id == 7u);
    assert(container_plan.container.spacing == 24.0f);
    assert(container_plan.container.blend_distance == 24.0f);
    assert(container_plan.container.participates);
    assert(container_plan.container.shared_backdrop_scope);
    assert(container_plan.container.shape_union_expected);
    assert(container_plan.container.shape_blending_expected);
    assert(container_plan.container.interactive);
    assert(container_plan.container.requested_morph_transitions);
    assert(container_plan.container.morph_transitions);
    assert(!container_plan.container.reduced_motion_suppressed_morph);
    assert(!container_plan.container.spacing_clamped);
    assert(std::string_view(container_plan.container.blend_policy)
           == "union-shape-proximity");
    assert(std::string_view(container_plan.container.morph_policy)
           == "union-morph");
    assert(std::string_view(container_plan.container.performance_policy)
           == "shared-union-capture");
    assert(std::string_view(container_plan.interaction.enablement_reason)
           == "interactive-container");
    assert(container_plan.command_descriptor.container.container_id == 41u);
    assert(container_plan.resource_budget.max_container_spacing == 24.0f);
    assert(container_plan.verifier.require_container_identity);
    assert(container_plan.verifier.require_container_morph_contract);
    assert(container_plan.reference_model.interactive_response);
    assert(container_plan.reference_model.container_grouped);
    assert(container_plan.reference_model.container_union);
    assert(container_plan.reference_model.container_morphing);

    MaterialRequest clamped_shape_request = request;
    clamped_shape_request.geometry.radius = 200.0f;
    auto clamped_shape_plan_owner =
        make_test_material_plan(clamped_shape_request, glass_env);
    auto& clamped_shape_plan = *clamped_shape_plan_owner;
    assert(!clamped_shape_plan.fallback());
    assert(clamped_shape_plan.shape.valid);
    assert(clamped_shape_plan.shape.kind == MaterialShapeKind::Capsule);
    assert(clamped_shape_plan.shape.rounded);
    assert(clamped_shape_plan.shape.capsule);
    assert(clamped_shape_plan.shape.radius_clamped);
    assert(clamped_shape_plan.shape.radius_limit == 48.0f);
    assert(clamped_shape_plan.shape.effective_radius == 48.0f);
    assert(clamped_shape_plan.shape.normalized_radius == 1.0f);
    assert(std::string(clamped_shape_plan.reference_model.shape) == "capsule");

    MaterialEnvironment reduced_motion_container_env = glass_env;
    reduced_motion_container_env.capabilities.reduce_motion = true;
    auto reduced_motion_container_plan_owner =
        make_test_material_plan(container_request, reduced_motion_container_env);
    auto& reduced_motion_container_plan =
        *reduced_motion_container_plan_owner;
    assert(reduced_motion_container_plan.container.participates);
    assert(reduced_motion_container_plan.container.interactive);
    assert(reduced_motion_container_plan.container.requested_morph_transitions);
    assert(!reduced_motion_container_plan.container.morph_transitions);
    assert(reduced_motion_container_plan.container.reduced_motion_suppressed_morph);
    assert(std::string_view(reduced_motion_container_plan.container.morph_policy)
           == "reduced-motion-static");
    assert(!reduced_motion_container_plan.verifier.require_container_morph_contract);
    assert(std::string(
               reduced_motion_container_plan.reference_model
                   .accessibility_response)
           == "reduced-motion");

    MaterialEnvironment dark_backdrop_env = glass_env;
    dark_backdrop_env.backdrop.luma_min = 0.02f;
    dark_backdrop_env.backdrop.luma_max = 0.28f;
    dark_backdrop_env.backdrop.luma_mean = 0.12f;
    auto dark_backdrop_plan_owner =
        make_test_material_plan(request, dark_backdrop_env);
    auto& dark_backdrop_plan = *dark_backdrop_plan_owner;
    assert(dark_backdrop_plan.backdrop_sampling);
    assert(dark_backdrop_plan.luminance_floor
           > glass_plan.luminance_floor);
    assert(dark_backdrop_plan.luminance_gain
           > glass_plan.luminance_gain);
    assert(dark_backdrop_plan.edge_highlight
           > glass_plan.edge_highlight);
    assert(std::string(dark_backdrop_plan.backdrop.luminance_response)
           == "dark");
    assert(std::string(dark_backdrop_plan.backdrop.frosting_response)
           == "dark-frost-lift");
    assert(std::string(dark_backdrop_plan.backdrop.tint_response)
           == "lift-dark-backdrop");
    assert(std::string(dark_backdrop_plan.backdrop.saturation_response)
           == "lift-dark-color");
    assert(std::string(dark_backdrop_plan.backdrop.depth_response)
           == "soften-dark-depth");
    assert(dark_backdrop_plan.backdrop.luminance_floor_delta > 0.0f);
    assert(dark_backdrop_plan.backdrop.luminance_gain_delta > 0.0f);
    assert(dark_backdrop_plan.backdrop.edge_highlight_delta > 0.0f);
    assert(dark_backdrop_plan.backdrop.opacity_delta > 0.0f);
    assert(dark_backdrop_plan.backdrop.tint_alpha_delta > 0.0f);
    assert(dark_backdrop_plan.backdrop.saturation_delta > 0.0f);
    assert(dark_backdrop_plan.backdrop.shadow_alpha_delta < 0.0f);
    assert(dark_backdrop_plan.backdrop.shadow_radius_delta < 0.0f);
    assert(dark_backdrop_plan.backdrop.response_strength > 0.0f);
    assert(dark_backdrop_plan.luminance_curve.gamma < 1.0f);
    assert(dark_backdrop_plan.luminance_curve.midpoint < 0.35f);
    assert(std::string(dark_backdrop_plan.foreground.scheme)
           == "vibrant-light");
    assert(dark_backdrop_plan.foreground.primary_contrast_ratio
           >= dark_backdrop_plan.foreground.minimum_contrast_ratio);

    MaterialEnvironment bright_backdrop_env = glass_env;
    bright_backdrop_env.backdrop.luma_min = 0.84f;
    bright_backdrop_env.backdrop.luma_max = 0.97f;
    bright_backdrop_env.backdrop.luma_mean = 0.90f;
    auto bright_backdrop_plan_owner =
        make_test_material_plan(request, bright_backdrop_env);
    auto& bright_backdrop_plan = *bright_backdrop_plan_owner;
    assert(bright_backdrop_plan.backdrop_sampling);
    assert(bright_backdrop_plan.luminance_gain
           < glass_plan.luminance_gain);
    assert(bright_backdrop_plan.edge_highlight
           > glass_plan.edge_highlight);
    assert(std::string(bright_backdrop_plan.backdrop.luminance_response)
           == "bright");
    assert(std::string(bright_backdrop_plan.backdrop.frosting_response)
           == "bright-frost-thin");
    assert(std::string(bright_backdrop_plan.backdrop.tint_response)
           == "thin-bright-backdrop");
    assert(std::string(bright_backdrop_plan.backdrop.saturation_response)
           == "compress-bright-color");
    assert(std::string(bright_backdrop_plan.backdrop.depth_response)
           == "restore-bright-depth");
    assert(bright_backdrop_plan.backdrop.luminance_gain_delta < 0.0f);
    assert(bright_backdrop_plan.backdrop.edge_highlight_delta > 0.0f);
    assert(bright_backdrop_plan.backdrop.opacity_delta < 0.0f);
    assert(bright_backdrop_plan.backdrop.tint_alpha_delta < 0.0f);
    assert(bright_backdrop_plan.backdrop.saturation_delta < 0.0f);
    assert(bright_backdrop_plan.backdrop.shadow_alpha_delta > 0.0f);
    assert(bright_backdrop_plan.backdrop.shadow_radius_delta > 0.0f);
    assert(bright_backdrop_plan.luminance_curve.gamma > 1.0f);
    assert(bright_backdrop_plan.luminance_curve.midpoint > 0.70f);
    assert(std::string(bright_backdrop_plan.foreground.scheme)
           == "vibrant-dark");
    assert(bright_backdrop_plan.foreground.primary_contrast_ratio
           >= bright_backdrop_plan.foreground.minimum_contrast_ratio);

    MaterialEnvironment flat_backdrop_env = glass_env;
    flat_backdrop_env.backdrop.luma_min = 0.46f;
    flat_backdrop_env.backdrop.luma_max = 0.50f;
    flat_backdrop_env.backdrop.luma_mean = 0.48f;
    auto flat_backdrop_plan_owner =
        make_test_material_plan(request, flat_backdrop_env);
    auto& flat_backdrop_plan = *flat_backdrop_plan_owner;
    assert(flat_backdrop_plan.backdrop_sampling);
    assert(flat_backdrop_plan.backdrop.luma_span < 0.05f);
    assert(std::string(flat_backdrop_plan.backdrop.luminance_response)
           == "flat");
    assert(std::string(flat_backdrop_plan.backdrop.frosting_response)
           == "flat-edge-frost");
    assert(std::string(flat_backdrop_plan.backdrop.tint_response)
           == "stabilize-flat-backdrop");
    assert(std::string(flat_backdrop_plan.backdrop.saturation_response)
           == "restore-flat-color");
    assert(std::string(flat_backdrop_plan.backdrop.depth_response)
           == "restore-flat-depth");
    assert(std::fabs(flat_backdrop_plan.backdrop.luminance_floor_delta)
           < 0.0001f);
    assert(std::fabs(flat_backdrop_plan.backdrop.luminance_gain_delta)
           < 0.0001f);
    assert(flat_backdrop_plan.backdrop.edge_highlight_delta > 0.0f);
    assert(flat_backdrop_plan.backdrop.opacity_delta > 0.0f);
    assert(flat_backdrop_plan.backdrop.tint_alpha_delta > 0.0f);
    assert(flat_backdrop_plan.backdrop.saturation_delta > 0.0f);
    assert(flat_backdrop_plan.backdrop.shadow_alpha_delta > 0.0f);
    assert(flat_backdrop_plan.backdrop.shadow_radius_delta > 0.0f);
    assert(flat_backdrop_plan.luminance_curve.contrast > 1.0f);

    MaterialEnvironment contrast_motion_env = glass_env;
    contrast_motion_env.capabilities.increase_contrast = true;
    contrast_motion_env.capabilities.reduce_motion = true;
    auto contrast_motion_plan_owner =
        make_test_material_plan(request, contrast_motion_env);
    auto& contrast_motion_plan = *contrast_motion_plan_owner;
    assert(contrast_motion_plan.backdrop_sampling);
    assert(contrast_motion_plan.decision_trace.increase_contrast);
    assert(contrast_motion_plan.decision_trace.reduce_motion);
    assert(contrast_motion_plan.opacity > glass_plan.opacity);
    assert(contrast_motion_plan.luminance_floor > glass_plan.luminance_floor);
    assert(contrast_motion_plan.saturation <= 1.0f);
    assert(contrast_motion_plan.noise_opacity == 0.0f);
    assert(contrast_motion_plan.sample_taps < glass_plan.sample_taps);
    assert(contrast_motion_plan.primary_pass.sample_taps
           == contrast_motion_plan.sample_taps);
    assert(std::string(contrast_motion_plan.foreground.scheme)
           == "high-contrast");
    assert(std::string(contrast_motion_plan.foreground.source)
           == "accessibility");
    assert(contrast_motion_plan.foreground.high_contrast);
    assert(std::string(contrast_motion_plan.reference_model
                           .accessibility_response)
           == "combined-accessibility");
    assert(std::string(contrast_motion_plan.reference_model
                           .performance_response)
           == "budgeted-effects");

    glass_env.capabilities.reduce_transparency = true;
    auto reduced_plan_owner = make_test_material_plan(request, glass_env);
    auto& reduced_plan = *reduced_plan_owner;
    assert(reduced_plan.fallback());
    assert(reduced_plan.fallback_path == MaterialFallbackPath::ReducedTransparency);
    assert(!reduced_plan.backdrop_sampling);
    assert(reduced_plan.sample_taps == 0);
    assert(reduced_plan.primary_pass.sample_taps == 0);
    assert(reduced_plan.noise_opacity == 0.0f);
    assert(std::string(reduced_plan.primary_pass.name)
           == "translucent-rounded-rect");
    assert(reduced_plan.execution_stage_count == 3);
    assert(std::string(reduced_plan.execution_stages[1].name)
           == "translucent-rounded-rect");
    assert(!reduced_plan.execution_stages[1].requires_backdrop);
    assert(std::string(reduced_plan.execution_stages[1].optics.channel)
           == "fallback-fill");
    assert(reduced_plan.execution_stages[1].optics.blur_radius == 0.0f);
    assert(reduced_plan.execution_stages[1].optics.saturation == 1.0f);
    assert(std::string(reduced_plan.reference_model.accessibility_response)
           == "reduced-transparency");
    assert(std::string(reduced_plan.reference_model.performance_response)
           == "deterministic-fallback");

    glass_env.capabilities.reduce_transparency = false;
    MaterialEnvironment disabled_quality_env = glass_env;
    disabled_quality_env.quality.allow_backdrop_sampling = false;
    auto disabled_quality_plan_owner =
        make_test_material_plan(request, disabled_quality_env);
    auto& disabled_quality_plan = *disabled_quality_plan_owner;
    assert(disabled_quality_plan.fallback());
    assert(disabled_quality_plan.fallback_path
           == MaterialFallbackPath::QualityPolicy);
    assert(!disabled_quality_plan.quality_policy.allow_backdrop_sampling);
    assert(!disabled_quality_plan.backdrop_sampling);
    assert(disabled_quality_plan.blur_radius == 0.0f);
    assert(disabled_quality_plan.sample_taps == 0);
    assert(disabled_quality_plan.primary_pass.sample_taps == 0);
    assert(std::string(disabled_quality_plan.fallback_reason)
           == "quality policy disables material backdrop sampling");
    assert(std::string(disabled_quality_plan.primary_pass.name)
           == "translucent-rounded-rect");

    MaterialEnvironment zero_tap_env = glass_env;
    zero_tap_env.quality.max_sample_taps = 0;
    auto zero_tap_plan_owner =
        make_test_material_plan(request, zero_tap_env);
    auto& zero_tap_plan = *zero_tap_plan_owner;
    assert(zero_tap_plan.fallback());
    assert(zero_tap_plan.fallback_path == MaterialFallbackPath::QualityPolicy);
    assert(zero_tap_plan.quality_policy.max_sample_taps == 0);
    assert(zero_tap_plan.sample_taps == 0);
    assert(zero_tap_plan.primary_pass.sample_taps == 0);

    MaterialEnvironment budget_env = glass_env;
    budget_env.capabilities.reduce_transparency = false;
    budget_env.quality.max_blur_radius = 12.0f;
    budget_env.quality.max_sample_taps = 7;
    budget_env.quality.max_backdrop_pixels = 600'000;
    budget_env.quality.allow_noise = false;
    budget_env.quality.allow_shadow = false;
    auto budget_plan_owner = make_test_material_plan(request, budget_env);
    auto& budget_plan = *budget_plan_owner;
    assert(budget_plan.blur_radius == 12.0f);
    assert(budget_plan.sample_taps == 5);
    assert(budget_plan.primary_pass.sample_taps == 5);
    assert(budget_plan.noise_opacity == 0.0f);
    assert(budget_plan.shadow_alpha == 0.0f);
    assert(budget_plan.shadow_radius == 0.0f);
    assert(budget_plan.quality_policy.max_blur_radius == 12.0f);
    assert(budget_plan.quality_policy.max_sample_taps == 7);
    assert(budget_plan.quality_policy.max_backdrop_pixels == 600'000);
    assert(!budget_plan.quality_policy.allow_noise);
    assert(!budget_plan.quality_policy.allow_shadow);
    assert(budget_plan.resource_budget.max_blur_radius == 12.0f);
    assert(budget_plan.resource_budget.max_sample_taps == 5);
    assert(budget_plan.resource_budget.max_sampling_kernel_radius == 1);
    assert(budget_plan.resource_budget.max_pass_count == 1);
    assert(budget_plan.resource_budget.max_execution_stages == 4);
    assert(budget_plan.execution_stage_capacity == 4);
    assert(budget_plan.resource_budget.max_backdrop_pixels == 600'000);
    assert(budget_plan.resource_budget.bounded_texture_copy);
    assert(budget_plan.resource_budget.deterministic_fallback);
    assert(budget_plan.execution_stage_count == 2);
    assert(budget_plan.dropped_execution_stage_count == 0);
    assert(std::string(budget_plan.reference_model.performance_response)
           == "budgeted-effects");
    assert(std::string(budget_plan.execution_stages[0].name)
           == "backdrop-sample-blur");
    assert(std::string(budget_plan.execution_stages[1].name)
           == "edge-highlight");

    MaterialEnvironment capability_env = glass_env;
    capability_env.capabilities.max_shader_sample_taps = 9;
    capability_env.capabilities.max_texture_dimension_2d = 1024;
    capability_env.capabilities.max_backdrop_pixels = 500'000;
    capability_env.quality.max_sample_taps = 25;
    capability_env.quality.max_backdrop_pixels = 1'000'000;
    auto capability_plan_owner =
        make_test_material_plan(request, capability_env);
    auto& capability_plan = *capability_plan_owner;
    assert(capability_plan.capability_snapshot.max_shader_sample_taps == 9);
    assert(capability_plan.capability_snapshot.max_texture_dimension_2d == 1024);
    assert(capability_plan.capability_snapshot.max_backdrop_pixels == 500'000);
    assert(capability_plan.capability_snapshot.texture_limits_known);
    assert(capability_plan.capability_snapshot.backdrop_budget_known);
    assert(capability_plan.capability_snapshot.target_within_texture_limits);
    assert(capability_plan.capability_snapshot.target_within_backdrop_budget);
    assert(capability_plan.quality_policy.max_sample_taps == 9);
    assert(capability_plan.quality_policy.max_backdrop_pixels == 500'000);
    assert(capability_plan.sample_taps == 9);
    assert(capability_plan.primary_pass.sample_taps == 9);
    assert(capability_plan.resource_budget.max_sample_taps == 9);
    assert(capability_plan.resource_budget.max_backdrop_pixels == 500'000);

    MaterialEnvironment texture_limit_env = glass_env;
    texture_limit_env.capabilities.max_texture_dimension_2d = 128;
    texture_limit_env.capabilities.max_backdrop_pixels = 1'000'000;
    auto texture_limit_plan_owner =
        make_test_material_plan(request, texture_limit_env);
    auto& texture_limit_plan = *texture_limit_plan_owner;
    assert(texture_limit_plan.fallback());
    assert(texture_limit_plan.fallback_path == MaterialFallbackPath::QualityPolicy);
    assert(texture_limit_plan.capability_snapshot.texture_limits_known);
    assert(!texture_limit_plan.capability_snapshot.target_within_texture_limits);
    assert(texture_limit_plan.capability_snapshot.target_within_backdrop_budget);
    assert(!texture_limit_plan.decision_trace.backdrop_pixels_within_budget);
    assert(!texture_limit_plan.decision_trace.quality_allows_backdrop);
    assert(std::string(texture_limit_plan.decision_trace.first_blocker)
           == "quality-policy");

    MaterialEnvironment excessive_quality_env = glass_env;
    excessive_quality_env.quality.max_blur_radius = 999.0f;
    excessive_quality_env.quality.max_sample_taps = 999;
    auto excessive_quality_plan_owner =
        make_test_material_plan(request, excessive_quality_env);
    auto& excessive_quality_plan = *excessive_quality_plan_owner;
    assert(!excessive_quality_plan.fallback());
    assert(excessive_quality_plan.quality_policy.max_blur_radius
           == material_max_blur_radius);
    assert(excessive_quality_plan.quality_policy.max_sample_taps
           == material_max_sample_taps);
    assert(excessive_quality_plan.resource_budget.max_blur_radius
           == material_max_blur_radius);
    assert(excessive_quality_plan.resource_budget.max_sample_taps
           == material_max_sample_taps);
    assert(excessive_quality_plan.blur_radius <= material_max_blur_radius);
    assert(excessive_quality_plan.sample_taps <= material_max_sample_taps);

    MaterialEnvironment oversize_env = glass_env;
    oversize_env.quality.max_backdrop_pixels = 100;
    auto oversize_plan_owner =
        make_test_material_plan(request, oversize_env);
    auto& oversize_plan = *oversize_plan_owner;
    assert(oversize_plan.fallback());
    assert(oversize_plan.fallback_path == MaterialFallbackPath::QualityPolicy);
    assert(oversize_plan.render_target.pixel_count == 395'200);
    assert(!oversize_plan.render_target.within_backdrop_budget);
    assert(!oversize_plan.decision_trace.backdrop_pixels_within_budget);
    assert(std::string(oversize_plan.decision_trace.first_blocker)
           == "quality-policy");
    assert(oversize_plan.quality_policy.max_backdrop_pixels == 100);
    assert(oversize_plan.resource_budget.max_backdrop_pixels == 100);
    assert(!oversize_plan.backdrop_sampling);
    assert(oversize_plan.sample_taps == 0);
    assert(oversize_plan.sampling_kernel.radius == 0);
    assert(oversize_plan.resource_budget.max_sampling_kernel_radius == 0);
    assert(oversize_plan.primary_pass.sample_taps == 0);
    assert(std::string(oversize_plan.fallback_reason)
           == "quality policy backdrop pixel budget exceeded");

    MaterialRequest invalid_request = request;
    invalid_request.geometry.w = 0.0f;
    auto invalid_plan_owner =
        make_test_material_plan(invalid_request, glass_env);
    auto& invalid_plan = *invalid_plan_owner;
    assert(invalid_plan.fallback());
    assert(invalid_plan.fallback_path == MaterialFallbackPath::InvalidGeometry);
    assert(!invalid_plan.primary_pass.active);
    assert(std::string(invalid_plan.primary_pass.executor) == "none");
    assert(invalid_plan.primary_pass.max_texture_copy_pixels == 0);
    assert(std::string(invalid_plan.verifier.likely_pass) == "none");
    assert(invalid_plan.execution_stage_count == 0);
    assert(invalid_plan.dropped_execution_stage_count == 0);
    assert(invalid_plan.observation_contract.fallback_expected);
    assert(!invalid_plan.observation_contract.backdrop_sampling_expected);
    assert(std::string(invalid_plan.observation_contract.fallback_path)
           == "invalid-geometry");
    assert(std::string(invalid_plan.observation_contract.primary_pass)
           == "none");
    assert(invalid_plan.observation_contract.expected_runtime_passes == 1);
    assert(invalid_plan.observation_contract.expected_active_runtime_passes
           == 0);
    assert(invalid_plan.observation_contract.expected_backdrop_runtime_passes
           == 0);
    assert(invalid_plan.observation_contract.expected_execution_stages == 0);
    assert(!invalid_plan.shape.valid);
    assert(invalid_plan.shape.kind == MaterialShapeKind::Invalid);
    assert(!invalid_plan.shape.rounded);
    assert(!invalid_plan.shape.capsule);
    assert(invalid_plan.shape.effective_radius == 0.0f);
    assert(std::string(invalid_plan.reference_model.shape) == "invalid");
    assert(!invalid_plan.reference_model.view_bounds_anchored);
    assert(!invalid_plan.reference_model.shape_matches_geometry);
    assert(!invalid_plan.reference_model.tint_applied);
    assert(invalid_plan.reference_model.deterministic_degradation);
    assert(!invalid_plan.decision_trace.has_geometry);
    assert(!invalid_plan.decision_trace.has_material);
    assert(std::string(invalid_plan.decision_trace.first_blocker)
           == "invalid-geometry");

    auto capacity_plan_owner = std::make_unique<MaterialPlan>();
    auto& capacity_plan = *capacity_plan_owner;
    capacity_plan.execution_stage_capacity = 1;
    MaterialExecutionStage active_stage{
        .name = "shape-shadow",
        .active = true,
        .requires_backdrop = false,
        .sample_taps = 0,
        .likely_layer = "material-shadow-pass",
        .executor = "shape-shadow",
        .max_texture_copy_pixels = 0,
        .bounded = true,
    };
    append_material_execution_stage(capacity_plan, active_stage);
    append_material_execution_stage(capacity_plan, active_stage);
    assert(capacity_plan.execution_stage_count == 1);
    assert(capacity_plan.dropped_execution_stage_count == 1);

    std::puts("PASS: material planner resolves backdrop and fallback paths");
}

void test_material_text_foreground_resolution() {
    Theme theme{};
    auto style = material_style_for_kind(MaterialKind::Regular, theme);
    MaterialRequest request{
        style,
        MaterialGeometry{12.0f, 20.0f, 240.0f, 96.0f, 10.0f},
    };

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.backdrop.excludes_foreground_text = true;
    env.backdrop.source = "previous-presented-frame";
    env.backdrop.luma_min = 0.84f;
    env.backdrop.luma_max = 0.97f;
    env.backdrop.luma_mean = 0.90f;
    env.render_target.width = 520;
    env.render_target.height = 760;
    env.render_target.scale = 2.0f;

    auto plan_owner = make_test_material_plan(request, env);
    auto& plan = *plan_owner;
    assert(plan.backdrop_sampling);
    assert(plan.foreground.backdrop_driven);
    std::vector<MaterialRuntimeRecord> records{
        MaterialRuntimeRecord{plan, 4u},
    };

    auto primary = material_resolve_text_foreground(
        records,
        5u,
        24.0f,
        36.0f,
        Color{theme.foreground.r, theme.foreground.g, theme.foreground.b, 96},
        theme);
    assert(primary.has_material);
    assert(primary.remapped);
    assert(std::string(primary.role) == "primary");
    assert(primary.material_command_index == 4u);
    assert(material_color_rgb_equal(primary.color, plan.foreground.primary));
    assert(primary.color.a == 96);

    auto secondary = material_resolve_text_foreground(
        records,
        5u,
        24.0f,
        36.0f,
        theme.muted,
        theme);
    assert(secondary.has_material);
    assert(secondary.remapped);
    assert(std::string(secondary.role) == "secondary");
    assert(material_color_rgb_equal(secondary.color, plan.foreground.secondary));

    auto accent = material_resolve_text_foreground(
        records,
        5u,
        24.0f,
        36.0f,
        theme.accent,
        theme);
    assert(accent.has_material);
    assert(accent.remapped);
    assert(std::string(accent.role) == "accent");
    assert(material_color_rgb_equal(accent.color, plan.foreground.accent));

    Color custom{12, 34, 56, 200};
    auto custom_result = material_resolve_text_foreground(
        records,
        5u,
        24.0f,
        36.0f,
        custom,
        theme);
    assert(custom_result.has_material);
    assert(!custom_result.remapped);
    assert(material_color_rgb_equal(custom_result.color, custom));
    assert(custom_result.color.a == custom.a);

    auto outside = material_resolve_text_foreground(
        records,
        5u,
        400.0f,
        36.0f,
        theme.foreground,
        theme);
    assert(!outside.has_material);
    assert(!outside.remapped);

    auto before_material = material_resolve_text_foreground(
        records,
        3u,
        24.0f,
        36.0f,
        theme.foreground,
        theme);
    assert(!before_material.has_material);
    assert(!before_material.remapped);

    std::puts("PASS: material text foreground resolution");
}

void test_material_surface_emits_material_rect_command() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_surface(MaterialKind::Regular, [] {
        widget::text("Glass command");
    });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const* material = static_cast<MaterialRectCmd const*>(nullptr);
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd)) {
            material = m;
            break;
        }
    }

    assert(material != nullptr);
    auto const& descriptor = material->material;
    assert(descriptor.kind == MaterialKind::Regular);
    assert(descriptor.role == MaterialSurfaceRole::Surface);
    assert(descriptor.container.container_id == 0u);
    assert(descriptor.container.union_id == 0u);
    assert(!descriptor.container.interactive);
    assert(descriptor.transition.kind == MaterialGlassTransitionKind::Identity);
    assert(descriptor.transition.progress == 1.0f);
    assert(descriptor.transition.appearing);
    assert(descriptor.glass_identity.namespace_id == 0u);
    assert(descriptor.glass_identity.effect_id == 0u);
    assert(!descriptor.glass_identity.participates());
    assert(descriptor.opacity > 0.5f);
    assert(descriptor.blur_radius >= 20.0f);
    assert(descriptor.tint.a > 0);
    assert(descriptor.saturation > 1.0f);
    assert(descriptor.luminance_floor > 0.0f);
    assert(descriptor.luminance_gain > 1.0f);
    assert(descriptor.edge_highlight > 0.0f);
    assert(descriptor.edge_width >= 1.0f);
    assert(descriptor.noise_opacity > 0.0f);
    assert(descriptor.shadow_alpha > 0.0f);
    assert(descriptor.shadow_radius > 0.0f);

    std::puts("PASS: material surface emits MaterialRect command");
}

void test_material_surface_per_edge_padding_overrides() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::None,
            .padding = SpaceToken::Lg,
            .padding_top = 3.0f,
            .padding_right = 5.0f,
            .padding_bottom = 0.0f,
            .padding_left = 7.0f,
            .gap_px = 0.0f,
        },
        [] {
            widget::text("Per edge padding");
            widget::text("No implicit gap");
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto& surface = detail::node_at(root.children[0]);
    assert(surface.style.padding[0] == 3.0f);
    assert(surface.style.padding[1] == 5.0f);
    assert(surface.style.padding[2] == 0.0f);
    assert(surface.style.padding[3] == 7.0f);
    assert(surface.style.gap == 0.0f);

    std::puts("PASS: material surface supports per-edge padding and gap overrides");
}

void test_material_surface_fixed_outer_height_accounts_for_padding() {
    set_theme(Theme{});
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::None,
            .padding = SpaceToken::Lg,
            .gap_px = 0.0f,
            .fixed_outer_height = 120.0f,
        },
        [] {
            widget::text("Padding-box fixed height");
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto& surface = detail::node_at(root.children[0]);
    float const vertical_padding =
        surface.style.padding[0] + surface.style.padding[2];
    assert(vertical_padding == 32.0f);
    assert(surface.style.fixed_height == 88.0f);
    assert(surface.height == 120.0f);

    std::puts("PASS: material surface fixed_outer_height includes padding");
}

void test_material_surface_shape_overrides() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Regular,
            .role = MaterialSurfaceRole::Content,
            .transition = MaterialTransitionDescriptor{
                .kind = MaterialGlassTransitionKind::Materialize,
                .progress = 0.5f,
                .appearing = false,
            },
            .fixed_height = 40.0f,
            .border_radius = 7.5f,
            .border_width = 0.0f,
            .semantic_label = "Shaped Material",
        },
        [] {
            widget::text("Override radius");
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const* material = static_cast<MaterialRectCmd const*>(nullptr);
    bool saw_stroke = false;
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd))
            material = m;
        if (std::holds_alternative<StrokeRectCmd>(cmd))
            saw_stroke = true;
    }

    assert(material != nullptr);
    assert(std::fabs(material->radius - 7.5f) < 0.0001f);
    assert(material->material.role == MaterialSurfaceRole::Content);
    assert(material->material.transition.kind
           == MaterialGlassTransitionKind::Materialize);
    assert(std::fabs(material->material.transition.progress - 0.5f)
           < 0.0001f);
    assert(!material->material.transition.appearing);
    assert(!saw_stroke);

    std::puts("PASS: material surface shape overrides");
}

void test_material_surface_glass_effect_shape_options() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Regular,
            .fixed_height = 40.0f,
            .border_radius = 7.5f,
            .shape = MaterialSurfaceShape::RoundedRectangle,
            .border_width = 0.0f,
            .semantic_label = "Rounded Glass Shape",
        },
        [] {
            widget::text("Rounded glass");
        });
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Regular,
            .fixed_height = 40.0f,
            .border_radius = 7.5f,
            .shape = MaterialSurfaceShape::Capsule,
            .border_width = 0.0f,
            .semantic_label = "Capsule Glass Shape",
        },
        [] {
            widget::text("Capsule glass");
        });
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Regular,
            .fixed_height = 40.0f,
            .border_radius = 7.5f,
            .shape = MaterialSurfaceShape::Rectangle,
            .border_width = 0.0f,
            .semantic_label = "Rectangle Glass Shape",
        },
        [] {
            widget::text("Rectangle glass");
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    std::vector<MaterialRectCmd> materials;
    for (auto const& cmd : cmds) {
        if (auto const* material = std::get_if<MaterialRectCmd>(&cmd))
            materials.push_back(*material);
    }

    assert(materials.size() == 3u);
    assert(std::fabs(materials[0].radius - 7.5f) < 0.0001f);
    auto const capsule_expected_radius =
        std::min(materials[1].w, materials[1].h) * 0.5f;
    assert(std::fabs(materials[1].radius - capsule_expected_radius) < 0.0001f);
    assert(std::fabs(materials[2].radius) < 0.0001f);

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 120;

    auto rounded_plan = make_test_material_plan(
        material_request_for_command(
            materials[0].material,
            MaterialGeometry{
                materials[0].x,
                materials[0].y,
                materials[0].w,
                materials[0].h,
                materials[0].radius},
            detail::g_app().theme),
        env);
    auto capsule_plan = make_test_material_plan(
        material_request_for_command(
            materials[1].material,
            MaterialGeometry{
                materials[1].x,
                materials[1].y,
                materials[1].w,
                materials[1].h,
                materials[1].radius},
            detail::g_app().theme),
        env);
    auto rectangle_plan = make_test_material_plan(
        material_request_for_command(
            materials[2].material,
            MaterialGeometry{
                materials[2].x,
                materials[2].y,
                materials[2].w,
                materials[2].h,
                materials[2].radius},
            detail::g_app().theme),
        env);

    assert(rounded_plan->shape.kind == MaterialShapeKind::RoundedRectangle);
    assert(capsule_plan->shape.kind == MaterialShapeKind::Capsule);
    assert(capsule_plan->shape.capsule);
    assert(rectangle_plan->shape.kind == MaterialShapeKind::Rectangle);

    std::puts("PASS: material surface glass effect shape options");
}

void test_material_surface_style_override_emits_explicit_material_contract() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto override_material = layout::material_style(MaterialKind::Thin);
    override_material.role = MaterialSurfaceRole::Sidebar;
    override_material.opacity = 0.34f;
    override_material.blur_radius = 28.0f;
    override_material.tint = Color{248, 248, 250, 76};
    override_material.border = Color{255, 255, 255, 84};
    override_material.saturation = 1.28f;
    override_material.contrast_intent = "context";

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Thick,
            .role = MaterialSurfaceRole::Sidebar,
            .fixed_height = 48.0f,
            .border_width = 0.0f,
            .semantic_label = "Custom Sidebar Material",
            .has_material_override = true,
            .material_override = override_material,
        },
        [] {
            widget::text("Sidebar glass");
        });
    Scope::set_current(nullptr);

    auto const& surface = detail::node_at(detail::node_at(root_h).children[0]);
    assert(surface.material.border.a == 84);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const* material = static_cast<MaterialRectCmd const*>(nullptr);
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd))
            material = m;
    }

    assert(material != nullptr);
    assert(material->material.kind == MaterialKind::Thin);
    assert(material->material.role == MaterialSurfaceRole::Sidebar);
    assert(std::fabs(material->material.opacity - 0.34f) < 0.0001f);
    assert(std::fabs(material->material.blur_radius - 28.0f) < 0.0001f);
    assert(material->material.tint.a == 76);
    assert(std::fabs(material->material.saturation - 1.28f) < 0.0001f);

    std::puts("PASS: material surface style override emits explicit material contract");
}

MaterialRectCmd const& first_material_command(
        std::vector<DrawCommand> const& commands) {
    for (auto const& cmd : commands) {
        if (auto const* material = std::get_if<MaterialRectCmd>(&cmd))
            return *material;
    }
    assert(false && "expected MaterialRect command");
    static MaterialRectCmd fallback{};
    return fallback;
}

void test_plain_material_style_disables_liquid_glass_for_chrome_roles() {
    set_theme(Theme{});
    auto style = layout::plain_material_style(
        Color{255, 255, 255, 255},
        Color{209, 209, 214, 255},
        MaterialSurfaceRole::Sidebar,
        "test-plain-sidebar",
        "test-plain-sidebar");
    style.container = MaterialContainerDescriptor{
        4100u,
        0u,
        16.0f,
        false,
        true};
    assert(!style.allows_liquid_glass);
    assert(style.kind == MaterialKind::Regular);
    assert(style.role == MaterialSurfaceRole::Sidebar);
    assert(style.blur_radius == 0.0f);
    assert(style.opacity == 1.0f);

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 640;
    env.render_target.height = 420;
    env.render_target.scale = 2.0f;
    auto plan_owner = make_test_material_plan(
        MaterialRequest{
            style,
            MaterialGeometry{0.0f, 0.0f, 320.0f, 240.0f, 18.0f},
        },
        env);
    auto& plan = *plan_owner;
    assert(plan.role == MaterialSurfaceRole::Sidebar);
    assert(!plan.decision_trace.style_allows_liquid_glass);
    assert(plan.decision_trace.role_allows_liquid_glass);
    assert(plan.decision_trace.content_layer_standard_material);
    assert(!plan.decision_trace.liquid_glass_backdrop_candidate);
    assert(plan.command_descriptor.container.container_id == 0u);
    assert(plan.container.mode == MaterialContainerMode::Isolated);
    assert(!plan.container.participates);
    assert(!plan.reference_model.container_grouped);
    assert(plan.resource_budget.max_container_spacing == 0.0f);
    assert(!plan.verifier.require_container_identity);
    assert(!plan.backdrop_sampling);
    assert(plan.blur_radius == 0.0f);
    assert(plan.opacity == 1.0f);
    assert(std::string(plan.plan_id)
           == "material.regular.standard-material");
    assert(std::string(plan.reference_model.technology)
           == "standard-material");

    auto translucent_style = layout::plain_material_style(
        Color{255, 255, 255, 76},
        Color{209, 209, 214, 255},
        MaterialSurfaceRole::Control,
        "test-translucent-plain-control",
        "test-translucent-plain-control");
    assert(std::fabs(translucent_style.opacity
                     - static_cast<float>(translucent_style.tint.a) / 255.0f)
           < 0.0001f);

    std::puts("PASS: plain material disables liquid glass for chrome roles");
}

void test_interaction_glass_button_uses_plain_idle_material() {
    auto paint_button = [](unsigned int hovered_id,
                           std::optional<std::pair<float, float>> pointer = {}) {
        detail::g_app().arena.reset();
        detail::g_app().prev_arena.reset();
        detail::g_app().callbacks.clear();
        detail::g_app().callback_roles.clear();
        detail::g_app().hovered_id = hovered_id;
        detail::g_app().pressed_id = 0xFFFFFFFFu;
        detail::g_app().focused_id = 0xFFFFFFFFu;
        detail::g_app().focus_visible = false;
        if (pointer.has_value())
            detail::set_pointer_position(pointer->first, pointer->second);
        else
            detail::clear_pointer_position();
        CMD_LEN = 0;

        auto root_h = detail::alloc_node();
        detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
        Scope scope(root_h);
        Scope::set_current(&scope);
        widget::button(
            "State",
            [] {},
            widget::interaction_glass_control_button_style(
                GlassControlStyleOptions{
                    .kind = MaterialKind::Clear,
                    .role = MaterialSurfaceRole::Control,
                    .width = 120.0f,
                    .height = 32.0f,
                    .border_radius = 10.0f,
                }));
        Scope::set_current(nullptr);

        LAYOUT_NODE(root_h, 240.0f);
        PAINT_NODE(root_h, 0, 0, 0, 120.0f);
        return parse_commands(CMD_BUF, CMD_LEN);
    };

    auto idle_commands = paint_button(0xFFFFFFFFu);
    auto const& idle = first_material_command(idle_commands).material;
    assert(!idle.allows_liquid_glass);
    assert(idle.role == MaterialSurfaceRole::Control);
    assert(idle.blur_radius == 0.0f);
    assert(idle.tint.a > 0);
    assert(std::fabs(idle.opacity
                     - static_cast<float>(idle.tint.a) / 255.0f)
           < 0.0001f);

    auto hover_commands = paint_button(0u);
    auto const& hover = first_material_command(hover_commands).material;
    assert(hover.allows_liquid_glass);
    assert(hover.role == MaterialSurfaceRole::Control);
    assert(hover.blur_radius >= 10.0f);
    assert(hover.interaction.hovered);
    assert(hover.interaction.pointer_inside);
    assert(std::fabs(hover.interaction.pointer_x - 0.5f) < 0.0001f);
    assert(std::fabs(hover.interaction.pointer_y - 0.5f) < 0.0001f);

    auto left_pointer_commands =
        paint_button(0u, std::pair<float, float>{4.0f, 16.0f});
    auto const& left_pointer =
        first_material_command(left_pointer_commands).material.interaction;
    assert(left_pointer.hovered);
    assert(left_pointer.pointer_inside);
    assert(std::fabs(left_pointer.pointer_x - 0.5f) < 0.0001f);
    assert(std::fabs(left_pointer.pointer_y - 0.5f) < 0.0001f);

    auto right_pointer_commands =
        paint_button(0u, std::pair<float, float>{116.0f, 16.0f});
    auto const& right_pointer =
        first_material_command(right_pointer_commands).material.interaction;
    assert(right_pointer.hovered);
    assert(right_pointer.pointer_inside);
    assert(std::fabs(right_pointer.pointer_x - 0.5f) < 0.0001f);
    assert(std::fabs(right_pointer.pointer_y - 0.5f) < 0.0001f);

    set_theme(apply_dark_color_scheme(Theme{}));
    auto dark_hover_commands = paint_button(0u);
    auto const& dark_hover = first_material_command(dark_hover_commands).material;
    auto const dark_hover_tint =
        material_with_alpha(detail::g_app().theme.surface, 150);
    assert(dark_hover.tint == dark_hover_tint);
    assert(dark_hover.tint.r < 80);
    assert(dark_hover.tint.g < 80);
    assert(dark_hover.tint.b < 80);
    set_theme(Theme{});

    std::puts("PASS: interaction glass button uses plain idle material");
}

void test_material_surface_resolves_live_input_interaction() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    detail::g_app().callback_roles.clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app().hovered_id = 0u;
    detail::g_app().focused_id = 0u;
    detail::g_app().focus_visible = false;
    detail::g_app().pressed_id = 0u;
    detail::g_app().prev_hovered_id = 0xFFFFFFFFu;
    detail::g_app().prev_focused_id = 0xFFFFFFFFu;
    detail::g_app().prev_focus_visible = false;
    detail::g_app().prev_pressed_id = 0xFFFFFFFFu;
    detail::g_app().prev_pointer_valid = false;
    detail::set_pointer_position(80.0f, 18.0f);
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_container(
        layout::MaterialContainerOptions{
            .container_id = 707u,
            .union_id = 11u,
            .spacing = 16.0f,
            .interactive = true,
            .morph_transitions = true,
        },
        [] {
            layout::material_surface(
                layout::MaterialSurfaceOptions{
                    .kind = MaterialKind::Regular,
                    .role = MaterialSurfaceRole::Toolbar,
                    .fixed_height = 64.0f,
                    .border_width = 0.0f,
                    .semantic_label = "Interactive Glass",
                },
                [] {
                    widget::button(
                        "Action",
                        [] {},
                        ButtonVariant::Default,
                        false);
                });
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    detail::g_app().paint_invalidation_mask =
        detail::compute_paint_invalidation_mask(detail::g_app());
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto commands = parse_commands(CMD_BUF, CMD_LEN);
    auto const& pointer_cmd = first_material_command(commands);
    auto const& pointer = pointer_cmd.material.interaction;
    assert(pointer_cmd.material.container.interactive);
    assert(pointer.hovered);
    assert(pointer.pressed);
    assert(!pointer.focused);
    assert(pointer.pointer_inside);
    assert(pointer.pointer_x > 0.20f && pointer.pointer_x < 0.30f);
    assert(pointer.pointer_y > 0.16f && pointer.pointer_y < 0.24f);

    detail::persist_paint_inputs(detail::g_app());
    detail::g_app().focus_visible = true;
    detail::g_app().pressed_id = 0xFFFFFFFFu;
    CMD_LEN = 0;
    detail::g_app().paint_invalidation_mask =
        detail::compute_paint_invalidation_mask(detail::g_app());
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);
    auto focused_commands = parse_commands(CMD_BUF, CMD_LEN);
    auto const& focused_cmd = first_material_command(focused_commands);
    auto const& focused = focused_cmd.material.interaction;
    assert(focused.hovered);
    assert(!focused.pressed);
    assert(focused.focused);
    assert(focused.pointer_inside);

    detail::g_app().hovered_id = 0xFFFFFFFFu;
    detail::g_app().focused_id = 0xFFFFFFFFu;
    detail::g_app().focus_visible = false;
    detail::g_app().pressed_id = 0xFFFFFFFFu;
    detail::clear_pointer_position();
    std::puts("PASS: material surface resolves live input interaction");
}

void test_capture_only_surface_ignores_live_hover_material_interaction() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    detail::g_app().callback_roles.clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app().hovered_id = 0u;
    detail::g_app().focused_id = 0u;
    detail::g_app().focus_visible = true;
    detail::g_app().pressed_id = 0u;
    detail::g_app().prev_hovered_id = 0xFFFFFFFFu;
    detail::g_app().prev_focused_id = 0xFFFFFFFFu;
    detail::g_app().prev_focus_visible = false;
    detail::g_app().prev_pressed_id = 0xFFFFFFFFu;
    detail::g_app().prev_pointer_valid = false;
    detail::set_pointer_position(40.0f, 20.0f);
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Regular,
            .role = MaterialSurfaceRole::Sidebar,
            .fixed_height = 80.0f,
            .border_width = 0.0f,
            .semantic_label = "Capture Surface",
            .capture_pointer = true,
        },
        [] {});
    Scope::set_current(nullptr);

    assert(detail::g_app().callbacks.size() == 1u);
    LAYOUT_NODE(root_h, 320.0f);
    detail::g_app().paint_invalidation_mask =
        detail::compute_paint_invalidation_mask(detail::g_app());
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto commands = parse_commands(CMD_BUF, CMD_LEN);
    auto const& cmd = first_material_command(commands);
    auto const& interaction = cmd.material.interaction;
    assert(!cmd.material.container.interactive);
    assert(!interaction.hovered);
    assert(!interaction.pressed);
    assert(!interaction.focused);
    assert(!interaction.pointer_inside);

    detail::g_app().hovered_id = 0xFFFFFFFFu;
    detail::g_app().focused_id = 0xFFFFFFFFu;
    detail::g_app().focus_visible = false;
    detail::g_app().pressed_id = 0xFFFFFFFFu;
    detail::clear_pointer_position();
    std::puts("PASS: capture-only surface ignores live hover material interaction");
}

void test_material_surface_interactive_option_enables_plan_response() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Regular,
            .role = MaterialSurfaceRole::Toolbar,
            .interaction = MaterialInteractionDescriptor{
                .hovered = true,
                .pressed = true,
                .focused = false,
                .pointer_inside = true,
                .pointer_x = 0.74f,
                .pointer_y = 0.27f,
            },
            .fixed_height = 56.0f,
            .border_width = 0.0f,
            .semantic_label = "Interactive Surface",
            .interactive = true,
        },
        [] {
            widget::text("Interactive command");
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto const commands = parse_commands(CMD_BUF, CMD_LEN);
    auto const& cmd = first_material_command(commands);
    auto const& descriptor = cmd.material;
    assert(descriptor.container.container_id == 0u);
    assert(descriptor.container.mode() == MaterialContainerMode::Isolated);
    assert(descriptor.container.interactive);
    assert(descriptor.interaction.hovered);
    assert(descriptor.interaction.pressed);
    assert(!descriptor.interaction.focused);
    assert(descriptor.interaction.pointer_inside);
    assert(std::fabs(descriptor.interaction.pointer_x - 0.74f) < 0.0001f);
    assert(std::fabs(descriptor.interaction.pointer_y - 0.27f) < 0.0001f);

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 56;

    auto plan_owner = make_test_material_plan(
        material_request_for_command(
            descriptor,
            MaterialGeometry{cmd.x, cmd.y, cmd.w, cmd.h, cmd.radius},
            detail::g_app().theme),
        env);
    auto& plan = *plan_owner;
    assert(plan.container.interactive);
    assert(plan.interaction.enabled);
    assert(plan.interaction.active);
    assert(plan.interaction.pressed);
    assert(plan.interaction.pointer_inside);
    assert(std::string_view(plan.interaction.enablement_reason)
           == "interactive-container");
    assert(plan.interaction.response_strength > 0.9f);
    assert(plan.reference_model.interactive_response);
    assert(plan.optical_response.interaction_active);
    assert(plan.optical_response.interaction_modulates_optics);

    std::puts("PASS: material surface interactive option enables plan response");
}

void test_glass_effect_surface_api_emits_capsule_tint_contract() {
    auto defaults = layout::glass_effect_surface_options(
        layout::GlassEffectOptions{});
    assert(defaults.kind == MaterialKind::Regular);
    assert(defaults.role == MaterialSurfaceRole::Surface);
    assert(defaults.shape == MaterialSurfaceShape::Capsule);
    assert(defaults.padding == SpaceToken::Md);
    assert(defaults.border_width == 0.0f);
    assert(!defaults.interactive);
    assert(!defaults.has_material_override);

    auto options = layout::GlassEffectOptions{};
    options.kind = MaterialKind::Clear;
    options.role = MaterialSurfaceRole::Control;
    options.interaction = MaterialInteractionDescriptor{
        .hovered = true,
        .pressed = false,
        .focused = true,
        .pointer_inside = true,
        .pointer_x = 0.25f,
        .pointer_y = 0.75f,
    };
    options.fixed_height = 44.0f;
    options.semantic_label = "Tinted Glass Effect";
    options.interactive = true;
    options.has_tint = true;
    auto const glass_tint = Color{64, 156, 255, 118};
    options.tint = glass_tint;
    options.has_border = true;
    auto const glass_border = Color{255, 255, 255, 96};
    options.border = glass_border;

    auto tinted = layout::glass_effect_surface_options(options);
    assert(tinted.kind == MaterialKind::Clear);
    assert(tinted.role == MaterialSurfaceRole::Control);
    assert(tinted.shape == MaterialSurfaceShape::Capsule);
    assert(tinted.interactive);
    assert(tinted.has_material_override);
    assert(tinted.material_override.kind == MaterialKind::Clear);
    assert(tinted.material_override.tint == glass_tint);
    assert(tinted.material_override.border == glass_border);

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_effect(options, [] {
        widget::text("Tinted glass");
    });
    Scope::set_current(nullptr);

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto const& surface = detail::node_at(root.children[0]);
    assert(surface.material.kind == MaterialKind::Clear);
    assert(surface.material.role == MaterialSurfaceRole::Control);
    assert(surface.material.tint == glass_tint);
    assert(surface.material.border == glass_border);
    assert(surface.material.container.interactive);
    assert(surface.material.interaction.focused);
    assert(surface.background == glass_tint);
    assert(surface.border_color == glass_border);
    assert(surface.border_width == 0.0f);
    assert(surface.material_shape == MaterialSurfaceShape::Capsule);
    assert(std::string_view{surface.debug_semantic_label}
           == "Tinted Glass Effect");

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto const commands = parse_commands(CMD_BUF, CMD_LEN);
    auto const& cmd = first_material_command(commands);
    auto const expected_radius = std::min(cmd.w, cmd.h) * 0.5f;
    assert(std::fabs(cmd.radius - expected_radius) < 0.0001f);
    assert(cmd.material.kind == MaterialKind::Clear);
    assert(cmd.material.role == MaterialSurfaceRole::Control);
    assert(cmd.material.tint == glass_tint);
    assert(cmd.material.container.interactive);
    assert(cmd.material.interaction.pointer_inside);
    assert(std::fabs(cmd.material.interaction.pointer_x - 0.25f) < 0.0001f);
    assert(std::fabs(cmd.material.interaction.pointer_y - 0.75f) < 0.0001f);

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 44;

    auto plan = make_test_material_plan(
        material_request_for_command(
            cmd.material,
            MaterialGeometry{cmd.x, cmd.y, cmd.w, cmd.h, cmd.radius},
            detail::g_app().theme),
        env);
    assert(plan->shape.kind == MaterialShapeKind::Capsule);
    assert(plan->interaction.enabled);
    assert(plan->interaction.active);
    assert(plan->reference_model.interactive_response);
    assert(plan->optical_response.interaction_modulates_optics);

    std::puts("PASS: glass effect surface api emits capsule tint contract");
}

void test_glass_effect_shape_argument_anchors_material_contract() {
    auto rectangular_options = layout::glass_effect_options(
        layout::glass_regular(),
        MaterialSurfaceShape::Rectangle);
    assert(rectangular_options.kind == MaterialKind::Regular);
    assert(rectangular_options.shape == MaterialSurfaceShape::Rectangle);

    auto rounded_surface = layout::glass_effect_surface_options(
        layout::glass_effect_options(
            layout::glass_clear().interactive(),
            MaterialSurfaceShape::RoundedRectangle));
    assert(rounded_surface.kind == MaterialKind::Clear);
    assert(rounded_surface.shape == MaterialSurfaceShape::RoundedRectangle);
    assert(rounded_surface.interactive);

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_effect(
        layout::glass_regular(),
        MaterialSurfaceShape::Rectangle,
        [] {
            widget::text("Rectangular glass");
        });
    layout::glass_effect(
        layout::glass_clear().interactive(),
        MaterialSurfaceShape::RoundedRectangle,
        [] {
            widget::text("Rounded glass");
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto const commands = parse_commands(CMD_BUF, CMD_LEN);
    std::vector<MaterialRectCmd> materials;
    for (auto const& cmd : commands) {
        if (auto const* material = std::get_if<MaterialRectCmd>(&cmd))
            materials.push_back(*material);
    }

    assert(materials.size() == 2u);
    assert(std::fabs(materials[0].radius) < 0.0001f);
    assert(materials[0].material.kind == MaterialKind::Regular);
    assert(materials[1].radius > 0.0f);
    assert(materials[1].radius < std::min(materials[1].w, materials[1].h)
           * 0.5f);
    assert(materials[1].material.kind == MaterialKind::Clear);
    assert(materials[1].material.container.interactive);

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 120;

    auto rectangle_plan = make_test_material_plan(
        material_request_for_command(
            materials[0].material,
            MaterialGeometry{
                materials[0].x,
                materials[0].y,
                materials[0].w,
                materials[0].h,
                materials[0].radius},
            detail::g_app().theme),
        env);
    auto rounded_plan = make_test_material_plan(
        material_request_for_command(
            materials[1].material,
            MaterialGeometry{
                materials[1].x,
                materials[1].y,
                materials[1].w,
                materials[1].h,
                materials[1].radius},
            detail::g_app().theme),
        env);
    assert(rectangle_plan->shape.kind == MaterialShapeKind::Rectangle);
    assert(rounded_plan->shape.kind == MaterialShapeKind::RoundedRectangle);
    assert(rounded_plan->interaction.enabled);
    assert(rounded_plan->reference_model.interactive_response);

    std::puts("PASS: glass effect shape argument anchors material contract");
}

void test_glass_background_effect_display_modes_follow_containment_contract() {
    assert(std::string_view{
        layout::glass_background_display_mode_name(
            layout::GlassBackgroundDisplayMode::Always)}
        == "always");
    assert(std::string_view{
        layout::glass_background_display_mode_name(
            layout::GlassBackgroundDisplayMode::Implicit)}
        == "implicit");
    assert(std::string_view{
        layout::glass_background_display_mode_name(
            layout::GlassBackgroundDisplayMode::Never)}
        == "never");
    assert(layout::glass_background_effect_should_display(
        layout::GlassBackgroundDisplayMode::Always,
        true));
    assert(layout::glass_background_effect_should_display(
        layout::GlassBackgroundDisplayMode::Implicit,
        false));
    assert(!layout::glass_background_effect_should_display(
        layout::GlassBackgroundDisplayMode::Implicit,
        true));
    assert(!layout::glass_background_effect_should_display(
        layout::GlassBackgroundDisplayMode::Never,
        false));

    auto defaults = layout::glass_background_effect_surface_options(
        layout::GlassBackgroundEffectOptions{});
    assert(defaults.kind == MaterialKind::Regular);
    assert(defaults.role == MaterialSurfaceRole::Surface);
    assert(defaults.shape == MaterialSurfaceShape::RoundedRectangle);
    assert(defaults.padding == SpaceToken::Md);
    assert(defaults.border_width == 0.0f);
    assert(defaults.has_material_override);
    assert(defaults.material_override.glass_background.kind
           == MaterialGlassBackgroundEffectKind::Automatic);
    assert(std::string_view{defaults.semantic_label} == "Glass Background");

    auto never = layout::glass_background_effect_surface_options(
        layout::GlassBackgroundEffectOptions{
            .display_mode = layout::GlassBackgroundDisplayMode::Never,
            .interactive = true,
        });
    assert(never.kind == MaterialKind::None);
    assert(!never.interactive);
    assert(never.shape == MaterialSurfaceShape::RoundedRectangle);
    assert(!never.has_material_override);

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_background_effect(
        layout::GlassBackgroundEffectOptions{
            .fixed_height = 36.0f,
            .semantic_label = "Always Glass Background",
        },
        [] {
            widget::text("Always background");
        });
    layout::glass_background_effect(
        layout::GlassBackgroundEffectOptions{
            .display_mode = layout::GlassBackgroundDisplayMode::Never,
            .fixed_height = 36.0f,
            .semantic_label = "Never Glass Background",
        },
        [] {
            widget::text("Never background");
        });
    auto outer = layout::GlassEffectOptions{};
    outer.fixed_height = 36.0f;
    outer.semantic_label = "Outer Glass Effect";
    layout::glass_effect(outer, [] {
        layout::glass_background_effect(
            layout::GlassBackgroundEffectOptions{
                .display_mode = layout::GlassBackgroundDisplayMode::Implicit,
                .fixed_height = 24.0f,
                .semantic_label = "Nested Implicit Glass Background",
            },
            [] {
                widget::text("Nested implicit background");
            });
    });
    layout::glass_background_effect(
        MaterialSurfaceShape::Rectangle,
        [] {
            widget::text("Outside implicit background");
        },
        layout::GlassBackgroundDisplayMode::Implicit);
    Scope::set_current(nullptr);

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 4u);
    auto const& never_node = detail::node_at(root.children[1]);
    assert(never_node.material.kind == MaterialKind::None);
    assert(std::string_view{never_node.debug_semantic_label}
           == "Never Glass Background");
    auto const& outer_node = detail::node_at(root.children[2]);
    assert(outer_node.children.size() == 1u);
    auto const& nested_implicit =
        detail::node_at(outer_node.children.front());
    assert(nested_implicit.material.kind == MaterialKind::None);
    assert(std::string_view{nested_implicit.debug_semantic_label}
           == "Nested Implicit Glass Background");

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto const commands = parse_commands(CMD_BUF, CMD_LEN);
    std::vector<MaterialRectCmd> materials;
    for (auto const& cmd : commands) {
        if (auto const* material = std::get_if<MaterialRectCmd>(&cmd))
            materials.push_back(*material);
    }

    assert(materials.size() == 3u);
    assert(materials[0].material.kind == MaterialKind::Regular);
    assert(materials[0].material.role == MaterialSurfaceRole::Surface);
    assert(materials[0].material.glass_background.kind
           == MaterialGlassBackgroundEffectKind::Automatic);
    assert(materials[0].radius > 0.0f);
    assert(materials[0].radius < std::min(materials[0].w, materials[0].h)
           * 0.5f);
    assert(materials[1].material.kind == MaterialKind::Regular);
    assert(materials[1].material.role == MaterialSurfaceRole::Surface);
    assert(materials[1].material.glass_background.kind
           == MaterialGlassBackgroundEffectKind::None);
    assert(materials[2].material.kind == MaterialKind::Regular);
    assert(materials[2].material.glass_background.kind
           == MaterialGlassBackgroundEffectKind::Automatic);
    assert(std::fabs(materials[2].radius) < 0.0001f);

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 36;

    auto background_plan = make_test_material_plan(
        material_request_for_command(
            materials[0].material,
            MaterialGeometry{
                materials[0].x,
                materials[0].y,
                materials[0].w,
                materials[0].h,
                materials[0].radius},
            detail::g_app().theme),
        env);
    assert(background_plan->shape.kind == MaterialShapeKind::RoundedRectangle);
    assert(background_plan->reference_model.view_bounds_anchored);
    assert(std::string_view{background_plan->reference_model.technology}
           == "liquid-glass");

    std::puts("PASS: glass background effect display modes follow containment contract");
}

void test_glass_background_effect_variants_map_to_material_contract() {
    auto const automatic_surface = layout::glass_background_effect_surface_options(
        layout::GlassBackgroundEffectOptions{});
    auto const plate_surface = layout::glass_background_effect_surface_options(
        layout::GlassBackgroundEffectOptions{
            .display_mode = layout::GlassBackgroundDisplayMode::Always,
            .effect = layout::glass_background_plate(),
        });
    auto const feathered_surface = layout::glass_background_effect_surface_options(
        layout::GlassBackgroundEffectOptions{
            .display_mode = layout::GlassBackgroundDisplayMode::Always,
            .effect = layout::glass_background_feathered(20.0f, 9.0f),
        });

    assert(automatic_surface.material_override.glass_background.kind
           == MaterialGlassBackgroundEffectKind::Automatic);
    assert(plate_surface.material_override.glass_background.kind
           == MaterialGlassBackgroundEffectKind::Plate);
    assert(plate_surface.material_override.opacity
           > automatic_surface.material_override.opacity);
    assert(plate_surface.material_override.blur_radius
           > automatic_surface.material_override.blur_radius);
    assert(feathered_surface.material_override.glass_background.kind
           == MaterialGlassBackgroundEffectKind::Feathered);
    assert(feathered_surface.material_override.glass_background.feather_padding
           == 20.0f);
    assert(feathered_surface.material_override.glass_background.soft_edge_radius
           == 9.0f);
    assert(feathered_surface.material_override.edge_width
           > automatic_surface.material_override.edge_width);

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_background_effect(
        layout::glass_background_plate(),
        [] {
            widget::text("Plate background");
        });
    layout::glass_background_effect(
        layout::glass_background_feathered(20.0f, 9.0f),
        MaterialSurfaceShape::Capsule,
        [] {
            widget::text("Feathered background");
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 240.0f);

    auto const commands = parse_commands(CMD_BUF, CMD_LEN);
    std::vector<MaterialRectCmd> materials;
    for (auto const& cmd : commands) {
        if (auto const* material = std::get_if<MaterialRectCmd>(&cmd))
            materials.push_back(*material);
    }

    assert(materials.size() == 2u);
    assert(materials[0].material.glass_background.kind
           == MaterialGlassBackgroundEffectKind::Plate);
    assert(materials[1].material.glass_background.kind
           == MaterialGlassBackgroundEffectKind::Feathered);
    assert(materials[1].material.glass_background.feather_padding == 20.0f);
    assert(materials[1].material.glass_background.soft_edge_radius == 9.0f);
    assert(std::fabs(materials[1].radius - materials[1].h * 0.5f)
           < 0.0001f);

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 96;

    auto plate_plan = make_test_material_plan(
        material_request_for_command(
            materials[0].material,
            MaterialGeometry{
                materials[0].x,
                materials[0].y,
                materials[0].w,
                materials[0].h,
                materials[0].radius},
            detail::g_app().theme),
        env);
    auto feathered_plan = make_test_material_plan(
        material_request_for_command(
            materials[1].material,
            MaterialGeometry{
                materials[1].x,
                materials[1].y,
                materials[1].w,
                materials[1].h,
                materials[1].radius},
            detail::g_app().theme),
        env);

    assert(plate_plan->glass_background.plate);
    assert(std::string_view{
        plate_plan->reference_model.glass_background_effect}
        == "plate");
    assert(plate_plan->reference_model.glass_background_plate);
    assert(feathered_plan->glass_background.feathered);
    assert(feathered_plan->glass_background.feather_padding == 20.0f);
    assert(feathered_plan->glass_background.soft_edge_radius == 9.0f);
    assert(std::string_view{
        feathered_plan->reference_model.glass_background_effect}
        == "feathered");
    assert(feathered_plan->reference_model.glass_background_feathered);

    std::puts("PASS: glass background effect variants map to material contract");
}

void test_glass_effect_style_variants_map_to_material_contract() {
    auto const accent_tint = Color{64, 156, 255, 112};
    auto regular = layout::glass_effect_options(
        layout::glass_regular()
            .tint(accent_tint)
            .interactive());
    assert(regular.kind == MaterialKind::Regular);
    assert(regular.interactive);
    assert(regular.has_tint);
    assert(regular.tint == accent_tint);
    assert(regular.shape == MaterialSurfaceShape::Capsule);

    auto const clear_border = Color{255, 255, 255, 120};
    auto clear = layout::glass_effect_surface_options(
        layout::glass_effect_options(
            layout::glass_clear()
                .tint(accent_tint)
                .border(clear_border)
                .interactive()));
    assert(clear.kind == MaterialKind::Clear);
    assert(clear.interactive);
    assert(clear.has_material_override);
    assert(clear.material_override.kind == MaterialKind::Clear);
    assert(clear.material_override.tint == accent_tint);
    assert(clear.material_override.border == clear_border);

    auto identity = layout::glass_effect_options(
        layout::glass_identity()
            .tint(accent_tint)
            .border(clear_border)
            .interactive());
    assert(identity.kind == MaterialKind::None);
    assert(!identity.interactive);
    assert(!identity.has_tint);
    assert(!identity.has_border);

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto identity_root_h = detail::alloc_node();
    detail::node_at(identity_root_h).style.flex_direction =
        FlexDirection::Column;
    Scope identity_scope(identity_root_h);
    Scope::set_current(&identity_scope);
    layout::glass_effect(layout::glass_identity().interactive(), [] {
        widget::text("Identity glass");
    });
    Scope::set_current(nullptr);

    auto const& identity_root = detail::node_at(identity_root_h);
    assert(identity_root.children.size() == 1);
    auto const& identity_surface =
        detail::node_at(identity_root.children[0]);
    assert(identity_surface.material.kind == MaterialKind::None);
    assert(identity_surface.material_shape == MaterialSurfaceShape::Capsule);
    assert(identity_surface.background.a == 0);
    assert(!identity_surface.material.container.interactive);

    LAYOUT_NODE(identity_root_h, 320.0f);
    PAINT_NODE(identity_root_h, 0, 0, 0, 600.0f);
    auto identity_commands = parse_commands(CMD_BUF, CMD_LEN);
    bool identity_emitted_material = false;
    for (auto const& cmd : identity_commands) {
        identity_emitted_material =
            identity_emitted_material
            || std::holds_alternative<MaterialRectCmd>(cmd);
    }
    assert(!identity_emitted_material);

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto clear_root_h = detail::alloc_node();
    detail::node_at(clear_root_h).style.flex_direction =
        FlexDirection::Column;
    Scope clear_scope(clear_root_h);
    Scope::set_current(&clear_scope);
    layout::glass_effect(
        layout::glass_clear().tint(accent_tint).interactive(),
        [] {
            widget::text("Clear glass");
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(clear_root_h, 320.0f);
    PAINT_NODE(clear_root_h, 0, 0, 0, 600.0f);
    auto clear_commands = parse_commands(CMD_BUF, CMD_LEN);
    auto const& clear_cmd = first_material_command(clear_commands);
    assert(clear_cmd.material.kind == MaterialKind::Clear);
    assert(clear_cmd.material.tint == accent_tint);
    assert(clear_cmd.material.container.interactive);

    std::puts("PASS: glass effect style variants map to material contract");
}

void test_glass_effect_style_carries_transition_and_identity() {
    auto const expected_identity =
        layout::glass_effect_identity("showcase", "surface");
    auto style = layout::glass_regular()
        .tint(Color{64, 156, 255, 112})
        .effect_id(expected_identity)
        .matched_geometry(0.45f, false);
    auto options = layout::glass_effect_options(style);
    assert(options.kind == MaterialKind::Regular);
    assert(options.glass_identity == expected_identity);
    assert(!options.inherit_material_identity);
    assert(options.transition.kind
           == MaterialGlassTransitionKind::MatchedGeometry);
    assert(std::fabs(options.transition.progress - 0.45f) < 0.0001f);
    assert(!options.transition.appearing);
    assert(!options.inherit_material_transition);

    auto materialize = layout::glass_effect_options(
        layout::glass_clear()
            .effect_id("showcase", "panel")
            .materialize(0.25f));
    assert(materialize.kind == MaterialKind::Clear);
    assert(materialize.glass_identity
           == layout::glass_effect_identity("showcase", "panel"));
    assert(materialize.transition.kind
           == MaterialGlassTransitionKind::Materialize);
    assert(std::fabs(materialize.transition.progress - 0.25f) < 0.0001f);
    assert(materialize.transition.appearing);

    auto inherited = layout::glass_effect_options(layout::glass_regular());
    assert(inherited.inherit_material_transition);
    assert(inherited.inherit_material_identity);

    auto identity_style = layout::glass_effect_options(
        layout::glass_identity()
            .effect_id(expected_identity)
            .materialize(0.25f)
            .interactive());
    assert(identity_style.kind == MaterialKind::None);
    assert(identity_style.inherit_material_transition);
    assert(identity_style.inherit_material_identity);

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_effect_transition(
        layout::glass_materialize_transition(0.2f),
        [&] {
            layout::glass_effect(
                layout::glass_regular()
                    .effect_id("showcase", "surface")
                    .matched_geometry(0.6f, false),
                [] {
                    widget::text("Matched style");
                });
            layout::glass_effect(
                layout::glass_regular().identity_transition(),
                [] {
                    widget::text("Identity style");
                });
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    std::vector<MaterialCommandDescriptor> materials;
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd))
            materials.push_back(m->material);
    }

    assert(materials.size() == 2u);
    assert(materials[0].glass_identity == expected_identity);
    assert(materials[0].transition.kind
           == MaterialGlassTransitionKind::MatchedGeometry);
    assert(std::fabs(materials[0].transition.progress - 0.6f) < 0.0001f);
    assert(!materials[0].transition.appearing);
    assert(materials[1].transition.kind == MaterialGlassTransitionKind::Identity);
    assert(materials[1].transition.progress == 1.0f);
    assert(materials[1].transition.appearing);

    std::puts("PASS: glass effect style carries transition and identity");
}

void test_glass_effect_style_carries_union_context() {
    auto const union_descriptor = layout::glass_effect_union_descriptor(
        layout::GlassEffectUnionOptions{
            .namespace_id = 771u,
            .union_id = 12u,
            .spacing = 18.0f,
            .interactive = true,
            .morph_transitions = true,
        });
    auto style = layout::glass_regular()
        .tint(Color{64, 156, 255, 96})
        .effect_union(union_descriptor);
    auto options = layout::glass_effect_options(style);
    assert(options.kind == MaterialKind::Regular);
    assert(options.container == union_descriptor);
    assert(!options.inherit_material_container);

    auto stable = layout::glass_effect_options(
        layout::glass_clear()
            .effect_union("showcase.union", "weather", 14.0f));
    assert(stable.container.container_id
           == layout::glass_effect_stable_id("showcase.union"));
    assert(stable.container.union_id
           == layout::glass_effect_stable_id("weather"));
    assert(std::fabs(stable.container.spacing - 14.0f) < 0.0001f);
    assert(stable.container.mode() == MaterialContainerMode::Union);
    assert(stable.container.morph_transitions);
    assert(!stable.inherit_material_container);

    auto inherited = layout::glass_effect_options(layout::glass_regular());
    assert(inherited.inherit_material_container);

    auto ignored_identity = layout::glass_effect_options(
        layout::glass_identity().effect_union(union_descriptor));
    assert(ignored_identity.kind == MaterialKind::None);
    assert(ignored_identity.inherit_material_container);

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_effect(
        layout::glass_regular().effect_union(union_descriptor),
        [] {
            widget::text("Union A");
        });
    layout::glass_effect(
        layout::glass_regular().effect_union(771u, 12u, 18.0f, true),
        [] {
            widget::text("Union B");
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    std::vector<MaterialRectCmd> materials;
    for (auto const& cmd : cmds) {
        if (auto const* material = std::get_if<MaterialRectCmd>(&cmd))
            materials.push_back(*material);
    }

    assert(materials.size() == 2u);
    for (auto const& material : materials) {
        auto const& container = material.material.container;
        assert(container.container_id == 771u);
        assert(container.union_id == 12u);
        assert(std::fabs(container.spacing - 18.0f) < 0.0001f);
        assert(container.interactive);
        assert(container.morph_transitions);
        assert(container.mode() == MaterialContainerMode::Union);
    }

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 120;

    auto plan = make_test_material_plan(
        material_request_for_command(
            materials[0].material,
            MaterialGeometry{
                materials[0].x,
                materials[0].y,
                materials[0].w,
                materials[0].h,
                materials[0].radius},
            detail::g_app().theme),
        env);
    assert(plan->container.mode == MaterialContainerMode::Union);
    assert(plan->container.shape_union_expected);
    assert(plan->reference_model.container_union);

    std::puts("PASS: glass effect style carries union context");
}

void test_glass_surface_presets_emit_material_contract() {
    auto toolbar = layout::glass_surface_options(
        layout::GlassSurfacePreset::Toolbar);
    assert(toolbar.kind == MaterialKind::Clear);
    assert(toolbar.role == MaterialSurfaceRole::Toolbar);
    assert(toolbar.direction == FlexDirection::Row);
    assert(toolbar.cross_align == CrossAxisAlignment::Center);
    assert(toolbar.border_radius == 0.0f);
    assert(toolbar.shape == MaterialSurfaceShape::Rectangle);
    assert(!toolbar.interactive);
    assert(std::string_view{toolbar.semantic_label} == "Toolbar");
    auto toolbar_group = layout::glass_surface_options(
        layout::GlassSurfacePreset::ToolbarGroup);
    assert(toolbar_group.interactive);
    assert(toolbar_group.shape == MaterialSurfaceShape::Capsule);
    auto segmented = layout::glass_surface_options(
        layout::GlassSurfacePreset::SegmentedControl);
    assert(segmented.kind == MaterialKind::Regular);
    assert(segmented.role == MaterialSurfaceRole::Navigation);
    assert(segmented.direction == FlexDirection::Row);
    assert(segmented.interactive);
    assert(segmented.shape == MaterialSurfaceShape::Capsule);
    assert(std::string_view{segmented.semantic_label}
           == "Segmented Control");
    auto navigation = layout::glass_surface_options(
        layout::GlassSurfacePreset::Navigation);
    assert(navigation.interactive);
    assert(navigation.shape == MaterialSurfaceShape::Capsule);
    auto popover = layout::glass_surface_options(
        layout::GlassSurfacePreset::Popover,
        "Actions");
    assert(popover.kind == MaterialKind::Regular);
    assert(popover.role == MaterialSurfaceRole::Overlay);
    assert(popover.direction == FlexDirection::Column);
    assert(popover.interactive);
    assert(std::string_view{popover.semantic_label} == "Actions");
    auto sheet = layout::glass_surface_options(
        layout::GlassSurfacePreset::Sheet,
        "Confirm Sheet");
    assert(sheet.kind == MaterialKind::Regular);
    assert(sheet.role == MaterialSurfaceRole::Overlay);
    assert(sheet.padding == SpaceToken::Lg);
    assert(sheet.interactive);
    assert(std::string_view{sheet.semantic_label} == "Confirm Sheet");
    auto inspector = layout::glass_surface_options(
        layout::GlassSurfacePreset::Inspector);
    assert(inspector.kind == MaterialKind::Thin);
    assert(inspector.role == MaterialSurfaceRole::Overlay);
    assert(inspector.interactive);
    auto command_palette = layout::glass_surface_options(
        layout::GlassSurfacePreset::CommandPalette);
    assert(command_palette.kind == MaterialKind::Thick);
    assert(command_palette.role == MaterialSurfaceRole::Overlay);
    assert(command_palette.interactive);
    assert(std::string_view{
        layout::glass_surface_preset_name(
            layout::GlassSurfacePreset::ToolbarGroup)}
        == "toolbar_group");
    assert(std::string_view{
        layout::glass_surface_preset_name(
            layout::GlassSurfacePreset::SegmentedControl)}
        == "segmented_control");
    assert(std::string_view{
        layout::glass_surface_preset_name(
            layout::GlassSurfacePreset::CommandPalette)}
        == "command_palette");

    auto content = layout::glass_surface_options(
        layout::GlassSurfacePreset::Content,
        "Files");
    assert(content.kind == MaterialKind::Regular);
    assert(content.role == MaterialSurfaceRole::Content);
    assert(content.padding == SpaceToken::Xl);
    assert(!content.interactive);
    assert(std::string_view{content.semantic_label} == "Files");

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_surface(
        layout::GlassSurfacePreset::ToolbarGroup,
        [] {
            widget::text("Preset command");
        },
        "Preset Toolbar Group");
    Scope::set_current(nullptr);

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto const& surface = detail::node_at(root.children[0]);
    assert(surface.material.kind == MaterialKind::Thick);
    assert(surface.material.role == MaterialSurfaceRole::Toolbar);
    assert(surface.border_width == 0.0f);
    assert(surface.border_radius == detail::g_app().theme.radius_lg);
    assert(surface.material_shape == MaterialSurfaceShape::Capsule);
    assert(std::string_view{surface.debug_semantic_label}
           == "Preset Toolbar Group");

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const* material = static_cast<MaterialRectCmd const*>(nullptr);
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd)) {
            material = m;
            break;
        }
    }
    assert(material != nullptr);
    assert(material->material.kind == MaterialKind::Thick);
    assert(material->material.role == MaterialSurfaceRole::Toolbar);
    assert(material->material.container.interactive);
    auto const expected_radius = std::min(material->w, material->h) * 0.5f;
    assert(std::fabs(material->radius - expected_radius) < 0.0001f);

    auto const nav_identity =
        layout::glass_effect_identity("surfaces", "navigation");
    auto const nav_container_id =
        layout::glass_effect_stable_id("surfaces.scope");
    auto const nav_union_id =
        layout::glass_effect_stable_id("surfaces.nav");
    auto const nav_tint = Color{64, 156, 255, 82};
    auto const nav_border = Color{64, 156, 255, 146};
    auto styled_nav = layout::glass_surface_options(
        layout::GlassSurfacePreset::Navigation,
        layout::glass_clear()
            .tint(nav_tint)
            .border(nav_border)
            .effect_id(nav_identity)
            .matched_geometry(0.45f, false)
            .effect_union(
                "surfaces.scope",
                "surfaces.nav",
                18.0f,
                true,
                true),
        "Glass Navigation");
    assert(styled_nav.kind == MaterialKind::Clear);
    assert(styled_nav.role == MaterialSurfaceRole::Navigation);
    assert(styled_nav.direction == FlexDirection::Row);
    assert(styled_nav.interactive);
    assert(styled_nav.has_material_override);
    assert(styled_nav.material_override.kind == MaterialKind::Clear);
    assert(styled_nav.material_override.tint == nav_tint);
    assert(styled_nav.material_override.border == nav_border);
    assert(styled_nav.transition.kind
           == MaterialGlassTransitionKind::MatchedGeometry);
    assert(std::fabs(styled_nav.transition.progress - 0.45f) < 0.0001f);
    assert(!styled_nav.transition.appearing);
    assert(!styled_nav.inherit_material_transition);
    assert(styled_nav.glass_identity == nav_identity);
    assert(!styled_nav.inherit_material_identity);
    assert(styled_nav.container.container_id == nav_container_id);
    assert(styled_nav.container.union_id == nav_union_id);
    assert(std::fabs(styled_nav.container.spacing - 18.0f) < 0.0001f);
    assert(!styled_nav.inherit_material_container);
    assert(std::string_view{styled_nav.semantic_label} == "Glass Navigation");

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto sheet_root_h = detail::alloc_node();
    detail::node_at(sheet_root_h).style.flex_direction = FlexDirection::Column;
    Scope sheet_scope(sheet_root_h);
    Scope::set_current(&sheet_scope);
    layout::glass_surface(
        layout::GlassSurfacePreset::Sheet,
        layout::glass_regular()
            .tint(Color{255, 255, 255, 142})
            .effect_id("surfaces", "sheet")
            .materialize(0.6f, true)
            .effect_union("surfaces.scope", "sheet", 22.0f, true, true),
        [] {
            widget::text("Glass sheet");
        },
        "Glass Sheet");
    Scope::set_current(nullptr);

    auto const& sheet_root = detail::node_at(sheet_root_h);
    assert(sheet_root.children.size() == 1u);
    auto const& sheet_surface = detail::node_at(sheet_root.children[0]);
    assert(sheet_surface.material.kind == MaterialKind::Regular);
    assert(sheet_surface.material.role == MaterialSurfaceRole::Overlay);
    assert(sheet_surface.material.transition.kind
           == MaterialGlassTransitionKind::Materialize);
    assert(std::fabs(sheet_surface.material.transition.progress - 0.6f)
           < 0.0001f);
    assert(sheet_surface.material.glass_identity
           == layout::glass_effect_identity("surfaces", "sheet"));
    assert(sheet_surface.material.container.container_id
           == nav_container_id);
    assert(sheet_surface.material.container.union_id
           == layout::glass_effect_stable_id("sheet"));
    assert(std::fabs(sheet_surface.material.container.spacing - 22.0f)
           < 0.0001f);
    assert(sheet_surface.material.container.interactive);
    assert(std::string_view{sheet_surface.debug_semantic_label}
           == "Glass Sheet");

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto scoped_root_h = detail::alloc_node();
    detail::node_at(scoped_root_h).style.flex_direction = FlexDirection::Column;
    Scope scoped_scope(scoped_root_h);
    Scope::set_current(&scoped_scope);
    layout::glass_effect_container(
        layout::GlassEffectContainerOptions{
            .container_id = 906u,
            .union_id = 44u,
            .spacing = 15.0f,
            .interactive = false,
            .morph_transitions = true,
        },
        [] {
            layout::glass_effect_transition(
                layout::glass_matched_geometry_transition(0.35f, false),
                [] {
                    layout::glass_surface(
                        layout::GlassSurfacePreset::ToolbarGroup,
                        layout::glass_clear()
                            .effect_id("surfaces", "scoped-toolbar"),
                        [] {
                            widget::text("Scoped toolbar");
                        });
                });
        });
    Scope::set_current(nullptr);

    auto const& scoped_root = detail::node_at(scoped_root_h);
    assert(scoped_root.children.size() == 1u);
    auto const& scoped_surface = detail::node_at(scoped_root.children[0]);
    assert(scoped_surface.material.kind == MaterialKind::Clear);
    assert(scoped_surface.material.role == MaterialSurfaceRole::Toolbar);
    assert(scoped_surface.material.transition.kind
           == MaterialGlassTransitionKind::MatchedGeometry);
    assert(std::fabs(scoped_surface.material.transition.progress - 0.35f)
           < 0.0001f);
    assert(!scoped_surface.material.transition.appearing);
    assert(scoped_surface.material.glass_identity
           == layout::glass_effect_identity("surfaces", "scoped-toolbar"));
    assert(scoped_surface.material.container.container_id == 906u);
    assert(scoped_surface.material.container.union_id == 44u);
    assert(std::fabs(scoped_surface.material.container.spacing - 15.0f)
           < 0.0001f);
    assert(scoped_surface.material.container.interactive);

    std::puts("PASS: glass surface presets emit material contract");
}

void test_material_container_scope_emits_command_context() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::material_container(
        layout::MaterialContainerOptions{
            .container_id = 501u,
            .union_id = 9u,
            .spacing = 22.0f,
            .interactive = true,
            .morph_transitions = true,
        },
        [] {
            layout::material_surface(MaterialKind::Thin, [] {
                widget::text("Container glass");
            });
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const* material = static_cast<MaterialRectCmd const*>(nullptr);
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd)) {
            material = m;
            break;
        }
    }

    assert(material != nullptr);
    auto const& container = material->material.container;
    assert(container.container_id == 501u);
    assert(container.union_id == 9u);
    assert(std::fabs(container.spacing - 22.0f) < 0.0001f);
    assert(container.interactive);
    assert(container.morph_transitions);

    std::puts("PASS: material container scope emits command context");
}

void test_glass_effect_container_scope_emits_morph_context() {
    auto defaults = layout::glass_effect_container_options(
        layout::GlassEffectContainerOptions{
            .container_id = 601u,
        });
    assert(defaults.container_id == 601u);
    assert(defaults.union_id == 0u);
    assert(defaults.spacing == 0.0f);
    assert(!defaults.interactive);
    assert(defaults.morph_transitions);

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_effect_container(
        layout::GlassEffectContainerOptions{
            .container_id = 602u,
            .union_id = 17u,
            .spacing = 18.0f,
            .interactive = true,
            .morph_transitions = true,
        },
        [] {
            auto first = layout::GlassEffectOptions{};
            first.role = MaterialSurfaceRole::Control;
            first.fixed_height = 36.0f;
            first.semantic_label = "First Glass Effect";
            layout::glass_effect(first, [] {
                widget::text("One");
            });

            auto second = layout::GlassEffectOptions{};
            second.role = MaterialSurfaceRole::Control;
            second.fixed_height = 36.0f;
            second.semantic_label = "Second Glass Effect";
            layout::glass_effect(second, [] {
                widget::text("Two");
            });
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    std::vector<MaterialRectCmd> materials;
    for (auto const& cmd : cmds) {
        if (auto const* material = std::get_if<MaterialRectCmd>(&cmd))
            materials.push_back(*material);
    }

    assert(materials.size() == 2u);
    for (auto const& material : materials) {
        auto const& container = material.material.container;
        assert(container.container_id == 602u);
        assert(container.union_id == 17u);
        assert(std::fabs(container.spacing - 18.0f) < 0.0001f);
        assert(container.interactive);
        assert(container.morph_transitions);
        assert(container.mode() == MaterialContainerMode::Union);
        assert(material.material.kind == MaterialKind::Regular);
        assert(material.material.role == MaterialSurfaceRole::Control);
        assert(material.material.transition.kind
               == MaterialGlassTransitionKind::MatchedGeometry);
        assert(material.material.transition.progress == 1.0f);
        assert(material.material.transition.appearing);
    }

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 80;

    auto plan = make_test_material_plan(
        material_request_for_command(
            materials[0].material,
            MaterialGeometry{
                materials[0].x,
                materials[0].y,
                materials[0].w,
                materials[0].h,
                materials[0].radius},
            detail::g_app().theme),
        env);
    assert(plan->container.mode == MaterialContainerMode::Union);
    assert(plan->container.shared_backdrop_scope);
    assert(plan->container.shape_union_expected);
    assert(plan->container.shape_blending_expected);
    assert(plan->container.morph_transitions);
    assert(std::string_view{plan->container.blend_policy}
           == "union-shape-proximity");
    assert(std::string_view{plan->container.morph_policy} == "union-morph");
    assert(plan->reference_model.container_grouped);
    assert(plan->reference_model.container_union);
    assert(plan->reference_model.container_morphing);
    assert(plan->reference_model.interactive_response);
    assert(plan->transition.matched_geometry);
    assert(std::string_view(plan->transition.kind_name)
           == "matched-geometry");

    std::puts("PASS: glass effect container scope emits morph context");
}

void test_glass_effect_union_scope_emits_view_union_context() {
    auto direct = layout::glass_effect_union_descriptor(
        layout::GlassEffectUnionOptions{
            .namespace_id = 701u,
            .union_id = 9u,
            .spacing = 14.0f,
            .interactive = true,
            .morph_transitions = true,
        });
    assert(direct.container_id == 701u);
    assert(direct.union_id == 9u);
    assert(std::fabs(direct.spacing - 14.0f) < 0.0001f);
    assert(direct.interactive);
    assert(direct.morph_transitions);
    assert(direct.mode() == MaterialContainerMode::Union);

    auto stable = layout::glass_effect_union_descriptor(
        "showcase.controls",
        "filters",
        12.0f);
    assert(stable.container_id
           == layout::glass_effect_stable_id("showcase.controls"));
    assert(stable.union_id == layout::glass_effect_stable_id("filters"));
    assert(stable.mode() == MaterialContainerMode::Union);

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_effect_container(
        layout::GlassEffectContainerOptions{
            .container_id = 702u,
            .spacing = 18.0f,
            .interactive = true,
            .morph_transitions = true,
        },
        [] {
            auto outside = layout::GlassEffectOptions{};
            outside.role = MaterialSurfaceRole::Control;
            outside.fixed_height = 36.0f;
            outside.semantic_label = "Outside Glass Union";
            layout::glass_effect(outside, [] {
                widget::text("Outside");
            });

            layout::glass_effect_union(9u, [] {
                auto first = layout::GlassEffectOptions{};
                first.role = MaterialSurfaceRole::Control;
                first.fixed_height = 36.0f;
                first.semantic_label = "Union Glass One";
                layout::glass_effect(first, [] {
                    widget::text("One");
                });

                auto second = layout::GlassEffectOptions{};
                second.role = MaterialSurfaceRole::Control;
                second.fixed_height = 36.0f;
                second.semantic_label = "Union Glass Two";
                layout::glass_effect(second, [] {
                    widget::text("Two");
                });
            });
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    std::vector<MaterialRectCmd> materials;
    for (auto const& cmd : cmds) {
        if (auto const* material = std::get_if<MaterialRectCmd>(&cmd))
            materials.push_back(*material);
    }

    assert(materials.size() == 3u);
    assert(materials[0].material.container.container_id == 702u);
    assert(materials[0].material.container.union_id == 0u);
    assert(materials[0].material.container.mode()
           == MaterialContainerMode::Container);
    for (auto index = std::size_t{1u}; index < materials.size(); ++index) {
        auto const& material = materials[index].material;
        assert(material.container.container_id == 702u);
        assert(material.container.union_id == 9u);
        assert(std::fabs(material.container.spacing - 18.0f) < 0.0001f);
        assert(material.container.interactive);
        assert(material.container.morph_transitions);
        assert(material.container.mode() == MaterialContainerMode::Union);
        assert(material.transition.kind
               == MaterialGlassTransitionKind::MatchedGeometry);
    }

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 120;

    auto union_plan = make_test_material_plan(
        material_request_for_command(
            materials[1].material,
            MaterialGeometry{
                materials[1].x,
                materials[1].y,
                materials[1].w,
                materials[1].h,
                materials[1].radius},
            detail::g_app().theme),
        env);
    assert(union_plan->container.mode == MaterialContainerMode::Union);
    assert(union_plan->container.shape_union_expected);
    assert(union_plan->container.morph_transitions);
    assert(union_plan->reference_model.container_union);

    std::puts("PASS: glass effect union scope emits view union context");
}

void test_glass_effect_transition_scope_emits_command_context() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_effect_transition(
        layout::glass_materialize_transition(0.35f, false),
        [] {
            layout::material_surface(
                layout::MaterialSurfaceOptions{
                    .kind = MaterialKind::Regular,
                    .fixed_height = 36.0f,
                    .border_width = 0.0f,
                    .semantic_label = "Inherited Transition",
                },
                [] {
                    widget::text("Inherited transition");
                });
            layout::material_surface(
                layout::MaterialSurfaceOptions{
                    .kind = MaterialKind::Thin,
                    .transition = layout::glass_matched_geometry_transition(
                        0.8f),
                    .fixed_height = 36.0f,
                    .border_width = 0.0f,
                    .semantic_label = "Matched Transition",
                },
                [] {
                    widget::text("Matched transition");
                });
            layout::material_surface(
                layout::MaterialSurfaceOptions{
                    .kind = MaterialKind::Clear,
                    .inherit_material_transition = false,
                    .fixed_height = 36.0f,
                    .border_width = 0.0f,
                    .semantic_label = "Identity Transition",
                },
                [] {
                    widget::text("Identity transition");
                });
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    std::vector<MaterialCommandDescriptor> materials;
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd))
            materials.push_back(m->material);
    }

    assert(materials.size() == 3u);
    assert(materials[0].transition.kind
           == MaterialGlassTransitionKind::Materialize);
    assert(std::fabs(materials[0].transition.progress - 0.35f) < 0.0001f);
    assert(!materials[0].transition.appearing);

    assert(materials[1].transition.kind
           == MaterialGlassTransitionKind::MatchedGeometry);
    assert(std::fabs(materials[1].transition.progress - 0.8f) < 0.0001f);
    assert(materials[1].transition.appearing);

    assert(materials[2].transition.kind == MaterialGlassTransitionKind::Identity);
    assert(materials[2].transition.progress == 1.0f);
    assert(materials[2].transition.appearing);

    std::puts("PASS: glass effect transition scope emits command context");
}

void test_glass_effect_container_default_transition_respects_explicit_scope() {
    auto identity = layout::glass_identity_transition();
    assert(identity.kind == MaterialGlassTransitionKind::Identity);
    assert(identity.progress == 1.0f);
    assert(identity.appearing);

    auto materialize_default = layout::glass_materialize_transition();
    assert(materialize_default.kind == MaterialGlassTransitionKind::Materialize);
    assert(materialize_default.progress == 1.0f);
    assert(materialize_default.appearing);

    auto matched_default = layout::glass_matched_geometry_transition();
    assert(matched_default.kind == MaterialGlassTransitionKind::MatchedGeometry);
    assert(matched_default.progress == 1.0f);
    assert(matched_default.appearing);

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_effect_transition(
        layout::glass_materialize_transition(0.25f, false),
        [] {
            layout::glass_effect_container(
                layout::GlassEffectContainerOptions{
                    .container_id = 612u,
                    .spacing = 8.0f,
                    .morph_transitions = true,
                },
                [] {
                    auto glass = layout::GlassEffectOptions{};
                    glass.role = MaterialSurfaceRole::Control;
                    glass.fixed_height = 36.0f;
                    layout::glass_effect(glass, [] {
                        widget::text("Explicit transition");
                    });
                });
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const& material = first_material_command(cmds);
    assert(material.material.container.container_id == 612u);
    assert(material.material.transition.kind
           == MaterialGlassTransitionKind::Materialize);
    assert(std::fabs(material.material.transition.progress - 0.25f)
           < 0.0001f);
    assert(!material.material.transition.appearing);

    std::puts("PASS: glass effect container default transition respects explicit scope");
}

void test_glass_effect_id_scope_emits_command_context() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_effect_id(12u, 34u, [] {
        layout::material_surface(
            layout::MaterialSurfaceOptions{
                .kind = MaterialKind::Regular,
                .fixed_height = 36.0f,
                .border_width = 0.0f,
                .semantic_label = "Inherited Glass ID",
            },
            [] {
                widget::text("Inherited glass id");
            });
        layout::material_surface(
            layout::MaterialSurfaceOptions{
                .kind = MaterialKind::Thin,
                .glass_identity = layout::glass_effect_identity(12u, 99u),
                .fixed_height = 36.0f,
                .border_width = 0.0f,
                .semantic_label = "Overridden Glass ID",
            },
            [] {
                widget::text("Overridden glass id");
            });
        layout::material_surface(
            layout::MaterialSurfaceOptions{
                .kind = MaterialKind::Clear,
                .inherit_material_identity = false,
                .fixed_height = 36.0f,
                .border_width = 0.0f,
                .semantic_label = "No Glass ID",
            },
            [] {
                widget::text("No glass id");
            });
    });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    std::vector<MaterialCommandDescriptor> materials;
    for (auto const& cmd : cmds) {
        if (auto const* m = std::get_if<MaterialRectCmd>(&cmd))
            materials.push_back(m->material);
    }

    assert(materials.size() == 3u);
    assert(materials[0].glass_identity.namespace_id == 12u);
    assert(materials[0].glass_identity.effect_id == 34u);
    assert(materials[0].glass_identity.participates());

    assert(materials[1].glass_identity.namespace_id == 12u);
    assert(materials[1].glass_identity.effect_id == 99u);
    assert(materials[1].glass_identity.participates());

    assert(materials[2].glass_identity.namespace_id == 0u);
    assert(materials[2].glass_identity.effect_id == 0u);
    assert(!materials[2].glass_identity.participates());

    std::puts("PASS: glass effect id scope emits command context");
}

void test_glass_effect_string_id_scope_emits_stable_context() {
    auto const expected = layout::glass_effect_identity("showcase", "pencil");
    assert(expected.namespace_id != 0u);
    assert(expected.effect_id != 0u);
    assert(expected == layout::glass_effect_identity("showcase", "pencil"));
    assert(expected != layout::glass_effect_identity("showcase", "note"));
    assert(!layout::glass_effect_identity("", "pencil").participates());
    assert(!layout::glass_effect_identity("showcase", "").participates());

    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::glass_effect_container(
        layout::GlassEffectContainerOptions{
            .container_id = 901u,
            .spacing = 128.0f,
            .morph_transitions = true,
        },
        [&] {
            layout::glass_effect_id("showcase", "pencil", [&] {
                auto first = layout::GlassEffectOptions{};
                first.role = MaterialSurfaceRole::Control;
                first.fixed_height = 36.0f;
                first.semantic_label = "String Glass ID A";
                layout::glass_effect(first, [] {
                    widget::text("A");
                });
            });
            auto second = layout::GlassEffectOptions{};
            second.role = MaterialSurfaceRole::Control;
            second.glass_identity =
                layout::glass_effect_identity("showcase", "pencil");
            second.fixed_height = 36.0f;
            second.semantic_label = "String Glass ID B";
            layout::glass_effect(second, [] {
                widget::text("B");
            });
        });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    std::vector<MaterialRectCmd> materials;
    for (auto const& cmd : cmds) {
        if (auto const* material = std::get_if<MaterialRectCmd>(&cmd))
            materials.push_back(*material);
    }

    assert(materials.size() == 2u);
    for (auto const& material : materials) {
        assert(material.material.glass_identity == expected);
        assert(material.material.glass_identity.participates());
        assert(material.material.container.container_id == 901u);
        assert(material.material.transition.kind
               == MaterialGlassTransitionKind::MatchedGeometry);
        assert(material.material.transition.progress == 1.0f);
    }

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 80;

    std::vector<MaterialRuntimeRecord> records;
    auto command_index = std::uint32_t{1u};
    for (auto const& material : materials) {
        auto plan = make_test_material_plan(
            material_request_for_command(
                material.material,
                MaterialGeometry{
                    material.x,
                    material.y,
                    material.w,
                    material.h,
                    material.radius},
                detail::g_app().theme),
            env);
        records.push_back(MaterialRuntimeRecord{*plan, command_index++});
    }

    auto first_execution =
        material_container_execution_descriptor(records[0], records);
    assert(first_execution.glass_effect_match_execution);
    assert(first_execution.glass_namespace_id == expected.namespace_id);
    assert(first_execution.glass_effect_id == expected.effect_id);
    assert(first_execution.glass_effect_surface_count == 2u);
    assert(first_execution.glass_effect_match_progress == 1.0f);
    assert(first_execution.glass_effect_match_source_valid);
    assert(first_execution.glass_effect_match_source_y > 0.0f);
    assert(first_execution.glass_effect_match_source_radius
           == records[1].plan.geometry.radius);
    assert(first_execution.glass_effect_match_rect_y
           == records[0].plan.geometry.y);
    assert(first_execution.glass_effect_match_rect_radius
           == records[0].plan.geometry.radius);
    assert(std::string_view{first_execution.execution_policy}
           == "glass-effect-matched-geometry");
    assert(std::fabs(first_execution.glass_effect_match_blend_strength - 1.0f)
           < 0.0001f);

    std::puts("PASS: glass effect string id scope emits stable context");
}

void test_material_command_preserves_style_optics() {
    detail::g_app().arena.reset();
    detail::g_app().prev_arena.reset();
    detail::g_app().callbacks.clear();
    CMD_LEN = 0;

    auto root_h = detail::alloc_node();
    auto material_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;
    root.children.push_back(material_h);

    auto& material = detail::node_at(material_h);
    material.material = layout::material_style(MaterialKind::Regular);
    material.material.role = MaterialSurfaceRole::Content;
    material.material.container = MaterialContainerDescriptor{
        88u,
        12u,
        18.0f,
        true,
        true};
    material.material.glass_identity = MaterialGlassIdentityDescriptor{
        .namespace_id = 44u,
        .effect_id = 144u,
    };
    material.material.opacity = 0.63f;
    material.material.blur_radius = 18.0f;
    material.material.tint = Color{32, 64, 96, 144};
    material.material.saturation = 0.73f;
    material.material.luminance_floor = 0.19f;
    material.material.luminance_gain = 1.31f;
    material.material.edge_highlight = 0.57f;
    material.material.edge_width = 2.25f;
    material.material.noise_opacity = 0.031f;
    material.material.shadow_alpha = 0.22f;
    material.material.shadow_radius = 17.0f;
    material.material.foreground = Color{230, 240, 250, 255};
    material.material.secondary_foreground = Color{180, 190, 205, 255};
    material.material.accent_foreground = Color{80, 160, 255, 255};
    material.material.strong_accent_foreground = Color{45, 120, 220, 255};
    material.material.prominence = MaterialProminenceDescriptor{
        .enabled = true,
        .intensity = 0.82f,
    };
    material.background = material.material.tint;
    material.border_color = material.material.border;
    material.border_width = 1.0f;
    material.border_radius = 6.0f;
    material.style.fixed_height = 40.0f;

    LAYOUT_NODE(root_h, 320.0f);
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    auto const* cmd = static_cast<MaterialRectCmd const*>(nullptr);
    for (auto const& parsed : cmds) {
        if (auto const* material_cmd = std::get_if<MaterialRectCmd>(&parsed)) {
            cmd = material_cmd;
            break;
        }
    }

    assert(cmd != nullptr);
    auto const& descriptor = cmd->material;
    assert(descriptor.kind == MaterialKind::Regular);
    assert(descriptor.role == MaterialSurfaceRole::Content);
    assert(descriptor.container.container_id == 88u);
    assert(descriptor.container.union_id == 12u);
    assert(std::fabs(descriptor.container.spacing - 18.0f) < 0.0001f);
    assert(descriptor.container.interactive);
    assert(descriptor.container.morph_transitions);
    assert(descriptor.glass_identity.namespace_id == 44u);
    assert(descriptor.glass_identity.effect_id == 144u);
    assert(descriptor.glass_identity.participates());
    assert(std::fabs(descriptor.opacity - 0.63f) < 0.0001f);
    assert(std::fabs(descriptor.blur_radius - 18.0f) < 0.0001f);
    assert(descriptor.tint.r == 32 && descriptor.tint.g == 64
           && descriptor.tint.b == 96);
    assert(descriptor.tint.a == 144);
    assert(std::fabs(descriptor.saturation - 0.73f) < 0.0001f);
    assert(std::fabs(descriptor.luminance_floor - 0.19f) < 0.0001f);
    assert(std::fabs(descriptor.luminance_gain - 1.31f) < 0.0001f);
    assert(std::fabs(descriptor.edge_highlight - 0.57f) < 0.0001f);
    assert(std::fabs(descriptor.edge_width - 2.25f) < 0.0001f);
    assert(std::fabs(descriptor.noise_opacity - 0.031f) < 0.0001f);
    assert(std::fabs(descriptor.shadow_alpha - 0.22f) < 0.0001f);
    assert(std::fabs(descriptor.shadow_radius - 17.0f) < 0.0001f);
    assert(descriptor.prominence.enabled);
    assert(std::fabs(descriptor.prominence.intensity - 0.82f) < 0.0001f);
    assert(descriptor.has_foreground_palette);
    assert((descriptor.foreground == Color{230, 240, 250, 255}));
    assert((descriptor.secondary_foreground
            == Color{180, 190, 205, 255}));
    assert((descriptor.accent_foreground == Color{80, 160, 255, 255}));
    assert((descriptor.strong_accent_foreground
            == Color{45, 120, 220, 255}));

    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = true;
    env.capabilities.shader_blur = true;
    env.capabilities.frame_history = true;
    env.backdrop.available = true;
    env.backdrop.stable = true;
    env.render_target.width = 320;
    env.render_target.height = 40;
    auto plan_owner = make_test_material_plan(
        material_request_for_command(
            descriptor,
            MaterialGeometry{cmd->x, cmd->y, cmd->w, cmd->h, cmd->radius},
            detail::g_app().theme),
        env);
    auto& plan = *plan_owner;
    assert(!plan.fallback());
    assert(plan.role == MaterialSurfaceRole::Content);
    assert(plan.command_descriptor.role == MaterialSurfaceRole::Content);
    assert(!plan.decision_trace.role_allows_liquid_glass);
    assert(plan.decision_trace.content_layer_standard_material);
    assert(!plan.decision_trace.liquid_glass_backdrop_candidate);
    assert(!plan.decision_trace.can_sample_backdrop);
    assert(!plan.decision_trace.next_frame_capture_required);
    assert(!plan.backdrop_sampling);
    assert(!plan.backdrop_access.shared_frame_capture);
    assert(std::string(plan.plan_id)
           == "material.regular.standard-material");
    assert(std::string(plan.reference_model.technology)
           == "standard-material");
    assert(std::string(plan.reference_model.layer) == "content-layer");
    assert(std::string(plan.reference_model.material_policy)
           == "standard-material-content-layer");
    assert(std::string(plan.reference_model.blending_scope)
           == "standard-fill");
    assert(std::string(plan.primary_pass.name)
           == "standard-material-fill");
    assert(std::string(plan.primary_pass.executor) == "standard-fill");
    assert(std::string(plan.primary_pass.likely_layer)
           == "material-standard-pass");
    assert(plan.command_descriptor.container.container_id == 0u);
    assert(plan.command_descriptor.container.union_id == 0u);
    assert(!plan.command_descriptor.container.interactive);
    assert(plan.container.mode == MaterialContainerMode::Isolated);
    assert(!plan.container.participates);
    assert(!plan.container.interactive);
    assert(!plan.container.morph_transitions);
    assert(plan.glass_identity.namespace_id == 44u);
    assert(plan.glass_identity.effect_id == 144u);
    assert(plan.glass_identity.participates);
    assert(!plan.glass_identity.container_scoped);
    assert(plan.blur_radius == 0.0f);
    assert(plan.sample_taps == 0u);
    assert(plan.saturation == 1.0f);
    assert(std::fabs(plan.luminance_floor - 0.19f) < 0.0001f);
    assert(std::fabs(plan.luminance_gain - 1.31f) < 0.0001f);
    assert(std::fabs(plan.edge_highlight - 0.57f) < 0.0001f);
    assert(std::fabs(plan.edge_width - 2.25f) < 0.0001f);
    assert(plan.noise_opacity == 0.0f);
    assert(std::fabs(plan.shadow_alpha - 0.22f) < 0.0001f);
    assert(std::fabs(plan.shadow_radius - 17.0f) < 0.0001f);
    assert(plan.command_descriptor.has_foreground_palette);
    assert((plan.theme.foreground == Color{230, 240, 250, 255}));

    std::puts("PASS: material command preserves style optics");
}

int main() {
    test_material_props_invalidate_diff_cache();
    test_material_planner_backdrop_and_fallback_paths();
    test_material_text_foreground_resolution();
    test_material_surface_emits_material_rect_command();
    test_material_surface_per_edge_padding_overrides();
    test_material_surface_fixed_outer_height_accounts_for_padding();
    test_material_surface_shape_overrides();
    test_material_surface_glass_effect_shape_options();
    test_material_surface_style_override_emits_explicit_material_contract();
    test_material_surface_resolves_live_input_interaction();
    test_capture_only_surface_ignores_live_hover_material_interaction();
    test_material_surface_interactive_option_enables_plan_response();
    test_glass_effect_surface_api_emits_capsule_tint_contract();
    test_glass_effect_shape_argument_anchors_material_contract();
    test_glass_background_effect_display_modes_follow_containment_contract();
    test_glass_background_effect_variants_map_to_material_contract();
    test_glass_effect_style_variants_map_to_material_contract();
    test_glass_effect_style_carries_transition_and_identity();
    test_glass_effect_style_carries_union_context();
    test_glass_surface_presets_emit_material_contract();
    test_plain_material_style_disables_liquid_glass_for_chrome_roles();
    test_interaction_glass_button_uses_plain_idle_material();
    test_material_container_scope_emits_command_context();
    test_glass_effect_container_scope_emits_morph_context();
    test_glass_effect_union_scope_emits_view_union_context();
    test_glass_effect_transition_scope_emits_command_context();
    test_glass_effect_container_default_transition_respects_explicit_scope();
    test_glass_effect_id_scope_emits_command_context();
    test_glass_effect_string_id_scope_emits_stable_context();
    test_material_command_preserves_style_optics();
    std::puts("\nAll phenotype material surface tests passed.");
    return 0;
}

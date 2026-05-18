#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>

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
    assert(material_plan_contract_version == 23);
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
    std::puts("PASS: sampled backdrop access contract");
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

} // namespace

int main() {
    test_sampled_backdrop_access_contract();
    test_content_layer_stays_standard_material_contract();
    test_fallback_backdrop_access_contract();
    test_custom_theme_snapshot_contract();
    test_command_material_preserves_theme_snapshot_contract();
    test_warmup_backdrop_access_contract();
    test_surface_sample_pixels_are_scaled_and_bounded();
    test_executor_frame_capture_policy_contract();
    std::puts("\nAll material tests passed.");
    return 0;
}

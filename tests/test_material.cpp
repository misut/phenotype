#include <cassert>
#include <cstdio>
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

void test_sampled_backdrop_access_contract() {
    auto plan = plan_material_surface(regular_request(), sampled_environment());

    assert(plan.contract_version == material_plan_contract_version);
    assert(material_plan_contract_version == 18);
    assert(plan.backdrop_sampling);
    assert(!plan.fallback());
    assert(plan.backdrop_access.required);
    assert(plan.backdrop_access.stable_required);
    assert(plan.backdrop_access.frame_history_required);
    assert(plan.backdrop_access.shared_frame_capture);
    assert(plan.backdrop_access.next_frame_capture_required);
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
    assert(std::string_view(plan.observation_contract.backdrop_capture_scope)
        == "shared-frame");
    assert(std::string_view(plan.observation_contract.backdrop_capture_reason)
        == "sample-current-frame");
    assert(plan.observation_contract.max_frame_capture_count == 1);
    assert(plan.observation_contract.max_frame_capture_pixels == 320 * 240);
    assert(plan.observation_contract.max_surface_sample_pixels == 240 * 96);
    std::puts("PASS: sampled backdrop access contract");
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
    assert(std::string_view(plan.observation_contract.backdrop_capture_scope)
        == "none");
    assert(std::string_view(plan.observation_contract.backdrop_capture_reason)
        == "not-required");
    std::puts("PASS: fallback backdrop access contract");
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

} // namespace

int main() {
    test_sampled_backdrop_access_contract();
    test_fallback_backdrop_access_contract();
    test_warmup_backdrop_access_contract();
    test_surface_sample_pixels_are_scaled_and_bounded();
    std::puts("\nAll material tests passed.");
    return 0;
}

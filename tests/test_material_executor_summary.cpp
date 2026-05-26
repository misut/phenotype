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

} // namespace

int main() {
    test_surface_sample_pixels_are_scaled_and_bounded();
    test_executor_frame_capture_policy_contract();
    test_executor_sampled_status_contract();
    std::puts("\nAll material executor summary tests passed.");
    return 0;
}

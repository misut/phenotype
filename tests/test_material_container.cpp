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
    assert(first_execution.cohesion_active);
    assert(first_execution.cohesion_spacing_driven);
    assert(first_execution.cohesion_morph_driven);
    assert(!first_execution.cohesion_union_driven);
    assert(!first_execution.cohesion_overlap_driven);
    assert(std::string_view(first_execution.cohesion_model)
           == "proximity-liquid-glass-cohesion");
    assert(first_execution.cohesion_strength > 0.35f);
    assert(first_execution.cohesion_strength < 0.42f);
    assert(first_execution.cohesion_pressure > 0.50f);
    assert(first_execution.cohesion_pressure < 0.58f);
    assert(std::fabs(first_execution.cohesion_falloff - 0.5f)
           < 0.0001f);
    assert(first_execution.cohesion_stabilization > 0.30f);
    assert(first_execution.cohesion_stabilization < 0.36f);
    assert(!distant_execution.fusion_optics_active);
    assert(std::string_view(distant_execution.fusion_model) == "none");
    assert(!distant_execution.cohesion_active);
    assert(std::string_view(distant_execution.cohesion_model) == "none");

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
    assert(first_execution.union_response_active);
    assert(peer_execution.union_response_active);
    assert(first_execution.union_response_spacing_driven);
    assert(first_execution.union_response_member_driven);
    assert(!first_execution.union_response_overlap_driven);
    assert(std::string_view(first_execution.union_response_model)
           == "glass-effect-union-response");
    assert(first_execution.union_response_strength > 0.78f);
    assert(first_execution.union_response_strength < 0.84f);
    assert(first_execution.union_edge_continuity > 0.76f);
    assert(first_execution.union_edge_continuity < 0.82f);
    assert(first_execution.union_shape_coalescence > 0.78f);
    assert(first_execution.union_shape_coalescence < 0.86f);
    assert(first_execution.union_luma_stability > 0.68f);
    assert(first_execution.union_luma_stability < 0.76f);

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
    assert(!different_union_execution.union_response_active);

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
    std::puts("\nAll material container tests passed.");
    return 0;
}

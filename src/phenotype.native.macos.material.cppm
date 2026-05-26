module;
#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#endif

export module phenotype.native.macos.material;

#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
import phenotype.material;
import phenotype.native.macos.render;

export namespace phenotype::native::detail {

inline void append_material_instance(std::vector<MaterialInstanceGPU>& out,
                                     MaterialPlan const& plan,
                                     std::uint32_t command_index) {
    MaterialInstanceGPU inst{};
    auto const background_inflate = material_background_paint_inflate(plan);
    auto const background_soft_edge = plan.glass_background.feathered
        ? std::max(0.0f, plan.glass_background.soft_edge_radius)
        : 0.0f;
    auto const geometry = material_surface_execution_geometry(plan);
    if (!geometry.active)
        return;
    inst.rect[0] = geometry.x;
    inst.rect[1] = geometry.y;
    inst.rect[2] = geometry.w;
    inst.rect[3] = geometry.h;
    inst.tint[0] = plan.tint.r / 255.0f;
    inst.tint[1] = plan.tint.g / 255.0f;
    inst.tint[2] = plan.tint.b / 255.0f;
    inst.tint[3] = plan.tint.a / 255.0f;
    inst.params[0] = geometry.radius;
    inst.params[1] = plan.blur_radius;
    inst.params[2] = plan.opacity;
    inst.params[3] = static_cast<float>(plan.sample_taps);
    inst.optics[0] = plan.saturation;
    inst.optics[1] = plan.luminance_floor;
    inst.optics[2] = plan.luminance_gain;
    inst.optics[3] = plan.edge_highlight;
    inst.effects[0] = plan.edge_width;
    inst.effects[1] = plan.shadow_alpha;
    inst.effects[2] = plan.shadow_radius;
    inst.effects[3] = plan.noise_opacity;
    inst.sampling[0] = plan.sampling_kernel.blur_step_scale;
    inst.sampling[1] = static_cast<float>(plan.sampling_kernel.radius);
    inst.sampling[2] = background_soft_edge;
    inst.sampling[3] = background_inflate;
    inst.luminance_curve[0] = plan.luminance_curve.gamma;
    inst.luminance_curve[1] = plan.luminance_curve.midpoint;
    inst.luminance_curve[2] = plan.luminance_curve.contrast;
    inst.luminance_curve[3] = plan.luminance_curve.edge_lift;
    inst.interaction[0] = material_surface_execution_anchor_x(
        plan,
        geometry,
        plan.specular.anchor_x);
    inst.interaction[1] = material_surface_execution_anchor_y(
        plan,
        geometry,
        plan.specular.anchor_y);
    inst.interaction[2] = plan.specular.radius;
    inst.interaction[3] = plan.specular.intensity;
    inst.interaction_lens[0] = material_surface_execution_anchor_x(
        plan,
        geometry,
        plan.interaction.pointer_lens_anchor_x);
    inst.interaction_lens[1] = material_surface_execution_anchor_y(
        plan,
        geometry,
        plan.interaction.pointer_lens_anchor_y);
    inst.interaction_lens[2] = plan.interaction.pointer_lens_radius;
    inst.interaction_lens[3] = plan.interaction.pointer_lens_strength;
    inst.control_morph[0] = plan.interaction.control_morph_scale_delta;
    inst.control_morph[1] = plan.interaction.control_morph_depth;
    inst.control_morph[2] = plan.interaction.control_morph_edge;
    inst.control_morph[3] = plan.interaction.control_morph_shadow;
    inst.transition_optics[0] = plan.transition.materialize_wave_strength;
    inst.transition_optics[1] = plan.transition.materialize_edge_lift;
    inst.transition_optics[2] = plan.transition.materialize_lensing_gain;
    inst.transition_optics[3] = plan.transition.materialize_rim_position;
    inst.refraction[0] = plan.refraction.strength;
    inst.refraction[1] = plan.refraction.edge_bias;
    inst.refraction[2] = plan.refraction.max_offset_pixels;
    inst.refraction[3] = plan.refraction.edge_caustic_intensity;
    inst.edge_optics[0] = plan.edge_optics.bevel_width;
    inst.edge_optics[1] = plan.edge_optics.inner_highlight;
    inst.edge_optics[2] = plan.edge_optics.outer_shadow;
    inst.edge_optics[3] = plan.edge_optics.chromatic_fringe;
    inst.spectral_tint[0] = plan.spectral_tint.warmth;
    inst.spectral_tint[1] = plan.spectral_tint.coolness;
    inst.spectral_tint[2] = plan.spectral_tint.dispersion;
    inst.spectral_tint[3] = plan.spectral_tint.rim_tint;
    inst.lighting[0] = plan.dynamic_lighting.direction_x;
    inst.lighting[1] = plan.dynamic_lighting.direction_y;
    inst.lighting[2] = plan.dynamic_lighting.highlight_strength;
    inst.lighting[3] = plan.dynamic_lighting.shadow_strength;
    inst.thickness[0] = plan.glass_thickness.thickness;
    inst.thickness[1] = plan.glass_thickness.lensing_gain;
    inst.thickness[2] = plan.glass_thickness.shadow_gain;
    inst.thickness[3] = plan.glass_thickness.scattering_gain;
    inst.dispersion[0] = plan.glass_dispersion.axial_offset_pixels;
    inst.dispersion[1] = plan.glass_dispersion.tangential_offset_pixels;
    inst.dispersion[2] = plan.glass_dispersion.prismatic_gain;
    inst.dispersion[3] = plan.glass_dispersion.caustic_spread;
    inst.scroll_edge[0] = plan.scroll_edge.fade_extent_pixels;
    inst.scroll_edge[1] = plan.scroll_edge.dissolve_strength;
    inst.scroll_edge[2] = plan.scroll_edge.dimming_strength;
    inst.scroll_edge[3] = plan.scroll_edge.hard_style_strength;
    inst.prominent_glass[0] = plan.prominent_glass.intensity;
    inst.prominent_glass[1] = plan.prominent_glass.tint_weight;
    inst.prominent_glass[2] = plan.prominent_glass.edge_lift;
    inst.prominent_glass[3] = plan.prominent_glass.lensing_gain;
    inst.clear_glass[0] = plan.clear_glass_legibility.dimming_strength;
    inst.clear_glass[1] = plan.clear_glass_legibility.contrast_lift;
    inst.clear_glass[2] = plan.clear_glass_legibility.brightness_response;
    inst.clear_glass[3] = plan.clear_glass_legibility.detail_response;
    inst.glass_stability[0] = plan.glass_stabilization.strength;
    inst.glass_stability[1] = plan.glass_stabilization.damping;
    inst.glass_stability[2] = plan.glass_stabilization.shimmer_reduction;
    inst.glass_stability[3] = plan.glass_stabilization.transmission_bias;
    inst.glass_environment[0] = plan.glass_environment.reflection_strength;
    inst.glass_environment[1] = plan.glass_environment.color_pickup;
    inst.glass_environment[2] = plan.glass_environment.luminance_balance;
    inst.glass_environment[3] = plan.glass_environment.transmission_balance;
    inst.glass_union[0] = plan.glass_surface_cohesion.surface_response;
    inst.glass_union[1] = plan.glass_surface_cohesion.edge_adhesion;
    inst.glass_union[2] = plan.glass_surface_cohesion.shape_coalescence;
    inst.glass_union[3] = plan.glass_surface_cohesion.luma_stability;
    inst.clear_glass[0] = std::clamp(
        inst.clear_glass[0]
            + 0.050f * plan.glass_substrate_adhesion.settle_depth
            + 0.035f * plan.glass_substrate_adhesion.contact_shadow,
        0.0f,
        0.36f);
    inst.clear_glass[2] = std::clamp(
        inst.clear_glass[2]
            + 0.30f * plan.glass_substrate_adhesion.contact_strength,
        0.0f,
        1.0f);
    inst.thickness[2] = std::clamp(
        inst.thickness[2]
            + 0.16f * plan.glass_substrate_adhesion.contact_shadow
            + 0.08f * plan.glass_substrate_adhesion.settle_depth,
        1.0f,
        1.62f);
    inst.thickness[3] = std::clamp(
        inst.thickness[3]
            + 0.10f * plan.glass_substrate_adhesion.contact_strength,
        1.0f,
        1.45f);
    inst.refraction[1] = std::clamp(
        inst.refraction[1]
            + 0.12f * plan.glass_substrate_adhesion.refraction_crawl,
        0.0f,
        0.90f);
    inst.refraction[2] = std::clamp(
        inst.refraction[2]
            * (1.0f
               + 0.16f * plan.glass_substrate_adhesion.refraction_crawl),
        0.0f,
        8.0f);
    inst.glass_union[0] = std::clamp(
        inst.glass_union[0]
            + 0.24f * plan.glass_substrate_adhesion.contact_strength,
        0.0f,
        1.0f);
    inst.glass_union[1] = std::clamp(
        inst.glass_union[1]
            + 0.20f * plan.glass_substrate_adhesion.settle_depth,
        0.0f,
        1.0f);
    inst.glass_union[3] = std::clamp(
        inst.glass_union[3]
            + 0.16f * plan.glass_substrate_adhesion.contact_shadow,
        0.0f,
        1.0f);
    inst.edge_optics[0] = std::clamp(
        inst.edge_optics[0]
            + 2.40f * plan.glass_substrate_adhesion.contact_strength
            + 1.20f * plan.glass_substrate_adhesion.settle_depth,
        0.0f,
        18.0f);
    inst.edge_optics[1] = std::clamp(
        inst.edge_optics[1]
            + 0.055f * plan.glass_substrate_adhesion.contact_strength
            + 0.040f * plan.glass_substrate_adhesion.refraction_crawl,
        0.0f,
        0.95f);
    inst.edge_optics[2] = std::clamp(
        inst.edge_optics[2]
            + 0.070f * plan.glass_substrate_adhesion.contact_shadow
            + 0.035f * plan.glass_substrate_adhesion.settle_depth,
        0.0f,
        0.84f);
    inst.edge_optics[3] = std::clamp(
        inst.edge_optics[3]
            + 0.018f * plan.glass_substrate_adhesion.refraction_crawl
            + 0.012f * plan.glass_surface_cohesion.edge_adhesion,
        0.0f,
        0.20f);
    inst.clear_glass[3] = std::clamp(
        inst.clear_glass[3]
            + 0.24f * plan.glass_substrate_adhesion.contact_strength
            + 0.12f * plan.glass_surface_cohesion.luma_stability,
        0.0f,
        1.0f);
    inst.glass_environment[0] = std::clamp(
        inst.glass_environment[0]
            + 0.16f * plan.glass_substrate_adhesion.contact_strength
            + 0.10f * plan.glass_surface_cohesion.surface_response,
        0.0f,
        1.0f);
    inst.glass_environment[0] = std::clamp(
        inst.glass_environment[0]
            + 0.36f * plan.glass_ambient_reflection.reflection_gain
            + 0.08f * plan.glass_ambient_reflection.sheen_coherence,
        0.0f,
        1.0f);
    inst.glass_environment[1] = std::clamp(
        inst.glass_environment[1]
            + 0.12f * plan.glass_substrate_adhesion.contact_strength
            + 0.10f * plan.glass_surface_cohesion.surface_response
            + 0.08f * plan.glass_transmission.subsurface_scatter,
        0.0f,
        1.0f);
    inst.glass_environment[2] = std::clamp(
        inst.glass_environment[2]
            + 0.060f * plan.glass_surface_cohesion.luma_stability
            + 0.040f * plan.glass_substrate_adhesion.contact_shadow
            - 0.040f * plan.glass_transmission.volume_absorption,
        0.0f,
        1.0f);
    inst.glass_environment[1] = std::clamp(
        inst.glass_environment[1]
            + 0.28f * plan.glass_ambient_reflection.color_bleed,
        0.0f,
        1.0f);
    inst.glass_environment[2] = std::clamp(
        0.5f
            + (inst.glass_environment[2] - 0.5f)
                * (1.0f
                   + 0.24f
                       * plan.glass_ambient_reflection.luma_polarization)
            + 0.05f * plan.glass_ambient_reflection.sheen_coherence,
        0.0f,
        1.0f);
    inst.glass_environment[3] = std::clamp(
        inst.glass_environment[3]
            + 0.24f * plan.glass_ambient_reflection.reflection_gain
            - 0.06f * plan.glass_ambient_reflection.luma_polarization,
        0.0f,
        1.0f);
    inst.clear_glass[3] = std::clamp(
        inst.clear_glass[3]
            + 0.18f * plan.glass_ambient_reflection.sheen_coherence,
        0.0f,
        1.0f);
    inst.lighting[2] = std::clamp(
        inst.lighting[2]
            + 0.040f * plan.glass_substrate_adhesion.contact_strength
            + 0.030f * plan.glass_substrate_adhesion.refraction_crawl,
        0.0f,
        0.50f);
    inst.lighting[2] = std::clamp(
        inst.lighting[2]
            + 0.060f * plan.glass_ambient_reflection.reflection_gain
            + 0.040f * plan.glass_ambient_reflection.sheen_coherence,
        0.0f,
        0.54f);
    inst.edge_optics[1] = std::clamp(
        inst.edge_optics[1]
            + 0.040f * plan.glass_ambient_reflection.reflection_gain,
        0.0f,
        0.95f);
    inst.spectral_tint[3] = std::clamp(
        inst.spectral_tint[3]
            + 0.025f * plan.glass_ambient_reflection.color_bleed,
        0.0f,
        0.34f);
    inst.thickness[1] = std::clamp(
        inst.thickness[1]
            + 0.080f * plan.glass_depth.parallax_gain
            + 0.040f * plan.glass_depth.surface_lift,
        1.0f,
        1.62f);
    inst.thickness[2] = std::clamp(
        inst.thickness[2]
            + 0.060f * plan.glass_depth.depth_separation
            + 0.050f * plan.glass_depth.inner_shadow,
        1.0f,
        1.62f);
    inst.refraction[2] = std::clamp(
        inst.refraction[2] * (1.0f + 0.10f * plan.glass_depth.parallax_gain),
        0.0f,
        8.0f);
    inst.edge_optics[0] = std::clamp(
        inst.edge_optics[0] + plan.glass_depth.surface_lift * 1.8f,
        0.0f,
        18.0f);
    inst.edge_optics[1] = std::clamp(
        inst.edge_optics[1] + 0.050f * plan.glass_depth.surface_lift,
        0.0f,
        0.90f);
    inst.edge_optics[2] = std::clamp(
        inst.edge_optics[2] + 0.075f * plan.glass_depth.inner_shadow,
        0.0f,
        0.80f);
    inst.lighting[2] = std::clamp(
        inst.lighting[2]
            + 0.075f * plan.glass_caustic_flow.flow_strength
            + 0.060f * plan.glass_caustic_flow.highlight_drift
            + 0.040f * plan.glass_caustic_flow.caustic_focus,
        0.0f,
        0.50f);
    inst.lighting[3] = std::clamp(
        inst.lighting[3]
            + 0.045f * plan.glass_caustic_flow.flow_strength
            + 0.035f * plan.glass_caustic_flow.caustic_focus,
        0.0f,
        0.40f);
    inst.refraction[2] = std::clamp(
        inst.refraction[2]
            * (1.0f + 0.080f * plan.glass_caustic_flow.flow_strength),
        0.0f,
        8.0f);
    inst.refraction[3] = std::clamp(
        inst.refraction[3]
            + 0.075f * plan.glass_caustic_flow.caustic_focus
            + 0.045f * plan.glass_caustic_flow.flow_strength,
        0.0f,
        0.40f);
    inst.edge_optics[3] = std::clamp(
        inst.edge_optics[3]
            + 0.035f * plan.glass_caustic_flow.chroma_shear,
        0.0f,
        0.18f);
    inst.spectral_tint[2] = std::clamp(
        inst.spectral_tint[2]
            + 0.045f * plan.glass_caustic_flow.chroma_shear,
        0.0f,
        0.24f);
    inst.spectral_tint[3] = std::clamp(
        inst.spectral_tint[3]
            + 0.050f * plan.glass_caustic_flow.caustic_focus,
        0.0f,
        0.30f);
    inst.dispersion[0] = std::clamp(
        inst.dispersion[0]
            + 0.18f * plan.glass_caustic_flow.flow_strength,
        0.0f,
        3.40f);
    inst.dispersion[1] = std::clamp(
        inst.dispersion[1]
            + 0.14f * plan.glass_caustic_flow.chroma_shear,
        0.0f,
        2.60f);
    inst.dispersion[2] = std::clamp(
        inst.dispersion[2]
            + 0.060f * plan.glass_caustic_flow.caustic_focus,
        1.0f,
        1.80f);
    inst.dispersion[3] = std::clamp(
        inst.dispersion[3]
            + 0.060f * plan.glass_caustic_flow.caustic_focus
            + 0.040f * plan.glass_caustic_flow.flow_strength,
        0.0f,
        0.44f);
    inst.refraction[0] = std::clamp(
        inst.refraction[0] * plan.glass_meniscus.refraction_gain,
        0.0f,
        0.82f);
    inst.refraction[1] = std::clamp(
        inst.refraction[1] + 0.10f * plan.glass_meniscus.edge_pull,
        0.0f,
        0.88f);
    inst.refraction[2] = std::clamp(
        inst.refraction[2]
            * (1.0f
               + 0.045f * (plan.glass_meniscus.refraction_gain - 1.0f)
               + 0.060f * plan.glass_meniscus.edge_pull),
        0.0f,
        8.0f);
    inst.edge_optics[0] = std::clamp(
        inst.edge_optics[0] + plan.glass_meniscus.rim_width * 1.10f,
        0.0f,
        18.0f);
    inst.edge_optics[1] = std::clamp(
        inst.edge_optics[1] * plan.glass_meniscus.highlight_gain
            + 0.045f * plan.glass_meniscus.edge_pull,
        0.0f,
        0.95f);
    inst.edge_optics[2] = std::clamp(
        inst.edge_optics[2] + 0.055f * plan.glass_meniscus.edge_pull,
        0.0f,
        0.82f);
    inst.edge_optics[3] = std::clamp(
        inst.edge_optics[3] + 0.020f * plan.glass_meniscus.edge_pull,
        0.0f,
        0.20f);
    inst.spectral_tint[3] = std::clamp(
        inst.spectral_tint[3]
            + 0.034f * (plan.glass_meniscus.highlight_gain - 1.0f),
        0.0f,
        0.32f);
    inst.lighting[2] = std::clamp(
        inst.lighting[2]
            + 0.030f * (plan.glass_meniscus.highlight_gain - 1.0f)
            + 0.020f * plan.glass_meniscus.edge_pull,
        0.0f,
        0.54f);
    inst.thickness[0] = std::clamp(
        inst.thickness[0]
            + 0.055f * plan.glass_transmission.internal_transmission,
        0.0f,
        0.86f);
    inst.thickness[3] = std::clamp(
        inst.thickness[3]
            + 0.18f * plan.glass_transmission.subsurface_scatter
            + 0.08f * plan.glass_transmission.internal_transmission,
        1.0f,
        1.45f);
    inst.glass_environment[3] = std::clamp(
        inst.glass_environment[3]
            + 0.44f * plan.glass_transmission.internal_transmission
            + 0.10f * plan.glass_substrate_adhesion.refraction_crawl
            + 0.08f * plan.glass_surface_cohesion.shape_coalescence
            - 0.14f * plan.glass_transmission.volume_absorption,
        0.0f,
        1.0f);
    inst.glass_stability[3] = std::clamp(
        inst.glass_stability[3]
            + 0.055f * plan.glass_transmission.internal_transmission
            - 0.085f * plan.glass_transmission.volume_absorption,
        -0.24f,
        0.24f);
    inst.refraction[0] = std::clamp(
        inst.refraction[0]
            + 0.10f * plan.glass_transmission.interlayer_refraction,
        0.0f,
        0.86f);
    inst.refraction[2] = std::clamp(
        inst.refraction[2]
            * (1.0f
               + 0.24f * plan.glass_transmission.interlayer_refraction),
        0.0f,
        8.0f);
    inst.refraction[3] = std::clamp(
        inst.refraction[3]
            + 0.035f * plan.glass_transmission.interlayer_refraction,
        0.0f,
        0.44f);
    inst.spectral_tint[2] = std::clamp(
        inst.spectral_tint[2]
            + 0.030f * plan.glass_transmission.interlayer_refraction,
        0.0f,
        0.24f);
    inst.spectral_tint[3] = std::clamp(
        inst.spectral_tint[3]
            + 0.040f * plan.glass_transmission.subsurface_scatter,
        0.0f,
        0.34f);
    inst.lighting[2] = std::clamp(
        inst.lighting[2]
            + 0.050f * plan.glass_transmission.subsurface_scatter,
        0.0f,
        0.56f);
    inst.lighting[3] = std::clamp(
        inst.lighting[3]
            + 0.040f * plan.glass_transmission.volume_absorption,
        0.0f,
        0.42f);
    inst.group_rect[0] = inst.rect[0];
    inst.group_rect[1] = inst.rect[1];
    inst.group_rect[2] = inst.rect[2];
    inst.group_rect[3] = inst.rect[3];
    inst.group_effects[3] = static_cast<float>(command_index);
    out.push_back(inst);
}

inline void apply_material_paint_layer_execution_ranges(FrameScratch& scratch);

inline void apply_material_container_execution_descriptors(
        FrameScratch& scratch) {
    if (scratch.material_records.empty())
        return;
    scratch.material_container_execution_descriptors.clear();
    scratch.material_container_execution_descriptors.reserve(
        scratch.material_records.size());
    for (std::size_t index = 0; index < scratch.material_records.size(); ++index) {
        auto const& record = scratch.material_records[index];
        auto const& plan = record.plan;
        if (!plan.container.participates || plan.container.container_id == 0u)
            continue;
        auto const container_id = plan.container.container_id;
        auto seen = false;
        for (std::size_t prior = 0; prior < index; ++prior) {
            auto const& prior_plan = scratch.material_records[prior].plan;
            if (prior_plan.container.participates
                && prior_plan.container.container_id == container_id) {
                seen = true;
                break;
            }
        }
        if (seen)
            continue;
        auto const group = accumulate_material_container_group(
            scratch.material_records,
            container_id);
        for (auto const& candidate : scratch.material_records) {
            if (!material_plan_in_container(candidate.plan, container_id))
                continue;
            scratch.material_container_execution_descriptors.push_back(
                material_container_execution_descriptor_from_group(
                    candidate,
                    scratch.material_records,
                    group));
        }
    }
    apply_material_paint_layer_execution_ranges(scratch);
    for (auto& inst : scratch.material_instances) {
        auto const command_index =
            static_cast<std::uint32_t>(
                std::max(0.0f, std::round(inst.group_effects[3])));
        inst.group_effects[3] = 0.0f;
        auto const* execution =
            static_cast<MaterialContainerExecutionDescriptor const*>(nullptr);
        for (auto const& candidate :
             scratch.material_container_execution_descriptors) {
            if (candidate.command_index == command_index) {
                execution = &candidate;
                break;
            }
        }
        if (!execution)
            continue;
        auto const* record =
            static_cast<MaterialRuntimeRecord const*>(nullptr);
        for (auto const& candidate : scratch.material_records) {
            if (candidate.command_index == command_index) {
                record = &candidate;
                break;
            }
        }
        if (record && execution->glass_effect_materialize_execution) {
            auto const transition =
                material_execution_transition(
                    record->plan.transition,
                    execution);
            auto const gain_ratio = [](float target_gain,
                                       float current_gain) noexcept {
                if (current_gain <= 0.0001f)
                    return std::clamp(target_gain, 0.0f, 1.5f);
                return std::clamp(target_gain / current_gain, 0.0f, 1.5f);
            };
            auto const opacity_ratio = gain_ratio(
                execution->glass_effect_materialize_opacity_gain,
                record->plan.transition.opacity_gain);
            auto const optical_ratio = gain_ratio(
                execution->glass_effect_materialize_optical_gain,
                record->plan.transition.optical_gain);
            auto const shadow_ratio = gain_ratio(
                execution->glass_effect_materialize_shadow_gain,
                record->plan.transition.shadow_gain);
            auto const refraction_ratio = gain_ratio(
                execution->glass_effect_materialize_refraction_gain,
                record->plan.transition.refraction_gain);
            auto const geometry =
                material_surface_execution_geometry(record->plan, execution);
            if (!geometry.active) {
                inst.rect[2] = 0.0f;
                inst.rect[3] = 0.0f;
                inst.params[2] = 0.0f;
                continue;
            }
            inst.rect[0] = geometry.x;
            inst.rect[1] = geometry.y;
            inst.rect[2] = geometry.w;
            inst.rect[3] = geometry.h;
            inst.params[0] = geometry.radius;
            inst.params[2] = std::clamp(
                inst.params[2] * opacity_ratio,
                0.0f,
                1.0f);
            inst.tint[3] = std::clamp(inst.tint[3] * opacity_ratio,
                                      0.0f,
                                      1.0f);
            inst.params[1] = std::clamp(inst.params[1] * optical_ratio,
                                        0.0f,
                                        36.0f);
            inst.optics[0] =
                1.0f + (inst.optics[0] - 1.0f) * optical_ratio;
            inst.optics[3] = std::clamp(inst.optics[3] * optical_ratio,
                                        0.0f,
                                        1.0f);
            inst.effects[1] = std::clamp(inst.effects[1] * shadow_ratio,
                                         0.0f,
                                         0.4f);
            inst.effects[2] =
                std::max(0.0f, inst.effects[2] * shadow_ratio);
            inst.effects[3] = std::clamp(inst.effects[3] * optical_ratio,
                                         0.0f,
                                         0.05f);
            inst.refraction[0] = std::clamp(
                inst.refraction[0] * refraction_ratio,
                0.0f,
                0.35f);
            inst.refraction[2] = std::clamp(
                inst.refraction[2] * refraction_ratio,
                0.0f,
                8.0f);
            inst.refraction[3] = std::clamp(
                inst.refraction[3] * refraction_ratio,
                0.0f,
                0.35f);
            inst.edge_optics[1] = std::clamp(
                inst.edge_optics[1] * optical_ratio,
                0.0f,
                0.90f);
            inst.edge_optics[2] = std::clamp(
                inst.edge_optics[2] * shadow_ratio,
                0.0f,
                0.80f);
            inst.edge_optics[3] = std::clamp(
                inst.edge_optics[3] * refraction_ratio,
                0.0f,
                0.16f);
            inst.spectral_tint[0] = std::clamp(
                inst.spectral_tint[0] * optical_ratio,
                0.0f,
                0.22f);
            inst.spectral_tint[1] = std::clamp(
                inst.spectral_tint[1] * optical_ratio,
                0.0f,
                0.22f);
            inst.spectral_tint[2] = std::clamp(
                inst.spectral_tint[2] * optical_ratio,
                0.0f,
                0.22f);
            inst.spectral_tint[3] = std::clamp(
                inst.spectral_tint[3] * optical_ratio,
                0.0f,
                0.28f);
            inst.interaction[3] = std::clamp(
                inst.interaction[3] * optical_ratio,
                0.0f,
                1.0f);
            inst.lighting[2] = std::clamp(
                inst.lighting[2] * optical_ratio,
                0.0f,
                0.45f);
            inst.lighting[3] = std::clamp(
                inst.lighting[3] * optical_ratio,
                0.0f,
                0.36f);
            inst.group_rect[0] = inst.rect[0];
            inst.group_rect[1] = inst.rect[1];
            inst.group_rect[2] = inst.rect[2];
            inst.group_rect[3] = inst.rect[3];
            inst.transition_optics[0] =
                transition.materialize_wave_strength;
            inst.transition_optics[1] = transition.materialize_edge_lift;
            inst.transition_optics[2] =
                transition.materialize_lensing_gain;
            inst.transition_optics[3] =
                transition.materialize_rim_position;
        }
        if (execution->group_bounds_valid && execution->shape_blend_execution) {
            if (record
                && (execution->glass_effect_match_execution
                    || execution->union_execution
                    || execution->group_surface_execution)) {
                auto const geometry = material_surface_execution_geometry(
                    record->plan,
                    execution);
                if (!geometry.active) {
                    inst.rect[2] = 0.0f;
                    inst.rect[3] = 0.0f;
                    inst.params[2] = 0.0f;
                    continue;
                }
                inst.rect[0] = geometry.x;
                inst.rect[1] = geometry.y;
                inst.rect[2] = geometry.w;
                inst.rect[3] = geometry.h;
                inst.params[0] = geometry.radius;
                inst.group_rect[0] = inst.rect[0];
                inst.group_rect[1] = inst.rect[1];
                inst.group_rect[2] = inst.rect[2];
                inst.group_rect[3] = inst.rect[3];
                auto const match_motion =
                    material_glass_effect_match_motion_optics(*execution);
                auto const bridge_motion = match_motion.active
                    ? MaterialGlassEffectMotionOptics{}
                    : material_container_bridge_motion_optics(
                        *record,
                        scratch.material_records,
                        *execution);
                auto const& container_motion = match_motion.active
                    ? match_motion
                    : bridge_motion;
                if (container_motion.active) {
                    inst.bridge_motion[0] = container_motion.direction_x;
                    inst.bridge_motion[1] = container_motion.direction_y;
                    inst.bridge_motion[2] =
                        container_motion.specular_anchor_x;
                    inst.bridge_motion[3] =
                        container_motion.specular_anchor_y;
                    inst.bridge_optics[0] = container_motion.strength;
                    inst.bridge_optics[1] =
                        container_motion.flow_offset_gain;
                    inst.bridge_optics[2] = container_motion.ribbon_width;
                    inst.bridge_optics[3] =
                        container_motion.highlight_gain;
                }
                if (execution->group_interaction_source_valid) {
                    inst.interaction[0] =
                        execution->group_interaction_specular_anchor_x;
                    inst.interaction[1] =
                        execution->group_interaction_specular_anchor_y;
                    inst.interaction[2] = std::clamp(
                        std::max(
                            inst.interaction[2],
                            execution->group_interaction_specular_radius),
                        0.0f,
                        1.0f);
                    inst.interaction[3] = std::clamp(
                        std::max(
                            inst.interaction[3],
                            execution->group_interaction_specular_intensity),
                        0.0f,
                        1.0f);
                } else if (record->plan.specular.interaction_driven) {
                    inst.interaction[0] = material_surface_execution_anchor_x(
                        record->plan,
                        geometry,
                        record->plan.specular.anchor_x);
                    inst.interaction[1] = material_surface_execution_anchor_y(
                        record->plan,
                        geometry,
                        record->plan.specular.anchor_y);
                } else if (container_motion.active) {
                    inst.interaction[0] = container_motion.specular_anchor_x;
                    inst.interaction[1] = container_motion.specular_anchor_y;
                    inst.interaction[2] = std::clamp(
                        std::max(
                            inst.interaction[2],
                            record->plan.specular.radius
                                + 0.04f * container_motion.strength),
                        0.0f,
                        1.0f);
                    inst.interaction[3] = std::clamp(
                        inst.interaction[3]
                            * container_motion.specular_intensity_gain
                            + 0.06f * container_motion.strength,
                        0.0f,
                        1.0f);
                } else {
                    inst.interaction[0] = 0.5f;
                    inst.interaction[1] = 0.5f;
                }
                if (execution->group_interaction_pointer_lens_active) {
                    inst.interaction_lens[0] =
                        execution->group_interaction_pointer_lens_anchor_x;
                    inst.interaction_lens[1] =
                        execution->group_interaction_pointer_lens_anchor_y;
                    inst.interaction_lens[2] = std::clamp(
                        std::max(
                            inst.interaction_lens[2],
                            execution->group_interaction_pointer_lens_radius),
                        0.0f,
                        1.0f);
                    inst.interaction_lens[3] = std::clamp(
                        std::max(
                            inst.interaction_lens[3],
                            execution
                                ->group_interaction_pointer_lens_strength),
                        0.0f,
                        1.0f);
                } else if (record->plan.interaction.pointer_lens_active) {
                    inst.interaction_lens[0] =
                        material_surface_execution_anchor_x(
                            record->plan,
                            geometry,
                            record->plan.interaction.pointer_lens_anchor_x);
                    inst.interaction_lens[1] =
                        material_surface_execution_anchor_y(
                            record->plan,
                            geometry,
                            record->plan.interaction.pointer_lens_anchor_y);
                }
                if (execution->group_interaction_control_morph_active) {
                    inst.control_morph[0] =
                        execution
                            ->group_interaction_control_morph_scale_delta;
                    inst.control_morph[1] =
                        execution->group_interaction_control_morph_depth;
                    inst.control_morph[2] =
                        execution->group_interaction_control_morph_edge;
                    inst.control_morph[3] =
                        execution->group_interaction_control_morph_shadow;
                }
                if (execution->group_interaction_refraction_active) {
                    inst.refraction[0] = std::clamp(
                        std::max(
                            inst.refraction[0],
                            execution
                                ->group_interaction_refraction_strength),
                        0.0f,
                        0.35f);
                    inst.refraction[1] = std::clamp(
                        std::max(
                            inst.refraction[1],
                            execution
                                ->group_interaction_refraction_edge_bias),
                        0.0f,
                        1.0f);
                    inst.refraction[2] = std::clamp(
                        std::max(
                            inst.refraction[2],
                            execution
                                ->group_interaction_refraction_max_offset_pixels),
                        0.0f,
                        5.0f);
                    inst.refraction[3] = std::clamp(
                        std::max(
                            inst.refraction[3],
                            execution
                                ->group_interaction_refraction_edge_caustic_intensity),
                        0.0f,
                        0.35f);
                }
                if (execution->group_interaction_dynamic_lighting_active) {
                    inst.lighting[0] =
                        execution
                            ->group_interaction_dynamic_light_direction_x;
                    inst.lighting[1] =
                        execution
                            ->group_interaction_dynamic_light_direction_y;
                    inst.lighting[2] = std::clamp(
                        std::max(
                            inst.lighting[2],
                            execution
                                ->group_interaction_dynamic_light_highlight),
                        0.0f,
                        0.45f);
                    inst.lighting[3] = std::clamp(
                        std::max(
                            inst.lighting[3],
                            execution
                                ->group_interaction_dynamic_light_shadow),
                        0.0f,
                        0.36f);
                }
                if (execution->group_interaction_glass_thickness_active) {
                    inst.thickness[0] = std::clamp(
                        std::max(
                            inst.thickness[0],
                            execution->group_interaction_glass_thickness),
                        0.0f,
                        0.78f);
                    inst.thickness[1] = std::clamp(
                        std::max(
                            inst.thickness[1],
                            execution
                                ->group_interaction_glass_lensing_gain),
                        1.0f,
                        1.48f);
                    inst.thickness[2] = std::clamp(
                        std::max(
                            inst.thickness[2],
                            execution
                                ->group_interaction_glass_shadow_gain),
                        1.0f,
                        1.44f);
                    inst.thickness[3] = std::clamp(
                        std::max(
                            inst.thickness[3],
                            execution
                                ->group_interaction_glass_scattering_gain),
                        1.0f,
                        1.40f);
                }
                if (execution->group_interaction_glass_dispersion_active) {
                    inst.dispersion[0] = std::clamp(
                        std::max(
                            inst.dispersion[0],
                            execution
                                ->group_interaction_glass_dispersion_axial_offset),
                        0.0f,
                        3.20f);
                    inst.dispersion[1] = std::clamp(
                        std::max(
                            inst.dispersion[1],
                            execution
                                ->group_interaction_glass_dispersion_tangential_offset),
                        0.0f,
                        2.45f);
                    inst.dispersion[2] = std::clamp(
                        std::max(
                            inst.dispersion[2],
                            execution
                                ->group_interaction_glass_dispersion_prismatic_gain),
                        1.0f,
                        1.75f);
                    inst.dispersion[3] = std::clamp(
                        std::max(
                            inst.dispersion[3],
                            execution
                                ->group_interaction_glass_dispersion_caustic_spread),
                        0.0f,
                        0.40f);
                }
                if (execution->glass_effect_match_appearance_active) {
                    auto const blend = std::clamp(
                        execution->glass_effect_match_appearance_blend,
                        0.0f,
                        1.0f);
                    if (execution->glass_effect_match_appearance_tint_active) {
                        inst.tint[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_tint_r,
                                inst.tint[0],
                                blend),
                            0.0f,
                            1.0f);
                        inst.tint[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_tint_g,
                                inst.tint[1],
                                blend),
                            0.0f,
                            1.0f);
                        inst.tint[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_tint_b,
                                inst.tint[2],
                                blend),
                            0.0f,
                            1.0f);
                        inst.tint[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_tint_a,
                                inst.tint[3],
                                blend),
                            0.0f,
                            1.0f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_spectral_tint_active) {
                        inst.spectral_tint[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_spectral_tint_warmth,
                                inst.spectral_tint[0],
                                blend),
                            0.0f,
                            0.22f);
                        inst.spectral_tint[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_spectral_tint_coolness,
                                inst.spectral_tint[1],
                                blend),
                            0.0f,
                            0.22f);
                        inst.spectral_tint[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_spectral_tint_dispersion,
                                inst.spectral_tint[2],
                                blend),
                            0.0f,
                            0.22f);
                        inst.spectral_tint[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_spectral_tint_rim,
                                inst.spectral_tint[3],
                                blend),
                            0.0f,
                            0.28f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_prominent_glass_active) {
                        inst.prominent_glass[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_prominent_glass_intensity,
                                inst.prominent_glass[0],
                                blend),
                            0.0f,
                            1.0f);
                        inst.prominent_glass[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_prominent_glass_tint_weight,
                                inst.prominent_glass[1],
                                blend),
                            0.0f,
                            0.74f);
                        inst.prominent_glass[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_prominent_glass_edge_lift,
                                inst.prominent_glass[2],
                                blend),
                            0.0f,
                            0.30f);
                        inst.prominent_glass[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_prominent_glass_lensing_gain,
                                inst.prominent_glass[3],
                                blend),
                            1.0f,
                            1.36f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_clear_glass_active) {
                        inst.clear_glass[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_clear_glass_dimming,
                                inst.clear_glass[0],
                                blend),
                            0.0f,
                            0.34f);
                        inst.clear_glass[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_clear_glass_contrast,
                                inst.clear_glass[1],
                                blend),
                            0.0f,
                            0.28f);
                        inst.clear_glass[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_clear_glass_brightness_response,
                                inst.clear_glass[2],
                                blend),
                            0.0f,
                            1.0f);
                        inst.clear_glass[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_clear_glass_detail_response,
                                inst.clear_glass[3],
                                blend),
                            0.0f,
                            1.0f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_refraction_active) {
                        inst.refraction[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_refraction_strength,
                                inst.refraction[0],
                                blend),
                            0.0f,
                            0.35f);
                        inst.refraction[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_refraction_edge_bias,
                                inst.refraction[1],
                                blend),
                            0.0f,
                            1.0f);
                        inst.refraction[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_refraction_max_offset_pixels,
                                inst.refraction[2],
                                blend),
                            0.0f,
                            8.0f);
                        inst.refraction[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_refraction_edge_caustic_intensity,
                                inst.refraction[3],
                                blend),
                            0.0f,
                            0.35f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_dynamic_lighting_active) {
                        inst.lighting[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_dynamic_light_direction_x,
                                inst.lighting[0],
                                blend),
                            -0.98f,
                            0.98f);
                        inst.lighting[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_dynamic_light_direction_y,
                                inst.lighting[1],
                                blend),
                            -0.98f,
                            0.98f);
                        inst.lighting[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_dynamic_light_highlight,
                                inst.lighting[2],
                                blend),
                            0.0f,
                            0.45f);
                        inst.lighting[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_dynamic_light_shadow,
                                inst.lighting[3],
                                blend),
                            0.0f,
                            0.36f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_glass_thickness_active) {
                        inst.thickness[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_thickness,
                                inst.thickness[0],
                                blend),
                            0.0f,
                            0.78f);
                        inst.thickness[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_lensing_gain,
                                inst.thickness[1],
                                blend),
                            1.0f,
                            1.48f);
                        inst.thickness[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_shadow_gain,
                                inst.thickness[2],
                                blend),
                            1.0f,
                            1.44f);
                        inst.thickness[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_scattering_gain,
                                inst.thickness[3],
                                blend),
                            1.0f,
                            1.40f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_glass_dispersion_active) {
                        inst.dispersion[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_dispersion_axial_offset,
                                inst.dispersion[0],
                                blend),
                            0.0f,
                            3.20f);
                        inst.dispersion[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_dispersion_tangential_offset,
                                inst.dispersion[1],
                                blend),
                            0.0f,
                            2.45f);
                        inst.dispersion[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_dispersion_prismatic_gain,
                                inst.dispersion[2],
                                blend),
                            1.0f,
                            1.75f);
                        inst.dispersion[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_dispersion_caustic_spread,
                                inst.dispersion[3],
                                blend),
                            0.0f,
                            0.40f);
                    }
                }
                if (execution->group_appearance_tint_active) {
                    inst.tint[0] = std::clamp(
                        execution->group_appearance_tint_r,
                        0.0f,
                        1.0f);
                    inst.tint[1] = std::clamp(
                        execution->group_appearance_tint_g,
                        0.0f,
                        1.0f);
                    inst.tint[2] = std::clamp(
                        execution->group_appearance_tint_b,
                        0.0f,
                        1.0f);
                    inst.tint[3] = std::clamp(
                        execution->group_appearance_tint_a,
                        0.0f,
                        1.0f);
                }
                if (execution->group_appearance_spectral_tint_active) {
                    inst.spectral_tint[0] = std::clamp(
                        std::max(
                            inst.spectral_tint[0],
                            execution
                                ->group_appearance_spectral_tint_warmth),
                        0.0f,
                        0.22f);
                    inst.spectral_tint[1] = std::clamp(
                        std::max(
                            inst.spectral_tint[1],
                            execution
                                ->group_appearance_spectral_tint_coolness),
                        0.0f,
                        0.22f);
                    inst.spectral_tint[2] = std::clamp(
                        std::max(
                            inst.spectral_tint[2],
                            execution
                                ->group_appearance_spectral_tint_dispersion),
                        0.0f,
                        0.22f);
                    inst.spectral_tint[3] = std::clamp(
                        std::max(
                            inst.spectral_tint[3],
                            execution->group_appearance_spectral_tint_rim),
                        0.0f,
                        0.28f);
                }
                if (execution->group_appearance_prominent_glass_active) {
                    inst.prominent_glass[0] = std::clamp(
                        std::max(
                            inst.prominent_glass[0],
                            execution
                                ->group_appearance_prominent_glass_intensity),
                        0.0f,
                        1.0f);
                    inst.prominent_glass[1] = std::clamp(
                        std::max(
                            inst.prominent_glass[1],
                            execution
                                ->group_appearance_prominent_glass_tint_weight),
                        0.0f,
                        0.74f);
                    inst.prominent_glass[2] = std::clamp(
                        std::max(
                            inst.prominent_glass[2],
                            execution
                                ->group_appearance_prominent_glass_edge_lift),
                        0.0f,
                        0.30f);
                    inst.prominent_glass[3] = std::clamp(
                        std::max(
                            inst.prominent_glass[3],
                            execution
                                ->group_appearance_prominent_glass_lensing_gain),
                        1.0f,
                        1.36f);
                }
                if (execution->group_appearance_clear_glass_active) {
                    inst.clear_glass[0] = std::clamp(
                        std::max(
                            inst.clear_glass[0],
                            execution->group_appearance_clear_glass_dimming),
                        0.0f,
                        0.34f);
                    inst.clear_glass[1] = std::clamp(
                        std::max(
                            inst.clear_glass[1],
                            execution->group_appearance_clear_glass_contrast),
                        0.0f,
                        0.28f);
                    inst.clear_glass[2] = std::clamp(
                        std::max(
                            inst.clear_glass[2],
                            execution
                                ->group_appearance_clear_glass_brightness_response),
                        0.0f,
                        1.0f);
                    inst.clear_glass[3] = std::clamp(
                        std::max(
                            inst.clear_glass[3],
                            execution
                                ->group_appearance_clear_glass_detail_response),
                        0.0f,
                        1.0f);
                }
                if (execution->group_appearance_refraction_active) {
                    inst.refraction[0] = std::clamp(
                        std::max(
                            inst.refraction[0],
                            execution->group_appearance_refraction_strength),
                        0.0f,
                        0.35f);
                    inst.refraction[1] = std::clamp(
                        std::max(
                            inst.refraction[1],
                            execution->group_appearance_refraction_edge_bias),
                        0.0f,
                        1.0f);
                    inst.refraction[2] = std::clamp(
                        std::max(
                            inst.refraction[2],
                            execution
                                ->group_appearance_refraction_max_offset_pixels),
                        0.0f,
                        8.0f);
                    inst.refraction[3] = std::clamp(
                        std::max(
                            inst.refraction[3],
                            execution
                                ->group_appearance_refraction_edge_caustic_intensity),
                        0.0f,
                        0.35f);
                }
                if (execution->group_appearance_dynamic_lighting_active) {
                    inst.lighting[0] =
                        execution
                            ->group_appearance_dynamic_light_direction_x;
                    inst.lighting[1] =
                        execution
                            ->group_appearance_dynamic_light_direction_y;
                    inst.lighting[2] = std::clamp(
                        std::max(
                            inst.lighting[2],
                            execution
                                ->group_appearance_dynamic_light_highlight),
                        0.0f,
                        0.45f);
                    inst.lighting[3] = std::clamp(
                        std::max(
                            inst.lighting[3],
                            execution
                                ->group_appearance_dynamic_light_shadow),
                        0.0f,
                        0.36f);
                }
                if (execution->group_appearance_glass_thickness_active) {
                    inst.thickness[0] = std::clamp(
                        std::max(
                            inst.thickness[0],
                            execution->group_appearance_glass_thickness),
                        0.0f,
                        0.78f);
                    inst.thickness[1] = std::clamp(
                        std::max(
                            inst.thickness[1],
                            execution
                                ->group_appearance_glass_lensing_gain),
                        1.0f,
                        1.48f);
                    inst.thickness[2] = std::clamp(
                        std::max(
                            inst.thickness[2],
                            execution
                                ->group_appearance_glass_shadow_gain),
                        1.0f,
                        1.44f);
                    inst.thickness[3] = std::clamp(
                        std::max(
                            inst.thickness[3],
                            execution
                                ->group_appearance_glass_scattering_gain),
                        1.0f,
                        1.40f);
                }
                if (execution->group_appearance_glass_dispersion_active) {
                    inst.dispersion[0] = std::clamp(
                        std::max(
                            inst.dispersion[0],
                            execution
                                ->group_appearance_glass_dispersion_axial_offset),
                        0.0f,
                        3.20f);
                    inst.dispersion[1] = std::clamp(
                        std::max(
                            inst.dispersion[1],
                            execution
                                ->group_appearance_glass_dispersion_tangential_offset),
                        0.0f,
                        2.45f);
                    inst.dispersion[2] = std::clamp(
                        std::max(
                            inst.dispersion[2],
                            execution
                                ->group_appearance_glass_dispersion_prismatic_gain),
                        1.0f,
                        1.75f);
                    inst.dispersion[3] = std::clamp(
                        std::max(
                            inst.dispersion[3],
                            execution
                                ->group_appearance_glass_dispersion_caustic_spread),
                        0.0f,
                        0.40f);
                }
                if (execution->overlap_response_active) {
                    auto const overlap = std::clamp(
                        execution->overlap_response_strength,
                        0.0f,
                        1.0f);
                    inst.refraction[0] = std::clamp(
                        inst.refraction[0] * (1.0f - 0.10f * overlap),
                        0.0f,
                        0.35f);
                    inst.refraction[2] = std::clamp(
                        inst.refraction[2] * (1.0f - 0.12f * overlap),
                        0.0f,
                        5.0f);
                    inst.edge_optics[1] = std::clamp(
                        inst.edge_optics[1] + 0.030f * overlap,
                        0.0f,
                        0.85f);
                    inst.edge_optics[2] = std::clamp(
                        inst.edge_optics[2] + 0.024f * overlap,
                        0.0f,
                        0.60f);
                    inst.clear_glass[0] = std::clamp(
                        std::max(inst.clear_glass[0], 0.070f * overlap),
                        0.0f,
                        0.34f);
                    inst.clear_glass[1] = std::clamp(
                        std::max(inst.clear_glass[1], 0.052f * overlap),
                        0.0f,
                        0.28f);
                    inst.thickness[3] = std::clamp(
                        inst.thickness[3] + 0.050f * overlap,
                        1.0f,
                        1.40f);
                }
                if (container_motion.active) {
                    inst.refraction[0] = std::clamp(
                        inst.refraction[0]
                            * (1.0f + 0.18f * container_motion.strength),
                        0.0f,
                        0.35f);
                    inst.refraction[2] = std::clamp(
                        inst.refraction[2] * container_motion.refraction_gain
                            + 0.12f * container_motion.strength,
                        0.0f,
                        5.0f);
                    inst.refraction[3] = std::clamp(
                        inst.refraction[3] * container_motion.caustic_gain
                            + 0.035f * container_motion.strength,
                        0.0f,
                        0.35f);
                    inst.edge_optics[0] = std::clamp(
                        std::max(
                            inst.edge_optics[0],
                            record->plan.edge_optics.bevel_width
                                + 0.80f * container_motion.strength),
                        0.0f,
                        16.0f);
                    inst.edge_optics[1] = std::clamp(
                        inst.edge_optics[1]
                            * (1.0f + 0.40f * container_motion.strength)
                            + 0.040f * container_motion.strength,
                        0.0f,
                        0.85f);
                    inst.edge_optics[2] = std::clamp(
                        inst.edge_optics[2]
                            * (1.0f + 0.32f * container_motion.strength)
                            + 0.030f * container_motion.strength,
                        0.0f,
                        0.60f);
                    inst.edge_optics[3] = std::clamp(
                        inst.edge_optics[3]
                            + 0.025f * container_motion.strength,
                        0.0f,
                        0.16f);
                    inst.spectral_tint[2] = std::clamp(
                        inst.spectral_tint[2]
                            + 0.030f * container_motion.strength,
                        0.0f,
                        0.22f);
                    inst.spectral_tint[3] = std::clamp(
                        inst.spectral_tint[3]
                            + 0.040f * container_motion.strength,
                        0.0f,
                        0.28f);
                    inst.lighting[2] = std::clamp(
                        inst.lighting[2]
                            + 0.055f * container_motion.strength,
                        0.0f,
                        0.45f);
                    inst.lighting[3] = std::clamp(
                        inst.lighting[3]
                            + 0.040f * container_motion.strength,
                        0.0f,
                        0.36f);
                    inst.thickness[0] = std::clamp(
                        inst.thickness[0]
                            + 0.080f * container_motion.strength,
                        0.0f,
                        0.78f);
                    inst.thickness[1] = std::clamp(
                        inst.thickness[1]
                            + 0.070f * container_motion.strength,
                        1.0f,
                        1.48f);
                    inst.thickness[2] = std::clamp(
                        inst.thickness[2]
                            + 0.055f * container_motion.strength,
                        1.0f,
                        1.44f);
                    inst.thickness[3] = std::clamp(
                        inst.thickness[3]
                            + 0.050f * container_motion.strength,
                        1.0f,
                        1.40f);
                    inst.dispersion[0] = std::clamp(
                        inst.dispersion[0]
                            + 0.16f * container_motion.strength,
                        0.0f,
                        3.20f);
                    inst.dispersion[1] = std::clamp(
                        inst.dispersion[1]
                            + 0.12f * container_motion.strength,
                        0.0f,
                        2.45f);
                    inst.dispersion[2] = std::clamp(
                        inst.dispersion[2]
                            + 0.08f * container_motion.strength,
                        1.0f,
                        1.75f);
                    inst.dispersion[3] = std::clamp(
                        inst.dispersion[3]
                            + 0.030f * container_motion.strength,
                        0.0f,
                        0.40f);
                }
            } else {
                inst.group_rect[0] = execution->group_x;
                inst.group_rect[1] = execution->group_y;
                inst.group_rect[2] = execution->group_w;
                inst.group_rect[3] = execution->group_h;
            }
            inst.group_effects[0] = execution->shape_blend_strength;
            inst.group_effects[1] =
                execution->inner_edge_alpha_blend_strength;
            inst.group_effects[2] =
                (execution->shape_blend_execution ? 1.0f : 0.0f)
                + (execution->union_execution ? 2.0f : 0.0f)
                + (execution->morph_execution ? 4.0f : 0.0f)
                + (execution->shared_backdrop_scope ? 8.0f : 0.0f)
                + (execution->glass_effect_match_execution ? 16.0f : 0.0f)
                + (execution->group_surface_execution ? 32.0f : 0.0f);
            inst.group_effects[3] = execution->overlap_response_strength;
            inst.fusion_optics[0] = execution->fusion_strength;
            inst.fusion_optics[1] = execution->fusion_lensing_gain;
            inst.fusion_optics[2] = execution->fusion_edge_lift;
            inst.fusion_optics[3] = execution->fusion_shadow_gain;
            inst.container_cohesion[0] = execution->cohesion_strength;
            inst.container_cohesion[1] = execution->cohesion_pressure;
            inst.container_cohesion[2] = execution->cohesion_falloff;
            inst.container_cohesion[3] = execution->cohesion_stabilization;
            inst.glass_union[0] = std::max(
                inst.glass_union[0],
                execution->union_response_strength);
            inst.glass_union[1] = std::max(
                inst.glass_union[1],
                execution->union_edge_continuity);
            inst.glass_union[2] = std::max(
                inst.glass_union[2],
                execution->union_shape_coalescence);
            inst.glass_union[3] = std::max(
                inst.glass_union[3],
                execution->union_luma_stability);
        }
    }
}

inline float material_paint_layer_alpha(
        MaterialPaintLayer const& layer) noexcept {
    auto const color_alpha = layer.color.a / 255.0f;
    if (material_paint_layer_matches(layer.executor, "rounded-fill"))
        return std::max(color_alpha, layer.opacity);
    return color_alpha * layer.opacity;
}

inline bool configure_material_paint_layer_instance(
        ColorInstanceGPU& inst,
        MaterialPlan const& plan,
        MaterialPaintLayer const& layer,
        MaterialContainerExecutionDescriptor const* execution = nullptr) {
    auto const geometry =
        material_paint_layer_execution_geometry(plan, layer, execution);
    if (!geometry.active)
        return false;
    auto const rounded_edge =
        material_paint_layer_matches(layer.executor, "rounded-edge");
    inst.rect[0] = geometry.x;
    inst.rect[1] = geometry.y;
    inst.rect[2] = geometry.w;
    inst.rect[3] = geometry.h;
    inst.color[0] = layer.color.r / 255.0f;
    inst.color[1] = layer.color.g / 255.0f;
    inst.color[2] = layer.color.b / 255.0f;
    inst.color[3] = material_paint_layer_alpha(layer);
    inst.params[0] = geometry.radius;
    inst.params[1] = rounded_edge ? std::max(layer.stroke_width, 0.5f) : 0.0f;
    inst.params[2] =
        rounded_edge ? 3.0f : (layer.soft_edge_radius > 0.0f ? 5.0f : 2.0f);
    inst.params[3] = layer.soft_edge_radius;
    return true;
}

inline void append_material_paint_layer_instance(
        std::vector<ColorInstanceGPU>& out,
        MaterialPlan const& plan,
        MaterialPaintLayer const& layer) {
    ColorInstanceGPU inst{};
    if (!configure_material_paint_layer_instance(inst, plan, layer))
        return;
    out.push_back(inst);
}

inline void append_material_paint_layer_instances(
        std::vector<ColorInstanceGPU>& out,
        MaterialPlan const& plan) {
    for (unsigned int i = 0; i < plan.paint_layer_count; ++i)
        append_material_paint_layer_instance(out, plan, plan.paint_layers[i]);
}

inline MaterialRuntimeRecord const* find_material_runtime_record(
        std::span<MaterialRuntimeRecord const> records,
        std::uint32_t command_index) {
    for (auto const& record : records) {
        if (record.command_index == command_index)
            return &record;
    }
    return nullptr;
}

inline MaterialContainerExecutionDescriptor const*
find_material_container_execution_descriptor(
        std::span<MaterialContainerExecutionDescriptor const> descriptors,
        std::uint32_t command_index) {
    for (auto const& descriptor : descriptors) {
        if (descriptor.command_index == command_index)
            return &descriptor;
    }
    return nullptr;
}

inline void apply_material_paint_layer_execution_range(
        std::vector<ColorInstanceGPU>& instances,
        MaterialPlan const& plan,
        MaterialContainerExecutionDescriptor const* execution,
        std::size_t first,
        std::size_t count) {
    auto cursor = first;
    auto const end = std::min(first + count, instances.size());
    for (unsigned int i = 0; i < plan.paint_layer_count && cursor < end; ++i) {
        auto const& layer = plan.paint_layers[i];
        if (!material_paint_layer_execution_geometry(plan, layer).active)
            continue;
        if (!configure_material_paint_layer_instance(
                instances[cursor],
                plan,
                layer,
                execution)) {
            instances[cursor].color[3] = 0.0f;
        }
        ++cursor;
    }
    while (cursor < end) {
        instances[cursor].color[3] = 0.0f;
        ++cursor;
    }
}

inline void apply_material_paint_layer_execution_ranges(FrameScratch& scratch) {
    if (scratch.material_paint_layer_ranges.empty()
        || scratch.material_container_execution_descriptors.empty()) {
        return;
    }
    for (auto const& range : scratch.material_paint_layer_ranges) {
        if (range.batch_index >= scratch.batches.size())
            continue;
        auto const* record = find_material_runtime_record(
            scratch.material_records,
            range.command_index);
        auto const* execution = find_material_container_execution_descriptor(
            scratch.material_container_execution_descriptors,
            range.command_index);
        if (!record || !execution)
            continue;
        auto const flat_first =
            static_cast<std::size_t>(
                scratch.batches[range.batch_index].color_first)
            + range.first;
        apply_material_paint_layer_execution_range(
            scratch.color_instances,
            record->plan,
            execution,
            flat_first,
            range.count);
    }
}

} // namespace phenotype::native::detail
#endif

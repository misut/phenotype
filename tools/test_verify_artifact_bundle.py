#!/usr/bin/env python3
"""Self-tests for the artifact verifier's machine-readable failure contract."""

from __future__ import annotations

import contextlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest

import verify_artifact_bundle as verifier


def stage_optics(
    channel: str,
    *,
    opacity: float = 0.0,
    blur_radius: float = 0.0,
    tint_alpha: float = 0.0,
    saturation: float = 1.0,
    luminance_floor: float = 0.0,
    luminance_gain: float = 1.0,
    edge_highlight: float = 0.0,
    edge_width: float = 0.0,
    noise_opacity: float = 0.0,
    shadow_alpha: float = 0.0,
    shadow_radius: float = 0.0,
    specular_model: str = "none",
    specular_anchor_x: float = 0.5,
    specular_anchor_y: float = 0.5,
    specular_radius: float = 0.0,
    specular_intensity: float = 0.0,
    refraction_model: str = "none",
    refraction_strength: float = 0.0,
    refraction_edge_bias: float = 0.0,
    refraction_offset_pixels: float = 0.0,
) -> dict[str, object]:
    return {
        "channel": channel,
        "opacity": opacity,
        "blur_radius": blur_radius,
        "tint_alpha": tint_alpha,
        "saturation": saturation,
        "luminance_floor": luminance_floor,
        "luminance_gain": luminance_gain,
        "edge_highlight": edge_highlight,
        "edge_width": edge_width,
        "noise_opacity": noise_opacity,
        "shadow_alpha": shadow_alpha,
        "shadow_radius": shadow_radius,
        "specular_model": specular_model,
        "specular_anchor_x": specular_anchor_x,
        "specular_anchor_y": specular_anchor_y,
        "specular_radius": specular_radius,
        "specular_intensity": specular_intensity,
        "refraction_model": refraction_model,
        "refraction_strength": refraction_strength,
        "refraction_edge_bias": refraction_edge_bias,
        "refraction_offset_pixels": refraction_offset_pixels,
    }


def material_pass(sample_taps: int) -> dict[str, object]:
    return {
        "name": "translucent-rounded-rect",
        "active": True,
        "requires_backdrop": False,
        "sample_taps": sample_taps,
        "likely_layer": "material-fallback-pass",
        "executor": "fallback-fill",
        "max_texture_copy_pixels": 0,
    }


def material_paint_layers(primary_name: str) -> list[dict[str, object]]:
    return [
        {
            "name": "fallback-shadow",
            "active": True,
            "executor": "rounded-shadow",
            "x_offset": 0.0,
            "y_offset": 2.52,
            "inflate": 7.0,
            "radius_delta": 7.0,
            "stroke_width": 0.0,
            "color": {"r": 0, "g": 0, "b": 0, "a": 255},
            "opacity": 0.14,
            "bounded": True,
        },
        {
            "name": primary_name,
            "active": True,
            "executor": "rounded-fill",
            "x_offset": 0.0,
            "y_offset": 0.0,
            "inflate": 0.0,
            "radius_delta": 0.0,
            "stroke_width": 0.0,
            "color": {"r": 255, "g": 255, "b": 255, "a": 148},
            "opacity": 0.58,
            "bounded": True,
        },
        {
            "name": "fallback-edge-highlight",
            "active": True,
            "executor": "rounded-edge",
            "x_offset": 0.0,
            "y_offset": 0.0,
            "inflate": 0.0,
            "radius_delta": 0.0,
            "stroke_width": 1.0,
            "color": {"r": 255, "g": 255, "b": 255, "a": 87},
            "opacity": 1.0,
            "bounded": True,
        },
    ]


def sampling_kernel_contract(sample_taps: int) -> dict[str, object]:
    if sample_taps <= 1:
        return {
            "name": "weighted-center",
            "radius": 0,
            "blur_step_scale": 0.0,
            "weight_profile": "center4",
        }
    if sample_taps <= 5:
        return {
            "name": "weighted-cross-5",
            "radius": 1,
            "blur_step_scale": 0.35,
            "weight_profile": "gaussian-cross-5-separable",
        }
    if sample_taps <= 9:
        return {
            "name": "weighted-3x3-grid",
            "radius": 1,
            "blur_step_scale": 0.35,
            "weight_profile": "gaussian-3x3-separable",
        }
    return {
        "name": "gaussian-5x5",
        "radius": 2,
        "blur_step_scale": 0.35,
        "weight_profile": "gaussian-5x5-separable",
    }


def material_plan(
    sample_taps: int = 0,
    primary_sample_taps: int = 0,
) -> dict[str, object]:
    primary = material_pass(primary_sample_taps)
    container: dict[str, object] = {
        "mode": "isolated",
        "container_id": 0,
        "union_id": 0,
        "spacing": 0.0,
        "interactive": False,
        "morph_transitions": False,
    }
    interaction_descriptor: dict[str, object] = {
        "hovered": False,
        "pressed": False,
        "focused": False,
        "pointer_inside": False,
        "active": False,
        "pointer_x": 0.5,
        "pointer_y": 0.5,
    }
    command_descriptor: dict[str, object] = {
        "kind": "regular",
        "role": "surface",
        "container": container,
        "interaction": interaction_descriptor,
        "opacity": 0.58,
        "blur_radius": 22.0,
        "tint": {"r": 255, "g": 255, "b": 255, "a": 148},
        "saturation": 1.08,
        "luminance_floor": 0.08,
        "luminance_gain": 1.08,
        "edge_highlight": 0.34,
        "edge_width": 1.0,
        "noise_opacity": 0.014,
        "shadow_alpha": 0.14,
        "shadow_radius": 14.0,
    }
    plan: dict[str, object] = {
        "contract_version": verifier.MATERIAL_PLAN_CONTRACT_VERSION,
        "kind": "regular",
        "role": "surface",
        "container": {
            **container,
            "blend_distance": 0.0,
            "participates": False,
            "requested_morph_transitions": False,
            "shared_backdrop_scope": False,
            "shape_union_expected": False,
            "shape_blending_expected": False,
            "reduced_motion_suppressed_morph": False,
            "spacing_clamped": False,
            "blend_policy": "isolated",
            "morph_policy": "isolated",
            "performance_policy": "single-surface",
        },
        "reference_model": {
            "technology": "liquid-glass",
            "variant": "regular",
            "shape": "rounded-rectangle",
            "shape_scope": "view-bounds",
            "blending_scope": "deterministic-fallback",
            "semantic_thickness": "regular",
            "accessibility_response": "standard",
            "performance_response": "deterministic-fallback",
            "view_bounds_anchored": True,
            "shape_matches_geometry": True,
            "tint_applied": True,
            "interactive_response": False,
            "container_grouped": False,
            "container_union": False,
            "container_morphing": False,
            "legibility_preserved": True,
            "vibrancy_expected": False,
            "deterministic_degradation": True,
        },
        "plan_id": "material.regular.fallback",
        "command_descriptor": command_descriptor,
        "geometry": {"x": 12.0, "y": 20.0, "w": 240.0, "h": 96.0, "radius": 10.0},
        "shape": {
            "valid": True,
            "kind": "rounded-rectangle",
            "rounded": True,
            "capsule": False,
            "radius_clamped": False,
            "surface_area": 240.0 * 96.0,
            "min_extent": 96.0,
            "max_extent": 240.0,
            "radius_limit": 48.0,
            "effective_radius": 10.0,
            "normalized_radius": 10.0 / 48.0,
        },
        "render_target": {
            "width": 320,
            "height": 240,
            "scale": 1.0,
            "pixel_format": "rgba8unorm",
            "pixel_count": 320 * 240,
            "ready": True,
            "within_backdrop_budget": True,
        },
        "capability_snapshot": {
            "material_surfaces": True,
            "material_backdrop_blur": False,
            "shader_blur": False,
            "frame_history": False,
            "reduce_transparency": False,
            "increase_contrast": False,
            "reduce_motion": False,
            "max_shader_sample_taps": 25,
            "max_texture_dimension_2d": 0,
            "max_backdrop_pixels": 0,
            "texture_limits_known": False,
            "backdrop_budget_known": False,
            "target_within_texture_limits": True,
            "target_within_backdrop_budget": True,
            "profile": "generic",
            "source": "default",
        },
        "decision_trace": {
            "has_geometry": True,
            "has_material": True,
            "role_allows_liquid_glass": True,
            "content_layer_standard_material": False,
            "liquid_glass_backdrop_candidate": True,
            "target_ready": True,
            "quality_switches_allow_backdrop": True,
            "backdrop_pixels_within_budget": True,
            "quality_allows_backdrop": True,
            "capability_material_surfaces": True,
            "capability_material_backdrop_blur": False,
            "capability_shader_blur": False,
            "capability_frame_history": False,
            "capability_texture_limits_known": False,
            "capability_backdrop_budget_known": False,
            "capability_target_within_texture_limits": True,
            "capability_target_within_backdrop_budget": True,
            "backend_supports_backdrop": False,
            "backdrop_available": False,
            "backdrop_stable": False,
            "backdrop_source_ready": False,
            "next_frame_capture_required": False,
            "reduced_transparency": False,
            "increase_contrast": False,
            "reduce_motion": False,
            "can_sample_backdrop": False,
            "first_blocker": "unsupported-backend",
        },
        "opacity": 0.58,
        "blur_radius": 0.0,
        "tint": {"r": 255, "g": 255, "b": 255, "a": 148},
        "saturation": 1.0,
        "luminance_floor": 0.08,
        "luminance_gain": 1.08,
        "luminance_curve": {
            "name": "fallback-flat",
            "floor": 0.08,
            "gain": 1.08,
            "gamma": 1.0,
            "midpoint": 0.5,
            "contrast": 1.0,
            "edge_lift": 0.34,
            "backdrop_driven": False,
            "bounded": True,
        },
        "edge_highlight": 0.34,
        "edge_width": 1.0,
        "noise_opacity": 0.0,
        "shadow_alpha": 0.14,
        "shadow_radius": 14.0,
        "backdrop_sampling": False,
        "backdrop": {
            "available": False,
            "stable": False,
            "excludes_foreground_text": False,
            "color_mean": {"r": 255, "g": 255, "b": 255, "a": 255},
            "color_sample_count": 0,
            "color_sample_status": "not-sampled",
            "luma_min": 0.0,
            "luma_max": 1.0,
            "luma_mean": 0.5,
            "luma_span": 1.0,
            "luma_sample_count": 0,
            "luma_sample_grid_width": 0,
            "luma_sample_grid_height": 0,
            "luma_sample_frame": 0,
            "luma_sample_status": "not-sampled",
            "source": "none",
            "luminance_response": "not-sampled",
            "frosting_response": "not-sampled",
            "color_response": "not-sampled",
            "tint_response": "not-sampled",
            "saturation_response": "not-sampled",
            "depth_response": "not-sampled",
            "luminance_floor_delta": 0.0,
            "luminance_gain_delta": 0.0,
            "edge_highlight_delta": 0.0,
            "opacity_delta": 0.0,
            "tint_color_delta": 0.0,
            "tint_alpha_delta": 0.0,
            "saturation_delta": 0.0,
            "shadow_alpha_delta": 0.0,
            "shadow_radius_delta": 0.0,
            "response_strength": 0.0,
        },
        "backdrop_access": {
            "required": False,
            "stable_required": False,
            "frame_history_required": False,
            "shared_frame_capture": False,
            "next_frame_capture_required": False,
            "excludes_foreground_text": False,
            "source": "none",
            "capture_scope": "none",
            "capture_reason": "not-required",
            "max_frame_capture_count": 0,
            "max_frame_capture_pixels": 0,
            "max_surface_sample_pixels": 0,
            "bounded": True,
        },
        "theme": {
            "source": "material-style",
            "profile_name": "apple-glass-light",
            "token_policy": "explicit-material-style-tokens",
            "foreground": {"r": 28, "g": 28, "b": 30, "a": 255},
            "secondary_foreground": {"r": 99, "g": 99, "b": 102, "a": 255},
            "accent_foreground": {"r": 0, "g": 122, "b": 255, "a": 255},
            "strong_accent_foreground": {"r": 0, "g": 90, "b": 190, "a": 255},
            "tint": {"r": 255, "g": 255, "b": 255, "a": 148},
            "border": {"r": 209, "g": 209, "b": 214, "a": 190},
            "foreground_matches_theme": True,
            "accent_matches_theme": True,
            "tint_matches_surface": True,
            "border_matches_theme": True,
            "default_glass_tokens": True,
        },
        "foreground": {
            "primary": {"r": 17, "g": 24, "b": 39, "a": 255},
            "secondary": {"r": 71, "g": 85, "b": 105, "a": 255},
            "accent": {"r": 8, "g": 128, "b": 124, "a": 255},
            "scheme": "solid-fallback",
            "source": "fallback-material",
            "contrast_policy": "standard-contrast",
            "remap_policy": "theme-role-remap-if-needed",
            "background_luma": 0.72,
            "primary_contrast_ratio": 12.0,
            "secondary_contrast_ratio": 6.0,
            "accent_contrast_ratio": 4.5,
            "minimum_contrast_ratio": 4.5,
            "primary_contrast_margin": 7.5,
            "secondary_contrast_margin": 1.5,
            "accent_contrast_margin": 0.0,
            "backdrop_driven": False,
            "high_contrast": False,
            "uses_vibrancy": False,
            "deterministic": True,
        },
        "refraction": {
            "model": "none",
            "source": "none",
            "active": False,
            "backdrop_driven": False,
            "interaction_driven": False,
            "reduced_motion_suppressed": False,
            "bounded": True,
            "strength": 0.0,
            "edge_bias": 0.0,
            "max_offset_pixels": 0.0,
        },
        "interaction": {
            "enabled": False,
            "active": False,
            "hovered": False,
            "pressed": False,
            "focused": False,
            "pointer_inside": False,
            "reduce_motion": False,
            "pointer_x": 0.5,
            "pointer_y": 0.5,
            "response_strength": 0.0,
            "opacity_delta": 0.0,
            "blur_radius_delta": 0.0,
            "saturation_delta": 0.0,
            "edge_highlight_delta": 0.0,
            "shadow_alpha_delta": 0.0,
            "shadow_radius_delta": 0.0,
            "state": "inactive",
            "enablement_reason": "noninteractive-container",
            "response_model": "none",
            "motion_policy": "static",
            "specular_model": "none",
            "specular_highlight_active": False,
            "specular_anchor_x": 0.5,
            "specular_anchor_y": 0.5,
            "specular_radius": 0.0,
            "specular_intensity": 0.0,
            "deterministic": True,
        },
        "fallback": True,
        "fallback_path": "unsupported-backend",
        "fallback_reason": "backend reports no material backdrop blur support",
        "debug_seed": 4919,
        "sample_taps": sample_taps,
        "sampling_kernel": {
            "name": "none",
            "radius": 0,
            "sample_taps": sample_taps,
            "blur_step_scale": 0.0,
            "weight_profile": "none",
            "requires_backdrop": False,
            "bounded": True,
        },
        "quality_policy": {
            "allow_backdrop_sampling": True,
            "allow_noise": True,
            "allow_shadow": True,
            "max_blur_radius": 36.0,
            "max_sample_taps": 25,
            "max_backdrop_pixels": 4_000_000,
        },
        "primary_pass": primary,
        "resource_budget": {
            "max_blur_radius": 36.0,
            "max_sample_taps": 25,
            "max_sampling_kernel_radius": 0,
            "max_pass_count": 1,
            "max_execution_stages": 4,
            "max_paint_layers": 3,
            "max_backdrop_pixels": 320 * 240,
            "max_frame_capture_count": 0,
            "max_frame_capture_pixels": 0,
            "max_surface_sample_pixels": 0,
            "max_refraction_offset_pixels": 0.0,
            "max_container_spacing": 0.0,
            "max_paint_layer_inflate": 7.0,
            "bounded_texture_copy": True,
            "deterministic_fallback": True,
        },
        "execution_stage_capacity": 4,
        "dropped_execution_stage_count": 0,
        "paint_layer_capacity": 3,
        "dropped_paint_layer_count": 0,
        "verifier": {
            "require_backdrop_source": False,
            "require_edge_highlight": False,
            "require_container_identity": False,
            "require_container_morph_contract": False,
            "min_luma_delta": 3.0,
            "min_unique_colors": 2,
            "region_name": "regular-legibility-backdrop",
            "likely_layer": "material-fallback-pass",
            "likely_pass": "translucent-rounded-rect",
        },
        "passes": [primary],
        "execution_stages": [
            {
                "name": "shape-shadow",
                "active": True,
                "requires_backdrop": False,
                "sample_taps": 0,
                "likely_layer": "material-shadow-pass",
                "executor": "shape-shadow",
                "max_texture_copy_pixels": 0,
                "bounded": True,
                "optics": stage_optics(
                    "shape-shadow",
                    edge_width=1.0,
                    shadow_alpha=0.14,
                    shadow_radius=14.0,
                ),
            },
            {
                "name": "translucent-rounded-rect",
                "active": True,
                "requires_backdrop": False,
                "sample_taps": 0,
                "likely_layer": "material-fallback-pass",
                "executor": "fallback-fill",
                "max_texture_copy_pixels": 0,
                "bounded": True,
                "optics": stage_optics(
                    "fallback-fill",
                    opacity=0.58,
                    blur_radius=0.0,
                    tint_alpha=148 / 255,
                    saturation=1.0,
                    luminance_floor=0.08,
                    luminance_gain=1.08,
                ),
            },
            {
                "name": "edge-highlight",
                "active": True,
                "requires_backdrop": False,
                "sample_taps": 0,
                "likely_layer": "material-fallback-pass",
                "executor": "edge-highlight",
                "max_texture_copy_pixels": 0,
                "bounded": True,
                "optics": stage_optics(
                    "edge-highlight",
                    edge_highlight=0.34,
                    edge_width=1.0,
                ),
            },
        ],
        "paint_layers": material_paint_layers(str(primary["name"])),
    }
    refresh_observation_contract(plan)
    refresh_execution_audit(plan)
    return plan


def reference_accessibility_response(plan: dict[str, object]) -> str:
    decision_trace = plan["decision_trace"]
    assert isinstance(decision_trace, dict)
    flags = {
        "reduced_transparency": bool(decision_trace["reduced_transparency"]),
        "increase_contrast": bool(decision_trace["increase_contrast"]),
        "reduce_motion": bool(decision_trace["reduce_motion"]),
    }
    active = [name for name, enabled in flags.items() if enabled]
    if len(active) > 1:
        return "combined-accessibility"
    if not active:
        return "standard"
    return {
        "reduced_transparency": "reduced-transparency",
        "increase_contrast": "increased-contrast",
        "reduce_motion": "reduced-motion",
    }[active[0]]


def reference_performance_response(plan: dict[str, object]) -> str:
    if str(plan["kind"]) == "none":
        return "inactive"
    backdrop_access = plan["backdrop_access"]
    assert isinstance(backdrop_access, dict)
    if (bool(backdrop_access["next_frame_capture_required"])
            and not bool(plan["backdrop_sampling"])
            and str(plan["fallback_path"]) == "no-backdrop-source"):
        return "warmup-capture"
    if bool(plan["fallback"]):
        return "deterministic-fallback"
    resource_budget = plan["resource_budget"]
    quality_policy = plan["quality_policy"]
    assert isinstance(resource_budget, dict)
    assert isinstance(quality_policy, dict)
    budgeted = (
        float(resource_budget["max_blur_radius"]) < 36.0
        or int(resource_budget["max_sample_taps"]) < 25
        or not bool(quality_policy["allow_noise"])
        or not bool(quality_policy["allow_shadow"]))
    return "budgeted-effects" if budgeted else "standard"


def refresh_reference_model(plan: dict[str, object]) -> None:
    kind = str(plan["kind"])
    role = str(plan["role"])
    shape = plan["shape"]
    tint = plan["tint"]
    container = plan["container"]
    foreground = plan["foreground"]
    refraction = plan["refraction"]
    interaction = plan["interaction"]
    budget = plan["resource_budget"]
    assert isinstance(shape, dict)
    assert isinstance(tint, dict)
    assert isinstance(container, dict)
    assert isinstance(foreground, dict)
    assert isinstance(refraction, dict)
    assert isinstance(interaction, dict)
    assert isinstance(budget, dict)
    shape_valid = bool(shape["valid"])
    shape_kind = str(shape.get("kind", "invalid"))
    content_standard = kind != "none" and role == "content"
    blending_scope = "none"
    if kind != "none" and bool(plan["backdrop_sampling"]):
        blending_scope = "sampled-backdrop"
    elif kind != "none" and bool(plan["fallback"]):
        blending_scope = "deterministic-fallback"
    elif content_standard:
        blending_scope = "standard-fill"
    layer = "inactive"
    material_policy = "inactive"
    if kind != "none":
        layer = "content-layer" if content_standard else "functional-layer"
        material_policy = (
            "standard-material-content-layer"
            if content_standard
            else "liquid-glass-functional-layer")
    minimum = float(foreground["minimum_contrast_ratio"])
    legibility_preserved = (
        float(foreground["primary_contrast_ratio"]) >= minimum
        and float(foreground["secondary_contrast_ratio"]) >= minimum
        and float(foreground["accent_contrast_ratio"]) >= minimum)
    plan["reference_model"] = {
        "technology": "standard-material" if content_standard else "liquid-glass",
        "layer": layer,
        "material_policy": material_policy,
        "variant": kind,
        "shape": (
            shape_kind
            if shape.get("kind") is not None
            else "invalid"
            if not shape_valid
            else "capsule" if bool(shape.get("capsule", False))
            else "rounded-rectangle" if bool(shape["rounded"]) else "rectangle"),
        "shape_scope": "view-bounds",
        "blending_scope": blending_scope,
        "semantic_thickness": kind,
        "accessibility_response": reference_accessibility_response(plan),
        "performance_response": reference_performance_response(plan),
        "view_bounds_anchored": kind != "none" and shape_valid,
        "shape_matches_geometry": kind != "none" and shape_valid,
        "tint_applied": kind != "none" and int(tint["a"]) > 0,
        "interactive_response": bool(interaction["enabled"]),
        "container_grouped": bool(container["participates"]),
        "container_union": bool(container["shape_union_expected"]),
        "container_morphing": bool(container["morph_transitions"]),
        "legibility_preserved": legibility_preserved,
        "vibrancy_expected": bool(foreground["uses_vibrancy"]),
        "deterministic_degradation": bool(budget["deterministic_fallback"]),
    }


def refresh_optical_response(plan: dict[str, object]) -> None:
    kind = str(plan["kind"])
    role = str(plan["role"])
    primary = plan["primary_pass"]
    tint = plan["tint"]
    curve = plan["luminance_curve"]
    foreground = plan["foreground"]
    refraction = plan["refraction"]
    interaction = plan["interaction"]
    budget = plan["resource_budget"]
    reference = plan["reference_model"]
    backdrop_access = plan["backdrop_access"]
    sampling_kernel = plan["sampling_kernel"]
    assert isinstance(primary, dict)
    assert isinstance(tint, dict)
    assert isinstance(curve, dict)
    assert isinstance(foreground, dict)
    assert isinstance(refraction, dict)
    assert isinstance(interaction, dict)
    assert isinstance(budget, dict)
    assert isinstance(reference, dict)
    assert isinstance(backdrop_access, dict)
    assert isinstance(sampling_kernel, dict)
    backdrop_sampling = bool(plan["backdrop_sampling"])
    fallback = bool(plan["fallback"])
    if kind == "none":
        response_model = "inactive"
    elif backdrop_sampling:
        response_model = "sampled-backdrop"
    elif role == "content":
        response_model = "standard-content"
    else:
        response_model = "deterministic-fallback"

    blur_strategy = "none"
    if bool(primary["active"]):
        if bool(primary["requires_backdrop"]):
            blur_strategy = "backdrop-sample-blur"
        elif primary["executor"] == "standard-fill":
            blur_strategy = "standard-fill"
        elif primary["executor"] == "fallback-fill":
            blur_strategy = "fallback-fill"

    if kind == "none" or int(tint["a"]) <= 0:
        color_strategy = "none"
    elif backdrop_sampling:
        color_strategy = "adaptive-backdrop-color"
    elif role == "content":
        color_strategy = "standard-content-color"
    else:
        color_strategy = "fallback-solid-color"

    edge = float(plan["edge_highlight"]) > 0.0
    shadow = float(plan["shadow_alpha"]) > 0.0
    noise = backdrop_sampling and float(plan["noise_opacity"]) > 0.0
    if not bool(primary["active"]):
        depth_strategy = "none"
    elif backdrop_sampling and shadow and edge and noise:
        depth_strategy = "layered-shadow-edge-noise"
    elif backdrop_sampling and (shadow or edge):
        depth_strategy = "layered-shadow-edge"
    elif role == "content" and edge:
        depth_strategy = "standard-content-edge"
    elif (fallback or not backdrop_sampling) and shadow and edge:
        depth_strategy = "fallback-shadow-edge"
    elif (fallback or not backdrop_sampling) and edge:
        depth_strategy = "fallback-edge"
    else:
        depth_strategy = "none"

    if bool(primary["active"]):
        stages = []
        if shadow:
            stages.append("shadow")
        stages.append("primary")
        if edge:
            stages.append("edge")
        if noise:
            stages.append("noise")
        stage_order = "-".join(stages)
    else:
        stage_order = "none"

    captures_backdrop = (
        bool(backdrop_access["shared_frame_capture"])
        or bool(backdrop_access["next_frame_capture_required"])
        or bool(backdrop_access["required"]))
    if not captures_backdrop:
        backdrop_capture_policy = "no-capture"
        foreground_sampling_policy = "not-applicable"
    elif backdrop_sampling:
        backdrop_capture_policy = "sample-current-frame"
        foreground_sampling_policy = (
            "foreground-excluded-from-sample"
            if bool(backdrop_access["excludes_foreground_text"])
            else "foreground-inclusion-risk")
    else:
        backdrop_capture_policy = (
            "warmup-next-frame"
            if bool(backdrop_access["next_frame_capture_required"])
            else "shared-frame-capture")
        foreground_sampling_policy = (
            "foreground-excluded-from-warmup"
            if bool(backdrop_access["excludes_foreground_text"])
            else "foreground-inclusion-risk")

    if kind == "none":
        frosting_source = "none"
    elif backdrop_sampling:
        frosting_source = "sampled-backdrop-frosting"
    elif fallback:
        frosting_source = "solid-fallback-frosting"
    elif role == "content":
        frosting_source = "standard-material-fill"
    else:
        frosting_source = "none"

    if kind == "none" or int(tint["a"]) <= 0:
        tint_source = "none"
    elif backdrop_sampling:
        tint_source = "adaptive-backdrop-tint"
    else:
        tint_source = "style-tint"

    plan["optical_composition"] = {
        "schema_version": verifier.MATERIAL_PLAN_CONTRACT_VERSION,
        "model": response_model,
        "blur_source": blur_strategy,
        "frosting_source": frosting_source,
        "tint_source": tint_source,
        "luminance_source": curve["name"],
        "depth_source": depth_strategy,
        "refraction_source": (
            refraction["source"] if bool(refraction["active"]) else "none"),
        "interaction_source": (
            interaction["response_model"]
            if bool(interaction["active"]) else "none"),
        "fallback_source": plan["fallback_path"] if fallback else "none",
        "stage_order": stage_order,
        "backdrop_capture_policy": backdrop_capture_policy,
        "foreground_sampling_policy": foreground_sampling_policy,
        "backdrop_sampled": backdrop_sampling,
        "blur_required": bool(primary["requires_backdrop"]),
        "frosting_required": backdrop_sampling,
        "tint_required": kind != "none" and int(tint["a"]) > 0,
        "saturation_required": (
            backdrop_sampling and abs(float(plan["saturation"]) - 1.0) > 0.0001),
        "luminance_required": (
            kind != "none"
            and (bool(curve["bounded"])
                 or float(foreground["primary_contrast_ratio"])
                 >= float(foreground["minimum_contrast_ratio"]))),
        "edge_required": bool(primary["active"]) and edge,
        "shadow_required": bool(primary["active"]) and shadow,
        "noise_required": noise,
        "refraction_required": bool(refraction["active"]),
        "interaction_required": bool(interaction["active"]),
        "fallback_required": fallback,
        "backdrop_capture_required": captures_backdrop,
        "foreground_excluded_from_backdrop": bool(
            backdrop_access["excludes_foreground_text"]),
        "stage_order_stable": True,
        "bounded": (
            bool(budget["bounded_texture_copy"])
            and bool(sampling_kernel["bounded"])
            and bool(curve["bounded"])
            and bool(backdrop_access["bounded"])
            and bool(refraction["bounded"])),
        "deterministic": (
            bool(budget["deterministic_fallback"])
            and bool(foreground["deterministic"])
            and bool(interaction["deterministic"])),
        "opacity": plan["opacity"],
        "blur_radius": plan["blur_radius"],
        "tint_alpha": int(tint["a"]) / 255,
        "saturation": plan["saturation"],
        "luminance_floor": plan["luminance_floor"],
        "luminance_gain": plan["luminance_gain"],
        "edge_highlight": plan["edge_highlight"],
        "edge_width": plan["edge_width"],
        "noise_opacity": plan["noise_opacity"],
        "shadow_alpha": plan["shadow_alpha"],
        "shadow_radius": plan["shadow_radius"],
        "refraction_strength": refraction["strength"],
        "refraction_edge_bias": refraction["edge_bias"],
        "refraction_offset_pixels": refraction["max_offset_pixels"],
        "interaction_response_strength": interaction["response_strength"],
        "sample_taps": plan["sample_taps"],
        "max_texture_copy_pixels": primary["max_texture_copy_pixels"],
        "max_surface_sample_pixels": backdrop_access["max_surface_sample_pixels"],
    }

    plan["optical_response"] = {
        "response_model": response_model,
        "blur_strategy": blur_strategy,
        "color_strategy": color_strategy,
        "depth_strategy": depth_strategy,
        "backdrop_driven": backdrop_sampling,
        "blur_active": bool(primary["requires_backdrop"]),
        "frosting_active": backdrop_sampling,
        "tint_active": kind != "none" and int(tint["a"]) > 0,
        "saturation_active": (
            backdrop_sampling and abs(float(plan["saturation"]) - 1.0) > 0.0001),
        "luminance_preservation_active": (
            kind != "none"
            and (bool(curve["bounded"])
                 or bool(reference["legibility_preserved"]))),
        "edge_highlight_active": bool(primary["active"]) and edge,
        "depth_shadow_active": bool(primary["active"]) and shadow,
        "noise_dither_active": noise,
        "refraction_active": bool(refraction["active"]),
        "foreground_vibrancy_active": bool(foreground["uses_vibrancy"]),
        "interaction_active": bool(interaction["active"]),
        "interaction_modulates_optics": (
            bool(interaction["active"])
            and any(
                abs(float(interaction[key])) > 0.0001
                for key in (
                    "opacity_delta",
                    "blur_radius_delta",
                    "saturation_delta",
                    "edge_highlight_delta",
                    "shadow_alpha_delta",
                    "shadow_radius_delta"))),
        "deterministic_fallback": bool(budget["deterministic_fallback"]),
    }


def refresh_observation_contract(plan: dict[str, object]) -> None:
    refresh_reference_model(plan)
    refresh_optical_response(plan)
    primary = plan["primary_pass"]
    stages = plan["execution_stages"]
    paint_layers = plan["paint_layers"]
    budget = plan["resource_budget"]
    backdrop_access = plan["backdrop_access"]
    verifier_contract = plan["verifier"]
    assert isinstance(primary, dict)
    assert isinstance(stages, list)
    assert isinstance(paint_layers, list)
    assert isinstance(budget, dict)
    assert isinstance(backdrop_access, dict)
    assert isinstance(verifier_contract, dict)
    plan["observation_contract"] = {
        "schema_version": plan["contract_version"],
        "semantic_material_required": plan["kind"] != "none",
        "runtime_plan_required": plan["kind"] != "none",
        "fallback_expected": plan["fallback"],
        "backdrop_sampling_expected": plan["backdrop_sampling"],
        "stable_backdrop_required": plan["backdrop_sampling"],
        "shared_frame_capture_required": backdrop_access["shared_frame_capture"],
        "next_frame_capture_required": (
            backdrop_access["next_frame_capture_required"]
        ),
        "backdrop_capture_excludes_foreground_text": (
            backdrop_access["excludes_foreground_text"]
        ),
        "bounded_texture_copy_required": budget["bounded_texture_copy"],
        "deterministic_fallback_required": budget["deterministic_fallback"],
        "backdrop_capture_scope": backdrop_access["capture_scope"],
        "backdrop_capture_reason": backdrop_access["capture_reason"],
        "fallback_path": plan["fallback_path"],
        "fallback_reason": plan["fallback_reason"],
        "primary_pass": primary["name"],
        "primary_executor": primary["executor"],
        "expected_stage_order": plan["optical_composition"]["stage_order"],
        "expected_runtime_passes": len(plan["passes"]),
        "expected_active_runtime_passes": sum(
            1 for pass_plan in plan["passes"]
            if isinstance(pass_plan, dict) and pass_plan["active"]),
        "expected_backdrop_runtime_passes": sum(
            1 for pass_plan in plan["passes"]
            if isinstance(pass_plan, dict) and pass_plan["requires_backdrop"]),
        "expected_execution_stages": len(stages),
        "expected_active_execution_stages": sum(
            1 for stage in stages
            if isinstance(stage, dict) and stage["active"]),
        "expected_backdrop_execution_stages": sum(
            1 for stage in stages
            if isinstance(stage, dict) and stage["requires_backdrop"]),
        "expected_paint_layers": len(paint_layers),
        "expected_active_paint_layers": sum(
            1 for layer in paint_layers
            if isinstance(layer, dict) and layer["active"]),
        "expected_shadow_paint_layers": sum(
            1 for layer in paint_layers
            if isinstance(layer, dict) and layer["executor"] == "rounded-shadow"),
        "expected_fill_paint_layers": sum(
            1 for layer in paint_layers
            if isinstance(layer, dict) and layer["executor"] == "rounded-fill"),
        "expected_edge_paint_layers": sum(
            1 for layer in paint_layers
            if isinstance(layer, dict) and layer["executor"] == "rounded-edge"),
        "max_frame_capture_count": backdrop_access["max_frame_capture_count"],
        "max_frame_capture_pixels": backdrop_access["max_frame_capture_pixels"],
        "max_surface_sample_pixels": backdrop_access["max_surface_sample_pixels"],
        "max_texture_copy_pixels": primary["max_texture_copy_pixels"],
        "region_name": verifier_contract["region_name"],
        "likely_layer": verifier_contract["likely_layer"],
        "likely_pass": verifier_contract["likely_pass"],
    }


def refresh_execution_audit(plan: dict[str, object]) -> None:
    observation = plan["observation_contract"]
    passes = plan["passes"]
    stages = plan["execution_stages"]
    paint_layers = plan["paint_layers"]
    budget = plan["resource_budget"]
    assert isinstance(observation, dict)
    assert isinstance(passes, list)
    assert isinstance(stages, list)
    assert isinstance(paint_layers, list)
    assert isinstance(budget, dict)
    actuals: dict[str, int] = {
        "actual_runtime_passes": len(passes),
        "actual_active_runtime_passes": sum(
            1 for pass_plan in passes
            if isinstance(pass_plan, dict) and pass_plan["active"]),
        "actual_backdrop_runtime_passes": sum(
            1 for pass_plan in passes
            if isinstance(pass_plan, dict) and pass_plan["requires_backdrop"]),
        "actual_execution_stages": len(stages),
        "actual_active_execution_stages": sum(
            1 for stage in stages
            if isinstance(stage, dict) and stage["active"]),
        "actual_backdrop_execution_stages": sum(
            1 for stage in stages
            if isinstance(stage, dict) and stage["requires_backdrop"]),
        "actual_paint_layers": len(paint_layers),
        "actual_active_paint_layers": sum(
            1 for layer in paint_layers
            if isinstance(layer, dict) and layer["active"]),
        "actual_shadow_paint_layers": sum(
            1 for layer in paint_layers
            if isinstance(layer, dict) and layer["executor"] == "rounded-shadow"),
        "actual_fill_paint_layers": sum(
            1 for layer in paint_layers
            if isinstance(layer, dict) and layer["executor"] == "rounded-fill"),
        "actual_edge_paint_layers": sum(
            1 for layer in paint_layers
            if isinstance(layer, dict) and layer["executor"] == "rounded-edge"),
    }
    actual_stage_order = verifier.actual_execution_stage_order(
        stages,
        plan["primary_pass"] if isinstance(plan["primary_pass"], dict) else None)
    match_pairs = (
        ("runtime_passes_match", "expected_runtime_passes", "actual_runtime_passes", "runtime-pass-count"),
        ("active_runtime_passes_match", "expected_active_runtime_passes", "actual_active_runtime_passes", "active-runtime-pass-count"),
        ("backdrop_runtime_passes_match", "expected_backdrop_runtime_passes", "actual_backdrop_runtime_passes", "backdrop-runtime-pass-count"),
        ("execution_stages_match", "expected_execution_stages", "actual_execution_stages", "execution-stage-count"),
        ("active_execution_stages_match", "expected_active_execution_stages", "actual_active_execution_stages", "active-execution-stage-count"),
        ("backdrop_execution_stages_match", "expected_backdrop_execution_stages", "actual_backdrop_execution_stages", "backdrop-execution-stage-count"),
        ("paint_layers_match", "expected_paint_layers", "actual_paint_layers", "paint-layer-count"),
        ("active_paint_layers_match", "expected_active_paint_layers", "actual_active_paint_layers", "active-paint-layer-count"),
        ("shadow_paint_layers_match", "expected_shadow_paint_layers", "actual_shadow_paint_layers", "shadow-paint-layer-count"),
        ("fill_paint_layers_match", "expected_fill_paint_layers", "actual_fill_paint_layers", "fill-paint-layer-count"),
        ("edge_paint_layers_match", "expected_edge_paint_layers", "actual_edge_paint_layers", "edge-paint-layer-count"),
    )
    audit: dict[str, object] = {
        "schema_version": plan["contract_version"],
        **actuals,
        "bounded_texture_copy": budget["bounded_texture_copy"],
        "deterministic_fallback": budget["deterministic_fallback"],
        "expected_stage_order": observation["expected_stage_order"],
        "actual_stage_order": actual_stage_order,
        "likely_layer": observation["likely_layer"],
        "likely_pass": observation["likely_pass"],
    }
    mismatch_count = 0
    first_mismatch = "none"
    for match_key, expected_key, actual_key, mismatch_name in match_pairs:
        matched = observation[expected_key] == audit[actual_key]
        audit[match_key] = matched
        if not matched:
            mismatch_count += 1
            if first_mismatch == "none":
                first_mismatch = mismatch_name
    stage_order_matched = (
        observation["expected_stage_order"] == audit["actual_stage_order"])
    audit["stage_order_match"] = stage_order_matched
    if not stage_order_matched:
        mismatch_count += 1
        if first_mismatch == "none":
            first_mismatch = "stage-order"
    for audit_key, observation_key, mismatch_name in (
            ("bounded_texture_copy", "bounded_texture_copy_required", "bounded-texture-copy"),
            ("deterministic_fallback", "deterministic_fallback_required", "deterministic-fallback")):
        if observation[observation_key] is True and audit[audit_key] is not True:
            mismatch_count += 1
            if first_mismatch == "none":
                first_mismatch = mismatch_name
    audit["contract_satisfied"] = mismatch_count == 0
    audit["mismatch_count"] = mismatch_count
    audit["first_mismatch"] = first_mismatch
    plan["execution_audit"] = audit


def sampled_material_plan(sample_taps: int = 25) -> dict[str, object]:
    plan = material_plan(sample_taps=sample_taps, primary_sample_taps=sample_taps)
    assert isinstance(plan["decision_trace"], dict)
    assert isinstance(plan["capability_snapshot"], dict)
    assert isinstance(plan["backdrop"], dict)
    assert isinstance(plan["backdrop_access"], dict)
    assert isinstance(plan["primary_pass"], dict)
    assert isinstance(plan["sampling_kernel"], dict)
    assert isinstance(plan["luminance_curve"], dict)
    assert isinstance(plan["foreground"], dict)
    assert isinstance(plan["refraction"], dict)
    plan["plan_id"] = "material.regular.liquid-glass"
    plan["backdrop_sampling"] = True
    plan["fallback"] = False
    plan["fallback_path"] = "none"
    plan["fallback_reason"] = ""
    plan["blur_radius"] = 22.0
    plan["saturation"] = 1.08
    plan["noise_opacity"] = 0.014
    plan["decision_trace"].update({
        "capability_material_backdrop_blur": True,
        "capability_shader_blur": True,
        "capability_frame_history": True,
        "backend_supports_backdrop": True,
        "backdrop_available": True,
        "backdrop_stable": True,
        "backdrop_source_ready": True,
        "next_frame_capture_required": True,
        "can_sample_backdrop": True,
        "first_blocker": "none",
    })
    plan["capability_snapshot"].update({
        "material_backdrop_blur": True,
        "shader_blur": True,
        "frame_history": True,
    })
    plan["backdrop"].update({
        "available": True,
        "stable": True,
        "excludes_foreground_text": True,
        "color_mean": {"r": 255, "g": 255, "b": 255, "a": 255},
        "color_sample_count": sample_taps,
        "color_sample_status": "sampled-async-grid",
        "source": "previous-presented-frame",
        "luminance_response": "neutral",
        "frosting_response": "balanced",
        "color_response": "preserve",
        "tint_response": "preserve",
        "saturation_response": "preserve",
        "depth_response": "standard",
    })
    plan["backdrop_access"].update({
        "required": True,
        "stable_required": True,
        "frame_history_required": True,
        "shared_frame_capture": True,
        "next_frame_capture_required": True,
        "excludes_foreground_text": True,
        "source": "previous-presented-frame",
        "capture_scope": "shared-frame",
        "capture_reason": "sample-current-frame",
        "max_frame_capture_count": 1,
        "max_frame_capture_pixels": 320 * 240,
        "max_surface_sample_pixels": 240 * 96,
        "bounded": True,
    })
    plan["primary_pass"].update({
        "name": "backdrop-sample-blur",
        "active": True,
        "requires_backdrop": True,
        "sample_taps": sample_taps,
        "likely_layer": "material-blur-pass",
        "executor": "backdrop-filter",
        "max_texture_copy_pixels": 320 * 240,
    })
    kernel_contract = sampling_kernel_contract(sample_taps)
    plan["sampling_kernel"].update({
        "name": kernel_contract["name"],
        "radius": kernel_contract["radius"],
        "sample_taps": sample_taps,
        "blur_step_scale": kernel_contract["blur_step_scale"],
        "weight_profile": kernel_contract["weight_profile"],
        "requires_backdrop": True,
        "bounded": True,
    })
    plan["luminance_curve"].update({
        "name": "adaptive-backdrop-luma",
        "backdrop_driven": True,
    })
    plan["foreground"].update({
        "scheme": "vibrant-balanced",
        "source": "backdrop-analysis",
        "backdrop_driven": True,
        "uses_vibrancy": True,
    })
    plan["refraction"].update({
        "model": "edge-lens",
        "source": "sampled-backdrop-edge-refraction",
        "active": True,
        "backdrop_driven": True,
        "interaction_driven": False,
        "reduced_motion_suppressed": False,
        "bounded": True,
        "strength": 0.124167,
        "edge_bias": 0.378333,
        "max_offset_pixels": 0.572617,
    })
    assert isinstance(plan["resource_budget"], dict)
    plan["resource_budget"]["max_sampling_kernel_radius"] = (
        kernel_contract["radius"]
    )
    plan["resource_budget"]["max_frame_capture_count"] = 1
    plan["resource_budget"]["max_frame_capture_pixels"] = 320 * 240
    plan["resource_budget"]["max_surface_sample_pixels"] = 240 * 96
    plan["resource_budget"]["max_refraction_offset_pixels"] = 0.572617
    plan["resource_budget"]["max_paint_layer_inflate"] = 0.0
    plan["passes"] = [plan["primary_pass"]]
    plan["execution_stages"] = [
        {
            "name": "shape-shadow",
            "active": True,
            "requires_backdrop": False,
            "sample_taps": 0,
            "likely_layer": "material-shadow-pass",
            "executor": "shape-shadow",
            "max_texture_copy_pixels": 0,
            "bounded": True,
            "optics": stage_optics(
                "shape-shadow",
                edge_width=1.0,
                shadow_alpha=0.14,
                shadow_radius=14.0,
            ),
        },
        {
            "name": "backdrop-sample-blur",
            "active": True,
            "requires_backdrop": True,
            "sample_taps": sample_taps,
            "likely_layer": "material-blur-pass",
            "executor": "backdrop-filter",
            "max_texture_copy_pixels": 320 * 240,
            "bounded": True,
            "optics": stage_optics(
                "backdrop-filter",
                opacity=0.58,
                blur_radius=22.0,
                tint_alpha=148 / 255,
                saturation=1.08,
                luminance_floor=0.08,
                luminance_gain=1.08,
                refraction_model="edge-lens",
                refraction_strength=0.124167,
                refraction_edge_bias=0.378333,
                refraction_offset_pixels=0.572617,
            ),
        },
        {
            "name": "edge-highlight",
            "active": True,
            "requires_backdrop": False,
            "sample_taps": 0,
            "likely_layer": "material-edge-pass",
            "executor": "edge-highlight",
            "max_texture_copy_pixels": 0,
            "bounded": True,
            "optics": stage_optics(
                "edge-highlight",
                edge_highlight=0.34,
                edge_width=1.0,
            ),
        },
        {
            "name": "noise-dither",
            "active": True,
            "requires_backdrop": False,
            "sample_taps": 0,
            "likely_layer": "material-noise-pass",
            "executor": "ordered-dither",
            "max_texture_copy_pixels": 0,
            "bounded": True,
            "optics": stage_optics(
                "noise-dither",
                noise_opacity=0.014,
            ),
        },
    ]
    plan["paint_layers"] = []
    assert isinstance(plan["verifier"], dict)
    plan["verifier"].update({
        "require_backdrop_source": True,
        "require_edge_highlight": True,
        "min_luma_delta": 8.0,
        "min_unique_colors": 4,
        "likely_layer": "material-blur-pass",
        "likely_pass": "backdrop-sample-blur",
    })
    refresh_observation_contract(plan)
    refresh_execution_audit(plan)
    return plan


def material_container_group_summary(plan: dict[str, object]) -> dict[str, object]:
    container = plan["container"]
    primary = plan["primary_pass"]
    assert isinstance(container, dict)
    assert isinstance(primary, dict)
    participates = (
        bool(container["participates"])
        and int(container["container_id"]) > 0)
    if not participates:
        return {
            "group_count": 0,
            "multi_surface_group_count": 0,
            "union_group_count": 0,
            "morph_group_count": 0,
            "interactive_group_count": 0,
            "shared_backdrop_scope_group_count": 0,
            "shared_capture_surface_count": 0,
            "shared_capture_saved_surface_count": 0,
            "max_shared_capture_group_surfaces": 0,
            "shape_blend_execution_group_count": 0,
            "shape_blend_execution_surface_count": 0,
            "fallback_mixed_group_count": 0,
            "max_group_size": 0,
            "max_active_surfaces": 0,
            "max_sampled_backdrop_surfaces": 0,
            "max_fallback_surfaces": 0,
            "total_shape_pair_count": 0,
            "blend_candidate_pair_count": 0,
            "union_candidate_pair_count": 0,
            "morph_candidate_pair_count": 0,
            "separated_pair_count": 0,
            "min_shape_gap": 0.0,
            "max_shape_gap": 0.0,
            "max_blend_distance": 0.0,
            "max_group_bounds_width": 0.0,
            "max_group_bounds_height": 0.0,
            "max_group_bounds_area": 0.0,
            "max_shape_blend_strength": 0.0,
        }
    active = 1 if primary["active"] else 0
    sampled = 1 if plan["backdrop_sampling"] else 0
    fallback = 1 if plan["fallback"] else 0
    shared_scope = 1 if container["shared_backdrop_scope"] else 0
    geometry = plan["geometry"]
    assert isinstance(geometry, dict)
    width = float(geometry["w"])
    height = float(geometry["h"])
    blend_distance = float(container["blend_distance"])
    return {
        "group_count": 1,
        "multi_surface_group_count": 0,
        "union_group_count": 1 if container["shape_union_expected"] else 0,
        "morph_group_count": 1 if container["morph_transitions"] else 0,
        "interactive_group_count": 1 if container["interactive"] else 0,
        "shared_backdrop_scope_group_count": (
            1 if container["shared_backdrop_scope"] else 0),
        "shared_capture_surface_count": shared_scope,
        "shared_capture_saved_surface_count": 0,
        "max_shared_capture_group_surfaces": shared_scope,
        "shape_blend_execution_group_count": 0,
        "shape_blend_execution_surface_count": 0,
        "fallback_mixed_group_count": 0,
        "max_group_size": 1,
        "max_active_surfaces": active,
        "max_sampled_backdrop_surfaces": sampled,
        "max_fallback_surfaces": fallback,
        "total_shape_pair_count": 0,
        "blend_candidate_pair_count": 0,
        "union_candidate_pair_count": 0,
        "morph_candidate_pair_count": 0,
        "separated_pair_count": 0,
        "min_shape_gap": 0.0,
        "max_shape_gap": 0.0,
        "max_blend_distance": blend_distance,
        "max_group_bounds_width": width,
        "max_group_bounds_height": height,
        "max_group_bounds_area": width * height,
        "max_shape_blend_strength": 0.0,
    }


def material_runtime_summary(plan: dict[str, object]) -> dict[str, object]:
    budget = plan["resource_budget"]
    primary = plan["primary_pass"]
    shape = plan["shape"]
    backdrop_access = plan["backdrop_access"]
    execution_stages = plan["execution_stages"]
    paint_layers = plan["paint_layers"]
    interaction = plan["interaction"]
    refraction = plan["refraction"]
    audit = plan["execution_audit"]
    assert isinstance(budget, dict)
    assert isinstance(primary, dict)
    assert isinstance(shape, dict)
    assert isinstance(backdrop_access, dict)
    assert isinstance(execution_stages, list)
    assert isinstance(paint_layers, list)
    assert isinstance(interaction, dict)
    assert isinstance(refraction, dict)
    assert isinstance(audit, dict)
    active_paint_layers = [
        layer for layer in paint_layers
        if isinstance(layer, dict) and layer["active"]
    ]
    shadow_paint_layers = [
        layer for layer in paint_layers
        if isinstance(layer, dict) and layer["executor"] == "rounded-shadow"
    ]
    fill_paint_layers = [
        layer for layer in paint_layers
        if isinstance(layer, dict) and layer["executor"] == "rounded-fill"
    ]
    edge_paint_layers = [
        layer for layer in paint_layers
        if isinstance(layer, dict) and layer["executor"] == "rounded-edge"
    ]
    return {
        "plan_count": 1,
        "fallback_count": 1 if plan["fallback"] else 0,
        "backdrop_sampling_count": 1 if plan["backdrop_sampling"] else 0,
        "total_runtime_passes": 1,
        "active_runtime_passes": 1 if primary["active"] else 0,
        "backdrop_runtime_passes": 1 if primary["requires_backdrop"] else 0,
        "total_execution_stages": len(execution_stages),
        "active_execution_stages": sum(
            1 for stage in execution_stages
            if isinstance(stage, dict) and stage["active"]
        ),
        "backdrop_execution_stages": sum(
            1 for stage in execution_stages
            if isinstance(stage, dict) and stage["requires_backdrop"]
        ),
        "dropped_execution_stages": plan["dropped_execution_stage_count"],
        "max_execution_stage_count": len(execution_stages),
        "max_execution_stages": budget["max_execution_stages"],
        "max_execution_stage_capacity": plan["execution_stage_capacity"],
        "total_paint_layers": len(paint_layers),
        "active_paint_layers": len(active_paint_layers),
        "dropped_paint_layers": plan["dropped_paint_layer_count"],
        "shadow_paint_layers": len(shadow_paint_layers),
        "fill_paint_layers": len(fill_paint_layers),
        "edge_paint_layers": len(edge_paint_layers),
        "max_paint_layer_count": len(paint_layers),
        "max_paint_layers": budget["max_paint_layers"],
        "max_paint_layer_capacity": plan["paint_layer_capacity"],
        "max_paint_layer_inflate": budget["max_paint_layer_inflate"],
        "max_pass_texture_copy_pixels": primary["max_texture_copy_pixels"],
        "total_pass_texture_copy_pixels": primary["max_texture_copy_pixels"],
        "backdrop_access_count": 1 if backdrop_access["required"] else 0,
        "shared_frame_capture_plan_count": (
            1 if backdrop_access["shared_frame_capture"] else 0
        ),
        "next_frame_capture_plan_count": (
            1 if backdrop_access["next_frame_capture_required"] else 0
        ),
        "max_frame_capture_count": budget["max_frame_capture_count"],
        "max_frame_capture_pixels": budget["max_frame_capture_pixels"],
        "total_surface_sample_pixels": budget["max_surface_sample_pixels"],
        "max_surface_sample_pixels": budget["max_surface_sample_pixels"],
        "max_plan_blur_radius": plan["blur_radius"],
        "max_plan_sample_taps": plan["sample_taps"],
        "total_plan_sample_taps": plan["sample_taps"],
        "max_budget_blur_radius": budget["max_blur_radius"],
        "max_sample_taps": budget["max_sample_taps"],
        "max_sampling_kernel_radius": budget["max_sampling_kernel_radius"],
        "max_pass_count": budget["max_pass_count"],
        "max_backdrop_pixels": budget["max_backdrop_pixels"],
        "max_container_spacing": budget["max_container_spacing"],
        "containered_count": (
            1 if plan["container"]["participates"] else 0
        ),
        "unioned_count": (
            1 if plan["container"]["shape_union_expected"] else 0
        ),
        "interactive_count": (
            1 if plan["container"]["interactive"] else 0
        ),
        "morph_transition_count": (
            1 if plan["container"]["morph_transitions"] else 0
        ),
        "valid_shape_count": 1 if shape["valid"] else 0,
        "rounded_shape_count": 1 if shape["rounded"] else 0,
        "capsule_shape_count": 1 if shape.get("capsule") else 0,
        "radius_clamped_count": 1 if shape["radius_clamped"] else 0,
        "foreground_backdrop_driven_count": (
            1 if plan["foreground"]["backdrop_driven"] else 0
        ),
        "foreground_high_contrast_count": (
            1 if plan["foreground"]["high_contrast"] else 0
        ),
        "foreground_vibrant_count": (
            1 if plan["foreground"]["uses_vibrancy"] else 0
        ),
        "interaction_enabled_count": 1 if interaction["enabled"] else 0,
        "interaction_active_count": 1 if interaction["active"] else 0,
        "interaction_reduce_motion_count": (
            1 if interaction["reduce_motion"] else 0),
        "interaction_specular_highlight_count": (
            1 if interaction["specular_highlight_active"] else 0),
        "refraction_active_count": 1 if refraction["active"] else 0,
        "max_refraction_strength": refraction["strength"],
        "max_refraction_edge_bias": refraction["edge_bias"],
        "max_refraction_offset_pixels": refraction["max_offset_pixels"],
        "max_surface_area": shape["surface_area"],
        "max_effective_radius": shape["effective_radius"],
        "max_radius_limit": shape["radius_limit"],
        "max_normalized_radius": shape["normalized_radius"],
        "max_saturation": plan["saturation"],
        "max_edge_highlight": plan["edge_highlight"],
        "max_edge_width": plan["edge_width"],
        "max_noise_opacity": plan["noise_opacity"],
        "max_shadow_alpha": plan["shadow_alpha"],
        "max_shadow_radius": plan["shadow_radius"],
        "max_interaction_response_strength": interaction["response_strength"],
        "max_interaction_specular_radius": interaction["specular_radius"],
        "max_interaction_specular_intensity": interaction["specular_intensity"],
        "min_foreground_contrast_ratio": (
            plan["foreground"]["primary_contrast_ratio"]
        ),
        "unbounded_texture_copy": 0 if budget["bounded_texture_copy"] else 1,
        "non_deterministic_fallback": (
            0 if budget["deterministic_fallback"] else 1
        ),
        "execution_contract_satisfied_count": (
            1 if audit["contract_satisfied"] else 0),
        "execution_contract_mismatch_count": (
            0 if audit["contract_satisfied"] else 1),
        "execution_contract_mismatch_total": audit["mismatch_count"],
        "stage_order_match_count": 1 if audit["stage_order_match"] else 0,
        "stage_order_mismatch_count": 0 if audit["stage_order_match"] else 1,
        "first_execution_contract_mismatch": audit["first_mismatch"],
        "first_stage_order_mismatch": (
            "none" if audit["stage_order_match"]
            else audit["actual_stage_order"]),
        "container_groups": material_container_group_summary(plan),
    }


def material_container_group_details(plan: dict[str, object]) -> list[dict[str, object]]:
    container = plan["container"]
    geometry = plan["geometry"]
    shape = plan["shape"]
    primary = plan["primary_pass"]
    assert isinstance(container, dict)
    assert isinstance(geometry, dict)
    assert isinstance(shape, dict)
    assert isinstance(primary, dict)
    participates = (
        bool(container["participates"])
        and int(container["container_id"]) > 0)
    if not participates:
        return []
    width = float(geometry["w"])
    height = float(geometry["h"])
    summary = material_container_group_summary(plan)
    return [{
        "container_id": int(container["container_id"]),
        "surface_count": 1,
        "active_surfaces": 1 if primary["active"] else 0,
        "sampled_backdrop_surfaces": 1 if plan["backdrop_sampling"] else 0,
        "fallback_surfaces": 1 if plan["fallback"] else 0,
        "union_surfaces": 1 if container["shape_union_expected"] else 0,
        "morph_surfaces": 1 if container["morph_transitions"] else 0,
        "interactive_surfaces": 1 if container["interactive"] else 0,
        "shared_backdrop_scope_surfaces": (
            1 if container["shared_backdrop_scope"] else 0),
        "shared_capture_saved_surfaces": 0,
        "execution_policy": "group-isolated",
        "shape_blend_execution": False,
        "shape_blend_execution_surfaces": 0,
        "shape_blend_strength": 0.0,
        "shape_pair_count": 0,
        "blend_candidate_pair_count": 0,
        "union_candidate_pair_count": 0,
        "morph_candidate_pair_count": 0,
        "separated_pair_count": 0,
        "min_shape_gap": 0.0,
        "max_shape_gap": 0.0,
        "max_blend_distance": summary["max_blend_distance"],
        "bounds": {
            "valid": bool(shape["valid"]),
            "x": float(geometry["x"]),
            "y": float(geometry["y"]),
            "w": width,
            "h": height,
            "area": width * height,
        },
        "likely_layer": "material-container",
        "likely_pass": "container-group-analysis",
        "members": [{
            "command_index": 0,
            "plan_id": plan["plan_id"],
            "kind": plan["kind"],
            "role": plan["role"],
            "fallback_path": plan["fallback_path"],
            "backdrop_sampling": plan["backdrop_sampling"],
            "active_pass": primary["active"],
            "interactive": container["interactive"],
            "union_id": int(container["union_id"]),
            "mode": container["mode"],
            "spacing": container["spacing"],
            "blend_distance": container["blend_distance"],
            "shape_union_expected": container["shape_union_expected"],
            "shape_blending_expected": container["shape_blending_expected"],
            "morph_transitions": container["morph_transitions"],
            "shared_backdrop_scope": container["shared_backdrop_scope"],
            "group_execution_policy": "group-isolated",
            "shape_blend_execution": False,
            "shape_blend_strength": 0.0,
            "reduced_motion_suppressed_morph": (
                container["reduced_motion_suppressed_morph"]),
            "shape_kind": shape["kind"],
            "shape_valid": shape["valid"],
            "geometry": geometry.copy(),
        }],
    }]


def material_executor_summary(plan: dict[str, object]) -> dict[str, object]:
    sample_taps = 0 if plan["fallback"] else int(plan["sample_taps"])
    stages = plan["execution_stages"]
    paint_layers = plan["paint_layers"]
    assert isinstance(stages, list)
    assert isinstance(paint_layers, list)
    active_stages = [
        stage for stage in stages
        if isinstance(stage, dict) and stage["active"]
    ]
    backdrop_stages = [
        stage for stage in stages
        if isinstance(stage, dict) and stage["requires_backdrop"]
    ]
    primary = plan["primary_pass"]
    backdrop_access = plan["backdrop_access"]
    backdrop = plan["backdrop"]
    interaction = plan["interaction"]
    refraction = plan["refraction"]
    audit = plan["execution_audit"]
    assert isinstance(primary, dict)
    assert isinstance(backdrop_access, dict)
    assert isinstance(backdrop, dict)
    assert isinstance(interaction, dict)
    assert isinstance(refraction, dict)
    assert isinstance(audit, dict)
    sampled_backdrop_instances = (
        1 if primary["executor"] == "backdrop-filter" else 0)
    standard_fill_instances = (
        1 if primary["executor"] == "standard-fill" else 0)
    deterministic_fallback_instances = (
        1 if primary["executor"] == "fallback-fill" else 0)
    sampled_required = sampled_backdrop_instances > 0
    pipeline_ready = sampled_required
    backdrop_source_ready = sampled_required
    upload_bytes = 128 if sampled_required else 0
    draw_calls = sampled_backdrop_instances
    sampled_uploaded = upload_bytes > 0
    sampled_drawn = draw_calls > 0
    upload_status = "uploaded" if sampled_required else "not-needed"
    draw_status = "drawn" if sampled_required else "not-needed"
    sampling_kernel = plan["sampling_kernel"]
    render_target = plan["render_target"]
    assert isinstance(sampling_kernel, dict)
    assert isinstance(render_target, dict)
    shader_blur_step_pixels = 0.0
    if sampled_required:
        shader_blur_step_pixels = max(
            1.0,
            float(plan["blur_radius"])
            * float(sampling_kernel["blur_step_scale"])
            * float(render_target["scale"]))
    primary_stages = [
        stage for stage in stages
        if isinstance(stage, dict)
        and all(
            stage.get(key) == primary.get(key)
            for key in verifier.MATERIAL_PASS_FIELDS)
    ]
    active_paint_layers = [
        layer for layer in paint_layers
        if isinstance(layer, dict) and layer["active"]
    ]
    return {
        "plan_count": 1,
        "material_instance_count": 0 if plan["fallback"] else 1,
        "fallback_instance_count": 1 if plan["fallback"] else 0,
        "sampled_backdrop_instance_count": sampled_backdrop_instances,
        "standard_fill_instance_count": standard_fill_instances,
        "deterministic_fallback_instance_count": (
            deterministic_fallback_instances),
        "material_draw_calls": draw_calls,
        "backdrop_copy_count": 0,
        "execution_stage_count": len(stages),
        "active_execution_stage_count": len(active_stages),
        "backdrop_execution_stage_count": len(backdrop_stages),
        "primary_execution_stage_count": len(primary_stages),
        "dropped_execution_stage_count": plan["dropped_execution_stage_count"],
        "paint_layer_count": len(paint_layers),
        "active_paint_layer_count": len(active_paint_layers),
        "dropped_paint_layer_count": plan["dropped_paint_layer_count"],
        "shadow_paint_layer_count": sum(
            1 for layer in paint_layers
            if isinstance(layer, dict) and layer["executor"] == "rounded-shadow"
        ),
        "fill_paint_layer_count": sum(
            1 for layer in paint_layers
            if isinstance(layer, dict) and layer["executor"] == "rounded-fill"
        ),
        "edge_paint_layer_count": sum(
            1 for layer in paint_layers
            if isinstance(layer, dict) and layer["executor"] == "rounded-edge"
        ),
        "max_paint_layer_inflate": plan["resource_budget"][
            "max_paint_layer_inflate"],
        "backdrop_filter_stage_count": sum(
            1 for stage in stages
            if isinstance(stage, dict) and stage["executor"] == "backdrop-filter"
        ),
        "fallback_fill_stage_count": sum(
            1 for stage in stages
            if isinstance(stage, dict) and stage["executor"] == "fallback-fill"
        ),
        "shadow_stage_count": sum(
            1 for stage in stages
            if isinstance(stage, dict) and stage["name"] == "shape-shadow"
        ),
        "edge_highlight_stage_count": sum(
            1 for stage in stages
            if isinstance(stage, dict) and stage["name"] == "edge-highlight"
        ),
        "noise_dither_stage_count": sum(
            1 for stage in stages
            if isinstance(stage, dict) and stage["name"] == "noise-dither"
        ),
        "backdrop_access_plan_count": 1 if backdrop_access["required"] else 0,
        "next_frame_capture_plan_count": (
            1 if backdrop_access["next_frame_capture_required"] else 0
        ),
        "planned_frame_capture_count": backdrop_access["max_frame_capture_count"],
        "planned_frame_capture_pixels": backdrop_access["max_frame_capture_pixels"],
        "planned_surface_sample_pixels": backdrop_access[
            "max_surface_sample_pixels"],
        "material_max_sample_taps": sample_taps,
        "material_total_sample_taps": sample_taps,
        "backdrop_copy_pixels": 0,
        "backdrop_copy_excludes_foreground_text": False,
        "foreground_pass_after_backdrop_copy": False,
        "material_upload_bytes": upload_bytes,
        "material_buffer_capacity_bytes": upload_bytes,
        "material_buffer_reallocations": 0,
        "execution_contract_satisfied_count": (
            1 if audit["contract_satisfied"] else 0),
        "execution_contract_mismatch_count": (
            0 if audit["contract_satisfied"] else 1),
        "execution_contract_mismatch_total": audit["mismatch_count"],
        "stage_order_match_count": 1 if audit["stage_order_match"] else 0,
        "stage_order_mismatch_count": 0 if audit["stage_order_match"] else 1,
        "first_execution_contract_mismatch": audit["first_mismatch"],
        "first_stage_order_mismatch": (
            "none" if audit["stage_order_match"]
            else audit["actual_stage_order"]),
        "foreground_text_candidate_count": 1,
        "foreground_text_remap_count": 1,
        "interaction_enabled_count": 1 if interaction["enabled"] else 0,
        "interaction_active_count": 1 if interaction["active"] else 0,
        "interaction_specular_highlight_count": (
            1 if interaction["specular_highlight_active"] else 0),
        "refraction_active_count": 1 if refraction["active"] else 0,
        "max_refraction_strength": refraction["strength"],
        "max_refraction_edge_bias": refraction["edge_bias"],
        "max_refraction_offset_pixels": refraction["max_offset_pixels"],
        "max_interaction_response_strength": interaction["response_strength"],
        "max_interaction_specular_radius": interaction["specular_radius"],
        "max_interaction_specular_intensity": interaction["specular_intensity"],
        "backdrop_descriptor_color_available": (
            bool(backdrop["available"])
            and int(backdrop["color_sample_count"]) > 0),
        "backdrop_descriptor_color_mean": backdrop["color_mean"],
        "backdrop_descriptor_color_sample_count": (
            backdrop["color_sample_count"]),
        "backdrop_descriptor_color_status": (
            backdrop["color_sample_status"]),
        "backdrop_descriptor_luma_available": False,
        "backdrop_descriptor_luma_min": plan["backdrop"]["luma_min"],
        "backdrop_descriptor_luma_max": plan["backdrop"]["luma_max"],
        "backdrop_descriptor_luma_mean": plan["backdrop"]["luma_mean"],
        "backdrop_descriptor_luma_sample_count": (
            plan["backdrop"]["luma_sample_count"]),
        "backdrop_descriptor_luma_grid_width": (
            plan["backdrop"]["luma_sample_grid_width"]),
        "backdrop_descriptor_luma_grid_height": (
            plan["backdrop"]["luma_sample_grid_height"]),
        "backdrop_descriptor_luma_frame": (
            plan["backdrop"]["luma_sample_frame"]),
        "backdrop_descriptor_luma_status": (
            plan["backdrop"]["luma_sample_status"]),
        "backdrop_descriptor_source": plan["backdrop"]["source"],
        "material_pipeline_ready": pipeline_ready,
        "material_backdrop_source_ready": backdrop_source_ready,
        "material_sampled_backdrop_upload_required": sampled_required,
        "material_sampled_backdrop_draw_required": sampled_required,
        "material_sampled_backdrop_uploaded": sampled_uploaded,
        "material_sampled_backdrop_drawn": sampled_drawn,
        "material_upload_status": upload_status,
        "material_draw_status": draw_status,
        "backdrop_luma_sampling_skipped_count": 0,
        "backdrop_luma_sampling_skip_reason": "none",
        "container_groups": material_container_group_summary(plan),
        "material_shader_content_scale": render_target["scale"],
        "material_max_shader_blur_step_pixels": shader_blur_step_pixels,
        "cpu_decode_ns": 100,
        "cpu_material_upload_ns": 0,
        "cpu_total_ns": 200,
    }


def snapshot(plan: dict[str, object]) -> dict[str, object]:
    decision_trace = plan.get("decision_trace")
    if not isinstance(decision_trace, dict):
        decision_trace = {}
    material_surfaces = decision_trace.get("capability_material_surfaces")
    material_backdrop_blur = decision_trace.get("capability_material_backdrop_blur")
    material_shader_blur = decision_trace.get("capability_shader_blur")
    material_frame_history = decision_trace.get("capability_frame_history")
    return {
        "debug": {
            "platform_capabilities": {
                "platform": "test",
                "read_only": True,
                "snapshot_json": True,
                "capture_frame_rgba": False,
                "write_artifact_bundle": True,
                "semantic_tree": True,
                "input_debug": True,
                "platform_runtime": True,
                "frame_image": False,
                "platform_diagnostics": False,
                "material_surfaces": (
                    material_surfaces
                    if isinstance(material_surfaces, bool)
                    else True),
                "material_backdrop_blur": (
                    material_backdrop_blur
                    if isinstance(material_backdrop_blur, bool)
                    else False),
                "material_shader_blur": (
                    material_shader_blur
                    if isinstance(material_shader_blur, bool)
                    else False),
                "material_frame_history": (
                    material_frame_history
                    if isinstance(material_frame_history, bool)
                    else False),
                "material_max_shader_sample_taps": 25,
                "material_max_texture_dimension_2d": 0,
                "material_max_backdrop_pixels": 0,
                "material_capability_profile": "generic",
                "material_capability_source": "default",
                "reduce_transparency": False,
                "increase_contrast": False,
                "reduce_motion": False,
                "system_settings": {
                    "source": "test-fallback",
                    "font_family": "Pretendard",
                    "font_family_source": "test",
                    "body_font_size": 16.0,
                    "heading_font_size": 22.4,
                    "small_font_size": 14.4,
                    "line_height_ratio": 1.6,
                    "font_scale": 1.0,
                    "text_size_source": "fallback",
                    "font_weight_adjustment": 0,
                    "font_weight_source": "fallback",
                    "preferred_scroller_style": "none",
                    "overlay_scrollbars": False,
                    "scroll_line_height": 25.6,
                    "scroll_wheel_lines": 3.0,
                    "scroll_page_mode": False,
                    "scroll_vertical_factor": 76.8,
                    "scroll_horizontal_factor": 76.8,
                    "scroll_bar_size": 0.0,
                    "touch_slop": 0.0,
                    "scroll_friction": 0.0,
                    "scroll_delta_multiplier": 1.0,
                    "scroll_horizontal_delta_multiplier": 1.0,
                    "scroll_source": "fallback",
                    "font_family_available": True,
                    "font_metrics_available": True,
                    "font_scale_available": False,
                    "line_height_available": False,
                    "scroll_metrics_available": False,
                    "color_scheme_available": True,
                    "double_click_interval_ms": 500.0,
                    "key_repeat_delay_ms": 500.0,
                    "key_repeat_interval_ms": 50.0,
                    "caret_blink_interval_ms": 530.0,
                    "input_timing_source": "test",
                    "preferred_locale": "en",
                    "preferred_locale_source": "test",
                    "color_scheme": "light",
                    "color_scheme_source": "test",
                    "appearance_name": "test-light",
                    "reduce_transparency": False,
                    "increase_contrast": False,
                    "reduce_motion": False,
                    "accessibility_source": "test",
                    "accent_color_available": False,
                    "accent_color_source": "fallback",
                    "accent_color_opaque": True,
                },
            },
            "input_debug": {
                "event": "none",
                "source": "test",
                "detail": "synthetic verifier self-test",
                "result": "none",
                "pressed_id": 4294967295,
                "focus_visible": False,
                "input_modality": "none",
                "focus_visibility_reason": "no_focus",
                "caret_rect": {"x": 0, "y": 0, "w": 0, "h": 0, "valid": False},
            },
            "semantic_tree": {
                "role": "root",
                "label": "synthetic-root",
                "visible": True,
                "children": [
                    {
                        "role": "material",
                        "label": "Synthetic material",
                        "visible": True,
                        "material": {
                            "kind": "regular",
                            "role": "surface",
                            "opacity": 0.58,
                            "blur_radius": 22.0,
                            "tint": {"r": 255, "g": 255, "b": 255, "a": 148},
                            "saturation": 1.08,
                            "luminance_floor": 0.08,
                            "luminance_gain": 1.08,
                            "edge_highlight": 0.34,
                            "edge_width": 1.0,
                            "noise_opacity": 0.014,
                            "shadow_alpha": 0.14,
                            "shadow_radius": 14.0,
                            "container": {
                                "mode": "isolated",
                                "container_id": 0,
                                "union_id": 0,
                                "spacing": 0.0,
                                "interactive": False,
                                "morph_transitions": False,
                            },
                            "fallback": True,
                            "verifier_profile": "regular-legibility-backdrop",
                        },
                        "children": [],
                    }
                ],
            },
            "platform_runtime": {
                "backend": "synthetic",
                "content_height": 240.0,
                "viewport": {"x": 0, "y": 0, "w": 320, "h": 240, "valid": True},
                "pressed_callback_id": None,
                "focus_visible": False,
                "input_modality": "none",
                "focus_visibility_reason": "no_focus",
                "details": {
                    "renderer": {
                        "material_plan_contract_version": (
                            verifier.MATERIAL_PLAN_CONTRACT_VERSION),
                        "material_backdrop_luma_descriptor": {
                            "available": False,
                            "color_available": False,
                            "color_mean": {
                                "r": 255,
                                "g": 255,
                                "b": 255,
                                "a": 255,
                            },
                            "luma_min": 0.0,
                            "luma_max": 1.0,
                            "luma_mean": 0.5,
                            "sample_count": 0,
                            "sample_grid_width": 0,
                            "sample_grid_height": 0,
                            "sample_frame": 0,
                            "status": "unsupported-fallback",
                            "pending": False,
                            "skipped_sample_count": 0,
                        },
                        "material_plan_count": 1,
                        "material_plans": [plan],
                        "material_container_groups": (
                            material_container_group_details(plan)),
                        "material_runtime_summary": material_runtime_summary(plan),
                        "material_executor_summary": material_executor_summary(plan),
                    }
                },
            },
        }
    }


def snapshot_with_file_explorer_chrome(
    plan: dict[str, object],
    *,
    duplicate: bool = False,
    content_markers: bool = False,
    artifact_markers: bool = False,
    content_marker_count: int = 0,
    artifact_marker_count: int = 0,
    runtime_integrated: bool = True,
    runtime_visible_count: int = 3,
) -> dict[str, object]:
    root = snapshot(plan)
    debug = root["debug"]
    assert isinstance(debug, dict)
    window_control_single_owner = (
        not duplicate
        and not content_markers
        and not artifact_markers
        and content_marker_count == 0
        and artifact_marker_count == 0)
    window_control_guard = (
        "native_window_controls_single_owner"
        if window_control_single_owner
        else "duplicate_window_control_risk")
    runtime_single_owner = runtime_integrated and runtime_visible_count == 3
    runtime_guard = (
        "native_window_controls_single_owner"
        if runtime_single_owner
        else "native_window_controls_not_integrated")
    debug["application"] = {
        "file_explorer": {
            "kind": "file_explorer",
            "profile": "desktop",
            "finder_visual_contract": {
                "schema_version": 1,
                "name": "finder_visual_parity_contract",
                "profile": "desktop",
                "source": "file_explorer_shared::finder_visual_contract",
                "reference": (
                    "Apple HIG Materials, Icons, SF Symbols semantics, "
                    "and macOS Finder visual reference without embedded "
                    "Apple artwork"),
                "titlebar_strategy": (
                    "integrated_titlebar_content_reserve_with_platform_window_controls"),
                "native_control_owner": "platform-edge",
                "native_control_marker_policy": (
                    "no_content_or_artifact_window_control_markers"),
                "native_control_geometry_role": (
                    "reserve_metrics_only_not_paint_instructions"),
                "native_control_palette_policy": (
                    "traffic_light_palette_forbidden_in_content_and_artifacts"),
                "sidebar_selection_style": (
                    "finder_translucent_selected_row_no_outline_accent_symbol"),
                "sidebar_selected_row_border_width": 0.0,
                "sidebar_selected_row_background_alpha": 150,
                "focus_ring_policy": (
                    "keyboard_tab_navigation_only_pointer_click_hides_focus_ring"),
                "icon_source_policy": (
                    "audited permissive SVG only; accepted "
                    "external licenses include Lucide ISC"),
                "embedded_svg_policy": (
                    "Every embedded icon source must carry a pinned direct "
                    "raw SVG URL, source revision, platform extraction flag, "
                    "and runtime fetch flag."),
                "apple_asset_symbol_count": 0,
                "platform_extracted_symbol_count": 0,
                "runtime_fetched_symbol_count": 0,
                "thumbnail_preview_policy": "finder_rich_preview_thumbnails_v1",
                "leading_control_reserved_width": 176.0,
                "titlebar_drag_region_height": 64.0,
                "verifier_gate": (
                    "local_file_explorer_artifact_verify_not_default_pr_ci"),
            },
            "preferences": {
                "system_settings": {
                    "source": "test-fallback",
                    "font_family_available": False,
                    "font_metrics_available": True,
                    "font_scale_available": False,
                    "line_height_available": True,
                    "scroll_metrics_available": True,
                    "color_scheme_available": True,
                    "reduce_motion_available": True,
                    "accent_color_available": True,
                },
                "app_overrides": {
                    "apply_system_font_metrics": True,
                    "apply_system_reduce_motion": True,
                    "motion_duration_multiplier": 1.0,
                },
                "effective_theme": {
                    "font_family": "Pretendard",
                    "color_scheme": "light",
                    "body_font_size": 16.0,
                    "heading_font_size": 22.0,
                    "small_font_size": 14.0,
                    "line_height_ratio": 1.6,
                    "scroll_delta_multiplier": 1.0,
                    "scroll_horizontal_delta_multiplier": 1.0,
                    "motion_duration_multiplier": 1.0,
                },
                "resolution": {
                    "used_system_font_family": False,
                    "used_system_color_scheme": True,
                    "used_system_font_metrics": True,
                    "used_system_font_scale": False,
                    "used_user_font_scale": False,
                    "used_user_font_size": False,
                    "used_system_line_height": True,
                    "used_user_line_height": False,
                    "used_system_scroll_metrics": True,
                    "used_user_scroll_scale": False,
                    "used_system_accent_color": True,
                    "used_system_reduce_motion": False,
                    "used_user_motion_scale": False,
                },
            },
            "chrome": {
                "sidebar_selected_row_background_alpha": 150,
                "thumbnail_system": {
                    "visual_policy": "finder_rich_preview_thumbnails_v1",
                },
                "icon_system": {
                    "apple_asset_symbol_count": 0,
                },
                "native_window": {
                    "integrated_titlebar": True,
                    "native_window_controls": True,
                    "duplicate_window_controls": duplicate,
                    "window_control_single_owner": window_control_single_owner,
                    "content_window_control_markers": content_markers,
                    "artifact_window_control_markers": artifact_markers,
                    "window_control_marker_mode": "runtime-native-controls",
                    "native_window_control_owner": "platform-edge",
                    "window_control_duplication_guard": window_control_guard,
                    "native_window_control_count": 3,
                    "content_window_control_marker_count": content_marker_count,
                    "artifact_window_control_marker_count": artifact_marker_count,
                    "content_drawn_window_control_count": 0,
                    "artifact_drawn_window_control_count": 0,
                    "window_control_render_policy": (
                        "native_controls_runtime_only_no_content_or_artifact_markers"),
                    "titlebar_control_reserve_policy": (
                        "blank_reserve_under_os_window_controls"),
                    "native_window_control_integration_policy": (
                        "platform_standard_controls_inside_leading_content_reserve"),
                    "native_window_control_geometry_role": (
                        "reserve_metrics_only_not_paint_instructions"),
                    "native_window_control_palette_policy": (
                        "traffic_light_palette_forbidden_in_content_and_artifacts"),
                    "leading_control_reserved_width": 176.0,
                    "titlebar_drag_region_height": 64.0,
                }
            },
        }
    }
    debug["platform_runtime"]["details"]["window"] = {
        "surface_kind": "macos_window",
        "window_opaque": False,
        "window_background_clear": True,
        "window_background_alpha": 0,
        "metal_layer_opaque": False,
        "native_backdrop_underlay_enabled": True,
        "native_backdrop_underlay_kind": "nsvisualeffectview",
        "native_backdrop_underlay_placement": "sibling-underlay",
        "native_backdrop_underlay_material": "sidebar",
        "native_backdrop_expected_material": "sidebar",
        "native_backdrop_underlay_alpha": 1.0,
        "native_backdrop_expected_alpha": 1.0,
        "native_backdrop_underlay_blending_mode": "behind-window",
        "native_backdrop_underlay_state": "active",
        "native_backdrop_underlay_emphasized": True,
        "glass_backdrop_composition": {
            "schema_version": 1,
            "policy": (
                "transparent-window-clear-metal-native-sidebar"),
            "native_backdrop_expected_material": "sidebar",
            "native_backdrop_underlay_placement": "sibling-underlay",
            "native_backdrop_underlay_alpha": 1.0,
            "native_backdrop_expected_alpha": 1.0,
            "status": "ready",
            "ready": True,
            "failure_reason": "none",
            "likely_layer": "none",
            "likely_pass": "none",
            "requires_transparent_window": True,
            "requires_clear_metal_layer": True,
            "requires_native_backdrop_underlay": True,
            "requires_sibling_underlay": True,
            "requires_under_window_background_underlay": False,
            "requires_alpha_zero_clear": True,
            "requires_no_full_frame_opaque_fill": True,
            "samples_external_backdrop": True,
        },
        "native_window_controls": {
            "ownership_policy": "platform_edge_standard_buttons_only",
            "integration_policy": (
                "standard_buttons_inside_leading_content_reserve"),
            "expected_count": 3,
            "present_count": 3,
            "visible_count": runtime_visible_count,
            "hidden_count": 3 - runtime_visible_count,
            "within_leading_reserve_count": 3 if runtime_integrated else 2,
            "all_buttons_within_leading_reserve": runtime_integrated,
            "integrated_in_content_area": runtime_integrated,
            "duplicate_window_controls": False,
            "window_control_single_owner": runtime_single_owner,
            "window_control_duplication_guard": runtime_guard,
            "content_drawn_window_control_count": 0,
            "artifact_drawn_window_control_count": 0,
        },
    }
    debug["platform_runtime"]["details"]["renderer"]["clear_alpha"] = 0
    debug["platform_runtime"]["details"]["renderer"][
        "clear_alpha_for_transparent_window"] = True
    debug["platform_runtime"]["details"]["renderer"][
        "full_frame_opaque_fill_count"] = 0
    debug["platform_runtime"]["details"]["renderer"][
        "transparent_window_has_opaque_frame_fill"] = False
    return root


def install_file_explorer_sidebar_material(
    root: dict[str, object],
    *,
    opacity: float = 0.34,
    blur_radius: float = 28.0,
    tint_alpha: int = 76,
) -> None:
    container = {
        "mode": "container",
        "container_id": 2100,
        "union_id": 0,
        "spacing": 16.0,
        "interactive": False,
        "morph_transitions": True,
    }
    descriptor = {
        "kind": "thin",
        "role": "sidebar",
        "container": container,
        "interaction": {
            "hovered": False,
            "pressed": False,
            "focused": False,
            "pointer_inside": False,
            "active": False,
            "pointer_x": 0.5,
            "pointer_y": 0.5,
        },
        "opacity": opacity,
        "blur_radius": blur_radius,
        "tint": {"r": 248, "g": 248, "b": 250, "a": tint_alpha},
        "saturation": 1.28,
        "luminance_floor": 0.03,
        "luminance_gain": 1.03,
        "edge_highlight": 0.32,
        "edge_width": 1.0,
        "noise_opacity": 0.014,
        "shadow_alpha": 0.10,
        "shadow_radius": 14.0,
    }

    material = root["debug"]["semantic_tree"]["children"][0]["material"]
    material.update({
        key: value
        for key, value in descriptor.items()
        if key != "interaction"
    })
    material["fallback"] = True
    material["verifier_profile"] = "thin-balanced-backdrop"

    plan = root["debug"]["platform_runtime"]["details"]["renderer"][
        "material_plans"][0]
    plan.update({
        "kind": "thin",
        "role": "sidebar",
        "plan_id": "material.thin.fallback",
        "command_descriptor": descriptor,
        "container": {
            **container,
            "blend_distance": 16.0,
            "participates": True,
            "requested_morph_transitions": True,
            "shared_backdrop_scope": False,
            "shape_union_expected": False,
            "shape_blending_expected": True,
            "reduced_motion_suppressed_morph": False,
            "spacing_clamped": False,
            "blend_policy": "container",
            "morph_policy": "container-morph",
            "performance_policy": "shared-container-capture",
        },
        "opacity": opacity,
        "blur_radius": 0.0,
        "tint": descriptor["tint"],
        "saturation": 1.0,
        "luminance_floor": 0.03,
        "luminance_gain": 1.03,
        "edge_highlight": 0.32,
        "shadow_alpha": 0.10,
        "shadow_radius": 14.0,
    })
    reference_model = plan["reference_model"]
    reference_model.update({
        "variant": "thin",
        "semantic_thickness": "thin",
        "container_grouped": True,
        "container_morphing": True,
    })
    theme = plan["theme"]
    theme["tint"] = descriptor["tint"]
    verifier_obj = plan["verifier"]
    verifier_obj["region_name"] = "thin-balanced-backdrop"
    refresh_observation_contract(plan)
    refresh_execution_audit(plan)
    renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
    renderer["material_container_groups"] = material_container_group_details(plan)
    renderer["material_runtime_summary"] = material_runtime_summary(plan)
    renderer["material_executor_summary"] = material_executor_summary(plan)


def snapshot_with_glass_probe_contract(
    plan: dict[str, object],
) -> dict[str, object]:
    root = snapshot(plan)
    debug = root["debug"]
    assert isinstance(debug, dict)
    debug["application"] = {
        "glass_showcase": {
            "kind": "glass_showcase",
            "probe_contract": {
                "contract_name": "glass_showcase_material_probe_contract",
                "reference_technology": "liquid-glass",
                "active_material_probe_count": 7,
                "stage_count_per_probe": 4,
                "total_expected_execution_stages": 28,
                "fallback_contract": (
                    "translucent rounded rectangle when sampled backdrop is unavailable"),
                "material_probes": {
                    "visible_blur_probe": {
                        "kind": "thin",
                        "expected_pass": "backdrop-sample-blur",
                    },
                    "debug_contract": {
                        "kind": "thick",
                        "requires_inspector_open": True,
                    },
                },
            },
        }
    }
    return root


def write_synthetic_bmp(path: Path) -> None:
    width = 8
    height = 2
    rows = [
        [
            (0, 0, 0, 255),
            (255, 255, 255, 255),
            (0, 0, 0, 255),
            (255, 255, 255, 255),
            (100, 100, 100, 255),
            (110, 110, 110, 255),
            (120, 120, 120, 255),
            (130, 130, 130, 255),
        ],
        [
            (0, 0, 0, 255),
            (255, 255, 255, 255),
            (0, 0, 0, 255),
            (255, 255, 255, 255),
            (100, 100, 100, 255),
            (110, 110, 110, 255),
            (120, 120, 120, 255),
            (130, 130, 130, 255),
        ],
    ]
    pixel_data = bytearray()
    for row in rows:
        for r, g, b, a in row:
            pixel_data.extend((b, g, r, a))
    pixel_offset = 14 + 40
    file_size = pixel_offset + len(pixel_data)
    header = bytearray()
    header.extend(b"BM")
    header.extend(struct.pack("<IHHI", file_size, 0, 0, pixel_offset))
    header.extend(struct.pack("<IiiHHIIiiII",
                              40, width, -height, 1, 32, 0,
                              len(pixel_data), 0, 0, 0, 0))
    path.write_bytes(header + pixel_data)


def write_channel_probe_bmp(path: Path) -> None:
    width = 3
    height = 1
    row = [
        (255, 95, 86, 255),
        (255, 189, 46, 255),
        (39, 201, 63, 255),
    ]
    pixel_data = bytearray()
    for r, g, b, a in row:
        pixel_data.extend((b, g, r, a))
    pixel_offset = 14 + 40
    file_size = pixel_offset + len(pixel_data)
    header = bytearray()
    header.extend(b"BM")
    header.extend(struct.pack("<IHHI", file_size, 0, 0, pixel_offset))
    header.extend(struct.pack("<IiiHHIIiiII",
                              40, width, -height, 1, 32, 0,
                              len(pixel_data), 0, 0, 0, 0))
    path.write_bytes(header + pixel_data)


class ArtifactVerifierContractTest(unittest.TestCase):
    def run_verifier(
        self,
        snapshot_json: dict[str, object],
        manifest: dict[str, object] | None = None,
        write_frame: bool = False,
        frame_writer=None,
    ) -> tuple[int, dict[str, object]]:
        with tempfile.TemporaryDirectory() as raw_dir:
            bundle = Path(raw_dir)
            (bundle / "snapshot.json").write_text(
                json.dumps(snapshot_json),
                encoding="utf-8")
            if write_frame or frame_writer is not None:
                writer = frame_writer if frame_writer is not None else write_synthetic_bmp
                writer(bundle / "frame.bmp")
            args = [
                str(bundle),
                "--require-material-plan",
                "--require-material-semantic-runtime-match",
            ]
            if manifest is not None:
                manifest_path = bundle / "artifact_manifest.json"
                manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
                args.extend(["--manifest", str(manifest_path)])
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                code = verifier.main(args)
            return code, json.loads(output.getvalue())

    def test_material_plan_contract_accepts_synchronized_fallback_taps(self) -> None:
        code, report = self.run_verifier(snapshot(material_plan()))

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(report["failures"], [])
        self.assertEqual(report["material_plans"]["count"], 1)
        self.assertEqual(report["material_plans"]["fallback"], 1)
        self.assertEqual(report["material_plans"]["roles"], {"surface": 1})
        self.assertEqual(
            report["semantic_tree"]["material_roles"],
            {"surface": 1})
        self.assertEqual(
            report["material_plans"]["resource_bounds"]["max_plan_sample_taps"],
            0)

    def test_material_foreground_margin_mismatch_is_llm_actionable(self) -> None:
        plan = material_plan()
        foreground = plan["foreground"]
        assert isinstance(foreground, dict)
        foreground["primary_contrast_margin"] = 0.25

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"]
            == "material foreground primary_contrast_margin matches ratio")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".foreground.primary_contrast_margin")
        self.assertEqual(failure["expected"], 7.5)
        self.assertEqual(failure["actual"], 0.25)
        self.assertEqual(failure["likely_layer"], "material-foreground")
        self.assertIn("contrast_ratio - minimum_contrast_ratio", failure["hint"])

    def test_material_high_contrast_minimum_is_llm_actionable(self) -> None:
        plan = material_plan()
        foreground = plan["foreground"]
        assert isinstance(foreground, dict)
        foreground["high_contrast"] = True
        foreground["contrast_policy"] = "standard-contrast"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"]
            == "material high contrast raises foreground minimum")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".foreground.minimum_contrast_ratio")
        self.assertEqual(failure["expected"], {">=": 7.0})
        self.assertEqual(failure["actual"], 4.5)
        self.assertEqual(failure["likely_layer"], "material-foreground")

    def test_material_quality_policy_over_engine_cap_is_llm_actionable(self) -> None:
        plan = material_plan()
        quality = plan["quality_policy"]
        assert isinstance(quality, dict)
        quality["max_blur_radius"] = 40.0

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material quality policy blur radius respects engine cap"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".quality_policy.max_blur_radius")
        self.assertEqual(failure["expected"], {"<=": 36.0})
        self.assertEqual(failure["actual"], 40.0)
        self.assertEqual(failure["likely_pass"], "quality-policy")
        self.assertIn("material_max_blur_radius", failure["hint"])

    def test_material_resource_budget_over_engine_cap_is_llm_actionable(self) -> None:
        plan = material_plan()
        budget = plan["resource_budget"]
        assert isinstance(budget, dict)
        budget["max_sample_taps"] = 26

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material resource budget sample taps respect engine cap"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".resource_budget.max_sample_taps")
        self.assertEqual(failure["expected"], {"<=": 25})
        self.assertEqual(failure["actual"], 26)
        self.assertEqual(failure["likely_pass"], "resource-budget")
        self.assertIn("material_max_sample_taps", failure["hint"])

    def test_file_explorer_native_chrome_contract_accepts_platform_owner(self) -> None:
        code, report = self.run_verifier(
            snapshot_with_file_explorer_chrome(material_plan()))

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])

    def test_file_explorer_preferences_contract_rejects_missing_resolution_flag(self) -> None:
        snap = snapshot_with_file_explorer_chrome(material_plan())
        resolution = snap["debug"]["application"]["file_explorer"][
            "preferences"]["resolution"]
        del resolution["used_system_font_metrics"]

        code, report = self.run_verifier(snap)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["path"] == (
                "debug.application.file_explorer.preferences.resolution"
                ".used_system_font_metrics"))
        self.assertEqual(failure["likely_layer"], "file-explorer-preferences")
        self.assertIn("Resolver evidence booleans", failure["hint"])

    def test_file_explorer_preferences_contract_rejects_missing_availability_flag(self) -> None:
        snap = snapshot_with_file_explorer_chrome(material_plan())
        settings = snap["debug"]["application"]["file_explorer"][
            "preferences"]["system_settings"]
        del settings["scroll_metrics_available"]

        code, report = self.run_verifier(snap)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["path"] == (
                "debug.application.file_explorer.preferences.system_settings"
                ".scroll_metrics_available"))
        self.assertEqual(failure["likely_layer"], "file-explorer-preferences")
        self.assertIn("availability flags", failure["hint"])

    def test_file_explorer_preferences_contract_rejects_used_system_without_availability(self) -> None:
        snap = snapshot_with_file_explorer_chrome(material_plan())
        preferences = snap["debug"]["application"]["file_explorer"]["preferences"]
        settings = preferences["system_settings"]
        resolution = preferences["resolution"]
        settings["font_metrics_available"] = False
        resolution["used_system_font_metrics"] = True

        code, report = self.run_verifier(snap)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["path"] == (
                "debug.application.file_explorer.preferences.resolution"
                ".used_system_font_metrics"))
        self.assertEqual(failure["likely_pass"], "preference-resolution")
        self.assertEqual(
            failure["actual"],
            {
                "used_system_font_metrics": True,
                "font_metrics_available": False,
            })
        self.assertIn("cannot claim an OS preference", failure["hint"])

    def test_file_explorer_native_chrome_contract_rejects_content_markers(self) -> None:
        code, report = self.run_verifier(
            snapshot_with_file_explorer_chrome(
                material_plan(),
                content_markers=True,
                content_marker_count=3))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "file explorer does not draw duplicate native window controls")
        self.assertEqual(failure["likely_layer"], "native-window-chrome")
        self.assertEqual(failure["likely_pass"], "window-control-marker")
        self.assertIn("reserve the titlebar", failure["hint"])
        self.assertEqual(
            report["material_plans"]["resource_bounds"]["max_sample_taps"],
            25)
        self.assertEqual(
            report["material_plans"]["resource_bounds"]["total_runtime_passes"],
            1)
        self.assertEqual(
            report["material_plans"]["resource_bounds"]["active_runtime_passes"],
            1)
        self.assertEqual(
            report["material_plans"]["resource_bounds"]["backdrop_runtime_passes"],
            0)
        self.assertEqual(
            report["material_plans"]["pass_executors"],
            {"fallback-fill": 1})
        self.assertEqual(
            report["material_plans"]["luminance_curves"],
            {"fallback-flat": 1})
        self.assertEqual(
            report["material_plans"]["luminance_curve_backdrop_driven"],
            0)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "max_pass_texture_copy_pixels"],
            0)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "total_pass_texture_copy_pixels"],
            0)
        self.assertEqual(
            report["material_plans"]["backdrop_access"]["required"],
            0)
        self.assertEqual(
            report["material_plans"]["backdrop_access"]["capture_scopes"],
            {"none": 1})

    def test_file_explorer_native_chrome_contract_rejects_paint_geometry_policy(self) -> None:
        snap = snapshot_with_file_explorer_chrome(material_plan())
        native_window = snap["debug"]["application"]["file_explorer"][
            "chrome"]["native_window"]
        native_window["native_window_control_geometry_role"] = (
            "draw_three_titlebar_controls")

        code, report = self.run_verifier(snap)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "file explorer does not draw duplicate native window controls")
        self.assertEqual(failure["likely_layer"], "native-window-chrome")
        self.assertEqual(failure["likely_pass"], "window-control-marker")
        self.assertIn("not an instruction to draw", failure["hint"])
        self.assertEqual(
            failure["actual"]["native_window_control_geometry_role"],
            "draw_three_titlebar_controls")

    def test_file_explorer_native_chrome_contract_rejects_unintegrated_buttons(self) -> None:
        code, report = self.run_verifier(
            snapshot_with_file_explorer_chrome(
                material_plan(),
                runtime_integrated=False))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "file explorer native window buttons are integrated "
                "into content chrome"))
        self.assertEqual(failure["likely_layer"], "native-window-chrome")
        self.assertEqual(
            failure["likely_pass"],
            "appkit-standard-window-buttons")
        self.assertIn("leading content reserve", failure["hint"])
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "max_frame_capture_count"],
            0)
        self.assertEqual(
            report["material_plans"]["decision_trace"]["first_blockers"],
            {"unsupported-backend": 1})
        self.assertEqual(
            report["material_plans"]["command_descriptor_missing"],
            0)

    def test_file_explorer_native_chrome_contract_rejects_opaque_macos_window(self) -> None:
        snap = snapshot_with_file_explorer_chrome(material_plan())
        runtime_window = snap["debug"]["platform_runtime"]["details"]["window"]
        runtime_window["window_opaque"] = True
        runtime_window["window_background_clear"] = False
        runtime_window["window_background_alpha"] = 255
        runtime_window["metal_layer_opaque"] = True
        runtime_window["native_backdrop_underlay_enabled"] = False
        runtime_window["native_backdrop_underlay_kind"] = "none"
        runtime_window["native_backdrop_underlay_placement"] = "none"
        runtime_window["native_backdrop_underlay_material"] = "none"
        runtime_window["native_backdrop_expected_material"] = "sidebar"
        runtime_window["native_backdrop_underlay_alpha"] = 0.55
        runtime_window["native_backdrop_expected_alpha"] = 1.0
        runtime_window["native_backdrop_underlay_blending_mode"] = "none"
        runtime_window["native_backdrop_underlay_state"] = "none"
        runtime_window["glass_backdrop_composition"] = {
            "schema_version": 1,
            "policy": (
                "transparent-window-clear-metal-native-sidebar"),
            "native_backdrop_expected_material": "sidebar",
            "native_backdrop_underlay_placement": "none",
            "native_backdrop_underlay_alpha": 0.55,
            "native_backdrop_expected_alpha": 1.0,
            "status": "blocked",
            "ready": False,
            "failure_reason": "nswindow_opaque",
            "likely_layer": "native-window-composition",
            "likely_pass": "appkit-window-opacity",
        }
        runtime_renderer = snap["debug"]["platform_runtime"]["details"]["renderer"]
        runtime_renderer["clear_alpha"] = 1
        runtime_renderer["clear_alpha_for_transparent_window"] = False
        runtime_renderer["full_frame_opaque_fill_count"] = 1
        runtime_renderer["transparent_window_has_opaque_frame_fill"] = True

        code, report = self.run_verifier(snap)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "file explorer macOS window has native wallpaper "
                "backdrop underlay"))
        self.assertEqual(failure["likely_layer"], "native-window-composition")
        self.assertEqual(failure["likely_pass"], "appkit-visual-effect-underlay")
        self.assertIn("CAMetalLayer stays opaque", failure["hint"])
        self.assertEqual(
            failure["actual"],
            {
                "summary_schema_version": 1,
                "summary_policy": (
                    "transparent-window-clear-metal-native-sidebar"),
                "summary_native_backdrop_expected_material": "sidebar",
                "summary_native_backdrop_underlay_placement": "none",
                "summary_native_backdrop_underlay_alpha": 0.55,
                "summary_native_backdrop_expected_alpha": 1.0,
                "summary_status": "blocked",
                "summary_ready": False,
                "summary_failure_reason": "nswindow_opaque",
                "summary_likely_layer": "native-window-composition",
                "summary_likely_pass": "appkit-window-opacity",
                "window_opaque": True,
                "window_background_clear": False,
                "window_background_alpha": 255,
                "metal_layer_opaque": True,
                "native_backdrop_underlay_enabled": False,
                "native_backdrop_underlay_kind": "none",
                "native_backdrop_underlay_placement": "none",
                "native_backdrop_underlay_material": "none",
                "native_backdrop_expected_material": "sidebar",
                "native_backdrop_underlay_alpha": 0.55,
                "native_backdrop_expected_alpha": 1.0,
                "native_backdrop_underlay_blending_mode": "none",
                "native_backdrop_underlay_state": "none",
                "renderer_clear_alpha": 1,
                "renderer_clear_alpha_for_transparent_window": False,
                "renderer_full_frame_opaque_fill_count": 1,
                "renderer_transparent_window_has_opaque_frame_fill": True,
            })

    def test_file_explorer_native_chrome_contract_rejects_backdrop_alpha_drift(self) -> None:
        snap = snapshot_with_file_explorer_chrome(material_plan())
        runtime_window = snap["debug"]["platform_runtime"]["details"]["window"]
        runtime_window["native_backdrop_underlay_alpha"] = 0.55
        composition = runtime_window["glass_backdrop_composition"]
        composition["native_backdrop_underlay_alpha"] = 0.55

        code, report = self.run_verifier(snap)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "file explorer macOS window has native wallpaper "
                "backdrop underlay"))
        self.assertEqual(failure["likely_layer"], "native-window-composition")
        self.assertEqual(failure["likely_pass"], "appkit-visual-effect-underlay")
        self.assertEqual(
            failure["actual"]["native_backdrop_underlay_alpha"],
            0.55)
        self.assertEqual(
            failure["actual"]["native_backdrop_expected_alpha"],
            1.0)

    def test_file_explorer_finder_visual_contract_rejects_drift(self) -> None:
        snap = snapshot_with_file_explorer_chrome(material_plan())
        contract = snap["debug"]["application"]["file_explorer"][
            "finder_visual_contract"]
        contract["native_control_owner"] = "content-shell"
        contract["apple_asset_symbol_count"] = 1

        code, report = self.run_verifier(snap)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "file explorer Finder visual parity contract is coherent"))
        self.assertEqual(failure["likely_layer"], "finder-visual-contract")
        self.assertEqual(failure["likely_pass"], "application-debug")
        self.assertIn("OS-owned traffic lights", failure["hint"])
        self.assertEqual(failure["actual"]["native_control_owner"], "content-shell")
        self.assertEqual(failure["actual"]["apple_asset_symbol_count"], 1)
        self.assertEqual(
            report["material_plans"]["shape"]["valid"],
            1)
        self.assertEqual(
            report["material_plans"]["shape"]["rounded"],
            1)
        self.assertEqual(
            report["material_plans"]["shape"]["capsule"],
            0)
        self.assertEqual(
            report["material_plans"]["shape"]["radius_clamped"],
            0)
        self.assertEqual(
            report["material_plans"]["shape"]["max_effective_radius"],
            10.0)
        self.assertEqual(
            report["material_plans"]["reference_model"]["technologies"],
            {"liquid-glass": 1})
        self.assertEqual(
            report["material_plans"]["reference_model"]["blending_scopes"],
            {"deterministic-fallback": 1})
        self.assertEqual(
            report["material_plans"]["reference_model"][
                "accessibility_responses"],
            {"standard": 1})
        self.assertEqual(
            report["material_plans"]["reference_model"][
                "performance_responses"],
            {"deterministic-fallback": 1})
        self.assertEqual(
            report["material_plans"]["reference_model"]["view_bounds_anchored"],
            1)
        self.assertEqual(
            report["material_plans"]["reference_model"][
                "deterministic_degradation"],
            1)
        self.assertEqual(
            report["material_plans"]["theme"]["profile_names"],
            {"apple-glass-light": 1})
        self.assertEqual(
            report["material_plans"]["theme"]["sources"],
            {"material-style": 1})
        self.assertEqual(
            report["material_plans"]["theme"]["default_glass_tokens"],
            1)
        self.assertEqual(
            report["semantic_tree"]["material_descriptor_missing"],
            0)

    def test_file_explorer_sidebar_glass_contract_rejects_metal_self_blur(self) -> None:
        snap = snapshot_with_file_explorer_chrome(material_plan())
        install_file_explorer_sidebar_material(snap, opacity=0.70, tint_alpha=180)

        code, report = self.run_verifier(snap)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "file explorer desktop sidebar does not fake wallpaper blur "
                "with Metal self sampling"))
        self.assertEqual(failure["likely_layer"], "finder-sidebar-glass")
        self.assertEqual(failure["likely_pass"], "material-command-boundary")
        self.assertEqual(
            failure["path"],
            "debug.semantic_tree.material_descriptors"
            "[role=sidebar,kind=thin,container_id=2100]")
        self.assertEqual(
            failure["actual"]["semantic_candidates"][0]["opacity"],
            0.70)
        self.assertEqual(
            failure["actual"]["semantic_candidates"][0]["tint_alpha"],
            180)
        self.assertIn("native compositor", failure["hint"])

    def test_reference_model_failure_points_to_pure_planner(self) -> None:
        plan = material_plan()
        reference_model = plan["reference_model"]
        assert isinstance(reference_model, dict)
        reference_model["blending_scope"] = "sampled-backdrop"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material reference blending_scope is derived")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".reference_model.blending_scope")
        self.assertEqual(failure["expected"], "deterministic-fallback")
        self.assertEqual(failure["actual"], "sampled-backdrop")
        self.assertEqual(failure["likely_layer"], "material-reference")
        self.assertIn("MaterialPlan.reference_model", failure["suggested_action"])

    def test_reference_accessibility_response_is_derived(self) -> None:
        plan = material_plan()
        decision_trace = plan["decision_trace"]
        reference_model = plan["reference_model"]
        assert isinstance(decision_trace, dict)
        assert isinstance(reference_model, dict)
        decision_trace["increase_contrast"] = True
        reference_model["accessibility_response"] = "standard"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"]
            == "material reference accessibility_response is derived")
        self.assertEqual(failure["expected"], "increased-contrast")
        self.assertEqual(failure["actual"], "standard")
        self.assertEqual(failure["likely_layer"], "material-reference")

    def test_manifest_can_pin_reference_model_summary(self) -> None:
        manifest = {
            "require_material_plan_summary": {
                "reference_technologies": {"liquid-glass": 1},
                "reference_variants": {"regular": 1},
                "reference_shapes": {"rounded-rectangle": 1},
                "reference_shape_scopes": {"view-bounds": 1},
                "reference_blending_scopes": {"deterministic-fallback": 1},
                "reference_semantic_thickness": {"regular": 1},
                "reference_accessibility_responses": {"standard": 1},
                "reference_performance_responses": {
                    "deterministic-fallback": 1
                },
                "reference_view_bounds_anchored": 1,
                "reference_shape_matches_geometry": 1,
                "reference_tint_applied": 1,
                "reference_interactive_response": 0,
                "reference_container_grouped": 0,
                "reference_container_union": 0,
                "reference_container_morphing": 0,
                "reference_legibility_preserved": 1,
                "reference_vibrancy_expected": 0,
                "reference_deterministic_degradation": 1,
            }
        }

        code, report = self.run_verifier(
            snapshot_with_glass_probe_contract(material_plan()),
            manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])

    def test_command_descriptor_mismatch_points_to_material_command(self) -> None:
        plan = material_plan()
        descriptor = plan["command_descriptor"]
        assert isinstance(descriptor, dict)
        descriptor["saturation"] = 0.42

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material semantic/runtime command descriptors match")
        self.assertEqual(
            failure["path"],
            "debug.material_semantic_runtime_match.command_descriptors")
        self.assertEqual(failure["likely_layer"], "material-command")
        self.assertIn("MaterialCommandDescriptor", failure["suggested_action"])
        self.assertEqual(
            report["material_plans"]["decision_trace"][
                "can_sample_backdrop"],
            0)

    def test_material_shape_mismatch_is_llm_actionable(self) -> None:
        plan = material_plan()
        shape = plan["shape"]
        assert isinstance(shape, dict)
        shape["effective_radius"] = 64.0

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material shape effective radius matches geometry")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".shape.effective_radius")
        self.assertEqual(failure["expected"], 10.0)
        self.assertEqual(failure["actual"], 64.0)
        self.assertEqual(failure["likely_layer"], "material-shape")
        self.assertIn("MaterialPlan.shape", failure["suggested_action"])

    def test_material_shape_kind_mismatch_is_llm_actionable(self) -> None:
        plan = material_plan()
        shape = plan["shape"]
        reference_model = plan["reference_model"]
        assert isinstance(shape, dict)
        assert isinstance(reference_model, dict)
        shape["kind"] = "capsule"
        reference_model["shape"] = "capsule"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material shape kind matches geometry")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".shape.kind")
        self.assertEqual(failure["expected"], "rounded-rectangle")
        self.assertEqual(failure["actual"], "capsule")
        self.assertEqual(failure["likely_layer"], "material-shape")
        self.assertIn("MaterialShapeAnalysis.kind", failure["hint"])

    def test_manifest_can_require_material_shape_summary(self) -> None:
        manifest = {
            "require_material_plan_summary": {
                "shape_valid": 1,
                "shape_rounded": 1,
                "shape_capsule": 0,
                "shape_radius_clamped": 0,
                "shape_max_surface_area_lte": 240.0 * 96.0,
                "shape_max_surface_area_gte": 240.0 * 96.0,
                "shape_max_effective_radius_lte": 10.0,
                "shape_max_effective_radius_gte": 10.0,
                "shape_max_radius_limit_lte": 48.0,
                "shape_max_radius_limit_gte": 48.0,
                "shape_max_normalized_radius_lte": 10.0 / 48.0,
                "shape_max_normalized_radius_gte": 10.0 / 48.0,
            },
        }

        code, report = self.run_verifier(
            snapshot_with_glass_probe_contract(material_plan()),
            manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(
            report["material_plans"]["shape"]["max_radius_limit"],
            48.0)

    def test_command_descriptor_container_allows_resolved_reduce_motion(self) -> None:
        plan = material_plan()
        plan_container = plan["container"]
        descriptor = plan["command_descriptor"]
        verifier_contract = plan["verifier"]
        resource_budget = plan["resource_budget"]
        assert isinstance(plan_container, dict)
        assert isinstance(descriptor, dict)
        assert isinstance(verifier_contract, dict)
        assert isinstance(resource_budget, dict)

        request_container = {
            "mode": "container",
            "container_id": 41,
            "union_id": 0,
            "spacing": 12.0,
            "interactive": False,
            "morph_transitions": True,
        }
        descriptor["container"] = request_container
        plan_container.update({
            **request_container,
            "blend_distance": 12.0,
            "requested_morph_transitions": True,
            "morph_transitions": False,
            "participates": True,
            "shared_backdrop_scope": True,
            "shape_union_expected": False,
            "shape_blending_expected": True,
            "reduced_motion_suppressed_morph": True,
            "spacing_clamped": False,
            "blend_policy": "container-shape-proximity",
            "morph_policy": "reduced-motion-static",
            "performance_policy": "shared-container-capture",
        })
        verifier_contract["require_container_identity"] = True
        verifier_contract["require_container_morph_contract"] = False
        resource_budget["max_container_spacing"] = 12.0
        refresh_reference_model(plan)

        root = snapshot(plan)
        material_node = root["debug"]["semantic_tree"]["children"][0]
        assert isinstance(material_node, dict)
        material = material_node["material"]
        assert isinstance(material, dict)
        material["container"] = request_container
        renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
        assert isinstance(renderer, dict)
        renderer["material_container_groups"] = material_container_group_details(plan)
        renderer["material_runtime_summary"] = material_runtime_summary(plan)

        code, report = self.run_verifier(root)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(report["failures"], [])
        self.assertEqual(
            report["material_plans"]["container"]["morph_transitions"],
            0)
        self.assertEqual(
            report["semantic_tree"]["material_container_morph_transitions"],
            1)

    def test_manifest_can_require_container_group_summary(self) -> None:
        plan = sampled_material_plan()
        plan_container = plan["container"]
        descriptor = plan["command_descriptor"]
        resource_budget = plan["resource_budget"]
        verifier_contract = plan["verifier"]
        interaction = plan["interaction"]
        assert isinstance(plan_container, dict)
        assert isinstance(descriptor, dict)
        assert isinstance(resource_budget, dict)
        assert isinstance(verifier_contract, dict)
        assert isinstance(interaction, dict)
        request_container = {
            "mode": "union",
            "container_id": 41,
            "union_id": 7,
            "spacing": 12.0,
            "interactive": True,
            "morph_transitions": True,
        }
        descriptor["container"] = request_container
        plan_container.update({
            **request_container,
            "blend_distance": 12.0,
            "participates": True,
            "requested_morph_transitions": True,
            "shared_backdrop_scope": True,
            "shape_union_expected": True,
            "shape_blending_expected": True,
            "reduced_motion_suppressed_morph": False,
            "spacing_clamped": False,
            "blend_policy": "union-shape-proximity",
            "morph_policy": "union-morph",
            "performance_policy": "shared-union-capture",
        })
        interaction["enabled"] = True
        interaction["enablement_reason"] = "interactive-container"
        interaction["response_model"] = "liquid-glass-interaction"
        interaction["state"] = "idle"
        resource_budget["max_container_spacing"] = 12.0
        verifier_contract["require_container_identity"] = True
        verifier_contract["require_container_morph_contract"] = True
        refresh_observation_contract(plan)
        refresh_execution_audit(plan)

        root = snapshot(plan)
        material_node = root["debug"]["semantic_tree"]["children"][0]
        assert isinstance(material_node, dict)
        material = material_node["material"]
        assert isinstance(material, dict)
        material["container"] = request_container
        manifest = {
            "require_material_plan_summary": {
                "container_group_count": 1,
                "container_multi_surface_group_count": 0,
                "container_union_group_count": 1,
                "container_morph_group_count": 1,
                "container_interactive_group_count": 1,
                "container_shared_backdrop_scope_group_count": 1,
                "container_shared_capture_surface_count": 1,
                "container_shared_capture_saved_surface_count": 0,
                "container_max_shared_capture_group_surfaces": 1,
                "container_shape_blend_execution_group_count": 0,
                "container_shape_blend_execution_surface_count": 0,
                "container_max_shape_blend_strength": 0.0,
                "container_fallback_mixed_group_count": 0,
                "container_max_group_size": 1,
                "container_max_active_surfaces": 1,
                "container_max_sampled_backdrop_surfaces": 1,
                "container_max_fallback_surfaces": 0,
                "container_total_shape_pair_count": 0,
                "container_blend_candidate_pair_count": 0,
                "container_union_candidate_pair_count": 0,
                "container_morph_candidate_pair_count": 0,
                "container_separated_pair_count": 0,
                "container_min_shape_gap": 0.0,
                "container_max_shape_gap": 0.0,
                "container_max_blend_distance": 12.0,
                "container_max_group_bounds_width": 240.0,
                "container_max_group_bounds_height": 96.0,
                "container_max_group_bounds_area": 240.0 * 96.0,
            },
            "require_runtime_numeric_bounds": [
                {
                    "path": (
                        "renderer.material_executor_summary.container_groups"
                        ".group_count"),
                    "equals": 1,
                },
                {
                    "path": (
                        "renderer.material_executor_summary.container_groups"
                        ".max_sampled_backdrop_surfaces"),
                    "equals": 1,
                },
            ],
        }

        code, report = self.run_verifier(root, manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(
            report["material_plans"]["container_groups"]["group_count"],
            1)
        self.assertEqual(
            report["material_plans"]["container_groups"]["union_group_count"],
            1)

    def test_surface_role_mismatch_points_to_material_contract(self) -> None:
        plan = material_plan()
        plan["role"] = "toolbar"
        descriptor = plan["command_descriptor"]
        assert isinstance(descriptor, dict)
        descriptor["role"] = "toolbar"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material semantic/runtime roles match")
        self.assertEqual(failure["path"], "debug.material_semantic_runtime_match.roles")
        self.assertEqual(failure["likely_layer"], "material-contract")
        self.assertIn("MaterialRect", failure["hint"])

    def test_missing_command_descriptor_is_counted(self) -> None:
        plan = material_plan()
        del plan["command_descriptor"]

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        self.assertEqual(
            report["material_plans"]["command_descriptor_missing"],
            1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material runtime command descriptors are complete")
        self.assertEqual(
            failure["path"],
            "debug.material_semantic_runtime_match.runtime_command_descriptors")
        self.assertEqual(failure["likely_layer"], "material-command")

    def test_material_plan_mismatch_failure_is_llm_actionable(self) -> None:
        code, report = self.run_verifier(snapshot(material_plan(sample_taps=25)))

        self.assertEqual(code, 1)
        self.assertFalse(report["ok"])
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material primary pass sample taps match plan")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".primary_pass.sample_taps")
        self.assertEqual(failure["expected"], 25)
        self.assertEqual(failure["actual"], 0)
        self.assertEqual(failure["likely_layer"], "material.regular.fallback")
        self.assertEqual(failure["likely_pass"], "translucent-rounded-rect")
        self.assertIn("MaterialPlan.sample_taps", failure["hint"])
        self.assertEqual(
            report["material_plans"]["resource_bounds"]["max_plan_sample_taps"],
            25)
        self.assertEqual(report["failure_summary"]["count"], 3)
        self.assertEqual(
            report["failure_summary"]["by_likely_pass"]["translucent-rounded-rect"],
            1)
        self.assertEqual(
            report["failure_summary"]["by_likely_pass"]["material-executor"],
            2)
        self.assertEqual(
            report["failure_summary"]["by_path"][failure["path"]],
            1)

    def test_material_plan_contract_version_mismatch_is_llm_actionable(self) -> None:
        plan = material_plan()
        plan["contract_version"] = verifier.MATERIAL_PLAN_CONTRACT_VERSION + 1

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material plan contract version is supported")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".contract_version")
        self.assertEqual(
            failure["expected"],
            verifier.MATERIAL_PLAN_CONTRACT_VERSION)
        self.assertEqual(
            failure["actual"],
            verifier.MATERIAL_PLAN_CONTRACT_VERSION + 1)
        self.assertEqual(failure["likely_layer"], "material.regular.fallback")
        self.assertIn("MaterialPlan schema version", failure["hint"])
        self.assertIn("plan_material_surface", failure["suggested_action"])
        self.assertEqual(
            report["material_plans"]["contract_versions"],
            {str(verifier.MATERIAL_PLAN_CONTRACT_VERSION + 1): 1})

    def test_optical_composition_mismatch_points_to_pure_contract(self) -> None:
        plan = sampled_material_plan()
        composition = plan["optical_composition"]
        assert isinstance(composition, dict)
        composition["blur_source"] = "fallback-fill"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"]
            == "material optical composition blur_source matches plan")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".optical_composition.blur_source")
        self.assertEqual(failure["expected"], "backdrop-sample-blur")
        self.assertEqual(failure["actual"], "fallback-fill")
        self.assertEqual(failure["likely_layer"], "material-optical-composition")
        self.assertEqual(failure["likely_pass"], "backdrop-sample-blur")
        self.assertIn("MaterialPlan", failure["hint"])

    def test_optical_stage_order_mismatch_points_to_pure_contract(self) -> None:
        plan = sampled_material_plan()
        composition = plan["optical_composition"]
        assert isinstance(composition, dict)
        composition["stage_order"] = "primary"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"]
            == "material optical composition stage_order matches plan")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".optical_composition.stage_order")
        self.assertEqual(failure["expected"], "shadow-primary-edge-noise")
        self.assertEqual(failure["actual"], "primary")
        self.assertEqual(failure["likely_layer"], "material-optical-composition")
        self.assertEqual(failure["likely_pass"], "backdrop-sample-blur")
        self.assertIn("resolved pure MaterialPlan", failure["hint"])

    def test_renderer_contract_version_mismatch_is_llm_actionable(self) -> None:
        root = snapshot(material_plan())
        renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
        assert isinstance(renderer, dict)
        renderer["material_plan_contract_version"] = (
            verifier.MATERIAL_PLAN_CONTRACT_VERSION + 1)

        code, report = self.run_verifier(root)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "renderer material plan contract version is supported"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer"
            ".material_plan_contract_version")
        self.assertEqual(
            failure["expected"],
            verifier.MATERIAL_PLAN_CONTRACT_VERSION)
        self.assertEqual(
            failure["actual"],
            verifier.MATERIAL_PLAN_CONTRACT_VERSION + 1)
        self.assertEqual(failure["likely_layer"], "platform-runtime")
        self.assertIn("backend renderer contract", failure["hint"])
        self.assertIn("debug.platform_runtime.details", failure["suggested_action"])

    def test_manifest_can_require_plan_sample_and_runtime_pass_bounds(self) -> None:
        manifest = {
            "require_material_resource_bounds": {
                "max_plan_sample_taps_lte": 25,
                "max_plan_sample_taps_gte": 25,
                "total_plan_sample_taps_lte": 25,
                "total_plan_sample_taps_gte": 25,
                "max_sampling_kernel_radius_lte": 2,
                "max_sampling_kernel_radius_gte": 2,
                "total_runtime_passes_lte": 1,
                "total_runtime_passes_gte": 1,
                "active_runtime_passes_lte": 1,
                "active_runtime_passes_gte": 1,
                "backdrop_runtime_passes_lte": 1,
                "backdrop_runtime_passes_gte": 1,
                "total_execution_stages_lte": 4,
                "total_execution_stages_gte": 4,
                "active_execution_stages_lte": 4,
                "active_execution_stages_gte": 4,
                "backdrop_execution_stages_lte": 1,
                "backdrop_execution_stages_gte": 1,
                "dropped_execution_stages_lte": 0,
                "dropped_execution_stages_gte": 0,
                "max_execution_stage_count_lte": 4,
                "max_execution_stage_count_gte": 4,
                "max_execution_stage_capacity_lte": 4,
                "max_execution_stage_capacity_gte": 4,
                "max_execution_stages_lte": 4,
                "max_execution_stages_gte": 4,
                "total_paint_layers_lte": 0,
                "total_paint_layers_gte": 0,
                "max_paint_layers_lte": 3,
                "max_paint_layers_gte": 3,
                "max_paint_layer_capacity_lte": 3,
                "max_paint_layer_capacity_gte": 3,
                "max_paint_layer_inflate_lte": 0,
                "max_paint_layer_inflate_gte": 0,
                "max_frame_capture_count_lte": 1,
                "max_frame_capture_count_gte": 1,
                "max_frame_capture_pixels_lte": 320 * 240,
                "max_frame_capture_pixels_gte": 320 * 240,
                "total_surface_sample_pixels_lte": 240 * 96,
                "total_surface_sample_pixels_gte": 240 * 96,
                "max_surface_sample_pixels_lte": 240 * 96,
                "max_surface_sample_pixels_gte": 240 * 96,
                "max_pass_texture_copy_pixels_lte": 320 * 240,
                "total_pass_texture_copy_pixels_lte": 320 * 240,
            }
        }
        code, report = self.run_verifier(
            snapshot(sampled_material_plan(sample_taps=25)),
            manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(
            report["material_plans"]["resource_bounds"]["max_plan_sample_taps"],
            25)
        self.assertEqual(
            report["material_plans"]["resource_bounds"]["total_plan_sample_taps"],
            25)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "max_sampling_kernel_radius"],
            2)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "total_execution_stages"],
            4)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "max_execution_stages"],
            4)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "dropped_execution_stages"],
            0)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "max_execution_stage_capacity"],
            4)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "total_paint_layers"],
            0)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "max_paint_layers"],
            3)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "max_paint_layer_capacity"],
            3)
        self.assertEqual(
            report["material_plans"]["backdrop_access"]["required"],
            1)
        self.assertEqual(
            report["material_plans"]["backdrop_access"]["capture_scopes"],
            {"shared-frame": 1})
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "max_frame_capture_pixels"],
            320 * 240)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "total_surface_sample_pixels"],
            240 * 96)
        self.assertEqual(report["manifest"]["material_resource_bounds"], 44)
        self.assertEqual(
            report["manifest"]["material_resource_bound_fields"],
            [
                "active_execution_stages",
                "active_runtime_passes",
                "backdrop_execution_stages",
                "backdrop_runtime_passes",
                "dropped_execution_stages",
                "max_execution_stage_capacity",
                "max_execution_stage_count",
                "max_execution_stages",
                "max_frame_capture_count",
                "max_frame_capture_pixels",
                "max_paint_layer_capacity",
                "max_paint_layer_inflate",
                "max_paint_layers",
                "max_pass_texture_copy_pixels",
                "max_plan_sample_taps",
                "max_sampling_kernel_radius",
                "max_surface_sample_pixels",
                "total_execution_stages",
                "total_paint_layers",
                "total_pass_texture_copy_pixels",
                "total_plan_sample_taps",
                "total_runtime_passes",
                "total_surface_sample_pixels",
            ])
        resource_results = report["material_plans"]["resource_bound_results"]
        self.assertEqual(len(resource_results), 44)
        by_key = {item["key"]: item for item in resource_results}
        self.assertEqual(
            by_key["max_plan_sample_taps_lte"]["field"],
            "max_plan_sample_taps")
        self.assertEqual(by_key["max_plan_sample_taps_lte"]["bound"], "lte")
        self.assertEqual(by_key["max_plan_sample_taps_lte"]["expected"], 25)
        self.assertEqual(by_key["max_plan_sample_taps_lte"]["actual"], 25)
        self.assertTrue(by_key["max_plan_sample_taps_lte"]["ok"])
        self.assertEqual(by_key["max_plan_sample_taps_lte"]["margin"], 0.0)
        self.assertEqual(
            report["material_plans"]["resource_bound_summary"],
            {
                "bound_count": 44,
                "pass_count": 44,
                "fail_count": 0,
                "failed_keys": [],
                "tightest_bound_key": "max_plan_sample_taps_lte",
                "tightest_bound_field": "max_plan_sample_taps",
                "tightest_bound_margin": 0.0,
            })

    def test_material_resource_bound_result_failure_is_llm_actionable(self) -> None:
        manifest = {
            "require_material_resource_bounds": {
                "max_plan_sample_taps_lte": 0,
            }
        }
        code, report = self.run_verifier(
            snapshot(sampled_material_plan(sample_taps=25)),
            manifest)

        self.assertEqual(code, 1)
        summary = report["material_plans"]["resource_bound_summary"]
        self.assertEqual(summary["bound_count"], 1)
        self.assertEqual(summary["fail_count"], 1)
        self.assertEqual(summary["failed_keys"], ["max_plan_sample_taps_lte"])
        result = report["material_plans"]["resource_bound_results"][0]
        self.assertEqual(result["field"], "max_plan_sample_taps")
        self.assertEqual(result["bound"], "lte")
        self.assertEqual(result["expected"], 0)
        self.assertEqual(result["actual"], 25)
        self.assertFalse(result["ok"])
        self.assertEqual(result["margin"], -25.0)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material resource bound max_plan_sample_taps is within limit"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans"
            "#resource_bounds.max_plan_sample_taps")
        self.assertEqual(failure["expected"], {"<=": 0})
        self.assertEqual(failure["actual"], 25)

    def test_manifest_can_require_execution_stage_summary(self) -> None:
        manifest = {
            "require_material_plan_summary": {
                "stage_names": {
                    "shape-shadow": 1,
                    "translucent-rounded-rect": 1,
                    "edge-highlight": 1,
                },
                "stage_executors": {
                    "shape-shadow": 1,
                    "fallback-fill": 1,
                    "edge-highlight": 1,
                },
                "paint_layer_names": {
                    "fallback-shadow": 1,
                    "translucent-rounded-rect": 1,
                    "fallback-edge-highlight": 1,
                },
                "paint_layer_executors": {
                    "rounded-shadow": 1,
                    "rounded-fill": 1,
                    "rounded-edge": 1,
                },
            },
        }
        code, report = self.run_verifier(snapshot(material_plan()), manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(
            report["material_plans"]["stage_names"],
            {
                "shape-shadow": 1,
                "translucent-rounded-rect": 1,
                "edge-highlight": 1,
            })
        self.assertEqual(
            report["material_plans"]["stage_executors"],
            {
                "shape-shadow": 1,
                "fallback-fill": 1,
                "edge-highlight": 1,
            })
        self.assertEqual(
            report["material_plans"]["paint_layer_executors"],
            {"rounded-shadow": 1, "rounded-fill": 1, "rounded-edge": 1})

    def test_manifest_can_require_backdrop_access_summary(self) -> None:
        manifest = {
            "require_material_plan_summary": {
                "backdrop_access_required": 1,
                "backdrop_access_stable_required": 1,
                "backdrop_access_frame_history_required": 1,
                "backdrop_access_shared_frame_capture": 1,
                "backdrop_access_bounded": 1,
                "backdrop_access_sources": {
                    "previous-presented-frame": 1,
                },
                "backdrop_capture_scopes": {
                    "shared-frame": 1,
                },
            },
        }

        code, report = self.run_verifier(
            snapshot(sampled_material_plan()),
            manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(
            report["material_plans"]["backdrop_access"][
                "shared_frame_capture"],
            1)

    def test_missing_primary_execution_stage_points_to_stage_contract(self) -> None:
        plan = sampled_material_plan(sample_taps=25)
        plan["execution_stages"] = []

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        primary_failure = next(
            item for item in report["failures"]
            if item["name"] == "material execution stages include primary pass")
        self.assertEqual(
            primary_failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".execution_stages")
        self.assertEqual(primary_failure["likely_pass"], "backdrop-sample-blur")
        self.assertIn("primary render pass", primary_failure["hint"])
        backdrop_failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material backdrop sampling has backdrop execution stage"))
        self.assertEqual(backdrop_failure["likely_pass"], "backdrop-sample-blur")
        self.assertIn("Backdrop sampling", backdrop_failure["hint"])

    def test_dropped_execution_stage_points_to_stage_capacity(self) -> None:
        plan = sampled_material_plan()
        plan["dropped_execution_stage_count"] = 1
        root = snapshot(plan)
        renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
        assert isinstance(renderer, dict)
        runtime = renderer["material_runtime_summary"]
        executor = renderer["material_executor_summary"]
        assert isinstance(runtime, dict)
        assert isinstance(executor, dict)
        runtime["dropped_execution_stages"] = 1
        executor["dropped_execution_stage_count"] = 1

        code, report = self.run_verifier(root)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material execution stages did not overflow capacity"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".dropped_execution_stage_count")
        self.assertEqual(failure["expected"], 0)
        self.assertEqual(failure["actual"], 1)
        self.assertEqual(failure["likely_layer"], "material-stage")
        self.assertEqual(failure["likely_pass"], "stage-capacity")
        self.assertIn("material_max_execution_stages", failure["hint"])

    def test_fallback_backdrop_execution_stage_points_to_stage_contract(self) -> None:
        plan = material_plan()
        stages = plan["execution_stages"]
        assert isinstance(stages, list)
        stages.append({
            "name": "noise-dither",
            "active": True,
            "requires_backdrop": True,
            "sample_taps": 0,
            "likely_layer": "material-noise-pass",
            "executor": "ordered-dither",
            "max_texture_copy_pixels": 1,
            "bounded": True,
        })

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material fallback execution stages avoid backdrop work"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".execution_stages")
        self.assertEqual(failure["likely_pass"], "translucent-rounded-rect")
        self.assertIn("Fallback material stages", failure["hint"])

    def test_sampling_kernel_mismatch_is_llm_actionable(self) -> None:
        plan = sampled_material_plan(sample_taps=13)
        assert isinstance(plan["sampling_kernel"], dict)
        plan["sampling_kernel"]["sample_taps"] = 25

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material sampling kernel taps match plan")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".sampling_kernel.sample_taps")
        self.assertEqual(failure["expected"], 13)
        self.assertEqual(failure["actual"], 25)
        self.assertEqual(failure["likely_pass"], "sampling-kernel")
        self.assertIn("MaterialPlan.sampling_kernel", failure["suggested_action"])

    def test_center_sampling_kernel_accepts_zero_radius(self) -> None:
        plan = sampled_material_plan(sample_taps=1)

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 0)
        self.assertEqual(
            report["material_plans"]["sampling_kernels"],
            {"weighted-center": 1})
        self.assertEqual(
            report["material_plans"]["resource_bounds"]
            ["max_sampling_kernel_radius"],
            0)

    def test_sampling_kernel_radius_mismatch_is_llm_actionable(self) -> None:
        plan = sampled_material_plan(sample_taps=5)
        assert isinstance(plan["sampling_kernel"], dict)
        plan["sampling_kernel"]["radius"] = 2

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material sampling kernel radius matches tap tier"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".sampling_kernel.radius")
        self.assertEqual(failure["expected"], 1)
        self.assertEqual(failure["actual"], 2)
        self.assertEqual(failure["likely_pass"], "sampling-kernel")
        self.assertIn("shader radius", failure["hint"])

    def test_sampling_kernel_rejects_non_tier_taps(self) -> None:
        plan = sampled_material_plan(sample_taps=5)
        plan["sample_taps"] = 7
        refresh_observation_contract(plan)
        refresh_execution_audit(plan)

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material sample taps use executable tap tier"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".sample_taps")
        self.assertEqual(failure["expected"], [0, 1, 5, 9, 13, 25])
        self.assertEqual(failure["actual"], 7)
        self.assertIn("material_resolve_sample_taps", failure["hint"])

    def test_luminance_curve_mismatch_is_llm_actionable(self) -> None:
        plan = sampled_material_plan(sample_taps=25)
        assert isinstance(plan["luminance_curve"], dict)
        plan["luminance_curve"]["name"] = "fallback-flat"
        plan["luminance_curve"]["backdrop_driven"] = False

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material sampled backdrop uses adaptive luminance curve"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".luminance_curve")
        self.assertEqual(failure["likely_pass"], "luminance-curve")
        self.assertIn("MaterialPlan.luminance_curve", failure["suggested_action"])

    def test_optical_response_mismatch_is_llm_actionable(self) -> None:
        plan = sampled_material_plan(sample_taps=25)
        assert isinstance(plan["optical_response"], dict)
        plan["optical_response"]["response_model"] = "deterministic-fallback"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material optical response model matches role and sampling"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".optical_response.response_model")
        self.assertEqual(failure["expected"], "sampled-backdrop")
        self.assertEqual(failure["actual"], "deterministic-fallback")
        self.assertEqual(failure["likely_layer"], "material-optical-response")
        self.assertIn("MaterialPlan.optical_response", failure["suggested_action"])

    def test_interaction_summary_and_optical_contract_are_reported(self) -> None:
        plan = material_plan()
        container = plan["container"]
        descriptor = plan["command_descriptor"]
        interaction = plan["interaction"]
        assert isinstance(container, dict)
        assert isinstance(descriptor, dict)
        assert isinstance(descriptor["container"], dict)
        assert isinstance(interaction, dict)
        container["interactive"] = True
        descriptor["container"]["interactive"] = True
        descriptor["interaction"] = {
            "hovered": True,
            "pressed": False,
            "focused": True,
            "pointer_inside": True,
            "active": True,
            "pointer_x": 0.62,
            "pointer_y": 0.38,
        }
        interaction.update({
            "enabled": True,
            "active": True,
            "hovered": True,
            "pressed": False,
            "focused": True,
            "pointer_inside": True,
            "pointer_x": 0.62,
            "pointer_y": 0.38,
            "response_strength": 0.58,
            "opacity_delta": 0.03,
            "blur_radius_delta": 1.6,
            "saturation_delta": 0.04,
            "edge_highlight_delta": 0.09,
            "shadow_alpha_delta": 0.02,
            "shadow_radius_delta": 2.4,
            "state": "hovered",
            "enablement_reason": "interactive-container",
            "response_model": "liquid-glass-interaction",
            "motion_policy": "animated-optical-response",
            "specular_model": "pointer-specular",
            "specular_highlight_active": True,
            "specular_anchor_x": 0.62,
            "specular_anchor_y": 0.38,
            "specular_radius": 0.34,
            "specular_intensity": 0.24,
        })
        stages = plan["execution_stages"]
        assert isinstance(stages, list)
        edge_optics = stages[2]["optics"]
        assert isinstance(edge_optics, dict)
        edge_optics.update({
            "specular_model": "pointer-specular",
            "specular_anchor_x": 0.62,
            "specular_anchor_y": 0.38,
            "specular_radius": 0.34,
            "specular_intensity": 0.24,
        })
        refresh_observation_contract(plan)
        refresh_execution_audit(plan)
        manifest = {
            "require_material_plan_summary": {
                "interaction_enabled": 1,
                "interaction_active": 1,
                "interaction_hovered": 1,
                "interaction_focused": 1,
                "interaction_pointer_inside": 1,
                "interaction_specular_highlight_active": 1,
                "interaction_deterministic": 1,
                "interaction_states": {"hovered": 1},
                "interaction_enablement_reasons": {
                    "interactive-container": 1,
                },
                "interaction_response_models": {
                    "liquid-glass-interaction": 1,
                },
                "interaction_motion_policies": {
                    "animated-optical-response": 1,
                },
                "interaction_specular_models": {
                    "pointer-specular": 1,
                },
                "optical_interaction_active": 1,
                "optical_interaction_modulates_optics": 1,
            },
        }

        root = snapshot(plan)
        material_node = root["debug"]["semantic_tree"]["children"][0]
        assert isinstance(material_node, dict)
        material = material_node["material"]
        assert isinstance(material, dict)
        material["container"] = descriptor["container"]

        code, report = self.run_verifier(root, manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(report["material_plans"]["interaction"]["enabled"], 1)
        self.assertEqual(report["material_plans"]["interaction"]["states"], {
            "hovered": 1,
        })
        self.assertEqual(
            report["material_plans"]["interaction"]["enablement_reasons"],
            {"interactive-container": 1})
        self.assertEqual(
            report["material_plans"]["interaction"]["specular_models"],
            {"pointer-specular": 1})
        self.assertEqual(
            report["material_plans"]["interaction"]["specular_highlight_active"],
            1)
        self.assertEqual(
            report["material_plans"]["optical_response"][
                "interaction_modulates_optics"],
            1)

    def test_interaction_enablement_reason_mismatch_is_llm_actionable(self) -> None:
        plan = material_plan()
        interaction = plan["interaction"]
        assert isinstance(interaction, dict)
        interaction["enablement_reason"] = "interactive-container"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material interaction enablement_reason mirrors container "
                "eligibility"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".interaction.enablement_reason")
        self.assertEqual(failure["expected"], "noninteractive-container")
        self.assertEqual(failure["actual"], "interactive-container")
        self.assertEqual(failure["likely_layer"], "material-interaction")
        self.assertIn(
            "MaterialPlan.interaction.enablement_reason",
            failure["suggested_action"])

    def test_interaction_specular_model_mismatch_is_llm_actionable(self) -> None:
        plan = material_plan()
        interaction = plan["interaction"]
        assert isinstance(interaction, dict)
        interaction["specular_model"] = "pointer-specular"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material interaction specular_model matches highlight state"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".interaction.specular_model")
        self.assertEqual(failure["expected"], "none")
        self.assertEqual(failure["actual"], "pointer-specular")
        self.assertEqual(failure["likely_layer"], "material-interaction")
        self.assertIn("MaterialPlan.interaction", failure["suggested_action"])

    def test_interaction_pointer_failure_points_to_material_interaction(self) -> None:
        plan = material_plan()
        interaction = plan["interaction"]
        assert isinstance(interaction, dict)
        interaction["pointer_x"] = 1.5

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material interaction pointer_x is normalized")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".interaction.pointer_x")
        self.assertEqual(failure["likely_layer"], "material-interaction")
        self.assertIn("MaterialPlan.interaction", failure["suggested_action"])

    def test_manifest_can_require_fallback_reason_summary(self) -> None:
        manifest = {
            "require_material_plan_summary": {
                "fallback_reasons": {
                    "backend reports no material backdrop blur support": 1,
                },
            },
        }
        code, report = self.run_verifier(snapshot(material_plan()), manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(
            report["material_plans"]["fallback_reasons"],
            {"backend reports no material backdrop blur support": 1})

    def test_manifest_can_require_backdrop_analysis_summary(self) -> None:
        manifest = {
            "require_material_plan_summary": {
                "backdrop_sampling": 0,
                "backdrop_available": 0,
                "backdrop_stable": 0,
                "contract_versions": {
                    str(verifier.MATERIAL_PLAN_CONTRACT_VERSION): 1,
                },
                "roles": {"surface": 1},
                "backdrop_sources": {"none": 1},
                "luminance_responses": {"not-sampled": 1},
                "frosting_responses": {"not-sampled": 1},
                "tint_responses": {"not-sampled": 1},
                "saturation_responses": {"not-sampled": 1},
                "depth_responses": {"not-sampled": 1},
                "luminance_curves": {"fallback-flat": 1},
                "luminance_adapted": 0,
                "backdrop_optical_adapted": 0,
                "optical_response_models": {"deterministic-fallback": 1},
                "optical_blur_strategies": {"fallback-fill": 1},
                "optical_color_strategies": {"fallback-solid-color": 1},
                "optical_depth_strategies": {"fallback-shadow-edge": 1},
                "optical_backdrop_driven": 0,
                "optical_blur_active": 0,
                "optical_frosting_active": 0,
                "optical_tint_active": 1,
                "optical_saturation_active": 0,
                "optical_luminance_preservation_active": 1,
                "optical_edge_highlight_active": 1,
                "optical_depth_shadow_active": 1,
                "optical_noise_dither_active": 0,
                "optical_foreground_vibrancy_active": 0,
                "optical_deterministic_fallback": 1,
                "render_target_ready": 1,
                "render_target_within_backdrop_budget": 1,
                "render_target_pixel_formats": {"rgba8unorm": 1},
                "theme_default_glass_tokens": 1,
                "theme_profile_names": {"apple-glass-light": 1},
                "theme_sources": {"material-style": 1},
                "theme_token_policies": {
                    "explicit-material-style-tokens": 1,
                },
                "pass_executors": {"fallback-fill": 1},
                "decision_can_sample_backdrop": 0,
                "decision_backend_supports_backdrop": 0,
                "decision_backdrop_source_ready": 0,
                "decision_reduced_transparency": 0,
                "decision_increase_contrast": 0,
                "decision_reduce_motion": 0,
                "decision_blockers": {"unsupported-backend": 1},
                "verifier_require_backdrop_source": 0,
                "verifier_require_edge_highlight": 0,
                "verifier_profiles": {"regular-legibility-backdrop": 1},
                "verifier_region_layers": {
                    "regular-legibility-backdrop": "material-fallback-pass",
                },
                "verifier_region_passes": {
                    "regular-legibility-backdrop": "translucent-rounded-rect",
                },
            },
        }
        code, report = self.run_verifier(snapshot(material_plan()), manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(
            report["material_plans"]["backdrop"]["sources"],
            {"none": 1})
        self.assertEqual(
            report["material_plans"]["backdrop"]["luminance_responses"],
            {"not-sampled": 1})
        self.assertEqual(
            report["material_plans"]["luminance_curves"],
            {"fallback-flat": 1})
        self.assertEqual(
            report["material_plans"]["optical_response"]["response_models"],
            {"deterministic-fallback": 1})
        self.assertEqual(
            report["material_plans"]["optical_response"]["blur_strategies"],
            {"fallback-fill": 1})
        self.assertEqual(
            report["material_plans"]["render_target"]["pixel_formats"],
            {"rgba8unorm": 1})
        self.assertEqual(
            report["material_plans"]["decision_trace"]["first_blockers"],
            {"unsupported-backend": 1})
        self.assertEqual(
            report["material_plans"]["verifier_profiles"],
            {"regular-legibility-backdrop": 1})
        self.assertEqual(
            report["material_plans"]["region_passes"],
            {"regular-legibility-backdrop": "translucent-rounded-rect"})
        self.assertEqual(
            report["material_plans"]["region_layers"],
            {"regular-legibility-backdrop": "material-fallback-pass"})

    def test_manifest_can_require_material_surface_role(self) -> None:
        plan = material_plan()
        plan["role"] = "control"
        assert isinstance(plan["command_descriptor"], dict)
        plan["command_descriptor"]["role"] = "control"
        root = snapshot(plan)
        semantic_material = root["debug"]["semantic_tree"]["children"][0]["material"]
        assert isinstance(semantic_material, dict)
        semantic_material["role"] = "control"
        manifest = {
            "require_material_surface_roles": ["control"],
            "require_material_plan_summary": {
                "roles": {"control": 1},
            },
        }
        code, report = self.run_verifier(root, manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])

    def test_decision_trace_blocker_mismatch_points_to_plan_policy(self) -> None:
        plan = material_plan()
        trace = plan["decision_trace"]
        assert isinstance(trace, dict)
        trace["first_blocker"] = "none"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material decision blocker matches fallback path"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".decision_trace.first_blocker")
        self.assertEqual(failure["expected"], "unsupported-backend")
        self.assertEqual(failure["actual"], "none")
        self.assertEqual(failure["likely_layer"], "material.regular.fallback")
        self.assertIn("first decision blocker", failure["hint"])

    def test_verifier_expectation_mismatch_points_to_region_contract(self) -> None:
        plan = material_plan()
        verifier_contract = plan["verifier"]
        assert isinstance(verifier_contract, dict)
        verifier_contract["require_backdrop_source"] = True

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material verifier backdrop-source requirement is derived"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".verifier.require_backdrop_source")
        self.assertEqual(failure["expected"], False)
        self.assertEqual(failure["actual"], True)
        self.assertEqual(failure["likely_layer"], "material.regular.fallback")
        self.assertIn("backdrop_sampling decision", failure["hint"])

    def test_verifier_layer_mismatch_points_to_primary_pass(self) -> None:
        plan = material_plan()
        verifier_contract = plan["verifier"]
        assert isinstance(verifier_contract, dict)
        verifier_contract["likely_layer"] = "material-blur-pass"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material verifier likely layer matches primary pass"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".verifier.likely_layer")
        self.assertEqual(failure["expected"], "material-fallback-pass")
        self.assertEqual(failure["actual"], "material-blur-pass")
        self.assertEqual(failure["likely_pass"], "translucent-rounded-rect")
        self.assertIn("primary_pass", failure["hint"])

    def test_verifier_pass_mismatch_points_to_primary_pass(self) -> None:
        plan = material_plan()
        verifier_contract = plan["verifier"]
        assert isinstance(verifier_contract, dict)
        verifier_contract["likely_pass"] = "backdrop-sample-blur"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material verifier likely pass matches primary pass"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".verifier.likely_pass")
        self.assertEqual(failure["expected"], "translucent-rounded-rect")
        self.assertEqual(failure["actual"], "backdrop-sample-blur")
        self.assertEqual(failure["likely_pass"], "translucent-rounded-rect")
        self.assertIn("primary_pass", failure["hint"])

    def test_observation_contract_mismatch_points_to_pure_contract(self) -> None:
        plan = sampled_material_plan()
        observation = plan["observation_contract"]
        assert isinstance(observation, dict)
        observation["expected_backdrop_execution_stages"] = 0

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material observation expected_backdrop_execution_stages "
                "matches stages"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".observation_contract.expected_backdrop_execution_stages")
        self.assertEqual(failure["expected"], 1)
        self.assertEqual(failure["actual"], 0)
        self.assertEqual(failure["likely_layer"], "material-observation")
        self.assertEqual(failure["likely_pass"], "backdrop-sample-blur")
        self.assertIn("Observation stage counts", failure["hint"])

    def test_observation_runtime_pass_mismatch_points_to_pure_contract(
            self) -> None:
        plan = sampled_material_plan()
        observation = plan["observation_contract"]
        assert isinstance(observation, dict)
        observation["expected_active_runtime_passes"] = 0

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material observation expected_active_runtime_passes "
                "matches passes"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".observation_contract.expected_active_runtime_passes")
        self.assertEqual(failure["expected"], 1)
        self.assertEqual(failure["actual"], 0)
        self.assertEqual(failure["likely_layer"], "material-observation")
        self.assertEqual(failure["likely_pass"], "backdrop-sample-blur")
        self.assertIn("active/backdrop pass counts", failure["hint"])

    def test_execution_audit_mismatch_points_to_execution_contract(self) -> None:
        plan = sampled_material_plan()
        audit = plan["execution_audit"]
        assert isinstance(audit, dict)
        audit["actual_backdrop_execution_stages"] = 0

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material execution audit actual_backdrop_execution_stages "
                "matches stages"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".execution_audit.actual_backdrop_execution_stages")
        self.assertEqual(failure["expected"], 1)
        self.assertEqual(failure["actual"], 0)
        self.assertEqual(failure["likely_layer"], "material-execution-contract")
        self.assertEqual(failure["likely_pass"], "backdrop-sample-blur")
        self.assertIn("stage counts", failure["hint"])

    def test_execution_stage_order_mismatch_points_to_stage_contract(self) -> None:
        plan = sampled_material_plan()
        stages = plan["execution_stages"]
        assert isinstance(stages, list)
        stages[0], stages[1] = stages[1], stages[0]

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material execution audit actual stage order matches stages"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".execution_audit.actual_stage_order")
        self.assertEqual(failure["expected"], "unexpected-stage-order")
        self.assertEqual(failure["actual"], "shadow-primary-edge-noise")
        self.assertEqual(failure["likely_layer"], "material-execution-contract")
        self.assertEqual(failure["likely_pass"], "backdrop-sample-blur")
        self.assertIn("execution_stages", failure["hint"])

    def test_fallback_pass_texture_copy_points_to_pass_contract(self) -> None:
        plan = material_plan()
        primary = plan["primary_pass"]
        assert isinstance(primary, dict)
        primary["max_texture_copy_pixels"] = 1

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material non-backdrop primary pass has no texture copy"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".primary_pass.max_texture_copy_pixels")
        self.assertEqual(failure["expected"], 0)
        self.assertEqual(failure["actual"], 1)
        self.assertEqual(failure["likely_pass"], "translucent-rounded-rect")
        self.assertIn("Fallback/fill passes", failure["hint"])

    def test_render_target_pixel_mismatch_points_to_plan_metadata(self) -> None:
        plan = material_plan()
        target = plan["render_target"]
        assert isinstance(target, dict)
        target["pixel_count"] = 1

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material render target pixel count matches dimensions"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".render_target.pixel_count")
        self.assertEqual(failure["expected"], 320 * 240)
        self.assertEqual(failure["actual"], 1)
        self.assertEqual(failure["likely_layer"], "material.regular.fallback")
        self.assertIn("width * height", failure["hint"])

    def test_unknown_backdrop_response_points_to_backdrop_policy(self) -> None:
        plan = material_plan()
        backdrop = plan["backdrop"]
        assert isinstance(backdrop, dict)
        backdrop["luminance_response"] = "mystery"

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material backdrop luminance response is known")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".backdrop.luminance_response")
        self.assertEqual(failure["likely_layer"], "material.regular.fallback")
        self.assertIn("verifier vocabulary", failure["hint"])

    def test_fallback_reason_summary_failure_points_to_material_plan(self) -> None:
        manifest = {
            "require_material_plan_summary": {
                "fallback_reasons": {
                    "quality policy backdrop pixel budget exceeded": 1,
                },
            },
        }
        code, report = self.run_verifier(snapshot(material_plan()), manifest)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material plan summary fallback_reasons matches")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans#summary"
            ".fallback_reasons")
        self.assertEqual(failure["likely_layer"], "material-plan")
        self.assertIn("MaterialPlan.fallback_reason", failure["hint"])
        self.assertIn("plan_material_surface", failure["suggested_action"])
        self.assertIn("MaterialPlan fields", failure["suggested_action"])

    def test_contract_version_summary_failure_points_to_material_plan(self) -> None:
        manifest = {
            "require_material_plan_summary": {
                "contract_versions": {
                    str(verifier.MATERIAL_PLAN_CONTRACT_VERSION + 1): 1,
                },
            },
        }
        code, report = self.run_verifier(snapshot(material_plan()), manifest)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material plan summary contract_versions matches")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans#summary"
            ".contract_versions")
        self.assertEqual(failure["likely_layer"], "material-plan")
        self.assertIn("MaterialPlan.contract_version", failure["hint"])
        self.assertIn("plan_material_surface", failure["suggested_action"])

    def test_non_fallback_reason_failure_is_llm_actionable(self) -> None:
        plan = material_plan()
        plan["fallback"] = False
        plan["fallback_path"] = "none"
        plan["fallback_reason"] = "stale fallback reason"
        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material non-fallback reason is empty")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".fallback_reason")
        self.assertEqual(failure["expected"], "empty string")
        self.assertEqual(failure["actual"], "stale fallback reason")
        self.assertIn("stale fallback metadata", failure["hint"])

    def test_manifest_can_require_quality_policy_pixel_budget(self) -> None:
        manifest = {
            "require_material_quality_policy": {
                "require_backdrop_sampling_allowed": True,
                "require_noise_allowed": True,
                "require_shadow_allowed": True,
                "max_blur_radius_lte": 36.0,
                "max_sample_taps_lte": 25,
                "max_backdrop_pixels_lte": 4_000_000,
            },
        }
        code, report = self.run_verifier(snapshot(material_plan()), manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(
            report["material_plans"]["quality_policy"]["max_backdrop_pixels"],
            4_000_000)
        self.assertEqual(report["manifest"]["material_quality_policy_bounds"], 6)
        self.assertEqual(
            report["manifest"]["material_quality_policy_fields"],
            [
                "backdrop_sampling_disabled",
                "max_backdrop_pixels",
                "max_blur_radius",
                "max_sample_taps",
                "noise_disabled",
                "shadow_disabled",
            ])
        quality_results = report["material_plans"]["quality_policy_bound_results"]
        self.assertEqual(len(quality_results), 6)
        by_key = {item["key"]: item for item in quality_results}
        self.assertEqual(
            by_key["max_backdrop_pixels_lte"]["field"],
            "max_backdrop_pixels")
        self.assertEqual(by_key["max_backdrop_pixels_lte"]["bound"], "lte")
        self.assertEqual(
            by_key["max_backdrop_pixels_lte"]["expected"],
            4_000_000)
        self.assertEqual(
            by_key["max_backdrop_pixels_lte"]["actual"],
            4_000_000)
        self.assertTrue(by_key["max_backdrop_pixels_lte"]["ok"])
        self.assertEqual(
            report["material_plans"]["quality_policy_bound_summary"],
            {
                "bound_count": 6,
                "pass_count": 6,
                "fail_count": 0,
                "failed_keys": [],
                "tightest_bound_key": "max_blur_radius_lte",
                "tightest_bound_field": "max_blur_radius",
                "tightest_bound_margin": 0.0,
            })

    def test_material_quality_policy_bound_result_failure_is_llm_actionable(
            self) -> None:
        manifest = {
            "require_material_quality_policy": {
                "max_blur_radius_lte": 1.0,
            },
        }
        code, report = self.run_verifier(snapshot(material_plan()), manifest)

        self.assertEqual(code, 1)
        summary = report["material_plans"]["quality_policy_bound_summary"]
        self.assertEqual(summary["bound_count"], 1)
        self.assertEqual(summary["fail_count"], 1)
        self.assertEqual(summary["failed_keys"], ["max_blur_radius_lte"])
        result = report["material_plans"]["quality_policy_bound_results"][0]
        self.assertEqual(result["field"], "max_blur_radius")
        self.assertEqual(result["bound"], "lte")
        self.assertEqual(result["expected"], 1.0)
        self.assertEqual(result["actual"], 36.0)
        self.assertFalse(result["ok"])
        self.assertEqual(result["margin"], -35.0)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material quality policy max_blur_radius is within limit"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans"
            "#quality_policy.max_blur_radius")
        self.assertEqual(failure["expected"], {"<=": 1.0})
        self.assertEqual(failure["actual"], 36.0)

    def test_manifest_can_require_runtime_numeric_bounds(self) -> None:
        manifest = {
            "require_runtime_numeric_bounds": [
                {
                    "path": "renderer.material_executor_summary.plan_count",
                    "equals": 1,
                },
                {
                    "path": "renderer.material_executor_summary.cpu_total_ns",
                    "gte": 1,
                    "lte": 500,
                },
            ],
        }
        code, report = self.run_verifier(snapshot(material_plan()), manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(report["manifest"]["runtime_numeric_bounds"], 2)

    def test_manifest_can_require_material_executor_budget_bounds(self) -> None:
        manifest = {
            "require_material_executor_budget": {
                "draw_calls_gte": 1,
                "max_sample_taps_equals": 13,
                "upload_utilization_gte": 1.0,
                "upload_utilization_lte": 1.0,
                "backdrop_copy_utilization_lte": 0.0,
            },
            "require_material_executor_budget_coverage": {
                "required_fields": [
                    "backdrop_copy_utilization",
                    "draw_calls",
                    "max_sample_taps",
                    "upload_utilization",
                ],
                "min_bound_key_count": 5,
                "min_guarded_field_count": 4,
                "min_observed_field_count": 21,
            },
        }
        code, report = self.run_verifier(
            snapshot(sampled_material_plan(sample_taps=13)),
            manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(report["manifest"]["material_executor_budget_bounds"], 5)
        self.assertEqual(
            report["manifest"]["material_executor_budget_fields"],
            [
                "backdrop_copy_utilization",
                "draw_calls",
                "max_sample_taps",
                "upload_utilization",
            ])
        self.assertEqual(
            report["manifest"]
            ["material_executor_budget_coverage_required_fields"],
            [
                "backdrop_copy_utilization",
                "draw_calls",
                "max_sample_taps",
                "upload_utilization",
            ])
        budget = (
            report["artifact_context"]
            ["material_contract"]
            ["executor_budget"])
        self.assertEqual(budget["upload_utilization"], 1.0)
        self.assertEqual(budget["backdrop_copy_utilization"], 0.0)
        results = (
            report["artifact_context"]
            ["material_contract"]
            ["executor_budget_bound_results"])
        self.assertEqual(
            [item["key"] for item in results],
            [
                "backdrop_copy_utilization_lte",
                "draw_calls_gte",
                "max_sample_taps_equals",
                "upload_utilization_gte",
                "upload_utilization_lte",
            ])
        by_key = {item["key"]: item for item in results}
        self.assertEqual(
            by_key["upload_utilization_lte"]["field"],
            "upload_utilization")
        self.assertEqual(by_key["upload_utilization_lte"]["bound"], "lte")
        self.assertEqual(by_key["upload_utilization_lte"]["expected"], 1.0)
        self.assertEqual(by_key["upload_utilization_lte"]["actual"], 1.0)
        self.assertTrue(by_key["upload_utilization_lte"]["ok"])
        self.assertEqual(by_key["upload_utilization_lte"]["margin"], 0.0)
        summary = (
            report["artifact_context"]
            ["material_contract"]
            ["executor_budget_bound_summary"])
        self.assertEqual(summary["bound_count"], 5)
        self.assertEqual(summary["pass_count"], 5)
        self.assertEqual(summary["fail_count"], 0)
        self.assertEqual(summary["failed_keys"], [])

    def test_material_executor_budget_bound_failure_is_llm_actionable(
            self) -> None:
        manifest = {
            "require_material_executor_budget": {
                "upload_utilization_lte": 0.5,
            },
        }
        code, report = self.run_verifier(
            snapshot(sampled_material_plan(sample_taps=13)),
            manifest)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material executor budget upload_utilization is within limit"))
        self.assertEqual(
            failure["path"],
            "artifact_context.material_contract.executor_budget"
            ".upload_utilization")
        self.assertEqual(failure["expected"], {"<=": 0.5})
        self.assertEqual(failure["actual"], 1.0)
        self.assertEqual(failure["likely_layer"], "platform-runtime")
        self.assertEqual(failure["likely_pass"], "material-executor")
        self.assertIn("executor budget", failure["hint"])
        self.assertEqual(
            report["failure_summary"]["top_likely_pass"],
            "material-executor")
        self.assertIn(
            "executor_budget",
            report["failure_summary"]["artifact_context"]["material_contract"])
        results = (
            report["artifact_context"]
            ["material_contract"]
            ["executor_budget_bound_results"])
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0]["key"], "upload_utilization_lte")
        self.assertFalse(results[0]["ok"])
        self.assertEqual(results[0]["margin"], -0.5)
        summary = (
            report["artifact_context"]
            ["material_contract"]
            ["executor_budget_bound_summary"])
        self.assertEqual(summary["bound_count"], 1)
        self.assertEqual(summary["pass_count"], 0)
        self.assertEqual(summary["fail_count"], 1)
        self.assertEqual(summary["failed_keys"], ["upload_utilization_lte"])

    def test_runtime_numeric_bound_failure_is_llm_actionable(self) -> None:
        manifest = {
            "require_runtime_numeric_bounds": [
                {
                    "path": "renderer.material_executor_summary.material_draw_calls",
                    "gte": 1,
                }
            ],
        }
        code, report = self.run_verifier(
            snapshot_with_glass_probe_contract(material_plan()),
            manifest)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "runtime numeric >= bound: "
                "renderer.material_executor_summary.material_draw_calls"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_executor_summary"
            ".material_draw_calls")
        self.assertEqual(failure["expected"], {">=": 1})
        self.assertEqual(failure["actual"], 0)
        self.assertEqual(failure["likely_layer"], "platform-runtime")
        self.assertEqual(failure["likely_pass"], "material-executor")
        self.assertIn("missing pass or counter", failure["hint"])
        self.assertIn("material executor counters", failure["suggested_action"])
        failure_summary = report["failure_summary"]
        self.assertEqual(failure_summary["count"], 1)
        self.assertEqual(failure_summary["top_likely_layer"], "platform-runtime")
        self.assertEqual(failure_summary["top_likely_pass"], "material-executor")
        self.assertEqual(
            failure_summary["top_suggested_action"],
            failure["suggested_action"])
        self.assertEqual(
            failure_summary["by_suggested_action"][failure["suggested_action"]],
            1)
        self.assertEqual(
            failure_summary["first_failure"]["path"],
            failure["path"])
        self.assertEqual(
            failure_summary["first_failure"]["expected"],
            failure["expected"])
        self.assertEqual(
            failure_summary["first_failure"]["actual"],
            failure["actual"])
        self.assertIn(
            "missing pass or counter",
            failure_summary["first_failure"]["hint"])
        self.assertEqual(
            failure_summary["first_failure"]["suggested_action"],
            failure["suggested_action"])
        artifact_context = failure_summary["artifact_context"]
        self.assertEqual(report["artifact_context"], artifact_context)
        self.assertEqual(artifact_context["platform"], "test")
        self.assertEqual(artifact_context["backend"], "synthetic")
        material_contract = artifact_context["material_contract"]
        self.assertEqual(material_contract["semantic_material_nodes"], 1)
        self.assertEqual(
            material_contract["renderer_plan_contract_version"],
            verifier.MATERIAL_PLAN_CONTRACT_VERSION)
        self.assertEqual(material_contract["renderer_plan_count"], 1)
        self.assertTrue(material_contract["renderer_plans_present"])
        self.assertEqual(material_contract["resolved_plan_count"], 1)
        self.assertEqual(
            material_contract["plan_contract_versions"],
            {str(verifier.MATERIAL_PLAN_CONTRACT_VERSION): 1})
        self.assertEqual(
            material_contract["fallback_paths"],
            {"unsupported-backend": 1})
        self.assertEqual(
            material_contract["backdrop"]["luminance_responses"],
            {"not-sampled": 1})
        self.assertEqual(
            material_contract["backdrop"]["frosting_responses"],
            {"not-sampled": 1})
        self.assertEqual(material_contract["plan_shape"]["rounded"], 1)
        self.assertEqual(
            material_contract["decision_first_blockers"],
            {"unsupported-backend": 1})
        self.assertEqual(material_contract["decision_reduced_transparency"], 0)
        self.assertEqual(material_contract["decision_increase_contrast"], 0)
        self.assertEqual(material_contract["decision_reduce_motion"], 0)
        executor_budget = material_contract["executor_budget"]
        self.assertEqual(executor_budget["plan_count"], 1)
        self.assertEqual(executor_budget["sampled_backdrop_instance_count"], 0)
        self.assertEqual(executor_budget["fallback_instance_count"], 1)
        self.assertEqual(executor_budget["dropped_execution_stage_count"], 0)
        self.assertEqual(executor_budget["draw_calls"], 0)
        self.assertEqual(executor_budget["total_sample_taps"], 0)
        self.assertEqual(executor_budget["upload_bytes"], 0)
        self.assertEqual(executor_budget["buffer_capacity_bytes"], 0)
        self.assertEqual(executor_budget["upload_utilization"], 0.0)
        self.assertEqual(executor_budget["backdrop_copy_count"], 0)
        self.assertEqual(executor_budget["backdrop_copy_pixels"], 0)
        self.assertEqual(executor_budget["backdrop_copy_utilization"], 0.0)
        self.assertEqual(executor_budget["upload_status"], "not-needed")
        self.assertEqual(executor_budget["draw_status"], "not-needed")
        self.assertEqual(
            material_contract["app_probe_contract_name"],
            "glass_showcase_material_probe_contract")
        self.assertEqual(
            material_contract["app_probe_reference_technology"],
            "liquid-glass")
        self.assertEqual(material_contract["app_probe_active_count"], 7)
        self.assertEqual(
            material_contract["app_probe_total_expected_execution_stages"],
            28)
        self.assertTrue(material_contract["app_probe_material_probes_present"])
        self.assertEqual(
            material_contract["app_probe_names"],
            ["debug_contract", "visible_blur_probe"])

    def test_material_executor_budget_context_summarizes_sampled_work(
            self) -> None:
        code, report = self.run_verifier(
            snapshot(sampled_material_plan(sample_taps=13)))

        self.assertEqual(code, 0)
        budget = (
            report["artifact_context"]
            ["material_contract"]
            ["executor_budget"])
        self.assertEqual(budget["plan_count"], 1)
        self.assertEqual(budget["material_instance_count"], 1)
        self.assertEqual(budget["sampled_backdrop_instance_count"], 1)
        self.assertEqual(budget["fallback_instance_count"], 0)
        self.assertEqual(budget["execution_stage_count"], 4)
        self.assertEqual(budget["active_execution_stage_count"], 4)
        self.assertEqual(budget["backdrop_execution_stage_count"], 1)
        self.assertEqual(budget["dropped_execution_stage_count"], 0)
        self.assertEqual(budget["draw_calls"], 1)
        self.assertEqual(budget["total_sample_taps"], 13)
        self.assertEqual(budget["max_sample_taps"], 13)
        self.assertEqual(budget["upload_bytes"], 128)
        self.assertEqual(budget["buffer_capacity_bytes"], 128)
        self.assertEqual(budget["upload_utilization"], 1.0)
        self.assertEqual(budget["backdrop_copy_count"], 0)
        self.assertEqual(budget["backdrop_copy_pixels"], 0)
        self.assertEqual(budget["backdrop_copy_utilization"], 0.0)
        self.assertTrue(budget["pipeline_ready"])
        self.assertTrue(budget["backdrop_source_ready"])
        self.assertTrue(budget["upload_required"])
        self.assertTrue(budget["draw_required"])
        self.assertTrue(budget["uploaded"])
        self.assertTrue(budget["drawn"])
        self.assertEqual(budget["upload_status"], "uploaded")
        self.assertEqual(budget["draw_status"], "drawn")

    def test_material_executor_summary_mismatch_is_llm_actionable(self) -> None:
        root = snapshot(material_plan())
        renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
        assert isinstance(renderer, dict)
        executor = renderer["material_executor_summary"]
        assert isinstance(executor, dict)
        executor["fallback_instance_count"] = 0

        code, report = self.run_verifier(root)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material executor summary fallback_instance_count "
                "matches plans"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_executor_summary"
            ".fallback_instance_count")
        self.assertEqual(failure["expected"], 1)
        self.assertEqual(failure["actual"], 0)
        self.assertEqual(failure["likely_layer"], "platform-runtime")
        self.assertEqual(failure["likely_pass"], "material-executor")
        self.assertIn("material_executor_summary", failure["hint"])
        self.assertEqual(
            report["failure_summary"]["top_likely_pass"],
            "material-executor")

    def test_material_executor_sample_taps_mismatch_is_llm_actionable(self) -> None:
        root = snapshot(sampled_material_plan(sample_taps=13))
        renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
        assert isinstance(renderer, dict)
        executor = renderer["material_executor_summary"]
        assert isinstance(executor, dict)
        executor["material_total_sample_taps"] = 25

        code, report = self.run_verifier(root)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material executor summary material_total_sample_taps "
                "matches plans"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_executor_summary"
            ".material_total_sample_taps")
        self.assertEqual(failure["expected"], 13)
        self.assertEqual(failure["actual"], 25)
        self.assertEqual(failure["likely_pass"], "material-executor")
        self.assertIn("MaterialPlan.sample_taps", failure["hint"])

    def test_material_executor_shader_scale_mismatch_is_llm_actionable(
            self) -> None:
        plan = sampled_material_plan(sample_taps=25)
        assert isinstance(plan["render_target"], dict)
        plan["render_target"]["scale"] = 2.0
        root = snapshot(plan)
        renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
        assert isinstance(renderer, dict)
        executor = renderer["material_executor_summary"]
        assert isinstance(executor, dict)
        executor["material_shader_content_scale"] = 1.0

        code, report = self.run_verifier(root)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material executor summary material_shader_content_scale "
                "matches plans"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_executor_summary"
            ".material_shader_content_scale")
        self.assertEqual(failure["expected"], 2.0)
        self.assertEqual(failure["actual"], 1.0)
        self.assertEqual(failure["likely_pass"], "material-executor")

    def test_material_executor_shader_blur_step_mismatch_is_llm_actionable(
            self) -> None:
        plan = sampled_material_plan(sample_taps=25)
        assert isinstance(plan["render_target"], dict)
        plan["render_target"]["scale"] = 2.0
        root = snapshot(plan)
        renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
        assert isinstance(renderer, dict)
        executor = renderer["material_executor_summary"]
        assert isinstance(executor, dict)
        executor["material_max_shader_blur_step_pixels"] = 7.7

        code, report = self.run_verifier(root)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material executor summary "
                "material_max_shader_blur_step_pixels matches plans"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_executor_summary"
            ".material_max_shader_blur_step_pixels")
        self.assertAlmostEqual(failure["expected"], 15.4)
        self.assertEqual(failure["actual"], 7.7)
        self.assertEqual(failure["likely_pass"], "material-executor")

    def test_material_executor_sampled_instance_mismatch_is_llm_actionable(
            self) -> None:
        root = snapshot(sampled_material_plan(sample_taps=13))
        renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
        assert isinstance(renderer, dict)
        executor = renderer["material_executor_summary"]
        assert isinstance(executor, dict)
        executor["sampled_backdrop_instance_count"] = 0

        code, report = self.run_verifier(root)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material executor summary sampled_backdrop_instance_count "
                "matches plans"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_executor_summary"
            ".sampled_backdrop_instance_count")
        self.assertEqual(failure["expected"], 1)
        self.assertEqual(failure["actual"], 0)
        self.assertEqual(failure["likely_pass"], "material-executor")
        self.assertIn("material_executor_summary", failure["hint"])

    def test_material_executor_stage_count_mismatch_is_llm_actionable(self) -> None:
        root = snapshot(sampled_material_plan(sample_taps=13))
        renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
        assert isinstance(renderer, dict)
        executor = renderer["material_executor_summary"]
        assert isinstance(executor, dict)
        executor["execution_stage_count"] = 0

        code, report = self.run_verifier(root)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "material executor summary execution_stage_count "
                "matches plans"))
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_executor_summary"
            ".execution_stage_count")
        self.assertEqual(failure["expected"], 4)
        self.assertEqual(failure["actual"], 0)
        self.assertEqual(failure["likely_layer"], "platform-runtime")
        self.assertEqual(failure["likely_pass"], "material-executor")
        self.assertIn("renderer.material_plans", failure["hint"])

    def test_material_executor_upload_status_mismatch_is_llm_actionable(
            self) -> None:
        root = snapshot(sampled_material_plan(sample_taps=13))
        renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
        assert isinstance(renderer, dict)
        executor = renderer["material_executor_summary"]
        assert isinstance(executor, dict)
        executor["material_upload_bytes"] = 0
        executor["material_sampled_backdrop_uploaded"] = False
        executor["material_upload_status"] = "uploaded"

        code, report = self.run_verifier(root)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"]
            == "material executor upload status matches edge facts")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_executor_summary"
            ".material_upload_status")
        self.assertEqual(
            failure["expected"],
            "skipped-material-buffer-upload-not-recorded")
        self.assertEqual(failure["actual"], "uploaded")
        self.assertEqual(failure["likely_pass"], "material-executor")
        self.assertIn("edge facts", failure["hint"])

    def test_material_executor_draw_status_mismatch_is_llm_actionable(
            self) -> None:
        root = snapshot(sampled_material_plan(sample_taps=13))
        renderer = root["debug"]["platform_runtime"]["details"]["renderer"]
        assert isinstance(renderer, dict)
        executor = renderer["material_executor_summary"]
        assert isinstance(executor, dict)
        executor["material_draw_calls"] = 0
        executor["material_sampled_backdrop_drawn"] = False
        executor["material_draw_status"] = "drawn"

        code, report = self.run_verifier(root)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material executor draw status matches edge facts")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_executor_summary"
            ".material_draw_status")
        self.assertEqual(failure["expected"], "skipped-material-draw-not-recorded")
        self.assertEqual(failure["actual"], "drawn")
        self.assertEqual(failure["likely_pass"], "material-executor")
        self.assertIn("pipeline/backdrop readiness", failure["hint"])

    def test_manifest_can_compare_pixel_region_metrics(self) -> None:
        manifest = {
            "require_frame": True,
            "pixel_regions": [
                {
                    "name": "backdrop",
                    "rect": [0, 0, 4, 2],
                    "min_luma_delta": 200,
                    "min_unique_colors": 2,
                },
                {
                    "name": "blur",
                    "rect": [4, 0, 4, 2],
                    "min_luma_delta": 20,
                    "min_unique_colors": 2,
                },
            ],
            "pixel_region_metric_comparisons": [
                {
                    "region": "blur",
                    "metric": "edge_energy",
                    "reference_region": "backdrop",
                    "lte_ratio": 0.25,
                }
            ],
        }
        code, report = self.run_verifier(
            snapshot(material_plan()),
            manifest,
            write_frame=True)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        regions = {item["name"]: item for item in report["pixel_regions"]}
        self.assertLess(
            regions["blur"]["edge_energy"],
            regions["backdrop"]["edge_energy"] * 0.25)
        self.assertEqual(
            report["manifest"]["pixel_region_metric_comparisons"],
            1)

    def test_pixel_region_metrics_include_rgb_channel_means(self) -> None:
        manifest = {
            "require_frame": True,
            "pixel_regions": [
                {
                    "name": "close-control",
                    "rect": [0.0, 0.0, 0.34, 1.0],
                    "min_luma_delta": 0,
                    "min_unique_colors": 1,
                },
                {
                    "name": "zoom-control",
                    "rect": [0.67, 0.0, 0.34, 1.0],
                    "min_luma_delta": 0,
                    "min_unique_colors": 1,
                },
            ],
            "pixel_region_metrics": [
                {
                    "region": "close-control",
                    "metric": "red_mean",
                    "gte": 250,
                },
                {
                    "region": "close-control",
                    "metric": "green_mean",
                    "lte": 100,
                },
                {
                    "region": "zoom-control",
                    "metric": "green_mean",
                    "gte": 200,
                },
                {
                    "region": "zoom-control",
                    "metric": "red_mean",
                    "lte": 40,
                },
            ],
        }
        code, report = self.run_verifier(
            snapshot(material_plan()),
            manifest,
            frame_writer=write_channel_probe_bmp)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        regions = {item["name"]: item for item in report["pixel_regions"]}
        self.assertEqual(regions["close-control"]["red_mean"], 255.0)
        self.assertEqual(regions["zoom-control"]["green_mean"], 201.0)

    def test_forbidden_pixel_region_color_failure_is_llm_actionable(self) -> None:
        manifest = {
            "require_frame": True,
            "pixel_regions": [
                {
                    "name": "native-control-reserve",
                    "rect": [0.0, 0.0, 1.0, 1.0],
                    "min_luma_delta": 0,
                    "min_unique_colors": 1,
                }
            ],
            "forbid_pixel_region_colors": [
                {
                    "region": "native-control-reserve",
                    "name": "macos-close-red-marker",
                    "rgb": [255, 95, 87],
                    "tolerance": 4,
                    "max_pixels": 0,
                }
            ],
        }
        code, report = self.run_verifier(
            snapshot(material_plan()),
            manifest,
            frame_writer=write_channel_probe_bmp)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == (
                "forbidden pixel color absent: macos-close-red-marker"))
        self.assertEqual(failure["path"], (
            "frame.bmp#native-control-reserve.forbidden_color."
            "macos-close-red-marker"))
        self.assertEqual(failure["likely_layer"], "native-window-chrome")
        self.assertEqual(failure["likely_pass"], "window-control-marker")
        self.assertIn("Native traffic lights belong to the OS", failure["hint"])

    def test_pixel_region_metric_comparison_failure_is_llm_actionable(self) -> None:
        manifest = {
            "require_frame": True,
            "pixel_regions": [
                {
                    "name": "backdrop",
                    "rect": [0, 0, 4, 2],
                    "min_luma_delta": 200,
                    "min_unique_colors": 2,
                },
                {
                    "name": "blur",
                    "rect": [4, 0, 4, 2],
                    "min_luma_delta": 20,
                    "min_unique_colors": 2,
                },
            ],
            "pixel_region_metric_comparisons": [
                {
                    "region": "blur",
                    "metric": "edge_energy",
                    "reference_region": "backdrop",
                    "lte_ratio": 0.01,
                }
            ],
        }
        code, report = self.run_verifier(
            snapshot(material_plan()),
            manifest,
            write_frame=True)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "pixel region metric <= reference ratio: blur.edge_energy")
        self.assertEqual(failure["path"], "frame.bmp#blur.edge_energy")
        self.assertEqual(failure["region"], "blur")
        self.assertEqual(failure["likely_layer"], "material-or-backdrop-pass")
        self.assertEqual(
            failure["expected"]["<="]["reference_region"],
            "backdrop")
        self.assertEqual(failure["actual"]["region"], "blur")
        self.assertIn("smoother than the named backdrop reference", failure["hint"])
        self.assertIn("frame.bmp region blur", failure["suggested_action"])

    def test_material_contract_mismatch_has_specific_suggested_action(self) -> None:
        root = snapshot(material_plan())
        semantic_tree = root["debug"]["semantic_tree"]
        assert isinstance(semantic_tree, dict)
        children = semantic_tree["children"]
        assert isinstance(children, list)
        material_node = children[0]
        assert isinstance(material_node, dict)
        material_debug = material_node["material"]
        assert isinstance(material_debug, dict)
        material_debug["kind"] = "thin"

        code, report = self.run_verifier(root)

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material semantic/runtime kinds match")
        self.assertEqual(
            failure["path"],
            "debug.material_semantic_runtime_match.kinds")
        self.assertEqual(failure["likely_layer"], "material-contract")
        self.assertIn("semantic material nodes", failure["suggested_action"])
        self.assertIn("renderer.material_plans", failure["suggested_action"])


if __name__ == "__main__":
    unittest.main()

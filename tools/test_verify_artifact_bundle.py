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
    command_descriptor: dict[str, object] = {
        "kind": "regular",
        "role": "surface",
        "container": container,
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
            "participates": False,
            "shared_backdrop_scope": False,
            "shape_union_expected": False,
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
            "rounded": True,
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
            "luma_min": 0.0,
            "luma_max": 1.0,
            "luma_mean": 0.5,
            "luma_span": 1.0,
            "source": "none",
            "luminance_response": "not-sampled",
            "luminance_floor_delta": 0.0,
            "luminance_gain_delta": 0.0,
            "edge_highlight_delta": 0.0,
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
            "background_luma": 0.72,
            "primary_contrast_ratio": 12.0,
            "secondary_contrast_ratio": 6.0,
            "accent_contrast_ratio": 4.5,
            "minimum_contrast_ratio": 4.5,
            "backdrop_driven": False,
            "high_contrast": False,
            "uses_vibrancy": False,
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
            "max_backdrop_pixels": 320 * 240,
            "max_frame_capture_count": 0,
            "max_frame_capture_pixels": 0,
            "max_surface_sample_pixels": 0,
            "max_container_spacing": 0.0,
            "bounded_texture_copy": True,
            "deterministic_fallback": True,
        },
        "execution_stage_capacity": 4,
        "dropped_execution_stage_count": 0,
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
                "name": "translucent-rounded-rect",
                "active": True,
                "requires_backdrop": False,
                "sample_taps": 0,
                "likely_layer": "material-fallback-pass",
                "executor": "fallback-fill",
                "max_texture_copy_pixels": 0,
                "bounded": True,
            }
        ],
    }
    refresh_observation_contract(plan)
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
    budget = plan["resource_budget"]
    assert isinstance(shape, dict)
    assert isinstance(tint, dict)
    assert isinstance(container, dict)
    assert isinstance(foreground, dict)
    assert isinstance(budget, dict)
    shape_valid = bool(shape["valid"])
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
            "invalid"
            if not shape_valid
            else "rounded-rectangle" if bool(shape["rounded"]) else "rectangle"),
        "shape_scope": "view-bounds",
        "blending_scope": blending_scope,
        "semantic_thickness": kind,
        "accessibility_response": reference_accessibility_response(plan),
        "performance_response": reference_performance_response(plan),
        "view_bounds_anchored": kind != "none" and shape_valid,
        "shape_matches_geometry": kind != "none" and shape_valid,
        "tint_applied": kind != "none" and int(tint["a"]) > 0,
        "interactive_response": bool(container["interactive"]),
        "container_grouped": bool(container["participates"]),
        "container_union": bool(container["shape_union_expected"]),
        "container_morphing": bool(container["morph_transitions"]),
        "legibility_preserved": legibility_preserved,
        "vibrancy_expected": bool(foreground["uses_vibrancy"]),
        "deterministic_degradation": bool(budget["deterministic_fallback"]),
    }


def refresh_observation_contract(plan: dict[str, object]) -> None:
    refresh_reference_model(plan)
    primary = plan["primary_pass"]
    stages = plan["execution_stages"]
    budget = plan["resource_budget"]
    backdrop_access = plan["backdrop_access"]
    verifier_contract = plan["verifier"]
    assert isinstance(primary, dict)
    assert isinstance(stages, list)
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
        "expected_runtime_passes": len(plan["passes"]),
        "expected_execution_stages": len(stages),
        "expected_active_execution_stages": sum(
            1 for stage in stages
            if isinstance(stage, dict) and stage["active"]),
        "expected_backdrop_execution_stages": sum(
            1 for stage in stages
            if isinstance(stage, dict) and stage["requires_backdrop"]),
        "max_frame_capture_count": backdrop_access["max_frame_capture_count"],
        "max_frame_capture_pixels": backdrop_access["max_frame_capture_pixels"],
        "max_surface_sample_pixels": backdrop_access["max_surface_sample_pixels"],
        "max_texture_copy_pixels": primary["max_texture_copy_pixels"],
        "region_name": verifier_contract["region_name"],
        "likely_layer": verifier_contract["likely_layer"],
        "likely_pass": verifier_contract["likely_pass"],
    }


def sampled_material_plan(sample_taps: int = 25) -> dict[str, object]:
    plan = material_plan(sample_taps=sample_taps, primary_sample_taps=sample_taps)
    assert isinstance(plan["decision_trace"], dict)
    assert isinstance(plan["backdrop"], dict)
    assert isinstance(plan["backdrop_access"], dict)
    assert isinstance(plan["primary_pass"], dict)
    assert isinstance(plan["sampling_kernel"], dict)
    assert isinstance(plan["luminance_curve"], dict)
    assert isinstance(plan["foreground"], dict)
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
    plan["backdrop"].update({
        "available": True,
        "stable": True,
        "excludes_foreground_text": True,
        "source": "previous-presented-frame",
        "luminance_response": "neutral",
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
    plan["sampling_kernel"].update({
        "name": "weighted-5x5-manhattan",
        "radius": 2,
        "sample_taps": sample_taps,
        "blur_step_scale": 0.35,
        "weight_profile": "center4-cardinal2-diagonal1",
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
    assert isinstance(plan["resource_budget"], dict)
    plan["resource_budget"]["max_sampling_kernel_radius"] = 2
    plan["resource_budget"]["max_frame_capture_count"] = 1
    plan["resource_budget"]["max_frame_capture_pixels"] = 320 * 240
    plan["resource_budget"]["max_surface_sample_pixels"] = 240 * 96
    plan["passes"] = [plan["primary_pass"]]
    plan["execution_stages"] = [
        {
            "name": "backdrop-sample-blur",
            "active": True,
            "requires_backdrop": True,
            "sample_taps": sample_taps,
            "likely_layer": "material-blur-pass",
            "executor": "backdrop-filter",
            "max_texture_copy_pixels": 320 * 240,
            "bounded": True,
        }
    ]
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
    return plan


def material_runtime_summary(plan: dict[str, object]) -> dict[str, object]:
    budget = plan["resource_budget"]
    primary = plan["primary_pass"]
    shape = plan["shape"]
    backdrop_access = plan["backdrop_access"]
    execution_stages = plan["execution_stages"]
    assert isinstance(budget, dict)
    assert isinstance(primary, dict)
    assert isinstance(shape, dict)
    assert isinstance(backdrop_access, dict)
    assert isinstance(execution_stages, list)
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
        "max_execution_stage_capacity": plan["execution_stage_capacity"],
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
        "max_execution_stages": budget["max_execution_stages"],
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
        "max_surface_area": shape["surface_area"],
        "max_effective_radius": shape["effective_radius"],
        "max_radius_limit": shape["radius_limit"],
        "max_normalized_radius": shape["normalized_radius"],
        "min_foreground_contrast_ratio": (
            plan["foreground"]["primary_contrast_ratio"]
        ),
        "unbounded_texture_copy": 0 if budget["bounded_texture_copy"] else 1,
        "non_deterministic_fallback": (
            0 if budget["deterministic_fallback"] else 1
        ),
    }


def material_executor_summary(plan: dict[str, object]) -> dict[str, object]:
    sample_taps = 0 if plan["fallback"] else int(plan["sample_taps"])
    stages = plan["execution_stages"]
    assert isinstance(stages, list)
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
    assert isinstance(primary, dict)
    assert isinstance(backdrop_access, dict)
    primary_stages = [
        stage for stage in stages
        if isinstance(stage, dict)
        and all(
            stage.get(key) == primary.get(key)
            for key in verifier.MATERIAL_PASS_FIELDS)
    ]
    return {
        "plan_count": 1,
        "material_instance_count": 0 if plan["fallback"] else 1,
        "fallback_instance_count": 1 if plan["fallback"] else 0,
        "material_draw_calls": 0 if plan["fallback"] else 1,
        "backdrop_copy_count": 0,
        "execution_stage_count": len(stages),
        "active_execution_stage_count": len(active_stages),
        "backdrop_execution_stage_count": len(backdrop_stages),
        "primary_execution_stage_count": len(primary_stages),
        "dropped_execution_stage_count": plan["dropped_execution_stage_count"],
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
        "material_upload_bytes": 0,
        "material_buffer_capacity_bytes": 0,
        "material_buffer_reallocations": 0,
        "foreground_text_candidate_count": 1,
        "foreground_text_remap_count": 1,
        "cpu_decode_ns": 100,
        "cpu_material_upload_ns": 0,
        "cpu_total_ns": 200,
    }


def snapshot(plan: dict[str, object]) -> dict[str, object]:
    return {
        "debug": {
            "platform_capabilities": {
                "platform": "test",
                "snapshot_json": True,
                "write_artifact_bundle": True,
                "semantic_tree": True,
                "input_debug": True,
                "platform_runtime": True,
                "platform_diagnostics": False,
                "material_surfaces": True,
                "material_backdrop_blur": False,
            },
            "input_debug": {
                "event": "none",
                "source": "test",
                "detail": "synthetic verifier self-test",
                "result": "none",
                "pressed_id": 4294967295,
                "focus_visible": False,
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
                "details": {
                    "renderer": {
                        "material_plan_contract_version": (
                            verifier.MATERIAL_PLAN_CONTRACT_VERSION),
                        "material_plan_count": 1,
                        "material_plans": [plan],
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
) -> dict[str, object]:
    root = snapshot(plan)
    debug = root["debug"]
    assert isinstance(debug, dict)
    debug["application"] = {
        "file_explorer": {
            "kind": "file_explorer",
            "profile": "desktop",
            "chrome": {
                "native_window": {
                    "integrated_titlebar": True,
                    "native_window_controls": True,
                    "duplicate_window_controls": duplicate,
                    "content_window_control_markers": content_markers,
                    "artifact_window_control_markers": artifact_markers,
                    "window_control_marker_mode": "runtime-native-controls",
                    "native_window_control_owner": "platform-edge",
                    "native_window_control_count": 3,
                    "content_window_control_marker_count": content_marker_count,
                    "artifact_window_control_marker_count": artifact_marker_count,
                    "window_control_render_policy": (
                        "native_controls_runtime_only_no_content_or_artifact_markers"),
                    "titlebar_control_reserve_policy": (
                        "blank_reserve_under_os_window_controls"),
                }
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

    def test_file_explorer_native_chrome_contract_accepts_platform_owner(self) -> None:
        code, report = self.run_verifier(
            snapshot_with_file_explorer_chrome(material_plan()))

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])

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
        self.assertEqual(
            report["material_plans"]["shape"]["valid"],
            1)
        self.assertEqual(
            report["material_plans"]["shape"]["rounded"],
            1)
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

        code, report = self.run_verifier(snapshot(material_plan()), manifest)

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

    def test_manifest_can_require_material_shape_summary(self) -> None:
        manifest = {
            "require_material_plan_summary": {
                "shape_valid": 1,
                "shape_rounded": 1,
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

        code, report = self.run_verifier(snapshot(material_plan()), manifest)

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
            "morph_transitions": False,
            "participates": True,
            "shared_backdrop_scope": True,
            "shape_union_expected": False,
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
                "total_execution_stages_lte": 1,
                "total_execution_stages_gte": 1,
                "active_execution_stages_lte": 1,
                "active_execution_stages_gte": 1,
                "backdrop_execution_stages_lte": 1,
                "backdrop_execution_stages_gte": 1,
                "dropped_execution_stages_lte": 0,
                "dropped_execution_stages_gte": 0,
                "max_execution_stage_count_lte": 1,
                "max_execution_stage_count_gte": 1,
                "max_execution_stage_capacity_lte": 4,
                "max_execution_stage_capacity_gte": 4,
                "max_execution_stages_lte": 4,
                "max_execution_stages_gte": 4,
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
            1)
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

    def test_manifest_can_require_execution_stage_summary(self) -> None:
        manifest = {
            "require_material_plan_summary": {
                "stage_names": {
                    "translucent-rounded-rect": 1,
                },
                "stage_executors": {
                    "fallback-fill": 1,
                },
            },
        }
        code, report = self.run_verifier(snapshot(material_plan()), manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(
            report["material_plans"]["stage_names"],
            {"translucent-rounded-rect": 1})
        self.assertEqual(
            report["material_plans"]["stage_executors"],
            {"fallback-fill": 1})

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
                "luminance_curves": {"fallback-flat": 1},
                "luminance_adapted": 0,
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
        manifest = {
            "require_material_surface_roles": ["surface"],
            "require_material_plan_summary": {
                "roles": {"surface": 1},
            },
        }
        code, report = self.run_verifier(snapshot(material_plan()), manifest)

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

    def test_runtime_numeric_bound_failure_is_llm_actionable(self) -> None:
        manifest = {
            "require_runtime_numeric_bounds": [
                {
                    "path": "renderer.material_executor_summary.material_draw_calls",
                    "gte": 1,
                }
            ],
        }
        code, report = self.run_verifier(snapshot(material_plan()), manifest)

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
        self.assertEqual(material_contract["plan_shape"]["rounded"], 1)
        self.assertEqual(
            material_contract["decision_first_blockers"],
            {"unsupported-backend": 1})
        self.assertEqual(material_contract["decision_reduced_transparency"], 0)
        self.assertEqual(material_contract["decision_increase_contrast"], 0)
        self.assertEqual(material_contract["decision_reduce_motion"], 0)

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
        self.assertEqual(failure["expected"], 1)
        self.assertEqual(failure["actual"], 0)
        self.assertEqual(failure["likely_layer"], "platform-runtime")
        self.assertEqual(failure["likely_pass"], "material-executor")
        self.assertIn("renderer.material_plans", failure["hint"])

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

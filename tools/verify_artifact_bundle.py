#!/usr/bin/env python3
"""Validate a phenotype debug artifact bundle.

The verifier is intentionally dependency-free so CI, local examples, and LLM
debug sessions can all consume the same deterministic report.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys
from typing import Any


JsonObject = dict[str, Any]

ALLOWED_PIXEL_REGION_METRICS = {
    "alpha_mean",
    "blue_mean",
    "edge_energy",
    "green_mean",
    "luma_delta",
    "luma_mean",
    "luma_stddev",
    "red_mean",
    "unique_colors",
}

ALLOWED_MATERIAL_FALLBACK_PATHS = {
    "invalid-geometry",
    "no-backdrop-source",
    "no-material",
    "none",
    "quality-policy",
    "reduced-transparency",
    "unsupported-backend",
}

ALLOWED_MATERIAL_KINDS = {
    "clear",
    "none",
    "regular",
    "thick",
    "thin",
}

ALLOWED_MATERIAL_SURFACE_ROLES = {
    "content",
    "navigation",
    "overlay",
    "sidebar",
    "status_bar",
    "surface",
    "toolbar",
}

ALLOWED_MATERIAL_CONTAINER_MODES = {
    "container",
    "isolated",
    "union",
}

ALLOWED_MATERIAL_PASS_NAMES = {
    "backdrop-sample-blur",
    "none",
    "standard-material-fill",
    "translucent-rounded-rect",
}

ALLOWED_MATERIAL_PASS_EXECUTORS = {
    "backdrop-filter",
    "fallback-fill",
    "none",
    "standard-fill",
}

ALLOWED_MATERIAL_STAGE_NAMES = {
    "backdrop-sample-blur",
    "edge-highlight",
    "noise-dither",
    "shape-shadow",
    "standard-material-fill",
    "translucent-rounded-rect",
}

ALLOWED_MATERIAL_STAGE_EXECUTORS = {
    "backdrop-filter",
    "edge-highlight",
    "fallback-fill",
    "ordered-dither",
    "shape-shadow",
    "standard-fill",
}

ALLOWED_MATERIAL_LIKELY_LAYERS = {
    "material-blur-pass",
    "material-edge-pass",
    "material-fallback",
    "material-fallback-pass",
    "material-foreground",
    "material-noise-pass",
    "material-shadow-pass",
    "material-standard-pass",
}

ALLOWED_MATERIAL_LUMINANCE_RESPONSES = {
    "bright",
    "bright-flat",
    "dark",
    "dark-flat",
    "flat",
    "neutral",
    "not-sampled",
}

ALLOWED_MATERIAL_SAMPLING_KERNELS = {
    "none",
    "weighted-5x5-manhattan",
}

ALLOWED_MATERIAL_SAMPLING_WEIGHT_PROFILES = {
    "center4-cardinal2-diagonal1",
    "none",
}

ALLOWED_MATERIAL_LUMINANCE_CURVES = {
    "adaptive-backdrop-luma",
    "fallback-flat",
}

ALLOWED_MATERIAL_BACKDROP_CAPTURE_SCOPES = {
    "none",
    "shared-frame",
}

ALLOWED_MATERIAL_FOREGROUND_SCHEMES = {
    "high-contrast",
    "none",
    "solid-fallback",
    "standard",
    "vibrant-balanced",
    "vibrant-dark",
    "vibrant-light",
}

ALLOWED_MATERIAL_FOREGROUND_SOURCES = {
    "accessibility",
    "backdrop-analysis",
    "fallback-material",
    "none",
    "theme",
}

ALLOWED_MATERIAL_REFERENCE_TECHNOLOGIES = {
    "liquid-glass",
    "standard-material",
}

ALLOWED_MATERIAL_REFERENCE_LAYERS = {
    "content-layer",
    "functional-layer",
    "inactive",
}

ALLOWED_MATERIAL_REFERENCE_POLICIES = {
    "inactive",
    "liquid-glass-functional-layer",
    "standard-material-content-layer",
}

ALLOWED_MATERIAL_REFERENCE_SHAPES = {
    "capsule",
    "invalid",
    "none",
    "rectangle",
    "rounded-rectangle",
}

ALLOWED_MATERIAL_REFERENCE_SHAPE_SCOPES = {
    "view-bounds",
}

ALLOWED_MATERIAL_REFERENCE_BLENDING_SCOPES = {
    "deterministic-fallback",
    "none",
    "sampled-backdrop",
    "standard-fill",
}

ALLOWED_MATERIAL_REFERENCE_ACCESSIBILITY_RESPONSES = {
    "combined-accessibility",
    "increased-contrast",
    "reduced-motion",
    "reduced-transparency",
    "standard",
}

ALLOWED_MATERIAL_REFERENCE_PERFORMANCE_RESPONSES = {
    "budgeted-effects",
    "deterministic-fallback",
    "inactive",
    "standard",
    "warmup-capture",
}

MATERIAL_PLAN_CONTRACT_VERSION = 22


def suggested_action_for_failure(
    path: str,
    likely_layer: str,
    likely_pass: str,
    region: str,
) -> str:
    if likely_pass == "material-executor":
        return (
            "Inspect renderer.material_executor_summary and the backend "
            "material executor counters for the missing or excess pass work.")
    if likely_pass == "sampling-kernel":
        return (
            "Inspect MaterialPlan.sampling_kernel and the backend material "
            "shader inputs for stale blur-kernel metadata.")
    if likely_pass == "luminance-curve":
        return (
            "Inspect MaterialPlan.luminance_curve, backdrop luminance analysis, "
            "and the backend material shader curve inputs.")
    if likely_pass:
        return (
            f"Inspect the {likely_pass} material pass serialization and the "
            "matching MaterialPlan.primary_pass entry.")
    if region:
        return (
            f"Inspect frame.bmp region {region}, its manifest rectangle, and "
            "renderer.material_plans#summary.region_layers.")
    if likely_layer == "material-plan":
        return (
            "Inspect plan_material_surface, the resolved MaterialPlan fields, "
            "and the runtime material plan serializer for this plan-level contract.")
    if likely_layer == "material-shape":
        return (
            "Inspect MaterialPlan.shape, MaterialGeometry analysis, and the "
            "backend radius upload path for stale shape metadata.")
    if likely_layer == "material-contract":
        return (
            "Inspect semantic material nodes, MaterialRect command emission, "
            "and renderer.material_plans[] for semantic/runtime parity.")
    if likely_layer == "material-command":
        return (
            "Inspect semantic material nodes, MaterialCommandDescriptor "
            "serialization, and backend MaterialRect command decoding.")
    if likely_layer == "material-container":
        return (
            "Inspect layout::material_container, MaterialSurfaceOptions "
            "container inheritance, MaterialRect container payload writes, "
            "and MaterialPlan.container serialization.")
    if likely_layer == "material-or-backdrop-pass":
        return (
            "Inspect renderer.material_plans[], the backend material/backdrop "
            "pass output, and the named frame region before changing thresholds.")
    if likely_layer.startswith("material."):
        return (
            "Inspect plan_material_surface and the resolved MaterialPlan "
            "fields for this material plan id.")
    if likely_layer == "material-decision":
        return (
            "Inspect MaterialPlan.decision_trace, fallback_path, and the "
            "pure planner gate that selected the fallback.")
    if likely_layer == "material-verifier":
        return (
            "Inspect MaterialPlan.verifier, semantic verifier_profile, and "
            "primary_pass.likely_layer/name.")
    if likely_layer == "material-observation":
        return (
            "Inspect MaterialPlan.observation_contract, the pure plan fields it "
            "mirrors, and the runtime material plan serializer.")
    if likely_layer == "material-pass":
        return (
            "Inspect MaterialPlan.primary_pass and renderer.material_plans[].passes "
            "for pass name, executor, and texture-copy drift.")
    if likely_layer == "material-backdrop":
        return (
            "Inspect MaterialPlan.backdrop and the backend backdrop descriptor "
            "captured for this frame.")
    if likely_layer == "material-foreground":
        return (
            "Inspect MaterialPlan.foreground, foreground contrast selection, "
            "and the material style theme tokens used by the pure planner.")
    if likely_layer == "material-reference":
        return (
            "Inspect MaterialPlan.reference_model, plan_material_surface, and "
            "the Apple Liquid Glass alignment fields before changing backend policy.")
    if likely_layer == "material-render-target":
        return (
            "Inspect MaterialPlan.render_target and backend render-target "
            "metadata before checking blur output.")
    if likely_layer == "application-debug":
        return (
            "Inspect the application debug payload provider, the shared model "
            "snapshot serializer, and the artifact manifest path.")
    if likely_layer == "platform-runtime":
        return (
            "Inspect debug.platform_runtime.details and the backend runtime "
            "JSON serializer for the reported path.")
    if likely_layer == "frame-capture":
        return (
            "Inspect frame capture dimensions, frame.bmp validity, and the "
            "manifest region rectangle.")
    if likely_layer == "artifact-manifest":
        return (
            "Inspect artifact_manifest.json for an invalid path, metric, or "
            "region contract.")
    if path.startswith("debug.platform_runtime.details.renderer.material_plans"):
        return (
            "Inspect renderer.material_plans[] and the pure MaterialPlan "
            "contract that feeds the artifact verifier.")
    if path:
        return f"Inspect {path} in snapshot.json and the matching runtime writer."
    return "Inspect the first failing check and its surrounding artifact bundle."


class Report:
    def __init__(self, bundle: Path) -> None:
        self.data: JsonObject = {
            "ok": False,
            "bundle": str(bundle),
            "checks": [],
            "errors": [],
            "failures": [],
            "warnings": [],
        }

    def check(
        self,
        name: str,
        condition: bool,
        detail: str = "",
        *,
        path: str = "",
        expected: Any = None,
        actual: Any = None,
        region: str = "",
        likely_layer: str = "",
        likely_pass: str = "",
        hint: str = "",
        record_success: bool = True,
    ) -> bool:
        check: JsonObject = {
            "name": name,
            "ok": condition,
            "detail": detail,
        }
        if path:
            check["path"] = path
        if expected is not None:
            check["expected"] = expected
        if actual is not None:
            check["actual"] = actual
        if region:
            check["region"] = region
        if likely_layer:
            check["likely_layer"] = likely_layer
        if likely_pass:
            check["likely_pass"] = likely_pass
        if hint:
            check["hint"] = hint
        if condition and not record_success:
            return True
        self.data["checks"].append(check)
        if not condition:
            message = name if not detail else f"{name}: {detail}"
            self.data["errors"].append(message)
            failure: JsonObject = {
                "failure_type": "artifact-check",
                "name": name,
                "message": message,
            }
            if path:
                failure["path"] = path
            if expected is not None:
                failure["expected"] = expected
            if actual is not None:
                failure["actual"] = actual
            if region:
                failure["region"] = region
            if likely_layer:
                failure["likely_layer"] = likely_layer
            if likely_pass:
                failure["likely_pass"] = likely_pass
            if hint:
                failure["hint"] = hint
            failure["suggested_action"] = suggested_action_for_failure(
                path,
                likely_layer,
                likely_pass,
                region)
            self.data["failures"].append(failure)
        return condition

    def warn(self, message: str) -> None:
        self.data["warnings"].append(message)

    def finish(self) -> int:
        self.data["ok"] = len(self.data["errors"]) == 0
        failures = self.data.get("failures", [])
        if isinstance(failures, list):
            by_layer: JsonObject = {}
            by_pass: JsonObject = {}
            by_path: JsonObject = {}
            by_action: JsonObject = {}
            for failure in failures:
                if not isinstance(failure, dict):
                    continue
                layer = failure.get("likely_layer")
                if isinstance(layer, str) and layer:
                    by_layer[layer] = by_layer.get(layer, 0) + 1
                likely_pass = failure.get("likely_pass")
                if isinstance(likely_pass, str) and likely_pass:
                    by_pass[likely_pass] = by_pass.get(likely_pass, 0) + 1
                path = failure.get("path")
                if isinstance(path, str) and path:
                    by_path[path] = by_path.get(path, 0) + 1
                action = failure.get("suggested_action")
                if isinstance(action, str) and action:
                    by_action[action] = by_action.get(action, 0) + 1
            self.data["failure_summary"] = {
                "count": len(failures),
                "by_likely_layer": by_layer,
                "by_likely_pass": by_pass,
                "by_path": by_path,
                "by_suggested_action": by_action,
            }
            if failures:
                first = failures[0]
                if isinstance(first, dict):
                    self.data["failure_summary"]["first_failure"] = {
                        key: first[key]
                        for key in (
                            "name",
                            "path",
                            "expected",
                            "actual",
                            "region",
                            "likely_layer",
                            "likely_pass",
                            "hint",
                            "suggested_action",
                        )
                        if key in first
                    }
                artifact_context = self.data.get("artifact_context")
                if isinstance(artifact_context, dict):
                    self.data["failure_summary"]["artifact_context"] = (
                        artifact_context)
            if by_layer:
                self.data["failure_summary"]["top_likely_layer"] = max(
                    by_layer.items(),
                    key=lambda item: (item[1], item[0]))[0]
            if by_pass:
                self.data["failure_summary"]["top_likely_pass"] = max(
                    by_pass.items(),
                    key=lambda item: (item[1], item[0]))[0]
            if by_action:
                self.data["failure_summary"]["top_suggested_action"] = max(
                    by_action.items(),
                    key=lambda item: (item[1], item[0]))[0]
        json.dump(self.data, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
        return 0 if self.data["ok"] else 1


def load_json_file(path: Path, report: Report) -> Any:
    try:
        with path.open("r", encoding="utf-8") as handle:
            return json.load(handle)
    except OSError as exc:
        report.check(f"read {path.name}", False, str(exc))
    except json.JSONDecodeError as exc:
        report.check(f"parse {path.name}", False, str(exc))
    return None


def object_at(value: Any, path: str) -> JsonObject | None:
    current = value
    for part in path.split("."):
        if not isinstance(current, dict):
            return None
        current = current.get(part)
    return current if isinstance(current, dict) else None


def value_at(value: Any, path: str) -> tuple[bool, Any]:
    current = value
    for part in path.split("."):
        if not isinstance(current, dict) or part not in current:
            return False, None
        current = current[part]
    return True, current


def material_failure_context(
    capabilities: JsonObject,
    runtime: JsonObject,
    semantic_summary: JsonObject,
    renderer_details: JsonObject | None,
    material_plan_summary: JsonObject | None,
) -> JsonObject:
    context: JsonObject = {}
    platform = string_at(capabilities, "platform")
    if platform:
        context["platform"] = platform
    backend = runtime.get("backend")
    if isinstance(backend, str):
        context["backend"] = backend
    viewport = runtime.get("viewport")
    if isinstance(viewport, dict):
        context["viewport"] = {
            key: viewport.get(key)
            for key in ("x", "y", "w", "h", "valid")
            if key in viewport
        }

    material_contract: JsonObject = {
        "semantic_material_nodes": semantic_summary.get("material_nodes"),
        "semantic_material_fallback_nodes": semantic_summary.get(
            "material_fallback_nodes"),
        "semantic_material_kinds": semantic_summary.get("material_kinds"),
        "semantic_material_roles": semantic_summary.get("material_roles"),
        "semantic_material_container_modes": semantic_summary.get(
            "material_container_modes"),
        "semantic_material_container_ids": semantic_summary.get(
            "material_container_ids"),
        "semantic_verifier_profiles": semantic_summary.get(
            "material_verifier_profiles"),
    }
    if isinstance(renderer_details, dict):
        material_contract["renderer_plan_contract_version"] = (
            renderer_details.get("material_plan_contract_version"))
        material_contract["renderer_plan_count"] = renderer_details.get(
            "material_plan_count")
        material_contract["renderer_plans_present"] = isinstance(
            renderer_details.get("material_plans"),
            list)
        material_contract["renderer_runtime_summary_present"] = isinstance(
            renderer_details.get("material_runtime_summary"),
            dict)
        material_contract["renderer_executor_summary_present"] = isinstance(
            renderer_details.get("material_executor_summary"),
            dict)
    if isinstance(material_plan_summary, dict):
        decision_trace = material_plan_summary.get("decision_trace")
        if not isinstance(decision_trace, dict):
            decision_trace = {}
        material_contract["resolved_plan_count"] = material_plan_summary.get(
            "count")
        material_contract["plan_contract_versions"] = (
            material_plan_summary.get("contract_versions"))
        material_contract["plan_surface_roles"] = (
            material_plan_summary.get("roles"))
        material_contract["plan_container"] = (
            material_plan_summary.get("container"))
        material_contract["plan_reference_model"] = (
            material_plan_summary.get("reference_model"))
        material_contract["plan_shape"] = material_plan_summary.get("shape")
        material_contract["fallback_paths"] = material_plan_summary.get(
            "fallback_paths")
        material_contract["pass_executors"] = material_plan_summary.get(
            "pass_executors")
        material_contract["foreground"] = material_plan_summary.get(
            "foreground")
        material_contract["theme"] = material_plan_summary.get("theme")
        material_contract["decision_first_blockers"] = decision_trace.get(
            "first_blockers")
        material_contract["decision_reduced_transparency"] = (
            decision_trace.get("reduced_transparency"))
        material_contract["decision_increase_contrast"] = decision_trace.get(
            "increase_contrast")
        material_contract["decision_reduce_motion"] = decision_trace.get(
            "reduce_motion")
    context["material_contract"] = material_contract
    return context


def bool_at(value: JsonObject, key: str) -> bool | None:
    item = value.get(key)
    return item if isinstance(item, bool) else None


def string_at(value: JsonObject, key: str) -> str | None:
    item = value.get(key)
    return item if isinstance(item, str) else None


def number_at(value: JsonObject, key: str) -> int | float | None:
    item = value.get(key)
    return item if isinstance(item, (int, float)) and not isinstance(item, bool) else None


def material_container_from(value: JsonObject) -> JsonObject | None:
    container = value.get("container")
    if not isinstance(container, dict):
        return None
    mode = string_at(container, "mode")
    container_id = number_at(container, "container_id")
    union_id = number_at(container, "union_id")
    spacing = number_at(container, "spacing")
    interactive = container.get("interactive")
    morph_transitions = container.get("morph_transitions")
    if (not mode
            or container_id is None
            or union_id is None
            or spacing is None
            or not isinstance(interactive, bool)
            or not isinstance(morph_transitions, bool)):
        return None
    return {
        "mode": mode,
        "container_id": int(container_id),
        "union_id": int(union_id),
        "spacing": float(spacing),
        "interactive": interactive,
        "morph_transitions": morph_transitions,
    }


def material_descriptor_from(value: JsonObject) -> JsonObject | None:
    kind = string_at(value, "kind")
    role = string_at(value, "role")
    tint = value.get("tint")
    container = material_container_from(value)
    if not kind or not role or container is None or not isinstance(tint, dict):
        return None
    descriptor: JsonObject = {
        "kind": kind,
        "role": role,
        "container": container,
    }
    tint_descriptor: JsonObject = {}
    for key in ("r", "g", "b", "a"):
        channel = number_at(tint, key)
        if channel is None:
            return None
        tint_descriptor[key] = int(channel)
    descriptor["tint"] = tint_descriptor
    for key in MATERIAL_COMMAND_DESCRIPTOR_NUMERIC_FIELDS:
        number = number_at(value, key)
        if number is None:
            return None
        descriptor[key] = float(number)
    return descriptor


def descriptor_signature(value: JsonObject) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def container_identity_signature(value: JsonObject | None) -> str | None:
    if not isinstance(value, dict):
        return None
    return descriptor_signature({
        "mode": value.get("mode"),
        "container_id": value.get("container_id"),
        "union_id": value.get("union_id"),
        "spacing": value.get("spacing"),
        "interactive": value.get("interactive"),
    })


def check_object_field(
    report: Report,
    value: JsonObject,
    key: str,
    path: str,
    *,
    likely_layer: str,
    hint: str,
) -> JsonObject | None:
    item = value.get(key)
    report.check(
        f"{path}.{key} is object",
        isinstance(item, dict),
        path=f"{path}.{key}",
        expected="object",
        actual=type(item).__name__,
        likely_layer=likely_layer,
        hint=hint,
        record_success=False)
    return item if isinstance(item, dict) else None


def check_bool_field(
    report: Report,
    value: JsonObject,
    key: str,
    path: str,
    *,
    likely_layer: str,
    likely_pass: str = "",
    hint: str,
) -> bool | None:
    item = value.get(key)
    report.check(
        f"{path}.{key} is boolean",
        isinstance(item, bool),
        path=f"{path}.{key}",
        expected="boolean",
        actual=item,
        likely_layer=likely_layer,
        likely_pass=likely_pass,
        hint=hint,
        record_success=False)
    return item if isinstance(item, bool) else None


def check_number_field(
    report: Report,
    value: JsonObject,
    key: str,
    path: str,
    *,
    min_value: float | None = None,
    max_value: float | None = None,
    likely_layer: str,
    likely_pass: str = "",
    hint: str,
) -> int | float | None:
    item = value.get(key)
    ok = isinstance(item, (int, float)) and not isinstance(item, bool)
    if ok and min_value is not None:
        ok = float(item) >= min_value
    if ok and max_value is not None:
        ok = float(item) <= max_value
    expected: Any = "number"
    if min_value is not None or max_value is not None:
        expected = {"type": "number"}
        if min_value is not None:
            expected[">="] = min_value
        if max_value is not None:
            expected["<="] = max_value
    report.check(
        f"{path}.{key} is valid number",
        ok,
        path=f"{path}.{key}",
        expected=expected,
        actual=item,
        likely_layer=likely_layer,
        likely_pass=likely_pass,
        hint=hint,
        record_success=False)
    return item if isinstance(item, (int, float)) and not isinstance(item, bool) else None


def check_string_field(
    report: Report,
    value: JsonObject,
    key: str,
    path: str,
    *,
    allow_empty: bool = False,
    likely_layer: str,
    hint: str,
) -> str | None:
    item = value.get(key)
    ok = isinstance(item, str) and (allow_empty or bool(item))
    report.check(
        f"{path}.{key} is valid string",
        ok,
        path=f"{path}.{key}",
        expected="string" if allow_empty else "non-empty string",
        actual=item,
        likely_layer=likely_layer,
        hint=hint,
        record_success=False)
    return item if isinstance(item, str) else None


def check_file_explorer_native_chrome_contract(
    report: Report,
    debug: JsonObject,
) -> None:
    file_explorer = object_at(debug, "application.file_explorer")
    if file_explorer is None:
        return
    chrome = object_at(file_explorer, "chrome")
    if chrome is None:
        return
    native_window = object_at(chrome, "native_window")
    if native_window is None:
        return

    native_controls = native_window.get("native_window_controls") is True
    marker_fields = {
        "duplicate_window_controls": native_window.get("duplicate_window_controls"),
        "window_control_single_owner": native_window.get(
            "window_control_single_owner"),
        "content_window_control_markers": native_window.get("content_window_control_markers"),
        "artifact_window_control_markers": native_window.get("artifact_window_control_markers"),
        "window_control_duplication_guard": native_window.get(
            "window_control_duplication_guard"),
        "content_window_control_marker_count": native_window.get(
            "content_window_control_marker_count"),
        "artifact_window_control_marker_count": native_window.get(
            "artifact_window_control_marker_count"),
        "content_drawn_window_control_count": native_window.get(
            "content_drawn_window_control_count", 0),
        "artifact_drawn_window_control_count": native_window.get(
            "artifact_drawn_window_control_count", 0),
        "native_window_control_geometry_role": native_window.get(
            "native_window_control_geometry_role"),
        "native_window_control_palette_policy": native_window.get(
            "native_window_control_palette_policy"),
    }
    no_drawn_controls = (
        marker_fields["duplicate_window_controls"] is False
        and marker_fields["window_control_single_owner"] is True
        and marker_fields["content_window_control_markers"] is False
        and marker_fields["artifact_window_control_markers"] is False
        and marker_fields["window_control_duplication_guard"] in (
            "native_window_controls_single_owner",
            "no_window_controls_in_shell",
            "not_applicable_mobile_shell")
        and marker_fields["content_window_control_marker_count"] == 0
        and marker_fields["artifact_window_control_marker_count"] == 0
        and marker_fields["content_drawn_window_control_count"] == 0
        and marker_fields["artifact_drawn_window_control_count"] == 0)
    expected_geometry_role = (
        "reserve_metrics_only_not_paint_instructions"
        if native_controls
        else ["not_applicable_mobile_shell", "none"])
    expected_palette_policy = (
        "traffic_light_palette_forbidden_in_content_and_artifacts"
        if native_controls
        else ["not_applicable_mobile_shell", "none"])
    policy_ok = (
        marker_fields["native_window_control_geometry_role"]
        == "reserve_metrics_only_not_paint_instructions"
        and marker_fields["native_window_control_palette_policy"]
        == "traffic_light_palette_forbidden_in_content_and_artifacts"
    ) if native_controls else (
        marker_fields["native_window_control_geometry_role"]
        in expected_geometry_role
        and marker_fields["native_window_control_palette_policy"]
        in expected_palette_policy)
    report.check(
        "file explorer does not draw duplicate native window controls",
        no_drawn_controls and policy_ok,
        path="debug.application.file_explorer.chrome.native_window",
        expected={
            "duplicate_window_controls": False,
            "window_control_single_owner": True,
            "content_window_control_markers": False,
            "artifact_window_control_markers": False,
            "window_control_duplication_guard": [
                "native_window_controls_single_owner",
                "no_window_controls_in_shell",
                "not_applicable_mobile_shell"],
            "content_window_control_marker_count": 0,
            "artifact_window_control_marker_count": 0,
            "content_drawn_window_control_count": 0,
            "artifact_drawn_window_control_count": 0,
            "native_window_control_geometry_role": expected_geometry_role,
            "native_window_control_palette_policy": expected_palette_policy,
        },
        actual=marker_fields,
        likely_layer="native-window-chrome",
        likely_pass="window-control-marker",
        hint=(
            "File explorer content and artifacts must reserve the titlebar "
            "area; macOS traffic lights or Windows caption buttons are owned "
            "by the native window edge. The titlebar control geometry is a "
            "reserve-only contract, not an instruction to draw colored "
            "traffic-light controls."))
    if native_controls:
        report.check(
            "file explorer native window controls are platform-owned",
            native_window.get("native_window_control_owner") == "platform-edge"
            and native_window.get("window_control_single_owner") is True
            and native_window.get("window_control_duplication_guard")
            == "native_window_controls_single_owner"
            and native_window.get("window_control_marker_mode")
            == "runtime-native-controls"
            and isinstance(native_window.get("native_window_control_count"), int)
            and native_window.get("native_window_control_count") > 0
            and native_window.get("native_window_control_geometry_role")
            == "reserve_metrics_only_not_paint_instructions"
            and native_window.get("native_window_control_palette_policy")
            == "traffic_light_palette_forbidden_in_content_and_artifacts",
            path="debug.application.file_explorer.chrome.native_window",
            expected={
                "native_window_control_owner": "platform-edge",
                "window_control_single_owner": True,
                "window_control_duplication_guard":
                    "native_window_controls_single_owner",
                "window_control_marker_mode": "runtime-native-controls",
                "native_window_control_count": ">0",
                "native_window_control_geometry_role":
                    "reserve_metrics_only_not_paint_instructions",
                "native_window_control_palette_policy": (
                    "traffic_light_palette_forbidden_in_content_and_artifacts"),
            },
            actual={
                "native_window_control_owner": native_window.get(
                    "native_window_control_owner"),
                "window_control_single_owner": native_window.get(
                    "window_control_single_owner"),
                "window_control_duplication_guard": native_window.get(
                    "window_control_duplication_guard"),
                "window_control_marker_mode": native_window.get(
                    "window_control_marker_mode"),
                "native_window_control_count": native_window.get(
                    "native_window_control_count"),
                "native_window_control_geometry_role": native_window.get(
                    "native_window_control_geometry_role"),
                "native_window_control_palette_policy": native_window.get(
                    "native_window_control_palette_policy"),
            },
            likely_layer="native-window-chrome",
            likely_pass="runtime-native-controls",
            hint=(
                "If traffic lights appear twice, inspect the shared "
                "ExplorerChromeMetrics native_window_control_owner fields "
                "before adding content-side controls."))

        runtime_window = object_at(debug, "platform_runtime.details.window")
        if isinstance(runtime_window, dict) and (
                runtime_window.get("surface_kind") == "macos_window"):
            runtime_controls = object_at(runtime_window, "native_window_controls")
            actual = runtime_controls if isinstance(runtime_controls, dict) else None
            report.check(
                "file explorer native window buttons are integrated into content chrome",
                isinstance(actual, dict)
                and actual.get("ownership_policy")
                == "platform_edge_standard_buttons_only"
                and actual.get("integration_policy")
                == "standard_buttons_inside_leading_content_reserve"
                and actual.get("expected_count") == 3
                and actual.get("present_count") == 3
                and actual.get("visible_count") == 3
                and actual.get("hidden_count") == 0
                and actual.get("within_leading_reserve_count") == 3
                and actual.get("all_buttons_within_leading_reserve") is True
                and actual.get("integrated_in_content_area") is True
                and actual.get("duplicate_window_controls") is False
                and actual.get("window_control_single_owner") is True
                and actual.get("window_control_duplication_guard")
                == "native_window_controls_single_owner"
                and actual.get("content_drawn_window_control_count") == 0
                and actual.get("artifact_drawn_window_control_count") == 0,
                path="debug.platform_runtime.details.window.native_window_controls",
                expected={
                    "ownership_policy": "platform_edge_standard_buttons_only",
                    "integration_policy": (
                        "standard_buttons_inside_leading_content_reserve"),
                    "expected_count": 3,
                    "present_count": 3,
                    "visible_count": 3,
                    "hidden_count": 0,
                    "within_leading_reserve_count": 3,
                    "all_buttons_within_leading_reserve": True,
                    "integrated_in_content_area": True,
                    "duplicate_window_controls": False,
                    "window_control_single_owner": True,
                    "window_control_duplication_guard":
                        "native_window_controls_single_owner",
                    "content_drawn_window_control_count": 0,
                    "artifact_drawn_window_control_count": 0,
                },
                actual=actual,
                likely_layer="native-window-chrome",
                likely_pass="appkit-standard-window-buttons",
                hint=(
                    "The live AppKit traffic lights must be the only window "
                    "buttons and must sit inside the leading content reserve; "
                    "otherwise users see a separate titlebar or duplicated "
                    "content-side controls."))


def list_of_strings(value: Any, key: str) -> list[str]:
    item = value.get(key) if isinstance(value, dict) else None
    if item is None:
        return []
    if not isinstance(item, list) or not all(isinstance(entry, str) for entry in item):
        raise ValueError(f"{key} must be a list of strings")
    return item


def bool_manifest_value(value: JsonObject, key: str) -> bool | None:
    item = value.get(key)
    if item is None:
        return None
    if not isinstance(item, bool):
        raise ValueError(f"{key} must be a boolean")
    return item


def int_manifest_value(value: JsonObject, key: str) -> int | None:
    item = value.get(key)
    if item is None:
        return None
    if not isinstance(item, int):
        raise ValueError(f"{key} must be an integer")
    return item


def pixel_region_spec_from_manifest(region: Any) -> str:
    if not isinstance(region, dict):
        raise ValueError("pixel_regions entries must be objects")
    name = region.get("name")
    rect = region.get("rect")
    min_delta = region.get("min_luma_delta", 16.0)
    min_unique = region.get("min_unique_colors", 2)
    if not isinstance(name, str) or not name:
        raise ValueError("pixel region name must be a non-empty string")
    if (
        not isinstance(rect, list)
        or len(rect) != 4
        or not all(isinstance(value, (int, float)) for value in rect)
    ):
        raise ValueError("pixel region rect must be a list of four numbers")
    if not isinstance(min_delta, (int, float)):
        raise ValueError("pixel region min_luma_delta must be a number")
    if not isinstance(min_unique, int):
        raise ValueError("pixel region min_unique_colors must be an integer")
    coords = ",".join(str(float(value)) for value in rect)
    return f"{name}:{coords}:{float(min_delta)}:{min_unique}"


def pixel_region_metric_specs_from_manifest(value: Any) -> list[JsonObject]:
    if value is None:
        return []
    if not isinstance(value, list):
        raise ValueError("pixel_region_metrics must be a list")
    specs: list[JsonObject] = []
    for index, entry in enumerate(value):
        if not isinstance(entry, dict):
            raise ValueError(f"pixel_region_metrics[{index}] must be an object")
        region = entry.get("region")
        metric = entry.get("metric")
        if not isinstance(region, str) or not region:
            raise ValueError(f"pixel_region_metrics[{index}].region must be a non-empty string")
        if metric not in ALLOWED_PIXEL_REGION_METRICS:
            raise ValueError(
                f"pixel_region_metrics[{index}].metric must be one of "
                + ", ".join(sorted(ALLOWED_PIXEL_REGION_METRICS)))
        spec: JsonObject = {"region": region, "metric": metric}
        has_bound = False
        for bound in ("gte", "lte"):
            if bound in entry:
                spec[bound] = non_negative_number(
                    entry[bound],
                    f"pixel_region_metrics[{index}].{bound}")
                has_bound = True
        if not has_bound:
            raise ValueError(
                f"pixel_region_metrics[{index}] must contain gte or lte")
        specs.append(spec)
    return specs


def pixel_region_metric_comparison_specs_from_manifest(value: Any) -> list[JsonObject]:
    if value is None:
        return []
    if not isinstance(value, list):
        raise ValueError("pixel_region_metric_comparisons must be a list")
    specs: list[JsonObject] = []
    allowed = {
        "region",
        "metric",
        "reference_region",
        "reference_metric",
        "gte_ratio",
        "lte_ratio",
    }
    for index, entry in enumerate(value):
        if not isinstance(entry, dict):
            raise ValueError(
                f"pixel_region_metric_comparisons[{index}] must be an object")
        unknown = sorted(set(entry) - allowed)
        if unknown:
            raise ValueError(
                f"unknown pixel_region_metric_comparisons[{index}] keys: "
                + ", ".join(unknown))
        region = entry.get("region")
        metric = entry.get("metric")
        reference_region = entry.get("reference_region")
        reference_metric = entry.get("reference_metric", metric)
        if not isinstance(region, str) or not region:
            raise ValueError(
                f"pixel_region_metric_comparisons[{index}].region must be a non-empty string")
        if metric not in ALLOWED_PIXEL_REGION_METRICS:
            raise ValueError(
                f"pixel_region_metric_comparisons[{index}].metric must be one of "
                + ", ".join(sorted(ALLOWED_PIXEL_REGION_METRICS)))
        if not isinstance(reference_region, str) or not reference_region:
            raise ValueError(
                f"pixel_region_metric_comparisons[{index}].reference_region must be a non-empty string")
        if reference_metric not in ALLOWED_PIXEL_REGION_METRICS:
            raise ValueError(
                f"pixel_region_metric_comparisons[{index}].reference_metric must be one of "
                + ", ".join(sorted(ALLOWED_PIXEL_REGION_METRICS)))
        spec: JsonObject = {
            "region": region,
            "metric": metric,
            "reference_region": reference_region,
            "reference_metric": reference_metric,
        }
        has_bound = False
        for bound in ("gte_ratio", "lte_ratio"):
            if bound in entry:
                spec[bound] = non_negative_number(
                    entry[bound],
                    f"pixel_region_metric_comparisons[{index}].{bound}")
                has_bound = True
        if not has_bound:
            raise ValueError(
                f"pixel_region_metric_comparisons[{index}] must contain gte_ratio or lte_ratio")
        specs.append(spec)
    return specs


def color_channel(value: Any, field: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < 0 or value > 255:
        raise ValueError(f"{field} must be an integer between 0 and 255")
    return value


def pixel_region_forbidden_color_specs_from_manifest(value: Any) -> list[JsonObject]:
    if value is None:
        return []
    if not isinstance(value, list):
        raise ValueError("forbid_pixel_region_colors must be a list")
    specs: list[JsonObject] = []
    for index, entry in enumerate(value):
        if not isinstance(entry, dict):
            raise ValueError(f"forbid_pixel_region_colors[{index}] must be an object")
        region = entry.get("region")
        if not isinstance(region, str) or not region:
            raise ValueError(
                f"forbid_pixel_region_colors[{index}].region must be a non-empty string")
        name = entry.get("name", "forbidden-color")
        if not isinstance(name, str) or not name:
            raise ValueError(
                f"forbid_pixel_region_colors[{index}].name must be a non-empty string")
        rgb = entry.get("rgb")
        if not isinstance(rgb, list) or len(rgb) != 3:
            raise ValueError(
                f"forbid_pixel_region_colors[{index}].rgb must contain three channels")
        tolerance = non_negative_int(
            entry.get("tolerance", 0),
            f"forbid_pixel_region_colors[{index}].tolerance")
        max_pixels = non_negative_int(
            entry.get("max_pixels", 0),
            f"forbid_pixel_region_colors[{index}].max_pixels")
        specs.append({
            "region": region,
            "name": name,
            "rgb": [
                color_channel(rgb[0], f"forbid_pixel_region_colors[{index}].rgb[0]"),
                color_channel(rgb[1], f"forbid_pixel_region_colors[{index}].rgb[1]"),
                color_channel(rgb[2], f"forbid_pixel_region_colors[{index}].rgb[2]"),
            ],
            "tolerance": tolerance,
            "max_pixels": max_pixels,
        })
    return specs


def pixel_region_metric_spec_from_cli(spec: str) -> JsonObject:
    parts = spec.split(":")
    if len(parts) != 4:
        raise argparse.ArgumentTypeError(
            "expected REGION:METRIC:BOUND:VALUE")
    region, metric, bound, raw_value = parts
    if not region:
        raise argparse.ArgumentTypeError("REGION must be non-empty")
    if metric not in ALLOWED_PIXEL_REGION_METRICS:
        raise argparse.ArgumentTypeError(
            "METRIC must be one of "
            + ", ".join(sorted(ALLOWED_PIXEL_REGION_METRICS)))
    if bound not in ("gte", "lte"):
        raise argparse.ArgumentTypeError("BOUND must be gte or lte")
    try:
        value = float(raw_value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("VALUE must be a number") from exc
    if value < 0.0:
        raise argparse.ArgumentTypeError("VALUE must be non-negative")
    return {"region": region, "metric": metric, bound: value}


def runtime_detail_spec_from_manifest(entry: Any) -> str:
    if not isinstance(entry, dict):
        raise ValueError("require_runtime_details entries must be objects")
    path = entry.get("path")
    if not isinstance(path, str) or not path:
        raise ValueError("runtime detail path must be a non-empty string")
    if "equals" not in entry:
        raise ValueError("runtime detail entry must contain equals")
    expected = json.dumps(entry["equals"], separators=(",", ":"))
    return f"{path}={expected}"


def debug_detail_spec_from_manifest(entry: Any) -> str:
    if not isinstance(entry, dict):
        raise ValueError("require_debug_details entries must be objects")
    path = entry.get("path")
    if not isinstance(path, str) or not path:
        raise ValueError("debug detail path must be a non-empty string")
    if "equals" not in entry:
        raise ValueError("debug detail entry must contain equals")
    expected = json.dumps(entry["equals"], separators=(",", ":"))
    return f"{path}={expected}"


def runtime_numeric_bound_spec_from_manifest(entry: Any, index: int) -> JsonObject:
    if not isinstance(entry, dict):
        raise ValueError("require_runtime_numeric_bounds entries must be objects")
    path = entry.get("path")
    if not isinstance(path, str) or not path:
        raise ValueError(
            f"require_runtime_numeric_bounds[{index}].path must be a non-empty string")
    spec: JsonObject = {"path": path}
    has_bound = False
    for field in ("equals", "gte", "lte"):
        if field in entry:
            spec[field] = non_negative_number(
                entry[field],
                f"require_runtime_numeric_bounds[{index}].{field}")
            has_bound = True
    if not has_bound:
        raise ValueError(
            f"require_runtime_numeric_bounds[{index}] must contain equals, gte, or lte")
    return spec


def runtime_numeric_bound_specs_from_manifest(value: Any) -> list[JsonObject]:
    if value is None:
        return []
    if not isinstance(value, list):
        raise ValueError("require_runtime_numeric_bounds must be a list")
    return [
        runtime_numeric_bound_spec_from_manifest(entry, index)
        for index, entry in enumerate(value)
    ]


def non_negative_int(value: Any, field: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise ValueError(f"{field} must be a non-negative integer")
    return value


def non_negative_number(value: Any, field: str) -> int | float:
    if (not isinstance(value, (int, float))
            or isinstance(value, bool)
            or value < 0):
        raise ValueError(f"{field} must be a non-negative number")
    return value


def string_int_map(
    value: Any,
    field: str,
    allowed_keys: set[str] | None = None,
) -> JsonObject:
    if not isinstance(value, dict):
        raise ValueError(f"{field} must be an object")
    out: JsonObject = {}
    for key, count in value.items():
        if not isinstance(key, str) or not key:
            raise ValueError(f"{field} keys must be non-empty strings")
        if allowed_keys is not None and key not in allowed_keys:
            raise ValueError(
                f"{field}.{key} must be one of "
                + ", ".join(sorted(allowed_keys)))
        out[key] = non_negative_int(count, f"{field}.{key}")
    return out


def string_string_map(
    value: Any,
    field: str,
    allowed_values: set[str] | None = None,
) -> JsonObject:
    if not isinstance(value, dict):
        raise ValueError(f"{field} must be an object")
    out: JsonObject = {}
    for key, item in value.items():
        if not isinstance(key, str) or not key:
            raise ValueError(f"{field} keys must be non-empty strings")
        if not isinstance(item, str) or not item:
            raise ValueError(f"{field}.{key} must be a non-empty string")
        if allowed_values is not None and item not in allowed_values:
            raise ValueError(
                f"{field}.{key} must be one of "
                + ", ".join(sorted(allowed_values)))
        out[key] = item
    return out


def material_plan_summary_spec_from_manifest(value: Any) -> JsonObject | None:
    if value is None:
        return None
    if not isinstance(value, dict):
        raise ValueError("require_material_plan_summary must be an object")
    allowed = {
        "count",
        "min_count",
        "fallback",
        "backdrop_sampling",
        "contract_versions",
        "fallback_paths",
        "fallback_reasons",
        "kinds",
        "roles",
        "container_modes",
        "container_ids",
        "union_ids",
        "container_participating",
        "container_unioned",
        "container_interactive",
        "container_morph_transitions",
        "reference_view_bounds_anchored",
        "reference_shape_matches_geometry",
        "reference_tint_applied",
        "reference_interactive_response",
        "reference_container_grouped",
        "reference_container_union",
        "reference_container_morphing",
        "reference_legibility_preserved",
        "reference_vibrancy_expected",
        "reference_deterministic_degradation",
        "reference_technologies",
        "reference_layers",
        "reference_material_policies",
        "reference_variants",
        "reference_shapes",
        "reference_shape_scopes",
        "reference_blending_scopes",
        "reference_semantic_thickness",
        "reference_accessibility_responses",
        "reference_performance_responses",
        "shape_valid",
        "shape_rounded",
        "shape_capsule",
        "shape_radius_clamped",
        "shape_max_surface_area_lte",
        "shape_max_surface_area_gte",
        "shape_max_effective_radius_lte",
        "shape_max_effective_radius_gte",
        "shape_max_radius_limit_lte",
        "shape_max_radius_limit_gte",
        "shape_max_normalized_radius_lte",
        "shape_max_normalized_radius_gte",
        "pass_names",
        "pass_executors",
        "stage_names",
        "stage_executors",
        "sampling_kernels",
        "sampling_weight_profiles",
        "luminance_curves",
        "backdrop_available",
        "backdrop_stable",
        "backdrop_excludes_foreground_text",
        "backdrop_access_required",
        "backdrop_access_stable_required",
        "backdrop_access_frame_history_required",
        "backdrop_access_shared_frame_capture",
        "backdrop_access_next_frame_capture_required",
        "backdrop_access_excludes_foreground_text",
        "backdrop_access_bounded",
        "backdrop_sources",
        "backdrop_access_sources",
        "backdrop_capture_scopes",
        "backdrop_capture_reasons",
        "luminance_responses",
        "luminance_adapted",
        "foreground_schemes",
        "foreground_sources",
        "foreground_backdrop_driven",
        "foreground_high_contrast",
        "foreground_vibrant",
        "foreground_deterministic",
        "foreground_min_primary_contrast_gte",
        "foreground_minimum_contrast_gte",
        "theme_foreground_matches_theme",
        "theme_accent_matches_theme",
        "theme_tint_matches_surface",
        "theme_border_matches_theme",
        "theme_default_glass_tokens",
        "theme_profile_names",
        "theme_sources",
        "theme_token_policies",
        "render_target_ready",
        "render_target_within_backdrop_budget",
        "render_target_pixel_formats",
        "decision_can_sample_backdrop",
        "decision_role_allows_liquid_glass",
        "decision_content_layer_standard_material",
        "decision_liquid_glass_backdrop_candidate",
        "decision_backend_supports_backdrop",
        "decision_backdrop_source_ready",
        "decision_next_frame_capture_required",
        "decision_reduced_transparency",
        "decision_increase_contrast",
        "decision_reduce_motion",
        "decision_blockers",
        "verifier_require_backdrop_source",
        "verifier_require_edge_highlight",
        "verifier_require_container_identity",
        "verifier_require_container_morph_contract",
        "verifier_profiles",
        "verifier_region_layers",
        "verifier_region_passes",
    }
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise ValueError(
            "unknown require_material_plan_summary keys: "
            + ", ".join(unknown))
    spec: JsonObject = {}
    for field in (
            "count",
            "min_count",
            "fallback",
            "backdrop_sampling",
            "backdrop_available",
            "backdrop_stable",
            "backdrop_excludes_foreground_text",
            "backdrop_access_required",
            "backdrop_access_stable_required",
            "backdrop_access_frame_history_required",
            "backdrop_access_shared_frame_capture",
            "backdrop_access_next_frame_capture_required",
            "backdrop_access_excludes_foreground_text",
            "backdrop_access_bounded",
            "luminance_adapted",
            "render_target_ready",
            "render_target_within_backdrop_budget",
            "decision_role_allows_liquid_glass",
            "decision_content_layer_standard_material",
            "decision_liquid_glass_backdrop_candidate",
            "decision_can_sample_backdrop",
            "decision_backend_supports_backdrop",
            "decision_backdrop_source_ready",
            "decision_next_frame_capture_required",
            "decision_reduced_transparency",
            "decision_increase_contrast",
            "decision_reduce_motion",
            "container_participating",
            "container_unioned",
            "container_interactive",
            "container_morph_transitions",
            "reference_view_bounds_anchored",
            "reference_shape_matches_geometry",
            "reference_tint_applied",
            "reference_interactive_response",
            "reference_container_grouped",
            "reference_container_union",
            "reference_container_morphing",
            "reference_legibility_preserved",
            "reference_vibrancy_expected",
            "reference_deterministic_degradation",
            "shape_valid",
            "shape_rounded",
            "shape_capsule",
            "shape_radius_clamped",
            "foreground_backdrop_driven",
            "foreground_high_contrast",
            "foreground_vibrant",
            "foreground_deterministic",
            "theme_foreground_matches_theme",
            "theme_accent_matches_theme",
            "theme_tint_matches_surface",
            "theme_border_matches_theme",
            "theme_default_glass_tokens",
            "verifier_require_backdrop_source",
            "verifier_require_container_identity",
            "verifier_require_container_morph_contract",
            "verifier_require_edge_highlight"):
        if field in value:
            spec[field] = non_negative_int(
                value[field],
                f"require_material_plan_summary.{field}")
    for field in (
            "shape_max_surface_area_lte",
            "shape_max_surface_area_gte",
            "shape_max_effective_radius_lte",
            "shape_max_effective_radius_gte",
            "shape_max_radius_limit_lte",
            "shape_max_radius_limit_gte",
            "shape_max_normalized_radius_lte",
            "shape_max_normalized_radius_gte",
            "foreground_min_primary_contrast_gte",
            "foreground_minimum_contrast_gte"):
        if field in value:
            spec[field] = non_negative_number(
                value[field],
                f"require_material_plan_summary.{field}")
    map_vocabularies = {
        "fallback_paths": ALLOWED_MATERIAL_FALLBACK_PATHS,
        "kinds": ALLOWED_MATERIAL_KINDS,
        "roles": ALLOWED_MATERIAL_SURFACE_ROLES,
        "container_modes": ALLOWED_MATERIAL_CONTAINER_MODES,
        "reference_technologies": ALLOWED_MATERIAL_REFERENCE_TECHNOLOGIES,
        "reference_layers": ALLOWED_MATERIAL_REFERENCE_LAYERS,
        "reference_material_policies": ALLOWED_MATERIAL_REFERENCE_POLICIES,
        "reference_variants": ALLOWED_MATERIAL_KINDS,
        "reference_shapes": ALLOWED_MATERIAL_REFERENCE_SHAPES,
        "reference_shape_scopes": ALLOWED_MATERIAL_REFERENCE_SHAPE_SCOPES,
        "reference_blending_scopes": ALLOWED_MATERIAL_REFERENCE_BLENDING_SCOPES,
        "reference_semantic_thickness": ALLOWED_MATERIAL_KINDS,
        "reference_accessibility_responses": (
            ALLOWED_MATERIAL_REFERENCE_ACCESSIBILITY_RESPONSES),
        "reference_performance_responses": (
            ALLOWED_MATERIAL_REFERENCE_PERFORMANCE_RESPONSES),
        "pass_names": ALLOWED_MATERIAL_PASS_NAMES,
        "pass_executors": ALLOWED_MATERIAL_PASS_EXECUTORS,
        "stage_names": ALLOWED_MATERIAL_STAGE_NAMES,
        "stage_executors": ALLOWED_MATERIAL_STAGE_EXECUTORS,
        "sampling_kernels": ALLOWED_MATERIAL_SAMPLING_KERNELS,
        "sampling_weight_profiles": ALLOWED_MATERIAL_SAMPLING_WEIGHT_PROFILES,
        "luminance_curves": ALLOWED_MATERIAL_LUMINANCE_CURVES,
        "backdrop_capture_scopes": ALLOWED_MATERIAL_BACKDROP_CAPTURE_SCOPES,
        "luminance_responses": ALLOWED_MATERIAL_LUMINANCE_RESPONSES,
        "foreground_schemes": ALLOWED_MATERIAL_FOREGROUND_SCHEMES,
        "foreground_sources": ALLOWED_MATERIAL_FOREGROUND_SOURCES,
    }
    for field, allowed_keys in map_vocabularies.items():
        if field in value:
            spec[field] = string_int_map(
                value[field],
                f"require_material_plan_summary.{field}",
                allowed_keys)
    if "fallback_reasons" in value:
        spec["fallback_reasons"] = string_int_map(
            value["fallback_reasons"],
            "require_material_plan_summary.fallback_reasons")
    if "contract_versions" in value:
        spec["contract_versions"] = string_int_map(
            value["contract_versions"],
            "require_material_plan_summary.contract_versions")
    for field in (
            "theme_profile_names",
            "theme_sources",
            "theme_token_policies"):
        if field in value:
            spec[field] = string_int_map(
                value[field],
                f"require_material_plan_summary.{field}")
    if "container_ids" in value:
        spec["container_ids"] = string_int_map(
            value["container_ids"],
            "require_material_plan_summary.container_ids")
    if "union_ids" in value:
        spec["union_ids"] = string_int_map(
            value["union_ids"],
            "require_material_plan_summary.union_ids")
    if "backdrop_sources" in value:
        spec["backdrop_sources"] = string_int_map(
            value["backdrop_sources"],
            "require_material_plan_summary.backdrop_sources")
    if "backdrop_access_sources" in value:
        spec["backdrop_access_sources"] = string_int_map(
            value["backdrop_access_sources"],
            "require_material_plan_summary.backdrop_access_sources")
    if "backdrop_capture_reasons" in value:
        spec["backdrop_capture_reasons"] = string_int_map(
            value["backdrop_capture_reasons"],
            "require_material_plan_summary.backdrop_capture_reasons")
    if "render_target_pixel_formats" in value:
        spec["render_target_pixel_formats"] = string_int_map(
            value["render_target_pixel_formats"],
            "require_material_plan_summary.render_target_pixel_formats")
    if "decision_blockers" in value:
        spec["decision_blockers"] = string_int_map(
            value["decision_blockers"],
            "require_material_plan_summary.decision_blockers",
            ALLOWED_MATERIAL_FALLBACK_PATHS)
    if "verifier_profiles" in value:
        spec["verifier_profiles"] = string_int_map(
            value["verifier_profiles"],
            "require_material_plan_summary.verifier_profiles")
    if "verifier_region_layers" in value:
        spec["verifier_region_layers"] = string_string_map(
            value["verifier_region_layers"],
            "require_material_plan_summary.verifier_region_layers",
            ALLOWED_MATERIAL_LIKELY_LAYERS)
    if "verifier_region_passes" in value:
        spec["verifier_region_passes"] = string_string_map(
            value["verifier_region_passes"],
            "require_material_plan_summary.verifier_region_passes",
            ALLOWED_MATERIAL_PASS_NAMES)
    return spec


def material_resource_bounds_spec_from_manifest(value: Any) -> JsonObject | None:
    if value is None:
        return None
    if not isinstance(value, dict):
        raise ValueError("require_material_resource_bounds must be an object")
    number_fields = {
        "max_plan_blur_radius_lte",
        "max_plan_sample_taps_lte",
        "max_plan_sample_taps_gte",
        "total_plan_sample_taps_lte",
        "total_plan_sample_taps_gte",
        "max_budget_blur_radius_lte",
        "max_sample_taps_lte",
        "max_sampling_kernel_radius_lte",
        "max_sampling_kernel_radius_gte",
        "max_pass_count_lte",
        "max_backdrop_pixels_lte",
        "max_frame_capture_count_lte",
        "max_frame_capture_count_gte",
        "max_frame_capture_pixels_lte",
        "max_frame_capture_pixels_gte",
        "total_surface_sample_pixels_lte",
        "total_surface_sample_pixels_gte",
        "max_surface_sample_pixels_lte",
        "max_surface_sample_pixels_gte",
        "max_container_spacing_lte",
        "max_container_spacing_gte",
        "max_pass_texture_copy_pixels_lte",
        "max_pass_texture_copy_pixels_gte",
        "total_pass_texture_copy_pixels_lte",
        "total_pass_texture_copy_pixels_gte",
        "total_runtime_passes_lte",
        "total_runtime_passes_gte",
        "active_runtime_passes_lte",
        "active_runtime_passes_gte",
        "backdrop_runtime_passes_lte",
        "backdrop_runtime_passes_gte",
        "total_execution_stages_lte",
        "total_execution_stages_gte",
        "active_execution_stages_lte",
        "active_execution_stages_gte",
        "backdrop_execution_stages_lte",
        "backdrop_execution_stages_gte",
        "dropped_execution_stages_lte",
        "dropped_execution_stages_gte",
        "max_execution_stage_count_lte",
        "max_execution_stage_count_gte",
        "max_execution_stage_capacity_lte",
        "max_execution_stage_capacity_gte",
        "max_execution_stages_lte",
        "max_execution_stages_gte",
    }
    bool_fields = {
        "require_bounded_texture_copy",
        "require_deterministic_fallback",
    }
    unknown = sorted(set(value) - number_fields - bool_fields)
    if unknown:
        raise ValueError(
            "unknown require_material_resource_bounds keys: "
            + ", ".join(unknown))
    spec: JsonObject = {}
    for field in number_fields:
        if field in value:
            spec[field] = non_negative_number(
                value[field],
                f"require_material_resource_bounds.{field}")
    for field in bool_fields:
        if field in value:
            item = value[field]
            if not isinstance(item, bool):
                raise ValueError(
                    f"require_material_resource_bounds.{field} must be a boolean")
            spec[field] = item
    return spec


def material_quality_policy_spec_from_manifest(value: Any) -> JsonObject | None:
    if value is None:
        return None
    if not isinstance(value, dict):
        raise ValueError("require_material_quality_policy must be an object")
    number_fields = {
        "max_blur_radius_lte",
        "max_sample_taps_lte",
        "max_backdrop_pixels_lte",
    }
    bool_fields = {
        "require_backdrop_sampling_allowed",
        "require_noise_allowed",
        "require_shadow_allowed",
    }
    unknown = sorted(set(value) - number_fields - bool_fields)
    if unknown:
        raise ValueError(
            "unknown require_material_quality_policy keys: "
            + ", ".join(unknown))
    spec: JsonObject = {}
    for field in number_fields:
        if field in value:
            spec[field] = non_negative_number(
                value[field],
                f"require_material_quality_policy.{field}")
    for field in bool_fields:
        if field in value:
            item = value[field]
            if not isinstance(item, bool):
                raise ValueError(
                    f"require_material_quality_policy.{field} must be a boolean")
            spec[field] = item
    return spec


def apply_manifest(args: argparse.Namespace, report: Report) -> bool:
    if not args.manifest:
        return True

    manifest_path = Path(args.manifest).resolve()
    if not report.check("manifest exists", manifest_path.is_file(), str(manifest_path)):
        return False

    manifest = load_json_file(manifest_path, report)
    if not report.check("manifest root is object", isinstance(manifest, dict), str(manifest_path)):
        return False
    assert isinstance(manifest, dict)

    try:
        if args.expect_platform is None:
            expect_platform = manifest.get("expect_platform")
            if expect_platform is not None:
                if not isinstance(expect_platform, str):
                    raise ValueError("expect_platform must be a string")
                args.expect_platform = expect_platform

        require_frame = bool_manifest_value(manifest, "require_frame")
        if require_frame is True:
            args.require_frame = True

        require_disabled_count = int_manifest_value(manifest, "require_disabled_count")
        if require_disabled_count is not None:
            args.require_disabled_count = max(args.require_disabled_count, require_disabled_count)

        require_material_fallback = bool_manifest_value(manifest, "require_material_fallback")
        if require_material_fallback is True:
            args.require_material_fallback = True

        require_material_plans = bool_manifest_value(manifest, "require_material_plans")
        if require_material_plans is True:
            args.require_material_plan = True
        require_material_semantic_runtime_match = bool_manifest_value(
            manifest,
            "require_material_semantic_runtime_match")
        if require_material_semantic_runtime_match is True:
            args.require_material_plan = True
            args.require_material_semantic_runtime_match = True
        material_plan_summary = material_plan_summary_spec_from_manifest(
            manifest.get("require_material_plan_summary"))
        if material_plan_summary is not None:
            args.require_material_plan = True
            args.require_material_plan_summary = material_plan_summary
        material_resource_bounds = material_resource_bounds_spec_from_manifest(
            manifest.get("require_material_resource_bounds"))
        if material_resource_bounds is not None:
            args.require_material_plan = True
            args.require_material_resource_bounds = material_resource_bounds
        material_quality_policy = material_quality_policy_spec_from_manifest(
            manifest.get("require_material_quality_policy"))
        if material_quality_policy is not None:
            args.require_material_plan = True
            args.require_material_quality_policy = material_quality_policy

        args.require_label.extend(list_of_strings(manifest, "require_labels"))
        args.require_label_contains.extend(
            list_of_strings(manifest, "require_label_contains"))
        args.require_role.extend(list_of_strings(manifest, "require_roles"))
        args.require_material_kind.extend(
            list_of_strings(manifest, "require_material_kinds"))
        args.require_material_surface_role.extend(
            list_of_strings(manifest, "require_material_surface_roles"))
        args.require_capability.extend(
            list_of_strings(manifest, "require_capabilities"))
        runtime_details = manifest.get("require_runtime_details", [])
        if runtime_details is None:
            runtime_details = []
        if not isinstance(runtime_details, list):
            raise ValueError("require_runtime_details must be a list")
        args.require_runtime_detail.extend(
            runtime_detail_spec_from_manifest(entry) for entry in runtime_details)
        debug_details = manifest.get("require_debug_details", [])
        if debug_details is None:
            debug_details = []
        if not isinstance(debug_details, list):
            raise ValueError("require_debug_details must be a list")
        args.require_debug_detail.extend(
            debug_detail_spec_from_manifest(entry) for entry in debug_details)
        args.require_runtime_numeric_bound.extend(
            runtime_numeric_bound_specs_from_manifest(
                manifest.get("require_runtime_numeric_bounds")))

        pixel_regions = manifest.get("pixel_regions", [])
        if pixel_regions is None:
            pixel_regions = []
        if not isinstance(pixel_regions, list):
            raise ValueError("pixel_regions must be a list")
        args.require_pixel_region.extend(
            pixel_region_spec_from_manifest(region) for region in pixel_regions)
        args.require_pixel_region_metric.extend(
            pixel_region_metric_specs_from_manifest(
                manifest.get("pixel_region_metrics")))
        args.require_pixel_region_metric_comparison.extend(
            pixel_region_metric_comparison_specs_from_manifest(
                manifest.get("pixel_region_metric_comparisons")))
        args.forbid_pixel_region_color.extend(
            pixel_region_forbidden_color_specs_from_manifest(
                manifest.get("forbid_pixel_region_colors")))

    except ValueError as exc:
        report.check("manifest schema is valid", False, str(exc))
        return False

    report.data["manifest"] = {
        "path": str(manifest_path),
        "name": manifest.get("name"),
        "pixel_regions": len(manifest.get("pixel_regions", []) or []),
        "pixel_region_metrics": len(manifest.get("pixel_region_metrics", []) or []),
        "pixel_region_metric_comparisons": len(
            manifest.get("pixel_region_metric_comparisons", []) or []),
        "forbid_pixel_region_colors": len(
            manifest.get("forbid_pixel_region_colors", []) or []),
        "runtime_numeric_bounds": len(
            manifest.get("require_runtime_numeric_bounds", []) or []),
    }
    report.check("manifest schema is valid", True, str(manifest_path))
    return True


def parse_bmp(path: Path) -> JsonObject:
    with path.open("rb") as handle:
        header = handle.read(54)
    if len(header) < 54:
        raise ValueError("BMP header is shorter than 54 bytes")
    if header[0:2] != b"BM":
        raise ValueError("BMP signature is not BM")

    file_size = struct.unpack_from("<I", header, 2)[0]
    pixel_offset = struct.unpack_from("<I", header, 10)[0]
    dib_size = struct.unpack_from("<I", header, 14)[0]
    width = struct.unpack_from("<i", header, 18)[0]
    height = struct.unpack_from("<i", header, 22)[0]
    planes = struct.unpack_from("<H", header, 26)[0]
    bits_per_pixel = struct.unpack_from("<H", header, 28)[0]
    compression = struct.unpack_from("<I", header, 30)[0]
    actual_size = path.stat().st_size

    if width <= 0:
        raise ValueError(f"BMP width must be positive, got {width}")
    if height == 0:
        raise ValueError("BMP height must be non-zero")
    if planes != 1:
        raise ValueError(f"BMP planes must be 1, got {planes}")
    if bits_per_pixel not in (24, 32):
        raise ValueError(f"BMP bits-per-pixel must be 24 or 32, got {bits_per_pixel}")
    if compression != 0:
        raise ValueError(f"BMP compression must be BI_RGB(0), got {compression}")
    if file_size != actual_size:
        raise ValueError(
            f"BMP file size header {file_size} does not match actual {actual_size}")

    abs_height = abs(height)
    row_bytes = ((width * bits_per_pixel + 31) // 32) * 4
    minimum_size = pixel_offset + row_bytes * abs_height
    if actual_size < minimum_size:
        raise ValueError(
            f"BMP is truncated: expected at least {minimum_size} bytes, got {actual_size}")

    return {
        "bits_per_pixel": bits_per_pixel,
        "bytes": actual_size,
        "compression": compression,
        "height": abs_height,
        "pixel_offset": pixel_offset,
        "reported_file_size": file_size,
        "top_down": height < 0,
        "width": width,
    }


def parse_pixel_region_spec(spec: str) -> tuple[str, list[float], float, int]:
    parts = spec.split(":")
    if len(parts) < 2:
        raise ValueError(
            "expected NAME:X,Y,W,H[:MIN_LUMA_DELTA[:MIN_UNIQUE_COLORS]]")

    name = parts[0].strip()
    if not name:
        raise ValueError("region name must not be empty")

    coords = [float(item.strip()) for item in parts[1].split(",")]
    if len(coords) != 4:
        raise ValueError("region coordinates must be X,Y,W,H")

    min_delta = float(parts[2]) if len(parts) >= 3 and parts[2] else 16.0
    min_unique = int(parts[3]) if len(parts) >= 4 and parts[3] else 2
    if len(parts) > 4:
        raise ValueError("too many fields in pixel region spec")
    if min_delta < 0.0:
        raise ValueError("MIN_LUMA_DELTA must be non-negative")
    if min_unique < 1:
        raise ValueError("MIN_UNIQUE_COLORS must be at least 1")

    return name, coords, min_delta, min_unique


def resolve_region(coords: list[float], width: int, height: int) -> tuple[int, int, int, int]:
    normalized = all(0.0 <= value <= 1.0 for value in coords)
    x, y, w, h = coords
    if normalized:
        x *= width
        w *= width
        y *= height
        h *= height

    ix = max(0, min(width, int(round(x))))
    iy = max(0, min(height, int(round(y))))
    iw = max(0, int(round(w)))
    ih = max(0, int(round(h)))
    if ix + iw > width:
        iw = width - ix
    if iy + ih > height:
        ih = height - iy
    return ix, iy, iw, ih


def analyze_bmp_region(path: Path, frame: JsonObject, rect: tuple[int, int, int, int]) -> JsonObject:
    x, y, w, h = rect
    width = int(frame["width"])
    height = int(frame["height"])
    bits_per_pixel = int(frame["bits_per_pixel"])
    pixel_offset = int(frame["pixel_offset"])
    top_down = bool(frame["top_down"])
    bytes_per_pixel = bits_per_pixel // 8
    row_bytes = ((width * bits_per_pixel + 31) // 32) * 4

    with path.open("rb") as handle:
        data = handle.read()

    luma_min = 255.0
    luma_max = 0.0
    alpha_min = 255
    alpha_max = 0
    unique_colors: set[tuple[int, int, int, int]] = set()
    sampled = 0
    luma_sum = 0.0
    luma_square_sum = 0.0
    red_sum = 0.0
    green_sum = 0.0
    blue_sum = 0.0
    alpha_sum = 0.0
    edge_sum = 0.0
    edge_count = 0
    previous_row: list[float] | None = None

    for py in range(y, y + h):
        source_y = py if top_down else height - 1 - py
        row_start = pixel_offset + source_y * row_bytes
        row_luma: list[float] = []
        previous_luma: float | None = None
        for px in range(x, x + w):
            offset = row_start + px * bytes_per_pixel
            if offset + bytes_per_pixel > len(data):
                raise ValueError("pixel region reads beyond BMP data")
            b = data[offset]
            g = data[offset + 1]
            r = data[offset + 2]
            a = data[offset + 3] if bytes_per_pixel == 4 else 255
            luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
            luma_min = min(luma_min, luma)
            luma_max = max(luma_max, luma)
            luma_sum += luma
            luma_square_sum += luma * luma
            red_sum += r
            green_sum += g
            blue_sum += b
            alpha_sum += a
            alpha_min = min(alpha_min, a)
            alpha_max = max(alpha_max, a)
            if len(unique_colors) < 4096:
                unique_colors.add((r, g, b, a))
            if previous_luma is not None:
                edge_sum += abs(luma - previous_luma)
                edge_count += 1
            previous_luma = luma
            row_luma.append(luma)
            sampled += 1
        if previous_row is not None:
            for current, previous in zip(row_luma, previous_row):
                edge_sum += abs(current - previous)
                edge_count += 1
        previous_row = row_luma

    luma_mean = luma_sum / sampled if sampled else 0.0
    luma_variance = (
        max(0.0, (luma_square_sum / sampled) - (luma_mean * luma_mean))
        if sampled else 0.0)

    return {
        "alpha_mean": round(alpha_sum / sampled, 3) if sampled else 0.0,
        "alpha_max": alpha_max,
        "alpha_min": alpha_min,
        "blue_mean": round(blue_sum / sampled, 3) if sampled else 0.0,
        "edge_energy": round(edge_sum / edge_count, 3) if edge_count else 0.0,
        "green_mean": round(green_sum / sampled, 3) if sampled else 0.0,
        "height": h,
        "luma_delta": round(luma_max - luma_min, 3),
        "luma_max": round(luma_max, 3),
        "luma_mean": round(luma_mean, 3),
        "luma_min": round(luma_min, 3),
        "luma_stddev": round(luma_variance ** 0.5, 3),
        "red_mean": round(red_sum / sampled, 3) if sampled else 0.0,
        "sampled_pixels": sampled,
        "unique_colors": len(unique_colors),
        "width": w,
        "x": x,
        "y": y,
    }


def count_bmp_region_color(
    path: Path,
    frame: JsonObject,
    rect: tuple[int, int, int, int],
    rgb: list[int],
    tolerance: int,
) -> JsonObject:
    x, y, w, h = rect
    width = int(frame["width"])
    height = int(frame["height"])
    bits_per_pixel = int(frame["bits_per_pixel"])
    pixel_offset = int(frame["pixel_offset"])
    top_down = bool(frame["top_down"])
    bytes_per_pixel = bits_per_pixel // 8
    row_bytes = ((width * bits_per_pixel + 31) // 32) * 4

    with path.open("rb") as handle:
        data = handle.read()

    sampled = 0
    matches = 0
    target_r, target_g, target_b = rgb
    for py in range(y, y + h):
        source_y = py if top_down else height - 1 - py
        row_start = pixel_offset + source_y * row_bytes
        for px in range(x, x + w):
            offset = row_start + px * bytes_per_pixel
            if offset + bytes_per_pixel > len(data):
                raise ValueError("pixel region reads beyond BMP data")
            b = data[offset]
            g = data[offset + 1]
            r = data[offset + 2]
            sampled += 1
            if (
                abs(int(r) - int(target_r)) <= tolerance
                and abs(int(g) - int(target_g)) <= tolerance
                and abs(int(b) - int(target_b)) <= tolerance
            ):
                matches += 1
    return {
        "matching_pixels": matches,
        "sampled_pixels": sampled,
        "ratio": round(matches / sampled, 6) if sampled else 0.0,
        "rgb": rgb,
        "tolerance": tolerance,
    }


def walk_semantic(node: Any, summary: JsonObject) -> None:
    if not isinstance(node, dict):
        summary["invalid_nodes"] += 1
        return

    summary["nodes"] += 1
    role = node.get("role")
    if isinstance(role, str):
        roles = summary["roles"]
        roles[role] = roles.get(role, 0) + 1
    else:
        summary["nodes_without_role"] += 1

    label = node.get("label")
    if isinstance(label, str) and label:
        labels = summary["labels"]
        labels.append(label)

    if node.get("visible") is True:
        summary["visible_nodes"] += 1
        bounds = node.get("bounds")
        if not isinstance(bounds, dict) or bounds.get("valid") is not True:
            summary["visible_nodes_without_valid_bounds"] += 1

    if node.get("focusable") is True:
        summary["focusable_nodes"] += 1
    if node.get("enabled") is False:
        summary["disabled_nodes"] += 1

    material = node.get("material")
    if isinstance(material, dict):
        summary["material_nodes"] += 1
        descriptor = material_descriptor_from(material)
        if descriptor is None:
            summary["material_descriptor_missing"] += 1
        else:
            summary["material_descriptors"].append(descriptor)
        kind = material.get("kind")
        if isinstance(kind, str):
            material_kinds = summary["material_kinds"]
            material_kinds[kind] = material_kinds.get(kind, 0) + 1
        role = material.get("role")
        if isinstance(role, str):
            material_roles = summary["material_roles"]
            material_roles[role] = material_roles.get(role, 0) + 1
        verifier_profile = material.get("verifier_profile")
        if isinstance(verifier_profile, str):
            profiles = summary["material_verifier_profiles"]
            profiles[verifier_profile] = profiles.get(verifier_profile, 0) + 1
        container = material_container_from(material)
        if isinstance(container, dict):
            mode = container["mode"]
            modes = summary["material_container_modes"]
            modes[mode] = modes.get(mode, 0) + 1
            container_id = int(container["container_id"])
            if container_id > 0:
                summary["material_container_participating"] += 1
                ids = summary["material_container_ids"]
                key = str(container_id)
                ids[key] = ids.get(key, 0) + 1
            if int(container["union_id"]) > 0:
                summary["material_container_unioned"] += 1
            if container["interactive"] is True:
                summary["material_container_interactive"] += 1
            if container["morph_transitions"] is True:
                summary["material_container_morph_transitions"] += 1
        if material.get("fallback") is True:
            summary["material_fallback_nodes"] += 1

    children = node.get("children")
    if children is None:
        return
    if not isinstance(children, list):
        summary["invalid_child_lists"] += 1
        return
    for child in children:
        walk_semantic(child, summary)


def summarize_semantic_tree(tree: JsonObject) -> JsonObject:
    summary: JsonObject = {
        "disabled_nodes": 0,
        "focusable_nodes": 0,
        "invalid_child_lists": 0,
        "invalid_nodes": 0,
        "labels": [],
        "material_descriptor_missing": 0,
        "material_descriptors": [],
        "material_container_ids": {},
        "material_container_modes": {},
        "material_container_participating": 0,
        "material_container_unioned": 0,
        "material_container_interactive": 0,
        "material_container_morph_transitions": 0,
        "material_fallback_nodes": 0,
        "material_kinds": {},
        "material_nodes": 0,
        "material_roles": {},
        "material_verifier_profiles": {},
        "nodes": 0,
        "nodes_without_role": 0,
        "roles": {},
        "visible_nodes": 0,
        "visible_nodes_without_valid_bounds": 0,
    }
    walk_semantic(tree, summary)
    labels = summary["labels"]
    summary["labels_sample"] = labels[:40]
    summary["label_count"] = len(labels)
    del summary["labels"]
    return summary


REQUIRED_MATERIAL_PLAN_FIELDS = (
    "contract_version",
    "kind",
    "role",
    "plan_id",
    "container",
    "reference_model",
    "command_descriptor",
    "geometry",
    "shape",
    "render_target",
    "decision_trace",
    "opacity",
    "blur_radius",
    "tint",
    "saturation",
    "luminance_floor",
    "luminance_gain",
    "luminance_curve",
    "edge_highlight",
    "edge_width",
    "noise_opacity",
    "shadow_alpha",
    "shadow_radius",
    "backdrop_sampling",
    "backdrop",
    "backdrop_access",
    "theme",
    "foreground",
    "fallback",
    "fallback_path",
    "fallback_reason",
    "debug_seed",
    "sample_taps",
    "sampling_kernel",
    "quality_policy",
    "primary_pass",
    "resource_budget",
    "execution_stage_capacity",
    "dropped_execution_stage_count",
    "verifier",
    "observation_contract",
    "passes",
    "execution_stages",
)

MATERIAL_PLAN_NUMERIC_FIELDS = (
    "opacity",
    "blur_radius",
    "saturation",
    "luminance_floor",
    "luminance_gain",
    "edge_highlight",
    "edge_width",
    "noise_opacity",
    "shadow_alpha",
    "shadow_radius",
)

MATERIAL_COMMAND_DESCRIPTOR_NUMERIC_FIELDS = (
    "opacity",
    "blur_radius",
    "saturation",
    "luminance_floor",
    "luminance_gain",
    "edge_highlight",
    "edge_width",
    "noise_opacity",
    "shadow_alpha",
    "shadow_radius",
)

MATERIAL_GEOMETRY_FIELDS = ("x", "y", "w", "h", "radius")
MATERIAL_SHAPE_BOOL_FIELDS = ("valid", "rounded", "capsule", "radius_clamped")
MATERIAL_SHAPE_NUMERIC_FIELDS = (
    "surface_area",
    "min_extent",
    "max_extent",
    "radius_limit",
    "effective_radius",
    "normalized_radius",
)
MATERIAL_RENDER_TARGET_INT_FIELDS = ("width", "height", "pixel_count")
MATERIAL_RENDER_TARGET_BOOL_FIELDS = ("ready", "within_backdrop_budget")
MATERIAL_DECISION_TRACE_BOOL_FIELDS = (
    "has_geometry",
    "has_material",
    "role_allows_liquid_glass",
    "content_layer_standard_material",
    "liquid_glass_backdrop_candidate",
    "target_ready",
    "quality_switches_allow_backdrop",
    "backdrop_pixels_within_budget",
    "quality_allows_backdrop",
    "capability_material_surfaces",
    "capability_material_backdrop_blur",
    "capability_shader_blur",
    "capability_frame_history",
    "backend_supports_backdrop",
    "backdrop_available",
    "backdrop_stable",
    "backdrop_source_ready",
    "next_frame_capture_required",
    "reduced_transparency",
    "increase_contrast",
    "reduce_motion",
    "can_sample_backdrop",
)
MATERIAL_TINT_FIELDS = ("r", "g", "b", "a")
MATERIAL_COLOR_FIELDS = ("r", "g", "b", "a")
MATERIAL_THEME_COLOR_FIELDS = (
    "foreground",
    "secondary_foreground",
    "accent_foreground",
    "strong_accent_foreground",
    "tint",
    "border",
)
MATERIAL_THEME_STRING_FIELDS = ("source", "profile_name", "token_policy")
MATERIAL_THEME_BOOL_FIELDS = (
    "foreground_matches_theme",
    "accent_matches_theme",
    "tint_matches_surface",
    "border_matches_theme",
    "default_glass_tokens",
)
MATERIAL_BACKDROP_BOOL_FIELDS = (
    "available",
    "stable",
    "excludes_foreground_text",
)
MATERIAL_BACKDROP_LUMA_FIELDS = (
    "luma_min",
    "luma_max",
    "luma_mean",
    "luma_span",
)
MATERIAL_BACKDROP_DELTA_FIELDS = (
    "luminance_floor_delta",
    "luminance_gain_delta",
    "edge_highlight_delta",
)
MATERIAL_VERIFIER_FIELDS = (
    "require_backdrop_source",
    "require_edge_highlight",
    "require_container_identity",
    "require_container_morph_contract",
    "min_luma_delta",
    "min_unique_colors",
    "region_name",
    "likely_layer",
    "likely_pass",
)
MATERIAL_PASS_FIELDS = (
    "name",
    "active",
    "requires_backdrop",
    "sample_taps",
    "likely_layer",
    "executor",
    "max_texture_copy_pixels",
)
MATERIAL_EXECUTION_STAGE_FIELDS = MATERIAL_PASS_FIELDS + ("bounded",)
MATERIAL_OBSERVATION_BOOL_FIELDS = (
    "semantic_material_required",
    "runtime_plan_required",
    "fallback_expected",
    "backdrop_sampling_expected",
    "stable_backdrop_required",
    "shared_frame_capture_required",
    "next_frame_capture_required",
    "backdrop_capture_excludes_foreground_text",
    "bounded_texture_copy_required",
    "deterministic_fallback_required",
)
MATERIAL_OBSERVATION_INT_FIELDS = (
    "schema_version",
    "expected_runtime_passes",
    "expected_execution_stages",
    "expected_active_execution_stages",
    "expected_backdrop_execution_stages",
    "max_frame_capture_count",
    "max_frame_capture_pixels",
    "max_surface_sample_pixels",
    "max_texture_copy_pixels",
)
MATERIAL_OBSERVATION_STRING_FIELDS = (
    "backdrop_capture_scope",
    "backdrop_capture_reason",
    "fallback_path",
    "fallback_reason",
    "primary_pass",
    "primary_executor",
    "region_name",
    "likely_layer",
    "likely_pass",
)
MATERIAL_SAMPLING_KERNEL_FIELDS = (
    "name",
    "radius",
    "sample_taps",
    "blur_step_scale",
    "weight_profile",
    "requires_backdrop",
    "bounded",
)
MATERIAL_LUMINANCE_CURVE_FIELDS = (
    "name",
    "floor",
    "gain",
    "gamma",
    "midpoint",
    "contrast",
    "edge_lift",
    "backdrop_driven",
    "bounded",
)
MATERIAL_FOREGROUND_COLOR_FIELDS = (
    "primary",
    "secondary",
    "accent",
)
MATERIAL_FOREGROUND_NUMERIC_FIELDS = (
    "background_luma",
    "primary_contrast_ratio",
    "secondary_contrast_ratio",
    "accent_contrast_ratio",
    "minimum_contrast_ratio",
)
MATERIAL_FOREGROUND_BOOL_FIELDS = (
    "backdrop_driven",
    "high_contrast",
    "uses_vibrancy",
    "deterministic",
)
MATERIAL_QUALITY_POLICY_BOOL_FIELDS = (
    "allow_backdrop_sampling",
    "allow_noise",
    "allow_shadow",
)
MATERIAL_QUALITY_POLICY_NUMBER_FIELDS = (
    "max_blur_radius",
    "max_sample_taps",
    "max_backdrop_pixels",
)
MATERIAL_BACKDROP_ACCESS_BOOL_FIELDS = (
    "required",
    "stable_required",
    "frame_history_required",
    "shared_frame_capture",
    "next_frame_capture_required",
    "excludes_foreground_text",
    "bounded",
)
MATERIAL_BACKDROP_ACCESS_NUMBER_FIELDS = (
    "max_frame_capture_count",
    "max_frame_capture_pixels",
    "max_surface_sample_pixels",
)
MATERIAL_BACKDROP_ACCESS_STRING_FIELDS = (
    "source",
    "capture_scope",
    "capture_reason",
)


def check_material_observation_contract(
    report: Report,
    plan: JsonObject,
    plan_path: str,
    likely_layer: str,
) -> None:
    observation = check_object_field(
        report,
        plan,
        "observation_contract",
        plan_path,
        likely_layer="material-observation",
        hint=(
            "MaterialPlan.observation_contract should mirror the pure plan "
            "facts the artifact verifier must observe."))
    if observation is None:
        return

    observation_path = f"{plan_path}.observation_contract"
    likely_pass = string_at(observation, "likely_pass") or ""
    for key in MATERIAL_OBSERVATION_BOOL_FIELDS:
        check_bool_field(
            report,
            observation,
            key,
            observation_path,
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation booleans must be explicit pure-plan facts.")
    for key in MATERIAL_OBSERVATION_INT_FIELDS:
        check_number_field(
            report,
            observation,
            key,
            observation_path,
            min_value=0.0,
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation numeric bounds must be non-negative.")
    for key in MATERIAL_OBSERVATION_STRING_FIELDS:
        check_string_field(
            report,
            observation,
            key,
            observation_path,
            allow_empty=key == "fallback_reason",
            likely_layer="material-observation",
            hint="Observation strings should identify the debuggable plan path.")

    contract_version = plan.get("contract_version")
    schema_version = observation.get("schema_version")
    report.check(
        "material observation schema version matches plan",
        schema_version == contract_version,
        path=f"{observation_path}.schema_version",
        expected=contract_version,
        actual=schema_version,
        likely_layer="material-observation",
        likely_pass=likely_pass,
        hint="Keep MaterialPlan.contract_version and observation_contract.schema_version synchronized.",
        record_success=False)
    report.check(
        "material observation schema version is supported",
        schema_version == MATERIAL_PLAN_CONTRACT_VERSION,
        path=f"{observation_path}.schema_version",
        expected=MATERIAL_PLAN_CONTRACT_VERSION,
        actual=schema_version,
        likely_layer="material-observation",
        likely_pass=likely_pass,
        hint="Update the observation contract verifier when the material plan schema changes.",
        record_success=False)

    kind = string_at(plan, "kind")
    if kind is not None:
        expects_material = kind != "none"
        for key in ("semantic_material_required", "runtime_plan_required"):
            report.check(
                f"material observation {key} follows material kind",
                observation.get(key) == expects_material,
                path=f"{observation_path}.{key}",
                expected=expects_material,
                actual=observation.get(key),
                likely_layer="material-observation",
                likely_pass=likely_pass,
                hint="Observation material requirements should be derived from MaterialPlan.kind.",
                record_success=False)

    fallback = bool_at(plan, "fallback")
    backdrop_sampling = bool_at(plan, "backdrop_sampling")
    if fallback is not None:
        report.check(
            "material observation fallback flag matches plan",
            observation.get("fallback_expected") == fallback,
            path=f"{observation_path}.fallback_expected",
            expected=fallback,
            actual=observation.get("fallback_expected"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation fallback must mirror MaterialPlan.fallback.",
            record_success=False)
    if backdrop_sampling is not None:
        report.check(
            "material observation backdrop flag matches plan",
            observation.get("backdrop_sampling_expected") == backdrop_sampling,
            path=f"{observation_path}.backdrop_sampling_expected",
            expected=backdrop_sampling,
            actual=observation.get("backdrop_sampling_expected"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation backdrop sampling must mirror MaterialPlan.backdrop_sampling.",
            record_success=False)
        report.check(
            "material observation stable backdrop requirement matches sampling",
            observation.get("stable_backdrop_required") == backdrop_sampling,
            path=f"{observation_path}.stable_backdrop_required",
            expected=backdrop_sampling,
            actual=observation.get("stable_backdrop_required"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Only sampled backdrop plans should require a stable backdrop source.",
            record_success=False)

    backdrop_access = plan.get("backdrop_access")
    if isinstance(backdrop_access, dict):
        report.check(
            "material observation shared capture requirement matches access",
            observation.get("shared_frame_capture_required")
            == backdrop_access.get("shared_frame_capture"),
            path=f"{observation_path}.shared_frame_capture_required",
            expected=backdrop_access.get("shared_frame_capture"),
            actual=observation.get("shared_frame_capture_required"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation shared capture must mirror MaterialPlan.backdrop_access.",
            record_success=False)
        report.check(
            "material observation next-frame capture requirement matches access",
            observation.get("next_frame_capture_required")
            == backdrop_access.get("next_frame_capture_required"),
            path=f"{observation_path}.next_frame_capture_required",
            expected=backdrop_access.get("next_frame_capture_required"),
            actual=observation.get("next_frame_capture_required"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation warmup capture must mirror MaterialPlan.backdrop_access.",
            record_success=False)
        report.check(
            "material observation foreground-exclusion requirement matches access",
            observation.get("backdrop_capture_excludes_foreground_text")
            == backdrop_access.get("excludes_foreground_text"),
            path=(
                f"{observation_path}."
                "backdrop_capture_excludes_foreground_text"),
            expected=backdrop_access.get("excludes_foreground_text"),
            actual=observation.get(
                "backdrop_capture_excludes_foreground_text"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint=(
                "Observation foreground-exclusion must mirror "
                "MaterialPlan.backdrop_access."),
            record_success=False)
        report.check(
            "material observation capture scope matches access",
            observation.get("backdrop_capture_scope")
            == backdrop_access.get("capture_scope"),
            path=f"{observation_path}.backdrop_capture_scope",
            expected=backdrop_access.get("capture_scope"),
            actual=observation.get("backdrop_capture_scope"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation capture scope must mirror MaterialPlan.backdrop_access.",
            record_success=False)
        report.check(
            "material observation capture reason matches access",
            observation.get("backdrop_capture_reason")
            == backdrop_access.get("capture_reason"),
            path=f"{observation_path}.backdrop_capture_reason",
            expected=backdrop_access.get("capture_reason"),
            actual=observation.get("backdrop_capture_reason"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation capture reason must mirror MaterialPlan.backdrop_access.",
            record_success=False)
        for key in (
                "max_frame_capture_count",
                "max_frame_capture_pixels",
                "max_surface_sample_pixels"):
            report.check(
                f"material observation {key} matches access",
                observation.get(key) == backdrop_access.get(key),
                path=f"{observation_path}.{key}",
                expected=backdrop_access.get(key),
                actual=observation.get(key),
                likely_layer="material-observation",
                likely_pass=likely_pass,
                hint="Observation backdrop access bounds should mirror MaterialPlan.backdrop_access.",
                record_success=False)

    for key in ("fallback_path", "fallback_reason"):
        report.check(
            f"material observation {key} matches plan",
            observation.get(key) == plan.get(key),
            path=f"{observation_path}.{key}",
            expected=plan.get(key),
            actual=observation.get(key),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint=f"Observation {key} should be copied from MaterialPlan.{key}.",
            record_success=False)

    primary_pass = plan.get("primary_pass")
    if isinstance(primary_pass, dict):
        report.check(
            "material observation primary pass matches plan",
            observation.get("primary_pass") == primary_pass.get("name"),
            path=f"{observation_path}.primary_pass",
            expected=primary_pass.get("name"),
            actual=observation.get("primary_pass"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation primary_pass should mirror MaterialPlan.primary_pass.name.",
            record_success=False)
        report.check(
            "material observation primary executor matches plan",
            observation.get("primary_executor") == primary_pass.get("executor"),
            path=f"{observation_path}.primary_executor",
            expected=primary_pass.get("executor"),
            actual=observation.get("primary_executor"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation primary_executor should mirror MaterialPlan.primary_pass.executor.",
            record_success=False)
        report.check(
            "material observation texture-copy bound matches primary pass",
            observation.get("max_texture_copy_pixels")
            == primary_pass.get("max_texture_copy_pixels"),
            path=f"{observation_path}.max_texture_copy_pixels",
            expected=primary_pass.get("max_texture_copy_pixels"),
            actual=observation.get("max_texture_copy_pixels"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation texture-copy bound should mirror MaterialPlan.primary_pass.",
            record_success=False)

    passes = plan.get("passes")
    if isinstance(passes, list):
        report.check(
            "material observation runtime pass count matches plan",
            observation.get("expected_runtime_passes") == len(passes),
            path=f"{observation_path}.expected_runtime_passes",
            expected=len(passes),
            actual=observation.get("expected_runtime_passes"),
            likely_layer="material-observation",
            likely_pass=likely_pass,
            hint="Observation pass count should mirror renderer.material_plans[].passes.",
            record_success=False)

    execution_stages = plan.get("execution_stages")
    if isinstance(execution_stages, list):
        active_stages = sum(
            1 for stage in execution_stages
            if isinstance(stage, dict) and stage.get("active") is True)
        backdrop_stages = sum(
            1 for stage in execution_stages
            if isinstance(stage, dict)
            and stage.get("requires_backdrop") is True)
        for key, expected in (
                ("expected_execution_stages", len(execution_stages)),
                ("expected_active_execution_stages", active_stages),
                ("expected_backdrop_execution_stages", backdrop_stages)):
            report.check(
                f"material observation {key} matches stages",
                observation.get(key) == expected,
                path=f"{observation_path}.{key}",
                expected=expected,
                actual=observation.get(key),
                likely_layer="material-observation",
                likely_pass=likely_pass,
                hint="Observation stage counts should mirror MaterialPlan.execution_stages.",
                record_success=False)

    resource_budget = plan.get("resource_budget")
    if isinstance(resource_budget, dict):
        for observation_key, budget_key in (
                ("bounded_texture_copy_required", "bounded_texture_copy"),
                ("deterministic_fallback_required", "deterministic_fallback")):
            report.check(
                f"material observation {observation_key} matches resource budget",
                observation.get(observation_key) == resource_budget.get(budget_key),
                path=f"{observation_path}.{observation_key}",
                expected=resource_budget.get(budget_key),
                actual=observation.get(observation_key),
                likely_layer="material-observation",
                likely_pass=likely_pass,
                hint="Observation safety flags should mirror MaterialPlan.resource_budget.",
                record_success=False)
        for observation_key, budget_key in (
                ("max_frame_capture_count", "max_frame_capture_count"),
                ("max_frame_capture_pixels", "max_frame_capture_pixels"),
                ("max_surface_sample_pixels", "max_surface_sample_pixels")):
            report.check(
                f"material observation {observation_key} matches resource budget",
                observation.get(observation_key) == resource_budget.get(budget_key),
                path=f"{observation_path}.{observation_key}",
                expected=resource_budget.get(budget_key),
                actual=observation.get(observation_key),
                likely_layer="material-observation",
                likely_pass=likely_pass,
                hint="Observation access bounds should mirror MaterialPlan.resource_budget.",
                record_success=False)

    verifier = plan.get("verifier")
    if isinstance(verifier, dict):
        for observation_key, verifier_key in (
                ("region_name", "region_name"),
                ("likely_layer", "likely_layer"),
                ("likely_pass", "likely_pass")):
            report.check(
                f"material observation {observation_key} matches verifier",
                observation.get(observation_key) == verifier.get(verifier_key),
                path=f"{observation_path}.{observation_key}",
                expected=verifier.get(verifier_key),
                actual=observation.get(observation_key),
                likely_layer="material-observation",
                likely_pass=likely_pass,
                hint="Observation region hints should mirror MaterialPlan.verifier.",
                record_success=False)


def expected_reference_accessibility_response(plan: JsonObject) -> str | None:
    decision_trace = plan.get("decision_trace")
    if not isinstance(decision_trace, dict):
        return None
    flags = {
        "reduced_transparency": bool_at(decision_trace, "reduced_transparency"),
        "increase_contrast": bool_at(decision_trace, "increase_contrast"),
        "reduce_motion": bool_at(decision_trace, "reduce_motion"),
    }
    if any(value is None for value in flags.values()):
        return None
    active = [name for name, value in flags.items() if value]
    if len(active) > 1:
        return "combined-accessibility"
    if not active:
        return "standard"
    return {
        "reduced_transparency": "reduced-transparency",
        "increase_contrast": "increased-contrast",
        "reduce_motion": "reduced-motion",
    }[active[0]]


def material_plan_uses_budgeted_effects(plan: JsonObject) -> bool:
    if bool_at(plan, "backdrop_sampling") is not True:
        return False
    resource_budget = plan.get("resource_budget")
    quality_policy = plan.get("quality_policy")
    if isinstance(resource_budget, dict):
        max_blur = number_at(resource_budget, "max_blur_radius")
        max_taps = number_at(resource_budget, "max_sample_taps")
        if max_blur is not None and float(max_blur) < 36.0:
            return True
        if max_taps is not None and int(max_taps) < 25:
            return True
    if isinstance(quality_policy, dict):
        if bool_at(quality_policy, "allow_noise") is False:
            return True
        if bool_at(quality_policy, "allow_shadow") is False:
            return True
    return False


def expected_reference_performance_response(
        plan: JsonObject,
        kind: str | None) -> str | None:
    if kind == "none":
        return "inactive"
    fallback = bool_at(plan, "fallback")
    backdrop_sampling = bool_at(plan, "backdrop_sampling")
    fallback_path = string_at(plan, "fallback_path")
    backdrop_access = plan.get("backdrop_access")
    if fallback is None or backdrop_sampling is None or fallback_path is None:
        return None
    if isinstance(backdrop_access, dict):
        next_frame_capture = bool_at(backdrop_access, "next_frame_capture_required")
        if (next_frame_capture is True
                and not backdrop_sampling
                and fallback_path == "no-backdrop-source"):
            return "warmup-capture"
    if fallback:
        return "deterministic-fallback"
    if material_plan_uses_budgeted_effects(plan):
        return "budgeted-effects"
    return "standard"


def summarize_material_plans(plans: Any, report: Report, path: str) -> JsonObject:
    summary: JsonObject = {
        "count": 0,
        "fallback": 0,
        "backdrop_sampling": 0,
        "fallback_paths": {},
        "fallback_reasons": {},
        "kinds": {},
        "roles": {},
        "container": {
            "modes": {},
            "container_ids": {},
            "union_ids": {},
            "participating": 0,
            "unioned": 0,
            "interactive": 0,
            "morph_transitions": 0,
            "max_spacing": 0.0,
        },
        "reference_model": {
            "technologies": {},
            "layers": {},
            "material_policies": {},
            "variants": {},
            "shapes": {},
            "shape_scopes": {},
            "blending_scopes": {},
            "semantic_thickness": {},
            "accessibility_responses": {},
            "performance_responses": {},
            "view_bounds_anchored": 0,
            "shape_matches_geometry": 0,
            "tint_applied": 0,
            "interactive_response": 0,
            "container_grouped": 0,
            "container_union": 0,
            "container_morphing": 0,
            "legibility_preserved": 0,
            "vibrancy_expected": 0,
            "deterministic_degradation": 0,
        },
        "shape": {
            "valid": 0,
            "rounded": 0,
            "capsule": 0,
            "radius_clamped": 0,
            "max_surface_area": 0.0,
            "max_effective_radius": 0.0,
            "max_radius_limit": 0.0,
            "max_normalized_radius": 0.0,
        },
        "pass_names": {},
        "pass_executors": {},
        "stage_names": {},
        "stage_executors": {},
        "sampling_kernels": {},
        "sampling_weight_profiles": {},
        "luminance_curves": {},
        "luminance_curve_backdrop_driven": 0,
        "plan_ids": [],
        "command_descriptor_missing": 0,
        "command_descriptors": [],
        "contract_versions": {},
        "region_layers": {},
        "region_passes": {},
        "verifier_profiles": {},
        "verifier_require_backdrop_source": 0,
        "verifier_require_edge_highlight": 0,
        "verifier_require_container_identity": 0,
        "verifier_require_container_morph_contract": 0,
        "render_target": {
            "ready": 0,
            "within_backdrop_budget": 0,
            "max_pixel_count": 0,
            "pixel_formats": {},
        },
        "decision_trace": {
            "role_allows_liquid_glass": 0,
            "content_layer_standard_material": 0,
            "liquid_glass_backdrop_candidate": 0,
            "can_sample_backdrop": 0,
            "backend_supports_backdrop": 0,
            "backdrop_source_ready": 0,
            "next_frame_capture_required": 0,
            "reduced_transparency": 0,
            "increase_contrast": 0,
            "reduce_motion": 0,
            "first_blockers": {},
        },
        "backdrop": {
            "available": 0,
            "stable": 0,
            "excludes_foreground_text": 0,
            "sources": {},
            "luminance_responses": {},
            "luminance_adapted": 0,
            "max_luma_span": 0.0,
            "max_abs_floor_delta": 0.0,
            "max_abs_gain_delta": 0.0,
            "max_abs_edge_delta": 0.0,
        },
        "backdrop_access": {
            "required": 0,
            "stable_required": 0,
            "frame_history_required": 0,
            "shared_frame_capture": 0,
            "next_frame_capture_required": 0,
            "excludes_foreground_text": 0,
            "bounded": 0,
            "sources": {},
            "capture_scopes": {},
            "capture_reasons": {},
            "max_frame_capture_count": 0,
            "max_frame_capture_pixels": 0,
            "total_surface_sample_pixels": 0,
            "max_surface_sample_pixels": 0,
        },
        "theme": {
            "sources": {},
            "profile_names": {},
            "token_policies": {},
            "foreground_matches_theme": 0,
            "accent_matches_theme": 0,
            "tint_matches_surface": 0,
            "border_matches_theme": 0,
            "default_glass_tokens": 0,
        },
        "foreground": {
            "schemes": {},
            "sources": {},
            "backdrop_driven": 0,
            "high_contrast": 0,
            "vibrant": 0,
            "deterministic": 0,
            "min_primary_contrast": 0.0,
            "min_secondary_contrast": 0.0,
            "min_accent_contrast": 0.0,
            "min_minimum_contrast": 0.0,
            "max_background_luma": 0.0,
        },
        "resource_bounds": {
            "max_plan_blur_radius": 0.0,
            "max_plan_sample_taps": 0,
            "total_plan_sample_taps": 0,
            "max_budget_blur_radius": 0.0,
            "max_sample_taps": 0,
            "max_sampling_kernel_radius": 0,
            "max_pass_count": 0,
            "max_execution_stages": 0,
            "max_backdrop_pixels": 0,
            "max_frame_capture_count": 0,
            "max_frame_capture_pixels": 0,
            "total_surface_sample_pixels": 0,
            "max_surface_sample_pixels": 0,
            "max_container_spacing": 0.0,
            "total_runtime_passes": 0,
            "active_runtime_passes": 0,
            "backdrop_runtime_passes": 0,
            "total_execution_stages": 0,
            "active_execution_stages": 0,
            "backdrop_execution_stages": 0,
            "dropped_execution_stages": 0,
            "max_execution_stage_count": 0,
            "max_execution_stage_capacity": 0,
            "max_pass_texture_copy_pixels": 0,
            "total_pass_texture_copy_pixels": 0,
            "unbounded_texture_copy": 0,
            "non_deterministic_fallback": 0,
        },
        "quality_policy": {
            "backdrop_sampling_disabled": 0,
            "noise_disabled": 0,
            "shadow_disabled": 0,
            "max_blur_radius": 0.0,
            "max_sample_taps": 0,
            "max_backdrop_pixels": 0,
        },
    }
    if not isinstance(plans, list):
        report.check(
            "material plans is array",
            False,
            repr(type(plans).__name__),
            path=path,
            expected="array",
            actual=type(plans).__name__,
            likely_layer="platform-runtime",
            hint="Check renderer.material_plans serialization.")
        return summary

    summary["count"] = len(plans)
    for index, plan in enumerate(plans):
        plan_path = f"{path}[{index}]"
        if not isinstance(plan, dict):
            report.check(
                "material plan is object",
                False,
                repr(plan),
                path=plan_path,
                expected="object",
                actual=type(plan).__name__,
                likely_layer="platform-runtime",
                hint="Check the backend MaterialPlan JSON writer.")
            continue
        for key in REQUIRED_MATERIAL_PLAN_FIELDS:
            report.check(
                f"material plan contains {key}",
                key in plan,
                path=f"{plan_path}.{key}",
                expected="present",
                actual="missing" if key not in plan else "present",
                likely_layer=string_at(plan, "plan_id") or "material-plan",
                hint="Regenerate the resolved MaterialPlan before writing runtime JSON.",
                record_success=False)

        plan_id = plan.get("plan_id")
        if isinstance(plan_id, str):
            summary["plan_ids"].append(plan_id)
        likely_layer = plan_id if isinstance(plan_id, str) and plan_id else "material-plan"
        if "contract_version" in plan:
            contract_version = plan.get("contract_version")
            contract_version_is_int = (
                isinstance(contract_version, int)
                and not isinstance(contract_version, bool))
            report.check(
                "material plan contract version is integer",
                contract_version_is_int,
                path=f"{plan_path}.contract_version",
                expected="integer",
                actual=contract_version,
                likely_layer=likely_layer,
                hint=(
                    "MaterialPlan.contract_version must be serialized as an "
                    "integer before any schema-specific fields are trusted."),
                record_success=False)
            if contract_version_is_int:
                versions = summary["contract_versions"]
                key = str(contract_version)
                versions[key] = versions.get(key, 0) + 1
                report.check(
                    "material plan contract version is supported",
                    contract_version == MATERIAL_PLAN_CONTRACT_VERSION,
                    path=f"{plan_path}.contract_version",
                    expected=MATERIAL_PLAN_CONTRACT_VERSION,
                    actual=contract_version,
                    likely_layer=likely_layer,
                    hint=(
                        "Update verify_artifact_bundle.py and the material "
                        "artifact docs before emitting a new MaterialPlan "
                        "schema version."),
                    record_success=False)

        kind = plan.get("kind")
        if isinstance(kind, str):
            kinds = summary["kinds"]
            kinds[kind] = kinds.get(kind, 0) + 1
            report.check(
                "material kind is known",
                kind in ALLOWED_MATERIAL_KINDS,
                path=f"{plan_path}.kind",
                expected=sorted(ALLOWED_MATERIAL_KINDS),
                actual=kind,
                likely_layer=kind,
                hint="Update the pure MaterialKind serializer and verifier vocabulary together.",
                record_success=False)

        role = plan.get("role")
        if isinstance(role, str):
            roles = summary["roles"]
            roles[role] = roles.get(role, 0) + 1
            report.check(
                "material surface role is known",
                role in ALLOWED_MATERIAL_SURFACE_ROLES,
                path=f"{plan_path}.role",
                expected=sorted(ALLOWED_MATERIAL_SURFACE_ROLES),
                actual=role,
                likely_layer="material-contract",
                hint=(
                    "Update MaterialSurfaceRole serialization and verifier "
                    "vocabulary together."),
                record_success=False)

        plan_container = check_object_field(
            report,
            plan,
            "container",
            plan_path,
            likely_layer="material-container",
            hint=(
                "MaterialPlan.container should expose the pure glass "
                "container/union identity for this surface."))
        if plan_container is not None:
            container_summary = summary["container"]
            mode = check_string_field(
                report,
                plan_container,
                "mode",
                f"{plan_path}.container",
                likely_layer="material-container",
                hint="Material container mode should be isolated, container, or union.")
            if isinstance(mode, str):
                modes = container_summary["modes"]
                modes[mode] = modes.get(mode, 0) + 1
                report.check(
                    "material container mode is known",
                    mode in ALLOWED_MATERIAL_CONTAINER_MODES,
                    path=f"{plan_path}.container.mode",
                    expected=sorted(ALLOWED_MATERIAL_CONTAINER_MODES),
                    actual=mode,
                    likely_layer="material-container",
                    hint="Keep MaterialContainerMode serialization and verifier vocabulary aligned.",
                    record_success=False)
            container_id = check_number_field(
                report,
                plan_container,
                "container_id",
                f"{plan_path}.container",
                min_value=0.0,
                likely_layer="material-container",
                hint="Material container ids must be stable non-negative integers.")
            union_id = check_number_field(
                report,
                plan_container,
                "union_id",
                f"{plan_path}.container",
                min_value=0.0,
                likely_layer="material-container",
                hint="Material union ids must be stable non-negative integers.")
            spacing = check_number_field(
                report,
                plan_container,
                "spacing",
                f"{plan_path}.container",
                min_value=0.0,
                likely_layer="material-container",
                hint="Material container spacing should be an explicit non-negative value.")
            if isinstance(spacing, (int, float)):
                container_summary["max_spacing"] = max(
                    float(container_summary["max_spacing"]),
                    float(spacing))
                bounds = summary["resource_bounds"]
                bounds["max_container_spacing"] = max(
                    float(bounds["max_container_spacing"]),
                    float(spacing))
            for key in (
                    "interactive",
                    "morph_transitions",
                    "participates",
                    "shared_backdrop_scope",
                    "shape_union_expected"):
                value = check_bool_field(
                    report,
                    plan_container,
                    key,
                    f"{plan_path}.container",
                    likely_layer="material-container",
                    hint="Material container booleans must stay explicit.")
                if value is True:
                    if key == "participates":
                        container_summary["participating"] += 1
                    elif key == "shape_union_expected":
                        container_summary["unioned"] += 1
                    elif key == "interactive":
                        container_summary["interactive"] += 1
                    elif key == "morph_transitions":
                        container_summary["morph_transitions"] += 1
            if isinstance(container_id, (int, float)):
                cid = int(container_id)
                if cid > 0:
                    ids = container_summary["container_ids"]
                    key = str(cid)
                    ids[key] = ids.get(key, 0) + 1
            if isinstance(union_id, (int, float)):
                uid = int(union_id)
                if uid > 0:
                    ids = container_summary["union_ids"]
                    key = str(uid)
                    ids[key] = ids.get(key, 0) + 1
            expected_mode = "isolated"
            if isinstance(container_id, (int, float)) and int(container_id) > 0:
                expected_mode = "container"
                if isinstance(union_id, (int, float)) and int(union_id) > 0:
                    expected_mode = "union"
            if isinstance(mode, str):
                report.check(
                    "material container mode matches ids",
                    mode == expected_mode,
                    path=f"{plan_path}.container.mode",
                    expected=expected_mode,
                    actual=mode,
                    likely_layer="material-container",
                    hint="MaterialContainerDescriptor.mode should be derived from container_id and union_id.",
                    record_success=False)

        reference_model = check_object_field(
            report,
            plan,
            "reference_model",
            plan_path,
            likely_layer="material-reference",
            hint=(
                "MaterialPlan.reference_model should expose Apple Liquid Glass "
                "alignment as pure planner output."))
        if reference_model is not None:
            reference_summary = summary["reference_model"]
            string_specs = {
                "technology": (
                    "technologies",
                    ALLOWED_MATERIAL_REFERENCE_TECHNOLOGIES),
                "layer": (
                    "layers",
                    ALLOWED_MATERIAL_REFERENCE_LAYERS),
                "material_policy": (
                    "material_policies",
                    ALLOWED_MATERIAL_REFERENCE_POLICIES),
                "variant": ("variants", ALLOWED_MATERIAL_KINDS),
                "shape": ("shapes", ALLOWED_MATERIAL_REFERENCE_SHAPES),
                "shape_scope": (
                    "shape_scopes",
                    ALLOWED_MATERIAL_REFERENCE_SHAPE_SCOPES),
                "blending_scope": (
                    "blending_scopes",
                    ALLOWED_MATERIAL_REFERENCE_BLENDING_SCOPES),
                "semantic_thickness": (
                    "semantic_thickness",
                    ALLOWED_MATERIAL_KINDS),
                "accessibility_response": (
                    "accessibility_responses",
                    ALLOWED_MATERIAL_REFERENCE_ACCESSIBILITY_RESPONSES),
                "performance_response": (
                    "performance_responses",
                    ALLOWED_MATERIAL_REFERENCE_PERFORMANCE_RESPONSES),
            }
            reference_values: JsonObject = {}
            for key, (summary_key, allowed) in string_specs.items():
                value = check_string_field(
                    report,
                    reference_model,
                    key,
                    f"{plan_path}.reference_model",
                    likely_layer="material-reference",
                    hint="Keep MaterialPlan.reference_model vocabulary aligned with the verifier.")
                if isinstance(value, str):
                    reference_values[key] = value
                    bucket = reference_summary[summary_key]
                    bucket[value] = bucket.get(value, 0) + 1
                    report.check(
                        f"material reference {key} is known",
                        value in allowed,
                        path=f"{plan_path}.reference_model.{key}",
                        expected=sorted(allowed),
                        actual=value,
                        likely_layer="material-reference",
                        hint="Update reference_model verifier vocabulary with the pure planner.",
                        record_success=False)
            for key in (
                    "view_bounds_anchored",
                    "shape_matches_geometry",
                    "tint_applied",
                    "interactive_response",
                    "container_grouped",
                    "container_union",
                    "container_morphing",
                    "legibility_preserved",
                    "vibrancy_expected",
                    "deterministic_degradation"):
                value = check_bool_field(
                    report,
                    reference_model,
                    key,
                    f"{plan_path}.reference_model",
                    likely_layer="material-reference",
                    hint="Reference model booleans must stay explicit.")
                if value is True:
                    reference_summary[key] += 1

            if isinstance(kind, str):
                for key in ("variant", "semantic_thickness"):
                    if key in reference_values:
                        report.check(
                            f"material reference {key} matches kind",
                            reference_values[key] == kind,
                            path=f"{plan_path}.reference_model.{key}",
                            expected=kind,
                            actual=reference_values[key],
                            likely_layer="material-reference",
                            hint="Material reference variant and thickness should mirror MaterialPlan.kind.",
                            record_success=False)

            expected_accessibility = expected_reference_accessibility_response(plan)
            if (expected_accessibility is not None
                    and "accessibility_response" in reference_values):
                report.check(
                    "material reference accessibility_response is derived",
                    reference_values["accessibility_response"]
                    == expected_accessibility,
                    path=f"{plan_path}.reference_model.accessibility_response",
                    expected=expected_accessibility,
                    actual=reference_values["accessibility_response"],
                    likely_layer="material-reference",
                    hint="Reference accessibility response should mirror the pure decision_trace accessibility flags.",
                    record_success=False)

            expected_performance = expected_reference_performance_response(
                plan,
                kind)
            if (expected_performance is not None
                    and "performance_response" in reference_values):
                report.check(
                    "material reference performance_response is derived",
                    reference_values["performance_response"]
                    == expected_performance,
                    path=f"{plan_path}.reference_model.performance_response",
                    expected=expected_performance,
                    actual=reference_values["performance_response"],
                    likely_layer="material-reference",
                    hint="Reference performance response should mirror fallback, warmup capture, and bounded effect policy.",
                    record_success=False)

            shape_obj = plan.get("shape")
            shape_valid = bool_at(shape_obj, "valid") if isinstance(shape_obj, dict) else None
            shape_kind = string_at(shape_obj, "kind") if isinstance(shape_obj, dict) else None
            shape_rounded = bool_at(shape_obj, "rounded") if isinstance(shape_obj, dict) else None
            shape_capsule = bool_at(shape_obj, "capsule") if isinstance(shape_obj, dict) else None
            if shape_valid is not None and "shape" in reference_values:
                expected_shape = shape_kind
                if expected_shape is None:
                    expected_shape = "invalid"
                    if shape_valid:
                        expected_shape = (
                            "capsule" if shape_capsule else
                            "rounded-rectangle" if shape_rounded else
                            "rectangle")
                report.check(
                    "material reference shape matches geometry analysis",
                    reference_values["shape"] == expected_shape,
                    path=f"{plan_path}.reference_model.shape",
                    expected=expected_shape,
                    actual=reference_values["shape"],
                    likely_layer="material-reference",
                    hint="MaterialReferenceModel.shape should be derived from MaterialPlan.shape.",
                    record_success=False)
            if isinstance(kind, str) and shape_valid is not None:
                expected_anchor = kind != "none" and shape_valid
                for key in ("view_bounds_anchored", "shape_matches_geometry"):
                    if isinstance(reference_model.get(key), bool):
                        report.check(
                            f"material reference {key} is derived",
                            reference_model.get(key) is expected_anchor,
                            path=f"{plan_path}.reference_model.{key}",
                            expected=expected_anchor,
                            actual=reference_model.get(key),
                            likely_layer="material-reference",
                            hint="Liquid Glass reference shape anchoring should follow valid view bounds.",
                            record_success=False)

            tint_obj = plan.get("tint")
            tint_alpha = number_at(tint_obj, "a") if isinstance(tint_obj, dict) else None
            if isinstance(kind, str) and tint_alpha is not None:
                expected_tint = kind != "none" and int(tint_alpha) > 0
                report.check(
                    "material reference tint_applied is derived",
                    reference_model.get("tint_applied") is expected_tint,
                    path=f"{plan_path}.reference_model.tint_applied",
                    expected=expected_tint,
                    actual=reference_model.get("tint_applied"),
                    likely_layer="material-reference",
                    hint="Tint metadata should mirror the resolved MaterialPlan tint alpha.",
                    record_success=False)

            if isinstance(plan_container, dict):
                for reference_key, container_key in (
                        ("interactive_response", "interactive"),
                        ("container_grouped", "participates"),
                        ("container_union", "shape_union_expected"),
                        ("container_morphing", "morph_transitions")):
                    expected = plan_container.get(container_key)
                    if isinstance(expected, bool):
                        report.check(
                            f"material reference {reference_key} mirrors container",
                            reference_model.get(reference_key) is expected,
                            path=f"{plan_path}.reference_model.{reference_key}",
                            expected=expected,
                            actual=reference_model.get(reference_key),
                            likely_layer="material-reference",
                            hint="Liquid Glass container metadata should be pure and backend-independent.",
                            record_success=False)

            fallback = bool_at(plan, "fallback")
            backdrop_sampling = bool_at(plan, "backdrop_sampling")
            if isinstance(kind, str) and isinstance(role, str):
                content_standard = kind != "none" and role == "content"
                expected_technology = (
                    "standard-material" if content_standard else "liquid-glass")
                expected_layer = "inactive"
                expected_policy = "inactive"
                if kind != "none":
                    expected_layer = (
                        "content-layer" if content_standard else "functional-layer")
                    expected_policy = (
                        "standard-material-content-layer"
                        if content_standard
                        else "liquid-glass-functional-layer")
                for reference_key, expected in (
                        ("technology", expected_technology),
                        ("layer", expected_layer),
                        ("material_policy", expected_policy)):
                    if reference_key in reference_values:
                        report.check(
                            f"material reference {reference_key} matches role policy",
                            reference_values[reference_key] == expected,
                            path=f"{plan_path}.reference_model.{reference_key}",
                            expected=expected,
                            actual=reference_values[reference_key],
                            likely_layer="material-reference",
                            hint=(
                                "Keep the pure role policy aligned with Apple "
                                "HIG: Liquid Glass for functional chrome, "
                                "standard material for content."),
                            record_success=False)
                if (content_standard
                        and fallback is False
                        and backdrop_sampling is not None):
                    report.check(
                        "content material does not sample backdrop",
                        backdrop_sampling is False,
                        path=f"{plan_path}.backdrop_sampling",
                        expected=False,
                        actual=backdrop_sampling,
                        likely_layer="material-reference",
                        hint=(
                            "Content-layer surfaces should not execute the "
                            "Liquid Glass backdrop blur path."),
                        record_success=False)
            if isinstance(kind, str) and fallback is not None and backdrop_sampling is not None:
                expected_scope = "none"
                if kind != "none" and backdrop_sampling:
                    expected_scope = "sampled-backdrop"
                elif kind != "none" and fallback:
                    expected_scope = "deterministic-fallback"
                elif kind != "none" and role == "content":
                    expected_scope = "standard-fill"
                if "blending_scope" in reference_values:
                    report.check(
                        "material reference blending_scope is derived",
                        reference_values["blending_scope"] == expected_scope,
                        path=f"{plan_path}.reference_model.blending_scope",
                        expected=expected_scope,
                        actual=reference_values["blending_scope"],
                        likely_layer="material-reference",
                        hint="Reference blending scope should mirror fallback and backdrop_sampling.",
                        record_success=False)

            foreground = plan.get("foreground")
            if isinstance(foreground, dict):
                uses_vibrancy = bool_at(foreground, "uses_vibrancy")
                if uses_vibrancy is not None:
                    report.check(
                        "material reference vibrancy_expected mirrors foreground",
                        reference_model.get("vibrancy_expected") is uses_vibrancy,
                        path=f"{plan_path}.reference_model.vibrancy_expected",
                        expected=uses_vibrancy,
                        actual=reference_model.get("vibrancy_expected"),
                        likely_layer="material-reference",
                        hint="Reference vibrancy expectation should mirror MaterialPlan.foreground.",
                        record_success=False)
                minimum = number_at(foreground, "minimum_contrast_ratio")
                primary = number_at(foreground, "primary_contrast_ratio")
                secondary = number_at(foreground, "secondary_contrast_ratio")
                accent = number_at(foreground, "accent_contrast_ratio")
                if None not in (minimum, primary, secondary, accent):
                    expected_legibility = (
                        float(primary) >= float(minimum)
                        and float(secondary) >= float(minimum)
                        and float(accent) >= float(minimum))
                    report.check(
                        "material reference legibility_preserved is derived",
                        reference_model.get("legibility_preserved") is expected_legibility,
                        path=f"{plan_path}.reference_model.legibility_preserved",
                        expected=expected_legibility,
                        actual=reference_model.get("legibility_preserved"),
                        likely_layer="material-reference",
                        hint="Reference legibility should mirror foreground contrast guarantees.",
                        record_success=False)

            budget = plan.get("resource_budget")
            if isinstance(budget, dict):
                deterministic = bool_at(budget, "deterministic_fallback")
                if deterministic is not None:
                    report.check(
                        "material reference deterministic_degradation mirrors resource budget",
                        reference_model.get("deterministic_degradation") is deterministic,
                        path=f"{plan_path}.reference_model.deterministic_degradation",
                        expected=deterministic,
                        actual=reference_model.get("deterministic_degradation"),
                        likely_layer="material-reference",
                        hint="Reference fallback policy should mirror MaterialPlan.resource_budget.",
                        record_success=False)

        command_descriptor = check_object_field(
            report,
            plan,
            "command_descriptor",
            plan_path,
            likely_layer="material-command",
            hint=(
                "MaterialPlan.command_descriptor should preserve the decoded "
                "MaterialRect command values before fallback or luminance policy "
                "mutates the resolved plan."))
        if command_descriptor is not None:
            descriptor_values = material_descriptor_from(command_descriptor)
            if descriptor_values is None:
                summary["command_descriptor_missing"] = int(
                    summary["command_descriptor_missing"]) + 1
            else:
                summary["command_descriptors"].append(descriptor_values)
            descriptor_kind = check_string_field(
                report,
                command_descriptor,
                "kind",
                f"{plan_path}.command_descriptor",
                likely_layer="material-command",
                hint="The command descriptor must preserve the material kind.")
            if isinstance(descriptor_kind, str):
                report.check(
                    "material command descriptor kind matches plan kind",
                    descriptor_kind == kind,
                    path=f"{plan_path}.command_descriptor.kind",
                    expected=kind,
                    actual=descriptor_kind,
                    likely_layer="material-command",
                    hint=(
                        "The decoded command descriptor kind should feed the "
                        "same pure plan kind."),
                    record_success=False)
            descriptor_role = check_string_field(
                report,
                command_descriptor,
                "role",
                f"{plan_path}.command_descriptor",
                likely_layer="material-command",
                hint="The command descriptor must preserve the material surface role.")
            if isinstance(descriptor_role, str):
                report.check(
                    "material command descriptor role matches plan role",
                    descriptor_role == role,
                    path=f"{plan_path}.command_descriptor.role",
                    expected=role,
                    actual=descriptor_role,
                    likely_layer="material-command",
                    hint=(
                        "The decoded command descriptor role should feed the "
                        "same pure plan role."),
                    record_success=False)
                report.check(
                    "material command descriptor role is known",
                    descriptor_role in ALLOWED_MATERIAL_SURFACE_ROLES,
                    path=f"{plan_path}.command_descriptor.role",
                    expected=sorted(ALLOWED_MATERIAL_SURFACE_ROLES),
                    actual=descriptor_role,
                    likely_layer="material-command",
                    hint=(
                        "Check MaterialRect role decoding and "
                        "MaterialCommandDescriptor serialization."),
                    record_success=False)
            descriptor_container = material_container_from(command_descriptor)
            plan_container_signature = container_identity_signature(
                material_container_from(plan))
            descriptor_container_signature = container_identity_signature(
                descriptor_container)
            report.check(
                "material command descriptor container identity matches plan container",
                descriptor_container_signature == plan_container_signature,
                path=f"{plan_path}.command_descriptor.container",
                expected=plan_container_signature,
                actual=descriptor_container_signature,
                likely_layer="material-container",
                hint=(
                    "The decoded MaterialRect container identity should feed "
                    "MaterialPlan.container. Resolved morph_transitions may differ "
                    "when Reduce Motion is enabled."),
                record_success=False)
            descriptor_tint = check_object_field(
                report,
                command_descriptor,
                "tint",
                f"{plan_path}.command_descriptor",
                likely_layer="material-command",
                hint="The command descriptor must preserve the MaterialRect tint.")
            if descriptor_tint is not None:
                for key in MATERIAL_TINT_FIELDS:
                    check_number_field(
                        report,
                        descriptor_tint,
                        key,
                        f"{plan_path}.command_descriptor.tint",
                        min_value=0.0,
                        likely_layer="material-command",
                        hint="Check MaterialRect tint decoding and descriptor serialization.")
            for key in MATERIAL_COMMAND_DESCRIPTOR_NUMERIC_FIELDS:
                check_number_field(
                    report,
                    command_descriptor,
                    key,
                    f"{plan_path}.command_descriptor",
                    min_value=0.0,
                    likely_layer="material-command",
                    hint=(
                        "Check MaterialRect command decoding and "
                        "MaterialCommandDescriptor serialization."))
        else:
            summary["command_descriptor_missing"] = int(
                summary["command_descriptor_missing"]) + 1

        plan_blur_radius: int | float | None = None
        plan_edge_highlight: int | float | None = None
        plan_luminance_floor: int | float | None = None
        plan_luminance_gain: int | float | None = None
        for key in MATERIAL_PLAN_NUMERIC_FIELDS:
            number = check_number_field(
                report,
                plan,
                key,
                plan_path,
                min_value=0.0,
                likely_layer=likely_layer,
                hint="Check pure MaterialPlan clamping and backend JSON serialization.")
            if key == "blur_radius":
                plan_blur_radius = number
                if isinstance(number, (int, float)):
                    bounds = summary["resource_bounds"]
                    bounds["max_plan_blur_radius"] = max(
                        float(bounds["max_plan_blur_radius"]),
                        float(number))
            elif key == "edge_highlight":
                plan_edge_highlight = number
            elif key == "luminance_floor":
                plan_luminance_floor = number
            elif key == "luminance_gain":
                plan_luminance_gain = number
        backdrop_sampling = check_bool_field(
            report,
            plan,
            "backdrop_sampling",
            plan_path,
            likely_layer=likely_layer,
            hint="Resolved plans must explicitly state whether backdrop sampling runs.")
        fallback = check_bool_field(
            report,
            plan,
            "fallback",
            plan_path,
            likely_layer=likely_layer,
            hint="Resolved plans must explicitly state whether deterministic fallback runs.")
        fallback_path = check_string_field(
            report,
            plan,
            "fallback_path",
            plan_path,
            likely_layer=likely_layer,
            hint="Fallback state must name the exact fallback path.")
        fallback_reason = check_string_field(
            report,
            plan,
            "fallback_reason",
            plan_path,
            allow_empty=True,
            likely_layer=likely_layer,
            hint="Fallback plans require a reason; non-fallback plans should leave it empty.")
        if isinstance(fallback_path, str):
            paths = summary["fallback_paths"]
            paths[fallback_path] = paths.get(fallback_path, 0) + 1
            report.check(
                "material fallback path is known",
                fallback_path in ALLOWED_MATERIAL_FALLBACK_PATHS,
                path=f"{plan_path}.fallback_path",
                expected=sorted(ALLOWED_MATERIAL_FALLBACK_PATHS),
                actual=fallback_path,
                likely_layer=likely_layer,
                hint="Keep MaterialFallbackPath serialization and verifier vocabulary in sync.",
                record_success=False)
        if (fallback is True
                and isinstance(fallback_reason, str)
                and fallback_reason):
            reasons = summary["fallback_reasons"]
            reasons[fallback_reason] = reasons.get(fallback_reason, 0) + 1
        luminance_curve = check_object_field(
            report,
            plan,
            "luminance_curve",
            plan_path,
            likely_layer=likely_layer,
            hint="MaterialPlan must expose the pure luminance curve executed by the backend.")
        if luminance_curve is not None:
            curve_name = ""
            curve_backdrop_driven: bool | None = None
            curve_bounded: bool | None = None
            curve_numbers: dict[str, int | float] = {}
            for key in MATERIAL_LUMINANCE_CURVE_FIELDS:
                if key in ("backdrop_driven", "bounded"):
                    value = check_bool_field(
                        report,
                        luminance_curve,
                        key,
                        f"{plan_path}.luminance_curve",
                        likely_layer=likely_layer,
                        likely_pass="luminance-curve",
                        hint="Luminance curve booleans must stay explicit.")
                    if key == "backdrop_driven":
                        curve_backdrop_driven = value
                        if value is True:
                            summary["luminance_curve_backdrop_driven"] = int(
                                summary["luminance_curve_backdrop_driven"]) + 1
                    elif key == "bounded":
                        curve_bounded = value
                elif key == "name":
                    value = check_string_field(
                        report,
                        luminance_curve,
                        key,
                        f"{plan_path}.luminance_curve",
                        likely_layer=likely_layer,
                        hint="Luminance curve must name the pure contrast transform.")
                    if isinstance(value, str):
                        curve_name = value
                        curves = summary["luminance_curves"]
                        curves[value] = curves.get(value, 0) + 1
                        report.check(
                            "material luminance curve is known",
                            value in ALLOWED_MATERIAL_LUMINANCE_CURVES,
                            path=f"{plan_path}.luminance_curve.name",
                            expected=sorted(ALLOWED_MATERIAL_LUMINANCE_CURVES),
                            actual=value,
                            likely_layer=likely_layer,
                            likely_pass="luminance-curve",
                            hint="Add new luminance curves to the verifier vocabulary when intentional.",
                            record_success=False)
                else:
                    value = check_number_field(
                        report,
                        luminance_curve,
                        key,
                        f"{plan_path}.luminance_curve",
                        min_value=0.0,
                        likely_layer=likely_layer,
                        likely_pass="luminance-curve",
                        hint="Luminance curve numeric fields must be non-negative.")
                    if isinstance(value, (int, float)):
                        curve_numbers[key] = value
            if isinstance(plan_luminance_floor, (int, float)):
                report.check(
                    "material luminance curve floor matches plan",
                    abs(float(curve_numbers.get("floor", -1.0))
                        - float(plan_luminance_floor)) <= 0.0001,
                    path=f"{plan_path}.luminance_curve.floor",
                    expected=plan_luminance_floor,
                    actual=curve_numbers.get("floor"),
                    likely_layer=likely_layer,
                    likely_pass="luminance-curve",
                    hint="Keep MaterialPlan.luminance_floor and luminance_curve.floor in sync.",
                    record_success=False)
            if isinstance(plan_luminance_gain, (int, float)):
                report.check(
                    "material luminance curve gain matches plan",
                    abs(float(curve_numbers.get("gain", -1.0))
                        - float(plan_luminance_gain)) <= 0.0001,
                    path=f"{plan_path}.luminance_curve.gain",
                    expected=plan_luminance_gain,
                    actual=curve_numbers.get("gain"),
                    likely_layer=likely_layer,
                    likely_pass="luminance-curve",
                    hint="Keep MaterialPlan.luminance_gain and luminance_curve.gain in sync.",
                    record_success=False)
            for key in ("floor", "midpoint", "edge_lift"):
                value = curve_numbers.get(key)
                if isinstance(value, (int, float)):
                    report.check(
                        f"material luminance curve {key} is normalized",
                        float(value) <= 1.0,
                        path=f"{plan_path}.luminance_curve.{key}",
                        expected={"<=": 1.0},
                        actual=value,
                        likely_layer=likely_layer,
                        likely_pass="luminance-curve",
                        hint="Clamp luminance curve unit fields in pure planning.",
                        record_success=False)
            for key in ("gamma", "gain", "contrast"):
                value = curve_numbers.get(key)
                if isinstance(value, (int, float)):
                    report.check(
                        f"material luminance curve {key} is bounded",
                        float(value) <= 4.0,
                        path=f"{plan_path}.luminance_curve.{key}",
                        expected={"<=": 4.0},
                        actual=value,
                        likely_layer=likely_layer,
                        likely_pass="luminance-curve",
                        hint="Keep shader luminance curve inputs bounded.",
                        record_success=False)
            if backdrop_sampling is True:
                report.check(
                    "material sampled backdrop uses adaptive luminance curve",
                    curve_name == "adaptive-backdrop-luma"
                    and curve_backdrop_driven is True
                    and curve_bounded is True,
                    path=f"{plan_path}.luminance_curve",
                    expected="adaptive backdrop-driven bounded curve",
                    actual=luminance_curve,
                    likely_layer=likely_layer,
                    likely_pass="luminance-curve",
                    hint="Sampled glass should expose the backdrop-driven luminance transform.",
                    record_success=False)
            elif backdrop_sampling is False:
                report.check(
                    "material fallback uses flat luminance curve",
                    curve_name == "fallback-flat"
                    and curve_backdrop_driven is False
                    and curve_bounded is True,
                    path=f"{plan_path}.luminance_curve",
                    expected="flat bounded fallback curve",
                    actual=luminance_curve,
                    likely_layer=likely_layer,
                    likely_pass="luminance-curve",
                    hint="Fallback plans must not report a backdrop-driven luminance curve.",
                    record_success=False)
        foreground = check_object_field(
            report,
            plan,
            "foreground",
            plan_path,
            likely_layer="material-foreground",
            hint=(
                "MaterialPlan.foreground must expose the pure foreground "
                "legibility and vibrancy recommendation."))
        if foreground is not None:
            foreground_summary = summary["foreground"]
            for key in MATERIAL_FOREGROUND_COLOR_FIELDS:
                color = check_object_field(
                    report,
                    foreground,
                    key,
                    f"{plan_path}.foreground",
                    likely_layer="material-foreground",
                    hint="Foreground recommendation colors must be RGBA objects.")
                if color is not None:
                    for channel in MATERIAL_COLOR_FIELDS:
                        value = check_number_field(
                            report,
                            color,
                            channel,
                            f"{plan_path}.foreground.{key}",
                            min_value=0.0,
                            likely_layer="material-foreground",
                            hint="Foreground color channels must be numeric.")
                        if isinstance(value, (int, float)):
                            report.check(
                                "material foreground color channel is byte-sized",
                                0.0 <= float(value) <= 255.0,
                                path=f"{plan_path}.foreground.{key}.{channel}",
                                expected={"range": [0, 255]},
                                actual=value,
                                likely_layer="material-foreground",
                                hint="Serialize foreground colors as RGBA byte channels.",
                                record_success=False)
            scheme = check_string_field(
                report,
                foreground,
                "scheme",
                f"{plan_path}.foreground",
                likely_layer="material-foreground",
                hint="Foreground scheme must name the pure material legibility path.")
            if isinstance(scheme, str):
                schemes = foreground_summary["schemes"]
                schemes[scheme] = schemes.get(scheme, 0) + 1
                report.check(
                    "material foreground scheme is known",
                    scheme in ALLOWED_MATERIAL_FOREGROUND_SCHEMES,
                    path=f"{plan_path}.foreground.scheme",
                    expected=sorted(ALLOWED_MATERIAL_FOREGROUND_SCHEMES),
                    actual=scheme,
                    likely_layer="material-foreground",
                    hint="Update foreground scheme vocabulary with the pure planner.",
                    record_success=False)
            source = check_string_field(
                report,
                foreground,
                "source",
                f"{plan_path}.foreground",
                likely_layer="material-foreground",
                hint="Foreground source must name the input that drove color selection.")
            if isinstance(source, str):
                sources = foreground_summary["sources"]
                sources[source] = sources.get(source, 0) + 1
                report.check(
                    "material foreground source is known",
                    source in ALLOWED_MATERIAL_FOREGROUND_SOURCES,
                    path=f"{plan_path}.foreground.source",
                    expected=sorted(ALLOWED_MATERIAL_FOREGROUND_SOURCES),
                    actual=source,
                    likely_layer="material-foreground",
                    hint="Update foreground source vocabulary with the pure planner.",
                    record_success=False)
            foreground_numbers: dict[str, int | float] = {}
            for key in MATERIAL_FOREGROUND_NUMERIC_FIELDS:
                number = check_number_field(
                    report,
                    foreground,
                    key,
                    f"{plan_path}.foreground",
                    min_value=0.0,
                    likely_layer="material-foreground",
                    hint="Foreground contrast fields must be non-negative numbers.")
                if isinstance(number, (int, float)):
                    foreground_numbers[key] = number
                    if key == "background_luma":
                        foreground_summary["max_background_luma"] = max(
                            float(foreground_summary["max_background_luma"]),
                            float(number))
                    elif key == "primary_contrast_ratio":
                        current = float(foreground_summary["min_primary_contrast"])
                        foreground_summary["min_primary_contrast"] = (
                            float(number) if current == 0.0
                            else min(current, float(number)))
                    elif key == "secondary_contrast_ratio":
                        current = float(foreground_summary["min_secondary_contrast"])
                        foreground_summary["min_secondary_contrast"] = (
                            float(number) if current == 0.0
                            else min(current, float(number)))
                    elif key == "accent_contrast_ratio":
                        current = float(foreground_summary["min_accent_contrast"])
                        foreground_summary["min_accent_contrast"] = (
                            float(number) if current == 0.0
                            else min(current, float(number)))
                    elif key == "minimum_contrast_ratio":
                        current = float(foreground_summary["min_minimum_contrast"])
                        foreground_summary["min_minimum_contrast"] = (
                            float(number) if current == 0.0
                            else min(current, float(number)))
            for key in MATERIAL_FOREGROUND_BOOL_FIELDS:
                value = check_bool_field(
                    report,
                    foreground,
                    key,
                    f"{plan_path}.foreground",
                    likely_layer="material-foreground",
                    hint="Foreground recommendation booleans must stay explicit.")
                if value is True:
                    if key == "backdrop_driven":
                        foreground_summary["backdrop_driven"] = (
                            int(foreground_summary["backdrop_driven"]) + 1)
                    elif key == "high_contrast":
                        foreground_summary["high_contrast"] = (
                            int(foreground_summary["high_contrast"]) + 1)
                    elif key == "uses_vibrancy":
                        foreground_summary["vibrant"] = (
                            int(foreground_summary["vibrant"]) + 1)
                    elif key == "deterministic":
                        foreground_summary["deterministic"] = (
                            int(foreground_summary["deterministic"]) + 1)
            minimum_contrast = foreground_numbers.get("minimum_contrast_ratio")
            if isinstance(minimum_contrast, (int, float)):
                for key in (
                        "primary_contrast_ratio",
                        "secondary_contrast_ratio",
                        "accent_contrast_ratio"):
                    contrast = foreground_numbers.get(key)
                    if not isinstance(contrast, (int, float)):
                        continue
                    report.check(
                        f"material foreground {key} meets plan minimum",
                        float(contrast) + 0.0001 >= float(minimum_contrast),
                        path=f"{plan_path}.foreground.{key}",
                        expected={">=": minimum_contrast},
                        actual=contrast,
                        likely_layer="material-foreground",
                        hint=(
                            "Adjust MaterialPlan.foreground color selection before "
                            "weakening contrast thresholds."),
                        record_success=False)
            if backdrop_sampling is True:
                report.check(
                    "sampled material foreground is backdrop-driven",
                    foreground.get("backdrop_driven") is True
                    and foreground.get("uses_vibrancy") is True,
                    path=f"{plan_path}.foreground",
                    expected={
                        "backdrop_driven": True,
                        "uses_vibrancy": True,
                    },
                    actual={
                        "backdrop_driven": foreground.get("backdrop_driven"),
                        "uses_vibrancy": foreground.get("uses_vibrancy"),
                    },
                    likely_layer="material-foreground",
                    hint="Sampled Liquid Glass plans should expose vibrancy-driven foreground metadata.",
                    record_success=False)
        sample_taps = check_number_field(
            report,
            plan,
            "sample_taps",
            plan_path,
            min_value=0.0,
            likely_layer=likely_layer,
            hint="Material sample taps should be an explicit bounded integer.")
        if isinstance(sample_taps, (int, float)):
            bounds = summary["resource_bounds"]
            bounds["max_plan_sample_taps"] = max(
                int(bounds["max_plan_sample_taps"]),
                int(sample_taps))
            bounds["total_plan_sample_taps"] = (
                int(bounds["total_plan_sample_taps"]) + int(sample_taps))

        sampling_kernel = check_object_field(
            report,
            plan,
            "sampling_kernel",
            plan_path,
            likely_layer=likely_layer,
            hint="MaterialPlan must expose the pure backdrop sampling kernel.")
        if sampling_kernel is not None:
            kernel_name = ""
            kernel_weight_profile = ""
            kernel_radius: int | float | None = None
            kernel_taps: int | float | None = None
            kernel_requires_backdrop: bool | None = None
            kernel_bounded: bool | None = None
            for key in MATERIAL_SAMPLING_KERNEL_FIELDS:
                if key in ("requires_backdrop", "bounded"):
                    value = check_bool_field(
                        report,
                        sampling_kernel,
                        key,
                        f"{plan_path}.sampling_kernel",
                        likely_layer=likely_layer,
                        likely_pass="sampling-kernel",
                        hint="Sampling kernel booleans must stay explicit.")
                    if key == "requires_backdrop":
                        kernel_requires_backdrop = value
                    elif key == "bounded":
                        kernel_bounded = value
                elif key in ("radius", "sample_taps", "blur_step_scale"):
                    value = check_number_field(
                        report,
                        sampling_kernel,
                        key,
                        f"{plan_path}.sampling_kernel",
                        min_value=0.0,
                        likely_layer=likely_layer,
                        likely_pass="sampling-kernel",
                        hint="Sampling kernel numeric fields must be non-negative.")
                    if key == "radius":
                        kernel_radius = value
                        if isinstance(value, (int, float)):
                            bounds = summary["resource_bounds"]
                            bounds["max_sampling_kernel_radius"] = max(
                                int(bounds["max_sampling_kernel_radius"]),
                                int(value))
                    elif key == "sample_taps":
                        kernel_taps = value
                else:
                    value = check_string_field(
                        report,
                        sampling_kernel,
                        key,
                        f"{plan_path}.sampling_kernel",
                        likely_layer=likely_layer,
                        hint="Sampling kernel fields must name the pure blur contract.")
                    if key == "name" and isinstance(value, str):
                        kernel_name = value
                        kernels = summary["sampling_kernels"]
                        kernels[value] = kernels.get(value, 0) + 1
                        report.check(
                            "material sampling kernel is known",
                            value in ALLOWED_MATERIAL_SAMPLING_KERNELS,
                            path=f"{plan_path}.sampling_kernel.name",
                            expected=sorted(ALLOWED_MATERIAL_SAMPLING_KERNELS),
                            actual=value,
                            likely_layer=likely_layer,
                            likely_pass="sampling-kernel",
                            hint="Add new sampling kernels to the verifier vocabulary when intentional.",
                            record_success=False)
                    elif key == "weight_profile" and isinstance(value, str):
                        kernel_weight_profile = value
                        profiles = summary["sampling_weight_profiles"]
                        profiles[value] = profiles.get(value, 0) + 1
                        report.check(
                            "material sampling weight profile is known",
                            value in ALLOWED_MATERIAL_SAMPLING_WEIGHT_PROFILES,
                            path=f"{plan_path}.sampling_kernel.weight_profile",
                            expected=sorted(ALLOWED_MATERIAL_SAMPLING_WEIGHT_PROFILES),
                            actual=value,
                            likely_layer=likely_layer,
                            likely_pass="sampling-kernel",
                            hint="Add new sampling weight profiles to the verifier vocabulary when intentional.",
                            record_success=False)
            if isinstance(sample_taps, (int, float)) and isinstance(kernel_taps, (int, float)):
                report.check(
                    "material sampling kernel taps match plan",
                    int(kernel_taps) == int(sample_taps),
                    path=f"{plan_path}.sampling_kernel.sample_taps",
                    expected=int(sample_taps),
                    actual=int(kernel_taps),
                    likely_layer=likely_layer,
                    likely_pass="sampling-kernel",
                    hint="Keep MaterialPlan.sample_taps and sampling_kernel.sample_taps in sync.",
                    record_success=False)
            if backdrop_sampling is True:
                report.check(
                    "material backdrop sampling uses active kernel",
                    kernel_name != "none"
                    and kernel_requires_backdrop is True
                    and kernel_bounded is True
                    and isinstance(kernel_radius, (int, float))
                    and int(kernel_radius) > 0
                    and kernel_weight_profile != "none",
                    path=f"{plan_path}.sampling_kernel",
                    expected="active bounded backdrop kernel",
                    actual=sampling_kernel,
                    likely_layer=likely_layer,
                    likely_pass="sampling-kernel",
                    hint="Backdrop plans should expose the pure blur kernel executed by the backend.",
                    record_success=False)
            elif backdrop_sampling is False:
                report.check(
                    "material fallback uses no sampling kernel",
                    kernel_name == "none"
                    and kernel_requires_backdrop is False
                    and isinstance(kernel_radius, (int, float))
                    and int(kernel_radius) == 0,
                    path=f"{plan_path}.sampling_kernel",
                    expected="inactive kernel",
                    actual=sampling_kernel,
                    likely_layer=likely_layer,
                    likely_pass="sampling-kernel",
                    hint="Fallback plans must not carry stale blur-kernel metadata.",
                    record_success=False)

        geometry = check_object_field(
            report,
            plan,
            "geometry",
            plan_path,
            likely_layer=likely_layer,
            hint="MaterialPlan.geometry should be serialized from the pure request.")
        geometry_numbers: JsonObject = {}
        if geometry is not None:
            for key in MATERIAL_GEOMETRY_FIELDS:
                number = check_number_field(
                    report,
                    geometry,
                    key,
                    f"{plan_path}.geometry",
                    likely_layer=likely_layer,
                    hint="Check MaterialPlan geometry serialization.")
                if number is not None:
                    geometry_numbers[key] = float(number)

        shape = check_object_field(
            report,
            plan,
            "shape",
            plan_path,
            likely_layer="material-shape",
            hint=(
                "MaterialPlan.shape should expose the pure geometry "
                "analysis and effective radius executed by backends."))
        if shape is not None:
            shape_summary = summary["shape"]
            shape_bools: JsonObject = {}
            shape_numbers: JsonObject = {}
            for key in MATERIAL_SHAPE_BOOL_FIELDS:
                item = check_bool_field(
                    report,
                    shape,
                    key,
                    f"{plan_path}.shape",
                    likely_layer="material-shape",
                    hint="Check MaterialShapeAnalysis boolean serialization.")
                if item is not None:
                    shape_bools[key] = item
            for key in MATERIAL_SHAPE_NUMERIC_FIELDS:
                number = check_number_field(
                    report,
                    shape,
                    key,
                    f"{plan_path}.shape",
                    min_value=0.0,
                    likely_layer="material-shape",
                    hint="Check MaterialShapeAnalysis numeric serialization.")
                if number is not None:
                    shape_numbers[key] = float(number)
            if shape_bools.get("valid"):
                shape_summary["valid"] = int(shape_summary["valid"]) + 1
            if shape_bools.get("rounded"):
                shape_summary["rounded"] = int(shape_summary["rounded"]) + 1
            if shape_bools.get("capsule"):
                shape_summary["capsule"] = int(shape_summary["capsule"]) + 1
            if shape_bools.get("radius_clamped"):
                shape_summary["radius_clamped"] = (
                    int(shape_summary["radius_clamped"]) + 1)
            shape_kind = string_at(shape, "kind")
            if shape_kind is not None:
                report.check(
                    "material shape kind uses allowed vocabulary",
                    shape_kind in ALLOWED_MATERIAL_REFERENCE_SHAPES
                    and shape_kind != "none",
                    path=f"{plan_path}.shape.kind",
                    expected=sorted(ALLOWED_MATERIAL_REFERENCE_SHAPES - {"none"}),
                    actual=shape_kind,
                    likely_layer="material-shape",
                    hint="Add new MaterialShapeKind values to the verifier when intentional.",
                    record_success=False)
            for key, summary_key in (
                    ("surface_area", "max_surface_area"),
                    ("effective_radius", "max_effective_radius"),
                    ("radius_limit", "max_radius_limit"),
                    ("normalized_radius", "max_normalized_radius")):
                if key in shape_numbers:
                    shape_summary[summary_key] = max(
                        float(shape_summary[summary_key]),
                        shape_numbers[key])
            if {"w", "h", "radius"} <= geometry_numbers.keys():
                width = geometry_numbers["w"]
                height = geometry_numbers["h"]
                radius = geometry_numbers["radius"]
                expected_valid = width > 0.0 and height > 0.0
                expected_limit = min(width, height) * 0.5 if expected_valid else 0.0
                expected_radius = min(max(radius, 0.0), expected_limit)
                expected_normalized = (
                    expected_radius / expected_limit
                    if expected_limit > 0.0 else 0.0)
                expected_kind = "invalid"
                if expected_valid:
                    if expected_normalized >= 0.999:
                        expected_kind = "capsule"
                    elif expected_radius > 0.0:
                        expected_kind = "rounded-rectangle"
                    else:
                        expected_kind = "rectangle"
                if "valid" in shape_bools:
                    report.check(
                        "material shape validity matches geometry",
                        shape_bools["valid"] == expected_valid,
                        path=f"{plan_path}.shape.valid",
                        expected=expected_valid,
                        actual=shape_bools["valid"],
                        likely_layer="material-shape",
                        hint=(
                            "Keep MaterialShapeAnalysis.valid derived from "
                            "MaterialGeometry width and height."))
                if "effective_radius" in shape_numbers:
                    report.check(
                        "material shape effective radius matches geometry",
                        abs(shape_numbers["effective_radius"]
                            - expected_radius) <= 0.0001,
                        path=f"{plan_path}.shape.effective_radius",
                        expected=expected_radius,
                        actual=shape_numbers["effective_radius"],
                        likely_layer="material-shape",
                        hint=(
                            "Backends should consume MaterialPlan.shape."
                            "effective_radius rather than raw geometry.radius."))
                if "radius_limit" in shape_numbers:
                    report.check(
                        "material shape radius limit matches geometry",
                        abs(shape_numbers["radius_limit"]
                            - expected_limit) <= 0.0001,
                        path=f"{plan_path}.shape.radius_limit",
                        expected=expected_limit,
                        actual=shape_numbers["radius_limit"],
                        likely_layer="material-shape",
                        hint=(
                            "MaterialShapeAnalysis.radius_limit should be "
                            "half of the smaller valid extent."))
                if "normalized_radius" in shape_numbers:
                    report.check(
                        "material shape normalized radius matches geometry",
                        abs(shape_numbers["normalized_radius"]
                            - expected_normalized) <= 0.0001,
                        path=f"{plan_path}.shape.normalized_radius",
                        expected=expected_normalized,
                        actual=shape_numbers["normalized_radius"],
                        likely_layer="material-shape",
                        hint=(
                            "MaterialShapeAnalysis.normalized_radius should "
                            "describe the executable radius within its limit."))
                if shape_kind is not None:
                    report.check(
                        "material shape kind matches geometry",
                        shape_kind == expected_kind,
                        path=f"{plan_path}.shape.kind",
                        expected=expected_kind,
                        actual=shape_kind,
                        likely_layer="material-shape",
                        hint=(
                            "MaterialShapeAnalysis.kind should classify the "
                            "same executable shape that backends render."))
                if "capsule" in shape_bools:
                    report.check(
                        "material shape capsule flag matches geometry",
                        shape_bools["capsule"] == (expected_kind == "capsule"),
                        path=f"{plan_path}.shape.capsule",
                        expected=(expected_kind == "capsule"),
                        actual=shape_bools["capsule"],
                        likely_layer="material-shape",
                        hint=(
                            "MaterialShapeAnalysis.capsule should be true only "
                            "when the clamped radius reaches half of the smaller extent."))

        render_target_pixel_count: int | None = None
        render_target = check_object_field(
            report,
            plan,
            "render_target",
            plan_path,
            likely_layer=likely_layer,
            hint=(
                "MaterialPlan.render_target should expose the render target "
                "metadata and pixel budget decision used by pure planning."))
        if render_target is not None:
            render_summary = summary["render_target"]
            render_values: dict[str, int | float] = {}
            for key in MATERIAL_RENDER_TARGET_INT_FIELDS:
                value = check_number_field(
                    report,
                    render_target,
                    key,
                    f"{plan_path}.render_target",
                    min_value=0.0,
                    likely_layer=likely_layer,
                    hint="Render target dimensions and pixel count must be non-negative.")
                if isinstance(value, (int, float)):
                    render_values[key] = value
                    if key == "pixel_count":
                        render_summary["max_pixel_count"] = max(
                            int(render_summary["max_pixel_count"]),
                            int(value))
            width = render_values.get("width")
            height = render_values.get("height")
            pixel_count = render_values.get("pixel_count")
            if isinstance(pixel_count, (int, float)):
                render_target_pixel_count = int(pixel_count)
            if all(isinstance(value, (int, float))
                   for value in (width, height, pixel_count)):
                assert isinstance(width, (int, float))
                assert isinstance(height, (int, float))
                assert isinstance(pixel_count, (int, float))
                expected_pixels = int(width) * int(height)
                report.check(
                    "material render target pixel count matches dimensions",
                    int(pixel_count) == expected_pixels,
                    path=f"{plan_path}.render_target.pixel_count",
                    expected=expected_pixels,
                    actual=int(pixel_count),
                    likely_layer=likely_layer,
                    hint="Keep MaterialRenderTargetAnalysis.pixel_count derived from width * height.",
                    record_success=False)
            check_number_field(
                report,
                render_target,
                "scale",
                f"{plan_path}.render_target",
                min_value=0.0,
                likely_layer=likely_layer,
                hint="Render target scale should be sanitized before planning.")
            ready = None
            within_budget = None
            for key in MATERIAL_RENDER_TARGET_BOOL_FIELDS:
                value = check_bool_field(
                    report,
                    render_target,
                    key,
                    f"{plan_path}.render_target",
                    likely_layer=likely_layer,
                    hint="Render target readiness and budget flags must be explicit.")
                if value is True:
                    render_summary[key] = int(render_summary[key]) + 1
                if key == "ready":
                    ready = value
                elif key == "within_backdrop_budget":
                    within_budget = value
            if isinstance(width, (int, float)) and isinstance(height, (int, float)):
                expected_ready = int(width) > 0 and int(height) > 0
                report.check(
                    "material render target ready matches dimensions",
                    ready == expected_ready,
                    path=f"{plan_path}.render_target.ready",
                    expected=expected_ready,
                    actual=ready,
                    likely_layer=likely_layer,
                    hint="Render target readiness should be derived from positive dimensions.",
                    record_success=False)
            pixel_format = check_string_field(
                report,
                render_target,
                "pixel_format",
                f"{plan_path}.render_target",
                likely_layer=likely_layer,
                hint="Render target pixel format should name the backend target format.")
            if isinstance(pixel_format, str):
                formats = render_summary["pixel_formats"]
                formats[pixel_format] = formats.get(pixel_format, 0) + 1
            if backdrop_sampling is True:
                report.check(
                    "material sampled render target is ready and within budget",
                    ready is True and within_budget is True,
                    path=f"{plan_path}.render_target",
                    expected={"ready": True, "within_backdrop_budget": True},
                    actual={"ready": ready, "within_backdrop_budget": within_budget},
                    likely_layer=likely_layer,
                    hint="Sampled material plans require a render target inside the backdrop budget.",
                    record_success=False)

        decision_trace = check_object_field(
            report,
            plan,
            "decision_trace",
            plan_path,
            likely_layer=likely_layer,
            hint=(
                "MaterialPlan.decision_trace should expose the pure gate "
                "booleans that led to backdrop sampling or fallback."))
        if decision_trace is not None:
            trace_summary = summary["decision_trace"]
            trace_values: dict[str, bool] = {}
            for key in MATERIAL_DECISION_TRACE_BOOL_FIELDS:
                value = check_bool_field(
                    report,
                    decision_trace,
                    key,
                    f"{plan_path}.decision_trace",
                    likely_layer=likely_layer,
                    hint="Decision trace gate values must be explicit booleans.")
                if isinstance(value, bool):
                    trace_values[key] = value
                    if key in (
                            "role_allows_liquid_glass",
                            "content_layer_standard_material",
                            "liquid_glass_backdrop_candidate",
                            "can_sample_backdrop",
                            "backend_supports_backdrop",
                            "backdrop_source_ready",
                            "next_frame_capture_required",
                            "reduced_transparency",
                            "increase_contrast",
                            "reduce_motion") and value:
                        trace_summary[key] = int(trace_summary[key]) + 1
            first_blocker = check_string_field(
                report,
                decision_trace,
                "first_blocker",
                f"{plan_path}.decision_trace",
                likely_layer=likely_layer,
                hint="Decision trace should name the first pure blocker.")
            if isinstance(first_blocker, str):
                blockers = trace_summary["first_blockers"]
                blockers[first_blocker] = blockers.get(first_blocker, 0) + 1
                report.check(
                    "material decision blocker is known",
                    first_blocker in ALLOWED_MATERIAL_FALLBACK_PATHS,
                    path=f"{plan_path}.decision_trace.first_blocker",
                    expected=sorted(ALLOWED_MATERIAL_FALLBACK_PATHS),
                    actual=first_blocker,
                    likely_layer=likely_layer,
                    hint="Keep MaterialDecisionTrace.first_blocker aligned with fallback paths.",
                    record_success=False)
                report.check(
                    "material decision blocker matches fallback path",
                    first_blocker == fallback_path,
                    path=f"{plan_path}.decision_trace.first_blocker",
                    expected=fallback_path,
                    actual=first_blocker,
                    likely_layer=likely_layer,
                    hint=(
                        "The first decision blocker should explain the same "
                        "fallback path that the plan exposes."),
                    record_success=False)
            if isinstance(role, str):
                expected_role_allows = role != "content"
                actual_role_allows = trace_values.get("role_allows_liquid_glass")
                report.check(
                    "material decision role_allows_liquid_glass is derived",
                    actual_role_allows == expected_role_allows,
                    path=f"{plan_path}.decision_trace.role_allows_liquid_glass",
                    expected=expected_role_allows,
                    actual=actual_role_allows,
                    likely_layer="material-decision",
                    hint=(
                        "Content surfaces must resolve to standard material; "
                        "functional chrome/navigation roles may use Liquid Glass."),
                    record_success=False)
                if "has_material" in trace_values:
                    expected_content_standard = (
                        trace_values["has_material"]
                        and not expected_role_allows)
                    report.check(
                        "material decision content standard policy is derived",
                        trace_values.get("content_layer_standard_material")
                        == expected_content_standard,
                        path=(
                            f"{plan_path}.decision_trace."
                            "content_layer_standard_material"),
                        expected=expected_content_standard,
                        actual=trace_values.get(
                            "content_layer_standard_material"),
                        likely_layer="material-decision",
                        hint=(
                            "Content surfaces should be explicit standard "
                            "material plans rather than Liquid Glass fallbacks."),
                        record_success=False)
                    expected_backdrop_candidate = (
                        trace_values["has_material"]
                        and expected_role_allows)
                    report.check(
                        "material decision liquid glass candidate is derived",
                        trace_values.get("liquid_glass_backdrop_candidate")
                        == expected_backdrop_candidate,
                        path=(
                            f"{plan_path}.decision_trace."
                            "liquid_glass_backdrop_candidate"),
                        expected=expected_backdrop_candidate,
                        actual=trace_values.get(
                            "liquid_glass_backdrop_candidate"),
                        likely_layer="material-decision",
                        hint=(
                            "Only functional material roles should enter the "
                            "Liquid Glass backdrop sampling predicate."),
                        record_success=False)
            can_sample = trace_values.get("can_sample_backdrop")
            if can_sample is not None:
                report.check(
                    "material decision trace matches backdrop sampling",
                    can_sample == backdrop_sampling,
                    path=f"{plan_path}.decision_trace.can_sample_backdrop",
                    expected=backdrop_sampling,
                    actual=can_sample,
                    likely_layer=likely_layer,
                    hint=(
                        "Decision trace can_sample_backdrop should be the "
                        "pure predicate behind MaterialPlan.backdrop_sampling."),
                    record_success=False)
            backend_inputs = (
                trace_values.get("capability_material_surfaces"),
                trace_values.get("capability_material_backdrop_blur"),
                trace_values.get("capability_shader_blur"),
            )
            if all(isinstance(value, bool) for value in backend_inputs):
                expected_backend = all(backend_inputs)
                report.check(
                    "material decision backend support is derived",
                    trace_values.get("backend_supports_backdrop")
                    == expected_backend,
                    path=f"{plan_path}.decision_trace.backend_supports_backdrop",
                    expected=expected_backend,
                    actual=trace_values.get("backend_supports_backdrop"),
                    likely_layer=likely_layer,
                    hint=(
                        "backend_supports_backdrop should be the conjunction "
                        "of material surface, backdrop blur, and shader blur capabilities."),
                    record_success=False)
            backdrop_inputs = (
                trace_values.get("backdrop_available"),
                trace_values.get("backdrop_stable"),
            )
            if all(isinstance(value, bool) for value in backdrop_inputs):
                expected_source = all(backdrop_inputs)
                report.check(
                    "material decision backdrop source readiness is derived",
                    trace_values.get("backdrop_source_ready") == expected_source,
                    path=f"{plan_path}.decision_trace.backdrop_source_ready",
                    expected=expected_source,
                    actual=trace_values.get("backdrop_source_ready"),
                    likely_layer=likely_layer,
                    hint=(
                        "backdrop_source_ready should reflect available and "
                        "stable backdrop input."),
                    record_success=False)
            if all(key in trace_values for key in (
                    "quality_switches_allow_backdrop",
                    "backdrop_pixels_within_budget")):
                expected_quality = (
                    trace_values["quality_switches_allow_backdrop"]
                    and trace_values["backdrop_pixels_within_budget"])
                report.check(
                    "material decision quality allowance is derived",
                    trace_values.get("quality_allows_backdrop")
                    == expected_quality,
                    path=f"{plan_path}.decision_trace.quality_allows_backdrop",
                    expected=expected_quality,
                    actual=trace_values.get("quality_allows_backdrop"),
                    likely_layer=likely_layer,
                    hint=(
                        "quality_allows_backdrop should combine the quality "
                        "switches with the backdrop pixel budget."),
                    record_success=False)
            can_sample_inputs = (
                "liquid_glass_backdrop_candidate",
                "target_ready",
                "quality_allows_backdrop",
                "backend_supports_backdrop",
                "capability_frame_history",
                "backdrop_source_ready",
                "reduced_transparency",
            )
            if all(key in trace_values for key in can_sample_inputs):
                expected_can_sample = (
                    trace_values["liquid_glass_backdrop_candidate"]
                    and trace_values["target_ready"]
                    and trace_values["quality_allows_backdrop"]
                    and trace_values["backend_supports_backdrop"]
                    and trace_values["capability_frame_history"]
                    and trace_values["backdrop_source_ready"]
                    and not trace_values["reduced_transparency"])
                report.check(
                    "material decision can-sample predicate is derived",
                    trace_values.get("can_sample_backdrop")
                    == expected_can_sample,
                    path=f"{plan_path}.decision_trace.can_sample_backdrop",
                    expected=expected_can_sample,
                    actual=trace_values.get("can_sample_backdrop"),
                    likely_layer=likely_layer,
                    hint=(
                        "can_sample_backdrop should be the conjunction of "
                        "Liquid Glass role eligibility, target, quality, backend, frame-history, "
                        "backdrop-source, and reduced-transparency gates."),
                    record_success=False)
            next_frame_inputs = (
                "liquid_glass_backdrop_candidate",
                "target_ready",
                "quality_allows_backdrop",
                "backend_supports_backdrop",
                "reduced_transparency",
            )
            if all(key in trace_values for key in next_frame_inputs):
                expected_next_frame_capture = (
                    trace_values["liquid_glass_backdrop_candidate"]
                    and trace_values["target_ready"]
                    and trace_values["quality_allows_backdrop"]
                    and trace_values["backend_supports_backdrop"]
                    and not trace_values["reduced_transparency"])
                report.check(
                    "material decision next-frame capture predicate is derived",
                    trace_values.get("next_frame_capture_required")
                    == expected_next_frame_capture,
                    path=f"{plan_path}.decision_trace.next_frame_capture_required",
                    expected=expected_next_frame_capture,
                    actual=trace_values.get("next_frame_capture_required"),
                    likely_layer=likely_layer,
                    hint=(
                        "next_frame_capture_required should be the pure "
                        "Liquid Glass role, target, quality, backend, and reduced-"
                        "transparency gate for the edge frame-history copy."),
                    record_success=False)

        tint = check_object_field(
            report,
            plan,
            "tint",
            plan_path,
            likely_layer=likely_layer,
            hint="MaterialPlan.tint should be serialized as RGBA components.")
        if tint is not None:
            for key in MATERIAL_TINT_FIELDS:
                check_number_field(
                    report,
                    tint,
                    key,
                    f"{plan_path}.tint",
                    min_value=0.0,
                    likely_layer=likely_layer,
                    hint="Check MaterialPlan tint RGBA serialization.")

        backdrop = check_object_field(
            report,
            plan,
            "backdrop",
            plan_path,
            likely_layer=likely_layer,
            hint=(
                "MaterialPlan.backdrop should copy the pure backdrop "
                "descriptor and luminance response used by the planner."))
        if backdrop is not None:
            backdrop_summary = summary["backdrop"]
            available = None
            stable = None
            for key in MATERIAL_BACKDROP_BOOL_FIELDS:
                value = check_bool_field(
                    report,
                    backdrop,
                    key,
                    f"{plan_path}.backdrop",
                    likely_layer=likely_layer,
                    hint="Backdrop availability flags must stay explicit.")
                if value is True:
                    backdrop_summary[key] = int(backdrop_summary[key]) + 1
                if key == "available":
                    available = value
                elif key == "stable":
                    stable = value
            luma_values: dict[str, int | float] = {}
            for key in MATERIAL_BACKDROP_LUMA_FIELDS:
                value = check_number_field(
                    report,
                    backdrop,
                    key,
                    f"{plan_path}.backdrop",
                    min_value=0.0,
                    likely_layer=likely_layer,
                    hint="Backdrop luminance statistics must be sanitized in the pure plan.")
                if isinstance(value, (int, float)):
                    luma_values[key] = value
                    report.check(
                        f"material backdrop {key} is normalized",
                        float(value) <= 1.0,
                        path=f"{plan_path}.backdrop.{key}",
                        expected={"<=": 1.0},
                        actual=value,
                        likely_layer=likely_layer,
                        hint="Clamp backdrop luminance statistics before planning.",
                        record_success=False)
                    if key == "luma_span":
                        backdrop_summary["max_luma_span"] = max(
                            float(backdrop_summary["max_luma_span"]),
                            float(value))
            luma_min = luma_values.get("luma_min")
            luma_max = luma_values.get("luma_max")
            luma_mean = luma_values.get("luma_mean")
            luma_span = luma_values.get("luma_span")
            if isinstance(luma_min, (int, float)) and isinstance(luma_max, (int, float)):
                report.check(
                    "material backdrop luma min <= max",
                    float(luma_min) <= float(luma_max),
                    path=f"{plan_path}.backdrop.luma_min",
                    expected={"<=": luma_max},
                    actual=luma_min,
                    likely_layer=likely_layer,
                    hint="Sanitize MaterialBackdropDescriptor before planning.",
                    record_success=False)
            if all(isinstance(value, (int, float))
                   for value in (luma_min, luma_max, luma_mean)):
                assert isinstance(luma_min, (int, float))
                assert isinstance(luma_max, (int, float))
                assert isinstance(luma_mean, (int, float))
                report.check(
                    "material backdrop luma mean within range",
                    float(luma_min) <= float(luma_mean) <= float(luma_max),
                    path=f"{plan_path}.backdrop.luma_mean",
                    expected={">=": luma_min, "<=": luma_max},
                    actual=luma_mean,
                    likely_layer=likely_layer,
                    hint="Clamp backdrop mean to the sanitized luminance range.",
                    record_success=False)
            if all(isinstance(value, (int, float))
                   for value in (luma_min, luma_max, luma_span)):
                assert isinstance(luma_min, (int, float))
                assert isinstance(luma_max, (int, float))
                assert isinstance(luma_span, (int, float))
                expected_span = float(luma_max) - float(luma_min)
                report.check(
                    "material backdrop luma span matches min/max",
                    abs(float(luma_span) - expected_span) <= 0.0001,
                    path=f"{plan_path}.backdrop.luma_span",
                    expected=expected_span,
                    actual=luma_span,
                    likely_layer=likely_layer,
                    hint="Keep MaterialBackdropAnalysis.luma_span derived from min/max.",
                    record_success=False)
            max_abs_delta_for_plan = 0.0
            for key in MATERIAL_BACKDROP_DELTA_FIELDS:
                value = check_number_field(
                    report,
                    backdrop,
                    key,
                    f"{plan_path}.backdrop",
                    likely_layer=likely_layer,
                    hint="Backdrop luminance deltas must be explicit numeric values.")
                if isinstance(value, (int, float)):
                    summary_key = {
                        "luminance_floor_delta": "max_abs_floor_delta",
                        "luminance_gain_delta": "max_abs_gain_delta",
                        "edge_highlight_delta": "max_abs_edge_delta",
                    }[key]
                    backdrop_summary[summary_key] = max(
                        float(backdrop_summary[summary_key]),
                        abs(float(value)))
                    max_abs_delta_for_plan = max(
                        max_abs_delta_for_plan,
                        abs(float(value)))
            if max_abs_delta_for_plan > 0.0001:
                backdrop_summary["luminance_adapted"] = int(
                    backdrop_summary["luminance_adapted"]) + 1
            source = check_string_field(
                report,
                backdrop,
                "source",
                f"{plan_path}.backdrop",
                likely_layer=likely_layer,
                hint="Backdrop source should name the edge input used by the pure plan.")
            if isinstance(source, str):
                sources = backdrop_summary["sources"]
                sources[source] = sources.get(source, 0) + 1
            response = check_string_field(
                report,
                backdrop,
                "luminance_response",
                f"{plan_path}.backdrop",
                likely_layer=likely_layer,
                hint="Backdrop luminance response should explain the pure contrast policy.")
            if isinstance(response, str):
                responses = backdrop_summary["luminance_responses"]
                responses[response] = responses.get(response, 0) + 1
                report.check(
                    "material backdrop luminance response is known",
                    response in ALLOWED_MATERIAL_LUMINANCE_RESPONSES,
                    path=f"{plan_path}.backdrop.luminance_response",
                    expected=sorted(ALLOWED_MATERIAL_LUMINANCE_RESPONSES),
                    actual=response,
                    likely_layer=likely_layer,
                    hint="Update the verifier vocabulary with intentional new pure responses.",
                    record_success=False)
            if backdrop_sampling is True:
                report.check(
                    "material sampled backdrop descriptor is ready",
                    available is True and stable is True,
                    path=f"{plan_path}.backdrop",
                    expected={"available": True, "stable": True},
                    actual={"available": available, "stable": stable},
                    likely_layer=likely_layer,
                    hint="Sampled material plans require a stable backdrop descriptor.",
                    record_success=False)
                report.check(
                    "material sampled backdrop excludes foreground text",
                    backdrop.get("excludes_foreground_text") is True,
                    path=f"{plan_path}.backdrop.excludes_foreground_text",
                    expected=True,
                    actual=backdrop.get("excludes_foreground_text"),
                    likely_layer=likely_layer,
                    hint=(
                        "Sampled glass should read an edge-provided backdrop "
                        "capture that excludes foreground text and overlays."),
                    record_success=False)
                report.check(
                    "material sampled backdrop has luminance response",
                    response != "not-sampled",
                    path=f"{plan_path}.backdrop.luminance_response",
                    expected="not not-sampled",
                    actual=response,
                    likely_layer=likely_layer,
                    hint="Run apply_backdrop_luminance_policy for sampled plans.",
                    record_success=False)
            elif backdrop_sampling is False:
                report.check(
                    "material fallback backdrop response is not-sampled",
                    response == "not-sampled",
                    path=f"{plan_path}.backdrop.luminance_response",
                    expected="not-sampled",
                    actual=response,
                    likely_layer=likely_layer,
                    hint="Fallback plans should not report sampled luminance policy.",
                    record_success=False)

        next_frame_capture = None
        capture_reason = None
        backdrop_access = check_object_field(
            report,
            plan,
            "backdrop_access",
            plan_path,
            likely_layer="material-backdrop",
            hint=(
                "MaterialPlan.backdrop_access should describe shared frame "
                "capture and surface sampling bounds without backend policy."))
        if backdrop_access is not None:
            access_summary = summary["backdrop_access"]
            access_required = None
            shared_capture = None
            for key in MATERIAL_BACKDROP_ACCESS_BOOL_FIELDS:
                value = check_bool_field(
                    report,
                    backdrop_access,
                    key,
                    f"{plan_path}.backdrop_access",
                    likely_layer="material-backdrop",
                    hint="Backdrop access booleans must be explicit pure-plan facts.")
                if value is True:
                    access_summary[key] = int(access_summary[key]) + 1
                if key == "required":
                    access_required = value
                elif key == "shared_frame_capture":
                    shared_capture = value
                elif key == "next_frame_capture_required":
                    next_frame_capture = value
            access_numbers: dict[str, int | float] = {}
            for key in MATERIAL_BACKDROP_ACCESS_NUMBER_FIELDS:
                value = check_number_field(
                    report,
                    backdrop_access,
                    key,
                    f"{plan_path}.backdrop_access",
                    min_value=0.0,
                    likely_layer="material-backdrop",
                    hint="Backdrop access resource bounds must be non-negative.")
                if isinstance(value, (int, float)):
                    access_numbers[key] = value
                    if key == "max_frame_capture_count":
                        access_summary["max_frame_capture_count"] = max(
                            int(access_summary["max_frame_capture_count"]),
                            int(value))
                    elif key == "max_frame_capture_pixels":
                        access_summary["max_frame_capture_pixels"] = max(
                            int(access_summary["max_frame_capture_pixels"]),
                            int(value))
                    elif key == "max_surface_sample_pixels":
                        access_summary["total_surface_sample_pixels"] = int(
                            access_summary["total_surface_sample_pixels"]) + int(value)
                        access_summary["max_surface_sample_pixels"] = max(
                            int(access_summary["max_surface_sample_pixels"]),
                            int(value))
            access_source = check_string_field(
                report,
                backdrop_access,
                "source",
                f"{plan_path}.backdrop_access",
                likely_layer="material-backdrop",
                hint="Backdrop access source should name the edge input used by the plan.")
            if isinstance(access_source, str):
                sources = access_summary["sources"]
                sources[access_source] = sources.get(access_source, 0) + 1
            capture_scope = check_string_field(
                report,
                backdrop_access,
                "capture_scope",
                f"{plan_path}.backdrop_access",
                likely_layer="material-backdrop",
                hint="Capture scope should be none or a shared-frame contract.")
            if isinstance(capture_scope, str):
                scopes = access_summary["capture_scopes"]
                scopes[capture_scope] = scopes.get(capture_scope, 0) + 1
                report.check(
                    "material backdrop access capture scope is known",
                    capture_scope in ALLOWED_MATERIAL_BACKDROP_CAPTURE_SCOPES,
                    path=f"{plan_path}.backdrop_access.capture_scope",
                    expected=sorted(ALLOWED_MATERIAL_BACKDROP_CAPTURE_SCOPES),
                    actual=capture_scope,
                    likely_layer="material-backdrop",
                    hint="Update the verifier vocabulary for intentional capture scopes.",
                    record_success=False)
            capture_reason = check_string_field(
                report,
                backdrop_access,
                "capture_reason",
                f"{plan_path}.backdrop_access",
                likely_layer="material-backdrop",
                hint=(
                    "Capture reason should explain whether this plan samples "
                    "the current frame or warms the next frame."))
            if isinstance(capture_reason, str):
                reasons = access_summary["capture_reasons"]
                reasons[capture_reason] = reasons.get(capture_reason, 0) + 1
            if backdrop_sampling is True:
                report.check(
                    "material sampled backdrop has required access",
                    access_required is True
                    and shared_capture is True
                    and next_frame_capture is True
                    and backdrop_access.get("excludes_foreground_text") is True,
                    path=f"{plan_path}.backdrop_access",
                    expected={
                        "required": True,
                        "shared_frame_capture": True,
                        "next_frame_capture_required": True,
                        "excludes_foreground_text": True,
                    },
                    actual={
                        "required": access_required,
                        "shared_frame_capture": shared_capture,
                        "next_frame_capture_required": next_frame_capture,
                        "excludes_foreground_text": backdrop_access.get(
                            "excludes_foreground_text"),
                    },
                    likely_layer="material-backdrop",
                    hint=(
                        "Sampled plans should declare a shared frame capture "
                        "rather than hiding the copy at the backend edge."),
                    record_success=False)
                max_capture_pixels = access_numbers.get("max_frame_capture_pixels")
                if (isinstance(max_capture_pixels, (int, float))
                        and render_target_pixel_count is not None):
                    report.check(
                        "material backdrop access capture pixels match render target",
                        int(max_capture_pixels) == render_target_pixel_count,
                        path=f"{plan_path}.backdrop_access.max_frame_capture_pixels",
                        expected=render_target_pixel_count,
                        actual=max_capture_pixels,
                        likely_layer="material-backdrop",
                        hint="Shared frame capture budget should be derived from render_target.pixel_count.",
                        record_success=False)
            elif backdrop_sampling is False:
                expected_shared_capture = next_frame_capture is True
                report.check(
                    "material fallback backdrop access follows warmup contract",
                    access_required is False
                    and shared_capture is expected_shared_capture
                    and backdrop_access.get("excludes_foreground_text")
                        is expected_shared_capture,
                    path=f"{plan_path}.backdrop_access",
                    expected={
                        "required": False,
                        "shared_frame_capture": expected_shared_capture,
                        "excludes_foreground_text": expected_shared_capture,
                    },
                    actual={
                        "required": access_required,
                        "shared_frame_capture": shared_capture,
                        "excludes_foreground_text": backdrop_access.get(
                            "excludes_foreground_text"),
                    },
                    likely_layer="material-backdrop",
                    hint=(
                        "Fallback plans may reserve one shared frame copy only "
                        "when they are warming a supported backend for the next frame."),
                    record_success=False)

        theme = check_object_field(
            report,
            plan,
            "theme",
            plan_path,
            likely_layer="theme",
            hint=(
                "MaterialPlan.theme should expose the resolved material style "
                "tokens used by foreground, tint, and fallback decisions."))
        if theme is not None:
            theme_summary = summary["theme"]
            for key in MATERIAL_THEME_STRING_FIELDS:
                value = check_string_field(
                    report,
                    theme,
                    key,
                    f"{plan_path}.theme",
                    likely_layer="theme",
                    hint="Theme snapshot strings should be stable pure-plan metadata.")
                if isinstance(value, str):
                    bucket_name = {
                        "source": "sources",
                        "profile_name": "profile_names",
                        "token_policy": "token_policies",
                    }[key]
                    bucket = theme_summary[bucket_name]
                    bucket[value] = bucket.get(value, 0) + 1
            for key in MATERIAL_THEME_BOOL_FIELDS:
                value = check_bool_field(
                    report,
                    theme,
                    key,
                    f"{plan_path}.theme",
                    likely_layer="theme",
                    hint="Theme token booleans should be explicit for artifact debugging.")
                if value is True:
                    theme_summary[key] = int(theme_summary[key]) + 1
            for key in MATERIAL_THEME_COLOR_FIELDS:
                color = check_object_field(
                    report,
                    theme,
                    key,
                    f"{plan_path}.theme",
                    likely_layer="theme",
                    hint="Theme token colors should expose RGBA channels.")
                if color is not None:
                    for channel in MATERIAL_COLOR_FIELDS:
                        check_number_field(
                            report,
                            color,
                            channel,
                            f"{plan_path}.theme.{key}",
                            min_value=0.0,
                            max_value=255.0,
                            likely_layer="theme",
                            hint="Theme token color channels must be byte values.")
            profile_name = theme.get("profile_name")
            default_tokens = theme.get("default_glass_tokens")
            report.check(
                "material theme profile follows token match",
                (profile_name == "apple-glass-light") is (default_tokens is True),
                path=f"{plan_path}.theme.profile_name",
                expected="apple-glass-light when default_glass_tokens is true, custom otherwise",
                actual={
                    "profile_name": profile_name,
                    "default_glass_tokens": default_tokens,
                },
                likely_layer="theme",
                hint=(
                    "MaterialPlan.theme.profile_name should be derived from "
                    "the explicit MaterialStyle token snapshot."),
                record_success=False)

        verifier = check_object_field(
            report,
            plan,
            "verifier",
            plan_path,
            likely_layer=likely_layer,
            hint="The pure plan should provide verifier expectations.")
        if verifier is not None:
            verifier_values: JsonObject = {}
            for key in MATERIAL_VERIFIER_FIELDS:
                if key.startswith("require_"):
                    value = check_bool_field(
                        report,
                        verifier,
                        key,
                        f"{plan_path}.verifier",
                        likely_layer=likely_layer,
                        hint="Verifier expectation booleans must stay explicit.")
                    if isinstance(value, bool):
                        verifier_values[key] = value
                        if value:
                            summary[key.replace("require_", "verifier_require_")] += 1
                elif key in ("region_name", "likely_layer", "likely_pass"):
                    value = check_string_field(
                        report,
                        verifier,
                        key,
                        f"{plan_path}.verifier",
                        likely_layer=likely_layer,
                        hint="Verifier expectations must name the failing region/layer/pass.")
                    if isinstance(value, str):
                        verifier_values[key] = value
                        if key == "likely_layer":
                            report.check(
                                "material verifier likely layer is known",
                                value in ALLOWED_MATERIAL_LIKELY_LAYERS,
                                path=f"{plan_path}.verifier.likely_layer",
                                expected=sorted(ALLOWED_MATERIAL_LIKELY_LAYERS),
                                actual=value,
                                likely_layer=likely_layer,
                                hint=(
                                    "Verifier likely layers should stay aligned "
                                    "with material pass layer names."),
                                record_success=False)
                        if key == "likely_pass":
                            report.check(
                                "material verifier likely pass is known",
                                value in ALLOWED_MATERIAL_PASS_NAMES,
                                path=f"{plan_path}.verifier.likely_pass",
                                expected=sorted(ALLOWED_MATERIAL_PASS_NAMES),
                                actual=value,
                                likely_layer=likely_layer,
                                likely_pass=value,
                                hint=(
                                    "Verifier likely passes should stay aligned "
                                    "with MaterialPlan.primary_pass names."),
                                record_success=False)
                else:
                    value = check_number_field(
                        report,
                        verifier,
                        key,
                        f"{plan_path}.verifier",
                        min_value=0.0,
                        likely_layer=likely_layer,
                        hint="Verifier numeric thresholds must be non-negative.")
                    if isinstance(value, (int, float)):
                        verifier_values[key] = value
            region_name = string_at(verifier, "region_name")
            region_layer = string_at(verifier, "likely_layer")
            region_pass = string_at(verifier, "likely_pass")
            if region_name:
                profiles = summary["verifier_profiles"]
                profiles[region_name] = profiles.get(region_name, 0) + 1
            if region_name and region_layer:
                summary["region_layers"][region_name] = region_layer
            if region_name and region_pass:
                summary["region_passes"][region_name] = region_pass
            if "require_backdrop_source" in verifier_values:
                expected = backdrop_sampling is True
                report.check(
                    "material verifier backdrop-source requirement is derived",
                    verifier_values["require_backdrop_source"] == expected,
                    path=f"{plan_path}.verifier.require_backdrop_source",
                    expected=expected,
                    actual=verifier_values["require_backdrop_source"],
                    likely_layer=likely_layer,
                    hint=(
                        "require_backdrop_source should mirror the pure "
                        "backdrop_sampling decision."),
                    record_success=False)
            if "require_edge_highlight" in verifier_values:
                expected = (
                    isinstance(plan_edge_highlight, (int, float))
                    and float(plan_edge_highlight) > 0.0
                    and fallback is False)
                report.check(
                    "material verifier edge-highlight requirement is derived",
                    verifier_values["require_edge_highlight"] == expected,
                    path=f"{plan_path}.verifier.require_edge_highlight",
                    expected=expected,
                    actual=verifier_values["require_edge_highlight"],
                    likely_layer=likely_layer,
                    hint=(
                        "require_edge_highlight should only be true for "
                        "non-fallback plans with a positive edge highlight."),
                    record_success=False)
            if "require_container_identity" in verifier_values:
                container = material_container_from(plan)
                expected = (
                    isinstance(container, dict)
                    and int(container["container_id"]) > 0)
                report.check(
                    "material verifier container identity requirement is derived",
                    verifier_values["require_container_identity"] == expected,
                    path=f"{plan_path}.verifier.require_container_identity",
                    expected=expected,
                    actual=verifier_values["require_container_identity"],
                    likely_layer="material-container",
                    hint=(
                        "require_container_identity should be true only for "
                        "plans that participate in a material container."),
                    record_success=False)
            if "require_container_morph_contract" in verifier_values:
                container = material_container_from(plan)
                expected = (
                    isinstance(container, dict)
                    and container["morph_transitions"] is True)
                report.check(
                    "material verifier container morph requirement is derived",
                    verifier_values["require_container_morph_contract"] == expected,
                    path=f"{plan_path}.verifier.require_container_morph_contract",
                    expected=expected,
                    actual=verifier_values["require_container_morph_contract"],
                    likely_layer="material-container",
                    hint=(
                        "require_container_morph_contract should mirror the "
                        "pure MaterialPlan.container morph flag."),
                    record_success=False)
            if "min_luma_delta" in verifier_values:
                expected = 8.0 if backdrop_sampling is True else 3.0
                report.check(
                    "material verifier luma threshold is derived",
                    float(verifier_values["min_luma_delta"]) == expected,
                    path=f"{plan_path}.verifier.min_luma_delta",
                    expected=expected,
                    actual=verifier_values["min_luma_delta"],
                    likely_layer=likely_layer,
                    hint=(
                        "min_luma_delta should use the sampled-backdrop "
                        "threshold for glass plans and fallback threshold otherwise."),
                    record_success=False)
            if "min_unique_colors" in verifier_values:
                expected = 4 if backdrop_sampling is True else 2
                report.check(
                    "material verifier unique-color threshold is derived",
                    int(verifier_values["min_unique_colors"]) == expected,
                    path=f"{plan_path}.verifier.min_unique_colors",
                    expected=expected,
                    actual=verifier_values["min_unique_colors"],
                    likely_layer=likely_layer,
                    hint=(
                        "min_unique_colors should use the sampled-backdrop "
                        "threshold for glass plans and fallback threshold otherwise."),
                    record_success=False)
            primary_pass_for_verifier = plan.get("primary_pass")
            if isinstance(primary_pass_for_verifier, dict):
                expected_layer = string_at(
                    primary_pass_for_verifier,
                    "likely_layer")
                if expected_layer and "likely_layer" in verifier_values:
                    report.check(
                        "material verifier likely layer matches primary pass",
                        verifier_values["likely_layer"] == expected_layer,
                        path=f"{plan_path}.verifier.likely_layer",
                        expected=expected_layer,
                        actual=verifier_values["likely_layer"],
                        likely_layer=likely_layer,
                        likely_pass=string_at(primary_pass_for_verifier, "name"),
                        hint=(
                            "Verifier failures should point at the same "
                            "likely layer as MaterialPlan.primary_pass."),
                        record_success=False)
                expected_pass = string_at(primary_pass_for_verifier, "name")
                if expected_pass and "likely_pass" in verifier_values:
                    report.check(
                        "material verifier likely pass matches primary pass",
                        verifier_values["likely_pass"] == expected_pass,
                        path=f"{plan_path}.verifier.likely_pass",
                        expected=expected_pass,
                        actual=verifier_values["likely_pass"],
                        likely_layer=likely_layer,
                        likely_pass=expected_pass,
                        hint=(
                            "Verifier failures should point at the same "
                            "pass name as MaterialPlan.primary_pass."),
                        record_success=False)

        quality_policy = check_object_field(
            report,
            plan,
            "quality_policy",
            plan_path,
            likely_layer=likely_layer,
            hint="Material plans must expose the resolved pure quality policy.")
        if quality_policy is not None:
            for key in MATERIAL_QUALITY_POLICY_BOOL_FIELDS:
                enabled = check_bool_field(
                    report,
                    quality_policy,
                    key,
                    f"{plan_path}.quality_policy",
                    likely_layer=likely_layer,
                    hint="Quality policy booleans must stay explicit.")
                if enabled is False:
                    quality_summary = summary["quality_policy"]
                    if key == "allow_backdrop_sampling":
                        quality_summary["backdrop_sampling_disabled"] = int(
                            quality_summary["backdrop_sampling_disabled"]) + 1
                    elif key == "allow_noise":
                        quality_summary["noise_disabled"] = int(
                            quality_summary["noise_disabled"]) + 1
                    elif key == "allow_shadow":
                        quality_summary["shadow_disabled"] = int(
                            quality_summary["shadow_disabled"]) + 1
            for key in MATERIAL_QUALITY_POLICY_NUMBER_FIELDS:
                policy_limit = check_number_field(
                    report,
                    quality_policy,
                    key,
                    f"{plan_path}.quality_policy",
                    min_value=0.0,
                    likely_layer=likely_layer,
                    hint="Quality policy limits must be resolved to non-negative values.")
                if isinstance(policy_limit, (int, float)):
                    quality_summary = summary["quality_policy"]
                    if key == "max_blur_radius":
                        quality_summary["max_blur_radius"] = max(
                            float(quality_summary["max_blur_radius"]),
                            float(policy_limit))
                    elif key == "max_sample_taps":
                        quality_summary["max_sample_taps"] = max(
                            int(quality_summary["max_sample_taps"]),
                            int(policy_limit))
                    elif key == "max_backdrop_pixels":
                        quality_summary["max_backdrop_pixels"] = max(
                            int(quality_summary["max_backdrop_pixels"]),
                            int(policy_limit))

        resource_budget = check_object_field(
            report,
            plan,
            "resource_budget",
            plan_path,
            likely_layer=likely_layer,
            hint="Material plans must expose bounded resource policy for CI debugging.")
        max_pass_count: int | float | None = None
        max_execution_stages: int | float | None = None
        if resource_budget is not None:
            bounds = summary["resource_bounds"]
            max_blur_radius = check_number_field(
                report,
                resource_budget,
                "max_blur_radius",
                f"{plan_path}.resource_budget",
                min_value=0.0,
                likely_layer=likely_layer,
                hint="Blur radius should be clamped in the pure plan.")
            if isinstance(max_blur_radius, (int, float)):
                bounds["max_budget_blur_radius"] = max(
                    float(bounds["max_budget_blur_radius"]),
                    float(max_blur_radius))
            max_sample_taps = check_number_field(
                report,
                resource_budget,
                "max_sample_taps",
                f"{plan_path}.resource_budget",
                min_value=0.0,
                likely_layer=likely_layer,
                hint="Sample tap budget should be bounded in the pure plan.")
            if isinstance(max_sample_taps, (int, float)):
                bounds["max_sample_taps"] = max(
                    int(bounds["max_sample_taps"]),
                    int(max_sample_taps))
            max_pass_count = check_number_field(
                report,
                resource_budget,
                "max_pass_count",
                f"{plan_path}.resource_budget",
                min_value=0.0,
                likely_layer=likely_layer,
                hint="Material pass count should be bounded in the pure plan.")
            if isinstance(max_pass_count, (int, float)):
                bounds["max_pass_count"] = max(
                    int(bounds["max_pass_count"]),
                    int(max_pass_count))
            max_execution_stages = check_number_field(
                report,
                resource_budget,
                "max_execution_stages",
                f"{plan_path}.resource_budget",
                min_value=0.0,
                likely_layer=likely_layer,
                hint="Material execution stage count should be bounded in the pure plan.")
            if isinstance(max_execution_stages, (int, float)):
                bounds["max_execution_stages"] = max(
                    int(bounds["max_execution_stages"]),
                    int(max_execution_stages))
            max_backdrop_pixels = check_number_field(
                report,
                resource_budget,
                "max_backdrop_pixels",
                f"{plan_path}.resource_budget",
                min_value=0.0,
                likely_layer=likely_layer,
                hint="Backdrop texture copy budget should be bounded by the render target.")
            if isinstance(max_backdrop_pixels, (int, float)):
                bounds["max_backdrop_pixels"] = max(
                    int(bounds["max_backdrop_pixels"]),
                    int(max_backdrop_pixels))
            max_frame_capture_count = check_number_field(
                report,
                resource_budget,
                "max_frame_capture_count",
                f"{plan_path}.resource_budget",
                min_value=0.0,
                likely_layer="material-backdrop",
                hint="Shared frame capture count should be bounded in the pure plan.")
            if isinstance(max_frame_capture_count, (int, float)):
                bounds["max_frame_capture_count"] = max(
                    int(bounds["max_frame_capture_count"]),
                    int(max_frame_capture_count))
            max_frame_capture_pixels = check_number_field(
                report,
                resource_budget,
                "max_frame_capture_pixels",
                f"{plan_path}.resource_budget",
                min_value=0.0,
                likely_layer="material-backdrop",
                hint="Shared frame capture pixel budget should be explicit.")
            if isinstance(max_frame_capture_pixels, (int, float)):
                bounds["max_frame_capture_pixels"] = max(
                    int(bounds["max_frame_capture_pixels"]),
                    int(max_frame_capture_pixels))
            max_surface_sample_pixels = check_number_field(
                report,
                resource_budget,
                "max_surface_sample_pixels",
                f"{plan_path}.resource_budget",
                min_value=0.0,
                likely_layer="material-backdrop",
                hint="Surface sampling pixel budget should be explicit.")
            if isinstance(max_surface_sample_pixels, (int, float)):
                bounds["total_surface_sample_pixels"] = int(
                    bounds["total_surface_sample_pixels"]) + int(
                        max_surface_sample_pixels)
                bounds["max_surface_sample_pixels"] = max(
                    int(bounds["max_surface_sample_pixels"]),
                    int(max_surface_sample_pixels))
            max_container_spacing = check_number_field(
                report,
                resource_budget,
                "max_container_spacing",
                f"{plan_path}.resource_budget",
                min_value=0.0,
                likely_layer="material-container",
                hint="Container spacing should be surfaced as a resource/debug bound.")
            if isinstance(max_container_spacing, (int, float)):
                bounds["max_container_spacing"] = max(
                    float(bounds["max_container_spacing"]),
                    float(max_container_spacing))
            bounded_texture_copy = check_bool_field(
                report,
                resource_budget,
                "bounded_texture_copy",
                f"{plan_path}.resource_budget",
                likely_layer=likely_layer,
                hint="The backend contract should state whether texture copies are bounded.")
            if bounded_texture_copy is False:
                bounds["unbounded_texture_copy"] = int(
                    bounds["unbounded_texture_copy"]) + 1
            deterministic_fallback = check_bool_field(
                report,
                resource_budget,
                "deterministic_fallback",
                f"{plan_path}.resource_budget",
                likely_layer=likely_layer,
                hint="Fallback behavior must be deterministic for LLM debugging.")
            if deterministic_fallback is False:
                bounds["non_deterministic_fallback"] = int(
                    bounds["non_deterministic_fallback"]) + 1
            if isinstance(plan_blur_radius, (int, float)) and isinstance(max_blur_radius, (int, float)):
                report.check(
                    "material blur radius is within budget",
                    float(plan_blur_radius) <= float(max_blur_radius),
                    path=f"{plan_path}.blur_radius",
                    expected={"<=": float(max_blur_radius)},
                    actual=float(plan_blur_radius),
                    likely_layer=likely_layer,
                    hint="Clamp blur radius in plan_material_surface.",
                    record_success=False)
            if isinstance(sample_taps, (int, float)) and isinstance(max_sample_taps, (int, float)):
                report.check(
                    "material sample taps are within budget",
                    int(sample_taps) <= int(max_sample_taps),
                    path=f"{plan_path}.sample_taps",
                    expected={"<=": int(max_sample_taps)},
                    actual=int(sample_taps),
                    likely_layer=likely_layer,
                    hint="Clamp sample taps in plan_material_surface.",
                    record_success=False)

        execution_stage_capacity = check_number_field(
            report,
            plan,
            "execution_stage_capacity",
            plan_path,
            min_value=0.0,
            likely_layer="material-stage",
            likely_pass="stage-capacity",
            hint=(
                "MaterialPlan must expose the fixed execution stage array "
                "capacity so new stage work cannot disappear silently."))
        if isinstance(execution_stage_capacity, (int, float)):
            bounds = summary["resource_bounds"]
            capacity = int(execution_stage_capacity)
            bounds["max_execution_stage_capacity"] = max(
                int(bounds["max_execution_stage_capacity"]),
                capacity)
            if isinstance(max_execution_stages, (int, float)):
                report.check(
                    "material execution stage capacity matches resource budget",
                    capacity == int(max_execution_stages),
                    path=f"{plan_path}.execution_stage_capacity",
                    expected=int(max_execution_stages),
                    actual=capacity,
                    likely_layer="material-stage",
                    likely_pass="stage-capacity",
                    hint=(
                        "Keep MaterialPlan.execution_stage_capacity and "
                        "MaterialResourceBudget.max_execution_stages aligned."),
                    record_success=False)

        dropped_execution_stage_count = check_number_field(
            report,
            plan,
            "dropped_execution_stage_count",
            plan_path,
            min_value=0.0,
            likely_layer="material-stage",
            likely_pass="stage-capacity",
            hint=(
                "Dropped execution stages mean a pure MaterialPlan exceeded "
                "its fixed stage capacity before backend execution."))
        if isinstance(dropped_execution_stage_count, (int, float)):
            dropped = int(dropped_execution_stage_count)
            bounds = summary["resource_bounds"]
            bounds["dropped_execution_stages"] = (
                int(bounds["dropped_execution_stages"]) + dropped)
            report.check(
                "material execution stages did not overflow capacity",
                dropped == 0,
                path=f"{plan_path}.dropped_execution_stage_count",
                expected=0,
                actual=dropped,
                likely_layer="material-stage",
                likely_pass="stage-capacity",
                hint=(
                    "Increase material_max_execution_stages and the artifact "
                    "contract, or remove duplicate stage work in "
                    "plan_material_surface."),
                record_success=False)

        primary_pass = check_object_field(
            report,
            plan,
            "primary_pass",
            plan_path,
            likely_layer=likely_layer,
            hint="The pure plan should name the backend pass it expects.")
        primary_pass_sample_taps: int | float | None = None
        primary_pass_texture_copy_pixels: int | float | None = None
        primary_pass_name = ""
        if primary_pass is not None:
            primary_pass_name = string_at(primary_pass, "name") or ""
            for key in MATERIAL_PASS_FIELDS:
                if key in ("active", "requires_backdrop"):
                    check_bool_field(
                        report,
                        primary_pass,
                        key,
                        f"{plan_path}.primary_pass",
                        likely_layer=likely_layer,
                        likely_pass=primary_pass_name,
                        hint="Material pass booleans must be explicit.")
                elif key == "sample_taps":
                    primary_pass_sample_taps = check_number_field(
                        report,
                        primary_pass,
                        key,
                        f"{plan_path}.primary_pass",
                        min_value=0.0,
                        likely_layer=likely_layer,
                        likely_pass=primary_pass_name,
                        hint="Pass sample taps must be bounded.")
                elif key == "max_texture_copy_pixels":
                    primary_pass_texture_copy_pixels = check_number_field(
                        report,
                        primary_pass,
                        key,
                        f"{plan_path}.primary_pass",
                        min_value=0.0,
                        likely_layer=likely_layer,
                        likely_pass=primary_pass_name,
                        hint="Pass texture-copy bounds must be explicit.")
                else:
                    value = check_string_field(
                        report,
                        primary_pass,
                        key,
                        f"{plan_path}.primary_pass",
                        likely_layer=likely_layer,
                        hint="Material pass must name its layer for debugging.")
                    if key == "executor" and isinstance(value, str):
                        report.check(
                            "material primary pass executor is known",
                            value in ALLOWED_MATERIAL_PASS_EXECUTORS,
                            path=f"{plan_path}.primary_pass.executor",
                            expected=sorted(ALLOWED_MATERIAL_PASS_EXECUTORS),
                            actual=value,
                            likely_layer=likely_layer,
                            likely_pass=primary_pass_name,
                            hint="Add new pure executor roles to the verifier contract when intentional.",
                            record_success=False)
            if backdrop_sampling is False and next_frame_capture is True:
                report.check(
                    "material capture warmup fallback names its reason",
                    capture_reason == "warmup-next-frame",
                    path=f"{plan_path}.backdrop_access.capture_reason",
                    expected="warmup-next-frame",
                    actual=capture_reason,
                    likely_layer="material-backdrop",
                    hint=(
                        "Fallback plans may request a frame-history copy only "
                        "when warming a supported backend for the next frame."),
                    record_success=False)
            pass_name = primary_pass_name
            if pass_name:
                pass_names = summary["pass_names"]
                pass_names[pass_name] = pass_names.get(pass_name, 0) + 1
                report.check(
                    "material primary pass name is known",
                    pass_name in ALLOWED_MATERIAL_PASS_NAMES,
                    path=f"{plan_path}.primary_pass.name",
                    expected=sorted(ALLOWED_MATERIAL_PASS_NAMES),
                    actual=pass_name,
                    likely_layer=likely_layer,
                    likely_pass=pass_name,
                    hint="Add new backend pass names to the verifier contract when they are intentional.",
                    record_success=False)
            if isinstance(sample_taps, (int, float)) and isinstance(
                    primary_pass_sample_taps, (int, float)):
                report.check(
                    "material primary pass sample taps match plan",
                    int(primary_pass_sample_taps) == int(sample_taps),
                    path=f"{plan_path}.primary_pass.sample_taps",
                    expected=int(sample_taps),
                    actual=int(primary_pass_sample_taps),
                    likely_layer=likely_layer,
                    likely_pass=primary_pass_name,
                    hint="Keep MaterialPlan.sample_taps and primary_pass.sample_taps in sync.",
                    record_success=False)
            if isinstance(primary_pass_texture_copy_pixels, (int, float)):
                bounds = summary["resource_bounds"]
                bounds["max_pass_texture_copy_pixels"] = max(
                    int(bounds["max_pass_texture_copy_pixels"]),
                    int(primary_pass_texture_copy_pixels))
                if primary_pass.get("active") is False:
                    report.check(
                        "material inactive primary pass has no texture copy",
                        int(primary_pass_texture_copy_pixels) == 0,
                        path=f"{plan_path}.primary_pass.max_texture_copy_pixels",
                        expected=0,
                        actual=int(primary_pass_texture_copy_pixels),
                        likely_layer=likely_layer,
                        likely_pass=primary_pass_name,
                        hint="Inactive material passes must not reserve texture-copy work.",
                        record_success=False)
                if primary_pass.get("requires_backdrop") is False:
                    report.check(
                        "material non-backdrop primary pass has no texture copy",
                        int(primary_pass_texture_copy_pixels) == 0,
                        path=f"{plan_path}.primary_pass.max_texture_copy_pixels",
                        expected=0,
                        actual=int(primary_pass_texture_copy_pixels),
                        likely_layer=likely_layer,
                        likely_pass=primary_pass_name,
                        hint="Fallback/fill passes should not copy backdrop textures.",
                        record_success=False)
                elif (primary_pass.get("requires_backdrop") is True
                      and render_target_pixel_count is not None):
                    report.check(
                        "material backdrop primary pass texture copy is within render target",
                        0 < int(primary_pass_texture_copy_pixels)
                        <= render_target_pixel_count,
                        path=f"{plan_path}.primary_pass.max_texture_copy_pixels",
                        expected={">": 0, "<=": render_target_pixel_count},
                        actual=int(primary_pass_texture_copy_pixels),
                        likely_layer=likely_layer,
                        likely_pass=primary_pass_name,
                        hint=(
                            "Backdrop pass texture-copy bounds should be derived "
                            "from MaterialPlan.render_target.pixel_count."),
                        record_success=False)

        passes = plan.get("passes")
        report.check(
            "material plan passes is array",
            isinstance(passes, list),
            path=f"{plan_path}.passes",
            expected="array",
            actual=type(passes).__name__,
            likely_layer=likely_layer,
            hint="Backend runtime JSON should mirror MaterialPlan pass expectations.",
            record_success=False)
        if isinstance(passes, list):
            bounds = summary["resource_bounds"]
            bounds["total_runtime_passes"] = int(
                bounds["total_runtime_passes"]) + len(passes)
            if isinstance(max_pass_count, (int, float)):
                report.check(
                    "material pass count is within budget",
                    len(passes) <= int(max_pass_count),
                    path=f"{plan_path}.passes",
                    expected={"length<=": int(max_pass_count)},
                    actual=len(passes),
                    likely_layer=likely_layer,
                    hint="Keep the runtime pass list within MaterialResourceBudget.max_pass_count.",
                    record_success=False)
            for pass_index, pass_entry in enumerate(passes):
                pass_path = f"{plan_path}.passes[{pass_index}]"
                report.check(
                    "material pass entry is object",
                    isinstance(pass_entry, dict),
                    path=pass_path,
                    expected="object",
                    actual=type(pass_entry).__name__,
                    likely_layer=likely_layer,
                    hint="Pass entries should be structured objects.",
                    record_success=False)
                if not isinstance(pass_entry, dict):
                    continue
                bounds = summary["resource_bounds"]
                if pass_entry.get("active") is True:
                    bounds["active_runtime_passes"] = int(
                        bounds["active_runtime_passes"]) + 1
                if pass_entry.get("requires_backdrop") is True:
                    bounds["backdrop_runtime_passes"] = int(
                        bounds["backdrop_runtime_passes"]) + 1
                pass_name_for_hint = string_at(pass_entry, "name") or primary_pass_name
                pass_requires_backdrop = pass_entry.get("requires_backdrop")
                pass_active = pass_entry.get("active")
                pass_texture_copy_pixels: int | None = None
                for key in MATERIAL_PASS_FIELDS:
                    if key in ("active", "requires_backdrop"):
                        check_bool_field(
                            report,
                            pass_entry,
                            key,
                            pass_path,
                            likely_layer=likely_layer,
                            likely_pass=pass_name_for_hint,
                            hint="Pass entries must expose runtime activation state.")
                    elif key == "sample_taps":
                        check_number_field(
                            report,
                            pass_entry,
                            key,
                            pass_path,
                            min_value=0.0,
                            likely_layer=likely_layer,
                            likely_pass=pass_name_for_hint,
                            hint="Pass sample taps must be numeric.")
                    elif key == "max_texture_copy_pixels":
                        value = check_number_field(
                            report,
                            pass_entry,
                            key,
                            pass_path,
                            min_value=0.0,
                            likely_layer=likely_layer,
                            likely_pass=pass_name_for_hint,
                            hint="Pass texture-copy bounds must be numeric.")
                        if isinstance(value, (int, float)):
                            pass_texture_copy_pixels = int(value)
                            bounds["max_pass_texture_copy_pixels"] = max(
                                int(bounds["max_pass_texture_copy_pixels"]),
                                int(value))
                            bounds["total_pass_texture_copy_pixels"] = (
                                int(bounds["total_pass_texture_copy_pixels"])
                                + int(value))
                    else:
                        pass_value = check_string_field(
                            report,
                            pass_entry,
                            key,
                            pass_path,
                            likely_layer=likely_layer,
                            hint="Pass entries must name their layer for debugging.")
                        if key == "name" and isinstance(pass_value, str):
                            report.check(
                                "material pass name is known",
                                pass_value in ALLOWED_MATERIAL_PASS_NAMES,
                                path=f"{pass_path}.name",
                                expected=sorted(ALLOWED_MATERIAL_PASS_NAMES),
                                actual=pass_value,
                                likely_layer=likely_layer,
                                likely_pass=pass_value,
                                hint="Add new backend pass names to the verifier contract when they are intentional.",
                                record_success=False)
                        if key == "executor" and isinstance(pass_value, str):
                            pass_executors = summary["pass_executors"]
                            pass_executors[pass_value] = (
                                pass_executors.get(pass_value, 0) + 1)
                            report.check(
                                "material pass executor is known",
                                pass_value in ALLOWED_MATERIAL_PASS_EXECUTORS,
                                path=f"{pass_path}.executor",
                                expected=sorted(ALLOWED_MATERIAL_PASS_EXECUTORS),
                                actual=pass_value,
                                likely_layer=likely_layer,
                                likely_pass=pass_name_for_hint,
                                hint="Add new pure executor roles to the verifier contract when intentional.",
                                record_success=False)
                if pass_texture_copy_pixels is not None:
                    if pass_active is False:
                        report.check(
                            "material inactive runtime pass has no texture copy",
                            pass_texture_copy_pixels == 0,
                            path=f"{pass_path}.max_texture_copy_pixels",
                            expected=0,
                            actual=pass_texture_copy_pixels,
                            likely_layer=likely_layer,
                            likely_pass=pass_name_for_hint,
                            hint="Inactive material passes must not reserve texture-copy work.",
                            record_success=False)
                    if pass_requires_backdrop is False:
                        report.check(
                            "material non-backdrop runtime pass has no texture copy",
                            pass_texture_copy_pixels == 0,
                            path=f"{pass_path}.max_texture_copy_pixels",
                            expected=0,
                            actual=pass_texture_copy_pixels,
                            likely_layer=likely_layer,
                            likely_pass=pass_name_for_hint,
                            hint="Fallback/fill passes should not copy backdrop textures.",
                            record_success=False)
                    elif (pass_requires_backdrop is True
                          and render_target_pixel_count is not None):
                        report.check(
                            "material backdrop runtime pass texture copy is within render target",
                            0 < pass_texture_copy_pixels <= render_target_pixel_count,
                            path=f"{pass_path}.max_texture_copy_pixels",
                            expected={">": 0, "<=": render_target_pixel_count},
                            actual=pass_texture_copy_pixels,
                            likely_layer=likely_layer,
                            likely_pass=pass_name_for_hint,
                            hint=(
                                "Runtime backdrop pass texture-copy bounds "
                                "should be derived from MaterialPlan.render_target.pixel_count."),
                            record_success=False)
            if isinstance(primary_pass, dict):
                report.check(
                    "material pass list includes primary pass",
                    any(
                        isinstance(entry, dict)
                        and all(
                            entry.get(key) == primary_pass.get(key)
                            for key in MATERIAL_PASS_FIELDS)
                        for entry in passes
                    ),
                    path=f"{plan_path}.passes",
                    expected=primary_pass,
                    actual=passes,
                    likely_layer=likely_layer,
                    likely_pass=primary_pass_name,
                    hint="Runtime pass details should include the primary pass unchanged.",
                    record_success=False)

        execution_stages = plan.get("execution_stages")
        report.check(
            "material execution stages is array",
            isinstance(execution_stages, list),
            path=f"{plan_path}.execution_stages",
            expected="array",
            actual=type(execution_stages).__name__,
            likely_layer=likely_layer,
            hint=(
                "MaterialPlan.execution_stages should explain shadow, blur, "
                "edge, noise, or fallback work without backend policy guessing."),
            record_success=False)
        if isinstance(execution_stages, list):
            bounds = summary["resource_bounds"]
            bounds["total_execution_stages"] = int(
                bounds["total_execution_stages"]) + len(execution_stages)
            bounds["max_execution_stage_count"] = max(
                int(bounds["max_execution_stage_count"]),
                len(execution_stages))
            if isinstance(max_execution_stages, (int, float)):
                report.check(
                    "material execution stage count is within budget",
                    len(execution_stages) <= int(max_execution_stages),
                    path=f"{plan_path}.execution_stages",
                    expected={"length<=": int(max_execution_stages)},
                    actual=len(execution_stages),
                    likely_layer=likely_layer,
                    hint=(
                        "Keep execution stages within "
                        "MaterialResourceBudget.max_execution_stages."),
                    record_success=False)
            if isinstance(execution_stage_capacity, (int, float)):
                report.check(
                    "material execution stage count is within serialized capacity",
                    len(execution_stages) <= int(execution_stage_capacity),
                    path=f"{plan_path}.execution_stages",
                    expected={"length<=": int(execution_stage_capacity)},
                    actual=len(execution_stages),
                    likely_layer="material-stage",
                    likely_pass="stage-capacity",
                    hint=(
                        "The serialized stage list must fit within "
                        "MaterialPlan.execution_stage_capacity."),
                    record_success=False)
            has_primary_stage = False
            has_backdrop_stage = False
            for stage_index, stage_entry in enumerate(execution_stages):
                stage_path = f"{plan_path}.execution_stages[{stage_index}]"
                report.check(
                    "material execution stage entry is object",
                    isinstance(stage_entry, dict),
                    path=stage_path,
                    expected="object",
                    actual=type(stage_entry).__name__,
                    likely_layer=likely_layer,
                    hint="Execution stage entries should be structured objects.",
                    record_success=False)
                if not isinstance(stage_entry, dict):
                    continue
                stage_name_for_hint = string_at(stage_entry, "name") or primary_pass_name
                stage_requires_backdrop = stage_entry.get("requires_backdrop")
                stage_active = stage_entry.get("active")
                stage_texture_copy_pixels: int | None = None
                if stage_active is True:
                    bounds["active_execution_stages"] = int(
                        bounds["active_execution_stages"]) + 1
                if stage_requires_backdrop is True:
                    has_backdrop_stage = True
                    bounds["backdrop_execution_stages"] = int(
                        bounds["backdrop_execution_stages"]) + 1
                for key in MATERIAL_EXECUTION_STAGE_FIELDS:
                    if key in ("active", "requires_backdrop", "bounded"):
                        value = check_bool_field(
                            report,
                            stage_entry,
                            key,
                            stage_path,
                            likely_layer=likely_layer,
                            likely_pass=stage_name_for_hint,
                            hint="Execution stage booleans must be explicit.")
                        if key == "bounded" and value is False:
                            report.check(
                                "material execution stage is bounded",
                                False,
                                path=f"{stage_path}.bounded",
                                expected=True,
                                actual=False,
                                likely_layer=likely_layer,
                                likely_pass=stage_name_for_hint,
                                hint="Material stages on hot paths must expose bounded work.",
                                record_success=False)
                    elif key == "sample_taps":
                        check_number_field(
                            report,
                            stage_entry,
                            key,
                            stage_path,
                            min_value=0.0,
                            likely_layer=likely_layer,
                            likely_pass=stage_name_for_hint,
                            hint="Execution stage sample taps must be numeric.")
                    elif key == "max_texture_copy_pixels":
                        value = check_number_field(
                            report,
                            stage_entry,
                            key,
                            stage_path,
                            min_value=0.0,
                            likely_layer=likely_layer,
                            likely_pass=stage_name_for_hint,
                            hint="Execution stage texture-copy bounds must be numeric.")
                        if isinstance(value, (int, float)):
                            stage_texture_copy_pixels = int(value)
                    else:
                        stage_value = check_string_field(
                            report,
                            stage_entry,
                            key,
                            stage_path,
                            likely_layer=likely_layer,
                            hint="Execution stages must name their layer for debugging.")
                        if key == "name" and isinstance(stage_value, str):
                            stage_names = summary["stage_names"]
                            stage_names[stage_value] = (
                                stage_names.get(stage_value, 0) + 1)
                            report.check(
                                "material execution stage name is known",
                                stage_value in ALLOWED_MATERIAL_STAGE_NAMES,
                                path=f"{stage_path}.name",
                                expected=sorted(ALLOWED_MATERIAL_STAGE_NAMES),
                                actual=stage_value,
                                likely_layer=likely_layer,
                                likely_pass=stage_value,
                                hint="Add intentional material stages to the verifier contract.",
                                record_success=False)
                        if key == "likely_layer" and isinstance(stage_value, str):
                            report.check(
                                "material execution stage likely layer is known",
                                stage_value in ALLOWED_MATERIAL_LIKELY_LAYERS,
                                path=f"{stage_path}.likely_layer",
                                expected=sorted(ALLOWED_MATERIAL_LIKELY_LAYERS),
                                actual=stage_value,
                                likely_layer=stage_value,
                                likely_pass=stage_name_for_hint,
                                hint="Stage likely_layer should identify a debuggable backend layer.",
                                record_success=False)
                        if key == "executor" and isinstance(stage_value, str):
                            stage_executors = summary["stage_executors"]
                            stage_executors[stage_value] = (
                                stage_executors.get(stage_value, 0) + 1)
                            report.check(
                                "material execution stage executor is known",
                                stage_value in ALLOWED_MATERIAL_STAGE_EXECUTORS,
                                path=f"{stage_path}.executor",
                                expected=sorted(ALLOWED_MATERIAL_STAGE_EXECUTORS),
                                actual=stage_value,
                                likely_layer=likely_layer,
                                likely_pass=stage_name_for_hint,
                                hint="Add intentional material stage executors to the verifier contract.",
                                record_success=False)
                if stage_texture_copy_pixels is not None:
                    if stage_active is False:
                        report.check(
                            "material inactive execution stage has no texture copy",
                            stage_texture_copy_pixels == 0,
                            path=f"{stage_path}.max_texture_copy_pixels",
                            expected=0,
                            actual=stage_texture_copy_pixels,
                            likely_layer=likely_layer,
                            likely_pass=stage_name_for_hint,
                            hint="Inactive execution stages must not reserve texture-copy work.",
                            record_success=False)
                    if stage_requires_backdrop is False:
                        report.check(
                            "material non-backdrop execution stage has no texture copy",
                            stage_texture_copy_pixels == 0,
                            path=f"{stage_path}.max_texture_copy_pixels",
                            expected=0,
                            actual=stage_texture_copy_pixels,
                            likely_layer=likely_layer,
                            likely_pass=stage_name_for_hint,
                            hint="Only backdrop stages should reserve texture-copy work.",
                            record_success=False)
                    elif render_target_pixel_count is not None:
                        report.check(
                            "material backdrop execution stage texture copy is within render target",
                            0 < stage_texture_copy_pixels <= render_target_pixel_count,
                            path=f"{stage_path}.max_texture_copy_pixels",
                            expected={">": 0, "<=": render_target_pixel_count},
                            actual=stage_texture_copy_pixels,
                            likely_layer=likely_layer,
                            likely_pass=stage_name_for_hint,
                            hint=(
                                "Backdrop stage texture-copy bounds should be "
                                "derived from MaterialPlan.render_target.pixel_count."),
                            record_success=False)
                if isinstance(primary_pass, dict) and primary_pass.get("active") is True:
                    has_primary_stage = has_primary_stage or all(
                        stage_entry.get(key) == primary_pass.get(key)
                        for key in MATERIAL_PASS_FIELDS)
            if isinstance(primary_pass, dict) and primary_pass.get("active") is True:
                report.check(
                    "material execution stages include primary pass",
                    has_primary_stage,
                    path=f"{plan_path}.execution_stages",
                    expected=primary_pass,
                    actual=execution_stages,
                    likely_layer=likely_layer,
                    likely_pass=primary_pass_name,
                    hint="Execution stages should include the primary render pass unchanged.",
                    record_success=False)
            if backdrop_sampling is True:
                report.check(
                    "material backdrop sampling has backdrop execution stage",
                    has_backdrop_stage,
                    path=f"{plan_path}.execution_stages",
                    expected={"requires_backdrop": True},
                    actual=execution_stages,
                    likely_layer=likely_layer,
                    likely_pass=primary_pass_name,
                    hint="Backdrop sampling plans must include a backdrop execution stage.",
                    record_success=False)
            elif execution_stages:
                report.check(
                    "material fallback execution stages avoid backdrop work",
                    not has_backdrop_stage,
                    path=f"{plan_path}.execution_stages",
                    expected={"requires_backdrop": False},
                    actual=execution_stages,
                    likely_layer=likely_layer,
                    likely_pass=primary_pass_name,
                    hint="Fallback material stages must degrade without backdrop texture work.",
                    record_success=False)

        if fallback is True:
            report.check(
                "material fallback path is not none",
                fallback_path != "none",
                path=f"{plan_path}.fallback_path",
                expected="not none",
                actual=fallback_path,
                likely_layer=likely_layer,
                hint="Fallback plans should identify the exact deterministic fallback path.",
                record_success=False)
            report.check(
                "material fallback has reason",
                isinstance(fallback_reason, str) and bool(fallback_reason),
                path=f"{plan_path}.fallback_reason",
                expected="non-empty string",
                actual=fallback_reason,
                likely_layer=likely_layer,
                hint="Fallback plans should explain the exact missing capability or policy.",
                record_success=False)
        elif fallback is False:
            report.check(
                "material non-fallback path is none",
                fallback_path == "none",
                path=f"{plan_path}.fallback_path",
                expected="none",
                actual=fallback_path,
                likely_layer=likely_layer,
                hint="Non-fallback plans should not carry stale fallback metadata.",
                record_success=False)
            report.check(
                "material non-fallback reason is empty",
                fallback_reason == "",
                path=f"{plan_path}.fallback_reason",
                expected="empty string",
                actual=fallback_reason,
                likely_layer=likely_layer,
                hint="Non-fallback plans should not carry stale fallback metadata.",
                record_success=False)
        if plan.get("fallback") is True:
            summary["fallback"] += 1
        if plan.get("backdrop_sampling") is True:
            summary["backdrop_sampling"] += 1
            report.check(
                "material backdrop plan is not fallback",
                fallback is False,
                path=f"{plan_path}.fallback",
                expected=False,
                actual=fallback,
                likely_layer=likely_layer,
                hint="Backdrop sampling and fallback should be mutually exclusive.",
                record_success=False)
            report.check(
                "material backdrop primary pass requires backdrop",
                isinstance(primary_pass, dict)
                and primary_pass.get("requires_backdrop") is True,
                path=f"{plan_path}.primary_pass.requires_backdrop",
                expected=True,
                actual=None if not isinstance(primary_pass, dict)
                else primary_pass.get("requires_backdrop"),
                likely_layer=likely_layer,
                likely_pass=primary_pass_name,
                hint="Backdrop plans should expose the backdrop dependency.",
                record_success=False)
            report.check(
                "material backdrop plan has active pass",
                any(
                    isinstance(entry, dict)
                    and entry.get("active") is True
                    and "backdrop" in str(entry.get("name", ""))
                    for entry in plan.get("passes", [])
                ),
                path=f"{plan_path}.passes",
                expected="active backdrop pass",
                actual=plan.get("passes"),
                likely_layer=likely_layer,
                likely_pass=primary_pass_name,
                hint="The backend likely planned glass but did not record the blur pass.",
                record_success=False)
        check_material_observation_contract(
            report,
            plan,
            plan_path,
            likely_layer)
    return summary


def check_material_plan_summary_requirements(
        summary: JsonObject,
        spec: JsonObject,
        report: Report) -> None:
    base_path = "debug.platform_runtime.details.renderer.material_plans#summary"
    if "count" in spec:
        report.check(
            "material plan summary count matches",
            summary.get("count") == spec["count"],
            path=f"{base_path}.count",
            expected=spec["count"],
            actual=summary.get("count"),
            likely_layer="platform-runtime",
            hint="Check whether the scene emitted the expected number of MaterialRect commands.")
    if "min_count" in spec:
        actual = summary.get("count")
        report.check(
            "material plan summary count meets minimum",
            isinstance(actual, int) and actual >= spec["min_count"],
            path=f"{base_path}.count",
            expected={">=": spec["min_count"]},
            actual=actual,
            likely_layer="platform-runtime",
            hint="Check whether MaterialRect commands reached the backend decoder.")
    for field in (
            "fallback",
            "backdrop_sampling",
            "backdrop_available",
            "backdrop_stable",
            "backdrop_excludes_foreground_text",
            "backdrop_access_required",
            "backdrop_access_stable_required",
            "backdrop_access_frame_history_required",
            "backdrop_access_shared_frame_capture",
            "backdrop_access_next_frame_capture_required",
            "backdrop_access_excludes_foreground_text",
            "backdrop_access_bounded",
            "luminance_adapted",
            "render_target_ready",
            "render_target_within_backdrop_budget",
            "decision_can_sample_backdrop",
            "decision_backend_supports_backdrop",
            "decision_backdrop_source_ready",
            "decision_next_frame_capture_required",
            "decision_reduced_transparency",
            "decision_increase_contrast",
            "decision_reduce_motion",
            "container_participating",
            "container_unioned",
            "container_interactive",
            "container_morph_transitions",
            "reference_view_bounds_anchored",
            "reference_shape_matches_geometry",
            "reference_tint_applied",
            "reference_interactive_response",
            "reference_container_grouped",
            "reference_container_union",
            "reference_container_morphing",
            "reference_legibility_preserved",
            "reference_vibrancy_expected",
            "reference_deterministic_degradation",
            "shape_valid",
            "shape_rounded",
            "shape_radius_clamped",
            "foreground_backdrop_driven",
            "foreground_high_contrast",
            "foreground_vibrant",
            "foreground_deterministic",
            "verifier_require_backdrop_source",
            "verifier_require_container_identity",
            "verifier_require_container_morph_contract",
            "verifier_require_edge_highlight"):
        if field in spec:
            actual = summary.get(field)
            summary_path = f"{base_path}.{field}"
            if field in (
                    "backdrop_available",
                    "backdrop_stable",
                    "backdrop_excludes_foreground_text"):
                backdrop_summary = summary.get("backdrop")
                if not isinstance(backdrop_summary, dict):
                    backdrop_summary = {}
                nested_field = field.removeprefix("backdrop_")
                actual = backdrop_summary.get(nested_field)
                summary_path = f"{base_path}.backdrop.{nested_field}"
            elif field.startswith("backdrop_access_"):
                access_summary = summary.get("backdrop_access")
                if not isinstance(access_summary, dict):
                    access_summary = {}
                nested_field = field.removeprefix("backdrop_access_")
                actual = access_summary.get(nested_field)
                summary_path = f"{base_path}.backdrop_access.{nested_field}"
            elif field == "luminance_adapted":
                backdrop_summary = summary.get("backdrop")
                if not isinstance(backdrop_summary, dict):
                    backdrop_summary = {}
                actual = backdrop_summary.get("luminance_adapted")
                summary_path = f"{base_path}.backdrop.luminance_adapted"
            elif field in (
                    "render_target_ready",
                    "render_target_within_backdrop_budget"):
                render_summary = summary.get("render_target")
                if not isinstance(render_summary, dict):
                    render_summary = {}
                nested_field = field.removeprefix("render_target_")
                actual = render_summary.get(nested_field)
                summary_path = f"{base_path}.render_target.{nested_field}"
            elif field in (
                    "decision_role_allows_liquid_glass",
                    "decision_content_layer_standard_material",
                    "decision_liquid_glass_backdrop_candidate",
                    "decision_can_sample_backdrop",
                    "decision_backend_supports_backdrop",
                    "decision_backdrop_source_ready",
                    "decision_next_frame_capture_required",
                    "decision_reduced_transparency",
                    "decision_increase_contrast",
                    "decision_reduce_motion"):
                decision_summary = summary.get("decision_trace")
                if not isinstance(decision_summary, dict):
                    decision_summary = {}
                nested_field = field.removeprefix("decision_")
                actual = decision_summary.get(nested_field)
                summary_path = f"{base_path}.decision_trace.{nested_field}"
            elif field in (
                    "verifier_require_backdrop_source",
                    "verifier_require_container_identity",
                    "verifier_require_container_morph_contract",
                    "verifier_require_edge_highlight"):
                summary_path = f"{base_path}.{field}"
            elif field in (
                    "container_participating",
                    "container_unioned",
                    "container_interactive",
                    "container_morph_transitions"):
                container_summary = summary.get("container")
                if not isinstance(container_summary, dict):
                    container_summary = {}
                nested_field = field.removeprefix("container_")
                if nested_field == "unioned":
                    nested_field = "unioned"
                actual = container_summary.get(nested_field)
                summary_path = f"{base_path}.container.{nested_field}"
            elif field.startswith("reference_") and field not in (
                    "reference_technologies",
                    "reference_layers",
                    "reference_material_policies",
                    "reference_variants",
                    "reference_shapes",
                    "reference_shape_scopes",
                    "reference_blending_scopes",
                    "reference_semantic_thickness"):
                reference_summary = summary.get("reference_model")
                if not isinstance(reference_summary, dict):
                    reference_summary = {}
                nested_field = field.removeprefix("reference_")
                actual = reference_summary.get(nested_field)
                summary_path = f"{base_path}.reference_model.{nested_field}"
            elif field in (
                    "shape_valid",
                    "shape_rounded",
                    "shape_capsule",
                    "shape_radius_clamped"):
                shape_summary = summary.get("shape")
                if not isinstance(shape_summary, dict):
                    shape_summary = {}
                nested_field = field.removeprefix("shape_")
                actual = shape_summary.get(nested_field)
                summary_path = f"{base_path}.shape.{nested_field}"
            elif field in (
                    "foreground_backdrop_driven",
                    "foreground_high_contrast",
                    "foreground_vibrant",
                    "foreground_deterministic"):
                foreground_summary = summary.get("foreground")
                if not isinstance(foreground_summary, dict):
                    foreground_summary = {}
                nested_field = field.removeprefix("foreground_")
                if nested_field == "vibrant":
                    nested_field = "vibrant"
                actual = foreground_summary.get(nested_field)
                summary_path = f"{base_path}.foreground.{nested_field}"
            elif field.startswith("theme_"):
                theme_summary = summary.get("theme")
                if not isinstance(theme_summary, dict):
                    theme_summary = {}
                nested_field = field.removeprefix("theme_")
                actual = theme_summary.get(nested_field)
                summary_path = f"{base_path}.theme.{nested_field}"
            report.check(
                f"material plan summary {field} matches",
                actual == spec[field],
                path=summary_path,
                expected=spec[field],
                actual=actual,
                likely_layer="platform-runtime",
                hint="Inspect fallback_path and primary_pass for each material plan.")
    foreground_bounds = {
        "foreground_min_primary_contrast_gte": "min_primary_contrast",
        "foreground_minimum_contrast_gte": "min_minimum_contrast",
    }
    for field, nested_field in foreground_bounds.items():
        if field not in spec:
            continue
        foreground_summary = summary.get("foreground")
        if not isinstance(foreground_summary, dict):
            foreground_summary = {}
        actual = foreground_summary.get(nested_field)
        expected = spec[field]
        report.check(
            f"material plan summary {field} bound",
            isinstance(actual, (int, float))
            and not isinstance(actual, bool)
            and float(actual) >= float(expected),
            path=f"{base_path}.foreground.{nested_field}",
            expected={">=": expected},
            actual=actual,
            likely_layer="material-foreground",
            hint="Inspect MaterialPlan.foreground contrast selection.")
    shape_bounds = {
        "shape_max_surface_area": "max_surface_area",
        "shape_max_effective_radius": "max_effective_radius",
        "shape_max_radius_limit": "max_radius_limit",
        "shape_max_normalized_radius": "max_normalized_radius",
    }
    for prefix, nested_field in shape_bounds.items():
        for suffix, comparator in (("_lte", "<="), ("_gte", ">=")):
            field = f"{prefix}{suffix}"
            if field not in spec:
                continue
            shape_summary = summary.get("shape")
            if not isinstance(shape_summary, dict):
                shape_summary = {}
            actual = shape_summary.get(nested_field)
            expected = spec[field]
            condition = (
                isinstance(actual, (int, float))
                and not isinstance(actual, bool)
                and (actual <= expected if comparator == "<=" else actual >= expected))
            report.check(
                f"material plan summary {field} bound",
                condition,
                path=f"{base_path}.shape.{nested_field}",
                expected={comparator: expected},
                actual=actual,
                likely_layer="material-shape",
                hint=(
                    "Inspect MaterialPlan.shape and caller material surface "
                    "geometry before changing pixel-region thresholds."))
    summary_field_hints = {
        "fallback_paths": (
            "material-plan",
            "Inspect MaterialPlan.fallback_path and fallback classification."),
        "fallback_reasons": (
            "material-plan",
            "Inspect MaterialPlan.fallback_reason and the pure fallback decision."),
        "contract_versions": (
            "material-plan",
            "Inspect MaterialPlan.contract_version and the artifact verifier schema."),
        "kinds": (
            "material-contract",
            "Inspect MaterialKind serialization and MaterialRect command emission."),
        "roles": (
            "material-contract",
            "Inspect MaterialSurfaceRole serialization and MaterialRect command emission."),
        "container_modes": (
            "material-container",
            "Inspect MaterialPlan.container.mode and layout::material_container usage."),
        "container_ids": (
            "material-container",
            "Inspect MaterialPlan.container.container_id and material container inheritance."),
        "union_ids": (
            "material-container",
            "Inspect MaterialPlan.container.union_id and unioned glass surfaces."),
        "reference_technologies": (
            "material-reference",
            "Inspect MaterialPlan.reference_model.technology and the pure planner schema."),
        "reference_layers": (
            "material-reference",
            "Inspect MaterialPlan.reference_model.layer and the role policy."),
        "reference_material_policies": (
            "material-reference",
            "Inspect MaterialPlan.reference_model.material_policy and the role policy."),
        "reference_variants": (
            "material-reference",
            "Inspect MaterialPlan.reference_model.variant and MaterialPlan.kind."),
        "reference_shapes": (
            "material-reference",
            "Inspect MaterialPlan.reference_model.shape and MaterialPlan.shape."),
        "reference_shape_scopes": (
            "material-reference",
            "Inspect MaterialPlan.reference_model.shape_scope."),
        "reference_blending_scopes": (
            "material-reference",
            "Inspect MaterialPlan.reference_model.blending_scope and fallback/backdrop policy."),
        "reference_semantic_thickness": (
            "material-reference",
            "Inspect MaterialPlan.reference_model.semantic_thickness and MaterialPlan.kind."),
        "reference_accessibility_responses": (
            "material-reference",
            "Inspect MaterialPlan.reference_model.accessibility_response and decision_trace accessibility flags."),
        "reference_performance_responses": (
            "material-reference",
            "Inspect MaterialPlan.reference_model.performance_response, fallback, warmup capture, and resource budget."),
        "pass_names": (
            "material-pass",
            "Inspect MaterialPlan.primary_pass and runtime pass serialization."),
        "pass_executors": (
            "material-pass",
            "Inspect MaterialPlan.primary_pass.executor and runtime pass roles."),
        "stage_names": (
            "material-stage",
            "Inspect MaterialPlan.execution_stages[].name and pure stage planning."),
        "stage_executors": (
            "material-stage",
            "Inspect MaterialPlan.execution_stages[].executor and backend stage roles."),
        "sampling_kernels": (
            "sampling-kernel",
            "Inspect MaterialPlan.sampling_kernel.name and the backend blur shader contract."),
        "sampling_weight_profiles": (
            "sampling-kernel",
            "Inspect MaterialPlan.sampling_kernel.weight_profile and blur tap weighting."),
        "luminance_curves": (
            "luminance-curve",
            "Inspect MaterialPlan.luminance_curve and backend shader curve inputs."),
        "backdrop_sources": (
            "material-backdrop",
            "Inspect MaterialPlan.backdrop.source and the backend backdrop descriptor."),
        "backdrop_access_sources": (
            "material-backdrop",
            "Inspect MaterialPlan.backdrop_access.source and shared capture planning."),
        "backdrop_capture_scopes": (
            "material-backdrop",
            "Inspect MaterialPlan.backdrop_access.capture_scope and backend frame-history policy."),
        "backdrop_capture_reasons": (
            "material-backdrop",
            "Inspect MaterialPlan.backdrop_access.capture_reason and frame-history warmup policy."),
        "luminance_responses": (
            "material-backdrop",
            "Inspect MaterialPlan.backdrop.luminance_response and pure contrast policy."),
        "render_target_pixel_formats": (
            "material-render-target",
            "Inspect MaterialPlan.render_target.pixel_format and backend target metadata."),
        "decision_blockers": (
            "material-decision",
            "Inspect MaterialPlan.decision_trace.first_blocker and fallback_path."),
        "verifier_profiles": (
            "material-verifier",
            "Inspect semantic material verifier_profile and MaterialPlan.verifier.region_name."),
        "verifier_region_layers": (
            "material-verifier",
            "Inspect MaterialPlan.verifier.likely_layer and primary_pass.likely_layer."),
        "verifier_region_passes": (
            "material-verifier",
            "Inspect MaterialPlan.verifier.likely_pass and primary_pass.name."),
        "foreground_schemes": (
            "material-foreground",
            "Inspect MaterialPlan.foreground.scheme and pure contrast policy."),
        "foreground_sources": (
            "material-foreground",
            "Inspect MaterialPlan.foreground.source and planner inputs."),
        "theme_profile_names": (
            "theme",
            "Inspect MaterialPlan.theme.profile_name and the explicit MaterialStyle token snapshot."),
        "theme_sources": (
            "theme",
            "Inspect MaterialPlan.theme.source and the material request construction path."),
        "theme_token_policies": (
            "theme",
            "Inspect MaterialPlan.theme.token_policy and material style token resolution."),
    }
    for field in (
            "fallback_paths",
            "fallback_reasons",
            "contract_versions",
            "kinds",
            "roles",
            "container_modes",
            "container_ids",
            "union_ids",
            "reference_technologies",
            "reference_variants",
            "reference_shapes",
            "reference_shape_scopes",
            "reference_blending_scopes",
            "reference_semantic_thickness",
            "reference_accessibility_responses",
            "reference_performance_responses",
            "pass_names",
            "pass_executors",
            "sampling_kernels",
            "sampling_weight_profiles",
            "luminance_curves",
            "backdrop_sources",
            "backdrop_access_sources",
            "backdrop_capture_scopes",
            "backdrop_capture_reasons",
            "luminance_responses",
            "render_target_pixel_formats",
            "decision_blockers",
            "verifier_profiles",
            "verifier_region_layers",
            "verifier_region_passes",
            "foreground_schemes",
            "foreground_sources",
            "theme_profile_names",
            "theme_sources",
            "theme_token_policies"):
        if field in spec:
            actual = summary.get(field)
            summary_path = f"{base_path}.{field}"
            if field in ("container_modes", "container_ids", "union_ids"):
                container_summary = summary.get("container")
                if not isinstance(container_summary, dict):
                    container_summary = {}
                nested = {
                    "container_modes": "modes",
                    "container_ids": "container_ids",
                    "union_ids": "union_ids",
                }[field]
                actual = container_summary.get(nested)
                summary_path = f"{base_path}.container.{nested}"
            elif field.startswith("reference_"):
                reference_summary = summary.get("reference_model")
                if not isinstance(reference_summary, dict):
                    reference_summary = {}
                nested = {
                    "reference_technologies": "technologies",
                    "reference_variants": "variants",
                    "reference_shapes": "shapes",
                    "reference_shape_scopes": "shape_scopes",
                    "reference_blending_scopes": "blending_scopes",
                    "reference_semantic_thickness": "semantic_thickness",
                    "reference_accessibility_responses": "accessibility_responses",
                    "reference_performance_responses": "performance_responses",
                }[field]
                actual = reference_summary.get(nested)
                summary_path = f"{base_path}.reference_model.{nested}"
            elif field == "backdrop_sources":
                backdrop_summary = summary.get("backdrop")
                if not isinstance(backdrop_summary, dict):
                    backdrop_summary = {}
                actual = backdrop_summary.get("sources")
                summary_path = f"{base_path}.backdrop.sources"
            elif field in (
                    "backdrop_access_sources",
                    "backdrop_capture_scopes",
                    "backdrop_capture_reasons"):
                access_summary = summary.get("backdrop_access")
                if not isinstance(access_summary, dict):
                    access_summary = {}
                nested = {
                    "backdrop_access_sources": "sources",
                    "backdrop_capture_scopes": "capture_scopes",
                    "backdrop_capture_reasons": "capture_reasons",
                }[field]
                actual = access_summary.get(nested)
                summary_path = f"{base_path}.backdrop_access.{nested}"
            elif field == "luminance_responses":
                backdrop_summary = summary.get("backdrop")
                if not isinstance(backdrop_summary, dict):
                    backdrop_summary = {}
                actual = backdrop_summary.get("luminance_responses")
                summary_path = f"{base_path}.backdrop.luminance_responses"
            elif field == "render_target_pixel_formats":
                render_summary = summary.get("render_target")
                if not isinstance(render_summary, dict):
                    render_summary = {}
                actual = render_summary.get("pixel_formats")
                summary_path = f"{base_path}.render_target.pixel_formats"
            elif field == "decision_blockers":
                decision_summary = summary.get("decision_trace")
                if not isinstance(decision_summary, dict):
                    decision_summary = {}
                actual = decision_summary.get("first_blockers")
                summary_path = f"{base_path}.decision_trace.first_blockers"
            elif field == "verifier_region_layers":
                actual = summary.get("region_layers")
                summary_path = f"{base_path}.region_layers"
            elif field == "verifier_region_passes":
                actual = summary.get("region_passes")
                summary_path = f"{base_path}.region_passes"
            elif field in ("foreground_schemes", "foreground_sources"):
                foreground_summary = summary.get("foreground")
                if not isinstance(foreground_summary, dict):
                    foreground_summary = {}
                nested = {
                    "foreground_schemes": "schemes",
                    "foreground_sources": "sources",
                }[field]
                actual = foreground_summary.get(nested)
                summary_path = f"{base_path}.foreground.{nested}"
            elif field in (
                    "theme_profile_names",
                    "theme_sources",
                    "theme_token_policies"):
                theme_summary = summary.get("theme")
                if not isinstance(theme_summary, dict):
                    theme_summary = {}
                nested = {
                    "theme_profile_names": "profile_names",
                    "theme_sources": "sources",
                    "theme_token_policies": "token_policies",
                }[field]
                actual = theme_summary.get(nested)
                summary_path = f"{base_path}.theme.{nested}"
            likely_layer, hint = summary_field_hints[field]
            report.check(
                f"material plan summary {field} matches",
                actual == spec[field],
                path=summary_path,
                expected=spec[field],
                actual=actual,
                likely_layer=likely_layer,
                hint=hint)


def check_material_resource_bounds_requirements(
        summary: JsonObject,
        spec: JsonObject,
        report: Report) -> None:
    bounds = summary.get("resource_bounds")
    if not isinstance(bounds, dict):
        bounds = {}
    base_path = "debug.platform_runtime.details.renderer.material_plans#resource_bounds"
    field_map = {
        "max_plan_blur_radius_lte": "max_plan_blur_radius",
        "max_plan_sample_taps_lte": "max_plan_sample_taps",
        "total_plan_sample_taps_lte": "total_plan_sample_taps",
        "max_budget_blur_radius_lte": "max_budget_blur_radius",
        "max_sample_taps_lte": "max_sample_taps",
        "max_sampling_kernel_radius_lte": "max_sampling_kernel_radius",
        "max_pass_count_lte": "max_pass_count",
        "max_backdrop_pixels_lte": "max_backdrop_pixels",
        "max_frame_capture_count_lte": "max_frame_capture_count",
        "max_frame_capture_pixels_lte": "max_frame_capture_pixels",
        "total_surface_sample_pixels_lte": "total_surface_sample_pixels",
        "max_surface_sample_pixels_lte": "max_surface_sample_pixels",
        "max_container_spacing_lte": "max_container_spacing",
        "max_pass_texture_copy_pixels_lte": "max_pass_texture_copy_pixels",
        "total_pass_texture_copy_pixels_lte": "total_pass_texture_copy_pixels",
        "total_runtime_passes_lte": "total_runtime_passes",
        "active_runtime_passes_lte": "active_runtime_passes",
        "backdrop_runtime_passes_lte": "backdrop_runtime_passes",
        "total_execution_stages_lte": "total_execution_stages",
        "active_execution_stages_lte": "active_execution_stages",
        "backdrop_execution_stages_lte": "backdrop_execution_stages",
        "dropped_execution_stages_lte": "dropped_execution_stages",
        "max_execution_stage_count_lte": "max_execution_stage_count",
        "max_execution_stage_capacity_lte": "max_execution_stage_capacity",
        "max_execution_stages_lte": "max_execution_stages",
    }
    for spec_field, summary_field in field_map.items():
        if spec_field not in spec:
            continue
        actual = bounds.get(summary_field)
        expected = spec[spec_field]
        report.check(
            f"material resource bound {summary_field} is within limit",
            isinstance(actual, (int, float)) and not isinstance(actual, bool)
            and float(actual) <= float(expected),
            path=f"{base_path}.{summary_field}",
            expected={"<=": expected},
            actual=actual,
            likely_layer="platform-runtime",
            hint="Inspect MaterialResourceBudget in the resolved material plans.")
    min_field_map = {
        "max_plan_sample_taps_gte": "max_plan_sample_taps",
        "total_plan_sample_taps_gte": "total_plan_sample_taps",
        "max_sampling_kernel_radius_gte": "max_sampling_kernel_radius",
        "max_frame_capture_count_gte": "max_frame_capture_count",
        "max_frame_capture_pixels_gte": "max_frame_capture_pixels",
        "total_surface_sample_pixels_gte": "total_surface_sample_pixels",
        "max_surface_sample_pixels_gte": "max_surface_sample_pixels",
        "max_pass_texture_copy_pixels_gte": "max_pass_texture_copy_pixels",
        "max_container_spacing_gte": "max_container_spacing",
        "total_pass_texture_copy_pixels_gte": "total_pass_texture_copy_pixels",
        "total_runtime_passes_gte": "total_runtime_passes",
        "active_runtime_passes_gte": "active_runtime_passes",
        "backdrop_runtime_passes_gte": "backdrop_runtime_passes",
        "total_execution_stages_gte": "total_execution_stages",
        "active_execution_stages_gte": "active_execution_stages",
        "backdrop_execution_stages_gte": "backdrop_execution_stages",
        "dropped_execution_stages_gte": "dropped_execution_stages",
        "max_execution_stage_count_gte": "max_execution_stage_count",
        "max_execution_stage_capacity_gte": "max_execution_stage_capacity",
        "max_execution_stages_gte": "max_execution_stages",
    }
    for spec_field, summary_field in min_field_map.items():
        if spec_field not in spec:
            continue
        actual = bounds.get(summary_field)
        expected = spec[spec_field]
        report.check(
            f"material resource bound {summary_field} meets floor",
            isinstance(actual, (int, float)) and not isinstance(actual, bool)
            and float(actual) >= float(expected),
            path=f"{base_path}.{summary_field}",
            expected={">=": expected},
            actual=actual,
            likely_layer="platform-runtime",
            hint=(
                "Inspect MaterialPlan.sample_taps and "
                "renderer.material_plans[].passes in the resolved plans."))
    if spec.get("require_bounded_texture_copy") is True:
        actual = bounds.get("unbounded_texture_copy")
        report.check(
            "material resource bound requires bounded texture copies",
            actual == 0,
            path=f"{base_path}.unbounded_texture_copy",
            expected=0,
            actual=actual,
            likely_layer="platform-runtime",
            hint="MaterialResourceBudget.bounded_texture_copy must stay true for every plan.")
    if spec.get("require_deterministic_fallback") is True:
        actual = bounds.get("non_deterministic_fallback")
        report.check(
            "material resource bound requires deterministic fallback",
            actual == 0,
            path=f"{base_path}.non_deterministic_fallback",
            expected=0,
            actual=actual,
            likely_layer="platform-runtime",
            hint="MaterialResourceBudget.deterministic_fallback must stay true for every plan.")


def check_material_runtime_summary_contract(
        summary: JsonObject,
        runtime_summary: Any,
        report: Report) -> None:
    base_path = "debug.platform_runtime.details.renderer.material_runtime_summary"
    report.check(
        "material runtime summary is object",
        isinstance(runtime_summary, dict),
        path=base_path,
        expected="object",
        actual=type(runtime_summary).__name__,
        likely_layer="platform-runtime",
        hint=(
            "Backends should serialize material_runtime_summary next to "
            "renderer.material_plans so CI can compare executor counters."))
    if not isinstance(runtime_summary, dict):
        return

    bounds = summary.get("resource_bounds")
    if not isinstance(bounds, dict):
        bounds = {}
    expected_fields = {
        "plan_count": summary.get("count"),
        "fallback_count": summary.get("fallback"),
        "backdrop_sampling_count": summary.get("backdrop_sampling"),
        "total_runtime_passes": bounds.get("total_runtime_passes"),
        "active_runtime_passes": bounds.get("active_runtime_passes"),
        "backdrop_runtime_passes": bounds.get("backdrop_runtime_passes"),
        "total_execution_stages": bounds.get("total_execution_stages"),
        "active_execution_stages": bounds.get("active_execution_stages"),
        "backdrop_execution_stages": bounds.get("backdrop_execution_stages"),
        "dropped_execution_stages": bounds.get("dropped_execution_stages"),
        "max_execution_stage_count": bounds.get("max_execution_stage_count"),
        "max_execution_stage_capacity": bounds.get(
            "max_execution_stage_capacity"),
        "max_pass_texture_copy_pixels": bounds.get(
            "max_pass_texture_copy_pixels"),
        "total_pass_texture_copy_pixels": bounds.get(
            "total_pass_texture_copy_pixels"),
        "backdrop_access_count": summary.get("backdrop_access", {}).get("required")
            if isinstance(summary.get("backdrop_access"), dict) else None,
        "shared_frame_capture_plan_count": summary.get(
            "backdrop_access", {}).get("shared_frame_capture")
            if isinstance(summary.get("backdrop_access"), dict) else None,
        "max_frame_capture_count": bounds.get("max_frame_capture_count"),
        "max_frame_capture_pixels": bounds.get("max_frame_capture_pixels"),
        "total_surface_sample_pixels": bounds.get("total_surface_sample_pixels"),
        "max_surface_sample_pixels": bounds.get("max_surface_sample_pixels"),
        "max_plan_blur_radius": bounds.get("max_plan_blur_radius"),
        "max_plan_sample_taps": bounds.get("max_plan_sample_taps"),
        "total_plan_sample_taps": bounds.get("total_plan_sample_taps"),
        "max_budget_blur_radius": bounds.get("max_budget_blur_radius"),
        "max_sample_taps": bounds.get("max_sample_taps"),
        "max_sampling_kernel_radius": bounds.get(
            "max_sampling_kernel_radius"),
        "max_pass_count": bounds.get("max_pass_count"),
        "max_execution_stages": bounds.get("max_execution_stages"),
        "max_backdrop_pixels": bounds.get("max_backdrop_pixels"),
        "max_container_spacing": bounds.get("max_container_spacing"),
        "containered_count": summary.get("container", {}).get("participating")
            if isinstance(summary.get("container"), dict) else None,
        "unioned_count": summary.get("container", {}).get("unioned")
            if isinstance(summary.get("container"), dict) else None,
        "interactive_count": summary.get("container", {}).get("interactive")
            if isinstance(summary.get("container"), dict) else None,
        "morph_transition_count": summary.get("container", {}).get(
            "morph_transitions")
            if isinstance(summary.get("container"), dict) else None,
        "valid_shape_count": summary.get("shape", {}).get("valid")
            if isinstance(summary.get("shape"), dict) else None,
        "rounded_shape_count": summary.get("shape", {}).get("rounded")
            if isinstance(summary.get("shape"), dict) else None,
        "capsule_shape_count": summary.get("shape", {}).get("capsule")
            if isinstance(summary.get("shape"), dict) else None,
        "radius_clamped_count": summary.get("shape", {}).get(
            "radius_clamped")
            if isinstance(summary.get("shape"), dict) else None,
        "foreground_backdrop_driven_count": summary.get(
            "foreground", {}).get("backdrop_driven")
            if isinstance(summary.get("foreground"), dict) else None,
        "foreground_high_contrast_count": summary.get(
            "foreground", {}).get("high_contrast")
            if isinstance(summary.get("foreground"), dict) else None,
        "foreground_vibrant_count": summary.get(
            "foreground", {}).get("vibrant")
            if isinstance(summary.get("foreground"), dict) else None,
        "max_surface_area": summary.get("shape", {}).get("max_surface_area")
            if isinstance(summary.get("shape"), dict) else None,
        "max_effective_radius": summary.get("shape", {}).get(
            "max_effective_radius")
            if isinstance(summary.get("shape"), dict) else None,
        "max_radius_limit": summary.get("shape", {}).get("max_radius_limit")
            if isinstance(summary.get("shape"), dict) else None,
        "max_normalized_radius": summary.get("shape", {}).get(
            "max_normalized_radius")
            if isinstance(summary.get("shape"), dict) else None,
        "min_foreground_contrast_ratio": summary.get(
            "foreground", {}).get("min_primary_contrast")
            if isinstance(summary.get("foreground"), dict) else None,
        "unbounded_texture_copy": bounds.get("unbounded_texture_copy"),
        "non_deterministic_fallback": bounds.get("non_deterministic_fallback"),
    }
    for field, expected in expected_fields.items():
        actual = runtime_summary.get(field)
        report.check(
            f"material runtime summary {field} matches plans",
            actual == expected,
            path=f"{base_path}.{field}",
            expected=expected,
            actual=actual,
            likely_layer="platform-runtime",
            hint=(
                "Compare renderer.material_runtime_summary with the derived "
                "summary from renderer.material_plans[]."))


def check_material_executor_summary_contract(
        summary: JsonObject,
        executor_summary: Any,
        report: Report) -> None:
    base_path = "debug.platform_runtime.details.renderer.material_executor_summary"
    report.check(
        "material executor summary is object",
        isinstance(executor_summary, dict),
        path=base_path,
        expected="object",
        actual=type(executor_summary).__name__,
        likely_layer="platform-runtime",
        likely_pass="material-executor",
        hint=(
            "Backends should serialize material_executor_summary next to "
            "renderer.material_plans so CI can cross-check edge execution."))
    if not isinstance(executor_summary, dict):
        return

    count = summary.get("count")
    fallback = summary.get("fallback")
    material_instances = (
        count - fallback
        if isinstance(count, int) and isinstance(fallback, int)
        else None)
    expected_fields = {
        "plan_count": count,
        "fallback_instance_count": fallback,
        "material_instance_count": material_instances,
    }
    bounds = summary.get("resource_bounds")
    if not isinstance(bounds, dict):
        bounds = {}
    stage_names = summary.get("stage_names")
    if not isinstance(stage_names, dict):
        stage_names = {}
    stage_executors = summary.get("stage_executors")
    if not isinstance(stage_executors, dict):
        stage_executors = {}
    expected_stage_fields = {
        "execution_stage_count": bounds.get("total_execution_stages"),
        "active_execution_stage_count": bounds.get("active_execution_stages"),
        "backdrop_execution_stage_count": bounds.get(
            "backdrop_execution_stages"),
        "primary_execution_stage_count": bounds.get("active_runtime_passes"),
        "dropped_execution_stage_count": bounds.get(
            "dropped_execution_stages"),
        "backdrop_filter_stage_count": stage_executors.get(
            "backdrop-filter", 0),
        "fallback_fill_stage_count": stage_executors.get("fallback-fill", 0),
        "shadow_stage_count": stage_names.get("shape-shadow", 0),
        "edge_highlight_stage_count": stage_names.get("edge-highlight", 0),
        "noise_dither_stage_count": stage_names.get("noise-dither", 0),
    }
    expected_fields.update(expected_stage_fields)
    access_summary = summary.get("backdrop_access")
    if not isinstance(access_summary, dict):
        access_summary = {}
    expected_access_fields = {
        "backdrop_access_plan_count": access_summary.get("required"),
        "next_frame_capture_plan_count": access_summary.get(
            "next_frame_capture_required"),
        "planned_frame_capture_count": bounds.get("max_frame_capture_count"),
        "planned_frame_capture_pixels": bounds.get("max_frame_capture_pixels"),
        "planned_surface_sample_pixels": bounds.get(
            "total_surface_sample_pixels"),
    }
    expected_fields.update(expected_access_fields)
    for field, expected in expected_fields.items():
        actual = executor_summary.get(field)
        report.check(
            f"material executor summary {field} matches plans",
            actual == expected,
            path=f"{base_path}.{field}",
            expected=expected,
            actual=actual,
            likely_layer="platform-runtime",
            likely_pass="material-executor",
            hint=(
                "Compare renderer.material_executor_summary with "
                "renderer.material_plans#summary before changing backend policy."))

    numeric_fields = (
        "plan_count",
        "material_instance_count",
        "fallback_instance_count",
        "material_draw_calls",
        "backdrop_copy_count",
        "execution_stage_count",
        "active_execution_stage_count",
        "backdrop_execution_stage_count",
        "primary_execution_stage_count",
        "dropped_execution_stage_count",
        "backdrop_filter_stage_count",
        "fallback_fill_stage_count",
        "shadow_stage_count",
        "edge_highlight_stage_count",
        "noise_dither_stage_count",
        "backdrop_access_plan_count",
        "next_frame_capture_plan_count",
        "planned_frame_capture_count",
        "planned_frame_capture_pixels",
        "planned_surface_sample_pixels",
        "material_max_sample_taps",
        "material_total_sample_taps",
        "backdrop_copy_pixels",
        "material_upload_bytes",
        "material_buffer_capacity_bytes",
        "material_buffer_reallocations",
        "foreground_text_candidate_count",
        "foreground_text_remap_count",
        "cpu_decode_ns",
        "cpu_material_upload_ns",
        "cpu_total_ns",
    )
    for field in numeric_fields:
        actual = executor_summary.get(field)
        report.check(
            f"material executor summary {field} is non-negative",
            isinstance(actual, (int, float)) and not isinstance(actual, bool)
            and float(actual) >= 0.0,
            path=f"{base_path}.{field}",
            expected={">=": 0},
            actual=actual,
            likely_layer="platform-runtime",
            likely_pass="material-executor",
            hint="Executor telemetry must be numeric and non-negative.")

    bool_fields = (
        "backdrop_copy_excludes_foreground_text",
        "foreground_pass_after_backdrop_copy",
    )
    for field in bool_fields:
        actual = executor_summary.get(field)
        report.check(
            f"material executor summary {field} is boolean",
            isinstance(actual, bool),
            path=f"{base_path}.{field}",
            expected="boolean",
            actual=actual,
            likely_layer="platform-runtime",
            likely_pass="material-executor",
            hint="Executor foreground/capture ordering telemetry must be explicit.")

    foreground_candidates = executor_summary.get("foreground_text_candidate_count")
    foreground_remaps = executor_summary.get("foreground_text_remap_count")
    if (isinstance(foreground_candidates, (int, float))
            and not isinstance(foreground_candidates, bool)
            and isinstance(foreground_remaps, (int, float))
            and not isinstance(foreground_remaps, bool)):
        report.check(
            "material executor foreground remaps stay within candidates",
            float(foreground_remaps) <= float(foreground_candidates),
            path=f"{base_path}.foreground_text_remap_count",
            expected={"<=": foreground_candidates},
            actual=foreground_remaps,
            likely_layer="material-foreground",
            likely_pass="material-executor",
            hint=(
                "Foreground text remaps can only happen for text commands "
                "inside a prior material surface."))

    draw_calls = executor_summary.get("material_draw_calls")
    if isinstance(draw_calls, (int, float)) and not isinstance(draw_calls, bool):
        bounds = summary.get("resource_bounds")
        max_pass_count = None
        if isinstance(bounds, dict):
            max_pass_count = bounds.get("max_pass_count")
        draw_call_limit = (
            material_instances * max_pass_count
            if isinstance(material_instances, int)
            and isinstance(max_pass_count, int)
            else material_instances)
        report.check(
            "material executor draw calls are bounded by pass budget",
            isinstance(draw_call_limit, int)
            and float(draw_calls) <= float(draw_call_limit),
            path=f"{base_path}.material_draw_calls",
            expected={"<=": draw_call_limit},
            actual=draw_calls,
            likely_layer="platform-runtime",
            likely_pass="material-executor",
            hint=(
                "Material draw calls should stay within material instances "
                "times MaterialResourceBudget.max_pass_count."))

    expected_sample_fields = {
        "material_max_sample_taps": bounds.get("max_plan_sample_taps"),
        "material_total_sample_taps": bounds.get("total_plan_sample_taps"),
    }
    for field, expected in expected_sample_fields.items():
        actual = executor_summary.get(field)
        report.check(
            f"material executor summary {field} matches plans",
            actual == expected,
            path=f"{base_path}.{field}",
            expected=expected,
            actual=actual,
            likely_layer="platform-runtime",
            likely_pass="material-executor",
            hint=(
                "The backend must encode the resolved MaterialPlan.sample_taps "
                "into material instances instead of using a hidden shader default."))

    upload_bytes = executor_summary.get("material_upload_bytes")
    upload_capacity = executor_summary.get("material_buffer_capacity_bytes")
    if (isinstance(upload_bytes, (int, float))
            and not isinstance(upload_bytes, bool)
            and isinstance(upload_capacity, (int, float))
            and not isinstance(upload_capacity, bool)):
        report.check(
            "material executor upload bytes fit buffer capacity",
            float(upload_bytes) <= float(upload_capacity),
            path=f"{base_path}.material_upload_bytes",
            expected={"<=": upload_capacity},
            actual=upload_bytes,
            likely_layer="platform-runtime",
            likely_pass="material-executor",
            hint=(
                "The backend should grow the material instance buffer before "
                "recording uploads."))

    max_backdrop_pixels = bounds.get("max_backdrop_pixels")
    copied_pixels = executor_summary.get("backdrop_copy_pixels")
    if (isinstance(copied_pixels, (int, float))
            and not isinstance(copied_pixels, bool)
            and isinstance(max_backdrop_pixels, (int, float))
            and not isinstance(max_backdrop_pixels, bool)):
        report.check(
            "material executor backdrop copy stays within plan budget",
            float(copied_pixels) <= float(max_backdrop_pixels),
            path=f"{base_path}.backdrop_copy_pixels",
            expected={"<=": max_backdrop_pixels},
            actual=copied_pixels,
            likely_layer="platform-runtime",
            likely_pass="material-executor",
            hint=(
                "Compare the backend framebuffer-history copy with "
                "MaterialResourceBudget.max_backdrop_pixels."))

    planned_capture_count = executor_summary.get("planned_frame_capture_count")
    copied_count = executor_summary.get("backdrop_copy_count")
    if (isinstance(planned_capture_count, (int, float))
            and not isinstance(planned_capture_count, bool)
            and isinstance(copied_count, (int, float))
            and not isinstance(copied_count, bool)
            and planned_capture_count > 0):
        report.check(
            "material executor backdrop copies stay within shared capture plan",
            float(copied_count) <= float(planned_capture_count),
            path=f"{base_path}.backdrop_copy_count",
            expected={"<=": planned_capture_count},
            actual=copied_count,
            likely_layer="platform-runtime",
            likely_pass="material-executor",
            hint=(
                "The macOS edge should copy the previous frame once and share "
                "that capture across all backdrop-sampling material plans."))
        if copied_count > 0:
            report.check(
                "material executor backdrop copy excludes foreground text",
                executor_summary.get(
                    "backdrop_copy_excludes_foreground_text") is True,
                path=f"{base_path}.backdrop_copy_excludes_foreground_text",
                expected=True,
                actual=executor_summary.get(
                    "backdrop_copy_excludes_foreground_text"),
                likely_layer="platform-runtime",
                likely_pass="material-executor",
                hint=(
                    "The backend should copy frame history before foreground "
                    "text and overlays are encoded."))
            foreground_remaps_for_order = executor_summary.get(
                "foreground_text_remap_count")
            if (isinstance(foreground_remaps_for_order, (int, float))
                    and not isinstance(foreground_remaps_for_order, bool)
                    and foreground_remaps_for_order > 0):
                report.check(
                    "material executor foreground pass follows backdrop copy",
                    executor_summary.get(
                        "foreground_pass_after_backdrop_copy") is True,
                    path=f"{base_path}.foreground_pass_after_backdrop_copy",
                    expected=True,
                    actual=executor_summary.get(
                        "foreground_pass_after_backdrop_copy"),
                    likely_layer="platform-runtime",
                    likely_pass="material-foreground",
                    hint=(
                        "Foreground text that was remapped for material "
                        "legibility must be drawn after the safe backdrop copy."))
    planned_capture_pixels = executor_summary.get("planned_frame_capture_pixels")
    if (isinstance(planned_capture_pixels, (int, float))
            and not isinstance(planned_capture_pixels, bool)
            and isinstance(copied_pixels, (int, float))
            and not isinstance(copied_pixels, bool)
            and planned_capture_pixels > 0):
        report.check(
            "material executor backdrop copy pixels fit shared capture plan",
            float(copied_pixels) <= float(planned_capture_pixels),
            path=f"{base_path}.backdrop_copy_pixels",
            expected={"<=": planned_capture_pixels},
            actual=copied_pixels,
            likely_layer="platform-runtime",
            likely_pass="material-executor",
            hint=(
                "Shared frame capture pixel budget should mirror the largest "
                "MaterialPlan.backdrop_access.max_frame_capture_pixels."))


def check_material_quality_policy_requirements(
        summary: JsonObject,
        spec: JsonObject,
        report: Report) -> None:
    policy = summary.get("quality_policy")
    if not isinstance(policy, dict):
        policy = {}
    base_path = "debug.platform_runtime.details.renderer.material_plans#quality_policy"
    field_map = {
        "max_blur_radius_lte": "max_blur_radius",
        "max_sample_taps_lte": "max_sample_taps",
        "max_backdrop_pixels_lte": "max_backdrop_pixels",
    }
    for spec_field, summary_field in field_map.items():
        if spec_field not in spec:
            continue
        actual = policy.get(summary_field)
        expected = spec[spec_field]
        report.check(
            f"material quality policy {summary_field} is within limit",
            isinstance(actual, (int, float)) and not isinstance(actual, bool)
            and float(actual) <= float(expected),
            path=f"{base_path}.{summary_field}",
            expected={"<=": expected},
            actual=actual,
            likely_layer="material-plan",
            hint="Inspect MaterialPlan.quality_policy in the resolved plans.")

    required_allowed = {
        "require_backdrop_sampling_allowed": "backdrop_sampling_disabled",
        "require_noise_allowed": "noise_disabled",
        "require_shadow_allowed": "shadow_disabled",
    }
    for spec_field, summary_field in required_allowed.items():
        if spec.get(spec_field) is not True:
            continue
        actual = policy.get(summary_field)
        report.check(
            f"material quality policy {summary_field} is zero",
            actual == 0,
            path=f"{base_path}.{summary_field}",
            expected=0,
            actual=actual,
            likely_layer="material-plan",
            hint="Inspect MaterialPlan.quality_policy and the backend MaterialEnvironment.")


def check_material_semantic_runtime_match(
        semantic_summary: JsonObject,
        material_plan_summary: JsonObject,
        report: Report) -> None:
    base_path = "debug.material_semantic_runtime_match"
    semantic_count = semantic_summary.get("material_nodes")
    runtime_count = material_plan_summary.get("count")
    report.check(
        "material semantic/runtime count matches",
        semantic_count == runtime_count,
        path=f"{base_path}.count",
        expected=semantic_count,
        actual=runtime_count,
        likely_layer="material-contract",
        hint=(
            "Semantic material nodes and backend MaterialRect commands diverged; "
            "inspect layout::material_surface emission, overlay collection, and "
            "the backend command parser."))

    semantic_kinds = semantic_summary.get("material_kinds")
    runtime_kinds = material_plan_summary.get("kinds")
    report.check(
        "material semantic/runtime kinds match",
        semantic_kinds == runtime_kinds,
        path=f"{base_path}.kinds",
        expected=semantic_kinds,
        actual=runtime_kinds,
        likely_layer="material-contract",
        hint=(
            "The semantic tree and runtime plan summary disagree on material "
            "kinds; check whether a material node was skipped or decoded with "
            "the wrong kind."))

    semantic_roles = semantic_summary.get("material_roles")
    runtime_roles = material_plan_summary.get("roles")
    report.check(
        "material semantic/runtime roles match",
        semantic_roles == runtime_roles,
        path=f"{base_path}.roles",
        expected=semantic_roles,
        actual=runtime_roles,
        likely_layer="material-contract",
        hint=(
            "The semantic tree and runtime plan summary disagree on material "
            "surface roles; check layout::MaterialSurfaceOptions.role, "
            "MaterialRect payload writes, and backend command decoding."))

    semantic_container_modes = semantic_summary.get("material_container_modes")
    runtime_container = material_plan_summary.get("container")
    runtime_container_modes = (
        runtime_container.get("modes")
        if isinstance(runtime_container, dict)
        else None)
    report.check(
        "material semantic/runtime container modes match",
        semantic_container_modes == runtime_container_modes,
        path=f"{base_path}.container_modes",
        expected=semantic_container_modes,
        actual=runtime_container_modes,
        likely_layer="material-container",
        hint=(
            "The semantic tree and runtime plan summary disagree on material "
            "container modes; check layout::material_container inheritance and "
            "MaterialRect payload decoding."))

    semantic_container_ids = semantic_summary.get("material_container_ids")
    runtime_container_ids = (
        runtime_container.get("container_ids")
        if isinstance(runtime_container, dict)
        else None)
    report.check(
        "material semantic/runtime container ids match",
        semantic_container_ids == runtime_container_ids,
        path=f"{base_path}.container_ids",
        expected=semantic_container_ids,
        actual=runtime_container_ids,
        likely_layer="material-container",
        hint=(
            "The semantic tree and runtime plan summary disagree on material "
            "container ids; inspect material_container scopes and command "
            "descriptor serialization."))

    semantic_profiles = semantic_summary.get("material_verifier_profiles")
    runtime_profiles = material_plan_summary.get("verifier_profiles")
    report.check(
        "material semantic/runtime verifier profiles match",
        semantic_profiles == runtime_profiles,
        path=f"{base_path}.verifier_profiles",
        expected=semantic_profiles,
        actual=runtime_profiles,
        likely_layer="material-contract",
        hint=(
            "The semantic material verifier profile and runtime plan verifier "
            "region disagree; check material_style defaults and pure "
            "MaterialPlan verifier expectations."))

    semantic_missing = semantic_summary.get("material_descriptor_missing")
    runtime_missing = material_plan_summary.get("command_descriptor_missing")
    report.check(
        "material semantic descriptors are complete",
        semantic_missing == 0,
        path=f"{base_path}.semantic_command_descriptors",
        expected=0,
        actual=semantic_missing,
        likely_layer="material-command",
        hint=(
            "Semantic material nodes must expose every command descriptor field, "
            "including tint, so runtime command decoding can be compared."))
    report.check(
        "material runtime command descriptors are complete",
        runtime_missing == 0,
        path=f"{base_path}.runtime_command_descriptors",
        expected=0,
        actual=runtime_missing,
        likely_layer="material-command",
        hint=(
            "MaterialPlan.command_descriptor must be serialized for every "
            "runtime material plan."))

    semantic_descriptors = semantic_summary.get("material_descriptors")
    runtime_descriptors = material_plan_summary.get("command_descriptors")
    semantic_signatures = sorted(
        descriptor_signature(item)
        for item in semantic_descriptors
        if isinstance(item, dict))
    runtime_signatures = sorted(
        descriptor_signature(item)
        for item in runtime_descriptors
        if isinstance(item, dict))
    report.check(
        "material semantic/runtime command descriptors match",
        semantic_signatures == runtime_signatures,
        path=f"{base_path}.command_descriptors",
        expected=semantic_signatures,
        actual=runtime_signatures,
        likely_layer="material-command",
        hint=(
            "The semantic material node descriptor and runtime "
            "MaterialPlan.command_descriptor disagree; inspect material style "
            "emission, MaterialRect command payload writes, and backend command "
            "decoding before changing resolved fallback policy."))


def load_platform_files(platform_dir: Path, report: Report) -> list[JsonObject]:
    files: list[JsonObject] = []
    if not platform_dir.exists():
        return files

    for path in sorted(platform_dir.iterdir()):
        if not path.is_file():
            continue
        entry: JsonObject = {
            "bytes": path.stat().st_size,
            "path": str(path),
        }
        if path.suffix == ".json":
            parsed = load_json_file(path, report)
            entry["json"] = isinstance(parsed, dict)
            if isinstance(parsed, dict) and isinstance(parsed.get("artifact_reason"), str):
                entry["artifact_reason"] = parsed["artifact_reason"]
        files.append(entry)
    return files


def verify(args: argparse.Namespace) -> int:
    bundle = Path(args.bundle).resolve()
    report = Report(bundle)

    if not apply_manifest(args, report):
        return report.finish()

    if not report.check("bundle directory exists", bundle.is_dir(), str(bundle)):
        return report.finish()

    snapshot_path = bundle / "snapshot.json"
    if not report.check("snapshot.json exists", snapshot_path.is_file(), str(snapshot_path)):
        return report.finish()

    root = load_json_file(snapshot_path, report)
    if not report.check("snapshot root is object", isinstance(root, dict), "top-level JSON"):
        return report.finish()

    debug = object_at(root, "debug")
    if not report.check("debug object exists", debug is not None, "debug"):
        return report.finish()

    capabilities = object_at(debug, "platform_capabilities")
    input_debug = object_at(debug, "input_debug")
    semantic_tree = object_at(debug, "semantic_tree")
    runtime = object_at(debug, "platform_runtime")

    report.check("platform_capabilities object exists", capabilities is not None)
    report.check("input_debug object exists", input_debug is not None)
    report.check("semantic_tree object exists", semantic_tree is not None)
    report.check("platform_runtime object exists", runtime is not None)
    if not all((capabilities, input_debug, semantic_tree, runtime)):
        return report.finish()

    assert capabilities is not None
    assert input_debug is not None
    assert semantic_tree is not None
    assert runtime is not None

    for key in (
        "snapshot_json",
        "write_artifact_bundle",
        "semantic_tree",
        "input_debug",
        "platform_runtime",
    ):
        report.check(
            f"capability {key} is true",
            bool_at(capabilities, key) is True,
            repr(capabilities.get(key)))

    for key in args.require_capability:
        report.check(
            f"capability {key} is true",
            bool_at(capabilities, key) is True,
            repr(capabilities.get(key)))

    if args.require_debug_detail:
        report.check("debug details object exists", isinstance(debug, dict))
    for spec in args.require_debug_detail:
        if "=" not in spec:
            report.check("debug detail spec is valid", False, spec)
            continue
        path, expected_raw = spec.split("=", 1)
        try:
            expected = json.loads(expected_raw)
        except json.JSONDecodeError:
            expected = expected_raw
        exists, actual = value_at(debug, path) if isinstance(debug, dict) else (False, None)
        report.check(
            f"debug detail matches: {path}",
            exists and actual == expected,
            f"expected {expected!r}, got {actual!r}",
            path=f"debug.{path}",
            expected=expected,
            actual=actual,
            likely_layer="application-debug",
            hint=(
                "Check the app-specific debug payload provider and the "
                "shared model snapshot it serializes."))

    check_file_explorer_native_chrome_contract(report, debug)

    details = runtime.get("details")
    if args.require_runtime_detail:
        report.check("runtime details object exists", isinstance(details, dict))
    for spec in args.require_runtime_detail:
        if "=" not in spec:
            report.check("runtime detail spec is valid", False, spec)
            continue
        path, expected_raw = spec.split("=", 1)
        try:
            expected = json.loads(expected_raw)
        except json.JSONDecodeError:
            expected = expected_raw
        exists, actual = value_at(details, path) if isinstance(details, dict) else (False, None)
        report.check(
            f"runtime detail matches: {path}",
            exists and actual == expected,
            f"expected {expected!r}, got {actual!r}",
            path=f"debug.platform_runtime.details.{path}",
            expected=expected,
            actual=actual,
            likely_layer="platform-runtime",
            hint="Check the backend runtime JSON details for this renderer pass.")

    for spec in args.require_runtime_numeric_bound:
        path = spec["path"]
        exists, actual = value_at(details, path) if isinstance(details, dict) else (False, None)
        numeric = isinstance(actual, (int, float)) and not isinstance(actual, bool)
        likely_pass = "material-executor" if "material_executor_summary" in path else ""
        report.check(
            f"runtime numeric bound exists: {path}",
            exists and numeric,
            f"expected numeric runtime detail, got {actual!r}",
            path=f"debug.platform_runtime.details.{path}",
            expected="number",
            actual=actual,
            likely_layer="platform-runtime",
            likely_pass=likely_pass,
            hint="Check backend runtime numeric summary serialization.")
        if not (exists and numeric):
            continue
        assert isinstance(actual, (int, float)) and not isinstance(actual, bool)
        if "equals" in spec:
            expected = spec["equals"]
            report.check(
                f"runtime numeric equals: {path}",
                actual == expected,
                f"expected {path} == {expected}, got {actual}",
                path=f"debug.platform_runtime.details.{path}",
                expected=expected,
                actual=actual,
                likely_layer="platform-runtime",
                likely_pass=likely_pass,
                hint="Inspect the backend executor summary for a drifted count or budget.")
        if "gte" in spec:
            expected = spec["gte"]
            report.check(
                f"runtime numeric >= bound: {path}",
                float(actual) >= float(expected),
                f"expected {path} >= {expected}, got {actual}",
                path=f"debug.platform_runtime.details.{path}",
                expected={">=": expected},
                actual=actual,
                likely_layer="platform-runtime",
                likely_pass=likely_pass,
                hint="Inspect the backend executor summary for a missing pass or counter.")
        if "lte" in spec:
            expected = spec["lte"]
            report.check(
                f"runtime numeric <= bound: {path}",
                float(actual) <= float(expected),
                f"expected {path} <= {expected}, got {actual}",
                path=f"debug.platform_runtime.details.{path}",
                expected={"<=": expected},
                actual=actual,
                likely_layer="platform-runtime",
                likely_pass=likely_pass,
                hint="Inspect the backend executor summary for excess work or an unbounded resource.")

    platform = string_at(capabilities, "platform")
    if args.expect_platform:
        report.check(
            "platform matches expectation",
            platform == args.expect_platform,
            f"expected {args.expect_platform}, got {platform}")

    viewport = runtime.get("viewport")
    report.check("runtime backend is present", isinstance(runtime.get("backend"), str))
    report.check("runtime viewport is object", isinstance(viewport, dict))
    if isinstance(viewport, dict):
        report.check("runtime viewport is valid", viewport.get("valid") is True)
        report.check(
            "runtime viewport has positive size",
            number_at(viewport, "w") is not None and number_at(viewport, "h") is not None
            and float(viewport["w"]) > 0.0 and float(viewport["h"]) > 0.0,
            repr(viewport))

    for key in (
        "event",
        "source",
        "detail",
        "result",
        "pressed_id",
        "focus_visible",
        "caret_rect",
    ):
        report.check(f"input_debug contains {key}", key in input_debug)
    report.check(
        "runtime contains pressed callback state",
        "pressed_callback_id" in runtime,
        repr(runtime.get("pressed_callback_id")),
        path="debug.platform_runtime.pressed_callback_id",
        expected="null or callback id",
        actual=runtime.get("pressed_callback_id"),
        likely_layer="input-runtime",
        hint="Check core input state propagation from pressed_id into the debug plane.")
    report.check(
        "runtime contains focus-visible state",
        "focus_visible" in runtime,
        repr(runtime.get("focus_visible")),
        path="debug.platform_runtime.focus_visible",
        expected="boolean keyboard focus modality",
        actual=runtime.get("focus_visible"),
        likely_layer="input-runtime",
        hint=(
            "Check core focus modality propagation from focus_visible into "
            "the debug plane. Pointer and platform-consumed pointer presses "
            "must clear stale keyboard focus rings."))

    report.check("semantic root role is root", semantic_tree.get("role") == "root")
    summary = summarize_semantic_tree(semantic_tree)
    report.data["semantic_tree"] = summary
    report.check("semantic tree has nodes", summary["nodes"] > 0, str(summary["nodes"]))
    report.check(
        "semantic nodes have roles",
        summary["nodes_without_role"] == 0,
        str(summary["nodes_without_role"]))
    report.check(
        "semantic child lists are valid",
        summary["invalid_child_lists"] == 0,
        str(summary["invalid_child_lists"]))

    roles = summary["roles"]
    for required_role in args.require_role:
        report.check(
            f"semantic role exists: {required_role}",
            roles.get(required_role, 0) > 0,
            repr(roles))
    if args.require_disabled_count:
        report.check(
            "semantic disabled node count",
            summary["disabled_nodes"] >= args.require_disabled_count,
            f"expected at least {args.require_disabled_count}, "
            f"got {summary['disabled_nodes']}")
    material_kinds = summary["material_kinds"]
    for required_kind in args.require_material_kind:
        report.check(
            f"semantic material kind exists: {required_kind}",
            material_kinds.get(required_kind, 0) > 0,
            repr(material_kinds))
    material_roles = summary["material_roles"]
    for required_role in args.require_material_surface_role:
        report.check(
            f"semantic material surface role exists: {required_role}",
            material_roles.get(required_role, 0) > 0,
            repr(material_roles))
    if args.require_material_fallback:
        report.check(
            "semantic material fallback exists",
            summary["material_fallback_nodes"] > 0,
            str(summary["material_fallback_nodes"]))

    renderer_details = object_at(runtime, "details.renderer")
    material_plan_summary: JsonObject | None = None
    if args.require_material_plan:
        report.check(
            "renderer runtime details object exists for material plans",
            renderer_details is not None,
            path="debug.platform_runtime.details.renderer",
            expected="object",
            actual=None if renderer_details is None else "object",
            likely_layer="platform-runtime",
            hint="The backend must write renderer.material_plans into runtime details.")
    if renderer_details is not None:
        material_plans = renderer_details.get("material_plans")
        if args.require_material_plan or material_plans is not None:
            renderer_contract_version = renderer_details.get(
                "material_plan_contract_version")
            renderer_contract_version_is_int = (
                isinstance(renderer_contract_version, int)
                and not isinstance(renderer_contract_version, bool))
            report.check(
                "renderer material plan contract version is integer",
                renderer_contract_version_is_int,
                path=(
                    "debug.platform_runtime.details.renderer."
                    "material_plan_contract_version"),
                expected="integer",
                actual=renderer_contract_version,
                likely_layer="platform-runtime",
                hint=(
                    "Backends should publish the MaterialPlan artifact "
                    "contract version next to renderer.material_plans."))
            if renderer_contract_version_is_int:
                report.check(
                    "renderer material plan contract version is supported",
                    renderer_contract_version == MATERIAL_PLAN_CONTRACT_VERSION,
                    path=(
                        "debug.platform_runtime.details.renderer."
                        "material_plan_contract_version"),
                    expected=MATERIAL_PLAN_CONTRACT_VERSION,
                    actual=renderer_contract_version,
                    likely_layer="platform-runtime",
                    hint=(
                        "Update verify_artifact_bundle.py and all backend "
                        "renderer contract serializers before changing the "
                        "MaterialPlan artifact version."))
            material_plan_summary = summarize_material_plans(
                material_plans,
                report,
                "debug.platform_runtime.details.renderer.material_plans")
            report.data["material_plans"] = material_plan_summary
            if args.require_material_plan:
                report.check(
                    "material plans exist",
                    material_plan_summary["count"] > 0,
                    path="debug.platform_runtime.details.renderer.material_plans",
                    expected="non-empty array",
                    actual=material_plan_summary["count"],
                    likely_layer="platform-runtime",
                    hint="MaterialRect commands may not have reached the backend decoder.")
            plan_count = renderer_details.get("material_plan_count")
            if isinstance(plan_count, int) and not isinstance(plan_count, bool):
                report.check(
                    "material plan count matches array length",
                    plan_count == material_plan_summary["count"],
                    path="debug.platform_runtime.details.renderer.material_plan_count",
                    expected=material_plan_summary["count"],
                    actual=plan_count,
                    likely_layer="platform-runtime",
                    hint="Keep renderer.material_plan_count in sync with renderer.material_plans.")
            check_material_runtime_summary_contract(
                material_plan_summary,
                renderer_details.get("material_runtime_summary"),
                report)
            check_material_executor_summary_contract(
                material_plan_summary,
                renderer_details.get("material_executor_summary"),
                report)
            material_plan_summary_spec = getattr(
                args,
                "require_material_plan_summary",
                None)
            if isinstance(material_plan_summary_spec, dict):
                check_material_plan_summary_requirements(
                    material_plan_summary,
                    material_plan_summary_spec,
                    report)
            material_resource_bounds_spec = getattr(
                args,
                "require_material_resource_bounds",
                None)
            if isinstance(material_resource_bounds_spec, dict):
                check_material_resource_bounds_requirements(
                    material_plan_summary,
                    material_resource_bounds_spec,
                    report)
            material_quality_policy_spec = getattr(
                args,
                "require_material_quality_policy",
                None)
            if isinstance(material_quality_policy_spec, dict):
                check_material_quality_policy_requirements(
                    material_plan_summary,
                    material_quality_policy_spec,
                    report)
    if args.require_material_semantic_runtime_match:
        report.check(
            "material semantic/runtime summary is available",
            isinstance(material_plan_summary, dict),
            path="debug.platform_runtime.details.renderer.material_plans",
            expected="material plan summary",
            actual=type(material_plan_summary).__name__,
            likely_layer="material-contract",
            hint="Enable renderer.material_plans before requiring semantic/runtime parity.")
        if isinstance(material_plan_summary, dict):
            check_material_semantic_runtime_match(summary, material_plan_summary, report)

    report.data["artifact_context"] = material_failure_context(
        capabilities,
        runtime,
        summary,
        renderer_details,
        material_plan_summary)

    full_labels: list[str] = []
    walk_labels(semantic_tree, full_labels)
    for required_label in args.require_label:
        report.check(
            f"semantic label exists: {required_label}",
            required_label in full_labels,
            f"{len(full_labels)} labels")
    for required_substring in args.require_label_contains:
        report.check(
            f"semantic label contains: {required_substring}",
            any(required_substring in label for label in full_labels),
            f"{len(full_labels)} labels")

    frame_path = bundle / "frame.bmp"
    frame_capability = bool_at(capabilities, "frame_image")
    if frame_path.exists():
        try:
            frame = parse_bmp(frame_path)
            report.data["frame"] = frame
            report.check("frame.bmp is valid BMP", True, str(frame_path))
        except (OSError, ValueError) as exc:
            frame = None
            report.check("frame.bmp is valid BMP", False, str(exc))
    else:
        frame = None
        if args.require_frame or frame_capability is True:
            report.check("frame.bmp exists", False, str(frame_path))
        elif args.require_pixel_region or args.forbid_pixel_region_color:
            report.check(
                "frame.bmp exists for pixel-region checks",
                False,
                str(frame_path))
        else:
            report.warn("frame.bmp is absent; platform reported no frame image support")

    if frame is not None:
        renderer = object_at(runtime, "details.renderer")
        if renderer is not None:
            expected_width = number_at(renderer, "last_render_width") or number_at(
                renderer, "drawable_width")
            expected_height = number_at(renderer, "last_render_height") or number_at(
                renderer, "drawable_height")
            if expected_width and expected_height:
                report.check(
                    "frame size matches renderer runtime",
                    int(expected_width) == frame["width"]
                    and int(expected_height) == frame["height"],
                    f"frame={frame['width']}x{frame['height']} "
                    f"runtime={int(expected_width)}x{int(expected_height)}")

        pixel_regions: list[JsonObject] = []
        for spec in args.require_pixel_region:
            try:
                name, coords, min_delta, min_unique = parse_pixel_region_spec(spec)
                rect = resolve_region(
                    coords,
                    int(frame["width"]),
                    int(frame["height"]))
                region = analyze_bmp_region(frame_path, frame, rect)
                region["name"] = name
                region["min_luma_delta"] = min_delta
                region["min_unique_colors"] = min_unique
                pixel_regions.append(region)
                region_layers: JsonObject = {}
                region_passes: JsonObject = {}
                if material_plan_summary is not None:
                    layers = material_plan_summary.get("region_layers")
                    if isinstance(layers, dict):
                        region_layers = layers
                    passes = material_plan_summary.get("region_passes")
                    if isinstance(passes, dict):
                        region_passes = passes
                likely_layer = str(region_layers.get(
                    name,
                    "material-or-backdrop-pass"))
                likely_pass = str(region_passes.get(name, ""))
                report.check(
                    f"pixel region is non-empty: {name}",
                    region["sampled_pixels"] > 0 and region["width"] > 0 and region["height"] > 0,
                    repr(rect),
                    path=f"frame.bmp#{name}",
                    expected="non-empty region",
                    actual=region,
                    region=name,
                    likely_layer="frame-capture",
                    hint="Check the manifest rect or renderer capture dimensions.")
                report.check(
                    f"pixel region has luma contrast: {name}",
                    float(region["luma_delta"]) >= min_delta,
                    f"expected >= {min_delta}, got {region['luma_delta']}",
                    path=f"frame.bmp#{name}.luma_delta",
                    expected={">=": min_delta},
                    actual=region,
                    region=name,
                    likely_layer=likely_layer,
                    likely_pass=likely_pass,
                    hint="If this is a material region, inspect renderer.material_plans and backdrop pass output.")
                report.check(
                    f"pixel region has color variety: {name}",
                    int(region["unique_colors"]) >= min_unique,
                    f"expected >= {min_unique}, got {region['unique_colors']}",
                    path=f"frame.bmp#{name}.unique_colors",
                    expected={">=": min_unique},
                    actual=region,
                    region=name,
                    likely_layer=likely_layer,
                    likely_pass=likely_pass,
                    hint="A flat region usually means the fallback layer or capture pass replaced the expected scene detail.")
            except ValueError as exc:
                report.check(
                    f"pixel region spec is valid: {spec}",
                    False,
                    str(exc),
                    path="manifest.pixel_regions",
                    expected="NAME:X,Y,W,H[:MIN_LUMA_DELTA[:MIN_UNIQUE_COLORS]]",
                    actual=spec,
                    region=spec.split(":", 1)[0],
                    likely_layer="artifact-manifest",
                    hint="Fix the pixel region manifest entry.")
        if pixel_regions:
            report.data["pixel_regions"] = pixel_regions

        pixel_region_by_name = {
            str(region.get("name")): region
            for region in pixel_regions
            if isinstance(region.get("name"), str)
        }
        for index, spec in enumerate(args.forbid_pixel_region_color):
            region_name = str(spec.get("region", ""))
            region = pixel_region_by_name.get(region_name)
            report.check(
                f"forbidden pixel color region exists: {region_name}",
                isinstance(region, dict),
                path=f"manifest.forbid_pixel_region_colors[{index}].region",
                expected="existing pixel region",
                actual=region_name,
                region=region_name,
                likely_layer="artifact-manifest",
                hint="Add the named region to pixel_regions before forbidding colors.",
                record_success=False)
            if not isinstance(region, dict):
                continue
            try:
                rect = (
                    int(region["x"]),
                    int(region["y"]),
                    int(region["width"]),
                    int(region["height"]),
                )
                rgb = spec.get("rgb")
                if not isinstance(rgb, list):
                    raise ValueError("rgb must be a list")
                color_summary = count_bmp_region_color(
                    frame_path,
                    frame,
                    rect,
                    [int(rgb[0]), int(rgb[1]), int(rgb[2])],
                    int(spec.get("tolerance", 0)))
            except (KeyError, TypeError, ValueError) as exc:
                report.check(
                    f"forbidden pixel color spec is valid: {region_name}",
                    False,
                    str(exc),
                    path=f"manifest.forbid_pixel_region_colors[{index}]",
                    expected="valid region color probe",
                    actual=spec,
                    region=region_name,
                    likely_layer="artifact-manifest",
                    hint="Fix the forbidden color manifest entry.",
                    record_success=False)
                continue
            max_pixels = int(spec.get("max_pixels", 0))
            matching_pixels = int(color_summary["matching_pixels"])
            report.check(
                f"forbidden pixel color absent: {spec.get('name')}",
                matching_pixels <= max_pixels,
                (
                    f"expected <= {max_pixels} matching pixels, "
                    f"got {matching_pixels}"
                ),
                path=f"frame.bmp#{region_name}.forbidden_color.{spec.get('name')}",
                expected={"max_pixels": max_pixels, "rgb": spec.get("rgb")},
                actual=color_summary,
                region=region_name,
                likely_layer="native-window-chrome",
                likely_pass="window-control-marker",
                hint=(
                    "The titlebar reserve must stay visually neutral. Native "
                    "traffic lights belong to the OS window frame, not the "
                    "captured phenotype content or artifact-only overlays."))
        for index, spec in enumerate(args.require_pixel_region_metric):
            region_name = str(spec.get("region", ""))
            metric = str(spec.get("metric", ""))
            region = pixel_region_by_name.get(region_name)
            report.check(
                f"pixel region metric region exists: {region_name}",
                isinstance(region, dict),
                path=f"manifest.pixel_region_metrics[{index}].region",
                expected="existing pixel region",
                actual=region_name,
                region=region_name,
                likely_layer="artifact-manifest",
                hint="Add the named region to pixel_regions before applying metric bounds.",
                record_success=False)
            if not isinstance(region, dict):
                continue
            actual = region.get(metric)
            report.check(
                f"pixel region metric is numeric: {region_name}.{metric}",
                isinstance(actual, (int, float)) and not isinstance(actual, bool),
                path=f"frame.bmp#{region_name}.{metric}",
                expected="numeric metric",
                actual=actual,
                region=region_name,
                likely_layer="frame-capture",
                hint="Check analyze_bmp_region and the metric name.",
                record_success=False)
            if not isinstance(actual, (int, float)) or isinstance(actual, bool):
                continue
            likely_layer = "material-or-backdrop-pass"
            likely_pass = ""
            if material_plan_summary is not None:
                layers = material_plan_summary.get("region_layers")
                if isinstance(layers, dict):
                    likely_layer = str(layers.get(region_name, likely_layer))
                passes = material_plan_summary.get("region_passes")
                if isinstance(passes, dict):
                    likely_pass = str(passes.get(region_name, likely_pass))
            for bound, symbol in (("gte", ">="), ("lte", "<=")):
                if bound not in spec:
                    continue
                expected = float(spec[bound])
                ok = float(actual) >= expected if bound == "gte" else float(actual) <= expected
                report.check(
                    f"pixel region metric {symbol} bound: {region_name}.{metric}",
                    ok,
                    f"expected {symbol} {expected}, got {actual}",
                    path=f"frame.bmp#{region_name}.{metric}",
                    expected={symbol: expected},
                    actual=region,
                    region=region_name,
                    likely_layer=likely_layer,
                    likely_pass=likely_pass,
                    hint=(
                        "For blur probes, inspect renderer.material_plans, "
                        "the backdrop sample pass, and whether the manifest "
                        "region avoids foreground text."))

        for index, spec in enumerate(args.require_pixel_region_metric_comparison):
            region_name = str(spec.get("region", ""))
            metric = str(spec.get("metric", ""))
            reference_name = str(spec.get("reference_region", ""))
            reference_metric = str(spec.get("reference_metric", metric))
            region = pixel_region_by_name.get(region_name)
            reference_region = pixel_region_by_name.get(reference_name)
            report.check(
                f"pixel region comparison region exists: {region_name}",
                isinstance(region, dict),
                path=f"manifest.pixel_region_metric_comparisons[{index}].region",
                expected="existing pixel region",
                actual=region_name,
                region=region_name,
                likely_layer="artifact-manifest",
                hint="Add the named region to pixel_regions before applying metric comparisons.",
                record_success=False)
            report.check(
                f"pixel region comparison reference exists: {reference_name}",
                isinstance(reference_region, dict),
                path=f"manifest.pixel_region_metric_comparisons[{index}].reference_region",
                expected="existing pixel region",
                actual=reference_name,
                region=reference_name,
                likely_layer="artifact-manifest",
                hint="Add the reference region to pixel_regions before applying metric comparisons.",
                record_success=False)
            if not isinstance(region, dict) or not isinstance(reference_region, dict):
                continue
            actual = region.get(metric)
            reference_actual = reference_region.get(reference_metric)
            report.check(
                f"pixel region comparison metric is numeric: {region_name}.{metric}",
                isinstance(actual, (int, float)) and not isinstance(actual, bool),
                path=f"frame.bmp#{region_name}.{metric}",
                expected="numeric metric",
                actual=actual,
                region=region_name,
                likely_layer="frame-capture",
                hint="Check analyze_bmp_region and the comparison metric name.",
                record_success=False)
            report.check(
                f"pixel region comparison reference metric is numeric: {reference_name}.{reference_metric}",
                isinstance(reference_actual, (int, float))
                and not isinstance(reference_actual, bool),
                path=f"frame.bmp#{reference_name}.{reference_metric}",
                expected="numeric metric",
                actual=reference_actual,
                region=reference_name,
                likely_layer="frame-capture",
                hint="Check analyze_bmp_region and the comparison reference metric name.",
                record_success=False)
            if (
                not isinstance(actual, (int, float))
                or isinstance(actual, bool)
                or not isinstance(reference_actual, (int, float))
                or isinstance(reference_actual, bool)
            ):
                continue
            likely_layer = "material-or-backdrop-pass"
            likely_pass = ""
            if material_plan_summary is not None:
                layers = material_plan_summary.get("region_layers")
                if isinstance(layers, dict):
                    likely_layer = str(layers.get(region_name, likely_layer))
                passes = material_plan_summary.get("region_passes")
                if isinstance(passes, dict):
                    likely_pass = str(passes.get(region_name, likely_pass))
            for bound, symbol in (("gte_ratio", ">="), ("lte_ratio", "<=")):
                if bound not in spec:
                    continue
                ratio = float(spec[bound])
                expected_value = float(reference_actual) * ratio
                ok = (
                    float(actual) >= expected_value
                    if bound == "gte_ratio"
                    else float(actual) <= expected_value
                )
                report.check(
                    f"pixel region metric {symbol} reference ratio: {region_name}.{metric}",
                    ok,
                    (
                        f"expected {region_name}.{metric} {symbol} "
                        f"{reference_name}.{reference_metric} * {ratio}, "
                        f"got {actual} vs {reference_actual}"
                    ),
                    path=f"frame.bmp#{region_name}.{metric}",
                    expected={
                        symbol: {
                            "reference_region": reference_name,
                            "reference_metric": reference_metric,
                            "ratio": ratio,
                            "value": round(expected_value, 3),
                        }
                    },
                    actual={
                        "region": region_name,
                        "metric": metric,
                        "value": actual,
                        "reference_region": reference_name,
                        "reference_metric": reference_metric,
                        "reference_value": reference_actual,
                    },
                    region=region_name,
                    likely_layer=likely_layer,
                    likely_pass=likely_pass,
                    hint=(
                        "For blur comparisons, the material probe should be "
                        "smoother than the named backdrop reference; inspect "
                        "renderer.material_plans and the backdrop pass if this drifts."))

    platform_dir = bundle / "platform"
    platform_files = load_platform_files(platform_dir, report)
    report.data["platform_files"] = platform_files
    if bool_at(capabilities, "platform_diagnostics") is True:
        report.check(
            "platform diagnostics files exist",
            len(platform_files) > 0,
            str(platform_dir))

    report.data["capabilities"] = capabilities
    report.data["runtime"] = {
        "backend": runtime.get("backend"),
        "content_height": runtime.get("content_height"),
        "platform": runtime.get("platform"),
        "viewport": runtime.get("viewport"),
    }
    report.data["snapshot"] = {
        "bytes": snapshot_path.stat().st_size,
        "debug_keys": sorted(debug.keys()),
        "path": str(snapshot_path),
    }

    return report.finish()


def walk_labels(node: Any, labels: list[str]) -> None:
    if not isinstance(node, dict):
        return
    label = node.get("label")
    if isinstance(label, str) and label:
        labels.append(label)
    children = node.get("children")
    if isinstance(children, list):
        for child in children:
            walk_labels(child, labels)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate a phenotype debug artifact bundle.")
    parser.add_argument("bundle", help="Path to an artifact bundle directory.")
    parser.add_argument(
        "--manifest",
        help="JSON manifest containing reusable verifier requirements.")
    parser.add_argument(
        "--expect-platform",
        help="Require debug.platform_capabilities.platform to match this value.")
    parser.add_argument(
        "--require-frame",
        action="store_true",
        help="Fail if frame.bmp is missing.")
    parser.add_argument(
        "--require-label",
        action="append",
        default=[],
        help="Require an exact semantic tree label. Repeatable.")
    parser.add_argument(
        "--require-label-contains",
        action="append",
        default=[],
        help="Require a semantic tree label containing this substring. Repeatable.")
    parser.add_argument(
        "--require-role",
        action="append",
        default=[],
        help="Require at least one semantic tree node with this role. Repeatable.")
    parser.add_argument(
        "--require-disabled-count",
        type=int,
        default=0,
        help="Require at least this many semantic tree nodes with enabled=false.")
    parser.add_argument(
        "--require-material-kind",
        action="append",
        default=[],
        help="Require at least one semantic material node with this kind.")
    parser.add_argument(
        "--require-material-surface-role",
        action="append",
        default=[],
        help="Require at least one semantic material node with this surface role.")
    parser.add_argument(
        "--require-material-fallback",
        action="store_true",
        help="Require at least one semantic material node with fallback=true.")
    parser.add_argument(
        "--require-material-plan",
        action="store_true",
        help=(
            "Require debug.platform_runtime.details.renderer.material_plans "
            "to describe resolved backend material plans."))
    parser.add_argument(
        "--require-material-semantic-runtime-match",
        action="store_true",
        help=(
            "Require semantic material node count/kinds to match resolved "
            "backend material plan count/kinds."))
    parser.add_argument(
        "--require-capability",
        action="append",
        default=[],
        help="Require a debug.platform_capabilities boolean to be true. Repeatable.")
    parser.add_argument(
        "--require-runtime-detail",
        action="append",
        default=[],
        metavar="PATH=JSON",
        help=(
            "Require debug.platform_runtime.details PATH to equal the JSON value. "
            "Example: renderer.material_pipeline_ready=true. Repeatable."))
    parser.add_argument(
        "--require-debug-detail",
        action="append",
        default=[],
        metavar="PATH=JSON",
        help=(
            "Require debug PATH to equal the JSON value. "
            "Example: application.file_explorer.view_mode.value=\"icon\". "
            "Repeatable."))
    parser.add_argument(
        "--require-pixel-region",
        action="append",
        default=[],
        metavar="NAME:X,Y,W,H[:MIN_LUMA_DELTA[:MIN_UNIQUE_COLORS]]",
        help=(
            "Require a frame.bmp region to have contrast and color variety. "
            "Coordinates are absolute pixels, or normalized fractions when all "
            "four values are between 0 and 1. Repeatable."))
    parser.add_argument(
        "--require-pixel-region-metric",
        action="append",
        default=[],
        type=pixel_region_metric_spec_from_cli,
        metavar="REGION:METRIC:BOUND:VALUE",
        help=(
            "Require a previously named pixel region metric to satisfy a "
            "numeric bound. METRIC is one of alpha_mean, blue_mean, "
            "edge_energy, green_mean, luma_delta, luma_mean, luma_stddev, "
            "red_mean, unique_colors. BOUND is gte or lte. "
            "Repeatable."))
    parser.set_defaults(
        require_material_plan_summary=None,
        require_material_resource_bounds=None,
        require_material_quality_policy=None,
        require_runtime_numeric_bound=[],
        require_pixel_region_metric_comparison=[],
        forbid_pixel_region_color=[],
        require_material_semantic_runtime_match=False)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    return verify(parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

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
    return {
        "contract_version": 1,
        "kind": "regular",
        "plan_id": "material.regular.fallback",
        "geometry": {"x": 12.0, "y": 20.0, "w": 240.0, "h": 96.0, "radius": 10.0},
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
            "reduced_transparency": False,
            "can_sample_backdrop": False,
            "first_blocker": "unsupported-backend",
        },
        "opacity": 0.58,
        "blur_radius": 0.0,
        "tint": {"r": 255, "g": 255, "b": 255, "a": 148},
        "saturation": 1.0,
        "luminance_floor": 0.08,
        "luminance_gain": 1.08,
        "edge_highlight": 0.34,
        "edge_width": 1.0,
        "noise_opacity": 0.0,
        "shadow_alpha": 0.14,
        "shadow_radius": 14.0,
        "backdrop_sampling": False,
        "backdrop": {
            "available": False,
            "stable": False,
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
        "fallback": True,
        "fallback_path": "unsupported-backend",
        "fallback_reason": "backend reports no material backdrop blur support",
        "debug_seed": 4919,
        "sample_taps": sample_taps,
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
            "max_pass_count": 1,
            "max_backdrop_pixels": 320 * 240,
            "bounded_texture_copy": True,
            "deterministic_fallback": True,
        },
        "verifier": {
            "require_backdrop_source": False,
            "require_edge_highlight": False,
            "min_luma_delta": 3.0,
            "min_unique_colors": 2,
            "region_name": "regular-legibility-backdrop",
            "likely_layer": "material-fallback-pass",
        },
        "passes": [primary],
    }


def material_runtime_summary(plan: dict[str, object]) -> dict[str, object]:
    budget = plan["resource_budget"]
    primary = plan["primary_pass"]
    assert isinstance(budget, dict)
    assert isinstance(primary, dict)
    return {
        "plan_count": 1,
        "fallback_count": 1 if plan["fallback"] else 0,
        "backdrop_sampling_count": 1 if plan["backdrop_sampling"] else 0,
        "total_runtime_passes": 1,
        "active_runtime_passes": 1 if primary["active"] else 0,
        "backdrop_runtime_passes": 1 if primary["requires_backdrop"] else 0,
        "max_pass_texture_copy_pixels": primary["max_texture_copy_pixels"],
        "total_pass_texture_copy_pixels": primary["max_texture_copy_pixels"],
        "max_plan_blur_radius": plan["blur_radius"],
        "max_plan_sample_taps": plan["sample_taps"],
        "max_budget_blur_radius": budget["max_blur_radius"],
        "max_sample_taps": budget["max_sample_taps"],
        "max_pass_count": budget["max_pass_count"],
        "max_backdrop_pixels": budget["max_backdrop_pixels"],
        "unbounded_texture_copy": 0 if budget["bounded_texture_copy"] else 1,
        "non_deterministic_fallback": (
            0 if budget["deterministic_fallback"] else 1
        ),
    }


def material_executor_summary(plan: dict[str, object]) -> dict[str, object]:
    return {
        "plan_count": 1,
        "material_instance_count": 0 if plan["fallback"] else 1,
        "fallback_instance_count": 1 if plan["fallback"] else 0,
        "material_draw_calls": 0 if plan["fallback"] else 1,
        "backdrop_copy_count": 0,
        "backdrop_copy_pixels": 0,
        "material_upload_bytes": 0,
        "material_buffer_capacity_bytes": 0,
        "material_buffer_reallocations": 0,
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
                "details": {
                    "renderer": {
                        "material_plan_count": 1,
                        "material_plans": [plan],
                        "material_runtime_summary": material_runtime_summary(plan),
                        "material_executor_summary": material_executor_summary(plan),
                    }
                },
            },
        }
    }


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


class ArtifactVerifierContractTest(unittest.TestCase):
    def run_verifier(
        self,
        snapshot_json: dict[str, object],
        manifest: dict[str, object] | None = None,
        write_frame: bool = False,
    ) -> tuple[int, dict[str, object]]:
        with tempfile.TemporaryDirectory() as raw_dir:
            bundle = Path(raw_dir)
            (bundle / "snapshot.json").write_text(
                json.dumps(snapshot_json),
                encoding="utf-8")
            if write_frame:
                write_synthetic_bmp(bundle / "frame.bmp")
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
        self.assertEqual(
            report["material_plans"]["resource_bounds"]["max_plan_sample_taps"],
            0)
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
            report["material_plans"]["resource_bounds"][
                "max_pass_texture_copy_pixels"],
            0)
        self.assertEqual(
            report["material_plans"]["resource_bounds"][
                "total_pass_texture_copy_pixels"],
            0)
        self.assertEqual(
            report["material_plans"]["decision_trace"]["first_blockers"],
            {"unsupported-backend": 1})
        self.assertEqual(
            report["material_plans"]["decision_trace"][
                "can_sample_backdrop"],
            0)

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
        self.assertEqual(report["failure_summary"]["count"], 1)
        self.assertEqual(
            report["failure_summary"]["by_likely_pass"]["translucent-rounded-rect"],
            1)
        self.assertEqual(
            report["failure_summary"]["by_path"][failure["path"]],
            1)

    def test_material_plan_contract_version_mismatch_is_llm_actionable(self) -> None:
        plan = material_plan()
        plan["contract_version"] = 2

        code, report = self.run_verifier(snapshot(plan))

        self.assertEqual(code, 1)
        failure = next(
            item for item in report["failures"]
            if item["name"] == "material plan contract version is supported")
        self.assertEqual(
            failure["path"],
            "debug.platform_runtime.details.renderer.material_plans[0]"
            ".contract_version")
        self.assertEqual(failure["expected"], 1)
        self.assertEqual(failure["actual"], 2)
        self.assertEqual(failure["likely_layer"], "material.regular.fallback")
        self.assertIn("MaterialPlan schema version", failure["hint"])
        self.assertIn("plan_material_surface", failure["suggested_action"])
        self.assertEqual(
            report["material_plans"]["contract_versions"],
            {"2": 1})

    def test_manifest_can_require_plan_sample_and_runtime_pass_bounds(self) -> None:
        manifest = {
            "require_material_resource_bounds": {
                "max_plan_sample_taps_lte": 25,
                "max_plan_sample_taps_gte": 25,
                "total_runtime_passes_lte": 1,
                "total_runtime_passes_gte": 1,
                "active_runtime_passes_lte": 1,
                "active_runtime_passes_gte": 1,
                "backdrop_runtime_passes_lte": 0,
                "backdrop_runtime_passes_gte": 0,
                "max_pass_texture_copy_pixels_lte": 0,
                "total_pass_texture_copy_pixels_lte": 0,
            }
        }
        code, report = self.run_verifier(
            snapshot(material_plan(sample_taps=25, primary_sample_taps=25)),
            manifest)

        self.assertEqual(code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(
            report["material_plans"]["resource_bounds"]["max_plan_sample_taps"],
            25)

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
                "contract_versions": {"1": 1},
                "backdrop_sources": {"none": 1},
                "luminance_responses": {"not-sampled": 1},
                "luminance_adapted": 0,
                "render_target_ready": 1,
                "render_target_within_backdrop_budget": 1,
                "render_target_pixel_formats": {"rgba8unorm": 1},
                "pass_executors": {"fallback-fill": 1},
                "decision_can_sample_backdrop": 0,
                "decision_backend_supports_backdrop": 0,
                "decision_backdrop_source_ready": 0,
                "decision_blockers": {"unsupported-backend": 1},
                "verifier_require_backdrop_source": 0,
                "verifier_require_edge_highlight": 0,
                "verifier_profiles": {"regular-legibility-backdrop": 1},
                "verifier_region_layers": {
                    "regular-legibility-backdrop": "material-fallback-pass",
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
            report["material_plans"]["render_target"]["pixel_formats"],
            {"rgba8unorm": 1})
        self.assertEqual(
            report["material_plans"]["decision_trace"]["first_blockers"],
            {"unsupported-backend": 1})
        self.assertEqual(
            report["material_plans"]["verifier_profiles"],
            {"regular-legibility-backdrop": 1})
        self.assertEqual(
            report["material_plans"]["region_layers"],
            {"regular-legibility-backdrop": "material-fallback-pass"})

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


if __name__ == "__main__":
    unittest.main()

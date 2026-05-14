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
    "edge_energy",
    "luma_delta",
    "luma_mean",
    "luma_stddev",
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

ALLOWED_MATERIAL_PASS_NAMES = {
    "backdrop-sample-blur",
    "none",
    "translucent-rounded-rect",
}


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
            self.data["failure_summary"] = {
                "count": len(failures),
                "by_likely_layer": by_layer,
                "by_likely_pass": by_pass,
                "by_path": by_path,
            }
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


def bool_at(value: JsonObject, key: str) -> bool | None:
    item = value.get(key)
    return item if isinstance(item, bool) else None


def string_at(value: JsonObject, key: str) -> str | None:
    item = value.get(key)
    return item if isinstance(item, str) else None


def number_at(value: JsonObject, key: str) -> int | float | None:
    item = value.get(key)
    return item if isinstance(item, (int, float)) and not isinstance(item, bool) else None


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
    likely_layer: str,
    likely_pass: str = "",
    hint: str,
) -> int | float | None:
    item = value.get(key)
    ok = isinstance(item, (int, float)) and not isinstance(item, bool)
    if ok and min_value is not None:
        ok = float(item) >= min_value
    expected: Any = "number"
    if min_value is not None:
        expected = {"type": "number", ">=": min_value}
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
        "fallback_paths",
        "kinds",
        "pass_names",
    }
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise ValueError(
            "unknown require_material_plan_summary keys: "
            + ", ".join(unknown))
    spec: JsonObject = {}
    for field in ("count", "min_count", "fallback", "backdrop_sampling"):
        if field in value:
            spec[field] = non_negative_int(
                value[field],
                f"require_material_plan_summary.{field}")
    map_vocabularies = {
        "fallback_paths": ALLOWED_MATERIAL_FALLBACK_PATHS,
        "kinds": ALLOWED_MATERIAL_KINDS,
        "pass_names": ALLOWED_MATERIAL_PASS_NAMES,
    }
    for field, allowed_keys in map_vocabularies.items():
        if field in value:
            spec[field] = string_int_map(
                value[field],
                f"require_material_plan_summary.{field}",
                allowed_keys)
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
        "max_budget_blur_radius_lte",
        "max_sample_taps_lte",
        "max_pass_count_lte",
        "max_backdrop_pixels_lte",
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
        args.require_capability.extend(
            list_of_strings(manifest, "require_capabilities"))
        runtime_details = manifest.get("require_runtime_details", [])
        if runtime_details is None:
            runtime_details = []
        if not isinstance(runtime_details, list):
            raise ValueError("require_runtime_details must be a list")
        args.require_runtime_detail.extend(
            runtime_detail_spec_from_manifest(entry) for entry in runtime_details)

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

    except ValueError as exc:
        report.check("manifest schema is valid", False, str(exc))
        return False

    report.data["manifest"] = {
        "path": str(manifest_path),
        "name": manifest.get("name"),
        "pixel_regions": len(manifest.get("pixel_regions", []) or []),
        "pixel_region_metrics": len(manifest.get("pixel_region_metrics", []) or []),
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
        "alpha_max": alpha_max,
        "alpha_min": alpha_min,
        "edge_energy": round(edge_sum / edge_count, 3) if edge_count else 0.0,
        "height": h,
        "luma_delta": round(luma_max - luma_min, 3),
        "luma_max": round(luma_max, 3),
        "luma_mean": round(luma_mean, 3),
        "luma_min": round(luma_min, 3),
        "luma_stddev": round(luma_variance ** 0.5, 3),
        "sampled_pixels": sampled,
        "unique_colors": len(unique_colors),
        "width": w,
        "x": x,
        "y": y,
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
        kind = material.get("kind")
        if isinstance(kind, str):
            material_kinds = summary["material_kinds"]
            material_kinds[kind] = material_kinds.get(kind, 0) + 1
        verifier_profile = material.get("verifier_profile")
        if isinstance(verifier_profile, str):
            profiles = summary["material_verifier_profiles"]
            profiles[verifier_profile] = profiles.get(verifier_profile, 0) + 1
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
        "material_fallback_nodes": 0,
        "material_kinds": {},
        "material_nodes": 0,
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
    "kind",
    "plan_id",
    "geometry",
    "opacity",
    "blur_radius",
    "tint",
    "saturation",
    "luminance_floor",
    "luminance_gain",
    "edge_highlight",
    "edge_width",
    "noise_opacity",
    "shadow_alpha",
    "shadow_radius",
    "backdrop_sampling",
    "fallback",
    "fallback_path",
    "fallback_reason",
    "debug_seed",
    "sample_taps",
    "quality_policy",
    "primary_pass",
    "resource_budget",
    "verifier",
    "passes",
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

MATERIAL_GEOMETRY_FIELDS = ("x", "y", "w", "h", "radius")
MATERIAL_TINT_FIELDS = ("r", "g", "b", "a")
MATERIAL_VERIFIER_FIELDS = (
    "require_backdrop_source",
    "require_edge_highlight",
    "min_luma_delta",
    "min_unique_colors",
    "region_name",
    "likely_layer",
)
MATERIAL_PASS_FIELDS = (
    "name",
    "active",
    "requires_backdrop",
    "sample_taps",
    "likely_layer",
)
MATERIAL_QUALITY_POLICY_BOOL_FIELDS = (
    "allow_backdrop_sampling",
    "allow_noise",
    "allow_shadow",
)
MATERIAL_QUALITY_POLICY_NUMBER_FIELDS = (
    "max_blur_radius",
    "max_sample_taps",
)


def summarize_material_plans(plans: Any, report: Report, path: str) -> JsonObject:
    summary: JsonObject = {
        "count": 0,
        "fallback": 0,
        "backdrop_sampling": 0,
        "fallback_paths": {},
        "kinds": {},
        "pass_names": {},
        "plan_ids": [],
        "region_layers": {},
        "verifier_profiles": {},
        "resource_bounds": {
            "max_plan_blur_radius": 0.0,
            "max_plan_sample_taps": 0,
            "max_budget_blur_radius": 0.0,
            "max_sample_taps": 0,
            "max_pass_count": 0,
            "max_backdrop_pixels": 0,
            "unbounded_texture_copy": 0,
            "non_deterministic_fallback": 0,
        },
        "quality_policy": {
            "backdrop_sampling_disabled": 0,
            "noise_disabled": 0,
            "shadow_disabled": 0,
            "max_blur_radius": 0.0,
            "max_sample_taps": 0,
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
        plan_id = plan.get("plan_id")
        if isinstance(plan_id, str):
            summary["plan_ids"].append(plan_id)
        likely_layer = plan_id if isinstance(plan_id, str) and plan_id else "material-plan"

        plan_blur_radius: int | float | None = None
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
        check_bool_field(
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

        geometry = check_object_field(
            report,
            plan,
            "geometry",
            plan_path,
            likely_layer=likely_layer,
            hint="MaterialPlan.geometry should be serialized from the pure request.")
        if geometry is not None:
            for key in MATERIAL_GEOMETRY_FIELDS:
                check_number_field(
                    report,
                    geometry,
                    key,
                    f"{plan_path}.geometry",
                    likely_layer=likely_layer,
                    hint="Check MaterialPlan geometry serialization.")

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

        verifier = check_object_field(
            report,
            plan,
            "verifier",
            plan_path,
            likely_layer=likely_layer,
            hint="The pure plan should provide verifier expectations.")
        if verifier is not None:
            for key in MATERIAL_VERIFIER_FIELDS:
                if key.startswith("require_"):
                    check_bool_field(
                        report,
                        verifier,
                        key,
                        f"{plan_path}.verifier",
                        likely_layer=likely_layer,
                        hint="Verifier expectation booleans must stay explicit.")
                elif key in ("region_name", "likely_layer"):
                    check_string_field(
                        report,
                        verifier,
                        key,
                        f"{plan_path}.verifier",
                        likely_layer=likely_layer,
                        hint="Verifier expectations must name the failing region/layer.")
                else:
                    check_number_field(
                        report,
                        verifier,
                        key,
                        f"{plan_path}.verifier",
                        min_value=0.0,
                        likely_layer=likely_layer,
                        hint="Verifier numeric thresholds must be non-negative.")
            region_name = string_at(verifier, "region_name")
            region_layer = string_at(verifier, "likely_layer")
            if region_name:
                profiles = summary["verifier_profiles"]
                profiles[region_name] = profiles.get(region_name, 0) + 1
            if region_name and region_layer:
                summary["region_layers"][region_name] = region_layer

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

        resource_budget = check_object_field(
            report,
            plan,
            "resource_budget",
            plan_path,
            likely_layer=likely_layer,
            hint="Material plans must expose bounded resource policy for CI debugging.")
        max_pass_count: int | float | None = None
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

        primary_pass = check_object_field(
            report,
            plan,
            "primary_pass",
            plan_path,
            likely_layer=likely_layer,
            hint="The pure plan should name the backend pass it expects.")
        primary_pass_sample_taps: int | float | None = None
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
                else:
                    check_string_field(
                        report,
                        primary_pass,
                        key,
                        f"{plan_path}.primary_pass",
                        likely_layer=likely_layer,
                        hint="Material pass must name its layer for debugging.")
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
                for key in MATERIAL_PASS_FIELDS:
                    if key in ("active", "requires_backdrop"):
                        check_bool_field(
                            report,
                            pass_entry,
                            key,
                            pass_path,
                            likely_layer=likely_layer,
                            likely_pass=string_at(pass_entry, "name") or primary_pass_name,
                            hint="Pass entries must expose runtime activation state.")
                    elif key == "sample_taps":
                        check_number_field(
                            report,
                            pass_entry,
                            key,
                            pass_path,
                            min_value=0.0,
                            likely_layer=likely_layer,
                            likely_pass=string_at(pass_entry, "name") or primary_pass_name,
                            hint="Pass sample taps must be numeric.")
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
            if isinstance(primary_pass, dict):
                report.check(
                    "material pass list includes primary pass",
                    any(
                        isinstance(entry, dict)
                        and entry.get("name") == primary_pass.get("name")
                        and entry.get("active") == primary_pass.get("active")
                        and entry.get("requires_backdrop") == primary_pass.get(
                            "requires_backdrop")
                        and entry.get("sample_taps") == primary_pass.get("sample_taps")
                        and entry.get("likely_layer") == primary_pass.get(
                            "likely_layer")
                        for entry in passes
                    ),
                    path=f"{plan_path}.passes",
                    expected=primary_pass,
                    actual=passes,
                    likely_layer=likely_layer,
                    likely_pass=primary_pass_name,
                    hint="Runtime pass details should include the primary pass unchanged.",
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
    for field in ("fallback", "backdrop_sampling"):
        if field in spec:
            report.check(
                f"material plan summary {field} matches",
                summary.get(field) == spec[field],
                path=f"{base_path}.{field}",
                expected=spec[field],
                actual=summary.get(field),
                likely_layer="platform-runtime",
                hint="Inspect fallback_path and primary_pass for each material plan.")
    for field in ("fallback_paths", "kinds", "pass_names"):
        if field in spec:
            actual = summary.get(field)
            report.check(
                f"material plan summary {field} matches",
                actual == spec[field],
                path=f"{base_path}.{field}",
                expected=spec[field],
                actual=actual,
                likely_layer="platform-runtime",
                hint="Inspect renderer.material_plans[] for the unexpected plan kind/pass/fallback.")


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
        "max_budget_blur_radius_lte": "max_budget_blur_radius",
        "max_sample_taps_lte": "max_sample_taps",
        "max_pass_count_lte": "max_pass_count",
        "max_backdrop_pixels_lte": "max_backdrop_pixels",
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
            hint="Inspect MaterialPlan.sample_taps in the resolved plans.")
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

    for key in ("event", "source", "detail", "result", "caret_rect"):
        report.check(f"input_debug contains {key}", key in input_debug)

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
        elif args.require_pixel_region:
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
                if material_plan_summary is not None:
                    layers = material_plan_summary.get("region_layers")
                    if isinstance(layers, dict):
                        region_layers = layers
                likely_layer = str(region_layers.get(
                    name,
                    "material-or-backdrop-pass"))
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
            if material_plan_summary is not None:
                layers = material_plan_summary.get("region_layers")
                if isinstance(layers, dict):
                    likely_layer = str(layers.get(region_name, likely_layer))
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
                    hint=(
                        "For blur probes, inspect renderer.material_plans, "
                        "the backdrop sample pass, and whether the manifest "
                        "region avoids foreground text."))

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
            "numeric bound. METRIC is one of edge_energy, luma_delta, "
            "luma_mean, luma_stddev, unique_colors. BOUND is gte or lte. "
            "Repeatable."))
    parser.set_defaults(
        require_material_plan_summary=None,
        require_material_resource_bounds=None,
        require_material_quality_policy=None,
        require_material_semantic_runtime_match=False)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    return verify(parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

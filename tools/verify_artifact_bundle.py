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


class Report:
    def __init__(self, bundle: Path) -> None:
        self.data: JsonObject = {
            "ok": False,
            "bundle": str(bundle),
            "checks": [],
            "errors": [],
            "warnings": [],
        }

    def check(self, name: str, condition: bool, detail: str = "") -> bool:
        self.data["checks"].append({
            "name": name,
            "ok": condition,
            "detail": detail,
        })
        if not condition:
            message = name if not detail else f"{name}: {detail}"
            self.data["errors"].append(message)
        return condition

    def warn(self, message: str) -> None:
        self.data["warnings"].append(message)

    def finish(self) -> int:
        self.data["ok"] = len(self.data["errors"]) == 0
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
    return item if isinstance(item, (int, float)) else None


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

    except ValueError as exc:
        report.check("manifest schema is valid", False, str(exc))
        return False

    report.data["manifest"] = {
        "path": str(manifest_path),
        "name": manifest.get("name"),
        "pixel_regions": len(manifest.get("pixel_regions", []) or []),
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
            a = data[offset + 3] if bytes_per_pixel == 4 else 255
            luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
            luma_min = min(luma_min, luma)
            luma_max = max(luma_max, luma)
            alpha_min = min(alpha_min, a)
            alpha_max = max(alpha_max, a)
            if len(unique_colors) < 4096:
                unique_colors.add((r, g, b, a))
            sampled += 1

    return {
        "alpha_max": alpha_max,
        "alpha_min": alpha_min,
        "height": h,
        "luma_delta": round(luma_max - luma_min, 3),
        "luma_max": round(luma_max, 3),
        "luma_min": round(luma_min, 3),
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
            f"expected {expected!r}, got {actual!r}")

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
                report.check(
                    f"pixel region is non-empty: {name}",
                    region["sampled_pixels"] > 0 and region["width"] > 0 and region["height"] > 0,
                    repr(rect))
                report.check(
                    f"pixel region has luma contrast: {name}",
                    float(region["luma_delta"]) >= min_delta,
                    f"expected >= {min_delta}, got {region['luma_delta']}")
                report.check(
                    f"pixel region has color variety: {name}",
                    int(region["unique_colors"]) >= min_unique,
                    f"expected >= {min_unique}, got {region['unique_colors']}")
            except ValueError as exc:
                report.check(f"pixel region spec is valid: {spec}", False, str(exc))
        if pixel_regions:
            report.data["pixel_regions"] = pixel_regions

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
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    return verify(parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

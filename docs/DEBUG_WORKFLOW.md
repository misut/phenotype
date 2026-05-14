# Unified Debug Workflow

phenotype now exposes one shared debug plane across native and WASI targets.
Platform adapters extend the same common model instead of inventing separate
debug outputs per backend.

## Common model

Every diagnostics snapshot keeps the same top-level `debug` object:

- `platform_capabilities`
- `input_debug`
- `semantic_tree`
- `platform_runtime`

`platform_capabilities` is explicit. Unsupported features are reported as
`false`, not omitted. The shared fields currently cover:

- `read_only`
- `snapshot_json`
- `capture_frame_rgba`
- `write_artifact_bundle`
- `semantic_tree`
- `input_debug`
- `platform_runtime`
- `frame_image`
- `platform_diagnostics`

The common snapshot schema remains the source of truth for all platforms:

- `input_debug` reports the last input event, caret geometry, and composition state.
- `semantic_tree` serializes the same accessibility-oriented semantic tree on every target.
  Root-level overlays are included in paint/focus order and keep screen-fixed
  visibility semantics, so dialogs, popovers, and material probes cannot vanish
  from artifact debugging when the document underneath is scrolled.
- `platform_runtime` always includes the shared viewport/scroll/focus state plus a
  platform-specific `details` object.

`phenotype_diag_export()` remains the WASI export surface for the snapshot JSON.

## Artifact bundles

`write_artifact_bundle(directory, reason)` uses one shared bundle layout:

```text
<directory>/
  snapshot.json
  frame.bmp                      # optional
  platform/
    <platform>-runtime.json      # optional
```

Rules:

- `snapshot.json` always uses the common snapshot schema.
- `frame.bmp` is written only when `capture_frame_rgba` succeeds.
- `platform/<platform>-runtime.json` mirrors `platform_runtime.details` and adds
  `artifact_reason` when a reason is supplied.

## Desktop example artifact hook

GLFW-based native examples can write a startup artifact bundle without adding
example-specific code. Set `PHENOTYPE_ARTIFACT_DIR` before launching the
example:

```sh
cd examples/native
PHENOTYPE_ARTIFACT_DIR=/tmp/phenotype-native-startup \
PHENOTYPE_ARTIFACT_REASON=native-startup \
PHENOTYPE_ARTIFACT_EXIT=1 \
mise exec -- exon run
```

For automation, build first and execute the generated binary directly so the
artifact capture and process exit are not hidden behind the run wrapper:

```sh
cd examples/native
mise exec -- exon build
PHENOTYPE_ARTIFACT_DIR=/tmp/phenotype-native-startup \
PHENOTYPE_ARTIFACT_REASON=native-startup \
PHENOTYPE_ARTIFACT_EXIT=1 \
.exon/debug/native
```

`PHENOTYPE_ARTIFACT_EXIT=1` makes the example exit after writing the first
rendered frame, which is useful for CI or LLM debugging. Omit it for manual
inspection; the bundle is still written after the initial render and the window
continues running. The same hook applies to `examples/flight_board` and
`examples/workbook`. `examples/glass_showcase` uses the same route for
material-focused checks, including macOS sampled-backdrop rendering and
fallback metadata.

Material frames also expose a resolved backend plan. Semantic material nodes
describe the stable request (`kind`, opacity, blur intent, contrast intent, and
fallback availability), while `debug.platform_runtime.details.renderer` records
the actual `material_plans` executed for the frame. Each plan includes:

- `plan_id`, `kind`, geometry, tint, blur radius, saturation, luminance curve,
  edge highlight, noise, and shadow values;
- `backdrop_sampling`, `fallback`, `fallback_path`, and `fallback_reason`;
- `primary_pass`, `resource_budget`, and the pass list the backend attempted,
  including the likely layer name;
- verifier expectations for region checks.

When debugging a material failure, read the semantic node first to confirm the
UI emitted the expected material surface, then inspect
`renderer.material_plans[]` to see whether the backend ran the glass pass or a
deterministic fallback.
macOS writes sampled-backdrop plans when the previous frame capture is ready.
Windows and Android write the same plan schema with `fallback_path:
unsupported-backend` and `primary_pass.name: translucent-rounded-rect`.
Snapshot-only WASI/stub runtimes expose an empty renderer material contract
with `material_fallback_policy` so the absence of per-command execution is
machine-readable.

## Artifact verification

From the repo root, use `tools/verify_artifact_bundle.py` to validate the
bundle before handing it to an LLM, attaching it to an issue, or comparing it
in CI. The verifier checks the common debug schema, platform capabilities,
semantic tree shape, runtime viewport, platform diagnostics files, and
`frame.bmp` header/size invariants. For visual smoke checks, repeat
`--require-pixel-region` to require a named frame region to have minimum
luminance contrast and color variety. Manifest-driven gates can additionally
set `pixel_region_metrics` bounds for region metrics such as `edge_energy` and
`luma_stddev`; use those for blur-specific probes where a material region
should smooth high-frequency backdrop detail without becoming visually blank.
Use `--require-runtime-detail PATH=JSON` when a backend-specific contract must prove a value under
`debug.platform_runtime.details`.

Verifier failures are structured for automated diagnosis. Each failed check
adds an entry to `failures[]` with the JSON path or frame region, expected
value, actual value, likely layer/pass, and a short hint. The top-level
`failure_summary` groups failures by likely layer and JSON path so an LLM can
jump to the most likely source first. Use `--require-material-plan` when a
bundle must contain resolved material plans.

Manifests can also set `require_material_plan_summary` to assert the resolved
material aggregate, not just the per-plan schema. Supported keys are `count`,
`min_count`, `fallback`, `backdrop_sampling`, and exact count maps for
`fallback_paths`, `kinds`, and `pass_names`. This catches policy drift such as a
glass scene silently switching from backdrop blur to fallback, or a fallback
backend reporting the wrong deterministic pass.

Use `require_material_resource_bounds` when a material gate must prove the
runtime stayed within the pure plan's performance budget. Supported limits are
`max_plan_blur_radius_lte`, `max_budget_blur_radius_lte`,
`max_sample_taps_lte`, `max_pass_count_lte`, and
`max_backdrop_pixels_lte`; `require_bounded_texture_copy` and
`require_deterministic_fallback` require those booleans to hold for every
resolved plan.

Set `require_material_semantic_runtime_match` when a gate must prove that the
semantic material nodes and the backend runtime material plans describe the
same material surfaces. The verifier compares the material node count and
material kind/profile maps from `debug.semantic_tree` against
`debug.platform_runtime.details.renderer.material_plans#summary`; mismatches
point at the material contract layer so the likely break is not confused with a
pure pixel-capture failure.

```sh
tools/verify_artifact_bundle.py /tmp/phenotype-native-startup \
  --expect-platform macos \
  --require-frame \
  --require-label "Control States" \
  --require-label "Material Surface" \
  --require-label "Paint Command Showcase" \
  --require-role button \
  --require-role material \
  --require-role text_field \
  --require-disabled-count 2 \
  --require-material-kind regular \
  --require-material-fallback \
  --require-material-plan
```

For the material-focused showcase, require every public material kind:

```sh
tools/verify_artifact_bundle.py /tmp/phenotype-glass-showcase \
  --expect-platform macos \
  --manifest examples/glass_showcase/artifact_manifest.json
```

Or run the local gate, which builds the example, launches the startup artifact
hook, and applies the manifest in one command:

```sh
tools/verify_glass_showcase_artifact.sh
```

The command emits a deterministic JSON report and exits non-zero when an
invariant fails.

The same gate is wired into the macOS native CI job, so pull requests fail if
the committed glass showcase manifest no longer matches the startup frame or
semantic material contract. Windows artifact automation and Android CI wiring
remain future work.

## macOS extensions

The macOS adapter keeps the shared top-level schema and extends
`platform_runtime.details` with:

- `renderer`
  - initialization state
  - drawable size
  - last rendered size
  - last-frame/readback availability
  - resolved `material_plans`
- `images`
  - pending/completed queue counts
  - worker state
  - remote image entries with `url`, `state`, and `failure_reason`
- `text_input`
  - system caret state
  - composition state
  - scroll-tracking visibility context

Frame capture uses the Metal renderer path:

- `CAMetalLayer.framebufferOnly = false`
- the rendered drawable is copied into a debug capture texture
- `capture_frame_rgba()` blits that texture into a CPU-readable buffer and
  converts BGRA to RGBA

## WASI behavior

WASI exports the same debug snapshot schema through `phenotype_diag_export()`.
Its capabilities are explicit:

- `platform = "wasi"`
- `snapshot_json = true`
- `write_artifact_bundle = true`
- `capture_frame_rgba = false`
- `frame_image = false`
- `platform_diagnostics = false`

WASI-specific runtime details currently report:

- `host_model = "wasi"`
- `frame_capture_supported = false`
- `artifact_bundle_support = "snapshot-only"`

When the host provides a writable preopened directory, WASI artifact bundles
write:

- `snapshot.json`
- `platform/wasi-runtime.json`

No frame image is produced on WASI today.

The default `exon test --target wasm32-wasi` runner does not currently preopen
a writable directory for filesystem assertions, so the bundle test degrades to a
skip in that environment. A direct `wasmtime --dir=.` invocation exercises the
full bundle-writing path.

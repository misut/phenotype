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
- `material_surfaces`
- `material_backdrop_blur`
- `reduce_transparency`
- `increase_contrast`
- `reduce_motion`

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
The macOS native backend reads accessibility display preferences from
`NSWorkspace` by default. For deterministic artifact capture, set
`PHENOTYPE_ACCESSIBILITY_DISPLAY=standard` to disable those inputs, or set
`PHENOTYPE_ACCESSIBILITY_DISPLAY=reduce-transparency,increase-contrast,reduce-motion`
with any subset of tokens to exercise the pure accessibility downgrade paths.
`tools/verify_glass_showcase_artifact.sh` defaults to `standard` unless the
caller supplies a different value.

Material frames also expose a resolved backend plan. Semantic material nodes
describe the stable request (`kind`, opacity, blur intent, contrast intent, and
fallback availability), while `debug.platform_runtime.details.renderer` records
the actual `material_plans` executed for the frame. Each plan includes:

- `contract_version`, `plan_id`, `kind`, geometry, tint, blur radius,
  saturation, luminance curve, edge highlight, noise, and shadow values;
- `render_target`, including target dimensions, scale, pixel format, pixel
  count, readiness, and whether the backdrop-pixel budget was satisfied;
- `decision_trace`, including the pure gate booleans for geometry, quality,
  backend capabilities, backdrop-source readiness, reduced transparency,
  increased contrast, reduced motion, and the first blocker that explains the
  fallback path;
- `backdrop_sampling`, `fallback`, `fallback_path`, and `fallback_reason`;
- `backdrop`, including source, readiness flags, sanitized luminance
  statistics, luminance response, and floor/gain/edge deltas;
- `quality_policy`, `primary_pass`, `resource_budget`, and the pass list the
  backend attempted, including the likely layer name, pure executor role, and
  maximum texture-copy pixels;
- verifier expectations for region checks.

When debugging a material failure, read the semantic node first to confirm the
UI emitted the expected material surface, then inspect
`renderer.material_plans[]` to see whether the backend ran the glass pass or a
deterministic fallback.
`sample_taps` and `primary_pass.sample_taps` describe the actual resolved pass.
Fallback plans therefore report `0` taps, while `quality_policy.max_sample_taps`
and `resource_budget.max_sample_taps` preserve the allowed upper bound that led
to the decision. Reduced-motion plans disable material noise and cap backdrop
sample taps before a backend executes the pass; increased-contrast plans raise
opacity and luminance legibility in the same pure layer.
`primary_pass.executor` and each `passes[].executor` use pure roles:
`backdrop-filter` for sampled glass, `fallback-fill` for deterministic fallback,
and `none` for inactive material work. `max_texture_copy_pixels` is non-zero
only for backdrop passes and must not exceed `render_target.pixel_count`.
`decision_trace.can_sample_backdrop` must match `backdrop_sampling`, and
`decision_trace.first_blocker` must match `fallback_path`; a mismatch usually
means the backend serialized stale policy metadata or skipped the pure planner.
The verifier rejects unknown `contract_version` values before trusting
schema-specific fields, so artifact-schema drift becomes an explicit CI failure
instead of a silent debug-plane mismatch.
The renderer contract also publishes `material_plan_contract_version` next to
`material_plans[]`, so snapshot-only or empty material renderers still expose the
same artifact schema version without requiring a per-plan object.
The adjacent `verifier` object is also derived from the same plan:
`require_backdrop_source` mirrors `backdrop_sampling`,
`require_edge_highlight` is true only for non-fallback plans with a positive
edge highlight, and `likely_layer` must match `primary_pass.likely_layer`.
Manifests can pin `verifier_profiles` and `verifier_region_layers` in
`require_material_plan_summary` so a pixel-region failure reports the expected
region contract and layer before anyone opens the frame visually.
`backdrop.luminance_response` is `not-sampled` for fallback plans and one of
`neutral`, `dark`, `bright`, `flat`, `dark-flat`, or `bright-flat` for sampled
plans. The adjacent delta fields show whether the pure planner actually changed
the luminance floor, luminance gain, or edge highlight for that backdrop.
macOS writes sampled-backdrop plans when the previous frame capture is ready.
Windows and Android write the same plan schema with `fallback_path:
unsupported-backend` and `primary_pass.name: translucent-rounded-rect`.
Snapshot-only WASI/stub runtimes expose an empty renderer material contract
with `material_fallback_policy` so the absence of per-command execution is
machine-readable.

## Artifact verification

From the repo root, run the verifier through the uv-managed Python environment:
`mise exec -- uv run --frozen python tools/verify_artifact_bundle.py ...`.
`mise.toml` pins Python and uv, while `pyproject.toml`/`uv.lock` define the
Python tool environment. Use `mise run tools:artifact:test` for the verifier's
contract tests and `mise run tools:artifact:pycompile` for syntax checks.
Use the verifier command to validate the bundle before handing it to an LLM,
attaching it to an issue, or comparing it in CI. The verifier checks the common
debug schema, platform capabilities,
semantic tree shape, runtime viewport, platform diagnostics files, and
`frame.bmp` header/size invariants. For visual smoke checks, repeat
`--require-pixel-region` to require a named frame region to have minimum
luminance contrast and color variety, then repeat
`--require-pixel-region-metric REGION:METRIC:BOUND:VALUE` when a direct CLI
repro needs metric bounds. Manifest-driven gates use the same
`pixel_region_metrics` checks for region metrics such as `edge_energy` and
`luma_stddev`; use those for blur-specific probes where a material region
should smooth high-frequency backdrop detail without becoming visually blank.
Manifests can also set `pixel_region_metric_comparisons` to compare one named
region against another by ratio. The glass showcase uses this to prove the
visible blur probe stays smoother than the nearby backdrop reference, which
catches a blur pass that renders nonblank pixels but stops filtering the
backdrop.
Use `--require-runtime-detail PATH=JSON` when a backend-specific contract must prove a value under
`debug.platform_runtime.details`.

Verifier failures are structured for automated diagnosis. Each failed check
adds an entry to `failures[]` with the JSON path or frame region, expected
value, actual value, likely layer/pass, a short hint, and a deterministic
`suggested_action` string. The top-level `failure_summary` groups failures by
likely layer, likely pass, JSON path, and suggested action so an LLM can jump
to the most likely source first. It also repeats the first actionable failure
as `first_failure` and publishes `top_likely_layer`, `top_likely_pass`, and
`top_suggested_action` when those hints are available, which lets CI logs
identify the first file/pass family to inspect without scanning every check.
The report also includes `artifact_context`, and failed runs copy that compact
context into `failure_summary.artifact_context`. It records the platform,
backend, viewport, semantic material counts, renderer material plan count,
renderer material plan contract version, resolved plan count, fallback paths,
pass executors, first decision blockers, and accessibility decision counts so
CI logs can explain which semantic/runtime contract surface drifted before
opening the full bundle.
On macOS, also inspect
`debug.platform_runtime.details.renderer.accessibility_display_options`; it
records whether the frame used live system settings, `standard` gate settings,
or an explicit environment override before the pure planner produced
`decision_trace.reduced_transparency`, `decision_trace.increase_contrast`, and
`decision_trace.reduce_motion`.
Plan-level failures route to `plan_material_surface` and runtime plan
serialization; semantic/runtime contract failures route to semantic material
nodes, `MaterialRect` command emission, and `renderer.material_plans[]` parity.
Use `--require-material-plan` when a bundle must contain resolved material
plans.

Manifests can also set `require_material_plan_summary` to assert the resolved
material aggregate, not just the per-plan schema. Supported keys are `count`,
`min_count`, `fallback`, `backdrop_sampling`, `backdrop_available`,
`backdrop_stable`, `luminance_adapted`, `render_target_ready`,
`render_target_within_backdrop_budget`, `decision_can_sample_backdrop`,
`decision_backend_supports_backdrop`, `decision_backdrop_source_ready`,
`decision_reduced_transparency`, `decision_increase_contrast`,
`decision_reduce_motion`, and
exact count maps for `fallback_paths`, `fallback_reasons`, `kinds`,
`contract_versions`, `pass_names`, `backdrop_sources`, `luminance_responses`,
`render_target_pixel_formats`, `pass_executors`, `decision_blockers`,
`verifier_profiles`, and `verifier_region_layers`; it can also count
`verifier_require_backdrop_source` and `verifier_require_edge_highlight`. This
catches policy drift such as a glass scene silently switching from backdrop
blur to fallback, a fallback backend reporting the wrong deterministic pass, a
sampled scene losing its previous-frame backdrop source, a render target
exceeding the pure backdrop budget, a pass switching executor roles, a decision
trace naming the wrong blocker, an artifact emitting an unexpected material plan
schema version, verifier expectations pointing at the wrong region/layer, or a
quality/capability downgrade losing its LLM-actionable reason string.

The plan schema check also treats `primary_pass` as a runtime contract. Its
sample-tap count must match the plan, and the backend `passes[]` list must
include the same pass entry. When this fails, inspect the pass serializer before
changing material policy. Fallback metadata is similarly strict: fallback plans
must report a non-empty `fallback_reason`, while non-fallback plans must leave
`fallback_reason` empty so stale downgrade explanations cannot leak into glass
artifacts.
The verifier also treats material kind, fallback path, and material pass names
as fixed vocabularies. Current fallback paths are `none`, `no-material`,
`invalid-geometry`, `unsupported-backend`, `no-backdrop-source`,
`reduced-transparency`, and `quality-policy`; current pass names are `none`,
`backdrop-sample-blur`, and `translucent-rounded-rect`. Add new planner/backend
vocabulary to the verifier in the same patch that introduces it, so CI reports
schema drift before a human has to infer it visually.

Use `require_material_resource_bounds` when a material gate must prove the
runtime stayed within the pure plan's performance budget. Supported limits are
`max_plan_blur_radius_lte`, `max_plan_sample_taps_lte`,
`max_plan_sample_taps_gte`, `max_budget_blur_radius_lte`,
`max_sample_taps_lte`, `max_pass_count_lte`, `max_backdrop_pixels_lte`,
`max_pass_texture_copy_pixels_lte`/`gte`,
`total_pass_texture_copy_pixels_lte`/`gte`,
`total_runtime_passes_lte`/`gte`, `active_runtime_passes_lte`/`gte`, and
`backdrop_runtime_passes_lte`/`gte`;
`require_bounded_texture_copy` and
`require_deterministic_fallback` require those booleans to hold for every
resolved plan. The runtime pass limits are aggregated from
`renderer.material_plans[].passes` and should describe the actual executor pass
list, not only the pure resource budget. Backends also serialize
`renderer.material_runtime_summary`; the verifier recomputes the same counters
from `renderer.material_plans[]` and reports the exact summary field if the
backend's view of executed material work drifts from the resolved plans.
Backends also serialize `renderer.material_executor_summary` for edge-only
work that cannot be derived from the pure plan, including material instance
count, fallback instance count, material draw calls, upload bytes/capacity,
framebuffer-history copy pixels, and CPU enqueue timings. Use
`require_runtime_numeric_bounds` for CI-safe limits on those numeric runtime
paths. Each entry names a path under `debug.platform_runtime.details` and can
provide `equals`, `gte`, and/or `lte`; failures report the exact path plus the
likely `material-executor` pass when the path targets the executor summary.
Whenever material plans are present, the verifier also cross-checks executor
counts against `renderer.material_plans#summary`: `plan_count`,
`fallback_instance_count`, and `material_instance_count` must match the
resolved plan aggregate, draw calls must stay within material instances times
the pure pass budget, upload bytes must fit the reported material buffer
capacity, and copied backdrop pixels must stay within the pure resource budget.
Use `require_material_quality_policy` when a material gate must prove the
resolved pure policy stayed enabled and bounded. It can require backdrop
sampling, noise, and shadow to remain allowed for every plan, and can bound the
maximum quality-policy blur radius, sample taps, and backdrop pixel budget seen
in the artifact. `max_backdrop_pixels_lte` is the pure planning limit for
backdrop copy/blur eligibility; compare it with
`renderer.material_executor_summary.backdrop_copy_pixels` when diagnosing how
many pixels the backend actually copied in a frame.

Set `require_material_semantic_runtime_match` when a gate must prove that the
semantic material nodes and the backend runtime material plans describe the
same material surfaces. The verifier compares the material node count and
material kind/profile maps from `debug.semantic_tree` against
`debug.platform_runtime.details.renderer.material_plans#summary`; mismatches
point at the material contract layer so the likely break is not confused with a
pure pixel-capture failure.

```sh
mise exec -- uv run --frozen python tools/verify_artifact_bundle.py /tmp/phenotype-native-startup \
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
mise exec -- uv run --frozen python tools/verify_artifact_bundle.py /tmp/phenotype-glass-showcase \
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

To validate the accessibility downgrade contract end to end, run the companion
gate:

```sh
tools/verify_glass_showcase_accessibility_artifact.sh
```

It launches the same scene with
`PHENOTYPE_ACCESSIBILITY_DISPLAY=reduce-transparency,increase-contrast,reduce-motion`
and verifies `examples/glass_showcase/artifact_manifest.accessibility.json`.
That manifest requires platform capabilities and renderer runtime details to
show the override, every material plan to fall back through
`reduced-transparency`, and the runtime executor summary to contain only
fallback material work.

The standard and accessibility gates are local acceptance checks for PR work.
Run them before changing material rendering, artifact manifests, or verifier
expectations. PR CI deliberately does not run the slow macOS glass showcase
capture; docs-only and tools-only PRs avoid the root C++ test matrix, docs
changes run the docs WASI build, and tooling changes run the verifier's Python
contract checks. The main-branch push workflow is the automated artifact safety
net and only runs artifact/docs build gates. WASI root tests and docs builds
run on Linux runners; macOS runners are reserved for the main-branch native
glass artifact gates. Workflow-file changes deliberately enable code, docs,
and tooling gates so runner policy edits validate themselves. Windows artifact
automation and Android CI wiring remain future work.

The Android manifest intentionally does not assert an exact
`render_target_within_backdrop_budget` count because physical devices and
emulators expose different startup frame sizes. It still records the field in
the verifier summary and requires deterministic `unsupported-backend` fallback
metadata.

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

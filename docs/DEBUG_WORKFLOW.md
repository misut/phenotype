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

- `input_debug` reports the last input event, hovered/focused/pressed callback
  state, caret geometry, and composition state.
- `semantic_tree` serializes the same accessibility-oriented semantic tree on every target.
  Root-level overlays are included in paint/focus order and keep screen-fixed
  visibility semantics, so dialogs, popovers, and material probes cannot vanish
  from artifact debugging when the document underneath is scrolled.
- `platform_runtime` always includes the shared viewport, scroll, focus, hover,
  and press state plus a platform-specific `details` object.
- Native desktop `platform_runtime.details.window` records the resolved window
  surface kind, requested `WindowOptions`, integrated titlebar metrics,
  native-control ownership, and `uses_glfw=false` / `toolkit_window_shim=false`
  so an artifact can prove whether the app is running through AppKit/Win32
  native chrome rather than guessing from a screenshot. macOS also records
  WindowServer bounds, onscreen state, z-order index, app-window order index,
  and front-window occlusion state, so a native app that owns a Dock icon but
  ordered a 0x0 window, stayed behind another app window, or failed to move into
  the active Space fails the artifact contract with an exact runtime path
  instead of a visual-only symptom. `visibility_state` and
  `ready_for_user_interaction` condense those platform readbacks into a single
  launch-health contract for direct-run failures.

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

`tools/phenotype_cli` now provides a read-only structural check for bundles:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli artifact summary --json /tmp/phenotype-glass-showcase
```

This command is structural only. Use `phenotype artifact verify` or
`phenotype observe --verify` when the bundle also needs semantic verifier
assertions. The Python verifier remains the reference implementation behind
that CLI edge while C++ verifier parity is still being built.

For LLM-actionable output observation, prefer the unified observe command:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli observe --json /tmp/phenotype-glass-showcase \
  --manifest ../../examples/glass_showcase/artifact_manifest.json \
  --expect-platform macos \
  --require-frame
```

`observe` parses `snapshot.json` in C++ and reports semantic tree presence,
platform capabilities, platform runtime details, material plan counts,
material kinds and roles, fallback paths/reasons, backdrop sources and capture
reasons, executor summary counts, frame/platform files, and likely
layer/pass hints. Supplying `--manifest` or `--verify` also runs the
uv-managed verifier and embeds its JSON report in the same envelope. Use this
as the first artifact triage command when a CI log or local bundle needs one
machine-readable explanation before deeper pixel-contract debugging.

When debugging the CLI/native input-output boundary itself, first check the
pure contract surface:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli theme contract --json
.exon/debug/phenotype_cli io contract --json
```

`theme contract` reports the `phenotype_theme_contract` version, Apple-like
glass profile, Pretendard typography baseline, Liquid Glass usage boundary,
macOS/Finder-style iconography policy, phenotype-owned SVG asset policy,
grouped-container policy, performance bounds, accessibility fallback policy,
unsupported-backend degradation policy, color tokens, radii, typography, and
semantic surface roles. It should stay green before debugging theme-dependent
artifact differences.

`io contract` reports the `phenotype.io` version, accepted input event kinds,
output observation kinds, deterministic replay sample, LLM-debuggable artifact
sample, edge-effect policy, and release-adapter bypass policy. It should stay
green before investigating an example-specific driver or renderer artifact.

The CLI also exposes the verifier through an edge wrapper:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli artifact verify --json /tmp/phenotype-glass-showcase \
  --manifest ../../examples/glass_showcase/artifact_manifest.json \
  --expect-platform macos
```

`artifact verify` runs the uv-managed reference verifier from the repository
root and forwards the verifier's JSON report. This keeps Python managed by
`mise`/`uv` while moving the developer-facing entry point under the CLI.

The slow local native gates are available through the same CLI surface:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli artifact verify-glass-showcase --json
.exon/debug/phenotype_cli artifact verify-glass-showcase --accessibility --json
.exon/debug/phenotype_cli artifact verify-file-explorer --json
```

`artifact verify-glass-showcase` now owns the glass example build, native
artifact capture, and uv-managed verifier call directly. Its JSON includes
build/run/verifier exit state, artifact bundle presence, manifest path,
expected platform, and accessibility display policy. `artifact
verify-file-explorer` owns the shared-model test, desktop/mobile builds,
deterministic native captures, and uv-managed verifier calls directly. Both
commands are local verification commands, not default PR CI jobs.

For file explorer workflow debugging that does not need a native window, use
the deterministic drive command:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli drive file-explorer --json \
  --input view:gallery \
  --input viewport:900x620@2 \
  --input select:README.txt \
  --input shortcut:duplicate \
  --input key:delete
.exon/debug/phenotype_cli drive file-explorer --json \
  --input select:Documents \
  --input key:enter \
  --input select:Project\ Notes.txt \
  --expect location:Demo\ Root/Documents \
  --expect selected:Project\ Notes.txt
```

The drive output reports the typed input trace, sandbox paths, visible entries,
viewport, view mode, pure Finder chrome/grid metrics, selection capabilities,
operation receipts with resolved operation plans, preview excerpt, and desktop
keyboard command descriptors.
It is useful for validating
select/open/read/create/duplicate/delete/view-mode/resize/shortcut and
keyboard-selection model behavior before running the slower desktop/mobile
artifact capture gate.
Each operation plan records the sandbox-relative source and destination,
whether the action reads a file, writes a file, creates a directory, moves to
Trash, permanently deletes from Trash, and which fallback reason blocked a
failed operation.
The parser accepts desktop-style aliases including `key:enter`, `key:delete`,
`key:escape`, `key:arrow-up`, `key:arrow-down`, `key:arrow-left`,
`key:arrow-right`, `key:home`, `key:end`, `key:page-up`, `key:page-down`,
`shortcut:find`, `shortcut:duplicate`, and `shortcut:new-folder`, matching the
native key-command registry used by the desktop example.
Native file explorer artifact bundles expose the same model state under
`debug.application.file_explorer`: profile, location, status, sort mode, view
mode, selected entry plus index, operation receipt, entry counts, pure chrome
metrics, and keyboard command descriptors. The operation receipt embeds the
same plan shape as CLI drive output, so a startup artifact can explain the file
effect contract without replaying the UI.
The desktop payload includes Finder chrome counts, sidebar symbol/label metrics,
selected-row radius plus soft selected-row alpha policy, the
`phenotype.icon_catalog` / `phenotype.icons` style contract
(`design_reference`, `asset_policy`, 24x24 alignment grid, stroke width,
total/sidebar/toolbar/filled symbol counts, SF Symbols rendering-mode names,
regular text-aligned weight policy, monochrome/hierarchical/palette/multicolor
capability counts, and
`interface_metaphor_policy`, `visual_consistency_policy`,
`toolbar_symbol_chrome_policy`, `sidebar_symbol_color_policy`,
`interaction_tone_policy` / `file_type_color_policy`, and
`metrics_policy` / `hit_target_policy`), traffic-light marker
coordinates, and icon-grid density metrics such as column width, row height,
pitch, thumbnail canvas size, label size, gap, visible rows, and visible
capacity. Column view also records location-pane row count, row height, and
icon size so Finder-style navigation rows can be checked from the artifact
without reading pixels by eye. Entry samples include the resolved
`symbol` and `symbol_semantic_reference_name`, so PDF/text/archive/image/movie
fallback mistakes can be diagnosed from JSON before inspecting the screenshot.
`phenotype icons catalog --json` emits the complete all/sidebar/toolbar symbol
contract, name/reference lookup invariants, macOS role metrics, and SVG source
presence checks from the same pure metadata package.
`phenotype icons lookup <name-or-reference> --json` is the narrow metadata
probe for one glyph when a Finder token maps to the wrong visual metaphor or
hit target. `phenotype icons svg <name-or-reference>` emits the exact
phenotype-owned SVG source for that glyph, with `--json` adding the semantic
reference name, asset policy, and rendering capabilities. `phenotype icons
present <name-or-reference> --role ... --phase ... --json` resolves the exact
macOS-style presentation recipe for one glyph state, including visible RGBA,
background chrome, effective size, hit target, and likely layer/pass. Use these
icon probes when a renderer, path parser, or icon-source cache is suspect, while
`phenotype drive file-explorer --json` embeds the desktop chrome geometry and
icon-system contract under `chrome.geometry` and `chrome.icon_system`; the
same output includes the default glass theme contract under `theme_system`. The
verifier can assert those paths with `require_debug_details`, which keeps
Finder workflow failures debuggable without relying on a screenshot guess.
The native runtime window payload also reports `visibility_state` and
`ready_for_user_interaction`; when a Dock icon exists without a usable window,
the failure should point at `debug.platform_runtime.details.window.*` before
any pixel-region analysis starts.

For material probe input debugging that does not need a native window, use the
matching glass showcase drive command:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli drive glass-showcase --json \
  --script ../../examples/glass_showcase/glass_showcase.drive \
  --expect backdrop:high \
  --expect density:dense \
  --expect material-count:7
```

The output reports the shared glass state, per-input trace, public material
kinds, expected material plan count, backdrop/inspector/density/viewport state,
and expectation results. It is useful for validating the probe scene's input
abstraction before running `phenotype artifact verify-glass-showcase`.

## Desktop example artifact hook

Native desktop examples can write a startup artifact bundle without adding
example-specific code. Prefer the CLI launch wrapper so build output, process
exit state, timeout state, and artifact summary are captured in one receipt:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli run ../../examples/native \
  --artifact-dir /tmp/phenotype-native-startup \
  --artifact-reason native-startup \
  --artifact-exit \
  --timeout-seconds 120
```

The lower-level environment contract remains available when debugging the
example binary directly:

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

The file explorer artifact's `application.file_explorer.chrome.icon_system`
object also names the pure SVG subset, supported path commands, supported style
attributes, round cap/join policy, and arc lowering policy used by built-in
macOS-style glyphs. It also records regular weight alignment, monochrome and
hierarchical support, and explicit palette/multicolor unsupported counts, so
future richer symbol rendering can fail as a contract change instead of a
silent visual drift. If a Finder-like icon breaks, check those fields before
comparing screenshots; a missing SVG command or stroke-style policy should fail
as a JSON contract mismatch. `svg_path_arc_symbol_count` proves that at least
one built-in Finder sidebar symbol exercises the SVG path arc parser in normal
example artifacts, while `round_stroke_symbol_count` proves outline glyphs stay
on the macOS-like round stroke contract. The desktop payload also includes
`sidebar_symbol_tokens`, the renderer-facing table that maps sidebar tokens to
phenotype-owned symbols and semantic SF Symbols reference names, so a wrong
Recents/Shared/Desktop-style metaphor fails as data before anyone compares
pixels. `file_type_symbol_tokens` performs the same check for icon-grid
fallback symbols, including PDF, text, image, movie, archive, document, and
folder entries. `sidebar_symbol_presentations`,
`toolbar_symbol_presentations`, `file_type_symbol_presentations`, and
`presentation_samples` then resolve those semantic tokens into the same
role/phase/selected/disabled recipes used by `phenotype icons present`, so
alpha, effective point size, hit target, and likely icon layer/pass can be
checked before looking at pixels. `symbol_control_chrome_policy`,
`symbol_interaction_phase_policy`, toolbar/sidebar pressed background alpha,
pressed symbol opacity, and pressed scale describe the macOS-style normal,
hovered, pressed, selected, and disabled symbol chrome that the example uses
before rendering. Core `ButtonVisualState` carries the pressed callback id into
state-aware canvas buttons, so a Finder toolbar glyph can prove from JSON and
pixels that it is consuming the pure icon recipe rather than painting a separate
ad hoc pressed style.

The same artifact exposes `application.file_explorer.theme_system.*` from the
pure `phenotype_theme_contract` package. That block names the Apple-like glass
theme profile, Pretendard font policy, material planning boundary,
macOS/Finder-style iconography policy, phenotype-owned SVG asset policy, Liquid
Glass usage boundary, grouped-container policy, performance bounds,
accessibility fallbacks, and unsupported-backend degradation. Check it first
when a Finder artifact looks visually off but material plans and icon metadata
still pass: theme drift, material planning, backend execution, and example
geometry now have separate JSON owners.

`application.file_explorer.resource_system.*` is the package/debug-resource
counterpart. It records the file explorer application id/version/entry,
declared platforms, SVG/preload/runtime-visible asset counts, app icon
SVG/preload state, default locale/font state, locale coverage, Pretendard CJK
fallback status, and debug manifest/probe/verifier declarations. Use it when an
artifact looks correct but package inspection, i18n, icon loading, or the
verifier route might be stale.

`PHENOTYPE_ARTIFACT_EXIT=1` makes the example exit after writing the first
rendered frame, which is useful for CI or LLM debugging. Omit it for manual
inspection; the bundle is still written after the initial render and the window
continues running. `examples/glass_showcase` uses the same route for
material-focused checks, including macOS sampled-backdrop rendering and fallback
metadata. The desktop and mobile file explorer examples use the same route for
product-style material workflow artifacts.
File explorer examples can also replay deterministic startup input through the
same shared model parser used by `phenotype drive file-explorer`:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli run file_explorer_desktop \
  --artifact-dir /tmp/phenotype-file-explorer-input \
  --artifact-exit \
  --input view:gallery \
  --input select:README.txt \
  --input shortcut:duplicate
```

The CLI validates those inputs before launch, reports direct/script input
counts in the run receipt, and passes only
`PHENOTYPE_FILE_EXPLORER_INPUTS`/`PHENOTYPE_FILE_EXPLORER_SCRIPT` into the
native process. This keeps file reads, process execution, and environment
access at the edge while allowing artifact bundles to prove the result of GUI
input replay.
For a single command that drives input and observes native output, use
`--observe-output`:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli run file_explorer_desktop \
  --observe-output \
  --input view:gallery \
  --input select:README.txt \
  --json
```

`--observe-output` allocates an artifact directory if needed, implies
`PHENOTYPE_ARTIFACT_EXIT=1`, and embeds the same parsed semantic/material/
runtime observation produced by `phenotype observe` in the JSON run receipt.
The macOS native backend reads accessibility display preferences from
`NSWorkspace` by default. For deterministic artifact capture, set
`PHENOTYPE_ACCESSIBILITY_DISPLAY=standard` to disable those inputs, or set
`PHENOTYPE_ACCESSIBILITY_DISPLAY=reduce-transparency,increase-contrast,reduce-motion`
with any subset of tokens to exercise the pure accessibility downgrade paths.
`phenotype artifact verify-glass-showcase` defaults to `standard`, while its
`--accessibility` mode defaults to
`reduce-transparency,increase-contrast,reduce-motion` unless the caller supplies
`--accessibility-display` or `PHENOTYPE_ACCESSIBILITY_DISPLAY`.

Material frames also expose a resolved backend plan. Semantic material nodes
describe the stable request (`kind`, `role`, opacity, blur intent, contrast
intent, and fallback availability), while `debug.platform_runtime.details.renderer` records
the actual `material_plans` executed for the frame. Each plan includes:

- `contract_version`, `plan_id`, `kind`, `role`, and `command_descriptor`,
  which preserves the decoded `MaterialRect` material values and functional
  surface role before fallback or luminance policy mutates the resolved plan;
- `container`, including isolated/container/union mode, container id, union id,
  spacing, interactive flag, morph-transition expectation, shared backdrop
  scope, and shape-union expectation;
- `reference_model`, the pure Apple Liquid Glass alignment contract: technology
  name, semantic variant/thickness, view-bounds shape scope, resolved shape,
  sampled-backdrop or deterministic-fallback blending scope, tint participation,
  accessibility response, performance response, interactive response, container
  grouping/union/morph expectations, foreground legibility preservation,
  vibrancy expectation, and deterministic degradation;
- raw `geometry`, derived `shape` analysis, tint, blur radius, saturation,
  luminance curve, edge highlight, noise, and shadow values for the resolved
  plan. `shape.effective_radius` is the clamped radius the backend executes;
- `render_target`, including target dimensions, scale, pixel format, pixel
  count, readiness, and whether the backdrop-pixel budget was satisfied;
- `decision_trace`, including the pure gate booleans for geometry, quality,
  backend capabilities, backdrop-source readiness, reduced transparency,
  increased contrast, reduced motion, and the first blocker that explains the
  fallback path;
- `backdrop_sampling`, `fallback`, `fallback_path`, and `fallback_reason`;
- `backdrop`, including source, readiness flags, whether foreground text was
  excluded from the source, sanitized luminance statistics, luminance response,
  and floor/gain/edge deltas;
- `backdrop_access`, including whether the plan requires a stable frame-history
  source, whether that source is a shared frame capture, the capture scope, the
  capture reason, maximum capture count, maximum capture pixels, maximum
  surface sample pixels, whether foreground text must be excluded, and whether
  the access budget is bounded. Sampled plans use `capture_scope: shared-frame`
  with
  `capture_reason: sample-current-frame`. A supported first-frame fallback can
  use `capture_reason: warmup-next-frame` with zero surface sample pixels so
  the next frame has a debuggable history source; unsupported fallbacks use
  `none` with zero budgets;
- `theme`, including the resolved material-style token source, profile name,
  token policy, foreground, secondary foreground, accent, strong accent, tint,
  border, and booleans that show whether each token family matches the default
  Apple-like glass theme. Default-token plans report
  `profile_name: apple-glass-light`; custom token plans report
  `profile_name: custom`, so a verifier failure can distinguish theme drift
  from backend drawing before looking at pixels;
- `foreground`, including primary/secondary/accent recommendations, scheme,
  source, estimated background luminance, contrast ratios, accessibility flags,
  and whether the recommendation was backdrop-driven or vibrancy-enabled;
- `sampling_kernel`, including the pure kernel name, radius, tap count, blur
  step scale, weight profile, backdrop dependency, and boundedness flag;
- `quality_policy`, `primary_pass`, `resource_budget`,
  `execution_stage_capacity`, `dropped_execution_stage_count`, the pass list
  the backend attempted, and `execution_stages`. The stage list is a bounded
  pure description of shadow, primary blur/fallback, edge highlight, and
  noise/dither work, including likely layer, executor role, backdrop
  requirement, sample taps, texture-copy bound, and bounded-work flag. A
  nonzero dropped-stage count means the pure plan exceeded its fixed stage
  capacity before any backend ran it, so the verifier fails at the exact plan
  path instead of letting the missing stage become a visual guess;
- verifier expectations for region checks;
- `observation_contract`, a pure audit contract that repeats the runtime facts
  the verifier must observe: fallback/backdrop expectations, fallback reason,
  primary pass/executor, expected pass and execution-stage counts, texture-copy
  bounds, shared and next-frame capture expectations, capture reason,
  capture/sample pixel bounds, safety flags, and the region/layer/pass hints.
  The Python verifier
  checks this object against the rest of the same `MaterialPlan`, so stale JSON
  writers or backend-local policy decisions are reported at
  `renderer.material_plans[n].observation_contract.*` before visual thresholds
  are considered.

When debugging a material failure, read the semantic node first to confirm the
UI emitted the expected material surface, then inspect
`renderer.material_plans[]`. The verifier compares semantic material fields
against `MaterialPlan.command_descriptor`, then separately reports whether the
resolved plan ran the glass pass or a deterministic fallback.
It also compares semantic material roles against runtime plan roles, so app
chrome can prove that a glass surface was emitted as `toolbar`, `sidebar`,
`status_bar`, `navigation`, `content`, `overlay`, or a generic `surface`
without visual inspection.
It also compares semantic material container descriptors against
`MaterialPlan.command_descriptor.container` and compares resolved container
identity against `MaterialPlan.container`; a container mismatch points at layout
scope propagation, `MaterialRect` encoding, or command decode before it points
at backend drawing. `command_descriptor.container.morph_transitions` preserves
the request, while `MaterialPlan.container.morph_transitions` may be false when
Reduce Motion disables morph expectations.
`MaterialPlan.reference_model` is the next stop after container checks. It
normalizes the Apple guidance that Liquid Glass applies to a view-bounds shape,
can be tinted, can react to interaction, and can participate in grouped
container/union/morph behavior. It also mirrors HIG-style semantic thickness and
foreground legibility/vibrancy expectations. The verifier derives each
reference field from the surrounding `MaterialPlan`; a mismatch points at
`renderer.material_plans[n].reference_model.*` and suggests inspecting
`plan_material_surface` before changing a backend. `accessibility_response`
summarizes the HIG material adaptations (`standard`, `reduced-transparency`,
`increased-contrast`, `reduced-motion`, or `combined-accessibility`) while the
raw `decision_trace` preserves each flag. `performance_response` summarizes
whether the plan is inactive, running the standard sampled path, using bounded
effect reductions, warming up frame history for the next frame, or taking a
deterministic fallback.
`MaterialPlan.geometry` preserves the raw decoded rectangle. `MaterialPlan.shape`
is the pure executable shape contract: validity, surface area, min/max extent,
radius limit, effective radius, normalized radius, rounded flag, and whether the
requested radius was clamped. Backends must upload or draw
`shape.effective_radius`; a shape failure points at geometry emission, command
decode, or a backend using stale raw radius values.
`sample_taps` and `primary_pass.sample_taps` describe the actual resolved pass.
Fallback plans therefore report `0` taps, while `quality_policy.max_sample_taps`
preserves the caller's upper bound and `resource_budget.max_sample_taps`
records the executable kernel selected by the planner. The supported sampled
backdrop kernels are 1, 5, 9, 13, and 25 taps; the planner chooses the largest
kernel that does not exceed the policy limit. `sampling_kernel.name` is
`weighted-5x5-manhattan` for active sampled glass and `none` for deterministic
fallback. Its `blur_step_scale` is part of the pure contract and is uploaded to
the macOS material shader, so changing blur spread requires changing the plan,
serializer, verifier vocabulary, and docs together. Reduced-motion plans
disable material noise and cap backdrop sample taps before a backend executes
the pass; increased-contrast plans raise opacity and luminance legibility in
the same pure layer.
`luminance_curve` is the backend-executed contrast transform. Active sampled
glass must use `adaptive-backdrop-luma` with `backdrop_driven: true`; fallback
plans must use `fallback-flat` with `backdrop_driven: false`. `floor` and
`gain` must match `MaterialPlan.luminance_floor` and
`MaterialPlan.luminance_gain`, while `gamma`, `midpoint`, `contrast`, and
`edge_lift` are bounded shader inputs. A curve failure should be debugged from
`MaterialPlan.backdrop` first, then from the backend shader input upload.
`foreground` is the material text/icon legibility contract. Active sampled glass
must report `backdrop_driven: true` and `uses_vibrancy: true`; deterministic
fallback reports a fallback or accessibility source. The verifier checks that
`primary_contrast_ratio` meets `minimum_contrast_ratio` and that scheme/source
values are known, so foreground failures point at pure material policy before
backend drawing. Native backends then apply this plan to primary, secondary,
and accent text tokens whose draw origins fall inside a prior material command.
Use `renderer.material_executor_summary.foreground_text_candidate_count` and
`foreground_text_remap_count` to confirm that this execution step happened in
the artifact; custom colors should count as candidates but remain unremapped.
For sampled macOS glass, also require
`renderer.material_executor_summary.backdrop_copy_excludes_foreground_text:
true` and `foreground_pass_after_backdrop_copy: true`. If text appears as a
blurred ghost below current foreground text, inspect these fields first; a false
value means the material backdrop source is sampling a final foreground frame
instead of a foreground-excluded scene pass.
`theme` is adjacent to `foreground` because text and icon color debugging often
starts with the same token source. The verifier checks the theme object shape,
byte-range color channels, string buckets, token-match booleans, and the
`profile_name`/`default_glass_tokens` relationship. A theme failure points at
`MaterialStyle` construction or `plan_material_surface`, not a native backend
palette.
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
edge highlight, `likely_layer` must match `primary_pass.likely_layer`, and
`likely_pass` must match `primary_pass.name`.
Manifests can pin `verifier_profiles`, `verifier_region_layers`, and
`verifier_region_passes` in
`require_material_plan_summary` so a pixel-region failure reports the expected
region contract, layer, and pass before anyone opens the frame visually.
The same summary can pin `container_modes`, `container_ids`, `union_ids`,
`container_participating`, `container_unioned`, `container_interactive`,
`container_morph_transitions`, `verifier_require_container_identity`, and
`verifier_require_container_morph_contract`.
Reference-model gates can pin `reference_technologies`, `reference_variants`,
`reference_shapes`, `reference_shape_scopes`, `reference_blending_scopes`,
`reference_semantic_thickness`, `reference_view_bounds_anchored`,
`reference_shape_matches_geometry`, `reference_tint_applied`,
`reference_interactive_response`, `reference_container_grouped`,
`reference_container_union`, `reference_container_morphing`,
`reference_legibility_preserved`, `reference_vibrancy_expected`, and
`reference_deterministic_degradation`.
Foreground gates can pin `foreground_schemes`, `foreground_sources`,
`foreground_backdrop_driven`, `foreground_high_contrast`,
`foreground_vibrant`, `foreground_deterministic`,
`foreground_min_primary_contrast_gte`, and
`foreground_minimum_contrast_gte`.
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

From the repo root, prefer the CLI verifier edge:
`tools/phenotype_cli/.exon/debug/phenotype_cli artifact verify ...`.
That command runs the verifier through `mise` and `uv`; `mise.toml` pins Python
and uv, while `pyproject.toml`/`uv.lock` define the Python tool environment.
Use `mise run tools:artifact:test` for the verifier's contract tests and
`mise run tools:artifact:pycompile` for syntax checks when changing the
verifier implementation itself. Shell/Python tools should be treated as
compatibility wrappers once a matching CLI command exists.
The CLI owns `artifact verify-glass-showcase` and `artifact
verify-file-explorer` directly, so new docs and automation should prefer those
command names while shell scripts remain as stable local wrappers.
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
For Finder-style desktop windows, inspect
`debug.platform_runtime.details.window`: `chrome=integrated_titlebar`,
`window_options_present=true`, and matching `integrated_titlebar.*` metrics
mean the example requested native integrated chrome, including the leading
traffic-light reserve and trailing caption-button reserve. `native_controls_owned_by_os=true`
with `uses_glfw=false` confirms close/minimize/maximize controls and caption
hit testing stay at the platform edge rather than being redrawn by phenotype.
The file explorer's `debug.application.file_explorer.chrome` additionally
publishes `content_window_control_markers=true` and three titlebar-control marker
metrics; these describe the deterministic visual marker used in startup
artifacts, not an input-capable duplicate of OS controls. The same chrome object
publishes the native titlebar drag/control reserve widths plus a
`geometry.policy=finder_integrated_glass_chrome_geometry_v1` object with the
window inset/gap, sidebar surface origin, first sidebar row, toolbar shell,
navigation/title/trailing toolbar group x-coordinates, collapsed search x, and
content surface origin. It also publishes sidebar symbol size, symbol leading,
label leading, section gap, selected-row radius, and selected-row alpha policy
so Finder sidebar density, placement, and active-row weight can fail as JSON
contracts before a pixel-region summary is needed.
Its sibling `debug.application.file_explorer.keyboard_commands` publishes the
desktop command descriptors consumed by native key dispatch. If
`CommandOrControl+F`, `Enter`, `DeleteOrBackspace`, `CommandOrControl+D`,
`Shift+CommandOrControl+N`, or `Escape` drift, prefer fixing the shared
`file_explorer_shared` command contract before changing platform key maps.
On macOS, `titlebar_transparent=true`, `full_size_content_view=true`,
`title_hidden=true`, and `background_drag_enabled=true` are read back from the
live `NSWindow`, not inferred from the request, so a Finder-style artifact can
detect a shell that accepted `WindowOptions` but failed to apply AppKit chrome.
Plan-level failures route to `plan_material_surface` and runtime plan
serialization; semantic/runtime contract failures route to semantic material
nodes, `MaterialRect` command emission, and `renderer.material_plans[]` parity.
The `MaterialRect` command carries the material node's style descriptor
to the backend, including functional surface role, saturation, luminance curve,
edge highlight, noise, and shadow. Native command decoders convert this payload into
`MaterialCommandDescriptor` before building the pure `MaterialRequest`. If
semantic material fields are correct but runtime plans drift, inspect the
command descriptor decode path before changing backend policy.
Use `--require-material-plan` when a bundle must contain resolved material
plans.

Manifests can also set `require_material_plan_summary` to assert the resolved
material aggregate, not just the per-plan schema. Supported keys are `count`,
`min_count`, `fallback`, `backdrop_sampling`, `backdrop_available`,
`backdrop_stable`, `backdrop_access_required`,
`backdrop_access_stable_required`, `backdrop_access_frame_history_required`,
`backdrop_access_shared_frame_capture`,
`backdrop_access_next_frame_capture_required`, `backdrop_access_bounded`,
`luminance_adapted`, `render_target_ready`,
`render_target_within_backdrop_budget`, `decision_can_sample_backdrop`,
`decision_backend_supports_backdrop`, `decision_backdrop_source_ready`,
`decision_reduced_transparency`, `decision_increase_contrast`,
`decision_reduce_motion`, and
exact count maps for `fallback_paths`, `fallback_reasons`, `kinds`, `roles`,
`contract_versions`, `pass_names`, `backdrop_sources`,
`backdrop_access_sources`, `backdrop_capture_scopes`,
`backdrop_capture_reasons`, `luminance_responses`,
`render_target_pixel_formats`, `pass_executors`, `stage_names`,
`stage_executors`, `sampling_kernels`,
`sampling_weight_profiles`, `luminance_curves`, `decision_blockers`,
`foreground_schemes`, `foreground_sources`, `verifier_profiles`,
`verifier_region_layers`, `verifier_region_passes`, `container_modes`,
`container_ids`, `union_ids`, `theme_profile_names`, `theme_sources`, and
`theme_token_policies`;
it can also count
`container_participating`, `container_unioned`, `container_interactive`,
`container_morph_transitions`, `verifier_require_backdrop_source`,
`verifier_require_edge_highlight`, `verifier_require_container_identity`, and
`verifier_require_container_morph_contract`. Theme gates can additionally pin
`theme_foreground_matches_theme`, `theme_accent_matches_theme`,
`theme_tint_matches_surface`, `theme_border_matches_theme`, and
`theme_default_glass_tokens`. Foreground gates can additionally
pin `foreground_backdrop_driven`, `foreground_high_contrast`,
`foreground_vibrant`, `foreground_deterministic`,
`foreground_min_primary_contrast_gte`, and
`foreground_minimum_contrast_gte`. Shape gates can additionally pin
`shape_valid`, `shape_rounded`, `shape_radius_clamped`, and max shape bounds
such as `shape_max_effective_radius_lte`/`gte`. This
catches policy drift such as a glass scene silently switching from backdrop
blur to fallback, a fallback backend reporting the wrong deterministic pass, a
sampled scene losing its previous-frame backdrop source, a render target
exceeding the pure backdrop budget, a pass switching executor roles, a decision
trace naming the wrong blocker, an artifact emitting an unexpected material plan
schema version, verifier expectations pointing at the wrong region/layer/pass, a
material container losing its identity/union grouping, a backend using raw
radius instead of the pure effective shape radius, a blur kernel drifting out of
sync with the backend shader, a theme token snapshot drifting away from the
default Apple-like glass contract, a foreground contrast recommendation falling
below the pure minimum, or a quality/capability downgrade losing its
LLM-actionable reason string.
Use `require_material_surface_roles` when a scene must contain at least one
semantic material node for each functional role.

The plan schema check also treats `primary_pass` as a runtime contract. Its
sample-tap count must match the plan, and the backend `passes[]` list must
include the same pass entry. When this fails, inspect the pass serializer before
changing material policy. Fallback metadata is similarly strict: fallback plans
must report a non-empty `fallback_reason`, while non-fallback plans must leave
`fallback_reason` empty so stale downgrade explanations cannot leak into glass
artifacts.
The verifier also treats material kind, fallback path, reference-model strings,
material pass names, sampling kernel names, and luminance curve names as fixed
vocabularies. Current fallback paths are `none`, `no-material`, `invalid-geometry`,
`unsupported-backend`, `no-backdrop-source`, `reduced-transparency`, and
`quality-policy`; current pass names are `none`, `backdrop-sample-blur`, and
`translucent-rounded-rect`; current sampling kernels are `none` and
`weighted-5x5-manhattan`; current luminance curves are
`adaptive-backdrop-luma` and `fallback-flat`. Current reference blending scopes
are `none`, `sampled-backdrop`, and `deterministic-fallback`; current reference
shapes are `none`, `invalid`, `rectangle`, and `rounded-rectangle`; current
backdrop capture scopes are `none` and `shared-frame`. Add new
planner/backend
vocabulary to the verifier in the same patch that introduces it, so CI reports
schema drift before a human has to infer it visually.

Use `require_material_resource_bounds` when a material gate must prove the
runtime stayed within the pure plan's performance budget. Supported limits are
`max_plan_blur_radius_lte`, `max_plan_sample_taps_lte`,
`max_plan_sample_taps_gte`, `total_plan_sample_taps_lte`/`gte`,
`max_budget_blur_radius_lte`, `max_sample_taps_lte`,
`max_sampling_kernel_radius_lte`/`gte`, `max_pass_count_lte`,
`max_backdrop_pixels_lte`, `max_frame_capture_count_lte`/`gte`,
`max_frame_capture_pixels_lte`/`gte`,
`total_surface_sample_pixels_lte`/`gte`,
`max_surface_sample_pixels_lte`/`gte`,
`max_container_spacing_lte`/`gte`,
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
count, fallback instance count, material draw calls, encoded material sample
tap totals, planned shared frame capture count/pixels, planned surface sample
pixels, next-frame capture plan count, upload bytes/capacity,
framebuffer-history copy pixels, and CPU
enqueue timings. Foreground execution counters report how many text commands
landed inside material surfaces and how many default material text tokens were
remapped to `MaterialPlan.foreground`. Use
`require_runtime_numeric_bounds` for CI-safe limits on those numeric runtime
paths. Each entry names a path under `debug.platform_runtime.details` and can
provide `equals`, `gte`, and/or `lte`; failures report the exact path plus the
likely `material-executor` pass when the path targets the executor summary.
Whenever material plans are present, the verifier also cross-checks executor
counts against `renderer.material_plans#summary`: `plan_count`,
`fallback_instance_count`, `material_instance_count`,
`material_max_sample_taps`, and `material_total_sample_taps` must match the
resolved plan aggregate. Executor stage counters must also match the pure
`execution_stages` summary: total stages, active stages, backdrop-dependent
stages, dropped stages, primary runtime stages, backdrop-filter stages,
fallback-fill stages, shape-shadow stages, edge-highlight stages, and
noise/dither stages. Backdrop access counters must match the pure
`backdrop_access` aggregate, including next-frame warmup capture requests, and
any actual framebuffer-history copy must fit the planned shared capture count
and pixel budget. The resource-bound gate can
also require
`max_execution_stage_capacity_*` and `dropped_execution_stages_*` so artifacts
prove that stage growth stayed inside the serialized capacity. Draw calls must
stay within material instances times the pure pass budget, upload bytes must
fit the reported material buffer capacity, and copied backdrop pixels must stay
within the pure resource budget. Foreground remaps must also be less than or
equal to foreground text candidates, otherwise the backend has counted a remap
without a material surface hit.
For sampled-backdrop PRs, require the foreground-exclusion booleans in both the
plan summary and executor summary. This keeps the final artifact/readback frame
complete while proving the material shader sampled a separate safe backdrop
texture.
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
material kind/profile/container maps from `debug.semantic_tree` against
`debug.platform_runtime.details.renderer.material_plans#summary`; mismatches
point at the material contract layer so the likely break is not confused with a
pure pixel-capture failure.

```sh
tools/phenotype_cli/.exon/debug/phenotype_cli artifact verify /tmp/phenotype-native-startup \
  --json \
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
tools/phenotype_cli/.exon/debug/phenotype_cli artifact verify /tmp/phenotype-glass-showcase \
  --json \
  --expect-platform macos \
  --manifest examples/glass_showcase/artifact_manifest.json
```

Or run the local gate, which builds the example, launches the startup artifact
hook, and applies the manifest in one command:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli artifact verify-glass-showcase --json
```

The command emits a deterministic JSON report with build, run, verifier, and
artifact details, and exits non-zero when an invariant fails. The legacy
`tools/verify_glass_showcase_artifact.sh` wrapper delegates to the same CLI
command for local compatibility.

To validate the accessibility downgrade contract end to end, run the companion
gate:

```sh
cd tools/phenotype_cli
.exon/debug/phenotype_cli artifact verify-glass-showcase --accessibility --json
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
contract checks. Shared model-only file explorer and glass showcase changes run
their targeted shared-package tests plus CLI JSON smoke instead of the full
root matrix. The main-branch push workflow only runs docs builds and native
artifact builds; it does not repeat root code tests or slow glass artifact
capture after merge. WASI root tests and docs builds run on Linux runners.
Workflow-file changes deliberately enable code, docs, and tooling gates so
runner policy edits validate themselves. Windows artifact automation and
Android CI wiring remain future work.

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

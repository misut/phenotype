# Apple Glass GUI Roadmap

Status: implementation baseline for `origin/main` after
`feat: expose material shape execution contracts`. This includes the pure
material-planning boundary, macOS sampled-backdrop execution, deterministic
fallback contracts on non-macOS backends, edge executor telemetry, and the
artifact verifier gates described below.

This document turns the current long-term goal into concrete deliverables:

- analyze the current state of phenotype;
- implement an Apple-style glass GUI without compromising stability or
  performance;
- make the GUI debuggable by LLMs from deterministic artifacts, not visual
  guesswork;
- keep runnable examples under `examples/` that exercise the full supported
  feature surface locally.

## Official design baseline

Apple's current guidance names this material system **Liquid Glass** and
standard **Materials**, not generic decoration. The relevant constraints for
phenotype are:

- Liquid Glass is a functional layer for controls and navigation that floats
  above content, not a general content-card style.
- Standard materials use blur, vibrancy, and blending to separate foreground
  and background content.
- Material thickness trades context against legibility: thinner materials show
  more background, thicker materials improve contrast.
- Color on glass should be sparse and reserved for meaningful emphasis, such as
  primary actions or status.
- The implementation must adapt to accessibility settings such as reduced
  transparency, increased contrast, and reduced motion.
- phenotype's custom material system should follow these semantics without
  claiming to be a drop-in replacement for Apple system components. Use the
  `liquid-glass` backend path for functional layers and controls; use standard
  material/fallback semantics for content-layer grouping.

References:

- Apple Human Interface Guidelines, Materials:
  https://developer.apple.com/design/human-interface-guidelines/materials
- Apple technology overview, Liquid Glass:
  https://developer.apple.com/documentation/technologyoverviews/liquid-glass
- Apple technology overview, Adopting Liquid Glass:
  https://developer.apple.com/documentation/technologyoverviews/adopting-liquid-glass
- Apple developer guidance, Applying Liquid Glass to custom views:
  https://developer.apple.com/documentation/SwiftUI/Applying-Liquid-Glass-to-custom-views
- Apple AppKit accessibility display preferences:
  https://developer.apple.com/documentation/appkit/nsworkspace/accessibilitydisplayshouldreducetransparency
  https://developer.apple.com/documentation/appkit/nsworkspace/accessibilitydisplayshouldincreasecontrast
  https://developer.apple.com/documentation/appkit/nsworkspace/accessibilitydisplayshouldreducemotion
- Apple Human Interface Guidelines, File management:
  https://developer.apple.com/design/human-interface-guidelines/file-management
- Apple Human Interface Guidelines, Sidebars:
  https://developer.apple.com/design/human-interface-guidelines/sidebars
- Apple Human Interface Guidelines, Column views:
  https://developer.apple.com/design/human-interface-guidelines/column-views
- Apple Human Interface Guidelines, Toolbars:
  https://developer.apple.com/design/human-interface-guidelines/toolbars
- Apple Human Interface Guidelines, Search fields:
  https://developer.apple.com/design/human-interface-guidelines/search-fields

## Current state

### Core model

phenotype already has the correct high-level architecture for this work:

- pure C++23 core modules for state, layout, paint, commands, diagnostics, and
  theme JSON;
- a binary draw-command buffer shared by WASM and native backends;
- backend-specific adapters for macOS, Windows, Android, and WASI;
- a shared debug plane with capability reporting, semantic tree snapshots,
  input debug state, runtime details, artifact bundles, and optional frame
  capture.

This is the right shape for LLM-debuggable GUI work because the visible result
can be described through structured data (`snapshot.json`, semantic tree,
runtime JSON, frame capture) instead of relying only on screenshots.

### Supported platforms

`exon.toml` currently declares these supported targets:

| Target | State |
|---|---|
| `wasi` | Supported snapshot/debug target, no native frame capture |
| `macos/aarch64` | AppKit shell, CoreText, Metal, local and remote images |
| `windows/x86_64` | Win32 shell, DirectWrite, Direct3D 12, local and remote images |
| `android/aarch64` | GameActivity shell, Vulkan renderer, text/image pipelines, debug plane |

Linux native is intentionally not a supported renderer target today.

### Rendering primitives

The current command protocol is strong for retained-style UI, canvas drawing,
and renderer parity:

- `Clear`
- `FillRect`
- `StrokeRect`
- `RoundRect`
- `DrawText`
- `DrawLine`
- `HitRegion`
- `DrawImage`
- `Scissor`
- `DrawArc`
- `Path`
- `FillPath`
- `FillQuads`
- `FillRects`

This is enough for cards, controls, text, images, canvas-heavy views, clipping,
paths, arcs, batching, and bounded stripe-backed linear gradients through
`Painter::linear_gradient_rect`. It is not enough for faithful Apple material
work.

Missing primitives for true glass:

- gradient command as a backend-native primitive. The current
  `Painter::linear_gradient_rect` helper intentionally lowers to bounded
  `FillRects` so every backend shares the same deterministic behavior while a
  future opcode can optimize gradients without changing app code;
- foreground vibrancy tokens that adapt text/icon colors on top of material.

The current fallback can approximate glass with translucent rounded surfaces
over rich content. macOS now has a bounded sampled-backdrop material path.
Windows and Android intentionally degrade to translucent fallback passes while
recording the same resolved `MaterialPlan` schema; WASI/WebGPU snapshot paths
publish an empty renderer material contract instead of pretending a backend
pass executed.

### Pure material planning

Material policy is centralized in `phenotype.material`.
`plan_material_surface(MaterialRequest, MaterialEnvironment)` is a pure,
total function: platform capability probes, clocks, shader compilation,
filesystem writes, and image capture remain in backend adapters. The function
accepts immutable inputs:

- `MaterialStyle`
- material container descriptor
- geometry
- capability snapshot
- backdrop descriptor
- render-target metadata
- debug seed
- quality policy
- accessibility display inputs

The returned `MaterialPlan` describes the source command descriptor, blur
radius, tint, saturation, luminance curve, edge highlight, noise/dither,
shadow, material container analysis, pure shape analysis, backdrop sampling,
fallback path, debug metadata, resolved quality policy, pass expectations,
sampling kernel, resource budgets, and verifier expectations. The plan also
carries a
`luminance_curve` contract: sampled glass uses the backdrop-driven
`adaptive-backdrop-luma` curve, while deterministic fallback uses
`fallback-flat`.
The current sampled-glass kernel is a pure `weighted-5x5-manhattan` descriptor
with the resolved tap count, kernel radius, blur step scale, and weight profile;
fallback plans serialize the inactive `none` kernel. This keeps blur spread and
tap-shape policy in `plan_material_surface` while macOS Metal executes the
descriptor and other backends publish deterministic fallback metadata.
Backdrops also degrade through an explicit
`quality-policy` fallback when the pure quality policy disables sampling or
sets an unusable blur/tap budget, and when the render target exceeds the
resolved `max_backdrop_pixels` budget. Backends execute the plan; they do not
re-decide policy.
`MaterialPlan.shape` turns raw material geometry into the executable radius,
area, radius limit, and clamp status consumed by native backends. This keeps
Finder-style rounded chrome and mobile card surfaces debuggable from artifacts
instead of relying on screenshots to infer whether a backend used the intended
shape.
`MaterialRect` commands carry the material node's surface role plus numeric
optics/effects descriptor and material container identity into every backend as
`MaterialCommandDescriptor`, so runtime plans no longer need to reconstruct
app-chrome intent, saturation, luminance, edge, noise, shadow, or grouped glass
identity from the current theme.
The macOS backend now reads AppKit accessibility display preferences at the
edge and passes them as immutable planner inputs. Reduce Transparency resolves
to the deterministic material fallback path, Increase Contrast adjusts opacity
and luminance legibility in the pure plan, and Reduce Motion disables material
noise while lowering sampled-backdrop taps.

### Theme and widgets

Theme support has matured beyond the original small token set:

- color tokens, typography sizes, radius scale, spacing scale;
- state colors for hover, active, disabled, error, and focus;
- semantic colors for success, warning, info, and error;
- JSON parsing with partial overrides and string color forms;
- structural widget snapshot tests for the core widget surface.

Widgets currently cover:

- text, code, link, image, canvas;
- button, checkbox, radio, switch, tabs;
- progress and indeterminate progress;
- text field;
- layout containers: column, row, box, sized box, weighted, grid, scaffold,
  card, scroll view, overlay, dialog, accordion, list items, divider, spacer.

The style system is usable, but not yet material-aware. There is no first-class
`layout::material_surface` is now the low-level material container, with
first-class `layout::toolbar`, `layout::sidebar`, and `layout::status_bar`
helpers for app chrome. A richer glass-control style contract remains future
work.

### Diagnostics and LLM debugging

The current debug plane already provides important pieces:

- common `debug.platform_capabilities`;
- `debug.input_debug` for input routing, focus, caret, selection, and
  composition state;
- `debug.semantic_tree` for accessibility-oriented structure;
- `debug.platform_runtime` for shared and backend-specific runtime state;
- artifact bundles with `snapshot.json`, optional `frame.bmp`, and platform
  runtime JSON;
- native frame capture on macOS and Windows, explicit unsupported fields on
  WASI;
- remote image queue/debug state on macOS and Windows.
- resolved material plans with pass expectations, quality policy, resource
  budgets, fallback paths/reasons, verifier expectations, derived runtime
  summaries, and backend executor counters.

This is now strong enough for the current material CI gates: an LLM can inspect
the manifest failure path, material plan summary, fallback reason distribution,
resource bounds, executor counters, and pixel-region metrics before looking at
a rendered frame.

Remaining gaps:

- Windows and Android still use the documented material fallback, with resolved
  runtime fallback plans;
- WASI/WebGPU stays snapshot-only for now;
- macOS has blur-specific pixel probes based on smoothness metrics and ratio
  comparisons against the unfiltered backdrop reference; mirror those on
  additional backends as they gain native material rendering;
- Android CI device/emulator wiring remains a policy and runner-capacity
  decision;
- foreground vibrancy tokens and a backend-native gradient primitive remain
  future work for a fuller Apple-style material vocabulary.

### Examples

Current runnable examples under `examples/`:

| Example | Role | Current value |
|---|---|---|
| `examples/native` | Native widget showcase | Best desktop acceptance scene for shared widgets, input debug, images, scrolling, resizing |
| `examples/glass_showcase` | Material showcase | Exercises deterministic backdrop regions, macOS sampled backdrop material, all public material kinds, semantic material metadata, and startup artifact capture |
| `examples/file_explorer_desktop` | Finder-style desktop app example | Exercises glass toolbar/sidebar/icon-grid composition, Finder-like action clusters, document/image/video/folder thumbnail probes, and sandboxed view/read/create/duplicate/delete file workflows |
| `examples/file_explorer_mobile` | Mobile file explorer app example | Exercises a compact browse/preview/create flow with the same sandboxed file model, duplicate/delete action metadata, and all material kinds |
| `examples/android` | Android APK example | Exercises GameActivity/Vulkan/native Android route |

The examples now have a checked-in coverage matrix, generic pixel-region
verification, fallback reason gates, material quality/resource gates, backend
executor numeric bounds, a macOS glass showcase CI gate, and a local Android
device/emulator contract runner. Android CI wiring and stricter blur-specific
probes on future non-macOS material renderers remain open.

## Success criteria

### Glass GUI

Done means phenotype exposes a stable material system that can render an
Apple-style glass interface while preserving platform parity:

- first-class material tokens for clear, thin, regular, and thick
  material semantics;
- accessibility fallbacks for reduced transparency, increased contrast, and
  reduced motion;
- backend commands for material/backdrop blur or a documented fallback mode
  where the backend cannot support it;
- semantic/debug snapshots that describe material style, fallback reason, and
  contrast expectations;
- material container and shape-union identity that lets related glass surfaces
  share backdrop scope and morph expectations without moving policy into a
  backend;
- a glass showcase under `examples/` with controls/navigation as the glass
  layer and rich content below it;
- desktop and mobile file explorer examples under `examples/` that translate
  the glass contract into an app-like file-management workflow with toolbar,
  sidebar/location navigation, list content, preview, search, create, and
  delete actions;
- first-class material app-chrome helpers for toolbar, sidebar, and status bar
  surfaces so examples do not need to hand-roll every glass container shape;
- tests that cover command encoding, parser behavior, fallback policy, and at
  least one captured-frame invariant on native backends.

### LLM-debuggable GUI

Done means an LLM can inspect artifacts and identify layout, rendering, input,
or material problems without manual visual guessing:

- every example can write an artifact bundle on supported local targets;
- every artifact contains `snapshot.json`;
- every visual scene has a semantic tree with stable roles and labels;
- every material surface appears in debug output with type, opacity, blur,
  fallback availability, bounds, contrast intent, material container identity,
  and a backend-resolved `MaterialPlan`;
- native frame captures are available on macOS and Windows;
- visual verifier output names exact failing JSON paths or frame regions,
  expected values, actual values, likely layer/pass, and suggested next check;
- input failures can be diagnosed from `input_debug` and platform runtime
  state.

### Examples coverage

Done means `examples/` is a local acceptance suite, not just demos:

- each example has a documented purpose and run command;
- a checked-in coverage matrix maps every public widget, layout primitive,
  paint command, image path, input path, debug capability, and supported backend
  to at least one example/test;
- `examples/native` remains the compact all-widget desktop showcase;
- a new glass showcase exercises the material system specifically;
- Finder-style desktop and mobile file explorer examples exercise the material
  system as a real app surface, with filesystem side effects isolated to an
  example-owned temp directory;
- Android has a documented local device/emulator contract runner.
- Painter has a bounded `linear_gradient_rect` helper that uses the existing
  `FillRects` path with no heap allocation in the helper itself.

## Prompt-to-artifact checklist

| Requirement | Current evidence | Gap |
|---|---|---|
| Analyze current phenotype progress | This document, `README.md`, `docs/ARCHITECTURE.md`, `docs/DEBUG_WORKFLOW.md`, examples and tests | Keep updated as milestones land |
| Apple glass style GUI | First-class material surfaces exist with `MaterialRect`, material container/union identity, macOS sampled-backdrop rendering, resolved runtime fallback plans on Windows/Android, snapshot fallback contracts elsewhere, plus `examples/glass_showcase` for the target scene shape | Add Windows/Android/Web native material rendering or keep explicit fallback |
| LLM can debug GUI completely | Debug plane exists with snapshot, semantic tree, input debug, runtime, frame capture, material metadata, resolved material plans, startup bundle verifier, optional pixel-region checks, material/container/shape plan summary gates, fallback reason summary/stale-metadata gates, semantic/runtime material parity gates, material quality/resource bound gates, executor numeric bounds, ratio-based blur probes, a glass showcase manifest, a local glass showcase gate, CI artifact builds, and a local Android contract runner | Add Android CI wiring and mirror blur-specific probes on future native material backends |
| Stability is priority | Existing tests cover core widgets, native debug, text, remote images, command parsing | Add tests before each material/backend expansion |
| Performance is priority | Existing paint cache, scissor, batching, native renderer optimizations, pure material resource bounds for blur radius, sample taps, pass count, backdrop pixels, bounded texture copies, deterministic fallback, pure effective-radius shape bounds, backend `material_runtime_summary` counters cross-checked by the verifier, and backend `material_executor_summary` budget/timing telemetry guarded by artifact manifests | Keep tightening backend timing budgets as more native material renderers land |
| Runnable examples under `examples/` | Native, glass showcase, desktop/mobile file explorer, and Android examples exist | Add Android CI device/emulator wiring when runner capacity allows |
| All phenotype features testable locally | `docs/EXAMPLES_COVERAGE.md` maps current examples/tests to public surfaces and artifact expectations; `tools/verify_artifact_bundle.py` validates startup bundles, optional pixel regions, exact material/container plan summaries, semantic/runtime material parity, material resource bounds, runtime numeric bounds, `examples/glass_showcase/artifact_manifest.json`, `examples/file_explorer_desktop/artifact_manifest.json`, `examples/file_explorer_mobile/artifact_manifest.json`, and `examples/android/artifact_manifest.json`; `tools/verify_glass_showcase_artifact.sh` wraps the glass gate; `tools/verify_file_explorer_artifacts.sh` wraps the local desktop/mobile file explorer gate; `mise run android:contract` wraps the Android device/emulator artifact route | Android CI wiring remains |

## Implementation roadmap

### Milestone G0: coverage contract

Goal: make current support explicit before adding visual effects.

Deliverables:

- `docs/EXAMPLES_COVERAGE.md` mapping features to examples/tests;
- each example README or root examples README with run commands;
- debug artifact expectations for each runnable example;
- first-class artifact verifier command for startup bundles;
- no true backdrop blur renderer changes.

Validation:

- `mise exec -- exon test`;
- `cd examples/native && mise exec -- exon build`;
- `cd examples/glass_showcase && mise exec -- exon build`;
- `tools/verify_file_explorer_artifacts.sh`;
- `cd docs && mise exec -- exon build --target wasm32-wasi`.
- produce and verify startup artifact bundles for desktop examples.

### Milestone G1: material model and fallback contract

Goal: add stable public contracts while every backend has a safe fallback.

Deliverables:

- `MaterialKind` / material token definitions in core types;
- `LayoutNode` material fields;
- widget/layout helpers for material surfaces;
- snapshot and semantic-tree material serialization;
- fallback rendering using translucent rounded rect plus border;
- tests for theme JSON, snapshots, semantic output, and fallback policy.

Validation:

- root tests on supported local target;
- WASI tests prove unsupported frame/material capabilities are explicit;
- native example still builds and runs.

### Milestone G2: glass showcase example

Goal: create the target GUI shape before true blur lands.

Deliverables:

- `examples/glass_showcase`;
- rich content layer below;
- floating navigation/control layer using material surfaces;
- input debug panel and artifact-bundle instructions;
- deterministic visual regions for future verifier.

Validation:

- `cd examples/glass_showcase && mise exec -- exon build`;
- native launch smoke with `PHENOTYPE_ARTIFACT_DIR`;
- artifact bundle verifier using `examples/glass_showcase/artifact_manifest.json`
  to require clear, thin, regular, and thick material kinds, fallback metadata,
  and startup-frame pixel regions.
- `tools/verify_glass_showcase_artifact.sh` as the local build/capture/verify
  gate for the showcase.

### Milestone G3: backend material primitives

Goal: implement true material/backdrop behavior per backend with fallbacks.

Deliverables:

- command protocol extension for material/backdrop region or a renderer-local
  material path that is fully represented in debug output;
- macOS Metal implementation first: `MaterialRect` is resolved through the
  pure `MaterialPlan`, preserves the functional `MaterialSurfaceRole`, samples
  the previous captured framebuffer as the backdrop source, then applies blur,
  tint, saturation, luminance preservation,
  edge highlight, deterministic noise, depth/shadow, and the resolved sampling
  kernel values from the plan; runtime JSON mirrors the plan's `primary_pass`,
  sampling kernel, resource budget, fallback reason, and verifier expectations
  so a CI failure points back to the likely layer/pass;
- Windows Direct3D 12 implementation second;
- Android Vulkan implementation or explicit fallback;
- WASI/WebGPU shim path or explicit fallback;
- frame-capture invariants proving blur/material regions are nonblank and
  preserve foreground contrast.

Validation:

- parser tests;
- backend unit/contract tests;
- captured-frame pixel probes;
- performance counters showing no unbounded allocation or per-frame resource
  churn.

### Milestone G4: pixel verifier hardening and CI gate

Goal: make LLM debugging operational.

Deliverables:

- committed manifest policy for which examples run pixel-region probes in CI;
- CI-safe command or test helper that runs an example, captures artifacts, and
  checks glass-specific pixel invariants;
- failure output that names region, expected value, actual value, and likely
  subsystem;
- artifact bundle docs for attaching outputs to issues or PRs.

Current seed:

- `examples/glass_showcase/artifact_manifest.json` is the first committed
  pixel-region manifest and also requires the macOS material pipeline/source
  runtime details, material quality policy, resource bounds, executor numeric
  bounds, fallback reason summary, and stale fallback metadata checks;
- `examples/glass_showcase/artifact_manifest.accessibility.json` captures the
  same scene under reduced transparency, increased contrast, and reduced motion
  overrides so the artifact proves deterministic accessibility fallback
  behavior;
- `tools/verify_glass_showcase_artifact.sh` builds the example, captures a
  startup artifact bundle, and applies the manifest;
- `tools/verify_glass_showcase_accessibility_artifact.sh` wraps the
  accessibility-response manifest without adding another default CI job;
- PR CI keeps the slow macOS artifact capture as a local acceptance check rather
  than a required job; main-branch pushes run only artifact/docs build gates
  instead of the full code test matrix. WASI root tests and docs builds run on
  Linux runners, while macOS runners stay reserved for native artifact builds.

Validation:

- local verifier passes on the glass showcase;
- CI artifact gates run on macOS native, where GUI startup capture is currently
  reliable;
- verifier intentionally fails on a seeded bad snapshot in a test fixture or
  unit-level harness.

### Milestone G5: full examples acceptance suite

Goal: every public feature has a local acceptance route.

Deliverables:

- complete feature-to-example matrix;
- native, glass showcase, file explorer, and Android routes documented;
- test or build gate for every example that can run on the current host;
- Android emulator/device runner planned or implemented.

Validation:

- documented local command set passes;
- unsupported platform gaps are explicit and discoverable in capabilities.

Current seed:

- `mise run android:contract` builds and installs the Android example, launches
  it with an opt-in artifact property, pulls the app-private bundle via
  `adb run-as`, and applies `examples/android/artifact_manifest.json`.
- The runner is local/device-oriented for now; CI device/emulator wiring remains
  a follow-up decision.

## Engineering guardrails

- Add debug/schema fields before relying on a visual effect in examples.
- Prefer fallback-first public API, then backend acceleration.
- Do not add material effects only to one backend without an explicit
  capability/fallback story for every supported target.
- Keep command-buffer changes append-only and parser-tested.
- Avoid per-frame texture/buffer allocation for blur/material passes.
- Add performance metrics whenever material rendering adds offscreen passes,
  texture copies, or readbacks.
- Keep examples deterministic enough for artifact comparison.

## Next recommended PR

The initial G0-G4 path is now landed. The next useful PR should avoid another
schema-only increment unless a real failure appears. Recommended directions:

- add a first-class material-aware surface/control widget API on top of the
  existing `MaterialRect` command path;
- tighten macOS material executor budgets after collecting a small sample of
  local and CI timing/copy values;
- add Android CI emulator wiring if runner capacity and cost are acceptable;
- begin a Windows or Android native material renderer only if it can preserve
  the same pure `MaterialPlan`, fallback, artifact, and verifier contracts.

If the next PR chooses renderer work, decide whether true material rendering
should move next to Windows Direct3D 12, Android Vulkan, or stricter
blur-specific verifier probes, and whether Android device/emulator contract
coverage should be promoted into CI.

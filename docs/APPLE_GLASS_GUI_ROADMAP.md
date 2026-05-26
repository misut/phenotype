# Apple Glass GUI Roadmap

Status: implementation baseline for `origin/main` after
`feat(theme): add pure glass theme contract`, with the in-progress material
runtime branch carrying the schema-39 material contract through
`MaterialPlan.refraction`, `MaterialPlan.optical_composition`, and executable
stage-order auditing. This includes
the pure material-planning boundary, macOS sampled-backdrop execution,
deterministic fallback contracts on non-macOS backends, edge executor telemetry,
pure observation/execution contracts, transparent native window composition for
integrated glass chrome, and the artifact verifier gates described below.

This document turns the current long-term goal into concrete deliverables:

- analyze the current state of phenotype;
- implement an Apple-style glass GUI without compromising stability or
  performance;
- make the GUI debuggable by LLMs from deterministic artifacts, not visual
  guesswork;
- move artifact, Android, packaging, asset, and i18n diagnostics toward a
  first-class `phenotype` CLI instead of scattered shell/Python entry points;
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
  `liquid-glass` backend path for functional layers and controls; resolve
  content-layer grouping to `standard-material` with `standard-fill` execution
  unless the plan is invalid and must take an explicit deterministic fallback.

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
- Apple Human Interface Guidelines, Icons:
  https://developer.apple.com/design/human-interface-guidelines/icons
- Apple Human Interface Guidelines, SF Symbols:
  https://developer.apple.com/design/human-interface-guidelines/sf-symbols
- Dioxus Tools:
  https://dioxuslabs.com/learn/0.7/guides/tools/
- Dioxus Bundling:
  https://dioxuslabs.com/learn/0.7/tutorial/bundle/
- Dioxus Assets:
  https://dioxuslabs.com/learn/0.7/essentials/ui/assets/
- Dioxus Custom Renderer:
  https://dioxuslabs.com/learn/0.7/guides/depth/custom_renderer/
- Dioxus Internationalization:
  https://dioxuslabs.com/learn/0.7/guides/utilities/internationalization/

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
- `LinearGradientRect`

This is enough for cards, controls, text, images, canvas-heavy views, clipping,
paths, arcs, batching, and bounded stripe-backed linear gradients through a
first-class `LinearGradientRect` command. It is not enough for faithful Apple
material work.

Missing primitives for true glass:

- backend-native gradient shader paths. The current `LinearGradientRect`
  command intentionally lowers to bounded strips in each backend so every
  renderer shares deterministic output while a future shader path can optimize
  the same command without changing app code.

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

The current reference alignment follows Apple's Liquid Glass documentation:
custom views apply glass to an explicit view shape with optional tint and
interactivity, related glass surfaces should be grouped so the system can
combine and morph effects efficiently, HIG materials are semantic choices that
must preserve legibility and vibrancy, and Metal work must remain bounded by
runtime capability/resource limits. The corresponding phenotype fields live in
the pure `MaterialPlan.reference_model`; backend adapters only execute or
serialize the result.

The returned `MaterialPlan` describes the source command descriptor, blur
radius, tint, saturation, luminance curve, edge highlight, noise/dither,
shadow, material container analysis, pure shape analysis, backdrop sampling,
fallback path, debug metadata, resolved quality policy, pass expectations,
sampling kernel, foreground legibility/vibrancy recommendation, resource
budgets, a pure Apple reference alignment model, the resolved theme token
snapshot, and verifier expectations. The plan also carries a
`luminance_curve` contract: sampled glass uses the backdrop-driven
`adaptive-backdrop-luma` curve, while deterministic fallback uses
`fallback-flat`.
Interactive surfaces now carry a pure `MaterialInteractionDescriptor` from
layout through the `MaterialRect` command payload into
`MaterialPlan.interaction`. The planner resolves hover, press, focus, pointer
presence, normalized pointer coordinates, reduced-motion handling, and the
exact opacity/blur/saturation/edge/shadow deltas before any backend runs. This
keeps Liquid Glass response policy out of AppKit, Metal, Android, Windows, and
artifact-writing code while still giving each adapter an executable response
contract. The plan also exposes `interaction.enablement_reason` so artifacts can
distinguish inactive materials, noninteractive containers, and eligible
interactive containers without a visual guess. The renderer now also resolves
live pointer position and subtree
hover/press/focus ids into the descriptor before command emission, preserving
explicit probe descriptors while allowing real controls inside an interactive
glass container to drive the same pure response path. Surface-level control
chrome can now opt in with `MaterialSurfaceOptions.interactive`; the built-in
toolbar-group and navigation glass presets use that opt-in so Finder-style
toolbar clusters and segmented navigation controls expose interaction response
without requiring every app to wrap them in a separate material container.
Segmented controls, tab bars, popovers, tooltips, and context menus are now
first-class glass presets as well:
`widget::tabs` emits a material-backed segmented control by default,
`TabsStyleOptions` lets callers choose kind/role/label, and
`layout::segmented_control_surface` / `layout::tab_bar` / `layout::popover` /
`layout::tooltip` / `layout::context_menu` keep Finder-style toolbar,
overflow, help, and contextual chrome on the typed MaterialRect path.
Finder-style search fields now use the same typed path through
`TextFieldStyleOptions` and `widget::glass_text_field_style`, so input chrome
can be verified as interactive material while keyboard-only focus-ring behavior
stays in the core input/focus contract.
Finder-style selected rows now use `GlassOutlineRowStyleOptions` and
`widget::glass_outline_row_button_style`, giving sidebar/list/icon/column rows a
first-class material plan instead of ad hoc painted button chrome. Unselected
rows stay non-material by default, while expanded outline rows may opt into
clear material so hierarchy affordances remain debuggable without making every
file row a sampled-backdrop surface.
Finder-style split controls now use `GlassSplitButtonStyleOptions` and
`widget::glass_split_button_style`. The preset carries toolbar role, segment,
container, union, spacing, hover/press, and morph metadata as immutable widget
input before the backend sees a command, so grouped toolbar actions can join the
same material-container contract as surrounding glass surfaces.
Overlay panels now include first-class `Sheet`, `Inspector`, `CommandPalette`,
`Snackbar`, and `Toast` surface presets. `layout::dialog` consumes the sheet
preset as `Dialog Sheet`, so modal chrome is emitted as an interactive overlay
material node with the same pure planning, fallback, and artifact contract as
popovers.
Finder-style popover menu actions now use `GlassMenuItemStyleOptions` and
`widget::glass_menu_item_button_style`, giving transient overlay action chrome
a clear material plan while keeping disabled or hidden actions out of the
material hot path. The desktop file explorer `more-actions-open` verifier
scenario requires this overlay material role so menu regressions fail from the
semantic/debug contract instead of a screenshot comparison.
The glass showcase now includes noninteractive tooltip and interactive context
menu overlay probes, so transient overlay surfaces are exercised by the same
material plan, fallback, resource-bound, and accessibility manifests as the
base clear/thin/regular/thick material ramp.
It also exercises selected sidebar/list rows, sorted table headers, and
expanded disclosure headers, so app-like selection and hierarchy chrome are
covered by the same material-plan and accessibility artifact gates rather than
only by the file explorer examples.
Finder-style table/list headers now use `GlassTableHeaderStyleOptions` and
`widget::glass_table_header_button_style`. The preset emits clear `content`
material for sortable header cells, keeps sorted-state tint/border decisions in
the pure style layer, and avoids increasing sampled-backdrop work for content
tables.
Disclosure/outline rows now use `GlassDisclosureStyleOptions` and
`widget::glass_disclosure_header_style`. `layout::accordion` consumes the
helper for its header row, so expandable sections emit semantic `accordion-header`
nodes with clear `content` material while keeping state management in
`framework_local`.
Pointer-driven active responses now include a pure, normalized
`pointer-specular` highlight descriptor. The macOS Metal executor consumes that
descriptor as shader input to add a bounded glint near the pointer anchor, while
fallback backends publish the same semantic/debug contract with zero specular
intensity when no active pointer response exists.
`MaterialPlan.optical_response` is the compact schema-stable summary of that
contract. It classifies each surface as sampled backdrop glass, content-layer
standard material, deterministic fallback, or inactive, then records the
blur/color/depth strategy and explicit booleans for blur, frosting, tint,
saturation, luminance preservation, edge highlight, depth shadow, noise/dither,
foreground vibrancy, interaction-driven optical modulation, and fallback
behavior.
Schema 38 carries `MaterialPlan.refraction`, a bounded edge-lens profile with
stable model/source names, active/backdrop/interaction/reduced-motion flags, and
the exact strength, edge bias, and maximum pixel offset the backend may upload.
`MaterialPlan.optical_composition` remains the fuller pure optical recipe
consumed by execution stages. It carries the resolved model plus blur, frosting,
tint, luminance, refraction, depth, interaction, and fallback sources; booleans
for every required optical channel; the stable shadow/primary/edge/noise stage
order; backdrop capture and foreground exclusion policies; bounded/deterministic
safety flags; and the scalar values and copy/sample bounds that backend adapters
are allowed to use.
Stage `optics` are now derived from this composition, so a backend cannot
silently invent a different blur, tint, luminance, refraction, edge, shadow, or
noise contract.
`MaterialPlan.reference_model` maps the current Apple references into a
backend-independent product contract: functional roles are `liquid-glass` on
the `functional-layer`, content roles are `standard-material` on the
`content-layer`, the semantic clear/thin/regular/thick thickness stays explicit,
tint and interaction may apply, container/union/morph behavior is pure, and the
plan must preserve foreground legibility/vibrancy expectations where relevant.
The reference model also records accessibility/performance adaptation responses
and deterministic degradation behavior when backdrop blur is unavailable. The
macOS, Windows, Android, and WASI adapters consume or serialize this resolved
model; they do not own the policy.
Container groups now have a second pure aggregate contract:
`summarize_material_container_groups` derives group count, multi-surface groups,
union/morph/interactive groups, shared-backdrop-scope groups, mixed fallback
groups, max active/sampled/fallback surfaces, shape-pair counts, blend/union/
morph candidate pairs, separated pairs, min/max inter-shape gap, max blend
distance, and group bounds from resolved `MaterialRuntimeRecord` values. Runtime
and executor summaries serialize that same `container_groups` object, so
Apple-style grouping and future container-level resource reuse can be verified
without backend-local policy.
Schema 32 moved deterministic non-backdrop rendering into the pure plan.
Every non-sampled material now exposes a bounded `paint_layers[]` recipe with
explicit shadow, fill, and edge-highlight layers, layer executors, offsets,
inflation, radius deltas, stroke widths, colors, opacity, and drop counts.
Sampled backdrop glass intentionally exposes no paint layers because the
material shader owns that path. Runtime and executor summaries count active,
shadow, fill, edge, dropped, capacity, and maximum-inflation values so a
fallback artifact can prove what was drawn without backend-local policy or
visual guessing.
Schema 33 makes container group geometry executable in artifacts: spacing
resolves to a pure pairwise threshold, each pair records whether it should blend,
morph, union, or remain separated, and the group bounds expose how large a future
shared capture could become before a backend adds container-level caches.
Schema 34 adds an explicit capability snapshot to each material plan. Platform
adapters probe runtime limits at the edge, including shader tap budget, 2D
texture dimension limit, and bounded backdrop-copy pixel budget, then pass that
immutable snapshot into `plan_material_surface`. The pure planner clamps the
quality policy and records whether the render target fits both the caller budget
and backend limits before a backend sees the command. macOS derives this from
`MTLDevice.supportsFamily` and the public Metal capability tables, then exposes
the same numbers in debug artifacts.
Schema 35 adds a pure execution audit beside the observation contract. Each
plan now reports the actual pass, execution-stage, and paint-layer counts that
the backend is expected to execute, compares them with the verifier-facing
observation contract, and rolls the result into runtime and executor summaries.
This makes a stale pass array, missing edge layer, or unbounded fallback visible
as an explicit contract mismatch before a shader or native draw call is blamed.
Schema 39 extends that audit from counts to ordering. The optical composition
declares the expected shadow/primary/edge/noise order, the audit derives the
actual order from `execution_stages[]`, and runtime/executor summaries expose
stage-order match and mismatch counts. This keeps future Liquid Glass blur,
highlight, refraction, and dither work from being reordered at the backend edge
without a machine-readable artifact failure.
Schema 40 adds backdrop color response to the same pure contract. Edge adapters
may sample the backdrop color, but the planner owns the tint adaptation:
`MaterialBackdropDescriptor.color_mean`, `color_sample_count`, and
`color_sample_status` flow inward as immutable inputs, and
`MaterialPlan.backdrop.color_response` plus `tint_color_delta` explain whether
the resolved tint was preserved, treated as neutral backdrop color, or gently
pulled toward a sampled colored backdrop.
Schema 41 adds per-container group details beside the flat summaries. Runtime
artifacts now serialize `renderer.material_container_groups[]` with each
container id, member command index, plan id, geometry, union/morph flags,
fallback path, and aggregate pair counts. This keeps SwiftUI-style
GlassEffectContainer spacing, union, and morph behavior debuggable from CI logs
without visually guessing which surface in a grouped toolbar or sidebar failed.
Schema 42 adds capture-budget counters to the same group contract:
`shared_capture_surface_count`, `shared_capture_saved_surface_count`, and
`max_shared_capture_group_surfaces` show whether grouped glass can execute as a
single shared backdrop capture instead of one capture per surface.
Schema 44 connects that pure group contract to actual macOS execution:
`material_container_execution_descriptor` resolves per-command group bounds and
shape-blend strength, and the Metal material shader uses those values to reduce
inner-edge highlight, depth, and refraction discontinuities across nearby
container surfaces. The artifact contract records
`shape_blend_execution_group_count`, `shape_blend_execution_surface_count`, and
`max_shape_blend_strength`, so CI can prove grouped glass is executing as a
coordinated surface instead of a collection of independent rounded rectangles.
Each surface's container policy stays explicit: spacing resolves
to `blend_distance`, positive spacing drives `shape_blending_expected`, union ids
select the union-proximity blend policy, Reduced Motion suppresses only morphing,
and `performance_policy` tells backend adapters whether to use a single surface,
a shared container capture, or a shared union capture path.
The current sampled-glass kernel is a pure tap-tier descriptor:
`weighted-center`, `weighted-cross-5`, `weighted-3x3-grid`, or
`gaussian-5x5`, with the resolved tap count, kernel radius, blur step scale,
and Gaussian/separable weight profile; fallback plans serialize the inactive
`none` kernel. Schema 43 switches the high-quality macOS sampled material path
from the older cardinal/diagonal integer weighting to a Gaussian 5x5 separable
profile, keeping blur visibly frosted instead of alpha-only while preserving the
bounded 25-tap shader budget. This keeps blur spread and tap-shape policy in
`plan_material_surface` while macOS Metal executes the descriptor and other
backends publish deterministic fallback metadata. macOS converts that logical
blur spread with the current content scale before sampling the framebuffer, and
reports both the content
scale and maximum physical shader step in `material_executor_summary` so Retina
and non-Retina artifacts stay comparable.
Backdrops also degrade through an explicit
`quality-policy` fallback when the pure quality policy disables sampling or
sets an unusable blur/tap budget, when the render target exceeds the resolved
`max_backdrop_pixels` budget, or when backend capability limits reject the target
dimensions or copy size. Backends execute the plan; they do not re-decide policy.
`MaterialPlan.backdrop_access` is the pure shared-capture contract for sampled
glass. A sampled plan requires a stable previous-frame source, declares
`capture_scope: shared-frame` with `capture_reason: sample-current-frame`, caps
frame capture at one copy, records the maximum capture pixels from
`render_target.pixel_count`, requires foreground text and overlays to be
excluded from the captured source, and separately records the bounded surface
sample pixels for the material shape. A supported backend that lacks a stable
previous frame emits a deterministic `no-backdrop-source` fallback with
`capture_reason: warmup-next-frame`; this explains the first-frame warmup copy
that prepares sampling for the next frame. macOS executes this by separating the
material backdrop texture from the final artifact/readback texture, copying the
backdrop source before the foreground text pass, and reporting the ordering in
`renderer.material_executor_summary`. Unsupported, reduced-transparency,
invalid, and quality-policy fallback plans keep this access contract inactive.
with zero capture/sample budgets. `MaterialPlan.backdrop` also carries pure
optical response metadata: sampled mean color, `frosting_response`,
`color_response`, `tint_response`, `saturation_response`, `depth_response`, and
numeric deltas for opacity, tint color, tint alpha, saturation, shadow alpha,
and shadow radius, plus the existing luminance floor/gain/edge deltas. Dark,
bright, flat, neutral, and colored backdrop buckets can therefore change the
actual sampled-glass inputs in the pure plan while artifacts explain the exact
response. Neutral sampled backdrops keep balanced/preserve/standard responses
and zero deltas; fallback plans report `not-sampled` responses and zero optical
deltas.
macOS performs the actual texture copy at the edge and reports it through
`material_executor_summary`, while other backends
can keep the same contract and explicitly fall back.
The executor summary now also carries sampled-material upload/draw readiness and
status strings, so an artifact can distinguish `uploaded`/`drawn` from stable
`skipped-*` edge failures without moving the decision out of the pure status
helper.
`MaterialPlan.foreground` resolves primary, secondary, and accent foreground
recommendations with a named scheme, source, estimated background luminance,
contrast ratios, minimum contrast target, derived contrast margins, named
contrast/remap policies, accessibility flags, and deterministic/vibrancy
booleans. Standard glass uses a 4.5 foreground target, while Increase Contrast
raises that pure target to 7.0 and marks the plan with `enhanced-contrast` and
`strict-theme-role-remap`. This keeps text and icon legibility policy in the
pure material layer instead of letting each backend invent foreground colors for
glass surfaces. Native backends now execute that recommendation for default
text tokens inside material surfaces and publish foreground candidate/remap
counters in the artifact executor summary.
`MaterialPlan.theme` records the explicit material-style token snapshot that
drives that foreground recommendation and material tint. It names the token
source, profile, token policy, foreground/secondary/accent/strong-accent/tint/
border colors, and per-family booleans for whether the style still matches the
default Apple-like glass theme. This keeps Finder-style icon and text color
debugging in the pure plan: backends serialize the resolved token facts instead
of guessing whether a platform palette, custom theme, or fallback path changed
the visible surface.
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
Native backends now read platform accessibility display preferences at the
edge and pass them as immutable planner inputs. macOS uses AppKit
`NSWorkspace` display options, Windows uses `SystemParametersInfoW` accessibility
and UI-effect settings, and Android uses public animation-scale and UI-contrast
APIs. Reduce Transparency resolves to the deterministic material fallback path,
Increase Contrast adjusts opacity and luminance legibility in the pure plan,
and Reduce Motion disables material noise while lowering sampled-backdrop taps.
When a platform does not expose an equivalent public signal, the value remains a
documented deterministic fallback rather than a backend-side visual guess.
The same backend now treats full-frame backdrop copies as an execution of the
pure material plan summary rather than a frame-global habit: it allocates and
blits the material backdrop texture only when `MaterialExecutorSummary` reports
required shared-frame or next-frame capture, and it publishes
`backdrop_copy_policy`, `backdrop_copy_required`, and
`backdrop_copy_skip_reason` for artifact debugging.
macOS also samples the copied foreground-excluded backdrop with a fixed 5x5
asynchronous BGRA grid. The same readback now derives both luminance statistics
and a mean RGBA backdrop color, so tint color response does not add another GPU
pass or texture copy. Completed samples are consumed by the next pure
`MaterialEnvironment.backdrop`; pending or unsupported samples degrade to the
neutral deterministic descriptor and are reported through
`renderer.material_backdrop_luma_descriptor`,
`MaterialPlan.backdrop.luma_sample_*`, and
`MaterialPlan.backdrop.color_sample_*`.

### Theme and widgets

Theme support has matured beyond the original small token set:

- color tokens, typography sizes, radius scale, spacing scale;
- state colors for hover, active, disabled, error, and focus;
- semantic colors for success, warning, info, and error;
- JSON parsing with partial overrides and string color forms;
- OS/system preference capture for font family, system/small text size,
  accessibility display settings, accent color, scroller style, scrollbar size,
  axis-specific scroll factors, and app-controlled system scroll metric usage
  before app overrides are applied;
- structural widget snapshot tests for the core widget surface.

The default theme now starts from an Apple-like glass baseline: Pretendard for
product typography, system-blue accent/focus tokens, neutral grouped
backgrounds, a translucent white surface token, and larger chrome radii. These
are portable phenotype-owned tokens; platform materials still come from the
material planner/backend contract rather than from private Apple APIs. The
baseline is now owned by the pure `phenotype_theme_contract` package and exposed
through root metadata helpers plus
`theme_matches_default_glass_contract`, which lets tests and future artifact
tools verify the intended Apple-glass theme without adding platform-specific
keys to every theme fixture. CLI and file explorer artifacts read the same
package-level contract, including macOS/Finder-style iconography,
owned/permissive SVG asset policy, usage, container, performance,
accessibility, and unsupported-backend fallback policies.
High-level `layout::glass_surface_options` / `layout::glass_surface` presets
now sit above the generic material surface API. They keep execution on the
same `MaterialRect` path while giving examples named Apple-glass roles for
window chrome, toolbar shells, toolbar groups, navigation, sidebars, content,
and status bars.

SVG and built-in icons are first-class shared UI primitives. `phenotype.svg`
parses a bounded pure SVG subset into `svg::Document` and renders through
`Painter` path commands on every backend, including common transform lists and
`M/L/H/V/Q/T/C/S/A/Z` path commands used by packaged SVG icon assets. SVG
circles lower to native `ArcTo` path segments so Apple-style round glyph
geometry does not depend on cubic approximation quality; isolated circular path
`A/a` arcs now use the same native `ArcTo` path while chained or elliptical
path arcs still lower to bounded cubic Bezier segments for portable SVG icon
compatibility.
`phenotype.icon_catalog` now holds the pure metadata for the built-in
macOS-style symbol contract, and `phenotype.icons` provides the painter-facing
original 24x24 glyph SVGs for Finder-like chrome and common app actions. The
catalog follows Apple-style proportions, text-aligned medium-scale metrics,
macOS-like rounded stroke caps/joins, and bounded secondary-stroke opacity for
symbols with detail layers without copying SF Symbols assets. Each built-in
symbol carries a semantic SF Symbols reference name and explicit policy that
the reference is only a role/style anchor; the vector artwork is an audited
permissive SVG source such as Lucide ISC,
Feather-derived Lucide MIT, Tabler MIT, Iconoir MIT, or Material Symbols
Apache-2.0, with source family, icon name, exact license, license URL, pinned
direct raw SVG URL, source revision, copyright, and Apple-asset boundary exposed
in debug metadata. The catalog also exposes `reference_sources` with the Apple HIG
and SF Symbols design-reference URLs, the W3C SVG path reference, the active
Lucide embedded-source reference, the Feather MIT license-lineage reference,
the Tabler MIT candidate reference, the Iconoir MIT candidate reference, and
the Material Symbols Apache-2.0 candidate reference. Each reference row now
also carries an official license URL, source acquisition mode, embedding
permission, notice requirement, runtime-fetch flag, and platform-extraction
flag. Apple references are explicitly marked as non-embedded Apple-owned
artwork with runtime fetch and platform extraction disabled, so future file-icon
work can be Finder-like without copying Finder or SF Symbols assets. The current
built-in catalog uses audited Lucide SVGs pinned to a fixed source revision for
all 39 symbols across toolbar, sidebar, action, and file-type roles, including
AirDrop, Shared, Sort Group, and More.
Future icon imports must use permissive SVG sources found through web/reference
research first; macOS system-icon or Finder-artwork extraction stays out of
policy unless explicit redistribution clearance is recorded in the catalog.
The file explorer package gate now also requires each packaged file-type SVG
to be canonically source-equivalent to the audited icon catalog SVG while
keeping a package SHA-256 digest, license URL, pinned raw source URL, source
revision, and Apple/system extraction flags in CLI debug output. This is the
safe path for adding Finder-like icons from web references without copying
Apple-owned assets.
A pure Finder-style file-type
tint policy now gives
folder/document/PDF/text/image/SVG image/movie/archive/audio/code/spreadsheet/presentation glyphs deterministic colors for
list and column rows without querying native platform icon services. The CLI command
`phenotype icons catalog --json` exposes the same contract for CI and LLM
debugging on Linux without importing native GUI code. `phenotype icons sources
--json` provides the shorter source-review view for adding icons from web
references: pinned Lucide raw SVG URLs, source revision, license URL, official
reference-source license URLs, source acquisition modes, symbol usage, remaining
phenotype-owned count, and explicit zero Apple/platform extraction
counts. `phenotype icons svg <name-or-reference>` exposes the exact
audited SVG source for one glyph plus its attribution and rendering capability
envelope when a renderer, parser, or icon cache needs a pure source-level
probe.
The macOS Metal
renderer executes diagonal icon strokes as triangle bodies with round caps
instead of dot chains, keeping toolbar/search/sidebar symbols continuous while
remaining bounded. Windows accepts the same SVG-driven `Path`, `FillPath`, and
`DrawArc` command surface; strokes flatten to rotated color-pipeline capsules
and small fills flatten to color-pipeline strips so the built-in catalog does
not drop or mis-layer the native command stream on non-macOS desktop runs.
Larger fills still ear-clip into the existing triangle pipeline. The catalog
exposes semantic metadata for toolbar, sidebar, action, file-type roles,
outline/fill variants, preferred rendering mode, scale, and hierarchical
opacity/reference counts so verifier artifacts can prove which icon policy was
used. It also exposes regular text-aligned symbol weight, SF Symbols
rendering-mode vocabulary, explicit monochrome/hierarchical/palette/multicolor
capability counts, supported SVG style attributes, round cap/join policy names,
normal/hovered/pressed state recipes, pressed opacity/scale contracts, and the
count of round-stroked outline symbols so the Finder icon style
can fail as a data contract before it becomes a visual-only regression. The
catalog now also exposes a role-aware macOS presentation and
interaction-tone policy: toolbar/navigation symbols resolve to 24 pt
secondary, selected, or disabled tones, while sidebar symbols resolve to 26 pt
primary or selected-row accent tones with an explicit optical offset. The file
explorer artifact records those sizes and tone-policy names so the Finder-like
icon treatment is checked as data, not as a screenshot guess. The same pure
contract now covers symbol control chrome: selected, hover, and pressed
background alpha, pressed symbol opacity/scale, borderless toolbar grouping,
sidebar row radius, compact visual hit-target policy, and a separate 44 pt
minimum activation-region policy for controls. The desktop file explorer now
routes that recipe through core `ButtonVisualState`, and core button painting
expands `HitRegion` commands from immutable style inputs, so the actual
toolbar/sidebar renderer and the artifact JSON share one pressed-state and
input-reachability source of truth.
It also records the SVG subset and arc-lowering policy so a future icon
regression can fail on an exact JSON path before anyone compares pixels. The
AirDrop sidebar glyph now uses isolated circular SVG path arcs, so the example
exercises the native arc preservation path that packaged macOS-style icons are
allowed to depend on.
The desktop file explorer also records a pure Finder chrome geometry policy for
the integrated titlebar/sidebar/toolbar/content coordinates and native
control-reserve widths, so the example can move toward Finder-like placement
without leaving an LLM to infer layout drift from pixels alone. Its sidebar
selection contract records the soft selected-row alpha policy used by the
desktop example, keeping the active Recents row light enough for Finder-style
glass instead of turning it into a heavy opaque button.
The desktop SVG thumbnail path now renders sandboxed `.svg` file bodies through
`phenotype.svg` inside the Finder-like preview frame. Debug artifacts name the
render policy, source policy, and external-resource policy so CI can prove that
the example is not copying Apple artwork, extracting macOS icons, or fetching
remote SVG resources.
The icon catalog source contract now records per-symbol platform extraction and
runtime fetch flags as false, keeping Finder-style icons tied to pinned
permissive SVG sources rather than local macOS
artwork.
File explorer packages now declare `app.icon` as a package-owned SVG asset, so
the same CLI bundle contract can later feed platform app-icon generation
without embedding Apple artwork or depending on platform symbol fonts.

Widgets currently cover:

- text, code, link, image, SVG image, icon, canvas;
- button, checkbox, radio, switch, tabs;
- progress and indeterminate progress;
- text field;
- layout containers: column, row, box, sized box, weighted, grid, scaffold,
  card, scroll view, overlay, dialog, accordion, list items, divider, spacer.

`widget::svg_image` now derives the same kind of stable paint token used by
`widget::icon`, keyed by parsed SVG document geometry/style, requested size,
and current color. Static app-owned SVG assets therefore participate in the
paint cache instead of re-running their canvas painter every frame.

The style system is usable, but still early in its material-aware layer.
`layout::material_surface` is now the low-level material container, with
first-class `layout::toolbar`, `layout::sidebar`, and `layout::status_bar`
helpers for app chrome. `icons::macos_control_button_style` exposes the same
selected/hover/pressed glass-control recipe used by symbol buttons, so
Finder-like text and canvas buttons can share native-looking chrome without
copying sidebar/toolbar constants in each example. `ButtonStyleOptions` now has
an optional `MaterialStyle`, and `widget::glass_control_button_style` /
`widget::glass_button` attach resolved material metadata to regular and canvas
buttons. `widget::glass_outline_row_button_style` extends that contract to
Finder-like selected and expanded outline rows while preserving non-material
unselected rows. `widget::glass_split_button_style` adds grouped toolbar
control metadata, including optional container/union/morph identity, without
requiring a backend policy branch.
`layout::sheet`, `layout::inspector_panel`, and `layout::command_palette` now
cover overlay panels with typed glass presets; `layout::dialog` uses the sheet
preset instead of a raw painted card.
`widget::glass_menu_item_button_style`,
`widget::glass_dropdown_button`, and `widget::glass_table_header_button_style`
cover transient overlay actions, menu-backed selection, and content-layer table
headers without moving those policies into a backend.

### CLI and packaging direction

The next product-level tool surface is a `phenotype` CLI inspired by Dioxus'
integrated `dx` workflow: one command tree for doctor/build/run/bundle,
machine-readable JSON output for automation, and explicit asset/package
metadata. phenotype should not copy Dioxus' renderer model, but the same
event/output split applies well to this engine:

```text
External input -> input abstraction -> phenotype -> output abstraction -> real renderer
CLI input      -> input abstraction -> phenotype -> output abstraction -> CLI output
```

This keeps native platform APIs, Android device control, screenshots,
filesystem writes, resource packaging, process execution, and Python/uv
transitions at edge adapters. Core planning, layout, paint, material, and
command parsing continue to receive immutable value inputs and return
deterministic value outputs. The CLI roadmap is tracked in
`docs/PHENOTYPE_CLI_ROADMAP.md`; existing shell/Python tools should remain only
until the CLI reaches command parity and can emit equivalent structured
failure shapes.
The first concrete app-level bridge is the file explorer shared input model:
`phenotype drive file-explorer` and native artifacts now expose the same
keyboard-vs-pointer focus contract, including `focus_visible`, focus target,
focus order, and a macOS-style focus ring token. Core focusable widgets also
snap a previously keyboard-visible ring off when pointer modality takes over,
so click interaction does not leave a transient ring.

### Diagnostics and LLM debugging

The current debug plane already provides important pieces:

- common `debug.platform_capabilities`;
- `debug.input_debug` for input routing, raw focus, keyboard-only
  `focus_visible`, caret, selection, and composition state;
- `debug.semantic_tree` for accessibility-oriented structure;
- `debug.platform_runtime` for shared and backend-specific runtime state;
- artifact bundles with `snapshot.json`, optional `frame.bmp`, and platform
  runtime JSON;
- native frame capture on macOS and Windows, explicit unsupported fields on
  WASI;
- remote image queue/debug state on macOS and Windows.
- resolved material plans with pass expectations, quality policy, foreground
  contrast recommendations, resource budgets, fallback paths/reasons, verifier
  expectations, pure observation contracts, pointer-specular interaction
  descriptors, derived runtime summaries, and backend executor counters.

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
- backend-native gradient shader execution remains future work for a fuller
  Apple-style material vocabulary.

### Examples

Current runnable examples under `examples/`:

| Example | Role | Current value |
|---|---|---|
| `examples/native` | Native widget showcase | Best desktop acceptance scene for shared widgets, input debug, images, scrolling, resizing |
| `examples/glass_showcase` | Material showcase | Exercises deterministic backdrop regions, macOS sampled backdrop material, all public material kinds, material foreground/vibrancy metadata, semantic material metadata, and startup artifact capture |
| `examples/file_explorer_desktop` | Finder-style desktop app example | Exercises glass toolbar/sidebar/icon-grid composition, Finder-like action clusters, document/raster-image/SVG/vector/video/folder thumbnail probes, and sandboxed select/open/read/create/duplicate/delete file workflows |
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
  share backdrop scope, spacing/blend policy, morph expectations, and capture
  reuse policy without moving those decisions into a backend;
- macOS group-edge continuity execution for eligible container surfaces, driven
  by pure `MaterialRuntimeRecord` descriptors instead of shader-local grouping
  heuristics;
- a glass showcase under `examples/` with controls/navigation as the glass
  layer and rich content below it;
- desktop and mobile file explorer examples under `examples/` that translate
  the glass contract into an app-like file-management workflow with toolbar,
  sidebar/location navigation, list content, preview, search, create, and
  delete actions;
- first-class material app-chrome helpers for toolbar, navigation, tab bar,
  sidebar, status bar, split buttons, outline/list rows, popovers, tooltips,
  context menus, popover menu items, table headers, snackbars, and toasts so
  examples do not need to hand-roll every glass container shape;
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
  foreground contrast recommendation, and a backend-resolved `MaterialPlan`;
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
- Painter has a bounded `linear_gradient_rect` API backed by the explicit
  `LinearGradientRect` wire command; unsupported external painter subclasses
  still inherit the deterministic `FillRects` fallback.

## Prompt-to-artifact checklist

| Requirement | Current evidence | Gap |
|---|---|---|
| Analyze current phenotype progress | This document, `README.md`, `docs/ARCHITECTURE.md`, `docs/DEBUG_WORKFLOW.md`, examples and tests | Keep updated as milestones land |
| Apple glass style GUI | First-class material surfaces exist with `MaterialRect`, material container/union identity, macOS sampled-backdrop rendering, pure pointer-specular interaction descriptors executed by the macOS Metal path, resolved runtime fallback plans on Windows/Android, snapshot fallback contracts elsewhere, plus `examples/glass_showcase` for the target scene shape | Add Windows/Android/Web native material rendering or keep explicit fallback |
| LLM can debug GUI completely | Debug plane exists with snapshot, semantic tree, input debug, runtime, frame capture, material metadata, resolved material plans, pure material observation contracts, interaction enablement reasons/specular descriptors, foreground-excluded backdrop access/capture contracts, pure container-group summaries, bounded material execution stages, per-stage optical input descriptors, explicit stage-capacity/drop counters, startup bundle verifier, optional pixel-region checks, material/container/group/shape/foreground/backdrop-access/interaction plan summary gates, fallback reason summary/stale-metadata gates, semantic/runtime material parity gates, material quality/resource bound gates, runtime optical scalar bounds, executor numeric bounds, sampled upload/draw readiness/status gates, foreground text execution counters, foreground-feedback guard counters, ratio-based blur probes, a glass showcase manifest, a local glass showcase gate, CI artifact builds, and a local Android contract runner | Add Android CI wiring and mirror blur-specific probes on future native material backends |
| Stability is priority | Existing tests cover core widgets, native debug, text, remote images, command parsing | Add tests before each material/backend expansion |
| Performance is priority | Existing paint cache, scissor, batching, native renderer optimizations, pure material resource bounds for blur radius, sample taps, pass count, execution stage count/capacity, dropped-stage count, backdrop pixels, shared frame capture count/pixels, surface sample pixels, bounded texture copies, deterministic fallback, pure effective-radius shape bounds, pure optical scalar bounds, pure container-group max surface counters, backend `material_runtime_summary` counters cross-checked by the verifier, and backend `material_executor_summary` budget/timing/stage telemetry guarded by artifact manifests | Keep tightening backend timing budgets as more native material renderers land |
| Runnable examples under `examples/` | Native, glass showcase, desktop/mobile file explorer, and Android examples exist | Add Android CI device/emulator wiring when runner capacity allows |
| All phenotype features testable locally | `docs/EXAMPLES_COVERAGE.md` maps current examples/tests to public surfaces and artifact expectations; `phenotype artifact verify <bundle>` validates startup bundles, optional pixel regions, exact material/container plan summaries, semantic/runtime material parity, material resource bounds, runtime numeric bounds, `examples/glass_showcase/artifact_manifest.json`, `examples/file_explorer_desktop/artifact_manifest.json`, `examples/file_explorer_mobile/artifact_manifest.json`, and `examples/android/artifact_manifest.json` through the uv-managed verifier; `phenotype drive file-explorer` covers the shared file operation, keyboard selection, and keyboard-vs-pointer focus-ring model without a native window; `phenotype artifact verify-glass-showcase` owns the local glass gate; `phenotype artifact verify-file-explorer` owns the local desktop/mobile file explorer gate; thin shell wrappers remain only for existing script entry points; `phenotype android contract` wraps the Android device/emulator artifact route | Android CI wiring remains |

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
- `phenotype artifact verify-file-explorer`;
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
- `phenotype artifact verify-glass-showcase` as the local build/capture/verify
  gate for the showcase.

### Milestone G3: backend material primitives

Goal: implement true material/backdrop behavior per backend with fallbacks.

Deliverables:

- command protocol extension for material/backdrop region or a renderer-local
  material path that is fully represented in debug output;
- macOS Metal implementation first: `MaterialRect` is resolved through the
  pure `MaterialPlan`, preserves the functional `MaterialSurfaceRole`, samples
  the previous captured framebuffer as the backdrop source only for
  Liquid-Glass-eligible roles, and uses `standard-material-fill` for content
  roles. Sampled plans apply blur, tint, saturation, luminance preservation,
  edge highlight, deterministic noise, depth/shadow, and the resolved sampling
  kernel values from the plan; runtime JSON mirrors the plan's `primary_pass`,
  sampling kernel, resource budget, fallback reason, and verifier expectations
  so a CI failure points back to the likely layer/pass;
- control surfaces are first-class functional material roles: glass checkbox,
  radio, and switch helpers expose `MaterialSurfaceRole::Control`, semantic
  material nodes, and showcase probes so verifier gates cover control indicators
  as well as panels and overlays;
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
  bounds, sampled frame/copy and surface-area ceilings, fallback reason summary,
  and stale fallback metadata checks;
- `examples/glass_showcase/artifact_manifest.accessibility.json` captures the
  same scene under reduced transparency, increased contrast, and reduced motion
  overrides so the artifact proves deterministic accessibility fallback
  behavior;
- `phenotype artifact verify-glass-showcase` builds the example, captures a
  startup artifact bundle, and applies the manifest;
- `phenotype artifact verify-glass-showcase --accessibility` wraps the
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

- `phenotype android contract` builds and installs the Android example, launches
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

The initial G0-G4 path is now landed, and segmented controls/popovers/tooltips/
context menus/sheets/inspectors/command palettes/search fields/selected rows/
split buttons/outline rows/menu items/dropdowns/table headers/disclosure rows now
use the first-class material helper path. The next useful PR should avoid another
schema-only increment unless a real failure appears. Recommended directions:

- expand first-class material-aware controls into richer scroll/list selection,
  slider, and tooltip-specific interaction states;
- tighten macOS material executor budgets after collecting a small sample of
  local and CI timing/copy values;
- add Android CI emulator wiring if runner capacity and cost are acceptable;
- begin a Windows or Android native material renderer only if it can preserve
  the same pure `MaterialPlan`, fallback, artifact, and verifier contracts.

If the next PR chooses renderer work, decide whether true material rendering
should move next to Windows Direct3D 12, Android Vulkan, or stricter
blur-specific verifier probes, and whether Android device/emulator contract
coverage should be promoted into CI.

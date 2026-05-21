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
- `material_shader_blur`
- `material_frame_history`
- `reduce_transparency`
- `increase_contrast`
- `reduce_motion`

The material capability fields intentionally mirror the pure planner gates:
`material_surfaces`, `material_backdrop_blur`, and `material_shader_blur`
describe backend support, while `material_frame_history` describes whether a
previous foreground-free frame is available for sampled backdrop execution.
The verifier treats these as required booleans, not optional hints, and
cross-checks the stable backend support fields against each serialized
`MaterialPlan.decision_trace`.

The common snapshot schema remains the source of truth for all platforms:

- `input_debug` reports the last input event, hovered/focused/pressed callback
  state, keyboard-only `focus_visible` state, `input_modality`,
  `focus_visibility_reason`, caret geometry, and composition state.
- `semantic_tree` serializes the same accessibility-oriented semantic tree on every target.
  Root-level overlays are included in paint/focus order and keep screen-fixed
  visibility semantics, so dialogs, popovers, and material probes cannot vanish
  from artifact debugging when the document underneath is scrolled.
- `platform_runtime` always includes the shared viewport, scroll, focus,
  `focus_visible`, `input_modality`, `focus_visibility_reason`, hover, and
  press state plus a platform-specific `details` object.
- Native desktop `platform_runtime.details.window` records the resolved window
  surface kind, requested `WindowOptions`, integrated titlebar metrics,
  native-control ownership, and `uses_glfw=false` / `toolkit_window_shim=false`
  so an artifact can prove whether the app is running through AppKit/Win32
  native chrome rather than guessing from a screenshot. macOS also records
  WindowServer bounds, onscreen state, z-order index, app-window order index,
  and front-window occlusion state, so a native app that owns a Dock icon but
  ordered a 0x0 window, stayed behind another app window, or failed to move into
  the active Space fails the artifact contract with an exact runtime path
  instead of a visual-only symptom. `artifact_capture_visibility_state` and
  `artifact_capture_ready` describe the local artifact gate requirement: a
  visible, onscreen native window that can be captured even when the automation
  process is not the foreground app. `visibility_state` and
  `ready_for_user_interaction` stay stricter and condense key/main/active
  readbacks for direct-run failures.

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
.exon/debug/phenotype_cli theme resolve \
  --prefer-system-color-scheme --system-color-scheme dark \
  --font-scale 1.2 --scroll 1.5 --json
.exon/debug/phenotype_cli io contract --json
```

`theme contract` reports the `phenotype_theme_contract` version, Apple-like
glass profile, Pretendard typography baseline, Liquid Glass usage boundary,
macOS/Finder-style iconography policy, owned/permissive SVG asset policy,
grouped-container policy, performance bounds, accessibility fallback policy,
unsupported-backend degradation policy, color tokens, radii, typography, and
semantic surface roles. It should stay green before debugging theme-dependent
artifact differences.
`theme resolve` is the Linux-safe pure preference probe: provide OS snapshot
fields and app overrides, then compare the JSON `resolved.effective_theme` and
`resolved.resolution` booleans with an artifact's
`application.file_explorer.preferences` block. The `input.system` object also
reports availability bits; a default value such as `font_scale=1.0` should only
set `used_system_font_scale=true` when `font_scale_available=true`, and a
fallback font family or point size should only set the system-family/metrics
evidence bits when `font_family_available` or `font_metrics_available` is true.
This keeps fallback artifacts from pretending they consumed OS preferences.

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

For icon-source regressions, start with the aggregate icon gate:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli icons check --json
```

`icons check` combines the pure catalog, embedded source-provenance, and
Finder-style file-type icon checks. It reports whether every toolbar, sidebar,
action, and file-type symbol still uses audited permissive SVG sources, whether
the Lucide source URLs are pinned to the catalog revision, and whether any
Apple-owned, platform-extracted, or runtime-fetched artwork has entered the
contract. Use the narrower `icons catalog`, `icons sources`, or `icons
file-types` commands only after the aggregate gate identifies which group
drifted.

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
.exon/debug/phenotype_cli drive file-explorer --json \
  --input key:tab \
  --expect focus-visible:true \
  --expect focus-target:sidebar \
  --expect input-modality:keyboard
.exon/debug/phenotype_cli drive file-explorer --json \
  --input click:README.txt \
  --expect focus-visible:false \
  --expect focus-target:content-grid \
  --expect input-modality:pointer
```

The drive output reports the typed input trace, sandbox paths, visible entries,
viewport, view mode, pure Finder chrome/grid metrics, selection capabilities,
operation receipts with resolved operation plans, preview excerpt, and desktop
keyboard command descriptors plus the shared `input_model`.
It is useful for validating
select/open/read/create/duplicate/delete/view-mode/resize/shortcut and
keyboard-selection/focus-modality model behavior before running the slower
desktop/mobile artifact capture gate.
Each operation plan records the sandbox-relative source and destination,
whether the action reads a file, writes a file, creates a directory, moves to
Trash, permanently deletes from Trash, and which fallback reason blocked a
failed operation.
The parser accepts desktop-style aliases including `key:enter`, `key:delete`,
`key:escape`, `key:arrow-up`, `key:arrow-down`, `key:arrow-left`,
`key:arrow-right`, `key:home`, `key:end`, `key:page-up`, `key:page-down`,
`key:tab`, `shift-tab`, `shortcut:find`, `shortcut:duplicate`, and
`shortcut:new-folder`, matching the native key-command registry used by the
desktop example.
`key:tab`, `shift-tab`, `focus:*`, `pointer:*`, and `click:*` expose the
same focus contract as the renderer: keyboard focus navigation sets
`focus_visible=true`, while pointer focus updates the target but keeps the
ring hidden. The `input_model` records `last_input_modality`, `focus_target`,
`focus_visible`, the focus order, the macOS-style focus ring token, and the
reason a ring is or is not visible. The native shell applies the same reset
before platform-consumed left-button presses, which prevents titlebar, IME, or
other edge adapters from preserving a keyboard ring after a pointer click.
The shared debug plane mirrors this at top level:
`debug.input_debug.input_modality` and
`debug.input_debug.focus_visibility_reason` describe the most recent input
event, while `debug.platform_runtime.input_modality` and
`debug.platform_runtime.focus_visibility_reason` describe the rendered frame.
File Explorer also clears `focus_visible` for pointer/programmatic commands such
as view-mode, sort, refresh, and toolbar action clicks even when the logical
focus target remains unchanged.
Core focusable widgets also snap the focus-ring border back to the resting
state when pointer modality hides a previously keyboard-visible ring, so a
mouse click cannot leave a short-lived blue fade-out that looks like click
focus.
Native file explorer artifact bundles expose the same model state under
`debug.application.file_explorer`: profile, location, status, sort mode, view
mode, selected entry plus index, operation receipt, entry counts, pure chrome
metrics, keyboard command descriptors, and the `input_model`. The operation
receipt embeds the same plan shape as CLI drive output, so a startup artifact
can explain the file effect contract without replaying the UI.
The same object now includes `finder_visual_contract`, which is the first field
to inspect when a Finder-like visual regresses. It mirrors the shared chrome
metrics into an LLM-friendly summary: platform-owned traffic lights/caption
buttons, no content or artifact markers, reserve-only titlebar geometry, soft
sidebar selection with a zero-width selected-row border, keyboard-only focus
rings, permissive SVG provenance, zero Apple/SF Symbols embedded assets, zero
platform-extracted icons, and local artifact verification rather than default
PR capture. A mismatch is reported as `finder-visual-contract` before pixel
regions are interpreted.
The desktop payload includes Finder chrome counts, sidebar symbol/label metrics,
selected-row radius plus soft selected-row alpha policy, the
`phenotype.icon_catalog` / `phenotype.icons` style contract
(`design_reference`, `asset_policy`, 24x24 alignment grid, stroke width,
total/sidebar/toolbar/file-type/filled symbol counts,
SF Symbols rendering-mode names,
regular text-aligned weight policy, monochrome/hierarchical/palette/multicolor
capability counts, and
`interface_metaphor_policy`, `visual_consistency_policy`,
`toolbar_symbol_chrome_policy`, `sidebar_symbol_color_policy`,
`interaction_tone_policy` / `file_type_color_policy`, and
`metrics_policy` / `hit_target_policy`, plus `document_cache_policy` for the
parsed SVG icon cache), native-control reserve
coordinates, and icon-grid density metrics such as column width, row height,
pitch, thumbnail canvas size, label size, gap, visible rows, and visible
capacity. Column view also records location-pane row count, row height, and
icon size so Finder-style navigation rows can be checked from the artifact
without reading pixels by eye. Entry samples include the resolved
`symbol` and `symbol_semantic_reference_name`, so PDF/text/archive/image/SVG image/movie/audio/code/spreadsheet/presentation
fallback mistakes can be diagnosed from JSON before inspecting the screenshot.
The shared payload also emits `entry_symbol_summary`, which records that the
visible desktop or mobile entries were mapped by
`file_explorer_shared::entry_symbol` into the `file_type` presentation role.
Use `application.file_explorer.entry_symbol_summary.*` when a mobile Browse
row shows the wrong glyph; use `entries_sample[]` only when the exact filename
mapping needs inspection.
`phenotype icons catalog --json` emits the complete all/sidebar/toolbar/file-type symbol
contract, name/reference lookup invariants, macOS role metrics, and SVG source
presence checks from the same pure metadata package.
`phenotype icons lookup <name-or-reference> --json` is the narrow metadata
probe for one glyph when a Finder token maps to the wrong visual metaphor or
hit target. `phenotype icons svg <name-or-reference>` emits the exact
audited SVG source for that glyph, with `--json` adding the semantic reference
name, asset policy, source attribution, Apple-asset boundary, and rendering
capabilities. `phenotype icons present <name-or-reference> --role ... --phase
... --json` resolves the exact macOS-style presentation recipe for one glyph
state, including visible RGBA, background chrome, effective size, hit target,
source attribution, and likely layer/pass.
`phenotype icons sources --json` is the compact license/provenance probe:
it lists every pinned Lucide raw SVG source, the symbols that use it, the
source revision, exact license URL, remaining phenotype-owned count, and
checks that Apple/SF Symbols artwork was not embedded, platform-extracted, or
runtime-fetched.
`phenotype icons render <name-or-reference> --role ... --phase ... --json`
emits the same state as a standalone SVG wrapper with explicit source bytes,
viewBox, optional `--output` write path, background chrome, and
`standalone_svg_wrapper` pass metadata. Use these
icon probes when a renderer, path parser, or icon-source cache is suspect, while
`phenotype drive file-explorer --json` embeds the desktop chrome geometry and
icon-system contract under `chrome.geometry` and `chrome.icon_system`, including
the resolved sidebar/toolbar/file-type presentation arrays and sample
normal/pressed/disabled recipes, Finder density policy, icon-grid top inset,
plus per-entry file-type symbol names and `entry_symbol_summary`; the
same output now links each resolved file-type symbol to its package SVG asset
name/source and includes the default glass theme contract under `theme_system`.
The verifier can assert those paths with `require_debug_details`, which keeps
Finder workflow failures debuggable without relying on a screenshot guess.
The desktop thumbnail contract is adjacent to those metrics under
`chrome.thumbnail_system`: PDF pages, raster image strips, SVG vector previews,
movie strips, and shadow policy are all named separately, and
`uses_external_previews=false` confirms that no native Quick Look or platform
icon-service query is needed for CI artifacts. SVG thumbnails also expose
`svg_render_policy`, `svg_preview_source_policy`, and
`svg_external_resource_policy`, plus `svg_document_cache_policy` and
`svg_document_cache_limit`; together these say that the desktop preview renders
the sandbox file body through `phenotype.svg`, caches parsed preview documents
at the edge, does not extract macOS
icons, and does not fetch external SVG resources.
To verify that path explicitly, run the local-only scenario:

```sh
tools/phenotype_cli/.exon/debug/phenotype_cli artifact verify-file-explorer \
  --profile desktop \
  --view-mode icon \
  --scenario svg-selected
```

The native runtime window payload also reports
`artifact_capture_visibility_state`, `artifact_capture_ready`,
`visibility_state`, and `ready_for_user_interaction`; when a Dock icon exists
without a usable window, the failure should point at
`debug.platform_runtime.details.window.*` before any pixel-region analysis
starts. Local artifact gates should use the artifact capture fields, while
manual launch debugging can inspect the stricter key/main/active fields.

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
the pure `GlassProbeContract`, active probe count, expected pass, sampling
kernel, luminance, fallback metadata, and expectation results. Native artifacts
expose the same contract at `debug.application.glass_showcase`, so a verifier
failure can identify the intended material probe before pixel-region analysis
starts. It is useful for validating the probe scene's input abstraction before
running `phenotype artifact verify-glass-showcase`.

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
as a JSON contract mismatch. `svg_path_arc_symbol_count` proves that multiple
built-in Finder sidebar and file-type symbols exercise the SVG path arc parser
in normal example artifacts, while `round_stroke_symbol_count` proves outline
glyphs stay on the macOS-like round stroke contract. The desktop payload also
includes `source_attribution_policy`, `lucide_source_symbol_count`,
`lucide_unique_source_icon_count`, and `apple_asset_symbol_count` so
permissive-source adoption and Apple-asset exclusion are machine-checkable. A
healthy Finder-style desktop artifact should show 39 Lucide-backed symbols, 38
unique Lucide raw SVG source files, 0 phenotype-owned symbols, and 0 Apple/SF
Symbols assets; mismatches point to `phenotype.icon_catalog` attribution or
package-resource drift before a renderer bug. Each Lucide-backed symbol reports
the exact ISC or Feather-derived MIT license and a direct raw SVG URL pinned to
the catalog revision, plus `platform_extracted=false` and
`runtime_fetch_required=false`, so a future LLM can audit provenance without
trusting `main` or assuming a local macOS icon service was used. The
same payload should include
`document_cache_policy=explicit_symbol_document_cache_keyed_by_symbol_descriptor_no_frame_parse_churn`;
platform icon extraction must stay disabled unless a future source records
explicit redistribution clearance in the attribution payload.
if it is missing, suspect the icon runtime/cache contract before spending time
on backend paint output. The
file-explorer `resource_system.file_type_icon_source_map` repeats that
attribution next to each packaged `assets/icons/file-types/*.svg` declaration,
so package bundle bugs and catalog provenance bugs can be separated from a
single artifact. `phenotype package inspect --json` goes one step further for
checked-in package files: it computes each SVG digest, parses the SVG subset,
rejects native window-control palette markers, and requires each file-type SVG
to be canonically source-equivalent to the audited catalog source. A
`svg_file_type_icon_provenance` failure with `canonical_match=false` means the
package asset drifted from its pinned permissive source even if the rendered
shape still looks similar. For a package-independent source audit, run
`phenotype icons file-types --json`; it emits the same file-type token list,
semantic references, package asset paths, pinned Lucide raw SVG URLs, license
metadata, and Apple-artwork boundary without touching the filesystem. The
`reference_sources` array names the exact Apple HIG/SF Symbols
semantic references, W3C SVG path reference, Lucide embedded-source reference,
Feather MIT license-lineage reference, Tabler MIT future-source candidate,
Iconoir MIT future-source candidate, and Material Symbols Apache-2.0
future-source candidate used by the policy. Each row includes the official
license URL, acquisition mode, embedding permission, notice requirement,
runtime-fetch flag, and platform-extraction flag, so a future LLM can reject an
unsafe icon import from JSON alone.
Apple rows must stay
`used_as_embedded_asset_source=false`, `may_embed_svg_source=false`,
`apple_owned_artwork=true`, `runtime_fetch_allowed=false`, and
`platform_extraction_allowed=false`; a failure there points to provenance
policy before any renderer investigation. macOS system-icon extraction is also
out of policy unless a future reference row records explicit redistribution
clearance. The
desktop payload also includes
`sidebar_symbol_tokens`, the renderer-facing table that maps sidebar tokens to
audited symbols and semantic SF Symbols reference names, so a wrong
Recents/Shared/Desktop-style metaphor fails as data before anyone compares
pixels. `file_type_symbol_tokens` performs the same check for icon-grid
fallback symbols, including PDF, text, image, movie, archive, audio, code,
spreadsheet, presentation, document, and
folder entries. `sidebar_symbol_presentations`,
`toolbar_symbol_presentations`, `file_type_symbol_presentations`, and
`presentation_samples` then resolve those semantic tokens into the same
role/phase/selected/disabled recipes used by `phenotype icons present`, so
alpha, effective point size, hit target, and likely icon layer/pass can be
checked before looking at pixels. `symbol_control_chrome_policy`,
`symbol_interaction_phase_policy`, toolbar/sidebar pressed background alpha,
pressed symbol opacity, and pressed scale describe the macOS-style normal,
hovered, pressed, selected, and disabled symbol chrome that the example uses
before rendering. Core `widget::symbol_button` now composes that recipe with
`ButtonVisualState`, semantic labels, and deterministic paint tokens for
toolbar/navigation/action controls, so desktop Finder toolbar glyphs and mobile
file-explorer navigation controls can prove from JSON and pixels that they are
consuming the pure icon recipe rather than painting separate ad hoc pressed
styles. Mobile Browse rows consume the same file-type symbol contract through
canvas buttons, so row glyph color, semantic label, and paint token should be
debugged through `entry_symbol_summary`, `file_type_symbol_presentations`, and
the row's button semantic label before pixel-region checks.

For a package-owned or ad hoc SVG file, use `phenotype svg inspect <path>
--json` before blaming a backend. The output includes the pure SVG subset
policy, supported elements/path commands/style attributes, source byte count,
viewBox, shape count, unsupported command count, diagnostics, paintability, and
the renderer-facing `Painter` path from `phenotype_svg_contract`, the same
contract named by `phenotype.svg`. Filesystem access happens only in the CLI, so
a failure here points at the SVG source or parser contract rather than the native
image cache.

`phenotype package inspect --json <package>` now runs the same SVG contract for
every declared package SVG asset. Check
`resource_catalog.svg_asset_inspections[*]` or the `svg_asset_inspection` check
before launching a renderer: `present`, `bytes`, `sha256`, `paintable`,
`unsupported_count`, and `diagnostics` tell whether the packaged vector image is
valid under the same bounded parser that `widget::svg_image` uses. For
file-type icons, inspect `catalog_source`: it parses the embedded audited SVG
source tied to the pinned permissive URL and reports whether the packaged asset
keeps the same viewBox and shape contract without depending on a CI network
fetch.

The same artifact exposes `application.file_explorer.theme_system.*` from the
pure `phenotype_theme_contract` package. That block names the Apple-like glass
theme profile, Pretendard font policy, material planning boundary,
macOS/Finder-style iconography policy, owned/permissive SVG asset policy, Liquid
Glass usage boundary, grouped-container policy, performance bounds,
accessibility fallbacks, and unsupported-backend degradation. Check it first
when a Finder artifact looks visually off but material plans and icon metadata
still pass: theme drift, material planning, backend execution, and example
geometry now have separate JSON owners.

`debug.platform_capabilities.system_settings` records OS-derived typography,
preferred locale, accent, scrolling, and input timing before the theme overlay
runs.
macOS fills this from CoreText, `NSFont.systemFontSize` / `smallSystemFontSize`,
`NSEvent.hasPreciseScrollingDeltas`, `NSScroller.preferredScrollerStyle`,
`NSEvent.doubleClickInterval` / `keyRepeatDelay` / `keyRepeatInterval`,
`NSLocale.preferredLanguages`, and
regular-control scroller width; Windows from `SystemParametersInfoW`, DWM,
`GetSystemMetrics`, `GetDoubleClickTime`, `GetCaretBlinkTime`, and
`GetUserDefaultLocaleName`; Android from
`Resources.getConfiguration()` and `ViewConfiguration`; and unsupported targets
publish deterministic fallback values. Accessibility display inputs sit next to
the renderer details:
macOS uses `NSWorkspace`, Windows uses `SystemParametersInfoW`, and Android uses
public animation-scale and UI-contrast APIs. File explorer artifacts mirror the
result under
`application.file_explorer.preferences`: `system_settings` is the edge snapshot,
`app_overrides` is the pure override input, and `effective_theme` is what the
example applied before rendering. `resolution` is emitted from the core
`ResolvedThemePreferences` value, so it records whether the effective theme
actually consumed OS font metrics, OS scroll metrics, OS appearance, user font
scale, user font sizes, user line height, user scroll speed, OS Reduce Motion,
or user motion scale. The mirrored
`system_settings` object includes availability booleans for font family, font
metrics, font scale, line height, scroll metrics, color scheme, Reduce Motion,
and accent color so a fallback value is not mistaken for an OS preference. Use this block
when text size, family source, font-weight adjustment, vertical or horizontal
wheel/trackpad speed, scrollbar behavior, touch slop, double-click timeout, key
repeat timing, caret blink, or accent color looks wrong but the
material/icon/resource contracts are green. The vertical and horizontal scroll
multipliers are separate
so Android `ViewConfiguration` factors, Windows
`SPI_GETWHEELSCROLLCHARS`, and macOS trackpad/wheel paths can be diagnosed
without replaying native input. The startup locale resolves an explicit app
override first, then the OS preferred locale, then the package default locale.
Pretendard remains the package default font
family; file explorer examples opt in to system appearance and accent by
default, while the OS font family is still an explicit app/user override.
`app_overrides.apply_system_scroll_metrics=false` disables static platform
scroll multipliers such as future backend-provided scale factors; per-event
platform deltas still originate at the native edge. On macOS, inspect
`debug.platform_runtime.details.input.scroll` when scroll speed feels wrong:
it records the raw `NSEvent.scrollingDeltaX/Y`, whether AppKit marked the event
as precise, the theme multipliers, normalized logical-pixel deltas, scroll
phase, and handled flags. That path is the supported evidence trail because
AppKit exposes OS-adjusted event deltas rather than a public global scroll
speed scalar.
For text input feel, inspect `double_click_interval_ms`,
`key_repeat_delay_ms`, `key_repeat_interval_ms`, `caret_blink_interval_ms`,
and `input_timing_source`; native shells consume the caret interval only at the
input edge, while pure theme planning remains unaffected.
The file explorer examples refresh their native system-settings snapshot at
update-boundary theme sync. If an artifact disagrees with the current OS
settings, first check `application.file_explorer.preferences.system_refresh_policy`
and compare `debug.platform_capabilities.system_settings` with
`application.file_explorer.preferences.system_settings` before changing theme
tokens.

`application.file_explorer.resource_system.*` is the package/debug-resource
counterpart. It records the file explorer application id/version/entry,
declared platforms, SVG/preload/runtime-visible asset counts, app icon
SVG/preload state, default locale/font state, locale coverage, Pretendard CJK
fallback status, file-type icon asset count/source revision/license asset, and
debug manifest/probe/verifier declarations. Use it when an artifact looks
correct but package inspection, i18n, icon loading, or the verifier route might
be stale.

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
input replay. The same route covers application preference inputs such as
`font-family:system`, `font-scale:1.2`, `font-size:17`,
`heading-font-size:22`, `small-font-size:13`, `line-height:1.45`,
`system-scroll-metrics:app`, `scroll-speed:1.4`, and
`horizontal-scroll-speed:2`; artifacts then
record `application.file_explorer.preferences.source=application-input` plus
the resolved effective font metrics and axis-specific scroll multipliers.
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
Native backends read accessibility display preferences at the platform edge by
default. For deterministic artifact capture, set
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
- `optical_response`, the pure response summary that classifies the material as
  sampled Liquid Glass, content-layer standard material, deterministic fallback,
  or inactive, then records blur/color/depth strategies and booleans for
  backdrop, blur, frosting, tint, saturation, luminance preservation, edge
  highlight, bounded refraction, shadow depth, noise/dither, foreground
  vibrancy, and deterministic fallback;
- `refraction`, the schema-37 pure bounded edge-lens profile. It names the
  refraction model and source, records active/backdrop/interaction/reduced-
  motion/bounded flags, and exposes the exact strength, edge bias, and maximum
  pixel offset uploaded by sampled-backdrop material shaders;
- `optical_composition`, the pure execution recipe for the same surface. It
  names the blur, frosting, tint, luminance, refraction, depth, interaction, and
  fallback sources, records which optical channels are required, proves the
  composition is bounded and deterministic, and exposes the exact scalar values,
  sample taps, texture-copy pixels, and surface-sample pixels consumed by stage
  optics;
- raw `geometry`, derived `shape` analysis, tint, blur radius, saturation,
  luminance curve, edge highlight, noise, and shadow values for the resolved
  plan. `shape.kind` classifies the backend-executable geometry as
  `rectangle`, `rounded-rectangle`, `capsule`, or `invalid`, and
  `shape.effective_radius` is the clamped radius the backend executes;
- `render_target`, including target dimensions, scale, pixel format, pixel
  count, readiness, and whether the backdrop-pixel budget was satisfied;
- `decision_trace`, including the pure gate booleans for geometry, quality,
  backend capabilities, backdrop-source readiness, reduced transparency,
  increased contrast, reduced motion, and the first blocker that explains the
  fallback path;
- `backdrop_sampling`, `fallback`, `fallback_path`, and `fallback_reason`;
- `backdrop`, including source, readiness flags, whether foreground text was
  excluded from the source, sanitized luminance statistics, luminance response,
  optical frosting/tint/saturation/depth responses, and
  floor/gain/edge/opacity/tint-alpha/saturation/shadow deltas;
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
  source, estimated background luminance, contrast ratios, contrast margins,
  minimum contrast target, named contrast/remap policies, accessibility flags,
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
  primary pass/executor, expected total/active/backdrop pass counts,
  execution-stage counts, texture-copy bounds, shared and next-frame capture
  expectations, capture reason, capture/sample pixel bounds, safety flags, and
  the region/layer/pass hints.
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
Material-backed controls should therefore have stable semantic labels, not just
pixel signatures. For example, `widget::tabs` emits a segmented-control
material node through `TabsStyleOptions`, and Finder-style examples can require
labels such as `Density Segmented Control` or `Mobile Mode Segmented Control`
to prove that the pure material planner saw the control chrome.
Search fields follow the same rule: examples label their glass text inputs as
`Search Field` or `Mobile Search Field`, which lets verifier output explain
that the input surface produced a real `MaterialPlan` instead of an ordinary
painted rectangle. Finder-style selected rows use
`widget::glass_selection_button_style`; the desktop manifest now expects one
extra sampled sidebar selection plan, an isolated interactive container, and a
bounded increase from 200 to 225 total sample taps. If that count changes,
start with the selected sidebar row semantic material node before investigating
the backend.
Popover action rows follow the same semantic-material rule through
`widget::glass_menu_item_button_style`. The desktop `more-actions-open`
scenario requires an `overlay` material surface role while the menu is visible;
disabled actions intentionally resolve to non-material buttons. If that
requirement fails, inspect the `More Actions Menu` semantic subtree and the
`New File` / `New Folder` / `Duplicate` icon-button nodes before looking at
Metal or pixel thresholds.
Table/list headers use `widget::glass_table_header_button_style`. The desktop
list-view gate requires the `Name`, `Kind`, and `Size` header labels and a
resolved material plan; those headers should appear as `content` role material
plans, which means a standard-material content-layer pass rather than another
sampled backdrop pass. If a list header disappears from the semantic tree,
check the sort header button before debugging the content surface itself.
Disclosure rows follow the same content-layer path:
`layout::accordion` headers carry `widget::glass_disclosure_header_style`,
the `accordion-header` semantic role, the title label, and a resolved clear
`content` material plan. If an accordion body toggles correctly but the header
loses its material plan, inspect the disclosure style helper before looking at
layout state.
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
`MaterialPlan.interaction` records the pure interaction response that feeds
that reference model. `enabled` mirrors a non-inactive interactive material
container or `MaterialSurfaceOptions.interactive` surface opt-in, `active` is
true only when hover, press, focus, or pointer presence produces a positive
response strength, `state` uses the stable
`inactive`/`idle`/`focused`/`hovered`/`pressed` vocabulary, and
`enablement_reason` explains the pure eligibility decision as
`inactive-material`, `noninteractive-container`, or `interactive-container`.
`response_model` is either `none` or `liquid-glass-interaction`.
Reduced-motion plans keep the response deterministic, cap the response
strength, and report `motion_policy: reduced-motion-static`. The delta fields
(`opacity_delta`, `blur_radius_delta`, `saturation_delta`,
`edge_highlight_delta`, `shadow_alpha_delta`, and `shadow_radius_delta`) explain
exactly which optical channels changed before the backend executed a pass.
Pointer-driven active plans also publish `specular_model`,
`specular_highlight_active`, normalized `specular_anchor_x/y`, and bounded
`specular_radius` / `specular_intensity`. A backend should consume those fields
as shader input; it should not invent its own pointer highlight policy.
The verifier cross-checks those deltas against
`MaterialPlan.optical_response.interaction_active` and
`interaction_modulates_optics`, so a failed hover/press response identifies the
JSON path and the likely `material-interaction` layer instead of requiring a
screenshot guess.

When an interaction response is missing, inspect both
`MaterialPlan.command_descriptor.interaction` and the input debug plane. Native
and WASM adapters feed pointer coordinates into `AppState`; paint resolves
pointer containment against the material rectangle and merges it with callback
hover/press/focus state before emitting `MaterialRect`. A focused material cue
requires keyboard-visible focus (`focus_visible=true`); pointer focus should
leave `interaction.focused=false` while still allowing hover, press, or
`pointer_inside` to drive glass response. If `command_descriptor.interaction`
shows the expected hover/press/pointer data but `MaterialPlan.interaction`
still has `enabled=false`, inspect `MaterialPlan.interaction.enablement_reason`
first. `noninteractive-container` points to
`MaterialPlan.command_descriptor.container.interactive` and the source
`MaterialSurfaceOptions.interactive` / `material_container(... interactive=true)`
call; `inactive-material` points to material kind resolution. Only investigate
backend passes after the pure plan reports `interactive-container`.
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
kernel that does not exceed the policy limit. The pure policy sanitizer clamps
that limit to the engine cap (`material_max_sample_taps`, currently 25) and
also clamps executable blur to `material_max_blur_radius` (currently 36 px)
before a backend receives the plan. The artifact verifier repeats those cap
checks for `quality_policy` and `resource_budget`, so over-budget shader work
fails with the exact JSON path instead of becoming backend-specific behavior.
`sampling_kernel.name` is `weighted-5x5-manhattan` for active sampled glass and
`none` for deterministic fallback at the default 25-tap quality tier. Lower
quality tiers use `weighted-center` (1 tap), `weighted-cross-5` (5 taps), and
`weighted-3x3-grid` (9 taps) before the 13/25-tap
`weighted-5x5-manhattan` descriptor. Its `radius` and `blur_step_scale` are
part of the pure contract and are uploaded to the macOS material shader, so
changing blur spread requires changing the plan, serializer, verifier
vocabulary, and docs together. macOS also reports
`material_executor_summary.material_shader_content_scale` and
`material_max_shader_blur_step_pixels`; the verifier compares those values with
`MaterialPlan.render_target.scale`, `blur_radius`, and the sampling kernel's
`blur_step_scale` so HiDPI blur shrinkage appears as an exact JSON-path failure.
Reduced-motion plans disable material noise and cap backdrop sample taps before
a backend executes the pass;
increased-contrast plans raise opacity and luminance legibility in the same
pure layer.
`luminance_curve` is the backend-executed contrast transform. Active sampled
glass must use `adaptive-backdrop-luma` with `backdrop_driven: true`; fallback
plans must use `fallback-flat` with `backdrop_driven: false`. `floor` and
`gain` must match `MaterialPlan.luminance_floor` and
`MaterialPlan.luminance_gain`, while `gamma`, `midpoint`, `contrast`, and
`edge_lift` are bounded shader inputs. A curve failure should be debugged from
`MaterialPlan.backdrop` first, then from the backend shader input upload. The
backdrop object now includes `luma_sample_count`,
`luma_sample_grid_width`, `luma_sample_grid_height`, `luma_sample_frame`, and
`luma_sample_status`. On macOS, non-zero values mean the pure plan consumed a
completed asynchronous 5x5 previous-frame sample; zero with `not-sampled` means
the plan used deterministic neutral luminance for that frame.
`foreground` is the material text/icon legibility contract. Active sampled glass
must report `backdrop_driven: true` and `uses_vibrancy: true`; deterministic
fallback reports a fallback or accessibility source. The verifier checks that
every foreground contrast ratio meets `minimum_contrast_ratio`, each
`*_margin` equals `*_contrast_ratio - minimum_contrast_ratio`, and
`high_contrast: true` raises the minimum to at least `7.0` with
`contrast_policy: enhanced-contrast`. Scheme, source, contrast-policy, and
remap-policy values are known vocabulary, so foreground failures point at pure
material policy before backend drawing. Native backends then apply this plan to
primary, secondary, and accent text tokens whose draw origins fall inside a
prior material command.
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
For schema 32 and later, inspect `paint_layers[]` whenever a material surface is
not sampled backdrop glass. This array is the pure fallback draw recipe, not
backend telemetry: shadow, fill, and edge layers expose their executor,
geometry deltas, color, opacity, and bounded-resource flag. Sampled backdrop
plans must keep the array empty. The verifier recomputes paint-layer counts from
the array and compares them with `observation.expected_*_paint_layers`,
`renderer.material_runtime_summary`, and
`renderer.material_executor_summary`, then reports exact JSON paths and likely
layers when a backend serializes stale or hidden fallback policy.
For schema 33 and later, inspect `renderer.material_runtime_summary.container_groups`
and `renderer.material_executor_summary.container_groups` when grouped glass
looks wrong. The verifier derives the same object from `renderer.material_plans[]`
and checks group count, multi-surface groups, union/morph/interactive groups,
shared-backdrop scope, fallback mixing, shape-pair counts, blend/union/morph
candidate pairs, separated pairs, min/max shape gap, max blend distance, and max
group bounds. A mismatch points at `layout::material_container`,
`MaterialSurfaceOptions` inheritance, `MaterialRect` container payload encoding,
or backend summary finalization before it points at shaders.
For schema 34 and later, inspect each plan's `capability_snapshot` before
debugging shader output. It names the backend capability profile/source and the
runtime limits that entered pure planning: shader taps, 2D texture dimension,
backdrop-copy pixel budget, and whether the render target fits those limits.
The verifier checks that `decision_trace.capability_*` mirrors the snapshot and
that `backdrop_pixels_within_budget` combines the quality budget with those
backend limits. A failure here points at the platform capability provider or
`MaterialEnvironment` construction, not at Metal fragment math.
For schema 35 and later, inspect `execution_audit` immediately after
`observation_contract`. It compares the observation contract with the actual
serialized pass, stage, and paint-layer arrays. A mismatch points at pure
planning, command serialization, or debug JSON assembly. It should not be fixed
by adding backend policy branches.
For schema 37 and later, inspect `refraction` and `optical_composition` before
comparing stage optics or shader constants. A mismatch in
`execution_stages[n].optics.refraction_*` should trace back to these pure
objects, while a mismatch inside `optical_composition` itself points at
`plan_material_surface` policy or stale JSON serialization.
`renderer.material_backdrop_luma_descriptor` is the edge-side view of that same
contract. It reports the latest observed luma min/max/mean, sample grid, sample
frame, pending state, skipped sample count, and status such as
`sampled-async-grid` or `unsupported-fallback`. If a material plan looks neutral
while the frame image is visibly dark or bright, inspect this object before the
shader: it tells you whether the backend had a completed sample available for
the pure planner.
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
`verifier_require_container_morph_contract`. Container group gates can also pin
`container_group_count`, `container_multi_surface_group_count`,
`container_union_group_count`, `container_morph_group_count`,
`container_interactive_group_count`,
`container_shared_backdrop_scope_group_count`,
`container_fallback_mixed_group_count`, `container_max_group_size`,
`container_max_active_surfaces`, `container_max_sampled_backdrop_surfaces`, and
`container_max_fallback_surfaces`.
Within each plan, the verifier now validates the pure container policy fields:
`blend_distance` must mirror `spacing`, `shape_blending_expected` must follow
positive spacing on participating containers, `blend_policy` must match isolated,
touching-only, container-proximity, or union-proximity behavior, `morph_policy`
must explain static, active, union, or Reduced Motion-suppressed morphing, and
`performance_policy` must describe whether the backend should use a single
surface, shared container capture, or shared union capture path.
Reference-model gates can pin `reference_technologies`, `reference_variants`,
`reference_shapes`, `reference_shape_scopes`, `reference_blending_scopes`,
`reference_semantic_thickness`, `reference_view_bounds_anchored`,
`reference_shape_matches_geometry`, `reference_tint_applied`,
`reference_interactive_response`, `reference_container_grouped`,
`reference_container_union`, `reference_container_morphing`,
`reference_legibility_preserved`, `reference_vibrancy_expected`, and
`reference_deterministic_degradation`.
Foreground gates can pin `foreground_schemes`, `foreground_sources`,
`foreground_contrast_policies`, `foreground_remap_policies`,
`foreground_backdrop_driven`, `foreground_high_contrast`,
`foreground_vibrant`, `foreground_deterministic`,
`foreground_min_primary_contrast_gte`, and
`foreground_minimum_contrast_gte`. Per-plan verifier failures now also report
exact foreground margin paths such as
`renderer.material_plans[n].foreground.primary_contrast_margin` when a
serializer or planner drifts from the derived readability contract.
`backdrop.luminance_response` is `not-sampled` for fallback plans and one of
`neutral`, `dark`, `bright`, `flat`, `dark-flat`, or `bright-flat` for sampled
plans. Sampled plans also report `frosting_response`, `tint_response`,
`saturation_response`, and `depth_response`; fallback plans keep those fields
at `not-sampled`. The adjacent delta fields show whether the pure planner
actually changed the luminance floor, luminance gain, edge highlight, opacity,
tint alpha, saturation, shadow alpha, or shadow radius for that backdrop.
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
On native backends, also inspect
`debug.platform_runtime.details.renderer.accessibility_display_options`; it
records whether the frame used live system settings, deterministic fallback
settings, or an explicit environment override before the pure planner produced
`decision_trace.reduced_transparency`, `decision_trace.increase_contrast`, and
`decision_trace.reduce_motion`.
For Finder-style desktop windows, inspect
`debug.platform_runtime.details.window`: `chrome=integrated_titlebar`,
`window_options_present=true`, and matching `integrated_titlebar.*` metrics
mean the example requested native integrated chrome, including the leading
traffic-light reserve and trailing caption-button reserve. `native_controls_owned_by_os=true`
with `uses_glfw=false` confirms close/minimize/maximize controls and caption
hit testing stay at the platform edge rather than being redrawn by phenotype.
The same `window` object now includes `native_window_controls`: on macOS the
verifier expects AppKit `standardWindowButton:` controls to report
`ownership_policy=platform_edge_standard_buttons_only`, all three visible
buttons, `all_buttons_within_leading_reserve=true`, and
`integrated_in_content_area=true`. That check follows AppKit's full-size
content/titlebar transparency model, where content can sit under the titlebar
while the standard window buttons remain in the window hierarchy.
The file explorer's `debug.application.file_explorer.chrome` additionally
publishes `content_window_control_markers=false`,
`artifact_window_control_markers=false`, `duplicate_window_controls=false`, and
`window_control_marker_mode=runtime-native-controls`. It also reports
`native_window_control_owner=platform-edge`, native-control count, zero
content/artifact marker and drawn-control counts, the
`platform_standard_controls_inside_leading_content_reserve` integration policy,
and the
`native_controls_runtime_only_no_content_or_artifact_markers` render policy. It
also reports
`native_window_control_geometry_role=reserve_metrics_only_not_paint_instructions`
and
`native_window_control_palette_policy=traffic_light_palette_forbidden_in_content_and_artifacts`.
Those fields make the titlebar-control metrics reserve-only data, not a drawing
recipe for red/yellow/green controls. The
verifier checks that invariant independently of the manifest and pairs it with
low-detail, neutral pixel-region checks plus forbidden traffic-light palette
checks over the leading control reserve, so a future content-drawn
traffic-light marker fails before it can ship as a duplicate of OS controls.
`phenotype package inspect` additionally rejects any manifest-declared package
SVG asset that embeds macOS traffic-light marker colors, so app icons, file
icons, and runtime SVG images cannot reintroduce the same native-control visual
cue. The chrome object
also publishes the native titlebar drag/control reserve widths plus a
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
`debug.application.file_explorer.input_model` is the native-artifact mirror of
`phenotype drive file-explorer`: `focus_visibility_policy` must remain
`keyboard_tab_navigation_shows_ring_pointer_click_hides_ring`, `focus_visible`
must be false for pointer/click startup scripts, and the shared debug plane
must report `input_modality=pointer` plus
`focus_visibility_reason=pointer_input_hides_focus_ring` for pointer repros.
Keyboard-driven Tab repros should flip `focus_visible` to true with a concrete
`focus_target` and `focus_visibility_reason=keyboard_focus_navigation`.
On macOS, `titlebar_transparent=true`, `full_size_content_view=true`,
`title_hidden=true`, and `background_drag_enabled=true` are read back from the
live `NSWindow`, not inferred from the request, so a Finder-style artifact can
detect a shell that accepted `WindowOptions` but failed to apply AppKit chrome.
Finder-style sidebar glass also depends on the native window compositor staying
transparent before the material pass executes. Check
`window_opaque=false`, `window_background_clear=true`,
`window_background_alpha=0`, and `metal_layer_opaque=false` in the same
`window` object. Then check the native underlay:
`native_backdrop_underlay_enabled=true`,
`native_backdrop_underlay_kind=nsvisualeffectview`,
`native_backdrop_underlay_placement=sibling-underlay`,
`native_backdrop_underlay_alpha=1`,
`native_backdrop_expected_alpha=1`,
`native_backdrop_underlay_blending_mode=behind-window`, and
`native_backdrop_underlay_state=active`. Finder-style sidebar windows request
`native_backdrop_expected_material=sidebar`, so
`native_backdrop_underlay_material` must also be `sidebar`; generic integrated
titlebar windows keep the default `under-window-background` material. The
underlay must be a sibling behind the Metal view rather than a parent wrapper;
otherwise changing native backdrop opacity can fade the rendered content
instead of only changing the compositor material strength. The `status`,
`ready`, `failure_reason`, `likely_layer`, and `likely_pass` fields are produced
by the pure shared `plan_native_backdrop_composition()` contract from native
window/readback inputs, so macOS supplies AppKit evidence but does not invent a
backend-local failure vocabulary. The same
`window` object summarizes the combined contract in
`glass_backdrop_composition`: `status=ready`, `ready=true`,
`failure_reason=none`, `requires_native_backdrop_underlay=true`,
`requires_sibling_underlay=true`,
`native_backdrop_underlay_placement=sibling-underlay`,
`native_backdrop_underlay_alpha=1`,
`native_backdrop_expected_alpha=1`, and a policy such as
`transparent-window-clear-metal-native-sidebar`. If it is `blocked`, use
`failure_reason`, `likely_layer`, and `likely_pass` before guessing from pixels.
The renderer must also report
`renderer.clear_alpha=0`, `renderer.clear_alpha_for_transparent_window=true`,
`renderer.full_frame_opaque_fill_count=0`, and
`renderer.transparent_window_has_opaque_frame_fill=false`; otherwise an opaque
theme clear command or root background fill will flatten the drawable before
the WindowServer/AppKit backdrop can show through. If those fields drift, fix
AppKit/CAMetalLayer setup, clear-alpha handling, or the app root background
before tuning sidebar material tint, blur, or opacity; an opaque native surface,
missing `NSVisualEffectView` underlay, opaque Metal clear, or full-frame opaque
fill hides the real blurred wallpaper/backdrop even when the pure
`MaterialPlan` requests sampled glass.
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

For macOS material execution, inspect
`debug.platform_runtime.details.renderer.material_executor_summary` before
looking at pixels. `backdrop_copy_policy` must be
`copy_only_when_material_plan_requires_shared_or_next_frame_capture`;
`backdrop_copy_required=false` with
`backdrop_copy_skip_reason=no-material-plan-frame-capture` is the expected
stable path for frames that contain no functional liquid-glass capture work.
When the pure planner requires warmup or sampled backdrop access,
`backdrop_copy_required=true`, `backdrop_copy_count=1`, and
`backdrop_copy_pixels` should match the drawable size unless the skip reason
names a Metal texture or blit-encoder failure.
For sampled material passes, also check `material_upload_status` and
`material_draw_status`. These are derived from edge facts in
`material_executor_summary`: `material_pipeline_ready`,
`material_backdrop_source_ready`, upload bytes, draw calls, and the sampled
plan count. A status of `uploaded`/`drawn` means the backend executed the
sampled pass; a `skipped-*` value names the missing edge dependency without
requiring a visual guess.

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
`backdrop_capture_reasons`, `luminance_responses`, `frosting_responses`,
`tint_responses`, `saturation_responses`, `depth_responses`,
`render_target_pixel_formats`, `pass_executors`, `stage_names`,
`stage_executors`, `sampling_kernels`,
`sampling_weight_profiles`, `luminance_curves`, `decision_blockers`,
`foreground_schemes`, `foreground_sources`, `verifier_profiles`,
`verifier_region_layers`, `verifier_region_passes`, `container_modes`,
`container_ids`, `union_ids`, `theme_profile_names`, `theme_sources`,
`theme_token_policies`, `optical_response_models`, `optical_blur_strategies`,
`optical_color_strategies`, and `optical_depth_strategies`;
it can also count
`container_participating`, `container_unioned`, `container_interactive`,
`container_morph_transitions`, `verifier_require_backdrop_source`,
`verifier_require_edge_highlight`, `verifier_require_container_identity`, and
`verifier_require_container_morph_contract`. For Apple-style grouped glass, it
can additionally assert pure aggregate group facts:
`container_group_count`, `container_multi_surface_group_count`,
`container_union_group_count`, `container_morph_group_count`,
`container_interactive_group_count`,
`container_shared_backdrop_scope_group_count`,
`container_fallback_mixed_group_count`, `container_max_group_size`,
`container_max_active_surfaces`, `container_max_sampled_backdrop_surfaces`, and
`container_max_fallback_surfaces`. Per-plan container validation also checks
the derived blend, morph, and performance policy strings, so a grouped-glass
failure can point to planner policy drift before inspecting backend output.
Optical gates can additionally pin
`optical_backdrop_driven`, `optical_blur_active`, `optical_frosting_active`,
`optical_tint_active`, `optical_saturation_active`,
`optical_luminance_preservation_active`, `optical_edge_highlight_active`,
`optical_depth_shadow_active`, `optical_noise_dither_active`,
`optical_foreground_vibrancy_active`, `optical_interaction_active`,
`optical_interaction_modulates_optics`, and `optical_deterministic_fallback`.
Interaction gates can additionally pin `interaction_enabled`,
`interaction_active`, `interaction_hovered`, `interaction_pressed`,
`interaction_focused`, `interaction_pointer_inside`,
`interaction_reduce_motion`, `interaction_deterministic`,
`interaction_specular_highlight_active`, and exact maps for
`interaction_states`, `interaction_enablement_reasons`,
`interaction_response_models`, `interaction_specular_models`, and
`interaction_motion_policies`. Runtime gates can also check
`renderer.material_runtime_summary.interaction_specular_highlight_count`,
`max_interaction_specular_radius`, and
`max_interaction_specular_intensity`, with matching executor-summary counters
when a backend turns the plan into draw inputs.
Theme gates can additionally pin
`theme_foreground_matches_theme`, `theme_accent_matches_theme`,
`theme_tint_matches_surface`, `theme_border_matches_theme`, and
`theme_default_glass_tokens`. Platform system-settings failures now include
appearance and accessibility fields as first-class evidence: inspect
`debug.platform_capabilities.system_settings.color_scheme`,
`appearance_name`, `color_scheme_source`, `reduce_transparency`,
`increase_contrast`, `reduce_motion`, `accessibility_source`,
`text_size_source`, `scroll_source`, `scroll_vertical_factor`,
`scroll_horizontal_factor`, `scroll_bar_size`, `input_timing_source`,
`double_click_interval_ms`, `key_repeat_delay_ms`, `key_repeat_interval_ms`,
and `caret_blink_interval_ms` before changing theme tokens.
File explorer artifacts also mirror the app override
state at `debug.application.file_explorer.preferences.app_overrides` and the
resolved theme at `debug.application.file_explorer.preferences.effective_theme`,
including `color_scheme`, font family/scale, `apply_system_scroll_metrics`,
`apply_system_reduce_motion`, both scroll multipliers, and
`motion_duration_multiplier`. `debug.application.file_explorer.preferences.system_settings`
must include `font_family_available`, `font_metrics_available`,
`font_scale_available`, `line_height_available`, `scroll_metrics_available`, and
`color_scheme_available`, plus `reduce_motion_available` and
`accent_color_available` when those captures are reported. The local file
explorer artifact gate pins the app color-scheme override to `light` for stable
pixel thresholds while still recording OS appearance availability; the adjacent
`debug.application.file_explorer.preferences.resolution` object names the pure
resolver decisions, such as `used_system_font_metrics`,
`used_system_scroll_metrics`, `used_user_font_size`, and
`used_user_scroll_scale`, `used_system_reduce_motion`, and
`used_user_motion_scale`, before native runtime input deltas are considered.
macOS runtime artifacts additionally expose
`debug.platform_runtime.details.input.scroll.normalized_delta_x/y` and
`app_vertical_multiplier` / `app_horizontal_multiplier` for the last local
scroll event.
Foreground gates can additionally
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
include the same pass entry. `observation_contract.expected_runtime_passes`
mirrors the serialized pass list, while `expected_active_runtime_passes` and
`expected_backdrop_runtime_passes` state which of those entries are expected to
execute and sample backdrop. When this fails, inspect the pass serializer before
changing material policy. Fallback metadata is similarly strict: fallback plans
must report a non-empty `fallback_reason`, while non-fallback plans must leave
`fallback_reason` empty so stale downgrade explanations cannot leak into glass
artifacts.
The verifier also treats material kind, fallback path, reference-model strings,
material pass names, sampling kernel names, and luminance curve names as fixed
vocabularies. Current fallback paths are `none`, `no-material`, `invalid-geometry`,
`unsupported-backend`, `no-backdrop-source`, `reduced-transparency`, and
`quality-policy`; current pass names are `none`, `backdrop-sample-blur`, and
`translucent-rounded-rect`; current sampling kernels are `none`,
`weighted-center`, `weighted-cross-5`, `weighted-3x3-grid`, and
`weighted-5x5-manhattan`; current sampling weight profiles are `none`,
`center4`, `center4-cardinal2`, and `center4-cardinal2-diagonal1`; current
luminance curves are
`adaptive-backdrop-luma` and `fallback-flat`. Current reference blending scopes
are `none`, `sampled-backdrop`, and `deterministic-fallback`; current reference
shapes are `none`, `invalid`, `rectangle`, `rounded-rectangle`, and `capsule`; current
backdrop capture scopes are `none` and `shared-frame`. Each
`execution_stages[].optics.channel` is also vocabulary-checked (`backdrop-filter`,
`standard-fill`, `fallback-fill`, `shape-shadow`, `edge-highlight`,
`noise-dither`, or `none`) and its numeric fields are cross-checked against the
surrounding `MaterialPlan` scalars. Add new
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
backend's view of executed material work drifts from the resolved plans. This
includes optical maxima such as `max_saturation`, `max_edge_highlight`,
`max_edge_width`, `max_noise_opacity`, `max_shadow_alpha`, and
`max_shadow_radius`; if a glass frame looks visually flat, these fields show
whether the pure response reached the runtime artifact before inspecting pixels.
The same cross-check applies to
`renderer.material_runtime_summary.container_groups.*`, so grouped, unioned,
morphing, interactive, shared-backdrop-scope, and mixed-fallback glass groups
must match the pure summary.
Backends also serialize `renderer.material_executor_summary` for edge-only
work that cannot be derived from the pure plan, including material instance
count, fallback instance count, primary executor instance counts for sampled
backdrop, standard fill, and deterministic fallback fill, material draw calls,
encoded material sample tap totals, planned shared frame capture count/pixels,
planned surface sample pixels, next-frame capture plan count, upload bytes/capacity,
framebuffer-history copy pixels, and CPU
enqueue timings. Foreground execution counters report how many text commands
landed inside material surfaces and how many default material text tokens were
remapped to `MaterialPlan.foreground`. Use
`require_runtime_numeric_bounds` for CI-safe limits on those numeric runtime
paths. Each entry names a path under `debug.platform_runtime.details` and can
provide `equals`, `gte`, and/or `lte`; failures report the exact path plus the
likely `material-executor` pass when the path targets the executor summary.
Whenever material plans are present, the verifier also cross-checks executor
counts, including `material_executor_summary.container_groups.*`, against
`renderer.material_plans#summary`: `plan_count`,
`fallback_instance_count`, `material_instance_count`,
`sampled_backdrop_instance_count`, `standard_fill_instance_count`,
`deterministic_fallback_instance_count`, `material_max_sample_taps`, and
`material_total_sample_taps` must match the resolved plan aggregate.
`fallback_instance_count` means explicit
`MaterialPlan.fallback == true`; a content-layer `standard-material-fill` pass
is counted as a material instance with zero sample taps, not as fallback.
The primary executor instance counters are stricter: they are derived from
`MaterialPlan.primary_pass.executor`, so standard fills and deterministic
fallback fills cannot be confused with sampled backdrop blur work.
Executor stage counters must also match the pure
`execution_stages` summary: total stages, active stages, backdrop-dependent
stages, dropped stages, primary runtime stages, backdrop-filter stages,
fallback-fill stages, shape-shadow stages, edge-highlight stages, and
noise/dither stages. Stage `optics` values must also match the pure scalar each
stage consumes, which makes shadow-radius, edge-highlight, and noise failures
actionable before comparing pixels. Backdrop access counters must match the pure
`backdrop_access` aggregate, including next-frame warmup capture requests, and
any actual framebuffer-history copy must fit the planned shared capture count
and pixel budget. The resource-bound gate can
also require
`max_execution_stage_capacity_*` and `dropped_execution_stages_*` so artifacts
prove that stage growth stayed inside the serialized capacity. Draw calls must
stay within sampled backdrop instances times the pure pass budget, upload bytes
must fit the reported material buffer capacity, and copied backdrop pixels must stay
within the pure resource budget. Foreground remaps must also be less than or
equal to foreground text candidates, otherwise the backend has counted a remap
without a material surface hit.
The verifier additionally checks the sampled upload/draw requirement booleans
against `sampled_backdrop_instance_count`, checks uploaded/drawn booleans against
the numeric counters, and recomputes `material_upload_status` and
`material_draw_status` from the same facts. If one fails, the JSON report points
at the exact status path and the likely `material-executor` pass.
For sampled-backdrop PRs, require the foreground-exclusion booleans in both the
plan summary and executor summary. This keeps the final artifact/readback frame
complete while proving the material shader sampled a separate safe backdrop
texture.
Use `require_material_quality_policy` when a material gate must prove the
resolved pure policy stayed enabled and bounded. It can require backdrop
sampling, noise, and shadow to remain allowed for every plan, and can bound the
maximum quality-policy blur radius, sample taps, and backdrop pixel budget seen
in the artifact. Even without manifest overrides, the verifier rejects
`quality_policy.max_blur_radius`, `quality_policy.max_sample_taps`,
`resource_budget.max_blur_radius`, or `resource_budget.max_sample_taps` values
that exceed the engine caps surfaced by `phenotype.material`.
`max_backdrop_pixels_lte` is the pure planning limit for backdrop copy/blur
eligibility; compare it with
`renderer.material_executor_summary.backdrop_copy_pixels` when diagnosing how
many pixels the backend actually copied in a frame.
On macOS this budget is intentionally larger than the generic 4M fallback
because the native edge adapter probes Metal support and advertises a bounded
16M backdrop-copy budget for Retina-sized windows. The artifact still proves the
actual copied pixels separately, so the budget increase is debuggable rather
than hidden in backend policy.

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

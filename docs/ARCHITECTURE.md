# Architecture

## Overview

phenotype = **platform-agnostic core** (C++23 modules) + **platform backends**.

The core handles widgets, layout, state management, vDOM diff, and draw-command emission. It knows nothing about windows, GPUs, or font files — those are provided by the backend via a thin host interface.

- **Core modules**: types, material, io, theme_contract, state, layout, paint, commands, diag, theme_json
- **Backend contract**: `phenotype_host.h` (5 host functions) + `phenotype.commands` (typed draw commands)
- **Current backends**: WASM+JS (production), macOS native (AppKit shell + CoreText + Metal), Windows native (Win32 shell + DirectWrite + Direct3D 12), Linux stub native backend
- **Planned backends**: platform-specific native renderers/text systems behind the same shell + command-buffer contract

## Rendering pipeline

```
State change (message dispatch)
  │
  ▼
View function builds layout tree (Arena + LayoutNode)
  │
  ▼
vDOM diff v2 — copies layout for unchanged subtrees
  │
  ▼
layout_node() computes positions (skips layout_valid nodes)
  │
  ▼
paint_node() emits draw commands into cmd buffer (65 KB linear memory)
  │
  ▼
flush_if_changed() — FNV-1a hash, skip if identical to previous frame
  │
  ▼
Backend consumes the cmd buffer:
  ├── WASM:   JS shim parseCommands() → WebGPU renderer
  └── Native: phenotype::parse_commands() → platform renderer
```

## SVG And Iconography

`phenotype.svg` is a bounded vector-image subset for product UI assets and
built-in symbols. It parses `svg`, `g`, `path`, `rect`, `circle`, `ellipse`,
`line`, `polyline`, and `polygon`; path data supports `M/L/H/V/Q/T/C/S/A/Z`
with absolute and relative forms. The implementation intentionally lowers the
parsed document to phenotype `PathBuilder` commands, so native, WASM, and test
surfaces consume the same draw-command path instead of calling platform SVG
APIs from core code.

`phenotype.icons` builds on that parser with phenotype-owned and audited
permissive SVG symbols. The catalog keeps macOS/Finder/SF Symbols names only as
semantic references: it does not embed Apple or SF Symbols artwork. Most
toolbar, sidebar, action, and file-type glyphs currently use audited Lucide SVG
sources pinned to a fixed source revision, with each symbol reporting either
the Lucide ISC license or the Feather-derived MIT license when the upstream
license file requires it. The policy also allows future Tabler MIT, Iconoir
MIT, or Material Symbols Apache-2.0 sources. Every embedded external source
exposes family, icon name, exact license, license URL, pinned direct raw SVG
URL, source revision, copyright, modification status, and Apple-asset boundary
in debug metadata.
The catalog also exposes structured reference-source rows for Apple HIG/SF
Symbols, W3C SVG paths, Lucide, Feather Icons, Tabler Icons, Iconoir, and
Material Symbols. Each row records the official license URL, acquisition mode,
embedding permission, notice requirement, runtime-fetch flag, and
platform-extraction flag, so provenance checks can distinguish style
references, license lineage, future source candidates, and embedded asset
sources without inspecting screenshots.
`icons::macos_presentation(...)` is the
pure role/state mapper for toolbar, navigation, sidebar, file-type, and action
symbols. It resolves tone, opacity, scale, hit target, and optical offset from
explicit immutable inputs, while backends only paint the resulting SVG paths.
`widget::symbol_button` is the core control helper for those symbols: it lowers
the same pure recipe to `ButtonStyleOptions`, `ButtonVisualState`, a centered
SVG paint callback, semantic button labels, and deterministic paint tokens so
examples do not hand-roll macOS-style toolbar chrome.
File explorer package SVGs are checked against this catalog at the CLI edge:
`phenotype package inspect` parses each declared `file_type.*.icon` asset,
records its SHA-256 digest, verifies that it has no native traffic-light
palette markers, attaches the pinned source attribution, and requires the
package SVG to be canonically source-equivalent to the audited catalog SVG.
That leaves formatting freedom for checked-in SVG files while still catching
asset drift, platform icon extraction, and accidental Apple/Finder/SF Symbols
artwork before a renderer sees the package.

## View-time animation

Visual transitions are bound at view time, not paint time. A widget
asks for a value via `animate_color` / `animate_float` /
`animate_value<T>` (all in `phenotype.cppm`); the call returns the
current interpolated value to drop into a `LayoutNode` field, and the
runtime advances the interpolation automatically across rebuilds:

```cpp
node.background  = animate_color(is_pressed ? pressed : (is_hovered ? hover : base), 150);
node.border_width = animate_float(is_focused ? 2.0f : 1.0f, 150);
```

### Per-call-site state via `framework_local`

The interpolation's previous value, active target, and start timestamp
live in `framework_local<AnimationState<T>>`, keyed off:

```
widget_id = hash(scope_seed,
                 hash(hash_source_location(loc),
                      hash(extra_key, per_scope_sibling_counter)))
```

- `loc` defaults to `std::source_location::current()` at the
  `animate_*` call site.
- `extra_key` is the `key` argument that propagates through to
  `framework_local` (most widgets pass `0`); it lets a caller
  disambiguate keyed list iterations or other repeats it can't avoid.
- `per_scope_sibling_counter` lives on the active `Scope`. Each new
  call at a given `loc` increments the counter, so two `widget::button`
  invocations in the same row pick distinct widget ids without any
  manual key.
- `scope_seed` is reserved for parent-driven seeding so distinct
  containers can produce non-colliding ids. **Today every container
  constructs its `Scope` with `widget_id_seed = 0`**, which means two
  widgets at the same call counter inside different parent scopes
  (e.g. the first button in two separate cards) currently share state.
  The same caveat applies to every other `framework_local` consumer
  (scroll offsets, accordion expand state, etc.). A future PR will
  thread the parent's id into the seed to remove the collision; until
  then, callers that need to avoid it pass an explicit `key` (typically
  the loop index when iterating).

When `target` changes mid-flight, `animate_value` captures the current
interpolated value as the new `start_value` so the new fade picks up
where the previous one was. That makes interrupting Tab cycles or
hover entry/exit feel continuous instead of snapping.

A non-inline `detail::steady_ms()` wraps the only `<chrono>` call, so
widget templates that instantiate `animate_value<T>` see a plain
`std::int64_t` interface without pulling chrono into their
translation unit (MSVC enforces module type reachability strictly
enough for this to matter; Clang was lenient).

### Auto-tick + the `has_active_animations` flag

While any interpolation has `progress < 1.0`, `animate_value` sets
`g_app.has_active_animations = true`. The runner clears the flag at
the start of every view, so it reads as "did this frame's view
request another tick?". The native desktop shell loops poll the flag and
re-enters `trigger_rebuild()` on a ~16 ms cadence whenever it stays
set; once everything converges the flag stays false and the loop
drops back to its idle wait. Other shells (Android, wasm) inherit the
flag mechanism via the runner; their host loops grow their own
auto-tick path as a follow-up.

`trigger_rebuild` (not `repaint_current`) is the load-bearing
choice — repaint just walks the existing arena, so without rebuilding
the view the baked-in interpolated value would freeze at the start of
the fade. The shell's hover, press, focus, and scroll dispatches all promote
to `trigger_rebuild` whenever a transition is in flight (scroll stays
on the cheaper paint-only path while idle so the steady-state cost is
unchanged).

### Paint cache invalidation while animating

`paint_invalidation_mask` normally only includes the callback ids
whose hover / focus / press state changed since the previous frame, so paint
can blit cached subtree bytes for everything else. That falls down
during a fade-out: the widget that just lost focus or hover is no
longer in the diff, but its `node.border_width` /
`node.background` are still interpolating each frame. The runner
forces `paint_invalidation_mask = ~0ULL` whenever
`has_active_animations` is set during view, so every cached subtree
re-walks until the animation lands. The flag self-clears, so the
diff-driven cache returns the moment everything converges. Focus-ring hide
transitions are stricter: when pointer modality hides a ring that was visible
from keyboard navigation, focusable widgets snap their ring border back to
resting chrome instead of fading it out, matching macOS behavior where pointer
clicks do not create a visible focus ring.

## Host interface

`src/phenotype_host.h` declares 5 `extern "C"` functions that every backend implements:

| Function | Role |
|---|---|
| `phenotype_flush()` | Signal the renderer to consume the cmd buffer |
| `phenotype_measure_text(font_size, mono, text, len)` | Return pixel width of a text string |
| `phenotype_get_canvas_width()` | Viewport width in pixels |
| `phenotype_get_canvas_height()` | Viewport height in pixels |
| `phenotype_open_url(url, len)` | Open a URL in the browser or OS |

On WASM, a `PHENOTYPE_IMPORT` macro applies `__attribute__((import_module, import_name))` so the JS shim resolves them at link time. On native, the macro is empty and the backend provides implementations as regular C functions.

`Theme::default_font_family` defaults to `Pretendard`; widget text measurement
and draw-command emission use that family for proportional text and fall back
through the platform sans stack when the face is unavailable. Monospace text
keeps the platform monospace default. This keeps Korean, Chinese, and Japanese
text closer to the intended product typography without requiring every widget to
pass an explicit `FontSpec`.

The default theme uses an Apple-like glass palette: system blue for accent and
focus affordances, neutral grouped-background grays, a translucent white
surface token, and larger chrome radii. The values are phenotype-owned design
tokens rather than Apple private API or copied platform assets, so examples can
look native while remaining portable across macOS, Windows, Android, and WASI.
`packages/phenotype_theme_contract` owns the pure metadata contract for this
baseline: profile, reference, font/material policies, Liquid Glass usage
boundary, macOS/Finder-style iconography policy, owned/permissive SVG asset
policy, container grouping, performance bounds, accessibility fallbacks,
unsupported-backend degradation, color tokens, radii, typography, and expected
surface roles. The root helpers `default_theme_profile_name`,
`default_theme_reference`, `default_theme_font_policy`,
`default_theme_material_policy`, `default_theme_iconography_policy`,
`default_theme_icon_asset_policy`, `default_theme_usage_policy`,
`default_theme_container_policy`, `default_theme_performance_policy`,
`default_theme_accessibility_policy`, `default_theme_fallback_policy`, and
`theme_matches_default_glass_contract` delegate to that pure package so tests,
CLI output, and artifacts can check the same contract without giving backend
adapters any policy authority.

OS-derived typography, appearance, accessibility, scrolling, and accent-color
preferences follow the same edge-to-core rule. Native adapters collect a
`PlatformSystemSettingsSnapshot` from their platform APIs, then the core applies
it through the pure
`apply_system_theme_preferences(Theme, PlatformSystemSettingsSnapshot,
ThemePreferenceOverrides)` helper. Package defaults such as Pretendard remain
explicit theme inputs, OS font scale and scroll policy arrive as immutable
snapshot fields, and app/user overrides win without letting a backend mutate
theme state directly.
Font family source, text-size source, font-weight adjustment, vertical and
horizontal scroll factors, scrollbar size, touch slop, and scroll friction are
recorded when the platform exposes them. macOS uses CoreText for the resolved
system family, `NSFont.systemFontSize` / `smallSystemFontSize` for UI text
sizes, `NSEvent.hasPreciseScrollingDeltas` semantics for line-vs-pixel wheel
normalization, and `NSScroller.preferredScrollerStyle` plus regular-control
scroller width for scrollbar policy,
Windows uses `SystemParametersInfoW(SPI_GETNONCLIENTMETRICS)`,
`SPI_GETWHEELSCROLLLINES`, `SPI_GETWHEELSCROLLCHARS`, `GetSystemMetrics`, and
DWM, and Android uses `Resources.getConfiguration()` plus
`ViewConfiguration`. The base theme keeps Pretendard as the product default;
switching to the OS family is an explicit app override, while OS text scale and
axis-specific scroll multipliers are safe pure inputs by default.
The same snapshot records `color_scheme`, `appearance_name`, and accessibility
display booleans (`reduce_transparency`, `increase_contrast`, `reduce_motion`).
macOS resolves appearance through `NSApplication.effectiveAppearance` and
accessibility display preferences through `NSWorkspace`. Android reads
`Configuration.uiMode` for night mode and the existing public contrast/animation
bridges for accessibility. Windows records accessibility through
`SystemParametersInfoW`; app light/dark preference is captured separately from
the documented Personalize registry value and marked with its own source string.
Apps opt into system appearance with `prefer_system_color_scheme` or force a
deterministic `color_scheme` override such as `light`, `dark`, or
`high-contrast-dark`. The pure helper applies the Apple-like dark palette only
from those explicit overrides, so artifact tests can keep deterministic light
expectations while real apps can follow the OS.
Accent colors are also captured in this snapshot. macOS resolves
`NSColor.controlAccentColor` in a generic RGB color space, Windows reads
`DwmGetColorizationColor` and decodes its documented `0xAARRGGBB` value, and
Android reads `android.R.color.system_accent1_600` through the same Java
resource bridge used for `fontScale`. Applying that accent to app chrome is an
explicit pure override (`apply_system_accent_color`) so verifier scenes can
keep deterministic blue contracts until their pixel expectations become
dynamic.
The Android native edge cannot obtain `fontScale` from NDK
`AConfiguration`, so the GameActivity driver passes the Java activity object
to the backend and the backend reads
`Resources.getConfiguration().fontScale` through JNI when producing the same
snapshot. If that bridge is unavailable, Android falls back to the deterministic
1.0 scale and reports the fallback source in diagnostics.

`layout::glass_surface_options` and `layout::glass_surface` provide the
high-level Apple-glass surface presets used by examples. They are not a new
renderer path: each preset lowers to `MaterialSurfaceOptions`, then to the same
pure `MaterialStyle` and `MaterialRect` command contract as
`layout::material_surface`. The preset enum captures product roles such as
window, toolbar, toolbar group, navigation, sidebar, content, and status bar so
example code does not hand-roll blur thickness, semantic role, alignment, and
chrome radius for every surface.

`phenotype.svg` is a pure vector image layer. It parses a bounded SVG subset
(`svg/viewBox`, `g`, `path`, `rect`, `circle`, `ellipse`, `line`, `polyline`,
`polygon`, `fill`, `stroke`, `currentColor`, opacity, stroke width, and
stroke cap/join attributes, plus `translate`/`scale`/`rotate`/`matrix`
transforms) into a `svg::Document`.
Path data supports `M/L/H/V/Q/T/C/S/A/Z` in absolute and relative forms, which
covers the rounded and reflected curves common in interface glyphs without
handing SVG interpretation to a backend.
Rendering consumes only a `Painter`, target geometry, and an explicit
`svg::RenderOptions`, then emits existing `Path` / `FillPath` commands. It
never reads files, compiles shaders, probes platform SVG support, or mutates
global state. File/package loading belongs to resource and backend edges; core
SVG rendering starts from already-provided SVG text. Circular SVG shapes lower
to `PathVerb::ArcTo` segments instead of cubic approximations so backends that
own arc SDF paths can preserve SF Symbols-style round glyph geometry without a
separate SVG opcode. Isolated circular path `A/a` arcs also preserve native
`ArcTo`, which keeps small Finder-style sidebar glyphs on the same SDF arc
path. Chained or elliptical path `A/a` arcs lower to bounded cubic Bezier
segments and non-circular ellipses still use cubic curves.

`phenotype.icon_catalog` is a pure path package for the built-in symbol
contract. It contains no renderer dependency, no filesystem access, and no
platform API calls, so Linux CLI tooling and WASI docs builds can inspect the
same icon metadata as native apps. `phenotype.icons` provides the painter-facing
SVG sources and rendering helpers for that contract as original 24x24 glyphs.
The catalog intentionally follows general Apple HIG-style optical proportions
without copying SF Symbols artwork as assets. Each symbol declares a semantic
SF Symbols reference name, family, and policy, so the contract can say "this
glyph is playing the same UI role as `magnifyingglass`" without embedding
Apple's vector paths. In the current catalog, 35 of 39 built-in symbols come
from audited Lucide SVG sources pinned to the catalog revision. Feather Icons
is tracked separately as MIT license-lineage metadata for Lucide symbols
derived from Feather, while AirDrop, Shared, Sort Group, and More remain
phenotype-owned because their
Finder-specific metaphors or filled dot treatment need tighter product
control. Apps can call `icons::document`,
`icons::paint_symbol`,
or `widget::icon`; the widget helper paints through `widget::canvas` and uses a
deterministic paint token so stable icons do not re-emit every frame.
`phenotype.icons` also exposes an explicit `SymbolDocumentCache` keyed by the
pure symbol descriptor. The default widget path builds that cache once at the
runtime edge and reuses parsed SVG documents for icon paints, so toolbar,
sidebar, and file-type glyphs do not reparse XML during ordinary frame rebuilds.
Apps that need an interactive macOS-style symbol control should call
`widget::symbol_button` with an explicit `icons::SymbolButtonOptions` value.
That value selects the presentation role, selected/disabled state, optional
control size, and token salt; `phenotype.icons` then derives the button chrome,
symbol presentation, hit target, and paint token from pure immutable inputs.
The generic `widget::svg_image` path uses the same parsed-document token contract,
so app-owned SVG illustrations, package icons, and future generated symbols can
stay static in the paint cache unless their document, size, current color, or
aspect-ratio policy changes. `SvgImageOptions` makes current color,
`preserve_aspect_ratio`, and semantic labels explicit value inputs.
`widget::svg_image` also marks the resulting node as a semantic `image`, so
artifact snapshots can distinguish vector images from decorative canvas drawing
without asking a backend to decode SVG files. The
catalog encodes macOS-like rounded stroke caps and joins in each line icon's
SVG source, records those attributes in `svg::Style`, uses bounded secondary
opacity on detail strokes for SF Symbols-like hierarchical emphasis, and
exposes symbol metadata (`icon_catalog::descriptor`, stroke cap/join policies,
semantic reference names, name/reference lookup helpers, variant/rendering/scale
names, regular text-aligned weight policy, rendering capability flags for
monochrome/hierarchical/palette/multicolor, count constants, and index accessors
for all, sidebar, and toolbar symbols) so examples and artifact verifiers can assert the style contract
without pixel guessing.
The built-in set includes Finder-style file-type glyphs for folders, generic
documents, PDFs, text documents, raster and SVG images, movies, archives,
audio, code, spreadsheet, and presentation files. Apps choose those
symbols through pure filename and role metadata, then expose the resolved symbol
and semantic reference in debug JSON so native backends remain simple
executors. Each embedded symbol also carries source acquisition metadata: the
URL is pinned to a permissive source revision when the glyph is Lucide-backed,
and the attribution explicitly says that no platform icon extraction or runtime
network fetch is required.
The icon debug contract includes `document_cache_policy`, making this
performance boundary visible in artifacts and CLI output instead of leaving it
as an unobservable renderer implementation detail.

The Finder-style file explorer uses the same SVG parser for sandboxed `.svg`
file thumbnails. The renderer reads the already-loaded file body from the demo
model, resolves it through an explicit edge `SvgPreviewDocumentCache` keyed by
the file preview body, paints it through `phenotype.svg` inside the thumbnail
frame, and records that policy in `chrome.thumbnail_system`. This keeps file
icons source-safe and stable on hot paths: macOS system artwork remains a
reference only, no platform icon extraction runs on the hot path, SVG external
resources are not fetched, and normal frame rebuilds do not reparse the same
preview SVG XML.
`icons::presentation` adds the default macOS-inspired
presentation policy: toolbar symbols use 24 pt secondary/selected tones,
sidebar symbols use 26 pt primary/accent tones with a small optical vertical
adjustment, and disabled/destructive tones are explicit pure values.
`icon_catalog::metrics`, `icon_catalog::hit_target_size`, and
`icon_catalog::activation_hit_target_size` keep the Finder-like role metrics
pure as well: toolbar/navigation/action glyphs keep compact 36 pt visual
targets, sidebar glyphs align to 38 pt rows, file-type glyphs reserve larger
targets for icon-view thumbnails, and interactive controls expose at least a
44 pt activation region. This lets the CLI and examples validate macOS-style
icon sizing and Apple HIG-style button reachability without calling AppKit or
embedding SF Symbols assets.
The catalog also pins HIG-derived visual-language policies for familiar
simplified metaphors, consistent size/stroke/detail/perspective, borderless
toolbar symbols inside grouped controls, and accent-selected sidebar symbols.
`icon_catalog::macos_interaction_tone` keeps selected and disabled toolbar,
navigation, sidebar, file-type, and action tone choices in the same pure
catalog instead of leaving those decisions inside examples or native backends.
`icon_catalog::macos_control_chrome` extends that contract to selected and
hover background colors, corner radii, hit targets, and borderless/grouped
control flags. `icon_catalog::macos_state_recipe` then resolves normal,
hovered, pressed, selected, and disabled symbol states, including pressed
background alpha, symbol opacity, and scale. `phenotype.icons` mirrors those
pure catalog types into renderer-facing helpers:
`icons::macos_symbol_button_style` builds the button chrome,
`icons::macos_presentation(symbol, role, selected, ButtonVisualState)` maps the
live control state to the resolved glyph presentation, and
`icons::symbol_button_paint_token` keeps the canvas cache key aligned with that
state. `widget::symbol_button` composes those helpers on top of
`widget::canvas_button`, so Finder-like toolbar/navigation/action controls use
the same visual, press-state, and activation-region contract in both examples
and tests while remaining LLM-debuggable without consulting AppKit.
The same pure catalog exposes a Finder-style file-type symbol set and tint
policy for folder, document, PDF, text, image, movie, archive, audio, code,
spreadsheet, and presentation glyphs, which desktop
and mobile file explorer examples use in rows without asking a native icon
service for platform artwork. Their shared debug payload includes
`entry_symbol_summary` so the contract can fail as JSON before a screenshot
comparison is needed.
The desktop and mobile packages also declare those eleven file-type icons as
runtime-visible SVG assets under `assets/icons/file-types/`, sourced from the
same pinned Lucide revision as the pure catalog. This keeps package inspection,
resource bundling, and runtime fallback painters on one source-safe icon
contract while still avoiding Apple-owned Finder or SF Symbols artwork. The
packages also carry the Lucide license notice as a non-runtime text asset, so a
resource bundle preserves the permissive-source notice alongside the copied SVG
files. Entry debug JSON links each resolved file-type symbol back to the
package asset name/source, which lets CI logs explain whether an icon mismatch
belongs to filename classification, the package manifest, or SVG rendering.
`resource_system.file_type_icon_source_map` also ties each package asset to the
catalog source attribution, including direct pinned raw SVG URL, license,
`platform_extracted=false`, and `runtime_fetch_required=false`.
The catalog also publishes the SVG subset and arc-lowering policy, so CLI
checks and example artifacts can catch a regression where a macOS-style glyph
depends on an unsupported path command. AirDrop intentionally uses SVG `A/a`
path arcs in the built-in catalog, giving the Finder-like sidebar a real
runtime probe for the arc-lowering path instead of leaving that support covered
only by unit tests.
Finder-style examples serialize sidebar, toolbar, and file-type semantic
reference arrays plus the resolved presentation policy in their artifact debug
payload, and
`phenotype icons catalog --json` exposes the complete contract to CI and LLM
debugging before anyone inspects a screenshot. `phenotype icons svg
<name-or-reference>` exposes the exact built-in SVG source from the same pure
catalog with the matched rendering capability envelope, so renderer and parser
failures can be reproduced without launching a native window. `phenotype icons
sources --json` is the compact provenance audit for web-sourced icons: it
lists each pinned Lucide raw SVG source, the symbols using it, license URL,
source revision, phenotype-owned symbols, and the Apple/platform-extraction
boundary checks without dumping every catalog presentation field. `phenotype icons
present <name-or-reference>` resolves the same state recipe for a chosen role,
phase, selected state, and disabled state, including the effective visible
symbol color, background chrome, hit target, and likely icon layer/pass.
`phenotype icons render <name-or-reference>` then wraps that recipe into a
standalone SVG hit target, giving package probes and visual diffs the same pure
icon output path as the native examples. This
lets CI logs and artifact triage explain a toolbar/sidebar/file-type icon mismatch
without relying on a human visual guess. The style reference deliberately says the custom SVGs are
informed by Apple HIG, macOS Finder, and SF Symbols while the asset policy
continues to forbid copied Apple/SF Symbols vector artwork.
Package app icons are separate resources, not part of the glyph catalog: they
are declared as `app.icon` SVG assets in `phenotype.package.toml`, copied by the
CLI bundle step, and validated by `phenotype.resources` before a platform
packager maps them to `.app`, Windows, Android, or web icon formats. Generic
SVG assets use the same package contract (`image/svg+xml` plus a `.svg` source
suffix), and `phenotype package inspect --json` reports total/preload/runtime
SVG counts plus `svg_asset_inspections` for each declared SVG file. Those
inspections read files only at the CLI edge, parse them with the pure SVG
contract shared by `phenotype.svg`, and report viewBox, shape count,
unsupported command count, diagnostics, paintability, bytes, and SHA-256 so
asset regressions are visible in CI logs. File-type SVG assets add a second
catalog-source inspection: the CLI parses the embedded audited source associated
with the pinned permissive URL and verifies the packaged file keeps the same
viewBox and shape contract without fetching the network in CI.
`phenotype svg inspect <path> --json` is the narrow local edge probe for one file
and uses the same contract, so SVG failures can be debugged without AppKit,
Metal, or platform image decoders. The shared `phenotype_svg_contract` package
keeps that asset inspection available to the Linux CLI gate without depending
on the native renderer package.

## Input Command Boundary

Core application code can register hidden key commands with
`phenotype::app::key_command`. The command descriptor is pure app data: key,
normalized modifiers, focus policy, callback id, and an optional debug label.
Native shells translate platform key codes at the edge, then ask the core to
dispatch a matching descriptor. The core enforces exact key/modifier matching
and the `allow_when_input_focused` policy before invoking the callback, so
platform adapters do not decide product shortcuts.

Raw focus and visible focus are separate. Pointer focus updates
`focused_id` for routing clicks, text input, and activation, but leaves
`focus_visible=false` so toolbar buttons do not grow a ring after a mouse
click. Tab and Shift-Tab traversal set `focus_visible=true`; controls read
that keyboard-only flag for macOS-style focus-ring chrome, while text inputs
can still present their caret and selection from raw focus. Native shells also
clear `focus_visible` before forwarding a left-button press to any platform
input adapter, so AppKit/IME/titlebar hooks that consume the pointer event
cannot leave a stale keyboard ring behind.
Pure app input models mirror this split: pointer and programmatic commands that
do not move the logical focus target still clear `focus_visible`, while only
keyboard traversal or explicit keyboard focus commands can enable the ring.

This keeps the long-term input/output abstraction model testable without a
native window. `examples/file_explorer_shared` publishes Finder-style desktop
commands as data, `examples/file_explorer_desktop` registers those descriptors
with the core, and `phenotype drive file-explorer` accepts matching `key:` and
`shortcut:` aliases against the same shared model, including arrow/home/end/page
selection navigation. The shared file explorer model also emits a pure
`input_model`: last input modality, current focus target, focus order, focus
ring style token, and `focus_visible`. `key:tab` / `shift-tab` are keyboard
inputs and therefore show the ring; `click:*` and `pointer:*` keep the target
but hide the ring. Native artifacts mirror the same object under
`debug.application.file_explorer.input_model`, so CLI output and renderer
artifacts explain focus-ring behavior with the same value contract.

## Command buffer protocol

The cmd buffer (`phenotype_cmd_buf[65536]`) uses a binary encoding with
append-only opcodes:

| Opcode | Name | Payload (after 4-byte opcode) |
|--------|------|------|
| 1 | Clear | u32 color |
| 2 | FillRect | f32 x, y, w, h; u32 color |
| 3 | StrokeRect | f32 x, y, w, h, line_width; u32 color |
| 4 | RoundRect | f32 x, y, w, h, radius; u32 color |
| 5 | DrawText | f32 x, y, font_size, rotation, width_factor; u32 flags, color, family_len; bytes family (4-byte aligned); u32 text_len; bytes text (4-byte aligned) |
| 6 | DrawLine | f32 x1, y1, x2, y2, thickness; u32 color |
| 7 | HitRegion | f32 x, y, w, h; u32 callback_id, cursor_type |
| 8 | DrawImage | f32 x, y, w, h; u32 len; bytes url (4-byte aligned) |
| 9 | Scissor | f32 x, y, w, h |
| 10 | DrawArc | f32 cx, cy, radius, start_angle, end_angle, thickness; u32 color |
| 11 | Path | f32 thickness; u32 color, verb_count; path verb stream |
| 12 | FillPath | u32 color, verb_count; path verb stream |
| 13 | FillQuads | u32 quad_count; repeated u32 color + 8 f32 points |
| 14 | FillRects | u32 rect_count; repeated f32 x, y, w, h + u32 color |
| 15 | MaterialRect | f32 x, y, w, h, radius; u32 kind, role; f32 opacity, blur_radius; u32 tint; f32 saturation, luminance_floor, luminance_gain, edge_highlight, edge_width, noise_opacity, shadow_alpha, shadow_radius; u32 container_id, union_id; f32 container_spacing; u32 container_flags |
| 16 | LinearGradientRect | f32 x, y, w, h; u32 from_color, to_color, axis, steps |

All values are little-endian. Colors are packed as `(r << 24) | (g << 16) | (b << 8) | a`.
SVG and built-in icon rendering deliberately reuse `Path` and `FillPath`; there
is no SVG-specific backend opcode. On macOS, diagonal strokes from `DrawLine`
and `Path` commands execute as bounded triangle bodies with round color caps,
while axis-aligned strokes stay on the color SDF path and circular glyphs stay
on the arc SDF path. This keeps Finder-like toolbar and sidebar icons smooth
without introducing frame-to-frame dot-instance churn for diagonal segments.
On Windows, the Direct3D renderer accepts the same `DrawArc`, `Path`, and
`FillPath` command surface; stroked arcs and curves flatten into bounded
rotated capsule instances in the color pipeline so SVG icons preserve draw
order. Small SVG-style fills lower to color-pipeline scanline strips for the
same reason, while larger polygon fills still ear-clip into the existing
triangle pipeline until the Windows renderer grows the scissor-batched
triangle/arc executor used by macOS and Android.

The `phenotype.commands` module provides a C++ parser (`parse_commands(buf,
len)`) that decodes these bytes into typed structs (`ClearCmd`, `FillRectCmd`,
`MaterialRectCmd`, `LinearGradientRectCmd`, etc.) for native backends.

## Material Planning

Material policy lives in the pure `phenotype.material` module. Backends provide
immutable edge inputs — capability snapshot, backdrop descriptor, render-target
metadata, debug seed, and quality policy — then execute the returned
`MaterialPlan`.

`Cmd::MaterialRect` carries the material node's material command descriptor
across the backend boundary: kind, functional surface role, material container
identity, union identity, container spacing/flags, opacity, blur, tint,
saturation, luminance curve, edge highlight, edge width, noise opacity, and
shadow. In C++ this is represented as `MaterialCommandDescriptor`; backends
reconstruct `MaterialRequest` from that descriptor plus geometry, then call the
pure planner. They should not re-derive these style values, functional roles, or
container grouping from the current theme after the command has been emitted.

```cpp
MaterialPlan plan = plan_material_surface(request, environment);
```

The plan records its artifact `contract_version`, source
`command_descriptor`, material `role`, material container analysis, blur, tint,
saturation, luminance curve, edge highlight, noise/dither, shadow, render-target
analysis, pure shape analysis, backdrop sampling, backdrop analysis, backdrop
access/capture bounds, foreground-excluded capture requirements, decision
trace, fallback path, debug metadata, pass expectations, the resolved quality
policy, foreground legibility/vibrancy recommendation, an Apple Liquid Glass
`reference_model`, the resolved theme token snapshot, resource budgets, the
resolved sampling kernel, bounded execution stages, verifier expectations, and
an `observation_contract` that mirrors the pure facts the artifact verifier
must observe at runtime.
`reference_model` is intentionally pure. It records that the surface follows the
Apple material model, including `technology`, `layer`, and `material_policy`.
Functional chrome/control roles resolve to `liquid-glass` on the
`functional-layer`; `MaterialSurfaceRole::Content` resolves to
`standard-material` on the `content-layer`. The same model records semantic
thickness variant, view-bounds shape scope, resolved shape, tint participation,
interactive/container/union/morph expectations, blending scope, accessibility
adaptation response, performance response, vibrancy expectation, legibility
preservation, and deterministic degradation policy. This keeps Apple-reference
alignment out of AppKit, Metal, Direct3D, Vulkan, and snapshot writers; those
adapters execute or report the resolved plan instead of re-deciding whether a
surface behaves like glass.
`decision_trace` records the pure gate booleans for geometry, target readiness,
role eligibility for Liquid Glass, content-layer standard-material selection,
quality, backend capabilities, accessibility settings, backdrop-source
readiness, and the first fallback blocker. `primary_pass` states whether the
backend should run a sampled backdrop blur pass, a standard content fill, or a
deterministic translucent fallback. `execution_stages` then expands that
primary pass into a bounded pure list of shadow, blur/standard/fallback, edge,
and noise stages so artifacts can explain material work before visual
inspection.
`execution_stage_capacity` records the fixed plan storage capacity and
`dropped_execution_stage_count` records overflow explicitly; any nonzero drop is
treated as a verifier failure, which forces future stage additions to update the
pure capacity and artifact contract instead of silently disappearing.
`observation_contract` repeats only debuggability-critical facts from the same
plan: fallback expectation and reason, backdrop sampling expectation, stable
backdrop requirement, shared and next-frame capture expectations, capture
reason, primary pass/executor, total/active/backdrop runtime-pass counts,
execution-stage counts, texture-copy bounds, capture/sample pixel bounds, safety
flags, and region/layer/pass hints. The verifier cross-checks it against the
surrounding `MaterialPlan`, so stale serializers or backend-local policy drift
fail at a single JSON path before pixel-region checks need human interpretation.
`primary_pass` also records the pure executor role and maximum texture-copy
pixels for that pass, so a backend
artifact can prove whether it stayed within the render-target copy budget.
`sample_taps` records the actual taps required by that resolved pass, so
deterministic fallback plans use `sample_taps: 0` even when the quality budget
allows more. The pure planner normalizes caller tap limits to executable
backdrop kernels of 1, 5, 9, 13, or 25 taps, selecting the largest kernel that
does not exceed `quality_policy.max_sample_taps`. `sampling_kernel` then names
the exact bounded blur kernel (`weighted-5x5-manhattan` today), its radius,
resolved tap count, blur step scale, weight profile, and whether it requires a
backdrop source. Deterministic fallback plans use the inactive `none` kernel so
stale blur metadata cannot leak into fallback artifacts. macOS uploads the
kernel's blur step scale to Metal instead of hard-coding that policy in the
shader; other backends serialize the same kernel contract before they gain an
advanced material executor. Reduced-motion plans also disable material noise
and cap backdrop sample taps before any backend executes the pass.
`luminance_curve` is the pure contrast transform that the backend should apply
after saturation and before tint. It records the curve name, resolved
floor/gain, gamma, midpoint, contrast, edge lift, whether the curve is
backdrop-driven, and whether all shader inputs are bounded. Sampled glass uses
`adaptive-backdrop-luma`; deterministic fallback uses `fallback-flat`. macOS
uploads gamma, midpoint, contrast, and edge lift to Metal so adaptive backdrop
legibility is executed from the pure plan instead of a backend-local heuristic.
`MaterialBackdropDescriptor` also carries the sampled luminance provenance:
sample count, fixed grid dimensions, source frame, and sample status. The macOS
edge fills those fields from a small asynchronous 5x5 BGRA grid sampled from the
foreground-excluded previous frame, while unsupported backends publish a
deterministic zero-sample descriptor. The pure planner only consumes completed
samples from a prior command buffer; it never blocks the frame waiting for CPU
readback.
`foreground` is the pure text/icon legibility recommendation for content drawn
on top of a material surface. It records primary, secondary, and accent colors,
the foreground scheme/source, estimated background luminance, contrast ratios,
the minimum contrast target, and whether the recommendation is backdrop-driven,
high-contrast, vibrancy-enabled, and deterministic. Backends consume or expose
this recommendation; they do not choose independent foreground policy for glass
surfaces. Native command decoders also use the same pure recommendation to
remap default primary, secondary, and accent text tokens drawn inside a prior
material surface. Custom text colors are left unchanged, and the original text
alpha is preserved.
`theme` is the pure snapshot of the explicit `MaterialStyle` tokens that fed
the material surface. It records the token source, profile name, token policy,
foreground, secondary foreground, accent, strong accent, tint, and border
colors, plus booleans that show whether those values match the default
Apple-like glass theme contract. A plan that still uses the default contract
reports `profile_name: apple-glass-light` and `default_glass_tokens: true`;
custom token mixes report `profile_name: custom`. Backends do not infer theme
identity or substitute platform palettes at the edge; they execute or serialize
this resolved pure snapshot so artifacts can explain why text, icons, tint,
and fallback colors look the way they do.
`quality_policy` records the pure planner's resolved
sampling/noise/shadow switches and caller quality limits, including
`max_backdrop_pixels`. `render_target` records sanitized target dimensions,
scale, pixel format, pixel count, readiness, and whether the target stays
within that backdrop budget. `resource_budget` records the clamped
blur/executable sample-tap limits, max sampling kernel radius, the same allowed
backdrop-pixel budget, max shared frame capture count/pixels, max surface sample
pixels, and whether texture copies and fallback behavior are bounded.
`backdrop_access` mirrors the active shared frame contract per plan:
sampled-backdrop plans require `capture_scope: shared-frame` and
`capture_reason: sample-current-frame` with one bounded frame-history copy that
excludes foreground text and overlays. This prevents a material surface from
sampling text it drew in the previous frame and then drawing the same text again
as foreground in the current frame.
When a backend can support backdrop sampling but the stable previous frame is
not ready yet, a deterministic `no-backdrop-source` fallback may still request
`capture_reason: warmup-next-frame` with the same foreground-excluded capture
contract so the next frame can sample without hiding that warmup copy in backend
policy. Unsupported, reduced-transparency, invalid, and quality-policy fallbacks
keep capture/sample budgets at zero.
Container spacing is also reported as `max_container_spacing`, so
artifact gates can bound future container/union expansion work before a backend
starts allocating extra backdrop passes.
Backends use the pure `default_material_quality_policy()` and
`sanitize_material_quality_policy()` helpers instead of owning hard-coded
material quality limits locally, so policy changes stay visible in
`MaterialPlan` tests and artifact JSON. The same pure constants cap executable
blur and sample work before any backend sees the plan:
`material_max_blur_radius` is 36 px and `material_max_sample_taps` is 25, which
keeps shader/runtime resource bounds reviewable from source, tests, and the
serialized `resource_budget`. Non-finite blur limits sanitize to zero at the
same edge, so NaN/Infinity quality input deterministically takes the bounded
fallback path instead of leaking into shader constants.
`geometry` preserves the raw decoded `MaterialRect` rectangle, while `shape`
records the pure executable shape: validity, surface area, min/max extent,
radius limit, effective radius, normalized radius, rounded flag, and radius
clamp flag. Native backends draw with `shape.effective_radius`, so radius
sanitization remains deterministic and cross-platform instead of becoming a
shader-local or fallback-local policy.
`verifier` records the deterministic pixel-region contract derived from the
same plan: whether a backdrop source or edge highlight must be present, the
minimum luma/color thresholds for sampled or fallback rendering, the semantic
profile name, whether material container identity/morph contracts are expected,
and the likely layer/pass that a failure should inspect first.
`MaterialContainerAnalysis` records whether a surface is isolated, participates
in a named material container, or belongs to a shape union. Reduced Motion keeps
container identity intact but disables morph-transition expectations in the pure
plan, matching Apple's guidance to coordinate related glass surfaces while still
respecting accessibility settings.
When a stable backdrop descriptor is available, the pure planner also copies
its source, readiness flags, foreground-exclusion flag, sanitized luminance
statistics, luminance response bucket, optical response buckets, and
floor/gain/edge/tint/saturation/depth deltas into `MaterialPlan.backdrop`.
The same value drives the `MaterialPlan.luminance_curve`
gamma/midpoint/contrast, tint alpha, saturation, opacity, edge-highlight, and
shadow-depth adjustment before the plan reaches a backend. This keeps
legibility and Liquid Glass optical policy in the deterministic layer as blur,
tint, and fallback decisions, and lets artifacts explain why a material
responded to dark, bright, flat, or neutral backdrop content without requiring
a visual guess.
Artifact gates can separately bound actual plan taps and resource-budget taps,
which lets fallback scenes require zero executed taps while preserving the
backend's allowed quality budget in the same artifact.
If the quality policy disables backdrop sampling, reduces the blur/tap budget
to zero, or the render target exceeds `max_backdrop_pixels`, the pure planner
returns `fallback_path: quality-policy` instead of leaving the backend to infer
that downgrade.
Native backends probe platform accessibility display preferences at the edge and
pass immutable `reduce_transparency`, `increase_contrast`, and `reduce_motion`
booleans into `MaterialEnvironment`. macOS reads AppKit `NSWorkspace` display
options, Windows reads `SystemParametersInfoW` high-contrast,
client-area-animation, and overlapped-content settings, and Android reads the
public `Settings.Global` animation scales plus `UiModeManager.getContrast()`.
The pure planner remains the only layer that decides whether those inputs
produce opaque fallback, higher contrast, reduced noise, or lower sample taps.
Runtime artifacts record `renderer.accessibility_display_options` so a captured
frame explains which system or test override fed the plan; unsupported platform
signals stay deterministic false instead of being inferred in backend drawing
code.
Top-level debug capabilities expose the same material gates as explicit
booleans: `material_surfaces`, `material_backdrop_blur`,
`material_shader_blur`, and `material_frame_history`. These fields are not
policy decisions; they are backend/runtime inputs that explain why
`MaterialDecisionTrace.backend_supports_backdrop`,
`capability_shader_blur`, and `capability_frame_history` resolved as they did.
Runtime adapters serialize the same `MaterialRuntimeRecord` shape into
`debug.platform_runtime.details.renderer.material_plans`: macOS records the
sampled-backdrop pass for functional roles and the `standard-material-fill`
pass for content roles; Windows and Android record deterministic fallback or
standard-material plans according to the same pure role policy; snapshot-only
targets publish an empty renderer contract with an explicit fallback policy.
Each serialized plan carries the same
`contract_version` so artifact verifiers can reject unknown material plan
schemas before reading version-specific fields. The renderer object also carries
`material_plan_contract_version`, allowing empty renderer contracts to advertise
the same artifact schema. Backends also publish
`renderer.material_runtime_summary`, a flat count/max summary derived from
the same records; the artifact verifier recomputes it from
`material_plans[]` so CI can catch summary drift, unexpected executor pass
growth, stage-capacity overflow, texture-copy budget drift, and material shape
drift such as a clamped radius not matching the backend-executed radius.
Backends separately publish `renderer.material_executor_summary` for edge-only
execution telemetry: material instances, fallback instances, material draw
calls, planned shared frame capture count/pixels, planned surface sample
pixels, upload bytes/capacity, framebuffer-history copy bounds, CPU enqueue
timings, `foreground_text_candidate_count`, and
`foreground_text_remap_count`. The same summary includes the exact backdrop
luminance descriptor that fed the pure planner (`backdrop_descriptor_luma_*`)
and any bounded sampling skip reason. `renderer.material_backdrop_luma_descriptor`
exposes the latest observed backend descriptor independently of the plans, so an
artifact can tell whether the backend had a completed async sample, a pending
sample, or a deterministic unsupported fallback. macOS additionally separates
the material backdrop texture from the final debug/readback texture. The
backend only allocates or blits that backdrop texture when the pure executor
summary says a shared-frame or next-frame capture is required; standard content
material and non-material frames skip the full-frame copy and report
`backdrop_copy_skip_reason`.
When a capture is required, macOS copies the backdrop source after
non-foreground scene work, then draws text and overlays in a foreground pass,
and finally captures the complete frame for artifacts. The executor summary
publishes `backdrop_copy_policy`, `backdrop_copy_required`,
`backdrop_copy_excludes_foreground_text`, and
`foreground_pass_after_backdrop_copy` so artifact gates can detect a future
foreground-feedback regression. These fields let artifact gates prove the
backend stayed inside deterministic resource limits while also leaving
nondeterministic timing data available for post-failure diagnosis.
Platform APIs, Metal/AppKit calls, shader compilation, texture capture, clocks,
filesystem writes, and process execution stay outside this pure layer.

### CLI, input, and output observation boundary

The long-term developer tool surface is a first-class `phenotype` CLI, not a
set of unrelated shell and Python scripts. Its architecture follows the same
edge-effect rule as material rendering:

```text
External input -> input abstraction -> phenotype -> output abstraction -> real renderer
CLI input      -> input abstraction -> phenotype -> output abstraction -> CLI output
```

The abstraction layers are typed value boundaries, not a second runtime.
`phenotype.io` defines the shared pure value contract: input event kinds,
deterministic input frames/scripts, output observation summaries, artifact
bundle descriptors, edge-effect policy text, and the LLM-debuggable artifact
predicate. Native platforms translate AppKit, Win32, Android, WASI, or
JavaScript events into a neutral input frame before calling the app. The CLI
translates typed command inputs and future JSONL scripts into the same shape for
deterministic driving. Renderer backends
consume the command stream and debug descriptors as usual; the CLI can instead
emit command summaries, semantic nodes, material plans, runtime summaries,
pixel-region samples, and verifier failures without showing a window.
`phenotype io contract --json` exposes the contract surface and a sample
replay/debug artifact check for CI and LLM agents.
`phenotype run --observe-output` bridges the two paths: it drives a native
example with CLI-provided input, asks the app to write a startup artifact, exits
deterministically, and embeds the parsed artifact observation in the same run
receipt. That keeps input driving and output observation under one CLI command
while preserving the renderer/backend artifact writer as the only side-effect
edge.

Packaging and diagnostics stay at the CLI edge. Asset discovery, i18n resource
validation, font packaging, Android process control, screenshots, filesystem
artifact writes, and toolchain probes must not leak into core layout, material
planning, or paint code. Release builds may keep the neutral input/output
types as thin value bridges while omitting artifact writers and driver
endpoints unless the package manifest explicitly requests them. See
`docs/PHENOTYPE_CLI_ROADMAP.md` for the command surface and migration plan.

`phenotype.resources` is the pure value boundary between package manifests and
runtime code. It lives in the internal `packages/phenotype_resources` path
package so both the root framework and the Linux-capable CLI consume the same
catalog contract. It defines immutable application, asset, locale, font, debug,
and `ResourceCatalog` descriptors plus lookup/fallback/diagnostic functions. It
does not parse TOML, touch the filesystem, register fonts, copy assets, or
probe platform bundles. CLI/package adapters build a catalog snapshot at the
edge, then core code can resolve logical asset names, locale fallback chains,
required translation keys, and package default typography without depending on
where those files live. The pure contract also asserts package-owned SVG asset
counts, a package-owned SVG `app.icon` asset, and CJK-capable fallback for the
Pretendard default UI font, so platform packagers can fail early before any
native bundle work starts. The
root `phenotype` umbrella provides
`theme_with_resource_defaults` as a thin framework helper that returns a new
`Theme`; the pure catalog package stays independent from UI types.

Apple's Human Interface Guidelines distinguish Liquid Glass from standard
materials: Liquid Glass belongs primarily to functional control/navigation
layers that float above content. phenotype follows that boundary in its
contract language. Backends may render an Apple-inspired glass effect, but the
core material API remains a cross-platform semantic contract with explicit
fallbacks rather than a claim to reproduce private system component behavior.
The layout DSL exposes this boundary through `layout::material_surface` for
low-level material containers and `layout::toolbar`, `layout::navigation`,
`layout::sidebar`, and `layout::status_bar` for common app chrome. Those
helpers only configure layout, semantic labels, and `MaterialSurfaceRole`; they
still emit the same `MaterialRect` command and flow through the pure
planner/backend executor contract above. Artifact gates can require roles such
as `toolbar`, `sidebar`, `status_bar`, `navigation`, or `surface` without
changing backend rendering policy. `MaterialSurfaceOptions` also carries
explicit border radius and border width overrides; these are view/layout chrome
decisions that shape the emitted command while leaving material optics,
fallback decisions, and pass selection in the pure planner/backend executor
contract.
The chrome helpers also accept `MaterialSurfaceOptions` directly, so app-like
examples can keep custom Finder-style dimensions, radii, and border decisions
while still going through the typed toolbar/navigation/sidebar/status-bar
semantic path instead of hand-rolling role assignment.

## Native backend structure

The native path is intentionally split into three layers so macOS and Windows work mostly in separate files:

1. **`phenotype.native.shell`** — shell-neutral host state, dispatchers, focus/input routing, viewport notifications, and repaint lifecycle
2. **`phenotype.native.platform`** — capability contracts for `text`, `renderer`, native surface descriptors, and platform services
3. **Platform implementation modules** — `phenotype.native.macos`, `phenotype.native.windows`, `phenotype.native.stub`

This keeps the framework core pure while pushing side effects into thin adapters.
Desktop shells pass a `NativeSurfaceDescriptor` through the opaque
`native_surface_handle`; platform renderers and input adapters consume OS
handles (`NSWindow` / `NSView` or `HWND`) and viewport metadata from that
descriptor instead of depending on any third-party window handle. AppKit and
Win32 own the desktop event loops; the renderer/input contracts remain the
same across both shells. Desktop examples should not depend on third-party
toolkit shims for window creation; the platform shell is the product contract.
The descriptor also stores the requested window chrome fields for
artifact/runtime debugging. macOS and Windows serialize those fields under
`debug.platform_runtime.details.window`, including the integrated titlebar
metrics, leading/trailing native-control reserves, whether native OS controls
own the caption-button/traffic-light territory, and explicit `uses_glfw=false`
/ `toolkit_window_shim=false` markers.
Finder-style startup artifacts keep the leading native-control reserve blank in
phenotype content and report `runtime-native-controls`, leaving input and
caption-button ownership in AppKit or Win32. `ExplorerChromeMetrics` also
records `native_window_control_owner=platform-edge`, the native-control count,
zero content/artifact marker counts, and a render policy that forbids app-drawn
traffic-light probes. The artifact verifier checks both the structured
ownership invariant and the neutral/forbidden-palette titlebar reserve, so
traffic-light probes cannot regress into duplicate content-drawn controls.
Package inspection also keeps every manifest-declared SVG asset free of
traffic-light marker palettes, which prevents packaged icons, file-type
thumbnails, or runtime images from visually implying a second set of native
controls.

## Unified debug plane

Debugging follows one shared model across native and WASI targets:

- the common snapshot schema is defined in `phenotype` / `phenotype.diag`
- semantic snapshots include root-level overlays after the main tree with
  screen-fixed visibility, matching the paint and focus traversal order
- platform adapters only provide capability overrides and `platform_runtime.details`
- artifact bundles always start from `snapshot.json`, then add optional
  `frame.bmp` and `platform/<platform>-runtime.json`

See [DEBUG_WORKFLOW.md](DEBUG_WORKFLOW.md) for the full contract and the current
material/runtime extensions.

### Current native backends

- **macOS**: AppKit shell + CoreText text measurement/atlas + Metal renderer + native `DrawImage` for local files and async remote images. The shell creates an `NSWindow`, moves it to the active Space, primes AppKit's WindowServer ordering once, installs a small application delegate so Dock reopen/app-activation events reorder the existing window, passes `NSWindow` / `NSView` plus viewport metadata through `NativeSurfaceDescriptor`, and maps `IntegratedTitlebar` to a transparent full-size titlebar with background dragging so examples can extend content behind native window controls without drawing duplicate chrome. Runtime artifacts publish live AppKit chrome readbacks, WindowServer bounds, z-order/app-window order, active-Space collection behavior, onscreen state, activation state, and key/main-window state to catch regressions where a process owns a Dock icon but the ordered window is hidden, covered, or inaccessible. The Metal renderer executes SVG/icon circles and isolated circular SVG path arcs through the arc SDF path and diagonal line/path strokes through a triangle-body plus round-cap path so macOS-like toolbar glyphs stay continuous without per-pixel dot decomposition. There is no GLFW or toolkit window shim on the macOS desktop path. Empty non-monospace font requests prefer Pretendard before falling back to the system UI face.
- **Windows**: Win32 shell + DirectWrite text measurement/atlas + Direct3D 12 renderer + IME composition overlay + native `DrawImage` for local files and async remote images. The shell creates an `HWND`, passes it plus viewport metadata through `NativeSurfaceDescriptor`, and maps `IntegratedTitlebar` to a DWM custom frame with resize-edge hit testing, caption-button delegation, blank-toolbar dragging, phenotype toolbar hit-region preservation, DPI-aware size limits, and `WM_SIZING` aspect-ratio enforcement. The renderer accepts SVG/icon `DrawArc`, `Path`, and `FillPath` commands: line/curve/arc strokes execute as color-pipeline capsules, small fills execute as color-pipeline scanline strips, and larger path fills ear-clip into the existing triangle pipeline. There is no GLFW or toolkit window shim on the Windows desktop path. Empty non-monospace font requests prefer Pretendard before falling back to Segoe UI.
- **Android**: GameActivity-driven shell (`examples/android/`) + Vulkan renderer with three instanced pipelines: a color pipeline (FillRect / StrokeRect / RoundRect / DrawLine / Clear — same `ColorInstance { rect; color; params }` layout and `params.z` draw-type dispatch as the macOS / Windows color pipelines); a text pipeline that samples an R8 atlas rasterised via JNI into `android.graphics.Paint` / `Canvas` / `Bitmap` (UTF-8 → UTF-16 via `cppx::unicode::utf8_to_utf16` before `JNIEnv::NewString`, with empty non-monospace requests preferring Pretendard and relying on Android's deterministic default fallback when unavailable); and an image pipeline that samples a persistent 2048² RGBA8 atlas strip-packed from images decoded via NDK's `AImageDecoder` (PNG / JPEG / WebP / GIF / HEIF). Image URLs resolve through `asset://path` (bundled assets via `AAssetManager`) or absolute filesystem paths (`file://` prefix stripped); `http(s)://` renders a placeholder until Stage 7. Input routes GameActivity's `android_input_buffer` (motionEvents + keyEvents + ACTION_SCROLL axes) into `phenotype::native::detail::dispatch_*` via the `phenotype_android_dispatch_{pointer,key,char,scroll}` C ABI; the Android renderer snapshots each flushed command buffer so `renderer.hit_test` can walk the `HitRegionCmd` list in reverse without forcing another view pass. The example driver boots a baked-in counter demo via `phenotype_android_start_app`, which instantiates `detail::run_host<demo6::State, demo6::Msg>` so view/update runs entirely in-library. GLSL sources live at `src/phenotype.native.android.shaders/{color,text,image}.{vert,frag}` and are precompiled to SPIR-V via `tools/compile_android_shaders.sh` (NDK's bundled `glslc`) into `src/phenotype.native.android.shaders.inl`, which the native module includes directly. `debug_api` is feature-parity with macOS / Windows: every `renderer_flush` copies the presented swapchain image into a persistent `debug_capture_image` so `capture_frame_rgba()` reads a fresh snapshot on demand; `snapshot_json` + `write_artifact_bundle` delegate to the shared `::phenotype::diag::detail` helpers. Resume handling clears `last_paint_hash` and calls `trigger_rebuild()` from `phenotype_android_attach_surface` so the first post-`APP_CMD_INIT_WINDOW` frame paints instead of staying black. Emits `libphenotype-modules.a` via exon; the example Gradle project packages it into an APK together with `androidx.games:games-activity:3.0.5` and links `libjnigraphics.so` for both `AndroidBitmap_*` zero-copy pixel reads and the `AImageDecoder_*` family.
- **Linux / other desktop**: shared stub backend

All native capability payloads include `system_settings`: macOS uses
CoreText/`NSFont`/`NSEvent`/`NSScroller`/`NSWorkspace`, Windows uses
`SystemParametersInfoW` plus DWM and the Personalize app-theme registry value,
and Android uses Java
resource/configuration APIs reached through the GameActivity edge. WASI and the
stub backend publish deterministic fallback values.
`scroll_delta_multiplier` and `scroll_horizontal_delta_multiplier` are pure
theme inputs; native wheel callbacks only translate platform events into
logical pixels before the app/user multiplier is applied. Renderers may report
the same snapshot in `platform_runtime.details`, but they do not decide font,
appearance, accessibility, accent, or scroll policy themselves.

### Modularity guarantee

The core remains isolated from platform SDKs. Native implementations only meet two contracts:

1. **Core host capabilities** — measurement, canvas sizing, URL opening, command-buffer flush
2. **`phenotype.commands`** — typed draw commands parsed from the shared buffer

This means Metal, Direct3D, Vulkan, Skia, software raster, or another future renderer can replace a platform backend without rewriting `types`, `state`, `layout`, `paint`, or `diag`.

## Native backend roadmap

- [x] Host interface abstraction — `src/phenotype_host.h` (PR #52)
- [x] Command buffer C++ parser — `phenotype.commands` module (PR #53)
- [x] Shared shell core extracted from platform-specific native code
- [x] macOS native code isolated behind a dedicated platform module
- [x] Windows backend wired into the native module graph
- [x] Native text measurement on Windows (`text_api`) via DirectWrite
- [x] Windows renderer implementation behind `renderer_api` via Direct3D 12
- [x] OS-native URL opener on Windows
- [x] IME composition for native text input on Windows
- [x] Native `DrawImage` local-file support on Windows
- [x] Native `DrawImage` async remote-image support on Windows
- [x] Contract tests for the Windows native backend (text, renderer smoke, text input, local/remote image upload paths)
- [x] `examples/native` positioned as the Windows native acceptance showcase
- [ ] IME composition for native text input on macOS
- [x] Native `DrawImage` support on macOS
- [x] AppKit desktop shell driver (`phenotype.native.shell.macos`)
- [x] Win32 desktop shell driver (`phenotype.native.shell.windows`)
- [x] Desktop shell size/aspect contracts implemented in native AppKit and Win32 window APIs
- [x] Desktop `NativeSurfaceDescriptor` contract so macOS/Windows renderer and input adapters consume native OS handles directly
- [x] `aarch64-linux-android` exon target with Android-stub platform (Stage 0)
- [x] Android Vulkan clear-color backend + GameActivity example (Stage 2)
- [x] Android color primitives pipeline (`FillRect` / `StrokeRect` / `RoundRect` / `DrawLine`) — Stage 3
- [x] Android text pipeline via JNI → `android.graphics.Paint` — Stage 4
- [x] Android image pipeline — Stage 5
- [x] Android touch / GameTextInput routing — Stage 6
- [x] Android debug plane + resume-flash fix + scroll routing — Stage 7
- [ ] Android device contract test runner (mirror Windows / macOS suites onto emulator) — follow-up
- [ ] Android async image decode + HTTPS remote fetch (worker thread + JNI HttpsURLConnection) — follow-up
- [ ] Android multi-touch gestures / pinch / rotate (shell-core dispatcher additions) — follow-up
- [ ] Android soft keyboard + GameTextInput + IME composition — follow-up

## Module dependency graph

```
phenotype (umbrella re-export)
├── phenotype.types       — Color, Cmd, Style, LayoutNode, NodeHandle, Decoration
├── phenotype.resources   — path package with pure ResourceCatalog descriptors, lookup, fallback, diagnostics
├── phenotype.icon_catalog — path package with pure built-in icon metadata
├── phenotype.svg         — pure SVG subset parser + Painter renderer
├── phenotype.icons       — audited SVG icon catalog
├── phenotype.io          — pure input frame and output observation contracts
├── phenotype.theme_contract — pure Apple-like glass theme contract metadata
├── phenotype.state       — Arena, AppState, Scope, InputHandler, message queue
├── phenotype.diag        — OTel-shaped Counter, Gauge, Histogram, log ring, JSON snapshot
├── phenotype.layout      — flexbox engine, measure_text cache, vDOM diff
├── phenotype.paint       — draw command emitters, cmd buffer, flush_if_changed
├── phenotype.commands    — typed command structs (ClearCmd, ...) + parse_commands()
├── phenotype.theme_json  — Theme ↔ JSON via txn auto-reflection
├── phenotype.native      — public native entrypoint, platform selection
├── phenotype.native.shell — shell-neutral host state + input translation
├── phenotype.native.shell.macos — AppKit event loop driver
├── phenotype.native.shell.windows — Win32 event loop driver
├── phenotype.native.platform — shared native capability and surface contracts
├── phenotype.native.macos — macOS text + Metal renderer
├── phenotype.native.windows — Windows text + Direct3D 12 renderer
└── phenotype.native.stub — shared stub backend for non-macOS native targets
```

External dependencies:
- `jsoncpp` — JSON parse/emit (used by diag snapshot + theme_json)
- `txn` — serialization framework (used by theme_json)
- `cppx` — shared platform boundary helpers (HTTP, URL opening, Unicode/resource utilities) plus reflection used across native integration points

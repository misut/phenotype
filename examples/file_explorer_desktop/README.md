# Desktop File Explorer Example

This example is a Finder-style desktop workflow for phenotype's material
system. It uses a glass toolbar with Finder-like semantic navigation, view,
group/sort, share/tag/more, and search controls, translucent sidebar locations,
icon/list/column/gallery file views,
document, image, video, and folder thumbnails, separate select/open semantics
for files and folders, read/create/duplicate/delete file actions, folder
create/delete actions, and contextual Finder-style status.
The startup view intentionally leaves the file grid unselected, matching
Finder's neutral Recents presentation, and starts in a deterministic
`Sort: Recent` order whose first row mirrors the reference Finder-style Korean
PDF probe scene; read, duplicate, and delete flows are covered by deterministic
startup scenarios and explicit model inputs. The sidebar Trash item is backed
by a hidden `.Trash` directory inside the demo sandbox, so delete actions move
files and folders to Trash before any permanent removal behavior is exercised.

The desktop window requests `WindowChromeStyle::IntegratedTitlebar` with
explicit titlebar, leading-control, and trailing-control reserve metrics. On
macOS the backend uses native AppKit titlebar transparency/full-size content so
the system traffic-light controls are integrated into the content area instead
of being duplicated as interactive phenotype controls. The live example keeps
that sidebar/titlebar area as blank reserve, so the real OS-owned window
controls have space without phenotype drawing another set of buttons. Artifact
capture keeps that same reserve blank and verifies it with low edge/chroma
pixel contracts plus native-window debug metadata. The macOS runtime snapshot
also records the actual AppKit standard window buttons, their count, and their
top-left content frames, so the verifier can fail if the buttons move back into
a separate titlebar or if phenotype content starts drawing a second set. The
CLI gate deliberately does not draw traffic-light probes, and all packaged SVG
assets avoid traffic-light marker colors, because the real controls are owned
by the platform shell. On Windows
the native Win32 shell keeps the same contract through a
DWM custom frame, using `WM_NCHITTEST` to preserve
resize edges, caption-button behavior, blank-toolbar dragging, phenotype toolbar
hit regions, and native size/aspect-ratio constraints. The example does not use
a toolkit window shim.
The toolbar itself is a borderless material shell with rounded material control
groups, so the artifact keeps Finder-like chrome without duplicating native
window controls. File create, read, duplicate, and delete behavior is still
covered through the shared model, CLI inputs, startup scenarios, and the
Finder-style `More` overflow instead of an always-visible extra action group.
The default Recents artifact hides the status bar to match Finder's neutral
icon-grid presentation. Search, selection, and file-operation scenarios reveal
the status surface again so debug artifacts still expose action receipts and
state transitions without adding persistent chrome to the startup view.

Startup frame artifacts intentionally capture phenotype content rather than the
operating system's non-client controls. The top-left titlebar reserve is blank
in normal interactive runs and artifact captures. AppKit/Win32 own the real
close, minimize, maximize, and caption-button hit testing at runtime.
The manifest also checks `debug.platform_runtime.details.window` so CI can
prove the example requested `IntegratedTitlebar`, preserved the expected
titlebar/control reserve metrics, and is not using GLFW or another toolkit
window shim.
The artifact also includes `debug.application.file_explorer`, a pure shared
model payload containing the current profile, location, status, sort mode, view
mode, selection state, operation receipt, entry counts, and
`ExplorerChromeMetrics`. The manifest asserts this payload before visual
regions, so an LLM can distinguish model-state drift from renderer/capture
drift without guessing from pixels. The desktop manifest also asserts the
Finder toolbar group, separator, icon-button, and compact Recents icon-grid
density metrics from that payload, including thumbnail canvas, label, font,
grid-gap sizing, contextual `status_bar_visible` state, and the
`window_control_marker_mode` split between live native controls and artifact
probe markers. It records `native_window_control_owner=platform-edge`, a native
control count, zero content/artifact marker and drawn-control counts, the
`platform_standard_controls_inside_leading_content_reserve` integration policy,
and a render policy that allows only runtime OS controls. The
`native_window_control_geometry_role` value is
`reserve_metrics_only_not_paint_instructions`, so `titlebar_control_*` numbers
describe blank reserve geometry only; they are not permission to paint red,
yellow, or green traffic-light markers. The paired
`native_window_control_palette_policy` forbids traffic-light colors in content,
artifact, and package SVG asset paths. It also checks the
platform `window.native_window_controls` object for AppKit standard button
presence and content-reserve integration, then checks the pure `geometry` object for
the integrated chrome contract: window inset/gap, sidebar surface origin, first
sidebar row, toolbar shell, navigation/title/trailing group x-coordinates,
collapsed search button x-coordinate, content surface origin, and the native
titlebar drag and leading/trailing control reserve widths. Sidebar symbol and
label placement, section spacing, selected-row radius, soft selected-row alpha
policy, blank native-control reserve coordinates, and forbidden traffic-light
palette probes are asserted separately, so Finder parity regressions are
reported as structured chrome contract failures before pixel inspection.
The debug payload also emits `finder_visual_contract`, a pure summary that ties
those metrics to the Finder parity intent: platform-owned traffic lights only,
no content/artifact window-control markers, reserve-only titlebar geometry,
soft sidebar selection with no selected-row border, keyboard-only focus rings,
permissive SVG icon sources, zero Apple/SF Symbols embedded assets, zero
platform-extracted icons, and local artifact verification rather than slow PR
CI capture.
The sidebar and toolbar glyphs come from `phenotype.icons`, not copied SF
Symbols assets. They are phenotype-owned or audited permissive SVG symbols
with a macOS-style rounded-outline contract and bounded secondary-layer opacity
for detailed symbols. Each icon declares a semantic SF Symbols reference name
as a role and proportion anchor while keeping Apple artwork out of the asset
set. File-type glyphs may embed checked open SVG sources such as Lucide ISC or
Feather-derived MIT icons; artifacts record source family, icon name, exact
license, license URL, pinned direct raw SVG URL, source revision, copyright,
and Apple-asset boundary beside the rendered symbol metadata.
Artifacts can assert the icon module, source format,
ownership/permissive-source boundary, reference family/policy, round stroke
expectation, rendering mode, variant policy,
text-aligned scale, role-aware presentation policy, sidebar/toolbar point
sizes, role hit-target metrics, tone policy, symbol-state chrome policy,
normal/hovered/pressed state policy, toolbar/sidebar pressed alpha, pressed
symbol opacity/scale, toolbar button alpha/radius metrics, symbol counts, and
the exact semantic reference arrays for the visible Finder-style toolbar/sidebar
glyphs, plus the file-type symbol table used by icon-view fallback painters.
The debug payload also resolves representative sidebar, toolbar, and file-type
presentation recipes with visible RGBA, effective point size, hit target, and
likely icon layer/pass so Finder-like icon drift is diagnosable from JSON.
The live toolbar controls use the core `widget::symbol_button` helper, which
derives `ButtonStyleOptions`, `ButtonVisualState`, centered SVG glyph painting,
semantic labels, and paint tokens from `phenotype.icons::SymbolButtonOptions`.
Sidebar rows still own their custom label layout, but their glyphs use the same
pure role/state presentation recipe. Hover and press chrome therefore affect
both the button material and the glyph opacity/scale instead of remaining
debug-only metadata.
The desktop sidebar additionally exposes the pure `token -> symbol -> semantic
reference` table used by the renderer (`recents -> recents -> clock`,
`download -> download -> arrow.down.circle`, and so on), without depending on
platform icon fonts or bundled Apple assets. File entries use the same pure
mapping for Finder-like fallbacks: PDF, text, image, movie, archive, audio,
code, spreadsheet, presentation, document,
and folder entries resolve to audited SVG symbols and semantic reference names
before any native renderer sees them.
The package manifest now carries the same eleven file-type icons as
runtime-visible SVG assets under `assets/icons/file-types/`, sourced from the
pinned Lucide revision recorded by the icon catalog. That keeps desktop
packaging, CLI inspection, and the pure debug contract aligned with the
fallback painters instead of relying on platform file icons. The package also
bundles the pinned Lucide license notice as a non-runtime asset so copied
resource bundles retain the permissive-source notice.
When file-type or toolbar glyphs need more realism, prefer permissive SVG
sources such as Lucide ISC, Feather-derived MIT glyphs, or other audited open
icon sets and record the exact source/license in the package contract. Do not
extract or embed Apple-owned SF Symbols, Finder icons, or system file icons
unless the legal usage boundary is explicitly cleared.
The icon-grid thumbnails follow the same boundary. The example references
macOS Finder's document/image/movie preview proportions and SF Symbols-style
visual restraint, but it paints deterministic phenotype-owned vector previews
instead of reading user files or embedding platform preview assets. The shared
`thumbnail_system` contract exposes PDF page size/fold/detail counts, media
preview size/radius, image/video detail counts, shadow policy, and the
`uses_external_previews=false` asset boundary. SVG file thumbnails use a bounded
edge document cache keyed by the sandbox preview body, and the same contract
exposes the cache policy and limit so frame-to-frame SVG XML parse churn is
debuggable from artifacts. The manifest asserts those JSON paths and adds
pixel-region probes for the first PDF, image, and video thumbnails, so a
regression reports whether the preview painter, grid geometry, or capture
region drifted.
On macOS, the same runtime object reports live `NSWindow` chrome state:
transparent titlebar, full-size content view, hidden native title, and
background dragging must all be enabled. These fields are actual platform
readbacks, not request echoes.
On macOS it additionally checks WindowServer bounds, onscreen state, artifact
capture readiness, active-Space collection behavior, and key/main window state,
which catches regressions where the process owns a Dock icon but the ordered
NSWindow is still hidden, backgrounded, in another Space, or not eligible for
foreground interaction. The artifact also records WindowServer z-order fields
(`window_server_order_index`, `window_server_app_order_index`, and
`window_server_occluded_by_front_app_window`) so a launch failure can distinguish
"no NSWindow", "offscreen/minimized NSWindow", and "created but covered by
another app window". The artifact gate uses `artifact_capture_visibility_state`
and `artifact_capture_ready`; the stricter `visibility_state` and
`ready_for_user_interaction` fields remain for manual foreground launch
debugging. The AppKit shell installs a small native application delegate that reorders
the existing window when the app becomes active or the Dock icon is reopened;
if the launch path still cannot make the window visible/key/main, stderr names
the exact state before the event loop continues. The example no longer emits
traffic-light markers in either live content or artifact-only paths.

The toolbar search control starts as a Finder-style icon button. Activating it
reveals a search field wired to the shared file model, so desktop and mobile
examples use the same deterministic filename filter and startup artifact
scenario.
The toolbar `More` button opens a compact material overflow menu for file and
folder creation, duplicate, and delete actions. The default artifact keeps that
menu closed to preserve Finder's neutral chrome, while
`PHENOTYPE_FILE_EXPLORER_SCENARIOS=more-actions-open` captures the menu and
asserts its debug payload.
Folder clicks use the shared desktop activation contract: the first click
selects the folder so the status and delete capability are observable, while a
second activation opens it. The CLI can exercise the same behavior with
`activate:Folder`, or bypass it with `open:Folder` when a test wants direct
navigation.
The icon-grid column count, visible row budget, top inset, thumbnail canvas,
label size, titlebar reserve, sidebar row metrics, sidebar icon/label
placement, toolbar group/icon button metrics, and Finder chrome geometry come
from the shared pure `ExplorerChromeMetrics` contract, so
`phenotype drive file-explorer --json` can report the same layout decisions
without launching a native window. The `finder_density_policy` field names the
reference-density contract that keeps the first icon row anchored by data
instead of a renderer-local spacer.
Finder chrome surfaces are created through `layout::glass_surface_options`
presets, then locally constrained with those shared metrics, so the example
uses the same material surface API that other apps can call directly.
The desktop view also registers the shared keyboard command contract through
phenotype's native key-command registry. AppKit, Win32, and Android shells only
translate platform keys into normalized command input; the pure app contract
decides that search is `CommandOrControl+F`, selection activation is `Enter`,
grid navigation is `ArrowUp/Down/Left/Right`, jump navigation is
`Home`/`End`/`PageUp`/`PageDown`, Trash movement is `DeleteOrBackspace`,
duplicate is `CommandOrControl+D`, folder creation is
`Shift+CommandOrControl+N`, and transient dismissal is `Escape`.
`debug.application.file_explorer.keyboard_commands` serializes that same
contract so artifacts can catch shortcut drift before a manual key test.

All filesystem writes stay inside an example-owned temp directory named
`phenotype-file-explorer-desktop`. The example never points at the user's real
home folder.

The directory also includes an initial `phenotype.package.toml` plus `assets/`,
`locales/`, and `fonts/` fixtures. These are consumed by the new
`tools/phenotype_cli package inspect` command and describe the
asset/i18n/Pretendard bundle contract. The package icon is a phenotype-owned
SVG asset with a macOS-like rounded document-window composition; the CLI checks
that `app.icon` is SVG/preloaded and that Pretendard has a CJK-capable fallback.
Runtime labels are resolved through the shared pure `ResourceCatalog` helper in
`file_explorer_shared`, while file reads and package inspection remain
CLI/example edge work. Set
`PHENOTYPE_FILE_EXPLORER_LOCALE=ko` to smoke the Korean label path.
Startup artifacts serialize the same package contract at
`debug.application.file_explorer.resource_system`, including SVG asset counts,
the app icon declaration, default locale/font status, Pretendard's CJK fallback,
locale coverage, and debug manifest/probe/verifier declarations. This lets a
bundle explain package/resource drift even when the native window and material
rendering are otherwise healthy.

## Run

```sh
cd examples/file_explorer_desktop
mise exec -- exon build
mise exec -- exon run
```

## Artifact Gate

From the repo root:

```sh
(cd tools/phenotype_cli && mise exec -- exon build)
tools/phenotype_cli/.exon/debug/phenotype_cli artifact verify-file-explorer \
  --profile desktop
```

`tools/verify_file_explorer_artifacts.sh` is a thin compatibility wrapper for
existing local scripts; it builds the CLI when needed and delegates to the same
command.

For the package-resource contract:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli package inspect --json ../../examples/file_explorer_desktop
```

The package metadata's debug verifier is the CLI command
`phenotype artifact verify-file-explorer`; the shell script remains only as a
thin wrapper for older local run configurations.

To create a Finder-launchable development bundle after building the example:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli package bundle \
  ../../examples/file_explorer_desktop \
  --format macos-app \
  --output "/tmp/Phenotype File Explorer.app"
.exon/debug/phenotype_cli package verify-bundle \
  "/tmp/Phenotype File Explorer.app"
open "/tmp/Phenotype File Explorer.app"
```

The generated `.app` is intentionally a local development bundle, not a signed
distribution artifact. Its launcher sets `PHENOTYPE_PACKAGE_ROOT` to
`Contents/Resources` before execing the built native binary, so runtime labels,
the package-owned SVG app icon metadata, and the Pretendard/CJK fallback
contract come from the staged package instead of the caller's working
directory. The bundle step also converts the package-owned SVG `app.icon` into
`Contents/Resources/file_explorer_desktop.icns` with macOS `sips` and
`iconutil`, records its SHA-256 digest in `phenotype.bundle.json`, and writes
`CFBundleIconFile` in `Info.plist` so Finder and Dock use the packaged
macOS-style icon. This avoids the raw-executable `open` path that can route
through Terminal and make launch debugging look like a missing native window.

At runtime the example reads `phenotype.package.toml` and locale files from
`PHENOTYPE_FILE_EXPLORER_PACKAGE_ROOT`, `PHENOTYPE_PACKAGE_ROOT`, or the current
working directory. `phenotype run file_explorer_desktop` sets the package-root
environment automatically. When `--artifact-dir` and `--artifact-exit` are used
together, the CLI keeps the leading titlebar reserve blank and relies on the
runtime window debug payload plus pixel-region contracts to prove native control
ownership without drawing duplicate traffic lights.
The example also applies the native `system_settings` snapshot before
`set_theme`: Pretendard remains the package default, OS font size metrics and
scroll policy become pure theme inputs, system appearance/accent are applied by
default, OS preferred locale seeds the startup language when supported, OS font
family remains an opt-in override, native input timing is captured for
double-click, key repeat, and caret blink diagnostics, and user overrides win.
Direct launches can use
`PHENOTYPE_FILE_EXPLORER_FONT_FAMILY`, `PHENOTYPE_FILE_EXPLORER_USE_SYSTEM_FONT`,
`PHENOTYPE_FILE_EXPLORER_USE_SYSTEM_FONT_METRICS`,
`PHENOTYPE_FILE_EXPLORER_DISABLE_SYSTEM_FONT_METRICS`,
`PHENOTYPE_FILE_EXPLORER_FONT_SCALE`, `PHENOTYPE_FILE_EXPLORER_FONT_SIZE`,
`PHENOTYPE_FILE_EXPLORER_HEADING_FONT_SIZE`,
`PHENOTYPE_FILE_EXPLORER_SMALL_FONT_SIZE`,
`PHENOTYPE_FILE_EXPLORER_LINE_HEIGHT`,
`PHENOTYPE_FILE_EXPLORER_LINE_HEIGHT_RATIO`,
`PHENOTYPE_FILE_EXPLORER_SCROLL_SPEED`, and
`PHENOTYPE_FILE_EXPLORER_HORIZONTAL_SCROLL_SPEED`; the resulting edge snapshot,
overrides, and effective theme are written to
`application.file_explorer.preferences` in the artifact.
The same command can feed deterministic native startup input through the shared
model parser:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli run file_explorer_desktop \
  --artifact-dir /tmp/phenotype-file-explorer-input \
  --artifact-exit \
  --input select:README.txt \
  --input shortcut:duplicate
```

For direct launches, `PHENOTYPE_FILE_EXPLORER_INPUTS` accepts newline- or
semicolon-separated inputs, and `PHENOTYPE_FILE_EXPLORER_SCRIPT` points at the
same line-based script format used by `phenotype drive file-explorer
--script`. Preference commands use the same app input layer, so
`font-family:system`, `font-family:Pretendard`, `font-scale:1.2`,
`system-font-metrics:false`, `font-size:17`, `heading-font-size:22`,
`small-font-size:13`,
`line-height:1.45`, `system-scroll-metrics:app`, `scroll-speed:1.4`, and
`horizontal-scroll-speed:2` update the shared state before the native theme is
resolved. macOS records the last local scroll event under
`debug.platform_runtime.details.input.scroll`, so raw AppKit deltas, precise
scroll mode, app multipliers, and normalized logical-pixel deltas can be
checked without guessing from the captured image.
The native example captures OS settings before the first frame and refreshes
them when app input triggers theme sync, so font, appearance, accent, and scroll
policy changes can be reflected without restarting the demo.
The desktop More menu exposes the same family, OS/package text-size policy,
text scale, appearance, and scroll controls as shared file explorer inputs.
File reads and environment access remain example edge work; parsing and input
application stay in `file_explorer_shared`.
The desktop More menu exposes the same preference inputs as native buttons
(`System`, `Pretendard`, `Text +/-`, and `Scroll +/-`) so interactive changes
and CLI-driven startup replay share one model contract.
The desktop canvas labels read `Theme::default_font_family`, so OS/user font
family overrides affect both widget text and Finder-style custom drawing.

The checked-in manifest requires stable labels and roles, every public
`MaterialKind`, resolved material plans, semantic/runtime material parity,
semantic toolbar button labels, bounded material resource budgets, and
pixel-region checks for the sidebar, toolbar, icon grid, and shared keyboard
command debug contract. The default icon
artifact also requires `status_bar_visible=false` and the neutral `Sort:
Recent` model state, so accidental default file selection or sort drift is
caught without relying on a persistent bottom status label.

The gate captures the desktop example in `icon`, `list`, `column`, and
`gallery` modes through `PHENOTYPE_FILE_EXPLORER_VIEW` so each Finder-style
content surface has a machine-readable artifact contract.

It also captures deterministic startup scenarios through
`PHENOTYPE_FILE_EXPLORER_SCENARIO`: `created-preview`, `deleted-file`,
`trash-view`, `created-folder`, `deleted-folder`, `duplicated-file`,
`documents-preview`, `history-forward`, `sorted-kind`, `search-active`, and
`preferences-panel`.
Set `PHENOTYPE_FILE_EXPLORER_SCENARIOS=more-actions-open` when the desktop-only
More overflow contract needs a local artifact. These scenarios make create,
Trash movement, duplicate, folder operation, navigation history, file preview
behavior, sort state, active search, mobile preference controls, and overflow
action availability visible in the semantic artifact without requiring manual
click playback. File and
folder operation scenarios also expose an `Operation: ...` receipt in the
status surface so the artifact can identify the action kind, target, and
result.

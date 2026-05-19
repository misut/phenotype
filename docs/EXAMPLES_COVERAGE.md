# Examples Coverage

Status: current coverage contract for `origin/main`.

The examples under `examples/` are the local acceptance surface for phenotype.
They are not all equivalent: some are compact widget showcases, some are dense
product-style scenes, and Android is a platform-specific packaging route.

## Run commands

Run commands from the target repo or example directory, with tools resolved by
that directory's `mise.toml`.

```sh
cd examples/native
mise exec -- exon build
mise exec -- exon run
```

```sh
cd examples/glass_showcase
mise exec -- exon build
mise exec -- exon run
```

```sh
cd examples/file_explorer_desktop
mise exec -- exon build
mise exec -- exon run
```

```sh
cd examples/file_explorer_mobile
mise exec -- exon build
mise exec -- exon run
```

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli android doctor --json
.exon/debug/phenotype_cli android run --json
.exon/debug/phenotype_cli android logs --json
.exon/debug/phenotype_cli android contract --json
```

For docs/WASI dogfooding:

```sh
cd docs
mise exec -- exon build --target wasm32-wasi
```

For the first CLI/package contract:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli doctor --json
.exon/debug/phenotype_cli package list --json ../../examples
.exon/debug/phenotype_cli package inspect --json ../../examples/file_explorer_desktop
.exon/debug/phenotype_cli package inspect --json ../../examples/file_explorer_mobile
.exon/debug/phenotype_cli package bundle --json ../../examples/file_explorer_desktop \
  --output /tmp/phenotype-file-explorer-desktop-bundle
.exon/debug/phenotype_cli package bundle --json ../../examples/file_explorer_mobile \
  --output /tmp/phenotype-file-explorer-mobile-bundle
```

For LLM-debuggable native startup artifacts:

```sh
cd examples/native
mise exec -- exon build
PHENOTYPE_ARTIFACT_DIR=/tmp/phenotype-native-startup \
PHENOTYPE_ARTIFACT_REASON=native-startup \
PHENOTYPE_ARTIFACT_EXIT=1 \
.exon/debug/native
```

Use `examples/glass_showcase` when the artifact should prove material semantics
across every public `MaterialKind`, including macOS sampled-backdrop rendering
and fallback metadata. Use the desktop or mobile file explorer examples when
the artifact should prove an app-like material workflow.
The desktop file explorer additionally gates Finder-style icon-grid previews:
the shared model exposes `chrome.thumbnail_system` in CLI/debug JSON, and the
desktop manifest samples PDF, image, and video thumbnail regions so visual drift
is reported as concrete pixel-region metrics rather than a manual screenshot
guess.
Both file explorer profiles also expose font-family, font-scale, exact
body/heading/small font-size, line-height, vertical scroll-speed, and horizontal
scroll-speed preference inputs through the shared model, plus a
`system-scroll-metrics` switch that lets artifacts prove whether static
platform scroll multipliers are applied. Native examples also use the OS
preferred locale as the startup language when the package declares a matching
locale and no app locale override is present. Native examples apply those app
overrides after the platform system-settings snapshot is captured, so artifacts
can distinguish OS defaults from user-selected theme changes. Desktop and mobile
refresh that native snapshot at update-boundary theme sync, which keeps
appearance/font/accent/scroll changes observable after the next input. The
desktop More menu and mobile Create tab both call the same shared preference
inputs, including system/package font-size policy buttons, which keeps
interactive controls, startup scripts, and CLI-driven artifact replay on one
contract.

From the repo root, verify a generated bundle through the CLI verifier edge
before using it as a debugging artifact:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli artifact verify --json /tmp/phenotype-native-startup \
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
  --require-material-fallback
```

That CLI command runs the uv-managed Python verifier as an implementation
detail. Direct `uv run tools/verify_artifact_bundle.py ...` is reserved for
verifier development and fixture tests.

The same command can be run from the CLI directory during local iteration:

```sh
cd tools/phenotype_cli
.exon/debug/phenotype_cli artifact verify --json /tmp/phenotype-native-startup \
  --expect-platform macos \
  --require-frame \
  --require-label "Control States" \
  --require-label "Material Surface" \
  --require-label "Paint Command Showcase" \
  --require-role button \
  --require-role material \
  --require-role text_field \
  --require-material-kind regular \
  --require-material-fallback
```

For `examples/glass_showcase`, use the checked-in manifest:

```sh
.exon/debug/phenotype_cli artifact verify --json /tmp/phenotype-glass-showcase \
  --expect-platform macos \
  --manifest ../../examples/glass_showcase/artifact_manifest.json
```

The repo-local gate wraps build, startup capture, and manifest verification:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli artifact verify-glass-showcase --json
```

For single-example launch debugging, the same CLI can build and run the example
directly while injecting the startup artifact environment:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli run glass_showcase \
  --artifact-dir /tmp/phenotype-glass-showcase \
  --artifact-exit \
  --timeout-seconds 120
```

The gate pins `PHENOTYPE_ACCESSIBILITY_DISPLAY=standard` by default so local or
runner Accessibility settings cannot turn the showcase into a reduced
transparency fallback. Override that environment variable when deliberately
capturing accessibility-response artifacts.
Use `phenotype artifact verify-glass-showcase --accessibility --json` for the
committed accessibility-response gate; it runs the same scene with reduced
transparency, increased contrast, and reduced motion enabled, then applies
`examples/glass_showcase/artifact_manifest.accessibility.json`.

The file explorer examples use the same artifact contract, but are intended as
local product-workflow smoke tests instead of a default CI gate:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli artifact verify-file-explorer --json
.exon/debug/phenotype_cli drive file-explorer --json \
  --input view:gallery \
  --input viewport:900x620@2 \
  --input select:README.txt \
  --input shortcut:duplicate \
  --input key:delete
.exon/debug/phenotype_cli drive file-explorer --json \
  --input select:Documents \
  --input key:enter \
  --expect location:Demo\ Root/Documents
.exon/debug/phenotype_cli drive file-explorer --json \
  --input key:tab \
  --expect focus-visible:true \
  --expect focus-target:sidebar
.exon/debug/phenotype_cli drive file-explorer --json \
  --input click:README.txt \
  --expect focus-visible:false \
  --expect input-modality:pointer
.exon/debug/phenotype_cli drive glass-showcase --json \
  --script ../../examples/glass_showcase/glass_showcase.drive \
  --expect material-count:7
```

The same file explorer drive output and native artifacts expose
`finder_visual_contract`, a pure Finder-parity summary derived from shared
chrome metrics. It pins the integrated titlebar/reserve strategy, platform-owned
traffic-light ownership, no content/artifact window-control marker policy,
soft sidebar selected-row styling, keyboard-only focus ring behavior,
permissive SVG icon provenance, zero Apple/SF Symbols embedded asset counts,
and the local-only artifact verifier gate.

Pull-request CI does not run these slow startup artifact captures. Code changes
run the root test matrix, docs changes run the docs WASI build, Python verifier
tooling changes run the uv-managed verifier checks, and CLI/package-resource
changes run a lightweight Linux `phenotype_cli` build, package inspection,
deterministic file explorer drive check, deterministic glass showcase drive
check, and targeted shared-package tests without the root C++ test matrix. The
main-branch push workflow only runs artifact/docs build gates, not the full
code test matrix or glass artifact capture. WASI root tests, docs builds, and
CLI package checks run on Linux runners; native artifact builds remain
macOS-only. Workflow-file changes deliberately enable all relevant gates so
runner policy edits validate themselves.

## Example roles

| Example | Platforms | Role |
|---|---|---|
| `examples/native` | macOS, Windows | Compact desktop widget showcase for shared controls, input debug, local/remote bitmap images, inline SVG vector image rendering, scroll, resize, and manual acceptance |
| `examples/glass_showcase` | macOS, Windows | Material and glass-debug acceptance scene for deterministic backdrop regions, macOS sampled backdrop, all material kinds, artifact capture, and pixel-region checks |
| `examples/file_explorer_desktop` | macOS, Windows | Finder-style desktop product workflow with native integrated chrome, audited macOS-style SVG toolbar/sidebar icons, pure `icons::macos_presentation(...)` role/state mapping, and verifiable Apple-HIG/SF-Symbols-inspired catalog metadata, Finder-style segmented toolbar metrics, icon-led column location rows, viewport-aware icon/list/column/gallery panes, glass toolbar action clusters, sidebar locations, real sandboxed Trash, Recents icon grid, document/image/video/folder thumbnails, icon-revealed search, separate select/open activation, file/folder create, duplicate, delete, file preview, and sandboxed temp-root operations |
| `examples/file_explorer_mobile` | macOS, Windows | Mobile file explorer layout with browse/preview/create tabs, compact location strip including Trash, material surfaces, search, direct folder open, file preview, file/folder create, duplicate, delete, and the same sandboxed model |
| `examples/android` | Android | GameActivity/Vulkan packaging route, Android event dispatch, platform lifecycle, asset/local images, debug API, and logcat/manual device validation |
| `docs` | WASI | Dogfooded documentation app, WASI build, JS shim integration, and web-facing DSL examples |

The file explorer examples deliberately keep filesystem side effects at the
example edge. The shared model operates only under a deterministic temp
directory (`phenotype-file-explorer-desktop` or
`phenotype-file-explorer-mobile`) and never points at the user's real home
folder. File create, folder create, folder select/open, read, duplicate, Trash
movement, and permanent delete-from-Trash actions produce operation receipts
backed by resolved operation plans, and sort mode changes produce a shared
`Sort: ...` status contract, that the
desktop and mobile status surfaces expose to artifact verification. This keeps
the examples useful for interactive product checks while preserving a stable
startup artifact contract.

The same shared model is available without a native window through
`phenotype drive file-explorer`. That command applies typed inputs to the
sandboxed model and emits JSON containing the input trace, visible entries,
viewport, view mode, pure Finder chrome/grid metrics, capabilities, operation
receipt and plan, selected preview excerpt, desktop keyboard command
descriptors, the shared `input_model`, and final snapshot. It is the
lightweight CI-friendly counterpart to the local desktop/mobile artifact
capture gate.
File-operation inputs are resolved as direct child entry names in the current
sandbox folder; path traversal or hidden-entry names produce failed operation
receipts that can be asserted with `--expect operation:...:fail`.
Successful plans name sandbox-relative sources/destinations and whether the
operation reads, writes, creates a directory, moves to Trash, permanently
deletes from Trash, or degrades with a fallback reason.
Key and shortcut aliases such as `key:enter`, `key:delete`,
`shortcut:duplicate`, `shortcut:find`, and `shortcut:new-folder` resolve to the
same shared actions that the desktop native key-command registry dispatches.
Focus aliases such as `key:tab`, `shift-tab`, `focus:search`,
`click:README.txt`, and `pointer:content-grid` resolve to the same model-level
focus state as native artifacts: keyboard traversal sets `focus_visible=true`,
while pointer input changes `focus_target` without drawing the ring.
Preference aliases such as `font-family:system`, `font-scale:1.25`,
`font-size:17`, `heading-font-size:22`, `small-font-size:13`,
`line-height:1.45`, `system-scroll-metrics:app`, `scroll-speed:1.5`,
`horizontal-scroll-speed:2`, and `color-scheme:system`
drive the same pure
`ThemePreferenceOverrides` path as the native file explorer settings controls.
Artifacts record the OS snapshot (`color_scheme`, appearance/accessibility
sources, font family/metrics availability, accent availability, and axis-specific
scroll factors), the app overrides, and the resolved effective theme so OS-level
preferences and app-level overrides can be debugged without guessing from pixels.
View-mode inputs (`view:icon`, `view:list`, `view:column`, or `view:gallery`)
use the same shared state field as the desktop toolbar and
`PHENOTYPE_FILE_EXPLORER_VIEW`, so headless CLI traces and native artifacts
describe the same Finder pane contract.
Native desktop/mobile artifact bundles also publish that shared state at
`debug.application.file_explorer`. The checked-in manifests assert the
application debug payload for profile, location, sort/view mode, selection
state, file/folder counts, native-window chrome metrics, and desktop keyboard
command descriptors before pixel-region checks run.

The glass showcase has the same headless model path through
`examples/glass_showcase_shared` and `phenotype drive glass-showcase`. The
native example imports the shared state/update module, while the CLI applies
typed inputs such as `backdrop:high`, `density:dense`, `inspector:closed`,
`note:...`, and `viewport:640x820@2`, then reports the final state, trace,
public material kinds, and expected material plan count. This keeps material
probe input debugging cheap before running the local glass artifact gate.

`phenotype package bundle` is the lightweight packaging counterpart. It stages
the package manifest, declared SVG app icon, all declared SVG image assets,
locales, Pretendard alias descriptor, and debug artifact manifest into an output
directory, then writes `phenotype.bundle.json` with copied-file records and
package checks. The `macos-app` format also generates an ICNS app icon from the
package-owned SVG `app.icon`, wires `CFBundleIconFile`, and records the ICNS
digest so Finder/Dock icon drift is debuggable from the bundle manifest. This
is not a signed platform installer yet, but it gives CI and future packagers a
concrete resource inventory to validate without launching a native window.

The desktop and mobile file explorer examples also carry initial
`phenotype.package.toml` manifests plus `assets/`, `locales/`, and `fonts/`
fixtures. These package resources are inspectable by `tools/phenotype_cli` and
document the future asset/i18n bundle contract. The package contract now
requires package-owned SVG asset counts, requires `app.icon` to be a package-owned
SVG asset, and requires Pretendard's font descriptor to name a CJK-capable
fallback. Runtime widget text now flows through `Theme::default_font_family`
(`Pretendard` by default), while the
example-specific canvas labels pass the same family explicitly so the Finder
scene and package manifest agree on typography.
The examples also publish `application.file_explorer.preferences`, which joins
the platform `system_settings` snapshot with app/user overrides and the
effective theme values used at launch. This keeps OS font family source,
font-family availability, OS font metrics availability, OS font scale,
font-weight adjustment, OS scroll policy/factors, separate OS and app
vertical/horizontal scroll multipliers, the `apply_system_scroll_metrics`
switch, OS preferred locale/source, system accent capture and availability,
Pretendard package defaults, and direct
environment overrides debuggable from the same artifact bundle. Pretendard stays
the default family; choosing the OS family is an
explicit app override, while system appearance and accent are applied by the
native file explorer examples and recorded in artifacts.
Native file explorer artifacts publish the same contract under
`application.file_explorer.resource_system`: application id/version/entry,
platform list, SVG/preload/runtime-visible asset counts, app icon SVG/preload
state, default locale/font state, locale coverage, Pretendard CJK fallback state,
and debug manifest/probe/verifier declarations. This keeps packaging drift,
icon-resource drift, and artifact-debug drift visible from a startup bundle
without requiring a separate package-inspect command.
The desktop artifact manifest also verifies the built-in icon SVG subset,
arc-lowering policy, SF Symbols rendering-mode vocabulary, regular
text-aligned weight policy, and explicit monochrome/hierarchical/palette/
multicolor capability counts. It now also verifies normal/hovered/pressed
state policy, toolbar/sidebar pressed background alpha, pressed symbol opacity,
and pressed scale. The desktop payload additionally publishes resolved
presentation samples for selected sidebar, pressed toolbar, disabled toolbar,
and PDF file-type glyphs, including visible RGBA after opacity, effective
point size, visual hit target, and likely icon layer/pass. Core button styles
separately preserve a 44 pt minimum activation region by expanding emitted
`HitRegion` commands, so Finder-style icon work is tied to an engine feature
instead of a one-off screenshot fixture. AirDrop uses isolated circular SVG path
arcs in the sidebar, giving the artifact a concrete macOS-style native arc
probe.
The file explorer debug payload also exposes file-type symbol tokens, a shared
`entry_symbol_summary`, and per-entry resolved `symbol`,
`symbol_semantic_reference_name`, and file-type presentation fields, so PDF,
text, archive, image, movie, audio, code, spreadsheet, presentation, folder,
and generic document fallbacks can be
verified without relying on screenshot-only icon recognition.

## Widget coverage

| Public surface | Native | Glass | File explorer | Docs | Android | Tests |
|---|---:|---:|---:|---:|---:|---:|
| `widget::text` | yes | yes | yes | yes | yes | yes |
| text size/color variants | partial | no | yes | partial | partial | yes |
| `widget::code` | yes | yes | no | yes | no | yes |
| `widget::link` | yes | no | no | yes | no | yes |
| `widget::image` local | yes | no | no | no | asset/local | yes |
| `widget::image` remote | yes | no | no | no | no | macOS/Windows |
| `widget::svg_image` | yes | candidate | candidate | no | yes | yes |
| `widget::icon` / `phenotype.icons` | yes | candidate | candidate | no | yes | yes |
| `widget::canvas` | yes | yes | yes | no | no | yes |
| `widget::button` | yes | yes | yes | yes | yes | yes |
| `ButtonVariant::Primary` | yes | yes | yes | no | no | yes |
| `ButtonStyleOptions` | no | no | yes | no | no | yes |
| disabled button | yes | no | no | no | no | yes |
| `widget::checkbox` | yes | no | no | yes | no | yes |
| `widget::radio` | yes | no | no | yes | no | yes |
| `widget::switch_` | yes | no | no | no | no | yes |
| `widget::tabs` | yes | yes | yes | no | no | yes |
| `widget::progress` | yes | yes | no | no | no | yes |
| `widget::progress_indeterminate` | yes | no | no | no | no | yes |
| `widget::text_field` | yes | yes | yes | yes | no | yes |
| text-field error/disabled | yes | no | no | no | no | yes |
| `widget::cell` | no | no | no | no | no | yes |

Gaps:

- Disabled button, disabled/error text field, and determinate progress are now
  present in `examples/native`.
- `widget::cell` is currently covered by tests only.

## Layout coverage

| Public surface | Native | Glass | File explorer | Docs | Android | Tests |
|---|---:|---:|---:|---:|---:|---:|
| `layout::column` | yes | no | yes | yes | yes | yes |
| `layout::row` | yes | yes | yes | yes | yes | yes |
| `layout::box` | yes | no | no | no | no | yes |
| `layout::padded` | yes | no | yes | no | no | implicit |
| `layout::sized_box` | yes | no | yes | yes | no | yes |
| `layout::weighted` | yes | no | yes | no | no | yes |
| `layout::grid` | no | no | no | no | no | yes |
| `layout::scaffold` | yes | yes | no | yes | no | yes |
| `layout::card` | yes | yes | yes | yes | yes | yes |
| `layout::scroll_view` | yes | no | yes | no | no | yes |
| `layout::overlay` | via dialog | no | no | no | no | yes |
| `layout::dialog` | yes | no | no | no | no | yes |
| `layout::accordion` | yes | no | no | no | no | yes |
| `layout::material_surface` | yes | all kinds | yes | no | no | yes |
| `layout::toolbar` / `sidebar` / `status_bar` | file explorer | no | yes | no | no | yes |
| `layout::list_items` / `item` | yes | no | yes | yes | no | yes |
| `layout::divider` | no | no | yes | yes | no | yes |
| `layout::spacer` | yes | yes | yes | yes | yes | yes |
| `phenotype::keyed` | no | no | no | no | no | yes |

Gaps:

- `overlay` is still covered through `layout::dialog`; a standalone overlay
  example remains optional.
- `examples/native` now covers the previously missing compact layout primitives
  and the first material surface.
- `examples/glass_showcase` covers clear, thin, regular, and thick material
  surfaces in one artifact-oriented scene.
- `examples/file_explorer_desktop` and `examples/file_explorer_mobile` cover
  the first app-chrome material helpers: toolbar, sidebar/location strip, and
  status bar surfaces.

## Paint command coverage

| Command / painter path | Native | Glass | File explorer | Android | Tests |
|---|---:|---:|---:|---:|---:|
| `Clear` | yes | yes | yes | yes | yes |
| `FillRect` | yes | yes | yes | yes | yes |
| `StrokeRect` | yes | yes | yes | yes | yes |
| `RoundRect` | yes | yes | yes | yes | yes |
| `DrawText` | yes | yes | yes | yes | yes |
| `DrawLine` | yes | yes | no | yes | yes |
| `HitRegion` | yes | yes | yes | yes | yes |
| `DrawImage` | yes | no | no | asset/local | yes |
| `Scissor` | yes | yes | yes | yes | yes |
| `DrawArc` | yes | no | no | yes | yes |
| `Path` / `stroke_path` | yes | no | no | parser/backend path | yes |
| `FillPath` | yes | no | no | parser/backend path | yes |
| `FillQuads` | yes | yes | no | backend path | yes |
| `FillRects` | yes | yes | yes | backend path | yes |
| `LinearGradientRect` / `Painter::linear_gradient_rect` | yes | yes | yes | backend path | yes |

Gaps:

- `examples/native` now has a paint command showcase for the advanced command
  paths, including bounded gradient strips.
- The remaining examples avoid decorative visual-only scenes; command-level
  coverage now comes from focused tests and the compact native showcase.

## Input coverage

| Input path | Native | Glass | File explorer | Android | Tests |
|---|---:|---:|---:|---:|---:|
| hover | yes | yes | yes | pointer dependent | yes |
| pointer click/tap | yes | yes | yes | yes | yes |
| focus traversal | yes | yes | yes | hardware key | yes |
| Enter/Space activation | yes | yes | Enter command + focused activation | yes | yes |
| text entry | yes | yes | yes | no soft keyboard yet | yes |
| selection / select-all | yes | text field | search fields | no | yes |
| wheel / trackpad scroll | yes | page-level | page-level | wheel/roll | yes |
| keyboard scroll | manual | manual | selection navigation | no | yes |
| IME composition | Windows yes, macOS gap | text field only | search fields | Android gap | partial |
| transient overlay dismiss | platform-specific | no | Escape command | no | yes |

Gaps:

- macOS native IME composition remains a known gap.
- Android soft keyboard and IME composition remain follow-up work.
- The compact native example covers manual input behavior but does not produce
  a scripted input artifact.
- File explorer desktop shortcuts are registered as command descriptors and are
  observable through both `phenotype drive file-explorer` aliases and
  `debug.application.file_explorer.keyboard_commands`; arrow/home/end/page keys
  now drive the same pure selection navigation path as the CLI.
- File explorer focus visibility is observable without native capture through
  `phenotype drive file-explorer --expect focus-visible:...`, then mirrored in
  `debug.application.file_explorer.input_model` for startup artifacts.

## Debug coverage

| Debug capability | WASI/docs | macOS native | Windows native | Android |
|---|---:|---:|---:|---:|
| `snapshot_json` | yes | yes | yes | yes |
| `platform_capabilities` | yes | yes | yes | yes |
| `input_debug` | yes | yes | yes | yes |
| `semantic_tree` | yes | yes | yes | yes |
| `platform_runtime` | yes | yes | yes | yes |
| `capture_frame_rgba` | no | yes | yes | yes |
| `frame.bmp` artifact | no | yes | yes | yes |
| platform diagnostics | no | yes | yes | yes |
| remote image diagnostics | no | yes | yes | Android local/asset only |
| material/glass diagnostics | empty runtime contract + semantic fallback | macOS sampled backdrop + resolved runtime plans | resolved runtime fallback plans | resolved runtime fallback plans |

Gaps:

- Desktop native examples can write startup artifact bundles through
  `PHENOTYPE_ARTIFACT_DIR`.
- `examples/glass_showcase` is the material-focused startup artifact target
  and includes exact labels plus all public material kinds for verifier gates.
- `examples/file_explorer_desktop` and `examples/file_explorer_mobile` are
  product-style glass workflow targets. Their checked-in manifests require
  stable labels, button/material/text-field roles, all material kinds, material
  surface roles, material plan output, semantic/runtime material parity,
  material container ids/spacing, backdrop access/capture summaries,
  next-frame capture reason summaries, stage-capacity/drop counters, and
  bounded resource budgets.
- `phenotype artifact verify <bundle>` validates the common schema, semantic tree,
  disabled semantic state, material kind/fallback metadata, runtime viewport,
  platform diagnostics, backend runtime detail assertions, frame file, optional
  pixel-region contrast/color checks, resolved material plan schema and
  contract-version summary gates, material execution stages, explicit stage
  capacity and dropped-stage checks, backdrop access/capture checks, material
  next-frame capture warmup checks, resource bounds, failure summaries, and
  reusable JSON manifests through the uv-managed verifier implementation.
  `phenotype artifact verify-glass-showcase` is the local material showcase gate.
  Main-branch artifact workflow builds the glass showcase but does not capture
  or verify the slow startup artifact.
  `phenotype artifact verify-file-explorer` builds both file explorer examples
  and applies their manifests with the uv-managed verifier through the CLI.
- Material surfaces render a macOS sampled-backdrop material path when the
  previous frame capture is ready. If a supported backend lacks that stable
  source, the pure plan reports a deterministic `no-backdrop-source` fallback
  with `capture_reason: warmup-next-frame` so the next frame can sample without
  an invisible backend policy decision. macOS keeps the material backdrop source
  separate from the final artifact/readback texture and captures it before text
  and overlays, so sampled glass cannot feed foreground text back into the next
  frame's blur. Windows and Android consume the same `MaterialRect` command as
  deterministic translucent fallback and still write `renderer.material_plans[]`
  with fallback path/pass metadata.
- Android has a local device/emulator contract runner via
  `phenotype android contract`; CI wiring remains a separate policy decision.

## Platform image coverage

| URL kind | macOS | Windows | Android |
|---|---:|---:|---:|
| local filesystem path | supported | supported | supported |
| `file://` | supported | supported | supported |
| `asset://` | no | no | supported |
| `http://` / `https://` | supported | supported | rejected |
| failed remote placeholder | supported | supported | rejected/local placeholder |

The Android README now matches the code: `http://` and `https://` images are
rejected with `remote images not implemented (stage 7)` in
`src/phenotype.native.android.cppm`.

## Artifact bundle expectations

Native and Android backends expose the same debug API shape:

```cpp
auto bundle = phenotype::native::debug::write_artifact_bundle(
    "/tmp/phenotype-native-artifacts",
    "manual-example-capture");
```

The bundle layout is documented in `docs/DEBUG_WORKFLOW.md`:

- `snapshot.json`
- optional `frame.bmp`
- optional `platform/<platform>-runtime.json`

Current status by example:

| Example | Expected artifact route | Current gap |
|---|---|---|
| `examples/native` | `phenotype run examples/native --artifact-dir /tmp/phenotype-native-startup --artifact-exit`, or direct `.exon/debug/native` with the same environment variables | Verified by `phenotype artifact verify <bundle>`; scenario-specific pixel-region checks can be added as needed |
| `examples/glass_showcase` | `phenotype run glass_showcase --artifact-dir /tmp/phenotype-glass-showcase --artifact-exit`, `phenotype artifact verify-glass-showcase` from the CLI, or `phenotype drive glass-showcase --script examples/glass_showcase/glass_showcase.drive` for headless input checks | Verifies all public material kinds, macOS material capability, resolved material plan schema and contract version, material execution stages, explicit stage capacity/drop counters, surface-role summary, exact material/container/shape/foreground/backdrop-access/capture-reason plan summary, foreground-excluded material backdrop capture, semantic/runtime material parity, material quality policy, material resource/capture bounds, foreground text execution counters, fallback metadata, startup-frame pixel regions, and the app-level `GlassProbeContract` under `debug.application.glass_showcase` through `examples/glass_showcase/artifact_manifest.json`; the shared drive model verifies backdrop/density/inspector/viewport/material-count/probe-contract state without native capture; run the verifier locally before material PRs, while CI keeps only build-level artifact gates |
| `examples/file_explorer_desktop` | `phenotype run file_explorer_desktop --artifact-dir /tmp/phenotype-file-explorer --artifact-exit`, `phenotype package bundle examples/file_explorer_desktop --format macos-app --output "/tmp/Phenotype File Explorer.app"`, or `phenotype artifact verify-file-explorer` from the CLI | Verifies a Finder-style desktop startup scene with glass toolbar/sidebar/icon-grid/contextual status surfaces, audited macOS-style SVG icon contracts for toolbar/sidebar/file-type glyphs including supported style attributes, round cap/join policy, regular text-aligned symbol weight, SF Symbols rendering-mode vocabulary, monochrome/hierarchical capability counts, explicit palette/multicolor unsupported counts, resolved role/state presentation samples, and round-stroke symbol counts, neutral unselected Recents startup state with the default status bar hidden, deterministic recent ordering for the Korean PDF probe row, stable document/image/video/folder thumbnail labels, stable create/duplicate/delete labels, a real sandboxed Trash location, operation receipts for file select/open/create/read/duplicate/delete and folder select/open/create/delete scenarios, shared app-debug sort/search/view-mode receipts, keyboard command descriptors for search/activation/arrow-navigation/home-end/page-navigation/delete/duplicate/new-folder/dismissal, selection action metadata, Finder segmented toolbar group/separator/button metrics, sidebar symbol/label/section metrics, column location row/icon metrics, icon-grid thumbnail/label/top-inset/gap metrics, Finder density policy, native-window control ownership through `ExplorerChromeMetrics`, and macOS runtime `standardWindowButton:` count/frame integration inside the leading content reserve, package staging checks for flat resources and local macOS `.app` bundles with generated `Info.plist`, launcher, executable, `Contents/Resources`, and stored SHA-256 integrity records, semantic/runtime material parity, explicit material surface roles, material container identity, foreground-excluded backdrop access/capture bounds, executable material shape validity, bounded material resource budgets including stage capacity/drop counters, foreground text execution counters, macOS active-Space/key-window diagnostics, native `artifact_capture_visibility_state`/`artifact_capture_ready` artifact diagnostics plus stricter `visibility_state`/`ready_for_user_interaction` launch-health diagnostics, and pixel-region/forbidden-palette checks for the titlebar reserve, sidebar, toolbar, and icon grid; local gate only by default |
| `examples/file_explorer_mobile` | `phenotype run file_explorer_mobile --artifact-dir /tmp/phenotype-file-explorer-mobile --artifact-exit`, or `phenotype artifact verify-file-explorer` from the CLI | Verifies the compact mobile browse/preview/create startup scene with all material kinds, stable navigation labels including Trash, shared file-type SVG symbols in Browse rows through `entry_symbol_summary`, operation receipts for file open/create/read/duplicate/delete and folder open/create/delete scenarios, shared app-debug sort/search/view-mode receipts, duplicate/delete action metadata, semantic/runtime material parity, explicit toolbar/navigation/status material roles, material container identity, backdrop access/capture bounds, executable material shape validity, bounded material resource budgets including stage capacity/drop counters, and foreground text execution counters; local gate only by default |
| `examples/android` | `phenotype android contract` enables the app-private artifact hook, pulls `snapshot.json`, `frame.bmp`, and `platform/android-runtime.json` with `adb run-as`, then applies `examples/android/artifact_manifest.json` | Verifies Android debug/runtime basics plus a real `MaterialRect` fallback plan, exact fallback material plan summary and contract version, semantic/runtime material role parity, material shape validity, inactive backdrop access, material quality policy, material resource bounds, and explicit zero dropped-stage count; CI device/emulator wiring remains future work |
| `docs` | WASI snapshot bundle is available when the host preopens a writable directory | Default `exon test --target wasm32-wasi` does not preopen one |

The verifier milestone now consumes startup bundles and reports schema,
semantic, runtime, frame-file, and optional pixel-region invariant failures.
`examples/glass_showcase` is the first material-focused target for those
checks.

## Required next coverage work

After seeding the first material API, keep the remaining coverage work
focused on deterministic visual verification and platform contract parity:

1. Mirror the macOS blur-specific material-region probes on additional
   backends as they gain native material support.
2. Add Windows startup artifact automation if the runner proves reliable.
3. Decide whether Android device/emulator contract coverage belongs in CI or
   remains a local/device gate.

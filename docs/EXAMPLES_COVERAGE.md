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
mise run android:doctor
mise run android
mise run android:logs
mise run android:contract
```

For docs/WASI dogfooding:

```sh
cd docs
mise exec -- exon build --target wasm32-wasi
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

From the repo root, verify a generated bundle before using it as a debugging
artifact:

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
  --require-material-fallback
```

For `examples/glass_showcase`, use the checked-in manifest:

```sh
mise exec -- uv run --frozen python tools/verify_artifact_bundle.py /tmp/phenotype-glass-showcase \
  --expect-platform macos \
  --manifest examples/glass_showcase/artifact_manifest.json
```

The repo-local gate wraps build, startup capture, and manifest verification:

```sh
tools/verify_glass_showcase_artifact.sh
```

The gate pins `PHENOTYPE_ACCESSIBILITY_DISPLAY=standard` by default so local or
runner Accessibility settings cannot turn the showcase into a reduced
transparency fallback. Override that environment variable when deliberately
capturing accessibility-response artifacts.
Use `tools/verify_glass_showcase_accessibility_artifact.sh` for the committed
accessibility-response gate; it runs the same scene with reduced transparency,
increased contrast, and reduced motion enabled, then applies
`examples/glass_showcase/artifact_manifest.accessibility.json`.

The file explorer examples use the same artifact contract, but are intended as
local product-workflow smoke tests instead of a default CI gate:

```sh
tools/verify_file_explorer_artifacts.sh
```

Pull-request CI does not run these slow startup artifact captures. Code changes
run the root test matrix, docs changes run the docs WASI build, and tooling-only
changes run the uv-managed verifier checks without root C++ tests. The
main-branch push workflow only runs artifact/docs build gates, not the full code
test matrix or glass artifact capture. WASI root tests and docs builds run on
Linux runners; native artifact builds remain macOS-only. Workflow-file changes
deliberately enable all relevant gates so runner policy edits validate
themselves.

## Example roles

| Example | Platforms | Role |
|---|---|---|
| `examples/native` | macOS, Windows | Compact desktop widget showcase for shared controls, input debug, local/remote images, scroll, resize, and manual acceptance |
| `examples/glass_showcase` | macOS, Windows | Material and glass-debug acceptance scene for deterministic backdrop regions, macOS sampled backdrop, all material kinds, artifact capture, and pixel-region checks |
| `examples/file_explorer_desktop` | macOS, Windows | Finder-style desktop product workflow with glass toolbar action clusters, sidebar locations, real sandboxed Trash, Recents icon grid, document/image/video/folder thumbnails, icon-revealed search, file/folder create, duplicate, delete, file preview, and sandboxed temp-root operations |
| `examples/file_explorer_mobile` | macOS, Windows | Mobile file explorer layout with browse/preview/create tabs, compact location strip including Trash, material surfaces, search, file preview, file/folder create, duplicate, delete, and the same sandboxed model |
| `examples/android` | Android | GameActivity/Vulkan packaging route, Android event dispatch, platform lifecycle, asset/local images, debug API, and logcat/manual device validation |
| `docs` | WASI | Dogfooded documentation app, WASI build, JS shim integration, and web-facing DSL examples |

The file explorer examples deliberately keep filesystem side effects at the
example edge. The shared model operates only under a deterministic temp
directory (`phenotype-file-explorer-desktop` or
`phenotype-file-explorer-mobile`) and never points at the user's real home
folder. File create, folder create, read, duplicate, Trash movement, and
permanent delete-from-Trash actions produce operation receipts, and sort mode
changes produce a shared `Sort: ...` status contract, that the desktop and
mobile status surfaces expose to artifact verification. This keeps the examples
useful for interactive product checks while preserving a stable startup
artifact contract.

## Widget coverage

| Public surface | Native | Glass | File explorer | Docs | Android | Tests |
|---|---:|---:|---:|---:|---:|---:|
| `widget::text` | yes | yes | yes | yes | yes | yes |
| text size/color variants | partial | no | yes | partial | partial | yes |
| `widget::code` | yes | yes | no | yes | no | yes |
| `widget::link` | yes | no | no | yes | no | yes |
| `widget::image` local | yes | no | no | no | asset/local | yes |
| `widget::image` remote | yes | no | no | no | no | macOS/Windows |
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
| Enter/Space activation | yes | yes | partial | yes | yes |
| text entry | yes | yes | yes | no soft keyboard yet | yes |
| selection / select-all | yes | text field | search fields | no | yes |
| wheel / trackpad scroll | yes | page-level | page-level | wheel/roll | yes |
| keyboard scroll | manual | manual | no | no | yes |
| IME composition | Windows yes, macOS gap | text field only | search fields | Android gap | partial |
| transient overlay dismiss | platform-specific | no | no | no | yes |

Gaps:

- macOS native IME composition remains a known gap.
- Android soft keyboard and IME composition remain follow-up work.
- The compact native example covers manual input behavior but does not produce
  a scripted input artifact.

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
  material container ids/spacing, and bounded resource budgets.
- `tools/verify_artifact_bundle.py` validates the common schema, semantic tree,
  disabled semantic state, material kind/fallback metadata, runtime viewport,
  platform diagnostics, backend runtime detail assertions, frame file, optional
  pixel-region contrast/color checks, resolved material plan schema and
  contract-version summary gates, material execution stages, material resource
  bounds, failure summaries, and reusable JSON manifests.
  `tools/verify_glass_showcase_artifact.sh` is the local material showcase gate.
  Main-branch artifact workflow builds the glass showcase but does not capture
  or verify the slow startup artifact.
  `tools/verify_file_explorer_artifacts.sh` builds both file explorer examples
  and applies their manifests with the uv-managed verifier through `mise`.
- Material surfaces render a macOS sampled-backdrop material path when the
  previous frame capture is ready; Windows and Android consume the same
  `MaterialRect` command as deterministic translucent fallback and still write
  `renderer.material_plans[]` with fallback path/pass metadata.
- Android has a local device/emulator contract runner via
  `mise run android:contract`; CI wiring remains a separate policy decision.

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
| `examples/native` | `mise exec -- exon build`, then `PHENOTYPE_ARTIFACT_DIR=/tmp/phenotype-native-startup PHENOTYPE_ARTIFACT_EXIT=1 .exon/debug/native` | Verified by `tools/verify_artifact_bundle.py`; scenario-specific pixel-region checks can be added as needed |
| `examples/glass_showcase` | Same environment hook with `.exon/debug/glass_showcase` after the material scene startup frame renders | Verifies all public material kinds, macOS material capability, resolved material plan schema and contract version, material execution stages, surface-role summary, exact material/container/shape/foreground plan summary, semantic/runtime material parity, material quality policy, material resource bounds, foreground text execution counters, fallback metadata, and startup-frame pixel regions through `examples/glass_showcase/artifact_manifest.json`; run the verifier locally before material PRs, while CI keeps only build-level artifact gates |
| `examples/file_explorer_desktop` | Same environment hook with `.exon/debug/file_explorer_desktop`, or `tools/verify_file_explorer_artifacts.sh` from the repo root | Verifies a Finder-style desktop startup scene with glass toolbar/sidebar/icon-grid/status surfaces, stable document/image/video/folder thumbnail labels, stable create/duplicate/delete labels, a real sandboxed Trash location, operation receipts for file create/read/duplicate/delete and folder create/delete scenarios, shared sort/search-state receipts, selection action metadata, semantic/runtime material parity, explicit material surface roles, material container identity, executable material shape validity, bounded material resource budgets, foreground text execution counters, macOS active-Space/key-window diagnostics, and pixel-region checks for the sidebar, toolbar, icon grid, and selected-file label; local gate only by default |
| `examples/file_explorer_mobile` | Same environment hook with `.exon/debug/file_explorer_mobile`, or `tools/verify_file_explorer_artifacts.sh` from the repo root | Verifies the compact mobile browse/preview/create startup scene with all material kinds, stable navigation labels including Trash, operation receipts for file create/read/duplicate/delete and folder create/delete scenarios, shared sort/search-state receipts, duplicate/delete action metadata, semantic/runtime material parity, explicit toolbar/navigation/status material roles, material container identity, executable material shape validity, bounded material resource budgets, and foreground text execution counters; local gate only by default |
| `examples/android` | `mise run android:contract` enables the app-private artifact hook, pulls `snapshot.json`, `frame.bmp`, and `platform/android-runtime.json` with `adb run-as`, then applies `examples/android/artifact_manifest.json` | Verifies Android debug/runtime basics plus a real `MaterialRect` fallback plan, exact fallback material plan summary and contract version, semantic/runtime material role parity, material shape validity, material quality policy, and material resource bounds; CI device/emulator wiring remains future work |
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

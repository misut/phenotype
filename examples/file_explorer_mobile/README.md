# Mobile File Explorer Example

This example exercises the same sandboxed file model as the desktop file
explorer, but presents it as a compact mobile workflow with Browse, Preview,
and Create tabs plus a location strip. The shared filename search filter is
available from the top bar. The Preview tab exposes the same shared selection
action contract as the desktop example, including duplicate and delete
availability, while the Create tab can create either a file or a folder.
Browse taps use the shared `open` contract so folders navigate immediately on
mobile, while files move to the Preview tab for reading and actions.
The compact location strip includes the same sandboxed Trash location as the
desktop sidebar.
The Browse action strip uses the shared `widget::symbol_button` contract for
sort, back, forward, and parent navigation controls. Those buttons resolve
through the same audited SVG catalog and macOS-style state recipes as the
desktop Finder toolbar, including disabled navigation states, source
attribution, and semantic labels for artifacts.
Browse rows consume the shared `file_explorer_shared::entry_symbol` contract
too: folders, PDF/text/image/movie/archive/audio/code/spreadsheet/presentation
files, and generic documents render
through the same audited file-type SVG symbols and Finder-style tint policy as
the desktop icon grid. The startup artifact manifest verifies the pure
entry-symbol summary so a mobile row/icon mismatch is visible from JSON before
pixel inspection.
The mobile package manifest also includes the same runtime-visible
`assets/icons/file-types/` SVG files as the desktop package. They are sourced
from the pinned Lucide revision documented by the icon catalog, not extracted
from Apple or platform-owned system icons. The package carries the Lucide
license notice as a non-runtime text asset so resource bundles retain the
permissive-source notice.
The shared `finder_visual_contract` is still emitted for the mobile profile,
but titlebar/window-control fields resolve to `not_applicable_mobile_shell` and
`none`. This keeps the Apple-asset exclusion, permissive SVG source policy,
focus-ring modality policy, and local-only verifier gate consistent across
desktop and mobile without pretending that a mobile shell has Finder traffic
lights.

All filesystem writes stay inside an example-owned temp directory named
`phenotype-file-explorer-mobile`. The example never points at the user's real
home folder.

The directory also includes an initial `phenotype.package.toml` plus `assets/`,
`locales/`, and `fonts/` fixtures. These are consumed by the new
`tools/phenotype_cli package inspect` command and describe the
asset/i18n/Pretendard bundle contract. The package icon is a phenotype-owned
SVG asset with a macOS-like rounded document-window composition, and the
runtime-visible file-type icons are audited permissive SVG assets; the CLI
checks that every declared SVG is present/preloaded, traffic-light-palette safe,
and that Pretendard has a CJK-capable fallback.
Runtime labels are resolved through the shared pure `ResourceCatalog` helper in
`file_explorer_shared`, while file reads and package inspection remain
CLI/example edge work. Set
`PHENOTYPE_FILE_EXPLORER_LOCALE=ko` to smoke the Korean label path.

## Run

```sh
cd examples/file_explorer_mobile
mise exec -- exon build
mise exec -- exon run
```

## Artifact Gate

From the repo root:

```sh
(cd tools/phenotype_cli && mise exec -- exon build)
tools/phenotype_cli/.exon/debug/phenotype_cli artifact verify-file-explorer \
  --profile mobile
```

`tools/verify_file_explorer_artifacts.sh` is a thin compatibility wrapper for
existing local scripts; it builds the CLI when needed and delegates to the same
command.

For the package-resource contract:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli package inspect --json ../../examples/file_explorer_mobile
```

The package metadata's debug verifier is the CLI command
`phenotype artifact verify-file-explorer`; the shell script remains only as a
thin wrapper for older local run configurations.

At runtime the example reads `phenotype.package.toml` and locale files from
`PHENOTYPE_FILE_EXPLORER_PACKAGE_ROOT`, `PHENOTYPE_PACKAGE_ROOT`, or the current
working directory. `phenotype run file_explorer_mobile` sets the package-root
environment automatically.
The example also applies the native `system_settings` snapshot before
`set_theme`: Pretendard remains the package default, OS font size metrics and
scroll policy become pure theme inputs, system appearance/accent are applied by
default, OS preferred locale seeds the startup language when supported, OS font
family remains an opt-in override, native input timing is captured for
double-tap, key repeat, and caret blink diagnostics, and user overrides win.
Artifacts include the pure theme `resolution` block so a verifier can tell
whether the effective font sizes and scroll multipliers came from the OS,
application overrides, or package defaults.
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
.exon/debug/phenotype_cli run file_explorer_mobile \
  --artifact-dir /tmp/phenotype-file-explorer-mobile-input \
  --artifact-exit \
  --input open:Documents \
  --input select:Project\ Notes.txt
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
resolved.
The native example captures OS settings before the first frame and refreshes
them when app input triggers theme sync, so font, appearance, accent, and scroll
policy changes can be reflected without restarting the demo.
File reads and environment access remain example edge work; parsing and input
application stay in `file_explorer_shared`.
The mobile Create tab exposes the same preference inputs as app buttons
(`System`, `Pretendard`, OS/app text size, `Text +/-`, `Scroll +/-`, and
system/app scroll policy) so touch changes and CLI-driven startup replay share
one model contract.

The checked-in manifest requires stable labels and roles, every public
`MaterialKind`, resolved material plans, semantic/runtime material parity, and
bounded material resource budgets.

The local gate also captures `created-preview`, `deleted-file`,
`trash-view`, `created-folder`, `deleted-folder`, `duplicated-file`,
`documents-preview`, `history-forward`, `sorted-kind`, `search-active`, and
`preferences-panel` startup scenarios through
`PHENOTYPE_FILE_EXPLORER_SCENARIO`. These scenario
artifacts keep mobile file/folder create, Trash movement, duplicate, navigation
history, preview behavior, sort state, active search, and preference controls
debuggable without interactive input replay. File and folder operation
scenarios also write an
`Operation: ...` receipt into the status surface so the same shared model
contract is visible on mobile artifacts.

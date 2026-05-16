# Desktop File Explorer Example

This example is a Finder-style desktop workflow for phenotype's material
system. It uses a glass toolbar with Finder-like semantic view/action/search
buttons, translucent sidebar locations, icon/list/column/gallery file views,
document, image, video, and folder thumbnails, separate select/open semantics
for files and folders, read/create/duplicate/delete file actions, folder
create/delete actions, and compact Finder-style status.
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
of being redrawn by the example. On Windows the native Win32 shell keeps the
same contract through a DWM custom frame, using `WM_NCHITTEST` to preserve
resize edges, caption-button behavior, blank-toolbar dragging, phenotype toolbar
hit regions, and native size/aspect-ratio constraints. The example does not use
a toolkit window shim.
The toolbar itself is a borderless material shell with rounded material control
groups, so the artifact keeps Finder-like chrome without duplicating native
window controls.

Startup frame artifacts intentionally capture phenotype content rather than the
operating system's non-client controls. The top-left titlebar reserve must stay
visually quiet in the artifact because AppKit/Win32 own the real close,
minimize, maximize, and caption-button hit testing at runtime.
The manifest also checks `debug.platform_runtime.details.window` so CI can
prove the example requested `IntegratedTitlebar`, preserved the expected
titlebar/control reserve metrics, and is not using GLFW or another toolkit
window shim.
The artifact also includes `debug.application.file_explorer`, a pure shared
model payload containing the current profile, location, status, sort mode, view
mode, selection state, operation receipt, entry counts, and
`ExplorerChromeMetrics`. The manifest asserts this payload before visual
regions, so an LLM can distinguish model-state drift from renderer/capture
drift without guessing from pixels.
On macOS, the same runtime object reports live `NSWindow` chrome state:
transparent titlebar, full-size content view, hidden native title, and
background dragging must all be enabled. These fields are actual platform
readbacks, not request echoes.
On macOS it additionally checks WindowServer bounds, onscreen state, app
activation, active-Space collection behavior, and key/main window state, which
catches regressions where the process owns a Dock icon but the ordered NSWindow
is still hidden, backgrounded, in another Space, or not eligible for foreground
interaction. The artifact also records WindowServer z-order fields
(`window_server_order_index`, `window_server_app_order_index`, and
`window_server_occluded_by_front_app_window`) so a launch failure can distinguish
"no NSWindow", "offscreen/minimized NSWindow", and "created but covered by
another app window". The AppKit shell installs a small native application
delegate that reorders the existing window when the app becomes active or the
Dock icon is reopened; the example does not emulate traffic-light controls in
phenotype content.

The toolbar search control starts as a Finder-style icon button. Activating it
reveals a search field wired to the shared file model, so desktop and mobile
examples use the same deterministic filename filter and startup artifact
scenario.
Folder clicks use the shared desktop activation contract: the first click
selects the folder so the status and delete capability are observable, while a
second activation opens it. The CLI can exercise the same behavior with
`activate:Folder`, or bypass it with `open:Folder` when a test wants direct
navigation.
The icon-grid column count, visible row budget, titlebar reserve, sidebar row
metrics, and toolbar group/icon button metrics come from the shared pure
`ExplorerChromeMetrics` contract, so `phenotype drive file-explorer --json`
can report the same layout decisions without launching a native window.

All filesystem writes stay inside an example-owned temp directory named
`phenotype-file-explorer-desktop`. The example never points at the user's real
home folder.

The directory also includes an initial `phenotype.package.toml` plus `assets/`,
`locales/`, and `fonts/` fixtures. These are consumed by the new
`tools/phenotype_cli package inspect` command and describe the
asset/i18n/Pretendard bundle contract. Runtime labels are resolved through the
shared pure `ResourceCatalog` helper in `file_explorer_shared`, while file
reads and package inspection remain CLI/example edge work. Set
`PHENOTYPE_FILE_EXPLORER_LOCALE=ko` to smoke the Korean label path.

## Run

```sh
cd examples/file_explorer_desktop
mise exec -- exon build
mise exec -- exon run
```

## Artifact Gate

From the repo root:

```sh
tools/verify_file_explorer_artifacts.sh
```

For the package-resource contract:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli package inspect --json ../../examples/file_explorer_desktop
```

At runtime the example reads `phenotype.package.toml` and locale files from
`PHENOTYPE_FILE_EXPLORER_PACKAGE_ROOT`, `PHENOTYPE_PACKAGE_ROOT`, or the current
working directory. `phenotype run file_explorer_desktop` sets the package-root
environment automatically.
The same command can feed deterministic native startup input through the shared
model parser:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli run file_explorer_desktop \
  --artifact-dir /tmp/phenotype-file-explorer-input \
  --artifact-exit \
  --input select:README.txt \
  --input duplicate
```

For direct launches, `PHENOTYPE_FILE_EXPLORER_INPUTS` accepts newline- or
semicolon-separated inputs, and `PHENOTYPE_FILE_EXPLORER_SCRIPT` points at the
same line-based script format used by `phenotype drive file-explorer
--script`. File reads and environment access remain example edge work; parsing
and input application stay in `file_explorer_shared`.

The checked-in manifest requires stable labels and roles, every public
`MaterialKind`, resolved material plans, semantic/runtime material parity,
semantic toolbar button labels, bounded material resource budgets, and
pixel-region checks for the sidebar, toolbar, and icon grid. The default icon
artifact also requires the neutral `Sort: Recent` status label so accidental
default file selection or sort drift is caught without relying on a human
screenshot comparison.

The gate captures the desktop example in `icon`, `list`, `column`, and
`gallery` modes through `PHENOTYPE_FILE_EXPLORER_VIEW` so each Finder-style
content surface has a machine-readable artifact contract.

It also captures deterministic startup scenarios through
`PHENOTYPE_FILE_EXPLORER_SCENARIO`: `created-preview`, `deleted-file`,
`trash-view`, `created-folder`, `deleted-folder`, `duplicated-file`,
`documents-preview`, `history-forward`, `sorted-kind`, and `search-active`.
These scenarios make create, Trash movement, duplicate, folder operation,
navigation history, file preview behavior, sort state, and active search
visible in the semantic artifact without requiring manual click playback. File
and folder operation scenarios also expose an `Operation: ...` receipt in the
status surface so the artifact can identify the action kind, target, and
result.

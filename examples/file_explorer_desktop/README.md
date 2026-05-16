# Desktop File Explorer Example

This example is a Finder-style desktop workflow for phenotype's material
system. It uses a glass toolbar with Finder-like semantic view/action/search
buttons, translucent sidebar locations, icon/list/column/gallery file views,
document, image, video, and folder thumbnails, read/create/duplicate/delete
file actions, folder create/delete actions, and compact Finder-style
selection status. The sidebar Trash item is backed by a hidden `.Trash`
directory inside the demo sandbox, so delete actions move files and folders to
Trash before any permanent removal behavior is exercised.

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
On macOS it additionally checks WindowServer bounds, onscreen state, app
activation, and key/main window state, which catches regressions where the
process owns a Dock icon but the ordered NSWindow is still hidden, backgrounded,
or not eligible for foreground interaction.

The toolbar search control starts as a Finder-style icon button. Activating it
reveals a search field wired to the shared file model, so desktop and mobile
examples use the same deterministic filename filter and startup artifact
scenario.

All filesystem writes stay inside an example-owned temp directory named
`phenotype-file-explorer-desktop`. The example never points at the user's real
home folder.

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

The checked-in manifest requires stable labels and roles, every public
`MaterialKind`, resolved material plans, semantic/runtime material parity,
semantic toolbar button labels, bounded material resource budgets, and
pixel-region checks for the sidebar, toolbar, icon grid, and selected-file
label.

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

# Desktop File Explorer Example

This example is a Finder-style desktop workflow for phenotype's material
system. It uses a glass toolbar with Finder-like semantic view/action/search
buttons, translucent sidebar locations, icon/list/column/gallery file views,
document, image, video, and folder thumbnails, read/create/duplicate/delete
file actions, and compact Finder-style selection status.

The desktop window requests `WindowChromeStyle::IntegratedTitlebar` with an
explicit titlebar metric. On macOS the backend uses native AppKit titlebar
transparency/full-size content so the system traffic-light controls are
integrated into the content area instead of being redrawn by the example. On
Windows the native Win32 shell keeps the same contract through a DWM custom
frame, using `WM_NCHITTEST` to preserve resize edges, caption-button behavior,
blank-toolbar dragging, phenotype toolbar hit regions, and native
size/aspect-ratio constraints. The example does not use a toolkit window shim.
The toolbar itself is a borderless material shell with rounded material control
groups, so the artifact keeps Finder-like chrome without duplicating native
window controls.

Startup frame artifacts intentionally capture phenotype content rather than the
operating system's non-client controls. The top-left titlebar reserve must stay
visually quiet in the artifact because AppKit/Win32 own the real close,
minimize, maximize, and caption-button hit testing at runtime.
The manifest also checks `debug.platform_runtime.details.window` so CI can
prove the example requested `IntegratedTitlebar`, preserved the expected
titlebar metrics, and is not using GLFW or another toolkit window shim.

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
`duplicated-file`, `documents-preview`, `history-forward`, and `sorted-kind`.
These scenarios make create, delete, duplicate, navigation history, file
preview behavior, and sort state visible in the semantic artifact without
requiring manual click playback. File operation scenarios also expose an
`Operation: ...` receipt in the status surface so the artifact can identify the
action kind, target, and result.

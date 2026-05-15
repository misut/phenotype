# Desktop File Explorer Example

This example is a Finder-style desktop workflow for phenotype's material
system. It uses a glass toolbar with Finder-like semantic view/action/search
buttons, translucent sidebar locations, icon/list/column/gallery file views,
document, image, video, and folder thumbnails, read/create/duplicate/delete
file actions, and compact Finder-style selection status.

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
`duplicated-file`, `documents-preview`, and `history-forward`. These scenarios
make create, delete, duplicate, navigation history, and file preview behavior
visible in the semantic artifact without requiring manual click playback.

# Desktop File Explorer Example

This example is a Finder-style desktop workflow for phenotype's material
system. It uses a glass toolbar with Finder-like semantic view/action/search
buttons, translucent sidebar locations, icon/list/column/gallery file views,
document, image, video, and folder thumbnails, search, create, delete, and
reset actions.

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

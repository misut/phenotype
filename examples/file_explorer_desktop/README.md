# Desktop File Explorer Example

This example is a Finder-style desktop workflow for phenotype's material
system. It uses a glass toolbar, translucent sidebar locations, a Recents icon
grid with document thumbnails, search, create, delete, and reset actions.

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
`MaterialKind`, resolved material plans, semantic/runtime material parity, and
bounded material resource budgets.

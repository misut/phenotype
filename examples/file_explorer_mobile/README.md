# Mobile File Explorer Example

This example exercises the same sandboxed file model as the desktop file
explorer, but presents it as a compact mobile workflow with Browse, Preview,
and Create tabs plus a location strip. The Preview tab exposes the same shared
selection action contract as the desktop example, including duplicate and
delete availability.

All filesystem writes stay inside an example-owned temp directory named
`phenotype-file-explorer-mobile`. The example never points at the user's real
home folder.

## Run

```sh
cd examples/file_explorer_mobile
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

The local gate also captures `created-preview`, `deleted-file`,
`duplicated-file`, `documents-preview`, and `history-forward` startup scenarios
through `PHENOTYPE_FILE_EXPLORER_SCENARIO`. These scenario artifacts keep
mobile create, delete, duplicate, navigation history, and preview behavior
debuggable without interactive input replay.

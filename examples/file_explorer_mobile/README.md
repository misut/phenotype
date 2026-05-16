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

All filesystem writes stay inside an example-owned temp directory named
`phenotype-file-explorer-mobile`. The example never points at the user's real
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
cd examples/file_explorer_mobile
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
.exon/debug/phenotype_cli package inspect --json ../../examples/file_explorer_mobile
```

At runtime the example reads `phenotype.package.toml` and locale files from
`PHENOTYPE_FILE_EXPLORER_PACKAGE_ROOT`, `PHENOTYPE_PACKAGE_ROOT`, or the current
working directory. `phenotype run file_explorer_mobile` sets the package-root
environment automatically.

The checked-in manifest requires stable labels and roles, every public
`MaterialKind`, resolved material plans, semantic/runtime material parity, and
bounded material resource budgets.

The local gate also captures `created-preview`, `deleted-file`,
`trash-view`, `created-folder`, `deleted-folder`, `duplicated-file`,
`documents-preview`, `history-forward`, `sorted-kind`, and `search-active`
startup scenarios through `PHENOTYPE_FILE_EXPLORER_SCENARIO`. These scenario
artifacts keep mobile file/folder create, Trash movement, duplicate, navigation
history, preview behavior, sort state, and active search debuggable without
interactive input replay. File and folder operation scenarios also write an
`Operation: ...` receipt into the status surface so the same shared model
contract is visible on mobile artifacts.

# Shared File Explorer Model

This package exports the `file_explorer_shared` C++ module used by the
desktop and mobile file explorer examples.

The module owns the sandboxed file model, deterministic demo tree,
file/folder create and delete behavior, sandboxed Trash navigation,
duplicate/read/navigation behavior, filename search, sort mode state,
selection action metadata, operation receipts, and artifact startup scenarios.
Keeping it as a separate exon library makes the example UI packages depend on
a normal module contract instead of a relative header include.

The module also owns the typed input/observation contract used by
`phenotype drive file-explorer`. `ExplorerInput` describes operations such as
location selection, file selection, search, viewport updates, draft body
updates, create, duplicate, delete, sort, reset, and startup scenarios.
`drive_explorer()` applies those inputs to the sandboxed model and returns
`ExplorerInputTrace` records plus a final `Snapshot` and pure
`ExplorerChromeMetrics`, so the CLI can observe the same workflow and
viewport-derived Finder chrome/grid contract without opening a native AppKit or
Win32 window. The desktop chrome metrics include the integrated titlebar,
native-control ownership, icon-grid capacity, and Finder-style segmented
toolbar group/separator/button counts.

The module also provides the pure file explorer `ResourceCatalog` fixture and
locale label resolver used by the desktop and mobile examples. Package/locale
files are still inspected by the CLI at the edge, but runtime examples consume
the same immutable resource shape so labels and Pretendard defaults are no
longer hard-coded separately in each UI.

## Run

```sh
cd examples/file_explorer_shared
mise exec -- exon test
```

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli drive file-explorer --json \
  --input viewport:900x620@2 \
  --input select:README.txt \
  --input duplicate \
  --input delete
```

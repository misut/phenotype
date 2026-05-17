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
location selection, entry selection, explicit folder open, desktop-style
activation, search, viewport updates, draft body updates, create, duplicate,
delete, sort, reset, and startup scenarios.
`drive_explorer()` applies those inputs to the sandboxed model and returns
`ExplorerInputTrace` records plus a final `Snapshot` and pure
`ExplorerChromeMetrics`, so the CLI can observe the same workflow and
viewport-derived Finder chrome/grid contract without opening a native AppKit or
Win32 window. The desktop chrome metrics include the integrated titlebar,
native-control ownership, titlebar drag/control reserve widths, icon-grid
capacity, and Finder-style segmented toolbar group/separator/button counts plus
the sidebar row, toolbar group, toolbar icon button, and top-level chrome
geometry that the desktop example consumes. The geometry contract exposes the
window inset/gap, sidebar surface/first-row coordinates, toolbar shell and
trailing group coordinates, collapsed search position, and content surface
origin so headless CLI traces can reason about Finder-like placement before a
native screenshot exists. They also carry `status_bar_visible`, keeping the
default Recents surface clean while making search, selection, and
file-operation receipts observable in both native artifacts and CLI output.
Desktop metrics also include the pure `thumbnail_system` contract consumed by
the icon-grid painter: PDF page/fold/detail sizing, media preview sizing,
image/video detail counts, shadow policy, and the explicit
`uses_external_previews=false` rule. This keeps Finder-like thumbnail polish
observable from `phenotype drive file-explorer --json` before a native renderer
captures pixels.
The same package publishes the desktop keyboard command contract as pure data:
`CommandOrControl+F` for search, `Enter` for selection activation,
`DeleteOrBackspace` for Trash movement, `CommandOrControl+D` for duplicate,
`Shift+CommandOrControl+N` for folder creation, and `Escape` for transient
dismissal. `phenotype drive file-explorer` accepts the matching `key:` and
`shortcut:` aliases, so headless traces and native key dispatch exercise the
same model actions.

The module also provides the pure file explorer `ResourceCatalog` fixture and
locale label resolver used by the desktop and mobile examples. Package/locale
files are still inspected by the CLI at the edge, but runtime examples consume
the same immutable resource shape so labels and Pretendard defaults are no
longer hard-coded separately in each UI. The shared `phenotype.resources`
package also computes `ResourceCatalogContract`, so CLI package inspection can
report default locale/font resolution, asset preload intent, and per-locale
fallback coverage without opening a native window or reading platform APIs.

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
  --input shortcut:duplicate \
  --input key:delete
.exon/debug/phenotype_cli drive file-explorer --json \
  --input select:Documents \
  --input key:enter \
  --input select:Project\ Notes.txt \
  --expect location:Demo\ Root/Documents \
  --expect selected:Project\ Notes.txt
```

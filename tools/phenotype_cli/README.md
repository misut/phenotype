# phenotype CLI

`phenotype_cli` is the first host-side command surface for packaging,
diagnostics, and artifact inspection. The executable presents itself as
`phenotype` in help output so the package can be renamed or installed under
that command later without changing command contracts.

The initial scope is intentionally narrow:

- `phenotype doctor` checks repository-local tool and documentation surfaces.
- `phenotype artifact summary <bundle>` summarizes a debug artifact bundle
  without replacing the Python verifier yet.
- `phenotype artifact verify <bundle>` runs the uv-managed Python verifier
  through `mise` and preserves its JSON report shape.
- `phenotype artifact verify-glass-showcase` and
  `phenotype artifact verify-file-explorer` run the local contract gates from
  the CLI surface. They are intentionally local-only gates by default because
  they launch native examples and can be slow on CI.
- `phenotype package inspect <path>` checks the proposed package manifest,
  application/debug metadata, declared resource counts, referenced `source`
  files, Pretendard default-font policy, asset layout, locale layout, and font
  layout. JSON output includes the normalized `ResourceCatalog` produced by the
  shared pure `phenotype.resources` path package.
- `phenotype package list <root>` scans for package manifests below a root and
  emits a compact package catalog for CI or future bundling, including resource
  counts and catalog diagnostic counts.
- `phenotype package bundle <path> --output <dir>` stages the package manifest,
  assets, locales, fonts, and debug artifact manifest into a bundle directory
  and writes `phenotype.bundle.json` with copied-file records for CI and future
  platform packagers.
- `phenotype drive file-explorer` applies deterministic typed inputs to the
  shared desktop/mobile file explorer model without opening a native window.
  JSON output includes the input trace, sandbox root/current paths, visible
  entries, selection capabilities, operation receipts, and preview excerpts.
- `phenotype commands --json` emits machine-readable command metadata from
  `cppx.cli`.

The CLI keeps process execution, artifact writes, device access, and renderer
control outside the first pass except for commands that explicitly wrap local
edge gates. Future commands can call those edge adapters while preserving the
input and output abstraction model documented in
`docs/PHENOTYPE_CLI_ROADMAP.md`.

Example:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli drive file-explorer --json \
  --input select:README.txt \
  --input duplicate \
  --input delete
.exon/debug/phenotype_cli package bundle --json \
  ../../examples/file_explorer_desktop \
  --output /tmp/phenotype-file-explorer
```

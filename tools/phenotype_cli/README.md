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
  they launch native examples and can be slow on CI. The legacy
  `tools/verify_glass_showcase_artifact.sh`,
  `tools/verify_glass_showcase_accessibility_artifact.sh`, and
  `tools/verify_file_explorer_artifacts.sh` entry points are compatibility
  wrappers that build and delegate to these CLI commands. File explorer gates
  can be narrowed with `--profile`, repeated `--view-mode`, and repeated
  `--scenario` options for faster local iteration before running the full gate.
- `phenotype package inspect <path>` checks the proposed package manifest,
  application/debug metadata, declared resource counts, referenced `source`
  files, Pretendard default-font policy, asset layout, locale layout, and font
  layout. JSON output includes the normalized `ResourceCatalog` produced by the
  shared pure `phenotype.resources` path package; the CLI only reads files and
  checks paths, while manifest and locale text parsing stay in the pure package.
- `phenotype package list <root>` scans for package manifests below a root and
  emits a compact package catalog for CI or future bundling, including resource
  counts and catalog diagnostic counts.
- `phenotype package bundle <path> --output <dir>` stages the package manifest,
  assets, locales, fonts, and debug artifact manifest into a bundle directory
  and writes `phenotype.bundle.json` with copied-file records, content metadata,
  SHA-256 digests, byte counts, and bundle-level integrity totals for CI and
  future platform packagers.
- `phenotype package verify-bundle <dir>` re-opens a staged bundle directory,
  rebuilds the package resource contract from the copied manifest, checks that
  every declared resource is present, and recomputes SHA-256 digests without
  needing the original source package root.
- `phenotype drive file-explorer` applies deterministic typed inputs to the
  shared desktop/mobile file explorer model without opening a native window.
  JSON output includes the input trace, sandbox root/current paths, visible
  entries, viewport, pure Finder chrome/grid metrics, selection capabilities,
  operation receipts, and preview excerpts.
- `phenotype run <example>` builds and runs a repository example from one
  command, with `--json`, explicit environment overrides, artifact capture
  environment, and an optional timeout. This is the first CLI-owned launch path
  for observing native examples without adding another shell wrapper.
- `phenotype android ...` is the single Android workflow namespace for
  repository-local doctor/devices/emulator/build/APK/install/run/logs/screencap
  and artifact-contract commands. The implementation still delegates to the
  compatibility scripts under `tools/android`, but the CLI owns the stable
  command names and JSON result envelope.
- `phenotype commands --json` emits machine-readable command metadata from
  `cppx.cli`.

The CLI keeps process execution, artifact writes, device access, and renderer
control outside the first pass except for commands that explicitly wrap local
edge gates. Future commands can call those edge adapters while preserving the
input and output abstraction model documented in
`docs/PHENOTYPE_CLI_ROADMAP.md`. Python verifier calls go through `uv` and use
`PHENOTYPE_UV_PROJECT_ENVIRONMENT` or a temp-directory default so local gates do
not leave a project `.venv` behind.

Example:

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli drive file-explorer --json \
  --input viewport:900x620@2 \
  --input select:README.txt \
  --input duplicate \
  --input delete
.exon/debug/phenotype_cli package bundle --json \
  ../../examples/file_explorer_desktop \
  --output /tmp/phenotype-file-explorer
.exon/debug/phenotype_cli package verify-bundle --json \
  /tmp/phenotype-file-explorer
.exon/debug/phenotype_cli artifact verify-file-explorer \
  --profile desktop \
  --view-mode icon \
  --scenario search-active
.exon/debug/phenotype_cli run glass_showcase \
  --artifact-dir /tmp/phenotype-glass-showcase \
  --artifact-exit \
  --timeout-seconds 120
.exon/debug/phenotype_cli run file_explorer_desktop \
  --artifact-dir /tmp/phenotype-file-explorer \
  --artifact-exit \
  --env PHENOTYPE_FILE_EXPLORER_VIEW=icon
.exon/debug/phenotype_cli android doctor --json
.exon/debug/phenotype_cli android devices --json
.exon/debug/phenotype_cli android contract --json \
  --state-dir /tmp/phenotype-android \
  --contract-out /tmp/phenotype-android/contract-bundle
```

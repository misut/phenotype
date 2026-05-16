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
- `phenotype observe <bundle>` emits one output-observation envelope for an
  artifact bundle. It parses `snapshot.json` in C++, reports semantic tree and
  platform-runtime presence, material plan counts, material kinds/roles,
  fallback and backdrop capture reasons, executor summaries, likely
  layer/pass hints, frame/platform file state, and optional uv-managed verifier
  output when `--manifest` or `--verify` is supplied.
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
  operation receipts, preview excerpts, localized labels, package resource
	  metadata, and optional expectation results. Repeated `--script` files feed
	  line-based input sequences, and repeated `--expect` values make the command a
	  small state verifier for file select/open/read/create/duplicate/delete
	  workflows.
- `phenotype drive glass-showcase` applies deterministic typed inputs to the
  shared glass showcase model without opening a native window. JSON output
  includes the final state, per-input trace, public material kinds, expected
  material plan count, backdrop/inspector/density/viewport state, progress
  value, and optional expectation results. The native example imports the same
  shared module, so this command observes the probe scene's input abstraction
  before running slow artifact capture.
- `phenotype run <example>` builds and runs a repository example from one
  command, with `--json`, explicit environment overrides, artifact capture
  environment, and an optional timeout. This is the first CLI-owned launch path
  for observing native examples without adding another shell wrapper. When an
  example owns `phenotype.package.toml`, the run command passes
  `PHENOTYPE_PACKAGE_ROOT`, and file explorer examples also receive
  `PHENOTYPE_FILE_EXPLORER_PACKAGE_ROOT`, so runtime labels and font defaults
  can come from the packaged manifest/locales.
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
.exon/debug/phenotype_cli drive file-explorer --json \
  --input open:Documents \
  --input select:Project\ Notes.txt \
  --expect location:Demo\ Root/Documents \
  --expect selected:Project\ Notes.txt
.exon/debug/phenotype_cli drive file-explorer --json \
  --script /tmp/file-explorer.drive \
  --package ../../examples/file_explorer_desktop \
  --locale ko \
  --expect location:Trash \
  --expect 'operation:file_delete:ok'
.exon/debug/phenotype_cli drive glass-showcase --json \
  --script ../../examples/glass_showcase/glass_showcase.drive \
  --expect backdrop:high \
  --expect density:dense \
  --expect material-count:7
.exon/debug/phenotype_cli package bundle --json \
  ../../examples/file_explorer_desktop \
  --output /tmp/phenotype-file-explorer
.exon/debug/phenotype_cli package verify-bundle --json \
  /tmp/phenotype-file-explorer
.exon/debug/phenotype_cli artifact verify-file-explorer \
  --profile desktop \
  --view-mode icon \
  --scenario search-active
.exon/debug/phenotype_cli observe --json /tmp/phenotype-glass-showcase \
  --manifest ../../examples/glass_showcase/artifact_manifest.json \
  --expect-platform macos \
  --require-frame
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

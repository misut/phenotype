# phenotype CLI

`phenotype_cli` is the first host-side command surface for packaging,
diagnostics, and artifact inspection. The executable presents itself as
`phenotype` in help output so the package can be renamed or installed under
that command later without changing command contracts.

Implementation is split into CLI modules so the executable can grow without
turning `main.cpp` into every subsystem at once:

- `phenotype_cli.common` owns shared JSON escaping, path normalization, text
  file IO, positional/error helpers, check summaries, and resource diagnostic
  emission.
- `phenotype_cli.runtime` owns repository discovery, artifact bundle summaries,
  temporary artifact directories, example build/run process execution, and
  process-result JSON helpers. These are CLI edge adapters and should stay out
  of pure package, icon, theme, and file explorer model modules.
- `phenotype_cli.command_tree` owns machine-readable command tree emission for
  `phenotype commands --json`, keeping recursive command metadata formatting
  away from command dispatch.
- `phenotype_cli.doctor` owns repository-local health checks and the
  legacy-tool-to-CLI migration matrix for `phenotype doctor`.
- `phenotype_cli.artifacts` owns artifact summary, uv-managed verifier
  invocation, snapshot/material observation, and LLM-actionable
  likely-layer/suggestion reporting for `phenotype artifact ...` and
  `phenotype observe`.
- `phenotype_cli.contracts` owns pure theme and IO contract checks/JSON for
  `phenotype theme contract`, `phenotype theme resolve`, and
  `phenotype io contract`.
- `phenotype_cli.file_explorer` owns file explorer drive JSON, chrome/native
  window control JSON, expectation JSON, keyboard/focus model JSON, and
  localized label emission.
- `phenotype_cli.file_explorer_gate` owns the local file explorer artifact
  gate: shared tests, desktop/mobile example builds, scenario artifact capture,
  verifier argument shaping, and LLM-actionable JSON/status summaries.
- `phenotype_cli.glass_showcase` owns the glass showcase drive command JSON,
  expectation checking, pure input script parsing, and local artifact gate.
- `phenotype_cli.icons` owns `phenotype icons ...` and `phenotype svg inspect`
  command routing plus lookup/presentation/render JSON and the icon helper
  payloads reused by file explorer debug output. `phenotype_cli.icons_common`
  owns shared lookup, role parsing, color/source JSON, and interaction helpers;
  `phenotype_cli.icons_render` owns standalone rendered-icon SVG wrapping;
  `phenotype_cli.icons_checks` keeps catalog checks separate from command
  routing; and `phenotype_cli.icons_svg_inspect` owns SVG support and
  inspection JSON.
- `phenotype_cli.package_types` owns the exported immutable package, SVG
  inspection, and bundle summary value types so command routing and future
  packaging submodules can share the contract without importing package IO
  helpers.
- `phenotype_cli.package_inspect` owns package manifest inspection and resource
  catalog checks. It reads files at the CLI edge, then lowers manifest and
  locale text into the pure `phenotype.resources` contract.
- `phenotype_cli.package_json` owns package/resource JSON envelopes for
  `package inspect` and `package list`, keeping machine-readable output
  formatting separate from filesystem work.
- `phenotype_cli.package_bundle_json` owns staged-bundle JSON, stored manifest
  parsing, digest comparison, and bundle integrity checks.
- `phenotype_cli.package` owns package command routing plus bundle staging and
  macOS app bundle helpers. It re-exports the package submodules so existing
  consumers can keep importing `phenotype_cli.package`.
- `phenotype_cli.app` owns the remaining high-level command parsing, example
  run orchestration, Android script dispatch, and routing to focused modules.
- `main.cpp` is only the executable entry point and should stay a tiny forwarder
  into `phenotype_cli.app`.

The initial scope is intentionally narrow:

- `phenotype doctor` checks repository-local tool and documentation surfaces
  and reports a tool-migration matrix that maps legacy shell/Python entry
  points to their CLI replacements.
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
- `phenotype artifact verify-glass-showcase` owns the local glass showcase
  build, native artifact capture, and uv-managed verifier flow directly. It is
  intentionally local-only by default because native capture can be slow on CI;
  `tools/verify_glass_showcase_artifact.sh` and
  `tools/verify_glass_showcase_accessibility_artifact.sh` are compatibility
  wrappers that build and delegate to this command.
- `phenotype artifact verify-file-explorer` runs the local desktop/mobile file
  explorer contract gate from the CLI surface. It runs the shared model tests,
  builds the selected native examples, captures deterministic artifacts, and
  invokes the uv-managed verifier without delegating to the shell wrapper. For
  desktop captures, the gate requires the leading native-control reserve to
  stay blank in phenotype content while debug/runtime metadata proves AppKit or
  Win32 owns the real traffic-light/caption-button controls. The CLI no longer
  injects artifact traffic-light markers because those can visually duplicate
  OS-owned controls. The gate can be
  narrowed with
  `--profile`, repeated `--view-mode`, and repeated `--scenario` options for
  faster local iteration before running the full gate;
  `tools/verify_file_explorer_artifacts.sh` is a thin compatibility wrapper
  that builds and delegates to this command.
- `phenotype theme resolve` runs the same pure theme preference resolver used
  by examples without importing the native `phenotype` package. It accepts
  explicit OS snapshot fields and app overrides, then reports the effective
  font family, color scheme, text metrics, scroll multipliers, and resolution
  booleans as JSON. OS fields include availability bits, so fallback defaults
  remain distinct from preferences actually supplied by a backend or test.
  This is the CLI-friendly path for checking
  `system_settings + app_overrides -> effective_theme` on Linux CI.
- `phenotype package inspect <path>` checks the proposed package manifest,
  application/debug metadata, declared resource counts, referenced `source`
  files, CLI-owned debug verifier metadata, app-icon SVG policy, Pretendard
  default-font policy, CJK fallback coverage, SVG asset layout,
  source-safe file-type icon provenance, native-chrome-safe SVG asset palettes,
  locale layout, and font layout.
  JSON output includes the normalized `ResourceCatalog` produced
  by the shared pure `phenotype.resources` path package plus its pure
  `ResourceCatalogContract`. The contract reports asset preload/runtime-visible
  intent, SVG asset counts and policy, `app.icon` SVG/preload state, default
  locale/font resolution, CJK fallback state, debug metadata presence, and
  per-locale fallback coverage for the default key set. The same command reads
  each declared SVG asset at the CLI edge and reports
  `svg_asset_inspections` with viewBox, shape count, paintability, unsupported
  command count, diagnostics, bytes, and SHA-256 from the pure SVG contract.
  File-type SVG assets also report a `catalog_source` block that re-parses the
  embedded audited source associated with the pinned permissive URL and checks
  the packaged asset against that source's viewBox and shape contract. The CLI
  only reads files and checks paths, while
  manifest and locale text parsing stay in the pure package.
- `phenotype package list <root>` scans for package manifests below a root and
  emits a compact package catalog for CI or future bundling, including resource
  counts and catalog diagnostic counts.
- `phenotype package bundle <path> --output <dir>` stages the package manifest,
  assets, locales, fonts, and debug artifact manifest into a bundle directory
  and writes `phenotype.bundle.json` with copied-file records, content metadata,
  the pure resource contract, SHA-256 digests, byte counts, and bundle-level
  integrity totals for CI and platform packagers. Pass `--format macos-app`
  with an `.app` output path to create a development macOS app bundle with
  `Contents/MacOS`, `Contents/Resources`, `Info.plist`, `PkgInfo`, a tiny
  package-root launcher, the built example binary, a generated `.icns` app
  icon derived from the package-owned SVG `app.icon` via macOS `sips` and
  `iconutil`, and the same integrity manifest under `Contents/Resources`.
  The generated `Info.plist` points `CFBundleIconFile` at that ICNS file so
  Finder and Dock launches use the package icon instead of the raw executable
  placeholder.
- `phenotype package verify-bundle <dir>` re-opens a staged bundle directory,
  rebuilds the package resource contract from the copied manifest, checks that
  every declared resource is present, recomputes SHA-256 digests, and compares
  the stored `phenotype.bundle.json` schema, command, file count, byte total,
  relative destinations, generated app icon record, and digests without
  needing the original source package root.
- `phenotype icons catalog` emits the built-in icon catalog contract from the
  pure `phenotype.icon_catalog` path package. JSON output reports the
  macOS/Finder/SF-Symbols-inspired reference policy, package-owned SVG asset
  rule, explicit reference-source URLs, pinned per-symbol source URLs, exact
  ISC versus Feather-derived MIT attribution, supported SVG path subset and
  arc-lowering policy, count invariants,
  SF Symbols rendering-mode names, regular text-aligned weight policy,
  explicit monochrome/hierarchical/palette/multicolor capability counts,
  SVG arc-using built-in symbol count,
  all/sidebar/toolbar/file-type semantic reference sets,
  per-symbol role/variant/rendering/layer metadata, name/reference lookup
  invariants, presentation defaults, role hit-target metrics, and Finder-style
  normal/hovered/pressed/selected/disabled state recipes plus file-type tint
  policies for toolbar/sidebar/folder/document/image/movie glyphs. It also
  reports `document_cache_policy`, the runtime contract that parsed icon SVG
  documents are cached by pure symbol descriptor instead of reparsed during
  ordinary frame rebuilds.
  The command deliberately reports semantic SF Symbols names without embedding
  Apple vector artwork, so CI and future LLM debugging can verify icon intent
  without screenshot guessing.
- `phenotype icons lookup <name-or-reference>` resolves one built-in glyph by
  phenotype symbol name or semantic SF Symbols reference name and reports the
  same presentation role, metrics, ownership, and reference policy as the full
  catalog. This gives tests, examples, and LLM debugging a small probe when a
  Finder-style sidebar or toolbar token maps to the wrong visual metaphor.
- `phenotype icons sources` emits the compact provenance audit for the
  embedded icon sources. JSON output reports the Lucide source revision, unique
  pinned raw SVG records, symbols that use each source icon, license URL,
  remaining phenotype-owned count, reference-source rows, and checks that Apple/SF
  Symbols artwork was not embedded, platform-extracted, or runtime-fetched.
  This is the fast command to run before adding file-type icons from web
  references.
- `phenotype icons file-types` emits the file-explorer package icon contract
  without reading package files. JSON output lists every Finder-style file-type
  token, semantic reference, package asset name/source, package asset policy,
  direct pinned Lucide raw SVG URL, source revision, license metadata, and the
  Apple artwork boundary. Use it before changing `assets/icons/file-types/*.svg`
  so package assets stay aligned with the audited permissive catalog source.
- `phenotype icons svg <name-or-reference>` emits the exact audited SVG
  source for one built-in glyph. The default output is raw SVG for renderer or
  asset-pipeline probes; `--json` wraps the source with the matched symbol,
  semantic reference name, asset policy, exact source attribution, rendering
  capabilities, and byte count.
  This keeps macOS-style icon debugging inside the pure catalog boundary
  without embedding Apple or
  SF Symbols vector artwork.
- `phenotype icons present <name-or-reference>` resolves the same pure
  macOS/Finder-style presentation recipe used by native examples. `--role`,
  `--phase`, `--selected`, and `--disabled` make toolbar, sidebar, navigation,
  file-type, and action glyph states observable without launching AppKit:
  JSON output includes the semantic reference, asset policy, rendering mode,
  tone, visible RGBA after opacity, background color, symbol and hit-target
  metrics, corner radius, and likely icon layer/pass.
- `phenotype icons render <name-or-reference>` wraps the same built-in glyph and
  presentation recipe in a standalone SVG hit target. The default output is raw
  SVG for previews or package probes; `--output <path>` writes that SVG at the
  CLI edge. `--json` adds the rendered SVG source, bytes, viewBox, optional
  output path, role/phase state, visible RGBA, background chrome, and likely
  wrapper pass. This gives the CLI a deterministic icon output path while still
  keeping Apple/SF Symbols artwork out of the repository.
- `phenotype svg inspect <path>` parses a package or local SVG through the pure
  `phenotype_svg_contract` subset and reports the supported elements, path
  commands, style attributes, parser diagnostics, paintability, and
  renderer-facing path.
  Filesystem reads stay at the CLI edge while the parser summary is pure app
  data, which makes SVG asset support observable without starting a native
  renderer. `phenotype package inspect --json` uses the same inspector for all
  manifest-declared SVG assets, so packaged icons and runtime-visible SVG images
  fail before a backend image cache or platform renderer is involved. For
  Finder-style packages, it also rejects any packaged SVG asset that embeds
  macOS traffic-light marker colors because those controls belong to the native
  shell, and it annotates `file_type.*.icon` assets with the same pinned source
  URL/license/provenance contract used by the pure icon catalog. The file
  explorer desktop/mobile packages use this path for their runtime-visible
  file-type icons, all sourced from pinned permissive Lucide raw SVG URLs and
  checked without network access at runtime. The same packages declare a
  non-runtime Lucide license text asset so `package bundle` copies the
  permissive-source notice with the SVG files.
- `phenotype drive file-explorer` applies deterministic typed inputs to the
  shared desktop/mobile file explorer model without opening a native window.
  JSON output includes the input trace, sandbox root/current paths, visible
  entries, viewport, view mode, pure Finder chrome/grid metrics including the
  integrated titlebar geometry policy, icon-system contract, resolved
  sidebar/toolbar/file-type icon presentation samples, per-entry file-type
  symbol names, `entry_symbol_summary`, and `thumbnail_system`
  preview-painter/cache contract, selection capabilities,
  operation receipts with resolved operation plans, preview excerpts,
  localized labels, package resource metadata, desktop keyboard command
  descriptors, a pure `input_model` describing the last input modality,
  focus target, focus order, and keyboard-only focus ring visibility, and
  optional expectation results. Repeated
  `--script` files feed line-based input sequences, and repeated `--expect`
  values make the command a small state verifier for file
  select/open/read/create/duplicate/delete/view-mode/focus workflows. Each operation
  plan describes the sandboxed source/destination, read/write/delete flags,
  Trash movement, permanent delete-from-Trash, and fallback reason before the
  receipt reports whether execution succeeded. When
  expectations are present, the process exit code follows the expectation
  results, so expected failure receipts can be asserted while the JSON
  `operation.ok` field still records the failed operation. The command accepts
  the same key/shortcut aliases published by the shared model, including
  `key:enter`, `key:arrow-up`, `key:arrow-down`, `key:arrow-left`,
  `key:arrow-right`, `key:home`, `key:end`, `key:page-up`,
  `key:page-down`, `key:delete`, `key:escape`, `shortcut:find`,
  `shortcut:duplicate`, and `shortcut:new-folder`. `key:tab` and
  `shift-tab` move the pure focus target and set `focus_visible=true`, while
  `click:*`, `pointer:*`, `pointer-focus:*`, and pointer toolbar commands
  update focus or command state without showing the ring. The More action menu
  records the distinct `more_actions` pointer target, so artifacts can
  distinguish the action cluster from the opened menu while still matching
  macOS-style pointer-vs-keyboard focus behavior.
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
  can come from the packaged manifest/locales. File explorer runs can also take
  repeated `--input` values or one line-based `--script`; the CLI validates
  those inputs through `file_explorer_shared`, then passes them to the native
  example through `PHENOTYPE_FILE_EXPLORER_INPUTS` and
  `PHENOTYPE_FILE_EXPLORER_SCRIPT` before startup artifact capture.
  `--observe-output` makes the command allocate an artifact directory when
  needed, set `PHENOTYPE_ARTIFACT_EXIT=1`, run the native example, and embed the
  same parsed output observation used by `phenotype observe`. This connects
  CLI-driven inputs to native renderer output without a second shell step.
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
  --input view:gallery \
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
.exon/debug/phenotype_cli drive file-explorer --json \
  --input key:tab \
  --expect focus-visible:true \
  --expect focus-target:sidebar \
  --expect input-modality:keyboard
.exon/debug/phenotype_cli drive file-explorer --json \
  --input click:README.txt \
  --expect focus-visible:false \
  --expect focus-target:content-grid \
  --expect input-modality:pointer
.exon/debug/phenotype_cli drive file-explorer --json \
  --script /tmp/file-explorer.drive \
  --package ../../examples/file_explorer_desktop \
  --locale ko \
  --expect location:Trash \
  --expect 'operation:file_delete:ok'
.exon/debug/phenotype_cli drive file-explorer --json \
  --input 'select:../outside.txt' \
  --expect 'operation:file_read:fail' \
  --expect 'status:inside the current folder'
.exon/debug/phenotype_cli drive glass-showcase --json \
  --script ../../examples/glass_showcase/glass_showcase.drive \
  --expect backdrop:high \
  --expect density:dense \
  --expect material-count:7
.exon/debug/phenotype_cli package bundle --json \
  ../../examples/file_explorer_desktop \
  --output /tmp/phenotype-file-explorer
.exon/debug/phenotype_cli package bundle --json \
  ../../examples/file_explorer_desktop \
  --format macos-app \
  --output "/tmp/Phenotype File Explorer.app"
.exon/debug/phenotype_cli package verify-bundle --json \
  /tmp/phenotype-file-explorer
.exon/debug/phenotype_cli package verify-bundle --json \
  "/tmp/Phenotype File Explorer.app"
.exon/debug/phenotype_cli package inspect --json \
  ../../examples/file_explorer_desktop
.exon/debug/phenotype_cli icons lookup magnifyingglass --json
.exon/debug/phenotype_cli icons svg desktopcomputer --json
.exon/debug/phenotype_cli icons present magnifyingglass \
  --role toolbar \
  --phase pressed \
  --json
.exon/debug/phenotype_cli artifact verify-file-explorer \
  --profile desktop \
  --view-mode icon \
  --scenario search-active
.exon/debug/phenotype_cli artifact verify-glass-showcase --json \
  --bundle-dir /tmp/phenotype-glass-showcase
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
  --env PHENOTYPE_FILE_EXPLORER_VIEW=icon \
  --input select:README.txt \
  --input shortcut:duplicate
.exon/debug/phenotype_cli android doctor --json
.exon/debug/phenotype_cli android devices --json
.exon/debug/phenotype_cli android contract --json \
  --state-dir /tmp/phenotype-android \
  --contract-out /tmp/phenotype-android/contract-bundle
.exon/debug/phenotype_cli doctor --json
```

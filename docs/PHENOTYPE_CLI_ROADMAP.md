# Phenotype CLI Roadmap

Status: foundation in progress as part of the long-term Liquid Glass and
native-example stabilization goal. The first CLI milestone preserves the
existing material/debug contracts while replacing shell and Python entry points
only after command parity exists.

## Reference baseline

The CLI direction follows a few proven ideas from Dioxus without copying its
Rust-specific internals:

- `dx` groups development commands such as build, run, bundle, doctor, check,
  and project printing under one terminal tool.
- `dx bundle` produces platform-specific distributable packages and supports a
  machine-readable JSON mode whose final line can be parsed by automation.
- Dioxus assets are declared in app code, collected by CLI tooling, optimized
  or copied, and resolved differently for native bundles and web targets.
- Dioxus documents custom renderers as a split between user events entering
  the UI runtime and renderer mutations leaving it.
- Dioxus points i18n users at explicit translation resources rather than
  treating strings as incidental source files.

References:

- Dioxus Tools:
  https://dioxuslabs.com/learn/0.7/guides/tools/
- Dioxus Bundling:
  https://dioxuslabs.com/learn/0.7/tutorial/bundle/
- Dioxus Assets:
  https://dioxuslabs.com/learn/0.7/essentials/ui/assets/
- Dioxus asset resolver:
  https://docs.rs/dioxus/latest/dioxus/asset_resolver/index.html
- Dioxus Custom Renderer:
  https://dioxuslabs.com/learn/0.7/guides/depth/custom_renderer/
- Dioxus Internationalization:
  https://dioxuslabs.com/learn/0.7/guides/utilities/internationalization/

Local implementation should use the released `cppx` CLI and terminal modules
where available:

- `cppx.cli` for pure command metadata, parsing, suggestions, help, and shell
  completions.
- `cppx.terminal` for pure diagnostics and progress rendering.
- `cppx.terminal.system`, process, shell, filesystem, Android, and packaging
  integrations only at executable edge adapters.

## Product shape

The `phenotype` CLI should become the single developer-facing tool for
packaging, running, observing, and verifying phenotype applications:

```text
External input -> input abstraction -> phenotype -> output abstraction -> real renderer
CLI input      -> input abstraction -> phenotype -> output abstraction -> CLI output
```

The abstraction layers are not a second engine. They are typed value
boundaries:

- `InputEvent`, `InputFrame`, and `InputScript` describe pointer, key, text,
  scroll, viewport, timing, and platform capability inputs without naming
  AppKit, Win32, Android, WASI, or JavaScript APIs.
- `OutputFrame`, `OutputCommandStream`, `OutputObservation`, and
  `ArtifactBundle` describe semantic nodes, command buffers, material plans,
  runtime summaries, pixel contracts, screenshots, and verifier results without
  requiring a real OS window.
- Release builds may keep these as zero-policy value types while production
  adapters directly translate platform input into the same internal calls.
  Filesystem writes, clocks, process execution, shader compilation, captures,
  and device control remain CLI/backend edge effects.

## Command surface

The first durable CLI should expose a small command tree with stable JSON:

| Command | Purpose | Replaces |
|---|---|---|
| `phenotype doctor` | Report toolchain, platform, renderer, Android, and artifact prerequisites. | `tools/android/doctor.sh` plus ad hoc setup checks |
| `phenotype run <example>` | Build and run a local example with consistent logging and `--json`. | scattered `exon run` wrappers |
| `phenotype artifact verify <bundle>` | Validate an artifact bundle and emit human or machine-readable failures. | `tools/verify_artifact_bundle.py` |
| `phenotype artifact verify-glass-showcase` | Build, capture, and verify the glass showcase artifact, including accessibility mode. | `tools/verify_glass_showcase_artifact.sh` |
| `phenotype artifact verify-file-explorer` | Validate desktop/mobile file explorer artifact contracts. | `tools/verify_file_explorer_artifacts.sh` |
| `phenotype android doctor/run/install/logs/screencap/contract` | Keep Android workflows under one command namespace. | `tools/android/*.sh` |
| `phenotype package inspect` | Validate package manifests, assets, locales, fonts, and platform bundle metadata. | new |
| `phenotype package bundle` | Produce staged resource bundles, integrity manifests, and future macOS `.app`/`.dmg`, Windows, Android, and web package inputs. | new |
| `phenotype package verify-bundle` | Recompute staged bundle resource integrity without the original package root. | new |
| `phenotype theme contract` | Emit the pure Apple-like default glass theme and macOS/Finder icon-language contract shared by root `Theme`, CLI metadata, and example artifacts. | new |
| `phenotype io contract` | Emit the pure input-frame and output-observation contract used by CLI/native adapters. | new |
| `phenotype drive file-explorer` | Feed deterministic file workflow inputs into the shared desktop/mobile model. | new |
| `phenotype drive glass-showcase` | Feed deterministic material probe inputs into the shared glass showcase model. | new |
| `phenotype observe <artifact-or-run>` | Emit semantic tree, material plans, compact executor budget, runtime summaries, pixel-region summaries, and likely failing pass/layer. | new |

Every command that can be used by CI must support:

- `--json` for one final machine-readable result line;
- stable exit codes;
- no ANSI progress by default under CI;
- a human path that uses `cppx.terminal` diagnostics;
- a `--no-build` or `--artifact` mode when local artifacts already exist.

## Implemented foundation

`tools/phenotype_cli` is the first executable surface. The package is still
named `phenotype_cli`, but help and command metadata present the command as
`phenotype` so installation can later expose the stable command name without
changing command contracts.

Run the package through `mise exec -- exon build` from
`tools/phenotype_cli`; compatibility root tasks can be added after CI path
classification can distinguish task-only `mise.toml` edits from toolchain
changes.

The executable entry point stays intentionally thin. `src/main.cpp` only
forwards process arguments into `phenotype_cli.app`, while command metadata,
shared JSON/check helpers, package inspection, icon command routing, catalog
checks, SVG inspection, contracts, file-explorer observation, and glass-showcase
observation live in dedicated C++ modules. New commands should extend those
modules or add a narrowly named module instead of growing the entry point again.

Current commands:

| Command | Status | Notes |
|---|---|---|
| `phenotype doctor` | implemented | Read-only repository checks for `mise.toml`, verifier tools, Android contract script, CLI roadmap, shared packages, and the legacy-tool-to-CLI migration matrix. |
| `phenotype commands --json` | implemented | Emits a recursive command tree with `cppx.cli` command metadata, stable paths, and schema version `1`. |
| `phenotype artifact summary <bundle>` | implemented | Read-only structural summary for `snapshot.json`, `frame.bmp`, and platform runtime files. This does not replace semantic verification yet. |
| `phenotype artifact verify <bundle>` | implemented | Edge wrapper that runs the uv-managed Python verifier through `mise`, preserves direct material/debug contract options such as `--require-material-plan`, `--require-material-semantic-runtime-match`, `--require-debug-detail`, direct `--require-material-budget-bound`, `--require-material-resource-bound`, and `--require-material-quality-bound` key/value guards, plus direct budget/resource/quality coverage field and minimum-count guards, and prints compact manifest guard counts, material quality policy, material resource bounds, budget/resource/quality coverage, budget/resource/quality bound headroom, verifier material budget, and verifier failure summaries by default. `--json` keeps forwarding the raw verifier JSON report for machine consumers. |
| `phenotype artifact verify-glass-showcase` | implemented | Local-only CLI-owned glass showcase build/capture/verifier gate, including accessibility mode, manifest override, expected-platform override, direct material budget/resource/quality bound and coverage override flags, explicit bundle directory, structured JSON for build/run/verifier/artifact state, compact material budget fields, compact `material_quality_policy` limits/disabled-effect counters, compact `material_resource_bounds` plan/capture/runtime/pass safety counters, compact `verifier_manifest` runtime/budget/resource/policy bound and coverage-minimum visibility, `material_budget_coverage`, `material_resource_bound_coverage`, and `material_quality_policy_coverage` guarded/observed/required field visibility, `material_budget_bound_summary` bound headroom visibility, `material_budget_bound_results` expected/actual/margin details for utilization and execution-cost bounds, compact `material_resource_bound_summary`/`material_resource_bound_results` and `material_quality_policy_bound_summary`/`material_quality_policy_bound_results` details for resource and quality guards, grouped `verifier_bound_pressure`, and compact `verifier_failure_summary` metadata for failed parsed verifier reports. Non-JSON output includes material quality policy, material resource bounds, resource/quality/budget nearest or failed bound comparisons, grouped bound pressure, and compact verifier failure summaries instead of dumping raw parsed verifier JSON. PR CI should not run this slow native capture gate by default. Legacy `tools/verify_glass_showcase*.sh` entry points are thin compatibility wrappers that rebuild stale CLI binaries and delegate to this command. |
| `phenotype artifact verify-file-explorer` | implemented | Local-only edge wrapper around the desktop/mobile Finder-style artifact gate. Legacy `tools/verify_file_explorer_artifacts.sh` is now a thin stale-rebuild wrapper for this command. `--profile`, repeated `--view-mode`, repeated `--scenario`, and direct material budget/resource/quality bound and coverage override flags narrow the capture set and budget expectations for faster local iteration before the full gate. JSON and non-JSON output expose per-case material budget summaries, compact material quality policy limits/disabled-effect counters, compact material resource bounds, compact manifest guard summaries, per-case material budget/resource/quality coverage, per-case budget/resource/quality bound headroom, compact per-bound expected/actual/margin details, grouped bound pressure, and compact verifier failure summary metadata from verifier reports. Non-JSON output includes each case's material quality policy, material resource bounds, nearest or failed bound comparison for budget/resource/quality guards, grouped bound pressure, and compact verifier failure summaries instead of dumping raw parsed verifier JSON. |
| `phenotype observe <bundle>` | implemented | C++ artifact observation envelope for LLM-actionable debugging. It parses `snapshot.json`, summarizes semantic/platform/runtime/material plan presence, material kinds/roles, fallback and backdrop capture reasons, the compact `snapshot.material.executor_budget` counters and status strings, likely layer/pass hints, frame/platform files, and optionally embeds the uv-managed verifier report plus compact `verifier.material_budget`, `verifier.material_quality_policy`, `verifier.material_resource_bounds`, `verifier.verifier_manifest`, `verifier.material_budget_coverage`, `verifier.material_resource_bound_coverage`, `verifier.material_quality_policy_coverage`, `verifier.material_budget_bound_summary`, `verifier.material_budget_bound_results`, `verifier.material_resource_bound_summary`, `verifier.material_resource_bound_results`, `verifier.material_quality_policy_bound_summary`, `verifier.material_quality_policy_bound_results`, `verifier.verifier_bound_pressure`, and `verifier.verifier_failure_summary` summaries when `--manifest`, `--verify`, a direct material bound option, or a direct material coverage option is supplied. JSON and non-JSON output both surface the material budget, quality policy, resource bounds, manifest, budget/resource/quality coverage, budget/resource/quality bound headroom, nearest or failed bound comparison, grouped bound pressure, and compact verifier failure summary. |
| `phenotype android doctor/devices/emu-start/emu-stop/build/apk/install/launch/stop/run/logs/screencap/contract/clean` | implemented | Stable CLI namespace over the existing Android edge scripts and Android build command. `--json` emits a process/script result envelope, `--serial` forwards `ANDROID_SERIAL`, and `--state-dir`/`--avd`/`--apk` keep device state explicit. |
| `phenotype package inspect <path>` | implemented | Checks `phenotype.package.toml` sections, application/debug metadata, CLI-owned debug verifier metadata, declared asset/locale/font counts, referenced `source` files, package-owned `app.icon` SVG/preload policy, Pretendard default-font policy, CJK fallback coverage, package resource directories, artifact manifest presence, pure resource-contract defaults, asset preload intent, locale fallback coverage, and per-SVG-asset parser inspections from `phenotype_svg_contract`. |
| `phenotype package list <root>` | implemented | Scans for package manifests and emits a compact resource catalog for CI and future bundling. |
| `phenotype package bundle <path> --output <dir>` | implemented | Stages manifest-declared resources into a bundle directory and writes `phenotype.bundle.json` with copied-file records, package checks, app metadata, defaults, debug manifest references, byte counts, content metadata, the pure resource contract, and SHA-256 digests. `--format macos-app` additionally creates a development `.app` layout with `Contents/MacOS`, `Contents/Resources`, `Info.plist`, `PkgInfo`, a package-root launcher, the built example executable, and a generated `.icns` app icon derived from the package-owned SVG `app.icon` through macOS `sips` and `iconutil`. |
| `phenotype package verify-bundle <dir>` | implemented | Rebuilds the copied package contract from a staged bundle or macOS `.app`, checks `phenotype.bundle.json`, recomputes SHA-256 for every declared resource, generated ICNS app icon, and generated app file, compares stored manifest records against the staged files, and reports the same package checks plus bundle integrity totals. |
| `phenotype icons check` | implemented | Runs the aggregate icon gate for catalog, source-provenance, and file-type icon contracts. JSON reports all-symbol, Lucide-backed, unique source, file-type, reference-source, Apple-asset, platform-extracted, and runtime-fetch counts plus grouped check output, making toolbar, sidebar, action, and file-type icon regressions visible from one CLI command. |
| `phenotype icons catalog` | implemented | Emits the pure `phenotype.icon_catalog` contract for the built-in macOS-style SVG symbol set, including Apple HIG / macOS Finder / SF Symbols semantic reference policy, owned/permissive asset policy, explicit reference-source URLs, pinned per-symbol source URLs, unique Lucide raw SVG source names, exact ISC versus Feather-derived MIT source attribution and Feather license-lineage checks, parsed-document cache policy, count checks, all/sidebar/toolbar symbol lists, Finder-style PDF/text/archive/audio/code/spreadsheet/presentation file-type symbols, per-symbol role/variant/rendering/layer metadata, regular text-aligned weight policy, explicit monochrome/hierarchical/palette/multicolor capability counts, role-aware presentation defaults, normal/hovered/pressed/selected/disabled state recipes, and Finder-style file-type tint policy. |
| `phenotype icons sources` | implemented | Emits a compact icon provenance audit for license and source-review work: the Lucide source revision, every unique pinned raw SVG source record, symbols that use each source icon, exact license URL, remaining phenotype-owned count, reference-source rows, and checks that no Apple/SF Symbols artwork was embedded, platform-extracted, or runtime-fetched. |
| `phenotype icons file-types` | implemented | Emits the Finder-style file-type icon contract used by the desktop/mobile file explorer packages. JSON lists each token, semantic reference, package asset name/source, package asset policy, pinned direct Lucide raw SVG source URL, source revision, license, copyright, and the Apple-artwork boundary, then checks that all runtime-visible file-type icons avoid Apple-owned artwork, platform extraction, and runtime network fetches. |
| `phenotype icons svg <name-or-reference>` | implemented | Emits the raw audited SVG source for one built-in icon, or a JSON envelope with the matched symbol, semantic reference, asset policy, source attribution, rendering capabilities, byte count, and source. This gives renderer, path-parser, cache, license-audit, and package-debug flows a pure icon source probe without depending on a native window or copied Apple/SF Symbols artwork. |
| `phenotype icons present <name-or-reference>` | implemented | Resolves one symbol's pure macOS/Finder-style presentation recipe for an explicit role, interaction phase, and selected/disabled state. JSON includes the semantic reference, owned/permissive asset policy, source attribution, rendering mode/capabilities, tone, visible symbol color after opacity, background color, symbol size, effective pressed size, hit target, inset, corner radius, and likely icon layer/pass so screenshots are not required to debug toolbar or sidebar glyph state. |
| `phenotype icons render <name-or-reference>` | implemented | Wraps the same pure built-in glyph and macOS/Finder-style presentation recipe into a standalone SVG hit target. The raw mode emits SVG for visual probes, while `--json` reports source bytes, viewBox, role/phase state, output path receipt, visible color, background chrome, and the `standalone_svg_wrapper` pass. |
| `phenotype svg inspect <path>` | implemented | Reads a package or local SVG file at the CLI edge, then parses it through `phenotype_svg_contract` so Linux CI and native developers can see the supported SVG subset, source byte count, viewBox, shape count, unsupported command count, diagnostics, paintability, and renderer-facing `Painter` path without launching a backend. |
| `phenotype theme contract` | implemented | Emits the pure `phenotype_theme_contract` baseline for the default Apple-like glass theme, including Pretendard typography, Liquid Glass usage boundary, macOS/Finder-style iconography policy, owned/permissive SVG asset policy, grouped-container policy, performance bounds, accessibility fallback policy, unsupported-backend degradation policy, color tokens, radii, typography, and semantic surface roles. |
| `phenotype io contract` | implemented | Emits the pure `phenotype.io` contract for typed input events, deterministic input scripts, output observation summaries, LLM-debuggable artifact descriptors, edge-effect placement, and release-adapter bypass policy. |
| `phenotype drive file-explorer` | implemented | Drives the shared sandboxed desktop/mobile file explorer model from typed CLI inputs or line-based scripts through the `phenotype_cli.file_explorer` module and emits a stable observation JSON with trace, entries including file-type symbol metadata, `entry_symbol_summary`, viewport, view mode, pure Finder chrome/grid metrics including density policy and icon-grid top inset, default glass theme metadata, capabilities, operation receipt, preview excerpt fields, localized labels, package-resource metadata, the keyboard-vs-pointer `input_model` focus contract, and optional expectation results. |
| `phenotype drive glass-showcase` | implemented | Drives the shared material probe model through the `phenotype_cli.glass_showcase` module from typed CLI inputs or line-based scripts and emits a stable observation JSON with final state, trace, public material kinds, expected material plan count, expected execution-stage budget, expected sample-tap budget, backdrop/inspector/density/viewport state, progress value, the pure `GlassProbeContract`, per-probe material kind/container/pass/kernel/luminance/fallback metadata, and optional expectation results. |
| `phenotype run <example>` | implemented | Resolves repository examples by name or path, runs `mise exec -- exon build` unless `--no-build` is supplied, executes the generated `.exon/debug/<package>` binary, passes package-root environment when a manifest exists, validates file explorer `--input`/`--script` through the shared model, and emits a stable JSON launch receipt with build/run output tails, input counts, timeout state, artifact bundle summary, optional `--observe-output` artifact observation, and explicit environment overrides. |

The compact verifier failure summary is intentionally richer than a first-error
receipt. For material gates it now includes manifest context, material budget
coverage, budget/resource/quality bound summaries, failed bound results,
tightest bound results, and `bound_pressure`, which classifies each bound group
as `fail`, `tight`, `pass`, or `unknown`, counts zero/negative margins, and
names the zero/negative margin source key/field lists with their
expected/actual/margin evidence plus the tightest bound key/field and full
tightest bound result. The CLI JSON envelopes and non-JSON gate
output also expose the grouped `verifier_bound_pressure` outside failure
summaries whenever a parsed report has bound summaries, so automation and local
triage can distinguish a hard budget failure from a passing Liquid Glass budget
that has no headroom left.

Verifier bound summaries now carry the same source evidence directly:
zero/negative margin counts, zero/negative source result arrays, and the full
tightest bound result are present in the raw verifier JSON before the CLI derives
its grouped pressure view. The CLI preserves those fields in
`material_budget_bound_summary`, `material_resource_bound_summary`, and
`material_quality_policy_bound_summary` JSON envelopes, so automation can inspect
the raw summary or the grouped pressure view without losing bound-source detail.
The raw verifier JSON and the CLI compact envelopes also include
`resource_bound_coverage` and `quality_policy_bound_coverage` summaries when manifests opt into
`require_material_resource_bound_coverage` or
`require_material_quality_policy_coverage`. Those contracts use the same
required-field and minimum-count grammar as executor budget coverage, but guard
the material plan resource and legibility policy surfaces directly. The checked-in
glass showcase, file explorer, and Android artifact manifests now opt into those
resource and quality-policy coverage contracts.

The desktop and mobile file explorer examples now include inspectable
`phenotype.package.toml` manifests, package-owned SVG app icons,
English/Korean locale files, and a Pretendard alias descriptor with CJK-capable
fallbacks. The native examples load those
manifest/locales at startup from `PHENOTYPE_FILE_EXPLORER_PACKAGE_ROOT`,
`PHENOTYPE_PACKAGE_ROOT`, or their working directory, then resolve labels and
font defaults through the same pure `ResourceCatalog` path used by
`phenotype drive file-explorer --package`.
The same shared model now exposes the focus/input abstraction used by both CLI
and native artifact debugging: `key:tab` and `shift-tab` move the focus target
with `focus_visible=true`, while `click:*` and `pointer:*` update focus without
showing the ring. This is the first product workflow where CLI input,
framework focus routing, and artifact output all report the same
keyboard-vs-pointer visibility contract.
The glass showcase now follows the same shared-model pattern through
`examples/glass_showcase_shared`. The native scene imports that module for
state, update, density, viewport, material-count, and `GlassProbeContract`
contracts, while
`phenotype drive glass-showcase` applies the same typed inputs from the CLI and
serializes the material probe contract without opening a native window. Its
expectation grammar also asserts the cheap material budget envelope through
`material-count`, `execution-stages`, and `sample-taps`, so CI can catch probe
contract drift before a slow native artifact capture. Native startup artifacts
serialize the same contract under `debug.application.glass_showcase`, letting
verifier failures point at the expected material probe rather than relying on
visual inspection alone.
Pull-request CI routes CLI and file explorer package-resource edits through a
lightweight Linux CLI/package job instead of the full root native matrix.
The PR gate only validates command metadata and resource catalogs for the slow
native artifact commands; full glass/file-explorer captures remain local gates.
Pure support packages used by the CLI, including `phenotype_io` and
`phenotype_svg_contract`, are intentionally classified into that Linux
CLI/package lane instead of the root macOS/wasm matrix. This keeps package-only
contract work cheap while still exercising the command JSON surfaces that future
automation depends on.
The executable source is being split by command ownership as those surfaces
stabilize: `phenotype_cli.common` owns shared JSON/check, text-file, and
positional/error helpers, `phenotype_cli.commands` owns the `cppx.cli` command
metadata, `phenotype_cli.command_tree` owns recursive command-tree JSON,
`phenotype_cli.doctor` owns repository health and legacy-tool migration
reporting, `phenotype_cli.artifacts` owns artifact summary, uv-managed verifier
invocation, snapshot/material observation including compact executor budget
visibility, and likely-layer suggestions,
`phenotype_cli.material_budget` owns shared verifier budget, manifest summary,
budget coverage, bound result/headroom/pressure, and compact failure
context/distribution/first-failure parsing, JSON, and human
count/status/utilization formatting for material artifact gates,
`phenotype_cli.contracts` owns the pure theme/IO contract commands,
`phenotype_cli.file_explorer` owns file explorer observation, chrome/native
window control, and drive JSON, `phenotype_cli.icons` owns icon/SVG command
routing plus built-in icon helper payloads, `phenotype_cli.icons_common` owns
shared lookup/role/color/source helpers, `phenotype_cli.icons_render` owns
standalone rendered-icon SVG wrapping, `phenotype_cli.icons_checks` owns catalog
check generation, and `phenotype_cli.icons_svg_inspect` owns SVG support and
inspection JSON.
Package work is now split by ownership instead of accumulating in one command
module: `phenotype_cli.package_inspect` owns manifest/resource catalog checks,
`phenotype_cli.package_json` owns package JSON envelopes,
`phenotype_cli.package_bundle_json` owns staged-bundle JSON and stored manifest
comparison, and `phenotype_cli.package` keeps only package command routing plus
bundle staging/macOS app edge work. The facade re-exports those submodules so
older imports keep the same surface while the implementation stays reviewable.
`phenotype_cli.app` owns the remaining example-run and Android process glue
plus top-level routing, and `main.cpp` is only a tiny entry point. New CLI work
should prefer adding a focused module over expanding either `main.cpp` or the
app dispatcher.

Android workflows now follow the same CLI-first rule. The shell scripts under
`tools/android` remain as the edge adapter implementation because they already
encode repository-local SDK/NDK/JDK discovery and because Android device access
is inherently an edge effect. New automation should call `phenotype android ...`
instead of invoking those scripts directly. The CLI intentionally exposes
bounded `android logs` output by default so CI and LLM agents can observe recent
runtime output without hanging on a live `logcat` stream. Python verifier calls
from artifact and Android contract gates are still uv-managed, but wrappers set
`UV_PROJECT_ENVIRONMENT` to `PHENOTYPE_UV_PROJECT_ENVIRONMENT` or a temporary
default so the verifier environment remains explicit and does not dirty the
repository root.

`phenotype run` is intentionally an edge adapter, not a second app runtime. It
performs path resolution, process execution, timeout enforcement, and artifact
environment injection outside the core renderer. Example code still receives
only ordinary environment variables such as `PHENOTYPE_ARTIFACT_DIR`,
`PHENOTYPE_ARTIFACT_EXIT`, `PHENOTYPE_ARTIFACT_REASON`, and app-specific
contract inputs like `PHENOTYPE_FILE_EXPLORER_VIEW`. This keeps the long-term
input/output abstraction model compatible with production builds: the CLI can
drive or observe examples, while native shells continue to translate platform
input directly into phenotype.
For file explorer examples, `phenotype run` also bridges CLI input into the
same shared abstraction used by `phenotype drive file-explorer`: repeated
`--input` values are validated by `file_explorer_shared`, one `--script` file is
parsed with the same line/diagnostic rules, and the native process receives
only `PHENOTYPE_FILE_EXPLORER_INPUTS` plus
`PHENOTYPE_FILE_EXPLORER_SCRIPT`. The desktop/mobile examples read those edge
values at startup and call the shared parser/apply functions before the first
artifact frame. This keeps deterministic GUI input replay available to CI and
LLM debugging without introducing a second native event stack. Preference
inputs (`font-family:system`, `font-scale:1.2`, `font-size:17`,
`heading-font-size:22`, `small-font-size:13`, `line-height:1.45`,
`system-scroll-metrics:app`, `scroll-speed:1.4`,
`horizontal-scroll-speed:2`) use this
same path and are synced back into the native `phenotype::Theme` before the
first frame, so OS-derived settings and user overrides share one contract. The
observed OS snapshot includes font-family source, text scale, font-weight
adjustment, scroll factors, scrollbar/touch metrics, and accent capture; the
default package family remains Pretendard unless the input layer explicitly opts
into the OS family.
`--observe-output` closes that loop for native runs: it implies deterministic
artifact exit, creates an artifact directory when the caller did not supply one,
then parses the resulting `snapshot.json` through the same C++ output
observation path used by `phenotype observe`. A single CLI call can therefore
feed GUI inputs and report semantic tree, material plan, runtime, frame, and
artifact ownership facts in one JSON receipt.
The same path now owns Finder view-mode changes (`view:icon`, `view:list`,
`view:column`, and `view:gallery`), so the desktop toolbar, startup environment,
headless drive JSON, and artifact captures all observe one shared state field
instead of separate UI-only mode storage.
It also owns Finder-style key/shortcut aliases (`key:enter`,
`key:arrow-up`, `key:arrow-down`, `key:arrow-left`, `key:arrow-right`,
`key:home`, `key:end`, `key:page-up`, `key:page-down`, `key:delete`,
`key:escape`, `shortcut:find`, `shortcut:duplicate`, and
`shortcut:new-folder`). The aliases resolve through `file_explorer_shared` and
mirror the native key-command descriptors registered by the desktop example, so
CLI input replay and platform key dispatch do not fork product behavior.
Artifact capture now has a matching application-debug extension:
`debug.application.file_explorer` serializes the same shared snapshot and pure
chrome metrics plus desktop keyboard command descriptors into `snapshot.json`,
while the verifier's `require_debug_details` manifest field asserts those paths
alongside platform runtime details.

## Packaging contract

The package layer should be data-driven and cross-platform:

```text
phenotype.package.toml
assets/
locales/
fonts/
```

The manifest should describe:

- application identity, display name, bundle id, version, a package-owned
  SVG `app.icon`, and platform package types;
- assets with stable logical names, source paths, content type, optimization
  policy, preload intent, and runtime visibility;
- locales with BCP-47 tags, fallback chain, pluralization policy, and required
  key coverage;
- font families and aliases. Pretendard should be the default UI family when
  packaged, with deterministic CJK-capable fallback if a platform cannot
  register it;
- debug resources such as artifact manifests, probe scenes, expected pixel
  regions, and verifier schemas.

Native platforms may resolve assets from bundle files, Android asset managers,
or future web resources, but the core app should ask for logical resources
through an immutable `ResourceCatalog` snapshot. Missing assets and locale keys
must become structured CLI diagnostics before launch, not late renderer
surprises.

`phenotype.resources` now owns that pure snapshot shape as the internal
`packages/phenotype_resources` path package. Filesystem reads and bundle writes
remain CLI edge effects, while manifest and locale parsing are pure
text-to-value transformations. The module parses `phenotype.package.toml` text
into a `ResourceCatalog`, parses locale TOML text into `LocaleString` entries,
computes a `ResourceCatalogContract`, and validates duplicates, required
locale-key coverage, default locale/font references, artifact manifest
metadata, `app.icon` SVG/preload policy, default-font CJK fallback, and
verifier metadata without reading files. The contract records asset counts,
preload/runtime-visible intent, app-icon state, default locale/font resolution,
CJK fallback state, debug metadata presence, and per-locale fallback-chain
coverage. CLI commands receive this shared catalog before launch or future
bundling, and `package inspect --json` exposes the normalized catalog,
contract, and resource diagnostics.

`phenotype package bundle` is the first staging implementation. The default
`resources` format copies every declared package resource into an output
directory, rejects unsafe or package-escaping source paths, computes SHA-256 at
the CLI edge, and writes a machine-readable `phenotype.bundle.json` manifest.
The `macos-app` format keeps the same resource contract but places resources
under `Contents/Resources`, writes `Info.plist` and `PkgInfo`, copies the built
example executable into `Contents/MacOS`, generates an ICNS app icon from the
package-owned SVG `app.icon`, and installs a tiny launcher that sets
`PHENOTYPE_PACKAGE_ROOT` before execing the binary. This gives Finder-style
local launches a real app bundle with a Dock/Finder icon instead of a raw
executable that may open through Terminal. ICNS generation is a macOS-local
edge step implemented with `sips -s format png` plus `iconutil`; failures are
recorded as structured bundle file errors rather than hidden packaging drift.
The manifest records application identity, platform list, default locale/font,
debug probe metadata, generated app icon and app files, copied resource
destinations relative to the output root, byte counts, content metadata,
resource intent flags, SHA-256 digests, package checks, bundle integrity totals,
and structured errors.

`phenotype package verify-bundle` makes the staging output self-checking. It
detects either a flat resource bundle or a macOS `.app`, uses the copied
`phenotype.package.toml` as the bundle-local source of truth, checks that
`phenotype.bundle.json` exists, recomputes SHA-256 for every declared asset,
locale, font, debug artifact manifest, generated ICNS app icon, generated app
metadata file, launcher, and bundled executable, and compares stored manifest
schema, command, file count, total bytes, relative destinations, and digests
against the staged files.
This mirrors Dioxus-style asset collection while keeping the filesystem,
process, and checksum work in the CLI edge adapter rather than in
`phenotype.resources`.

`phenotype run <example>` is the first runtime injection point. It keeps process
launching at the CLI edge, but if the selected example contains
`phenotype.package.toml`, the child process receives `PHENOTYPE_PACKAGE_ROOT`.
File explorer examples also receive `PHENOTYPE_FILE_EXPLORER_PACKAGE_ROOT`.
The examples read package files at startup, call the pure manifest/locale
parsers, and fall back to the built-in catalog when files are unavailable.

## Diagnostic migration

Python remains acceptable while it is managed through `mise` and `uv`, but it
should become an implementation detail during migration:

1. Keep `tools/verify_artifact_bundle.py` as the reference verifier until the
   CLI emits byte-for-byte equivalent failure shapes for representative
   fixtures.
2. Keep `phenotype artifact verify --json` as the raw verifier report for
   machine consumers, while the default `phenotype artifact verify` output
   stays compact enough for local debugging and material-budget triage. It is
   currently a uv-managed CLI edge wrapper; the long-term state is native C++
   verification so CI does not depend on Python for core artifact contracts.
3. Convert shell scripts into thin compatibility wrappers that delegate to the
   matching CLI command. The glass showcase and file-explorer wrappers already
   delegate to the CLI.
4. Use `phenotype doctor --json` to inspect the migration matrix before
   deleting wrappers. It reports each legacy path, replacement command, edge
   boundary, and removal policy so CI can distinguish thin compatibility
   wrappers from Android edge implementation scripts.
5. Delete wrappers only after CI, docs, and local developer workflows use the
   CLI command directly.

## Input and output observation

The CLI-driven GUI path should be deterministic enough to debug native bugs
without a visible window:

- Input scripts are JSONL frames with a timestamp or tick, viewport state,
  capability snapshot, pointer/key/text/scroll events, and optional seeded
  image/resource responses.
- The current file explorer and glass showcase drivers accept a smaller
  line-based script format for model-level input. Each non-empty, non-comment
  line uses the same `kind[:value]` grammar as `--input`; this keeps CI smoke
  and local repros readable until the full JSONL native-event injector exists.
- The shell translates AppKit, Win32, Android, WASI, and CLI events into the
  same neutral event types before calling app update code.
- `phenotype.io` is the first shared pure contract for that boundary. It pins
  event kinds, replayability rules, output observation kinds, artifact bundle
  predicates, and edge-effect policy strings that CLI and native adapters can
  validate without platform APIs.
- Output observations include command-buffer summaries, semantic material
  nodes, resolved `MaterialPlan` records, runtime/executor summaries, pixel
  sample regions, fallback reasons, and likely pass/layer suggestions.
- A failing observation should report the exact JSON path, expected value,
  actual value, region summary, and suggested owner layer without requiring a
  human screenshot comparison.

`phenotype io contract --json` emits this contract directly. Use it as a cheap
CI and local sanity check when changing input translation, artifact observation,
or release-build bypass behavior.

`phenotype drive file-explorer` is the first concrete runtime-drive command.
It intentionally starts with the example's shared model rather than a hidden
native event injector: `ExplorerInput` is the neutral input value, the sandboxed
file model applies the same operations used by the desktop and mobile UIs, and
the CLI serializes a final `Snapshot` plus per-input `ExplorerInputTrace`
records. The command also accepts pure `ExplorerExpectation` contracts such as
`location:Trash`, `entry:CI Note copy.txt`,
`missing-entry:CI Note.txt`, and `operation:file_delete:ok`; failures make the
command exit non-zero and report expected versus actual state in JSON. This
gives CI and future agents a cheap way to verify file view, read, create,
duplicate, delete, sort, localized package labels, viewport-derived Finder
chrome/grid metrics, and scenario behavior before launching slow native
artifact captures.

`phenotype drive glass-showcase` is the matching material-probe drive command.
It starts with the example's shared pure state machine instead of native event
injection: `GlassInput` describes backdrop, inspector, density, note, viewport,
and reset operations; the shared update function is the same one imported by
the native scene; and the CLI serializes a final `State`, per-input
`GlassInputTrace`, public material kinds, expected material plan count, and
`GlassExpectation` results. This is intentionally cheaper than the local
macOS artifact gate, but it checks the probe scene's semantic input contract
before any renderer/backend effects are involved.

## Performance and release posture

The CLI should not make production rendering slower:

- Keep resource catalogs, capability snapshots, package descriptors, and input
  scripts immutable after construction.
- Do not add hidden global caches to core planning, layout, material, or
  command parsing. Edge caches must be keyed by explicit descriptors.
- Avoid frame-to-frame allocation churn in production adapters; the CLI can
  allocate while reading scripts or writing artifacts because that is an edge
  workflow.
- Keep debug artifact writes opt-in. Release app bundles must not include
  verifier scripts, JSON fixtures, or driver endpoints unless explicitly
  requested by the package manifest.

## Milestones

1. Document the CLI and packaging contract, then keep the existing Python and
   shell tools green. Initial state is complete.
2. Add a `phenotype_cli` package that uses `cppx.cli` and exposes `help`,
   command metadata, `doctor`, read-only artifact summary, and verifier wrapper
   commands. Initial state is complete.
3. Move artifact verification into a reusable C++ verifier surface and add
   Python fixture parity tests until the old verifier can be retired. Initial
   artifact observation is now in C++ through `phenotype observe <bundle>`;
   assertion-heavy pixel/schema verification still remains in the uv-managed
   Python verifier until C++ parity is complete.
4. Add package manifest parsing for assets, locales, fonts, and debug
   resources. Start with inspect-only validation before producing bundles.
   Initial parsing now builds the shared pure `ResourceCatalog`; bundle
   staging is implemented through `phenotype package bundle`, and
   `phenotype run` now injects package roots for examples that can consume
   runtime resources. Platform-native installers still need to adopt the staged
   bundle format.
5. Add deterministic CLI input scripts and output observations for
   `glass_showcase` and `file_explorer_desktop`. Initial
   `file_explorer_desktop`/mobile model driving is complete through
   `phenotype drive file-explorer`; glass showcase model driving is complete
   through `phenotype drive glass-showcase`; artifact output observation is
   complete through `phenotype observe <bundle>`. Native command-buffer
   observation remains.
6. Replace CI and docs references to shell/Python tools with CLI commands.
   PR CLI/package checks now parse JSON through `uv run --frozen python`
   instead of direct `python3`, and staged package bundles are verified through
   `phenotype package verify-bundle`. Glass showcase local docs now prefer
   `phenotype artifact verify-glass-showcase`.
7. Remove compatibility wrappers once the CLI covers Android, artifact, and
   package workflows.

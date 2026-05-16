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
| `phenotype artifact verify-glass-showcase` | Build or locate the glass showcase artifact, then run the contract gate. | `tools/verify_glass_showcase_artifact.sh` |
| `phenotype artifact verify-file-explorer` | Validate desktop/mobile file explorer artifact contracts. | `tools/verify_file_explorer_artifacts.sh` |
| `phenotype android doctor/run/install/logs/screencap/contract` | Keep Android workflows under one command namespace. | `tools/android/*.sh` |
| `phenotype package inspect` | Validate package manifests, assets, locales, fonts, and platform bundle metadata. | new |
| `phenotype package bundle` | Produce macOS `.app`/`.dmg`, Windows bundle metadata, Android package inputs, and debug-resource inventories. | new |
| `phenotype drive <script>` | Feed deterministic CLI input frames into the same input abstraction used by native shells. | new |
| `phenotype observe <artifact-or-run>` | Emit semantic tree, material plans, runtime summaries, pixel-region summaries, and likely failing pass/layer. | new |

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

Current commands:

| Command | Status | Notes |
|---|---|---|
| `phenotype doctor` | implemented | Read-only repository checks for `mise.toml`, verifier tools, Android contract script, CLI roadmap, and file explorer shared package presence. |
| `phenotype commands --json` | implemented | Emits a recursive command tree with `cppx.cli` command metadata, stable paths, and schema version `1`. |
| `phenotype artifact summary <bundle>` | implemented | Read-only structural summary for `snapshot.json`, `frame.bmp`, and platform runtime files. This does not replace semantic verification yet. |
| `phenotype artifact verify <bundle>` | implemented | Edge wrapper that runs the uv-managed Python verifier through `mise` and forwards the verifier JSON report. |
| `phenotype artifact verify-glass-showcase` | implemented | Local-only edge wrapper around the glass showcase build/capture/verifier gate, including accessibility mode. PR CI should not run this slow native capture gate by default. |
| `phenotype artifact verify-file-explorer` | implemented | Local-only edge wrapper around the desktop/mobile Finder-style artifact gate. This keeps the future replacement command shape visible while shell compatibility remains. |
| `phenotype package inspect <path>` | implemented | Checks `phenotype.package.toml` sections, application/debug metadata, declared asset/locale/font counts, referenced `source` files, Pretendard default-font policy, package resource directories, and artifact manifest presence. |
| `phenotype package list <root>` | implemented | Scans for package manifests and emits a compact resource catalog for CI and future bundling. |

The desktop and mobile file explorer examples now include inspectable
`phenotype.package.toml` manifests, textual asset placeholders, English/Korean
locale files, and a Pretendard alias descriptor. These resources are package
contract fixtures only; the runtime examples still use the existing hard-coded
view text until the resource catalog layer lands.
Pull-request CI routes CLI and file explorer package-resource edits through a
lightweight Linux CLI/package job instead of the full root native matrix.
The PR gate only validates command metadata and resource catalogs for the slow
native artifact commands; full glass/file-explorer captures remain local gates.

## Packaging contract

The package layer should be data-driven and cross-platform:

```text
phenotype.package.toml
assets/
locales/
fonts/
```

The manifest should describe:

- application identity, display name, bundle id, version, icon, and platform
  package types;
- assets with stable logical names, source paths, content type, optimization
  policy, preload intent, and runtime visibility;
- locales with BCP-47 tags, fallback chain, pluralization policy, and required
  key coverage;
- font families and aliases. Pretendard should be the default UI family when
  packaged, with deterministic fallback if a platform cannot register it;
- debug resources such as artifact manifests, probe scenes, expected pixel
  regions, and verifier schemas.

Native platforms may resolve assets from bundle files, Android asset managers,
or future web resources, but the core app should ask for logical resources
through an immutable `ResourceCatalog` snapshot. Missing assets and locale keys
must become structured CLI diagnostics before launch, not late renderer
surprises.

`phenotype.resources` now owns that pure snapshot shape. It intentionally starts
below the CLI: package parsers and bundle builders remain edge adapters, while
core code receives a `ResourceCatalog` with application metadata, logical
assets, locale tables, font descriptors, and debug-resource descriptors. The
module validates duplicates, required locale-key coverage, default locale/font
references, artifact manifest metadata, and verifier metadata without reading
files. CLI commands should gradually switch their hand-written manifest checks
to this shared value layer once the package parser is moved out of
`tools/phenotype_cli/src/main.cpp`.

## Diagnostic migration

Python remains acceptable while it is managed through `mise` and `uv`, but it
should become an implementation detail during migration:

1. Keep `tools/verify_artifact_bundle.py` as the reference verifier until the
   CLI emits byte-for-byte equivalent failure shapes for representative
   fixtures.
2. Add `phenotype artifact verify --json` backed by a C++ verifier library or a
   temporary CLI edge wrapper. The long-term state is native C++ verification
   so CI does not depend on Python for core artifact contracts.
3. Convert shell scripts into thin compatibility wrappers that print the
   matching CLI command and delegate to it.
4. Delete wrappers only after CI, docs, and local developer workflows use the
   CLI command directly.

## Input and output observation

The CLI-driven GUI path should be deterministic enough to debug native bugs
without a visible window:

- Input scripts are JSONL frames with a timestamp or tick, viewport state,
  capability snapshot, pointer/key/text/scroll events, and optional seeded
  image/resource responses.
- The shell translates AppKit, Win32, Android, WASI, and CLI events into the
  same neutral event types before calling app update code.
- Output observations include command-buffer summaries, semantic material
  nodes, resolved `MaterialPlan` records, runtime/executor summaries, pixel
  sample regions, fallback reasons, and likely pass/layer suggestions.
- A failing observation should report the exact JSON path, expected value,
  actual value, region summary, and suggested owner layer without requiring a
  human screenshot comparison.

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
   Python fixture parity tests until the old verifier can be retired.
4. Add package manifest parsing for assets, locales, fonts, and debug
   resources. Start with inspect-only validation before producing bundles.
   The pure `phenotype.resources` catalog layer is in place; the parser/bundler
   edge still needs to adopt it.
5. Add deterministic CLI input scripts and output observations for
   `glass_showcase` and `file_explorer_desktop`.
6. Replace CI and docs references to shell/Python tools with CLI commands.
7. Remove compatibility wrappers once the CLI covers Android, artifact, and
   package workflows.

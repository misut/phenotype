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
- `phenotype package inspect <path>` checks the proposed package manifest,
  declared resource counts, referenced `source` files, Pretendard default-font
  policy, asset layout, locale layout, and font layout.
- `phenotype commands --json` emits machine-readable command metadata from
  `cppx.cli`.

The CLI keeps process execution, artifact writes, device access, and renderer
control outside the first pass. Future commands can call those edge adapters
explicitly while preserving the input and output abstraction model documented
in `docs/PHENOTYPE_CLI_ROADMAP.md`.

# phenotype

`phenotype` is a local development helper for the WASI documentation app.
It wraps the common loop into one native C++23 executable:

1. Build `docs` with `exon build --target wasm32-wasi`.
2. Stage `docs/index.html`, `shim/phenotype.js`, and the WASI artifact as
   `docs.wasm`.
3. Serve the staged directory with `cppx.http.server`.
4. Optionally smoke-test the served HTML, JavaScript, and WebAssembly assets.

## Build

```sh
mise exec -- exon build
```

## Commands

```sh
.exon/debug/phenotype build
.exon/debug/phenotype test
.exon/debug/phenotype serve --open
```

The default staged directory is:

```text
<repo>/.phenotype/site/wasm32-wasi/debug
```

Use `--release` to stage a release build, `--out-dir <dir>` for a custom
preview directory, and `--no-build` when a fresh `docs` artifact already exists.

By default the CLI invokes `mise exec -- exon ...` from the repository's
`docs` directory. Use `--no-mise --exon <program>` in CI or other environments
where `exon` is already on `PATH`.

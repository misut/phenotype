# Unified Debug Workflow

phenotype now exposes one shared debug plane across native and WASI targets.
Platform adapters extend the same common model instead of inventing separate
debug outputs per backend.

## Common model

Every diagnostics snapshot keeps the same top-level `debug` object:

- `platform_capabilities`
- `input_debug`
- `semantic_tree`
- `platform_runtime`

`platform_capabilities` is explicit. Unsupported features are reported as
`false`, not omitted. The shared fields currently cover:

- `read_only`
- `snapshot_json`
- `capture_frame_rgba`
- `write_artifact_bundle`
- `semantic_tree`
- `input_debug`
- `platform_runtime`
- `frame_image`
- `platform_diagnostics`

The common snapshot schema remains the source of truth for all platforms:

- `input_debug` reports the last input event, caret geometry, and composition state.
- `semantic_tree` serializes the same accessibility-oriented semantic tree on every target.
- `platform_runtime` always includes the shared viewport/scroll/focus state plus a
  platform-specific `details` object.

`phenotype_diag_export()` remains the WASI export surface for the snapshot JSON.

## Artifact bundles

`write_artifact_bundle(directory, reason)` uses one shared bundle layout:

```text
<directory>/
  snapshot.json
  frame.bmp                      # optional
  platform/
    <platform>-runtime.json      # optional
```

Rules:

- `snapshot.json` always uses the common snapshot schema.
- `frame.bmp` is written only when `capture_frame_rgba` succeeds.
- `platform/<platform>-runtime.json` mirrors `platform_runtime.details` and adds
  `artifact_reason` when a reason is supplied.

## macOS extensions

The macOS adapter keeps the shared top-level schema and extends
`platform_runtime.details` with:

- `renderer`
  - initialization state
  - drawable size
  - last rendered size
  - last-frame/readback availability
- `images`
  - pending/completed queue counts
  - worker state
  - remote image entries with `url`, `state`, and `failure_reason`
- `text_input`
  - system caret state
  - composition state
  - scroll-tracking visibility context

Frame capture uses the Metal renderer path:

- `CAMetalLayer.framebufferOnly = false`
- the rendered drawable is copied into a debug capture texture
- `capture_frame_rgba()` blits that texture into a CPU-readable buffer and
  converts BGRA to RGBA

## WASI behavior

WASI exports the same debug snapshot schema through `phenotype_diag_export()`.
Its capabilities are explicit:

- `platform = "wasi"`
- `snapshot_json = true`
- `write_artifact_bundle = true`
- `capture_frame_rgba = false`
- `frame_image = false`
- `platform_diagnostics = false`

WASI-specific runtime details currently report:

- `host_model = "wasi"`
- `frame_capture_supported = false`
- `artifact_bundle_support = "snapshot-only"`

When the host provides a writable preopened directory, WASI artifact bundles
write:

- `snapshot.json`
- `platform/wasi-runtime.json`

No frame image is produced on WASI today.

The default `exon test --target wasm32-wasi` runner does not currently preopen
a writable directory for filesystem assertions, so the bundle test degrades to a
skip in that environment. A direct `wasmtime --dir=.` invocation exercises the
full bundle-writing path.

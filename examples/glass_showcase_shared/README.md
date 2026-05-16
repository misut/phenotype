# Shared Glass Showcase Model

This package exports the `glass_showcase_shared` C++ module used by the
material-focused glass showcase example and the phenotype CLI.

The module owns the deterministic app state, typed messages, line-oriented
drive input grammar, final-state expectations, and material-plan count contract
for `examples/glass_showcase`. Keeping the state machine in a separate exon
library makes the native example a renderer shell: AppKit, Win32, artifact
capture, and process execution stay at the edge, while material showcase input
planning remains a pure value transformation.

`phenotype drive glass-showcase` consumes the same input grammar without
opening a native window. It reports the final state, per-input trace,
public material kinds, expected material plan count, backdrop mode, inspector
state, density, viewport, progress value, and expectation results. This gives
CI and future LLM agents a cheap way to validate the probe scene before running
slow local native artifact capture.

## Run

```sh
cd examples/glass_showcase_shared
mise exec -- exon test
```

```sh
cd tools/phenotype_cli
mise exec -- exon build
.exon/debug/phenotype_cli drive glass-showcase --json \
  --script ../../examples/glass_showcase/glass_showcase.drive \
  --expect backdrop:high \
  --expect density:dense \
  --expect material-count:7
```

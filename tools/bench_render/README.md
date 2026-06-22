# bench_render

Host-only micro-benchmark for the phenotype layout/scene pipeline. The
MEASURE-FIRST baseline for Phase 5 perf work.

## Run

```sh
cd tools/bench_render
exon build
./.exon/debug/bench-render          # prints JSON to stdout
```

To refresh the committed baseline:

```sh
./.exon/debug/bench-render > ../../docs/bench_baseline.json
```

## What it measures

Each scenario builds a synthetic `ui::View` tree and runs the real
`phenotype::layout::LayoutScene` for 240 frames, reporting:

- `emitted` — draw-command counts the scene produces (panels / buttons / texts).
- `truncated` — whether a per-kind cap (16 panels, 128 buttons, 128 texts)
  clipped the scene. This is the silent truncation that **slice 10**
  (storage buffers + instancing) removes.
- `layout_us` — `LayoutScene` wall-clock per frame (mean / median / p95).

Scenarios:

- `uniform_static` — 1000 text leaves in a scroll, identical every frame.
- `list_churn` — 1000 leaves; ~5% get new text content per frame.
- `scroll_only` — 1000 leaves, tree unchanged, scroll offset advances.
- `flat_dense` — 1000 short leaves with **no scroll viewport**, so >128 are
  visible at once and the text cap truncates (`truncated: true`).

## Reading the numbers

- **`emitted` / `truncated` are deterministic** — they are the load-bearing
  signal. The scrolled scenarios emit only the visible window (~36 leaves)
  because the scroll pass culls to the viewport; `flat_dense` lays the whole
  stack out and hits the 128-text cap. So the caps only bite when many items
  are visible without scroll culling.
- **`layout_us` is indicative, not absolute** — it depends on the build
  profile (the committed baseline is a `debug` build) and the machine. Compare
  deltas from a baseline captured on the *same* machine and profile, not raw
  numbers across machines.

`docs/bench_baseline.json` is the pre-Phase-5 snapshot. Re-run after slices 10
and 11 and report the deltas.

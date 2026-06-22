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
- `exceeds_old_caps` — whether the emitted counts exceed the old fixed caps
  (16 panels, 128 buttons, 128 texts). **Slice 10** replaced those caps with
  storage buffers, so the scene no longer truncates; this flag now identifies
  scenarios that *would* have been silently clipped before the change.
- `layout_us` — `LayoutScene` wall-clock per frame (mean / median / p95).

Scenarios:

- `uniform_static` — 1000 text leaves in a scroll, identical every frame.
- `list_churn` — 1000 leaves; ~5% get new text content per frame.
- `scroll_only` — 1000 leaves, tree unchanged, scroll offset advances.
- `flat_dense` — 1000 short leaves with **no scroll viewport**, so >128 are
  visible at once. Pre-slice-10 this clipped at 128 texts; now it emits all
  144 visible leaves (`exceeds_old_caps: true`).

## Reading the numbers

- **`emitted` / `exceeds_old_caps` are deterministic** — they are the
  load-bearing signal. The scrolled scenarios emit only the visible window
  (~36 leaves) because the scroll pass culls to the viewport; `flat_dense`
  lays the whole stack out and emits all 144 visible leaves (which the old
  128-text cap would have clipped). So the caps only ever bit when many items
  were visible without scroll culling.
- **`layout_us` is indicative, not absolute** — it depends on the build
  profile (the committed baseline is a `debug` build) and the machine. Compare
  deltas from a baseline captured on the *same* machine and profile, not raw
  numbers across machines.

`docs/bench_baseline.json` is the current snapshot (post storage-buffers +
instancing). `layout_us` is unchanged from the pre-Phase-5 baseline because
those slices reworked the GPU renderer, not the CPU `LayoutScene` pass; the
visible delta is `flat_dense` now emitting all 144 leaves instead of clipping
at 128. Re-run after slice 11 (damage / partial repaint) and report the deltas.

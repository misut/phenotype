import phenotype;
import std;

namespace ui = phenotype::ui;

namespace {

bool Approx(float lhs, float rhs) noexcept { return std::abs(lhs - rhs) < 0.001f; }

// Drive one animate_float call across a rebuild at clock time `now`, returning
// the sampled value. Each frame opens a pass (BeginFrame), animates, and prunes;
// the call site is fixed so the same cell is reused frame to frame.
float Frame(ui::Runtime &runtime, float target, double now, float duration_ms = 100.0f) {
  runtime.BeginFrame();
  ui::Context ctx{runtime, now};
  float value = ctx.animate_float(target, duration_ms);
  runtime.Prune();
  return value;
}

// Same, for animate_color — a single fixed call site so the per-channel cells
// are reused across frames (call-site identity, like a real body()).
ui::Color ColorFrame(ui::Runtime &runtime, ui::Color target, double now) {
  runtime.BeginFrame();
  ui::Context ctx{runtime, now};
  ui::Color value = ctx.animate_color(target, 100.0f);
  runtime.Prune();
  return value;
}

} // namespace

// Covers the animation primitives: closed-form interpolation, frame-rate
// independence, mid-flight retarget continuity, the needs-tick gate, and the
// generation GC of settled/abandoned cells.
int main() {
  // --- First touch returns the target and seeds a settled cell -------------
  {
    ui::Runtime runtime;
    float v = Frame(runtime, 1.0f, 0.0);
    if (!Approx(v, 1.0f) || runtime.anim_count() != 1) {
      return 1;
    }
    // Already at target: nothing to tick.
    if (runtime.needs_tick()) {
      return 2;
    }
  }

  // --- Retarget interpolates over the duration and reaches the target ------
  {
    ui::Runtime runtime;
    Frame(runtime, 0.0f, 0.0);            // seed at 0
    float mid = Frame(runtime, 1.0f, 0.0); // retarget to 1 at t=0
    // At the retarget frame the value is still the old value (0).
    if (!Approx(mid, 0.0f)) {
      return 3;
    }
    // Halfway through the 100ms duration -> 0.5 (linear), still animating.
    float half = Frame(runtime, 1.0f, 0.05);
    if (!Approx(half, 0.5f) || !runtime.needs_tick()) {
      return 4;
    }
    // At the end -> exactly 1.0, settled (no tick).
    float end = Frame(runtime, 1.0f, 0.1);
    if (!Approx(end, 1.0f) || runtime.needs_tick()) {
      return 5;
    }
    // Past the end stays clamped at the target.
    float after = Frame(runtime, 1.0f, 0.2);
    if (!Approx(after, 1.0f)) {
      return 6;
    }
  }

  // --- Frame-rate independence: the value depends on clock time, not the ---
  //     number of frames sampled.
  {
    ui::Runtime coarse;
    Frame(coarse, 0.0f, 0.0);
    Frame(coarse, 1.0f, 0.0);           // retarget
    float coarse_v = Frame(coarse, 1.0f, 0.05); // one jump to t=0.05

    ui::Runtime fine;
    Frame(fine, 0.0f, 0.0);
    Frame(fine, 1.0f, 0.0);             // retarget
    for (int step = 1; step <= 5; ++step) {
      Frame(fine, 1.0f, 0.01 * step);   // five small steps to t=0.05
    }
    float fine_v = Frame(fine, 1.0f, 0.05);

    if (!Approx(coarse_v, fine_v)) {
      return 7;
    }
  }

  // --- Mid-flight retarget is continuous (rebases from the current value) --
  {
    ui::Runtime runtime;
    Frame(runtime, 0.0f, 0.0);
    Frame(runtime, 1.0f, 0.0);          // animate 0 -> 1 over 100ms
    float at_half = Frame(runtime, 1.0f, 0.05); // ~0.5
    if (!Approx(at_half, 0.5f)) {
      return 8;
    }
    // Retarget to 0 at t=0.05: the new interval must start from ~0.5, not jump.
    float retarget = Frame(runtime, 0.0f, 0.05);
    if (!Approx(retarget, 0.5f)) {
      return 9;
    }
    // Halfway through the new 100ms interval (t=0.10) -> ~0.25.
    float quarter = Frame(runtime, 0.0f, 0.10);
    if (!Approx(quarter, 0.25f)) {
      return 10;
    }
  }

  // --- An abandoned animation cell is pruned when no longer queried --------
  {
    ui::Runtime runtime;
    Frame(runtime, 1.0f, 0.0);
    if (runtime.anim_count() != 1) {
      return 11;
    }
    // A frame that does not animate anything drops the untouched cell.
    runtime.BeginFrame();
    ui::Context ctx{runtime, 1.0};
    (void)ctx;
    runtime.Prune();
    if (runtime.anim_count() != 0) {
      return 12;
    }
  }

  // --- animate_color interpolates every channel together -------------------
  {
    ui::Runtime runtime;
    ColorFrame(runtime, ui::Color{0.0f, 0.0f, 0.0f, 1.0f}, 0.0); // seed black
    ColorFrame(runtime, ui::Color{1.0f, 1.0f, 1.0f, 1.0f}, 0.0); // retarget white
    // Halfway -> mid grey on rgb.
    ui::Color c = ColorFrame(runtime, ui::Color{1.0f, 1.0f, 1.0f, 1.0f}, 0.05);
    if (!Approx(c.red, 0.5f) || !Approx(c.green, 0.5f) || !Approx(c.blue, 0.5f)) {
      return 13;
    }
    // Four channels -> four animation cells.
    if (runtime.anim_count() != 4) {
      return 14;
    }
  }

  return 0;
}

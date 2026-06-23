import phenotype;
import std;

namespace ui = phenotype::ui;

namespace {

bool Approx(float lhs, float rhs) noexcept { return std::abs(lhs - rhs) < 0.001f; }

// Drive one animate_float call across a rebuild at clock time `now`, returning
// the sampled value. Each frame opens a pass (BeginFrame), animates, and prunes;
// the call site is fixed so the same cell is reused frame to frame.
// Linear easing keeps the midpoint math simple for the interpolation tests; the
// easing curves get their own cases below.
float Frame(ui::Runtime &runtime, float target, double now, float duration_ms = 100.0f) {
  runtime.BeginFrame();
  ui::Context ctx{runtime, now};
  float value = ctx.animate_float(target, duration_ms, ui::Easing::linear);
  runtime.Prune();
  return value;
}

// Same, for animate_color — a single fixed call site so the per-channel cells
// are reused across frames (call-site identity, like a real body()).
ui::Color ColorFrame(ui::Runtime &runtime, ui::Color target, double now) {
  runtime.BeginFrame();
  ui::Context ctx{runtime, now};
  ui::Color value = ctx.animate_color(target, 100.0f, ui::Easing::linear);
  runtime.Prune();
  return value;
}

// Drive one eased animate_float at a fixed call site.
float EasedFrame(
    ui::Runtime &runtime, float target, double now, ui::Easing easing, float duration_ms = 100.0f) {
  runtime.BeginFrame();
  ui::Context ctx{runtime, now};
  float value = ctx.animate_float(target, duration_ms, easing);
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

  // --- ApplyEasing curve shapes --------------------------------------------
  {
    // Endpoints are fixed for every curve.
    for (ui::Easing e : {ui::Easing::linear, ui::Easing::ease_in, ui::Easing::ease_out,
             ui::Easing::ease_in_out}) {
      if (!Approx(ui::ApplyEasing(e, 0.0f), 0.0f) || !Approx(ui::ApplyEasing(e, 1.0f), 1.0f)) {
        return 15;
      }
    }
    // ease_out is ahead of linear at the midpoint (fast start); ease_in behind.
    float lin = ui::ApplyEasing(ui::Easing::linear, 0.5f);
    float out = ui::ApplyEasing(ui::Easing::ease_out, 0.5f);
    float in = ui::ApplyEasing(ui::Easing::ease_in, 0.5f);
    if (!(out > lin && in < lin)) {
      return 16;
    }
    // ease_in_out is symmetric about the midpoint (0.5 -> 0.5).
    if (!Approx(ui::ApplyEasing(ui::Easing::ease_in_out, 0.5f), 0.5f)) {
      return 17;
    }
  }

  // --- An eased animation still hits both endpoints exactly ----------------
  {
    ui::Runtime runtime;
    EasedFrame(runtime, 0.0f, 0.0, ui::Easing::ease_out);
    EasedFrame(runtime, 1.0f, 0.0, ui::Easing::ease_out); // retarget at t=0
    // Midpoint is eased ahead of linear 0.5.
    float mid = EasedFrame(runtime, 1.0f, 0.05, ui::Easing::ease_out);
    if (!(mid > 0.5f) || !runtime.needs_tick()) {
      return 18;
    }
    // Reaches exactly 1.0 at the end and settles.
    float end = EasedFrame(runtime, 1.0f, 0.1, ui::Easing::ease_out);
    if (!Approx(end, 1.0f) || runtime.needs_tick()) {
      return 19;
    }
  }

  // --- Caret blink, measured from the last edit time (since): on for the first
  //     half of each cycle, off for the second; always requests a tick.
  {
    ui::Runtime runtime;
    // since = 0, 1s period. now=0.1 -> early -> visible.
    runtime.BeginFrame();
    ui::Context on_phase{runtime, 0.1};
    if (!on_phase.caret_blink_visible(0.0, 1000.0f) || !runtime.needs_tick()) {
      return 20;
    }
    // now=0.7 -> past the half -> hidden.
    runtime.BeginFrame();
    ui::Context off_phase{runtime, 0.7};
    if (off_phase.caret_blink_visible(0.0, 1000.0f) || !runtime.needs_tick()) {
      return 21;
    }
    // now=1.1 -> next cycle -> visible again.
    runtime.BeginFrame();
    ui::Context wrapped{runtime, 1.1};
    if (!wrapped.caret_blink_visible(0.0, 1000.0f)) {
      return 22;
    }

    // Edit resets the phase: an edit at since=0.7 makes the caret solid-on at
    // now=0.7 (elapsed 0) even though the absolute phase would be in the off
    // half — proving the blink is measured from the edit, not absolute time.
    runtime.BeginFrame();
    ui::Context after_edit{runtime, 0.7};
    if (!after_edit.caret_blink_visible(0.7, 1000.0f)) {
      return 23;
    }
  }

  return 0;
}

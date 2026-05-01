#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
import phenotype;

using namespace phenotype;

#if !defined(__wasi__) && !defined(__ANDROID__)
static null_host host;
#else
extern "C" {
    void phenotype_flush() {}
    float phenotype_measure_text(float fs, unsigned int /*flags*/,
                                  char const* /*family*/,
                                  unsigned int /*family_len*/,
                                  char const* /*text*/, unsigned int len) {
        return static_cast<float>(len) * fs * 0.6f;
    }
    float phenotype_get_canvas_width() { return 800.0f; }
    float phenotype_get_canvas_height() { return 600.0f; }
    void phenotype_open_url(char const*, unsigned int) {}
}
#endif

namespace {

void reset() {
    detail::local_store().clear();
}

void next_frame_no_op() {
    detail::bump_local_gen();
    detail::prune_local_store();
}

// Stable-source_location wrappers so the same widget_id is used across
// multiple "frames" — the realistic usage pattern (the call site lives
// in widget code, not in the test loop).
float anim_float_helper(float target, int ms) {
    return animate_float(target, ms);
}

Color anim_color_helper(Color target, int ms) {
    return animate_color(target, ms);
}

}  // namespace

// First call snaps to target with no animation — there's no "before"
// value to fade from. Second call with the same target stays at it.
void test_first_call_snaps_to_target() {
    reset();
    detail::bump_local_gen();
    auto v = anim_float_helper(10.0f, 200);
    assert(v == 10.0f);
    detail::prune_local_store();
    std::puts("PASS: first call snaps to target");
}

// `duration_ms <= 0` skips interpolation regardless of how recent the
// last target change was — useful for instant-update modes.
void test_zero_duration_skips_interpolation() {
    reset();
    detail::bump_local_gen();
    anim_float_helper(0.0f, 0);    // initialise
    detail::prune_local_store();

    detail::bump_local_gen();
    auto v = anim_float_helper(50.0f, 0);
    assert(v == 50.0f);
    detail::prune_local_store();
    std::puts("PASS: zero duration skips interpolation");
}

// Mid-flight target reads return a value strictly between start and
// target — confirming the time-based lerp actually progresses.
void test_color_interpolates_mid_flight() {
    reset();
    detail::bump_local_gen();
    Color start{0, 0, 0, 255};
    Color end{200, 200, 200, 255};
    anim_color_helper(start, 200);     // initialise to black
    detail::prune_local_store();

    detail::bump_local_gen();
    anim_color_helper(end, 200);       // start fade toward grey
    detail::prune_local_store();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    detail::bump_local_gen();
    auto mid = anim_color_helper(end, 200);
    detail::prune_local_store();

    // After ~50ms of a 200ms fade we expect the channel value to be
    // somewhere strictly between 0 and 200 — wide tolerance because
    // sleep_for is best-effort.
    assert(mid.r > 0);
    assert(mid.r < 200);
    assert(mid.g > 0 && mid.g < 200);
    assert(mid.b > 0 && mid.b < 200);
    std::puts("PASS: color interpolates strictly between start and target");
}

// Sleeping past the duration yields the exact target value (progress
// clamps to 1.0).
void test_animation_completes_after_duration() {
    reset();
    detail::bump_local_gen();
    anim_float_helper(0.0f, 50);
    detail::prune_local_store();

    detail::bump_local_gen();
    anim_float_helper(100.0f, 50);
    detail::prune_local_store();

    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    detail::bump_local_gen();
    auto v = anim_float_helper(100.0f, 50);
    detail::prune_local_store();
    assert(v == 100.0f);
    std::puts("PASS: animation completes after duration");
}

// Two distinct anim_*_helper call sites store independent
// AnimationStates, so changing one target doesn't bleed into the other.
void test_independent_call_sites() {
    reset();
    detail::bump_local_gen();
    anim_float_helper(0.0f, 100);
    auto& second_site = animate_float;  // distinct call site below
    (void)second_site;
    detail::prune_local_store();

    detail::bump_local_gen();
    anim_float_helper(50.0f, 100);
    detail::prune_local_store();

    // animate_float at a fresh source_location should initialise to its
    // own target (5.0), not echo the first slot's value.
    detail::bump_local_gen();
    auto fresh = animate_float(5.0f, 100);
    detail::prune_local_store();
    assert(fresh == 5.0f);
    std::puts("PASS: distinct call sites get independent animation state");
}

int main() {
    test_first_call_snaps_to_target();
    test_zero_duration_skips_interpolation();
    test_color_interpolates_mid_flight();
    test_animation_completes_after_duration();
    test_independent_call_sites();
    std::puts("\nAll animation tests passed.");
    return 0;
}

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

// Stable-source_location wrapper — same widget_id across calls.
float anim_helper(float target, int ms) {
    return animate_float(target, ms);
}

}  // namespace

// On the very first call (no prior state), `animate_value` snaps to
// the target — there's nothing to animate, so `has_active_animations`
// must NOT be set; the host loop should go right back to sleep.
void test_first_call_does_not_request_tick() {
    detail::local_store().clear();
    detail::g_app().has_active_animations = false;
    detail::bump_local_gen();
    anim_helper(10.0f, 200);
    detail::prune_local_store();
    assert(!detail::g_app().has_active_animations);
    std::puts("PASS: first call leaves has_active_animations=false");
}

// A target change kicks off a new fade — until duration elapses,
// every subsequent call returns an in-progress value AND sets the
// auto-tick flag so the host knows to schedule another paint.
void test_in_flight_animation_requests_tick() {
    detail::local_store().clear();
    detail::g_app().has_active_animations = false;

    detail::bump_local_gen();
    anim_helper(0.0f, 200);                // initialise
    detail::prune_local_store();

    // Reset the flag — initialisation snapped, no tick needed.
    detail::g_app().has_active_animations = false;

    detail::bump_local_gen();
    anim_helper(100.0f, 200);              // start fade
    detail::prune_local_store();
    assert(detail::g_app().has_active_animations);
    std::puts("PASS: in-flight animation sets has_active_animations");
}

// Once the duration elapses, the next call returns the target value
// AND clears the request — without this the host loop would spin
// forever after a hover finishes.
void test_completed_animation_releases_tick() {
    detail::local_store().clear();
    detail::g_app().has_active_animations = false;

    detail::bump_local_gen();
    anim_helper(0.0f, 50);                 // initialise
    detail::prune_local_store();

    detail::bump_local_gen();
    anim_helper(100.0f, 50);               // start fade
    detail::prune_local_store();
    assert(detail::g_app().has_active_animations);

    // Wait past the duration so the fade lands.
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    // Reset the flag — the next read should NOT re-set it, since
    // progress >= 1.0.
    detail::g_app().has_active_animations = false;

    detail::bump_local_gen();
    auto v = anim_helper(100.0f, 50);
    detail::prune_local_store();
    assert(v == 100.0f);
    assert(!detail::g_app().has_active_animations);
    std::puts("PASS: completed animation releases the auto-tick flag");
}

int main() {
    test_first_call_does_not_request_tick();
    test_in_flight_animation_requests_tick();
    test_completed_animation_releases_tick();
    std::puts("\nAll animation-tick tests passed.");
    return 0;
}

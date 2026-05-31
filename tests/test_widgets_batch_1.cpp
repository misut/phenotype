#include <cassert>
#include <cmath>
#include <cstdio>
import phenotype;

using namespace phenotype;

#if !defined(__wasi__) && !defined(__ANDROID__)
static null_host host;
#define LAYOUT_NODE(h, w)              detail::layout_node(host, h, w)
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
#define LAYOUT_NODE(h, w)              detail::layout_node(h, w)
#endif

namespace {

bool nearly(float a, float b, float eps = 0.5f) {
    return std::fabs(a - b) <= eps;
}

template <typename View>
NodeHandle build(View&& view) {
    detail::bump_local_gen();
    detail::g_app().arena.reset();
    detail::g_app().callbacks.clear();
    detail::g_app().callback_roles.clear();
    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    view();
    Scope::set_current(nullptr);
    detail::prune_local_store();
    return root_h;
}

}  // namespace

// progress(0.5, 200) lays out an outer track 200x6 with a centered
// child filled to 100x6 — the visible "half-full" state most apps
// expect from a determinate progress reading.
void test_progress_half_full_layout() {
    auto root_h = build([&] {
        widget::progress(0.5f, 200.0f);
    });
    LAYOUT_NODE(root_h, 600.0f);

    auto& root = detail::node_at(root_h);
    auto& outer = detail::node_at(root.children[0]);
    assert(outer.width == 200.0f);
    assert(outer.height == 6.0f);
    assert(outer.children.size() == 1);
    auto& bar = detail::node_at(outer.children[0]);
    assert(bar.width == 100.0f);
    assert(bar.height == 6.0f);
    std::puts("PASS: progress half-full layout matches value");
}

// progress(0) renders the track only — no fill child — so apps don't
// emit a stray zero-width filled rectangle when the bar sits at 0%.
void test_progress_zero_omits_fill() {
    auto root_h = build([&] {
        widget::progress(0.0f, 200.0f);
    });
    LAYOUT_NODE(root_h, 600.0f);

    auto& root = detail::node_at(root_h);
    auto& outer = detail::node_at(root.children[0]);
    assert(outer.children.empty());
    std::puts("PASS: progress(0) omits the fill child");
}

// Out-of-range values clamp into [0, 1] rather than producing an
// over-wide bar or a negative width.
void test_progress_clamps_value() {
    auto root_h = build([&] {
        widget::progress(2.5f, 200.0f);    // way above 1
        widget::progress(-0.5f, 200.0f);   // below 0
    });
    LAYOUT_NODE(root_h, 600.0f);

    auto& root = detail::node_at(root_h);
    auto& above = detail::node_at(root.children[0]);
    auto& below = detail::node_at(root.children[1]);
    auto& bar = detail::node_at(above.children[0]);
    assert(bar.width == 200.0f);             // clamped to 1.0
    assert(below.children.empty());          // clamped to 0
    std::puts("PASS: progress value clamps to [0, 1]");
}

// switch_("on", true) puts the thumb on the right edge of the track
// and uses theme.accent for the track; switch_("off", false) flips
// both. The thumb position is driven by an animated leading spacer
// (children[0]); the thumb sits at children[1]. On the first frame
// `animate_float` snaps to its target, so the assertions below test
// the steady-state geometry without timing concerns.
void test_switch_on_state_alignment() {
    enum Msg { Toggle };
    detail::local_store().clear();
    auto root_h = build([&] {
        widget::switch_<Msg>("toggled", true, Toggle);
    });
    LAYOUT_NODE(root_h, 600.0f);

    auto& root = detail::node_at(root_h);
    auto& row = detail::node_at(root.children[0]);
    auto& track = detail::node_at(row.children[0]);
    auto& spacer = detail::node_at(track.children[0]);
    auto& thumb  = detail::node_at(track.children[1]);
    auto const& theme = current_theme();
    // Track adopts accent when on.
    assert(track.background.r == theme.accent.r);
    assert(track.background.g == theme.accent.g);
    assert(track.background.b == theme.accent.b);
    // Spacer pushed to its full 14px → thumb sits at padding 2 + 14 = 16.
    assert(nearly(spacer.width, 14.0f));
    assert(nearly(thumb.x, 16.0f));
    std::puts("PASS: switch_ on-state moves thumb to right edge");
}

void test_switch_off_state_alignment() {
    enum Msg { Toggle };
    detail::local_store().clear();
    auto root_h = build([&] {
        widget::switch_<Msg>("untoggled", false, Toggle);
    });
    LAYOUT_NODE(root_h, 600.0f);

    auto& root = detail::node_at(root_h);
    auto& row = detail::node_at(root.children[0]);
    auto& track = detail::node_at(row.children[0]);
    auto& spacer = detail::node_at(track.children[0]);
    auto& thumb  = detail::node_at(track.children[1]);
    auto const& theme = current_theme();
    // Track adopts border colour when off.
    assert(track.background.r == theme.border.r);
    // Spacer collapses to a sub-pixel positive width (we keep it >0 so
    // the row layout treats it as a fixed slot rather than an
    // unspecified flex child).
    assert(spacer.width < 1.0f);
    assert(nearly(thumb.x, 2.0f, /*eps=*/1.0f));
    std::puts("PASS: switch_ off-state pins thumb to left edge");
}

// switch_ registers a click callback (so Tab+Enter / pointer click
// can flip it) and is focusable.
void test_switch_registers_callback() {
    enum Msg { Toggle };
    bool fired = false;
    build([&] {
        widget::switch_<Msg>("a", false, Toggle);
    });
    auto& app = detail::g_app();
    assert(app.callbacks.size() == 1);
    // Invoking the callback should post a Msg, but the test isn't
    // wiring an `update` — we just confirm a callable was registered.
    (void)fired;
    std::puts("PASS: switch_ registers a click callback");
}

int main() {
    test_progress_half_full_layout();
    test_progress_zero_omits_fill();
    test_progress_clamps_value();
    test_switch_on_state_alignment();
    test_switch_off_state_alignment();
    test_switch_registers_callback();
    std::puts("\nAll widgets-batch-1 tests passed.");
    return 0;
}

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
import phenotype;

using namespace phenotype;

#if !defined(__wasi__) && !defined(__ANDROID__)
static null_host host;
#define LAYOUT_NODE(h, w)              detail::layout_node(host, h, w)
#define PAINT_NODE(h)                  detail::paint_node(host, host, h, 0, 0, 0.0f, 0.0f, 800.0f, 600.0f)
#define CMD_BUF                        host.buf()
#define CMD_LEN                        host.buf_len()
#else
extern "C" {
    extern unsigned char phenotype_cmd_buf[];
    extern unsigned int phenotype_cmd_len;
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
#define PAINT_NODE(h)                  detail::wasi_paint_node(h, 0, 0, 0.0f, 0.0f, 800.0f, 600.0f)
#define CMD_BUF                        phenotype_cmd_buf
#define CMD_LEN                        phenotype_cmd_len
#endif

namespace {

// Drives one rebuild: clears the arena and scroll_targets, runs the
// view lambda inside a fresh Scope (so framework_local generation /
// per-call-site counter mirror the runner), then runs layout and
// paint against the null_host. Returns the root NodeHandle so tests
// can inspect node fields after the cycle.
template <typename View>
NodeHandle run_frame(View&& view) {
    detail::bump_local_gen();
    detail::g_app.arena.reset();
    detail::g_app.scroll_targets.clear();
    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    view();
    Scope::set_current(nullptr);
    detail::prune_local_store();

    LAYOUT_NODE(root_h, 800.0f);
    CMD_LEN = 0;
    detail::g_app.scrollbar_animation_active = false;
    PAINT_NODE(root_h);
    return root_h;
}

bool cmd_buf_contains_opcode(Cmd opcode) {
    auto target = static_cast<unsigned int>(opcode);
    for (unsigned int i = 0; i + 4 <= CMD_LEN; i += 4) {
        unsigned int w;
        std::memcpy(&w, &CMD_BUF[i], 4);
        if (w == target) return true;
    }
    return false;
}

}  // namespace

// scroll_view with content taller than the viewport keeps node.height
// pinned to fixed_height while content_height records the natural
// total — the wheel dispatcher's clamp depends on this.
void test_scroll_view_clamps_height_to_viewport() {
    auto root_h = run_frame([&] {
        layout::scroll_view(120.0f, [&] {
            for (int i = 0; i < 6; ++i) {
                widget::text("row");
            }
        });
    });
    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto& sv = detail::node_at(root.children[0]);
    assert(sv.is_scroll_container);
    assert(sv.height == 120.0f);
    assert(sv.content_height > sv.height);
    std::puts("PASS: scroll_view layout pins height + records content_height");
}

// Painting a scroll_view registers exactly one scroll_target whose
// rect matches the viewport, content_height is plumbed through, and
// a Scissor command lands in the buffer so the renderer clips the
// overflow.
void test_scroll_view_registers_target_and_emits_scissor() {
    auto root_h = run_frame([&] {
        layout::scroll_view(80.0f, [&] {
            for (int i = 0; i < 5; ++i) widget::text("entry");
        });
    });
    (void)root_h;

    assert(detail::g_app.scroll_targets.size() == 1);
    auto const& tgt = detail::g_app.scroll_targets[0];
    assert(tgt.state != nullptr);
    assert(tgt.h == 80.0f);
    assert(tgt.content_height > tgt.h);
    assert(cmd_buf_contains_opcode(Cmd::Scissor));
    std::puts("PASS: scroll_view registers scroll_target + emits Scissor");
}

// Scroll position persists across rebuilds — framework_local lives
// outside the user `State`, so writing through the registered target
// survives a full view/layout/paint cycle and is read back by the
// next scroll_view DSL invocation at the same call site.
void test_scroll_view_state_persists_across_frames() {
    auto sv = [&] {
        layout::scroll_view(100.0f, [&] {
            for (int i = 0; i < 8; ++i) widget::text("line");
        });
    };

    auto root_h = run_frame(sv);
    assert(detail::g_app.scroll_targets.size() == 1);
    auto* state = detail::g_app.scroll_targets[0].state;
    state->offset_y = 25.0f;

    root_h = run_frame(sv);
    assert(detail::g_app.scroll_targets.size() == 1);
    assert(detail::g_app.scroll_targets[0].state == state);
    assert(state->offset_y == 25.0f);

    // Confirm the LayoutNode picked up the persisted offset, since
    // paint reads it from there rather than dereferencing the state
    // pointer mid-walk.
    auto& sv_node = detail::node_at(detail::node_at(root_h).children[0]);
    assert(sv_node.scroll_offset_y == 25.0f);
    std::puts("PASS: scroll_view offset persists across rebuilds");
}

// An out-of-range offset (e.g. content shrank between frames) clamps
// down to max_offset = max(0, content_height - height) at paint time;
// future wheel events read the corrected value rather than the stale
// over-scroll.
void test_scroll_view_paint_clamps_offset() {
    auto sv = [&] {
        layout::scroll_view(100.0f, [&] {
            for (int i = 0; i < 4; ++i) widget::text("row");
        });
    };

    run_frame(sv);
    assert(detail::g_app.scroll_targets.size() == 1);
    auto* state = detail::g_app.scroll_targets[0].state;
    float content_h = detail::g_app.scroll_targets[0].content_height;
    float max_off = content_h - 100.0f;
    if (max_off < 0.0f) max_off = 0.0f;

    state->offset_y = content_h + 1000.0f;  // wildly out of range
    run_frame(sv);
    assert(state->offset_y == max_off);

    state->offset_y = -50.0f;
    run_frame(sv);
    assert(state->offset_y == 0.0f);
    std::puts("PASS: scroll_view paint clamps offset into valid range");
}

// Hit regions inside a scrolled viewport must record positions that
// line up with where the content visually sits, otherwise clicks miss.
// `hit_test` re-adds *global* scroll on top of the recorded rect, so
// for nodes inside a scroll_view we need the emit to subtract just
// the inner offset — verified here by reading the wire-format
// HitRegion bytes back after a non-zero scroll.
void test_scroll_view_hit_region_offset_matches_scroll() {
    enum Msg { Tap };
    auto sv = [&] {
        layout::scroll_view(80.0f, [&] {
            widget::button<Msg>("hit me", Tap);
            for (int i = 0; i < 6; ++i) widget::text("filler");
        });
    };

    run_frame(sv);
    auto* state = detail::g_app.scroll_targets[0].state;
    state->offset_y = 32.0f;
    auto root_h = run_frame(sv);

    // Find the button's recorded layout y so we can compare against
    // what the cmd buffer reports.
    auto& sv_node = detail::node_at(detail::node_at(root_h).children[0]);
    auto& button_node = detail::node_at(sv_node.children[0]);
    float layout_y = sv_node.y + button_node.y;

    // First HitRegion in the buffer is the button's. Decode and check
    // that it sits at layout_y - offset (i.e. the visually-drawn y).
    bool found = false;
    for (unsigned int i = 0; i + 28 <= CMD_LEN; i += 4) {
        unsigned int op;
        std::memcpy(&op, &CMD_BUF[i], 4);
        if (op != static_cast<unsigned int>(Cmd::HitRegion)) continue;
        float hx, hy, hw, hh;
        std::memcpy(&hx, &CMD_BUF[i + 4],  4);
        std::memcpy(&hy, &CMD_BUF[i + 8],  4);
        std::memcpy(&hw, &CMD_BUF[i + 12], 4);
        std::memcpy(&hh, &CMD_BUF[i + 16], 4);
        (void)hx; (void)hw; (void)hh;
        // We only inspect the first HitRegion encountered — that's
        // the button's. Children further down emit theirs after.
        float const hit_slop_y = std::max(
            0.0f,
            (button_node.min_hit_height - button_node.height) * 0.5f);
        assert(hy == layout_y - 32.0f - hit_slop_y);
        found = true;
        break;
    }
    assert(found);
    std::puts("PASS: scroll_view HitRegion follows the scroll offset");
}

// Two scroll_views in the same view get independent ScrollStates —
// the per-scope sibling counter on framework_local disambiguates the
// two invocations even though they share a source_location.
void test_two_scroll_views_get_independent_state() {
    auto view = [&] {
        layout::scroll_view(60.0f, [&] {
            for (int i = 0; i < 4; ++i) widget::text("a");
        });
        layout::scroll_view(60.0f, [&] {
            for (int i = 0; i < 4; ++i) widget::text("b");
        });
    };

    run_frame(view);
    assert(detail::g_app.scroll_targets.size() == 2);
    auto* sa = detail::g_app.scroll_targets[0].state;
    auto* sb = detail::g_app.scroll_targets[1].state;
    assert(sa != sb);
    sa->offset_y = 10.0f;
    sb->offset_y = 20.0f;

    run_frame(view);
    assert(detail::g_app.scroll_targets.size() == 2);
    assert(detail::g_app.scroll_targets[0].state == sa);
    assert(detail::g_app.scroll_targets[1].state == sb);
    assert(sa->offset_y == 10.0f);
    assert(sb->offset_y == 20.0f);
    std::puts("PASS: two scroll_views keep independent state");
}

void test_scroll_view_always_paints_overlay_scrollbar() {
    auto previous_visibility = detail::g_app.theme.scroll_bar_visibility;
    detail::g_app.theme.scroll_bar_visibility = "always";
    run_frame([&] {
        layout::scroll_view(80.0f, [&] {
            for (int i = 0; i < 6; ++i) widget::text("entry");
        });
    });
    assert(cmd_buf_contains_opcode(Cmd::RoundRect));
    detail::g_app.theme.scroll_bar_visibility = previous_visibility;
    std::puts("PASS: always-visible scroll_view paints overlay scrollbar");
}

void test_scroll_view_hidden_suppresses_overlay_scrollbar() {
    auto previous_visibility = detail::g_app.theme.scroll_bar_visibility;
    detail::g_app.theme.scroll_bar_visibility = "hidden";
    run_frame([&] {
        layout::scroll_view(80.0f, [&] {
            for (int i = 0; i < 6; ++i) widget::text("entry");
        });
    });
    assert(!cmd_buf_contains_opcode(Cmd::RoundRect));
    detail::g_app.theme.scroll_bar_visibility = previous_visibility;
    std::puts("PASS: hidden scroll_view suppresses overlay scrollbar");
}

void test_scroll_view_auto_scrollbar_fades_after_offset_change() {
    auto previous_visibility = detail::g_app.theme.scroll_bar_visibility;
    detail::g_app.theme.scroll_bar_visibility = "auto";
    auto sv = [&] {
        layout::scroll_view(80.0f, [&] {
            for (int i = 0; i < 6; ++i) widget::text("entry");
        });
    };

    run_frame(sv);
    auto* state = detail::g_app.scroll_targets[0].state;
    state->offset_y = 0.0f;
    state->scrollbar_last_offset_y = 0.0f;
    state->scrollbar_active_since_ns = 0;
    run_frame(sv);
    assert(!detail::g_app.scrollbar_animation_active);
    state = detail::g_app.scroll_targets[0].state;
    state->offset_y = 24.0f;
    run_frame(sv);
    assert(cmd_buf_contains_opcode(Cmd::RoundRect));
    assert(detail::g_app.scrollbar_animation_active);
    detail::g_app.theme.scroll_bar_visibility = previous_visibility;
    std::puts("PASS: auto scroll_view scrollbar fades in after scrolling");
}

int main() {
    test_scroll_view_clamps_height_to_viewport();
    test_scroll_view_registers_target_and_emits_scissor();
    test_scroll_view_state_persists_across_frames();
    test_scroll_view_paint_clamps_offset();
    test_scroll_view_hit_region_offset_matches_scroll();
    test_two_scroll_views_get_independent_state();
    test_scroll_view_always_paints_overlay_scrollbar();
    test_scroll_view_hidden_suppresses_overlay_scrollbar();
    test_scroll_view_auto_scrollbar_fades_after_offset_change();
    std::puts("\nAll scroll_view tests passed.");
    return 0;
}

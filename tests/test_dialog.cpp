#include <cassert>
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

template <typename View>
NodeHandle build(View&& view) {
    detail::bump_local_gen();
    detail::g_app.arena.reset();
    detail::g_app.overlays.clear();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
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

// `dialog(builder)` registers an overlay (it doesn't attach to the
// main tree) whose subtree is a top spacer + a row containing a
// max_width-bounded card hosting the builder's content.
void test_dialog_registers_overlay_with_centered_card() {
    auto root_h = build([&] {
        widget::text("background");
        layout::dialog([&] {
            widget::text("hello world");
        });
    });
    (void)root_h;

    auto& app = detail::g_app;
    assert(app.overlays.size() == 1);
    auto overlay_h = app.overlays[0];
    auto& overlay = detail::node_at(overlay_h);
    assert(overlay.children.size() == 2);              // spacer + row

    auto& row = detail::node_at(overlay.children[1]);
    assert(row.style.flex_direction == FlexDirection::Row);
    assert(row.style.main_align == MainAxisAlignment::Center);

    auto& sized = detail::node_at(row.children[0]);
    auto& card = detail::node_at(sized.children[0]);
    auto const& theme = current_theme();
    // Card chrome — surface fill, themed border, rounded corners,
    // even inner padding on all sides.
    assert(card.background.r == theme.surface.r);
    assert(card.border_color.r == theme.border.r);
    assert(card.border_width == 1.0f);
    assert(card.style.padding[0] == theme.space_lg);
    assert(card.style.padding[1] == theme.space_lg);
    assert(card.style.padding[2] == theme.space_lg);
    assert(card.style.padding[3] == theme.space_lg);
    assert(card.children.size() == 1);                  // user content
    std::puts("PASS: dialog registers overlay + centered card chrome");
}

// Dialog laid out at viewport width 800 with max_width 360 places
// the card horizontally centred — the row's MainAxisAlignment::Center
// puts the sized_box at (800 - 360) / 2 = 220 from the left.
void test_dialog_horizontal_centering() {
    auto root_h = build([&] {
        layout::dialog([&] { widget::text("body"); }, 360.0f, 0);
    });
    (void)root_h;

    auto& app = detail::g_app;
    assert(app.overlays.size() == 1);
    auto overlay_h = app.overlays[0];
    LAYOUT_NODE(overlay_h, 800.0f);

    auto& overlay = detail::node_at(overlay_h);
    auto& row = detail::node_at(overlay.children[1]);
    auto& sized = detail::node_at(row.children[0]);
    // sized_box is the centred slot: x = (800 - 360) / 2 = 220.
    assert(sized.x == 220.0f);
    assert(sized.width == 360.0f);
    std::puts("PASS: dialog horizontally centers the card at viewport mid");
}

// Calling `dialog` twice in a single view registers two overlays —
// the framework_local store handles widget id derivation per call
// site without the overlays interfering with each other.
void test_dialog_two_invocations_are_independent() {
    build([&] {
        layout::dialog([&] { widget::text("first"); });
        layout::dialog([&] { widget::text("second"); });
    });
    assert(detail::g_app.overlays.size() == 2);
    std::puts("PASS: two dialogs in one view register two overlays");
}

int main() {
    test_dialog_registers_overlay_with_centered_card();
    test_dialog_horizontal_centering();
    test_dialog_two_invocations_are_independent();
    std::puts("\nAll dialog tests passed.");
    return 0;
}

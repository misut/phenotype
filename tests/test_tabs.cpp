#include <cassert>
#include <cstdio>
#include <vector>
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

// Each tab item lands as a child of the outer row, in order. With
// three items and `selected = 1`, the second button paints with the
// accent background and the others stay transparent.
void test_tabs_renders_one_button_per_item() {
    enum class Msg { Pick0, Pick1, Pick2 };
    auto on_select = [](std::size_t i) -> Msg {
        switch (i) { case 0: return Msg::Pick0;
                     case 1: return Msg::Pick1;
                     default: return Msg::Pick2; }
    };

    auto root_h = build([&] {
        // Use the runtime (ptr, len) ctor — MSVC rejects calling the
        // consteval string-literal ctor inside a vector initializer
        // list as "not a constant expression", even though the call
        // would in fact evaluate at compile time.
        std::vector<str> items;
        items.emplace_back("Home", 4u);
        items.emplace_back("Posts", 5u);
        items.emplace_back("About", 5u);
        widget::tabs<Msg>(items, /*selected=*/1, on_select);
    });
    LAYOUT_NODE(root_h, 600.0f);

    auto& root = detail::node_at(root_h);
    // tabs is now a pill column wrapping (a) the tab row and
    // (b) the sliding-indicator track.
    auto& pill = detail::node_at(root.children[0]);
    assert(pill.children.size() == 2);
    auto& bar = detail::node_at(pill.children[0]);
    assert(bar.children.size() == 3);

    auto const& theme = current_theme();
    auto& selected = detail::node_at(bar.children[1]);
    auto& other_a  = detail::node_at(bar.children[0]);
    auto& other_b  = detail::node_at(bar.children[2]);
    // Active tab now reads as a raised slot in `theme.surface`; the
    // accent colour lives on the indicator track below and on the
    // active label.
    assert(selected.background.r == theme.surface.r);
    assert(selected.background.g == theme.surface.g);
    assert(selected.background.b == theme.surface.b);
    assert(other_a.background.a == 0);   // transparent
    assert(other_b.background.a == 0);

    // Sliding-indicator track sits below the tab row with three
    // children: leading spacer, indicator line, trailing spacer.
    // With selected=1 of 3 the leading spacer should hold flex_grow=1
    // and the trailing spacer flex_grow=1 so the indicator centres
    // under the middle tab.
    auto& track = detail::node_at(pill.children[1]);
    assert(track.children.size() == 3);
    auto& lead = detail::node_at(track.children[0]);
    auto& line = detail::node_at(track.children[1]);
    auto& trail = detail::node_at(track.children[2]);
    assert(lead.style.flex_grow == 1.0f);
    assert(line.style.flex_grow == 1.0f);
    assert(trail.style.flex_grow == 1.0f);
    assert(line.background.r == theme.accent.r);
    std::puts("PASS: tabs renders one button per item with accent on selected");
}

// Each tab registers a click callback wired to `on_select(i)`. The
// callback list ends up in click order.
void test_tabs_registers_per_tab_callback() {
    enum class Msg { Tag0, Tag1 };
    auto on_select = [](std::size_t i) -> Msg {
        return i == 0 ? Msg::Tag0 : Msg::Tag1;
    };

    build([&] {
        std::vector<str> items;
        items.emplace_back("a", 1u);
        items.emplace_back("b", 1u);
        widget::tabs<Msg>(items, /*selected=*/0, on_select);
    });
    auto& app = detail::g_app;
    assert(app.callbacks.size() == 2);
    assert(app.callback_roles[0] == InteractionRole::Button);
    assert(app.callback_roles[1] == InteractionRole::Button);
    std::puts("PASS: tabs registers a click callback per tab");
}

// Tabs items are focusable, so Tab navigation lands on each one in
// declaration order.
void test_tabs_focusable_in_order() {
    enum class Msg { Sel };
    auto on_select = [](std::size_t) -> Msg { return Msg::Sel; };

    auto root_h = build([&] {
        std::vector<str> items;
        items.emplace_back("a", 1u);
        items.emplace_back("b", 1u);
        items.emplace_back("c", 1u);
        widget::tabs<Msg>(items, 0, on_select);
    });

    auto& app = detail::g_app;
    LAYOUT_NODE(root_h, 600.0f);
    app.focusable_ids.clear();
    detail::collect_focusable_ids(root_h);
    assert(app.focusable_ids.size() == 3);
    assert(app.focusable_ids[0] == 0u);
    assert(app.focusable_ids[1] == 1u);
    assert(app.focusable_ids[2] == 2u);
    std::puts("PASS: tabs focusables collected in document order");
}

int main() {
    test_tabs_renders_one_button_per_item();
    test_tabs_registers_per_tab_callback();
    test_tabs_focusable_in_order();
    std::puts("\nAll tabs tests passed.");
    return 0;
}

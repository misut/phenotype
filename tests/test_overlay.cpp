#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>
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

template <typename View>
NodeHandle build(View&& view) {
    detail::bump_local_gen();
    detail::g_app().arena.reset();
    detail::g_app().overlays.clear();
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

LayoutNode const* find_text(NodeHandle h, char const* text) {
    auto const& node = detail::node_at(h);
    if (node.text.compare(text) == 0)
        return &node;
    for (auto child : node.children) {
        if (auto const* found = find_text(child, text))
            return found;
    }
    return nullptr;
}

enum class DropdownMsg { Icon, List, Column };

DropdownMsg dropdown_pick(std::size_t index) {
    if (index == 1)
        return DropdownMsg::List;
    if (index == 2)
        return DropdownMsg::Column;
    return DropdownMsg::Icon;
}

}  // namespace

// Calling `layout::overlay` registers a NodeHandle on `g_app().overlays`
// and *not* on the parent root's children — the overlay subtree is a
// root-level alternate the runner walks separately.
void test_overlay_registers_off_main_tree() {
    auto root_h = build([&] {
        widget::text("main");
        layout::overlay([&] {
            widget::text("on top");
        });
    });
    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);                   // only "main"
    assert(detail::g_app().overlays.size() == 1);
    auto& overlay_root = detail::node_at(detail::g_app().overlays[0]);
    assert(overlay_root.children.size() == 1);           // "on top"
    std::puts("PASS: overlay registers as a root-level alternate");
}

// Painting walks main first, then the overlay, so the overlay's
// HitRegion lands later in the cmd buffer than any main-tree
// HitRegion. Reverse-iteration hit_test (top of paint stack wins)
// therefore returns the overlay's callback even when both overlap
// the cursor's screen position.
void test_overlay_paints_on_top_of_main_tree() {
    enum Msg { TopHit, BottomHit };
    auto root_h = build([&] {
        widget::button<Msg>("under", BottomHit);
        layout::overlay([&] {
            widget::button<Msg>("over", TopHit);
        });
    });

    LAYOUT_NODE(root_h, 800.0f);
    for (auto h : detail::g_app().overlays) LAYOUT_NODE(h, 800.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_h);
    auto main_end = CMD_LEN;
    for (auto h : detail::g_app().overlays) PAINT_NODE(h);

    // Walk HitRegions in emit order, locating the indices of the two
    // buttons. Overlay's button is registered *after* the main button
    // (callback_id > main's), so we identify by id rather than text.
    int main_callback = 0;
    int overlay_callback = 1;  // declaration order: under, then over

    bool main_seen_before_overlay = false;
    bool found_main = false;
    for (unsigned int i = 0; i + 28 <= CMD_LEN; i += 4) {
        unsigned int op;
        std::memcpy(&op, &CMD_BUF[i], 4);
        if (op != static_cast<unsigned int>(Cmd::HitRegion)) continue;
        unsigned int cb;
        std::memcpy(&cb, &CMD_BUF[i + 20], 4);
        if (cb == static_cast<unsigned int>(main_callback)) {
            found_main = true;
            assert(i < main_end);
        } else if (cb == static_cast<unsigned int>(overlay_callback)) {
            assert(found_main);
            assert(i >= main_end);
            main_seen_before_overlay = true;
            break;
        }
    }
    assert(main_seen_before_overlay);
    std::puts("PASS: overlay HitRegion comes after the main tree's");
}

// Overlays are cleared at the start of every view rebuild — so leaving
// an overlay out of one frame removes it from the cmd buffer on the
// next, no leakage from the prior frame.
void test_overlay_cleared_between_rebuilds() {
    build([&] {
        layout::overlay([&] { widget::text("transient"); });
    });
    assert(detail::g_app().overlays.size() == 1);

    build([&] {
        widget::text("only main");
    });
    assert(detail::g_app().overlays.empty());
    std::puts("PASS: overlays clear between rebuilds");
}

// Overlay buttons participate in Tab navigation by being collected
// into `g_app().focusable_ids` alongside main-tree focusables.
void test_overlay_focusables_collected() {
    enum Msg { Action };
    auto root_h = build([&] {
        widget::button<Msg>("main btn", Action);
        layout::overlay([&] {
            widget::button<Msg>("overlay btn", Action);
        });
    });

    auto& app = detail::g_app();
    LAYOUT_NODE(root_h, 800.0f);
    for (auto h : app.overlays) LAYOUT_NODE(h, 800.0f);

    app.focusable_ids.clear();
    detail::collect_focusable_ids(root_h);
    for (auto h : app.overlays) detail::collect_focusable_ids(h);

    // Two registered buttons → two focusables, with the overlay's id
    // appearing after the main one in document/declaration order.
    assert(app.focusable_ids.size() == 2);
    assert(app.focusable_ids[0] == 0u);
    assert(app.focusable_ids[1] == 1u);
    std::puts("PASS: overlay focusables collected for Tab navigation");
}

void test_glass_dropdown_button_opens_context_menu() {
    detail::msg_queue().clear();
    std::vector<str> items;
    items.emplace_back("Icon", 4u);
    items.emplace_back("List", 4u);
    items.emplace_back("Column", 6u);
    GlassDropdownStyleOptions options;
    options.menu_width = 180.0f;
    options.item_height = 32.0f;
    options.top_padding = 24;

    auto render = [&] {
        return build([&] {
            widget::glass_dropdown_button<DropdownMsg>(
                "View",
                items,
                0,
                dropdown_pick,
                options);
        });
    };

    auto root_h = render();
    assert(detail::g_app().overlays.empty());
    auto const& closed_root = detail::node_at(root_h);
    assert(closed_root.children.size() == 1);
    auto const& closed_button = detail::node_at(closed_root.children[0]);
    assert(closed_button.text.compare("View: Icon") == 0);
    assert(closed_button.material.kind == MaterialKind::Clear);
    assert(closed_button.material.role == MaterialSurfaceRole::Navigation);
    assert(detail::g_app().callbacks.size() == 1);

    detail::g_app().callbacks[closed_button.callback_id]();

    root_h = render();
    assert(detail::g_app().overlays.size() == 1);
    assert(detail::g_app().callbacks.size() == items.size() + 1);
    auto const* icon = find_text(detail::g_app().overlays[0], "Icon");
    auto const* list = find_text(detail::g_app().overlays[0], "List");
    assert(icon != nullptr);
    assert(list != nullptr);
    assert(icon->material.kind == MaterialKind::Thin);
    assert(icon->material.role == MaterialSurfaceRole::Overlay);
    assert(list->material.kind == MaterialKind::Clear);
    assert(list->material.role == MaterialSurfaceRole::Overlay);

    detail::g_app().callbacks[2]();
    auto messages = detail::drain<DropdownMsg>();
    assert(messages.size() == 1);
    assert(messages[0] == DropdownMsg::List);

    render();
    assert(detail::g_app().overlays.empty());
    std::puts("PASS: glass dropdown opens a material context menu");
}

void test_glass_dropdown_button_accepts_effect_style() {
    std::vector<str> items;
    items.emplace_back("Icon", 4u);
    items.emplace_back("List", 4u);
    auto root_h = build([&] {
        widget::glass_dropdown_button<DropdownMsg>(
            "View",
            items,
            1,
            dropdown_pick,
            layout::glass_regular(),
            GlassDropdownStyleOptions{.button_role = MaterialSurfaceRole::Toolbar});
    });

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto const& button = detail::node_at(root.children[0]);
    assert(button.text.compare("View: List") == 0);
    assert(button.material.kind == MaterialKind::Regular);
    assert(button.material.role == MaterialSurfaceRole::Toolbar);
    std::puts("PASS: glass dropdown accepts effect style");
}

int main() {
    test_overlay_registers_off_main_tree();
    test_overlay_paints_on_top_of_main_tree();
    test_overlay_cleared_between_rebuilds();
    test_overlay_focusables_collected();
    test_glass_dropdown_button_opens_context_menu();
    test_glass_dropdown_button_accepts_effect_style();
    std::puts("\nAll overlay tests passed.");
    return 0;
}

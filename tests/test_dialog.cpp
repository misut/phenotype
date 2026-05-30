#include <cassert>
#include <cstring>
#include <cstdio>
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
    detail::g_app.arena.reset();
    detail::g_app.overlays.clear();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
    detail::g_app.debug_viewport_width = 800.0f;
    detail::g_app.debug_viewport_height = 600.0f;
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
    assert(overlay.callback_id != 0xFFFFFFFFu);
    assert(!overlay.focusable);
    assert(overlay.debug_semantic_hidden);
    assert(overlay.style.fixed_height == 600.0f);

    auto& row = detail::node_at(overlay.children[1]);
    assert(row.style.flex_direction == FlexDirection::Row);
    assert(row.style.main_align == MainAxisAlignment::Center);

    auto& sized = detail::node_at(row.children[0]);
    auto& card = detail::node_at(sized.children[0]);
    auto const& theme = current_theme();
    // Dialog chrome is a first-class overlay material sheet.
    assert(card.debug_semantic_label.compare("Dialog Sheet") == 0);
    assert(card.material.kind == MaterialKind::Regular);
    assert(card.material.role == MaterialSurfaceRole::Overlay);
    assert(card.material.container.interactive);
    assert(card.background == card.material.tint);
    assert(card.border_color == card.material.border);
    assert(card.border_width == 1.0f);
    assert(card.border_radius == theme.radius_lg);
    assert(card.style.padding[0] == theme.space_lg);
    assert(card.style.padding[1] == theme.space_lg);
    assert(card.style.padding[2] == theme.space_lg);
    assert(card.style.padding[3] == theme.space_lg);
    assert(card.children.size() == 1);                  // user content
    std::puts("PASS: dialog registers overlay + centered card chrome");
}

void test_dialog_modal_capture_paints_after_main_hit_regions() {
    enum Msg { MainHit, DialogHit };
    auto root_h = build([&] {
        widget::button<Msg>("under", MainHit);
        layout::dialog([&] {
            widget::button<Msg>("modal action", DialogHit);
        }, 360.0f, 0);
    });

    auto& app = detail::g_app;
    assert(app.overlays.size() == 1);
    auto overlay_h = app.overlays[0];
    auto& overlay = detail::node_at(overlay_h);
    unsigned int const main_callback = 0u;
    unsigned int const capture_callback = overlay.callback_id;
    unsigned int const dialog_callback = 2u;
    assert(capture_callback != 0xFFFFFFFFu);
    assert(capture_callback != main_callback);
    assert(dialog_callback != capture_callback);

    LAYOUT_NODE(root_h, 800.0f);
    LAYOUT_NODE(overlay_h, 800.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_h);
    auto const main_end = CMD_LEN;
    PAINT_NODE(overlay_h);

    bool found_main = false;
    bool found_capture_after_main = false;
    bool found_dialog_after_capture = false;
    for (unsigned int i = 0; i + 28 <= CMD_LEN; i += 4) {
        unsigned int op = 0;
        std::memcpy(&op, &CMD_BUF[i], 4);
        if (op != static_cast<unsigned int>(Cmd::HitRegion))
            continue;
        unsigned int callback_id = 0;
        std::memcpy(&callback_id, &CMD_BUF[i + 20], 4);
        if (callback_id == main_callback) {
            found_main = true;
            assert(i < main_end);
        } else if (callback_id == capture_callback) {
            assert(found_main);
            assert(i >= main_end);
            found_capture_after_main = true;
        } else if (callback_id == dialog_callback) {
            assert(found_capture_after_main);
            assert(i >= main_end);
            found_dialog_after_capture = true;
            break;
        }
    }
    assert(found_capture_after_main);
    assert(found_dialog_after_capture);
    std::puts("PASS: dialog modal capture paints below modal hit regions");
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

void test_glass_overlay_helpers_register_preset_chrome() {
    build([&] {
        layout::popover_overlay([&] { widget::text("popover"); },
                                320.0f,
                                12,
                                "Quick Popover");
        layout::tooltip_overlay([&] { widget::text("tooltip"); },
                                260.0f,
                                8,
                                "Hover Tip");
        layout::context_menu_overlay([&] { widget::text("menu"); },
                                     240.0f,
                                     16,
                                     "Action Menu");
        layout::command_palette_overlay([&] { widget::text("commands"); },
                                        560.0f,
                                        20,
                                        "Command Search");
    });

    auto& overlays = detail::g_app.overlays;
    assert(overlays.size() == 4);

    auto assert_overlay_surface =
        [](NodeHandle overlay_h,
           char const* label,
           MaterialKind kind,
           bool interactive,
           float max_width,
           float top_padding) {
            auto& overlay = detail::node_at(overlay_h);
            assert(overlay.children.size() == 2);
            auto& spacer = detail::node_at(overlay.children[0]);
            assert(spacer.style.fixed_height == top_padding);
            auto& row = detail::node_at(overlay.children[1]);
            assert(row.style.flex_direction == FlexDirection::Row);
            assert(row.style.main_align == MainAxisAlignment::Center);
            auto& sized = detail::node_at(row.children[0]);
            assert(sized.style.max_width == max_width);
            auto& surface = detail::node_at(sized.children[0]);
            assert(surface.debug_semantic_label.compare(label) == 0);
            assert(surface.material.kind == kind);
            assert(surface.material.role == MaterialSurfaceRole::Overlay);
            assert(surface.material.container.interactive == interactive);
            assert(surface.style.max_width == max_width);
            assert(surface.background == surface.material.tint);
            assert(surface.border_color == surface.material.border);
        };

    assert_overlay_surface(
        overlays[0], "Quick Popover", MaterialKind::Regular,
        true, 320.0f, 12.0f);
    assert_overlay_surface(
        overlays[1], "Hover Tip", MaterialKind::Thin,
        false, 260.0f, 8.0f);
    assert_overlay_surface(
        overlays[2], "Action Menu", MaterialKind::Regular,
        true, 240.0f, 16.0f);
    assert_overlay_surface(
        overlays[3], "Command Search", MaterialKind::Thick,
        true, 560.0f, 20.0f);
    std::puts("PASS: glass overlay helpers register preset chrome");
}

int main() {
    test_dialog_registers_overlay_with_centered_card();
    test_dialog_modal_capture_paints_after_main_hit_regions();
    test_dialog_horizontal_centering();
    test_dialog_two_invocations_are_independent();
    test_glass_overlay_helpers_register_preset_chrome();
    std::puts("\nAll dialog tests passed.");
    return 0;
}

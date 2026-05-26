#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
import phenotype;
import phenotype.svg;

using namespace phenotype;

#if !defined(__wasi__) && !defined(__ANDROID__)
static null_host host;
#define RUN_APP(S, M, V, U)              run<S, M>(host, V, U)
#define LAYOUT_NODE(h, w)                detail::layout_node(host, h, w)
#define PAINT_NODE(h, ox, oy, sy, vh)    detail::paint_node(host, host, h, ox, oy, 0.0f, sy, 800.0f, vh)
#define CMD_BUF                          host.buf()
#define CMD_LEN                          host.buf_len()
#else
extern "C" {
    extern unsigned char phenotype_cmd_buf[];
    extern unsigned int phenotype_cmd_len;
    // Stubs for WASM host imports — wasmtime has no JS shim.
    void phenotype_flush() {}
    float phenotype_measure_text(float fs, unsigned int /*flags*/,
                                  char const* /*family*/, unsigned int /*family_len*/,
                                  char const* /*text*/, unsigned int len) {
        return static_cast<float>(len) * fs * 0.6f;
    }
    float phenotype_get_canvas_width() { return 800.0f; }
    float phenotype_get_canvas_height() { return 600.0f; }
    void phenotype_open_url(char const*, unsigned int) {}
}
#define RUN_APP(S, M, V, U)              run<S, M>(V, U)
#define LAYOUT_NODE(h, w)                detail::layout_node(h, w)
#define PAINT_NODE(h, ox, oy, sy, vh)    detail::wasi_paint_node(h, ox, oy, 0.0f, sy, 800.0f, vh)
#define CMD_BUF                          phenotype_cmd_buf
#define CMD_LEN                          phenotype_cmd_len
#endif

namespace button_test {
struct Click {};
using ButtonMsg = std::variant<Click>;

inline NodeHandle build_button(ButtonVariant variant, bool disabled,
                               unsigned int hovered_id = 0xFFFFFFFFu,
                               unsigned int focused_id = 0xFFFFFFFFu,
                               unsigned int pressed_id = 0xFFFFFFFFu,
                               bool focus_visible = false) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    // Wipe per-call-site animation state so the first `animate_color` /
    // `animate_float` inside `widget::button` snaps to its target
    // instead of inheriting the previous test's interpolation.
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = hovered_id;
    detail::g_app.focused_id = focused_id;
    detail::g_app.pressed_id = pressed_id;
    detail::g_app.focus_visible = focus_visible;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::button<ButtonMsg>("Click", Click{}, variant, disabled);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}

inline NodeHandle build_button_frame_preserving_animation(
        bool focus_visible,
        bool prev_focus_visible) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    detail::g_app.hovered_id = 0xFFFFFFFFu;
    detail::g_app.focused_id = 0u;
    detail::g_app.prev_focused_id = 0u;
    detail::g_app.pressed_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = focus_visible;
    detail::g_app.prev_focus_visible = prev_focus_visible;
    detail::bump_local_gen();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::button<ButtonMsg>(
        "Click",
        Click{},
        ButtonVariant::Default,
        false);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}

inline NodeHandle build_button_with_options(
        ButtonStyleOptions options,
        unsigned int hovered_id = 0xFFFFFFFFu,
        unsigned int focused_id = 0xFFFFFFFFu,
        unsigned int pressed_id = 0xFFFFFFFFu,
        bool focus_visible = false) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = hovered_id;
    detail::g_app.focused_id = focused_id;
    detail::g_app.pressed_id = pressed_id;
    detail::g_app.focus_visible = focus_visible;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::button<ButtonMsg>("Click", Click{}, options);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}

inline NodeHandle build_canvas_button_with_options(
        ButtonStyleOptions options,
        std::uint64_t paint_token = 0xCAFEu,
        unsigned int hovered_id = 0xFFFFFFFFu,
        unsigned int focused_id = 0xFFFFFFFFu,
        unsigned int pressed_id = 0xFFFFFFFFu,
        bool focus_visible = false) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = hovered_id;
    detail::g_app.focused_id = focused_id;
    detail::g_app.pressed_id = pressed_id;
    detail::g_app.focus_visible = focus_visible;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::canvas_button<ButtonMsg>(
        "Grid View",
        44.0f,
        36.0f,
        [](Painter& p) {
            p.line(12.0f, 12.0f, 32.0f, 24.0f,
                   2.0f, Color{20, 20, 20, 255});
        },
        Click{},
        options,
        paint_token);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}

inline NodeHandle build_symbol_button_with_options(
        icons::SymbolButtonOptions options,
        unsigned int hovered_id = 0xFFFFFFFFu,
        unsigned int focused_id = 0xFFFFFFFFu,
        unsigned int pressed_id = 0xFFFFFFFFu,
        bool focus_visible = false) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = hovered_id;
    detail::g_app.focused_id = focused_id;
    detail::g_app.pressed_id = pressed_id;
    detail::g_app.focus_visible = focus_visible;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::symbol_button<ButtonMsg>(
        "Grid View",
        icons::Symbol::Grid,
        Click{},
        options);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}
} // namespace button_test

void test_button_default_variant() {
    auto btn_h = button_test::build_button(ButtonVariant::Default, /*disabled=*/false);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.background.r == t.surface.r &&
           btn.background.g == t.surface.g &&
           btn.background.b == t.surface.b);
    assert(btn.text_color.r == t.foreground.r);
    // Hover is now driven by view-time animate_color; the static
    // `hover_background` field is no longer set so paint's fallback
    // branch falls through to the animated `background`.
    assert(btn.hover_background.a == 0);
    assert(btn.border_color.r == t.border.r);
    assert(btn.border_width == 1);
    assert(btn.border_radius == t.radius_sm);
    assert(btn.cursor_type == 1);
    assert(btn.focusable == true);
    assert(btn.callback_id != 0xFFFFFFFFu);
    assert(btn.interaction_role == InteractionRole::Button);
    std::puts("PASS: button default variant");
}

void test_button_primary_variant() {
    auto btn_h = button_test::build_button(ButtonVariant::Primary, /*disabled=*/false);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.background.r == t.accent.r &&
           btn.background.g == t.accent.g &&
           btn.background.b == t.accent.b);
    assert(btn.text_color.r == t.state_active_fg.r);
    assert(btn.hover_background.a == 0);
    assert(btn.border_color.r == t.accent.r);
    assert(btn.border_width == 1);
    assert(btn.cursor_type == 1);
    assert(btn.focusable == true);
    assert(btn.callback_id != 0xFFFFFFFFu);
    std::puts("PASS: button primary variant");
}

// Hover smoke test: build a button with `g_app.hovered_id` pre-set to
// the id we know the button will land on. animate_color's first call
// snaps to its target (initialised=false), so the resulting node
// background must equal the variant's hover colour exactly. This wires
// up the view-time check without needing to advance time.
void test_button_default_hovered_snaps_to_hover_bg() {
    auto btn_h = button_test::build_button(
        ButtonVariant::Default, /*disabled=*/false, /*hovered_id=*/0u);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.callback_id == 0u);
    assert(btn.background.r == t.state_hover_bg.r &&
           btn.background.g == t.state_hover_bg.g &&
           btn.background.b == t.state_hover_bg.b);
    detail::g_app.hovered_id = 0xFFFFFFFFu;
    std::puts("PASS: button default snaps to hover bg when hovered");
}

void test_button_pressed_snaps_to_pressed_bg() {
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = Color{1, 2, 3, 255};
    options.has_hover_background = true;
    options.hover_background = Color{5, 6, 7, 255};
    options.has_pressed_background = true;
    options.pressed_background = Color{9, 10, 11, 255};

    auto btn_h = button_test::build_button_with_options(
        options,
        /*hovered_id=*/0u,
        /*focused_id=*/0xFFFFFFFFu,
        /*pressed_id=*/0u);
    auto& btn = detail::node_at(btn_h);
    assert(btn.callback_id == 0u);
    assert(btn.background.r == 9 && btn.background.g == 10
           && btn.background.b == 11 && btn.background.a == 255);
    detail::g_app.hovered_id = 0xFFFFFFFFu;
    detail::g_app.pressed_id = 0xFFFFFFFFu;
    std::puts("PASS: button snaps to pressed bg when pressed");
}

void test_button_primary_hovered_snaps_to_hover_bg() {
    auto btn_h = button_test::build_button(
        ButtonVariant::Primary, /*disabled=*/false, /*hovered_id=*/0u);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.callback_id == 0u);
    assert(btn.background.r == t.accent_strong.r &&
           btn.background.g == t.accent_strong.g &&
           btn.background.b == t.accent_strong.b);
    detail::g_app.hovered_id = 0xFFFFFFFFu;
    std::puts("PASS: button primary snaps to accent_strong when hovered");
}

// Keyboard focus ring smoke test: pre-set `g_app.focused_id` and
// `g_app.focus_visible` to the id the button is about to claim, then
// assert the first frame snaps the animated border_width to
// `state_focus_ring_width`. Pointer focus keeps the id for event routing
// but leaves `focus_visible=false`, so it should not draw this ring.
void test_button_focused_snaps_to_focus_ring_width() {
    auto btn_h = button_test::build_button(
        ButtonVariant::Default, /*disabled=*/false,
        /*hovered_id=*/0xFFFFFFFFu, /*focused_id=*/0u,
        /*pressed_id=*/0xFFFFFFFFu, /*focus_visible=*/true);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.callback_id == 0u);
    assert(btn.border_width == t.state_focus_ring_width);
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;
    std::puts("PASS: button border_width snaps to focus ring width when focused");
}

void test_button_pointer_focused_hides_focus_ring() {
    auto btn_h = button_test::build_button(
        ButtonVariant::Default, /*disabled=*/false,
        /*hovered_id=*/0xFFFFFFFFu, /*focused_id=*/0u,
        /*pressed_id=*/0xFFFFFFFFu, /*focus_visible=*/false);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.callback_id == 0u);
    assert(btn.border_width == 1.0f);
    assert(btn.border_color.r == t.border.r);
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;
    std::puts("PASS: pointer-focused button hides focus ring");
}

void test_button_pointer_focus_reset_snaps_focus_ring_off() {
    detail::local_store().clear();
    detail::bump_local_gen();

    auto keyboard_h =
        button_test::build_button_frame_preserving_animation(
            /*focus_visible=*/true,
            /*prev_focus_visible=*/false);
    auto& keyboard = detail::node_at(keyboard_h);
    auto const& t = detail::g_app.theme;
    assert(keyboard.callback_id == 0u);
    assert(keyboard.border_width == t.state_focus_ring_width);
    assert(keyboard.border_color.r == t.state_focus_ring.r);

    auto pointer_h =
        button_test::build_button_frame_preserving_animation(
            /*focus_visible=*/false,
            /*prev_focus_visible=*/true);
    auto& pointer = detail::node_at(pointer_h);
    assert(pointer.callback_id == 0u);
    assert(pointer.border_width == 1.0f);
    assert(pointer.border_color.r == t.border.r);

    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.prev_focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;
    detail::g_app.prev_focus_visible = false;
    std::puts("PASS: pointer focus reset snaps focus ring off");
}

void test_focus_visible_tracks_keyboard_modality() {
    detail::g_app.focusable_ids.clear();
    detail::g_app.focusable_ids.push_back(7u);
    detail::g_app.focusable_ids.push_back(11u);
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;

    bool const tabbed =
        detail::handle_tab(0u, "test", "tab");
    assert(tabbed);
    assert(detail::g_app.focused_id == 7u);
    assert(detail::g_app.focus_visible == true);
    auto debug = diag::input_debug_snapshot();
    assert(debug.focused_id == 7u);
    assert(debug.focus_visible == true);
    assert(debug.input_modality == "keyboard");
    assert(debug.focus_visibility_reason == "keyboard_focus_navigation");

    bool const pointer_focus =
        detail::set_focus_id(
            7u,
            "test",
            "pointer-focus",
            false,
            InputModality::Pointer);
    assert(pointer_focus);
    assert(detail::g_app.focused_id == 7u);
    assert(detail::g_app.focus_visible == false);
    debug = diag::input_debug_snapshot();
    assert(debug.focused_id == 7u);
    assert(debug.focus_visible == false);
    assert(debug.input_modality == "pointer");
    assert(debug.focus_visibility_reason == "pointer_input_hides_focus_ring");

    detail::g_app.focusable_ids.clear();
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;
    std::puts("PASS: focus-visible tracks keyboard modality");
}

// Defocused (resting) border_width is the variant's natural 1px — not
// the focus-ring upgrade. Pairs with the focused test above so a
// regression that pins the value to either side stands out.
void test_button_defocused_resting_border_width() {
    auto btn_h = button_test::build_button(
        ButtonVariant::Default, /*disabled=*/false);
    auto& btn = detail::node_at(btn_h);
    assert(btn.border_width == 1);
    std::puts("PASS: button defocused border_width snaps to resting 1px");
}

void test_button_disabled() {
    for (auto variant : {ButtonVariant::Default, ButtonVariant::Primary}) {
        auto btn_h = button_test::build_button(variant, /*disabled=*/true);
        auto& btn = detail::node_at(btn_h);
        auto const& t = detail::g_app.theme;
        assert(btn.background.r == t.state_disabled_bg.r);
        assert(btn.text_color.r == t.state_disabled_fg.r);
        assert(btn.border_color.r == t.state_disabled_border.r);
        assert(btn.border_width == 1);
        assert(btn.cursor_type == 0);
        assert(btn.focusable == false);
        assert(btn.callback_id == 0xFFFFFFFFu);
        // hover_background was never assigned, stays transparent so paint
        // skips the hover overlay.
        assert(btn.hover_background.a == 0);
        // Disabled buttons keep the Button role for accessibility.
        assert(btn.interaction_role == InteractionRole::Button);
    }
    std::puts("PASS: button disabled (both variants)");
}

void test_button_disabled_custom_chrome() {
    ButtonStyleOptions options;
    options.disabled = true;
    options.has_background = true;
    options.background = Color{1, 2, 3, 4};
    options.has_border_color = true;
    options.border_color = Color{5, 6, 7, 8};
    options.has_text_color = true;
    options.text_color = Color{9, 10, 11, 12};
    options.border_width = 0.0f;

    auto btn_h = button_test::build_button_with_options(options);
    auto& btn = detail::node_at(btn_h);
    assert(btn.background.r == 1 && btn.background.a == 4);
    assert(btn.border_color.r == 5 && btn.border_color.a == 8);
    assert(btn.text_color.r == 9 && btn.text_color.a == 12);
    assert(btn.border_width == 0.0f);
    assert(btn.cursor_type == 0);
    assert(btn.focusable == false);
    assert(btn.callback_id == 0xFFFFFFFFu);
    assert(btn.debug_semantic_enabled == false);
    assert(detail::g_app.callbacks.empty());

    std::puts("PASS: button disabled custom chrome");
}

void test_button_style_options_custom_chrome() {
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = Color{1, 2, 3, 4};
    options.has_hover_background = true;
    options.hover_background = Color{5, 6, 7, 8};
    options.has_pressed_background = true;
    options.pressed_background = Color{17, 18, 19, 20};
    options.has_border_color = true;
    options.border_color = Color{9, 10, 11, 12};
    options.has_text_color = true;
    options.text_color = Color{13, 14, 15, 16};
    options.border_width = 0.0f;
    options.border_radius = 11.0f;
    options.font_size = 17.0f;
    options.padding_top = 1.0f;
    options.padding_right = 2.0f;
    options.padding_bottom = 3.0f;
    options.padding_left = 4.0f;
    options.max_width = 120.0f;
    options.fixed_height = 32.0f;
    options.text_align = TextAlign::Center;

    auto btn_h = button_test::build_button_with_options(options);
    auto& btn = detail::node_at(btn_h);
    assert(btn.background.r == 1 && btn.background.g == 2
           && btn.background.b == 3 && btn.background.a == 4);
    assert(btn.text_color.r == 13 && btn.text_color.a == 16);
    assert(btn.border_color.r == 9 && btn.border_color.a == 12);
    assert(btn.border_width == 0.0f);
    assert(btn.border_radius == 11.0f);
    assert(btn.font_size == 17.0f);
    assert(btn.style.padding[0] == 1.0f);
    assert(btn.style.padding[1] == 2.0f);
    assert(btn.style.padding[2] == 3.0f);
    assert(btn.style.padding[3] == 4.0f);
    assert(btn.style.max_width == 120.0f);
    assert(btn.style.fixed_height == 32.0f);
    assert(btn.min_hit_width == minimum_button_activation_size);
    assert(btn.min_hit_height == minimum_button_activation_size);
    assert(btn.style.text_align == TextAlign::Center);
    assert(btn.callback_id != 0xFFFFFFFFu);

    auto hovered_h = button_test::build_button_with_options(
        options, /*hovered_id=*/0u);
    auto& hovered = detail::node_at(hovered_h);
    assert(hovered.background.r == 5 && hovered.background.g == 6
           && hovered.background.b == 7 && hovered.background.a == 8);

    auto pressed_h = button_test::build_button_with_options(
        options,
        /*hovered_id=*/0u,
        /*focused_id=*/0xFFFFFFFFu,
        /*pressed_id=*/0u);
    auto& pressed = detail::node_at(pressed_h);
    assert(pressed.background.r == 17 && pressed.background.g == 18
           && pressed.background.b == 19 && pressed.background.a == 20);

    std::puts("PASS: button style options custom chrome");
}

void test_canvas_button_semantic_and_layout_contract() {
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = Color{255, 255, 255, 0};
    options.has_hover_background = true;
    options.hover_background = Color{230, 230, 230, 255};
    options.has_border_color = true;
    options.border_color = Color{0, 0, 0, 0};
    options.border_width = 0.0f;
    options.border_radius = 18.0f;
    options.max_width = 44.0f;
    options.fixed_height = 36.0f;

    auto btn_h = button_test::build_canvas_button_with_options(
        options, 0xF00Du);
    LAYOUT_NODE(btn_h, 100.0f);

    auto& btn = detail::node_at(btn_h);
    assert(btn.width == 44.0f);
    assert(btn.height == 36.0f);
    assert(btn.min_hit_width == minimum_button_activation_size);
    assert(btn.min_hit_height == minimum_button_activation_size);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.debug_semantic_label == "Grid View");
    assert(btn.callback_id == 0u);
    assert(btn.focusable == true);
    assert(btn.cursor_type == 1);
    assert(detail::g_app.callbacks.size() == 1);
    assert(detail::g_app.callback_roles.size() == 1);
    assert(detail::g_app.callback_roles[0] == InteractionRole::Button);
    assert(btn.children.size() == 1);

    auto& canvas = detail::node_at(btn.children[0]);
    assert(canvas.width == 44.0f);
    assert(canvas.height == 36.0f);
    assert(canvas.paint_fn);
    assert(canvas.paint_token == 0xF00Du);
    assert(canvas.debug_semantic_hidden == true);

    std::puts("PASS: canvas_button semantic + layout contract");
}

void test_canvas_button_minimum_hit_region_contract() {
    ButtonStyleOptions options;
    options.max_width = 36.0f;
    options.fixed_height = 36.0f;
    options.has_background = true;
    options.background = Color{255, 255, 255, 0};
    options.has_border_color = true;
    options.border_color = Color{0, 0, 0, 0};
    options.border_width = 0.0f;

    auto btn_h = button_test::build_canvas_button_with_options(
        options, 0xBEEFu);
    LAYOUT_NODE(btn_h, 100.0f);
    auto const& btn = detail::node_at(btn_h);
    float const expected_slop_x =
        (minimum_button_activation_size - btn.width) * 0.5f;
    float const expected_slop_y =
        (minimum_button_activation_size - btn.height) * 0.5f;
    CMD_LEN = 0;
    PAINT_NODE(btn_h, 0, 0, 0, 100.0f);

    bool found = false;
    for (unsigned int i = 0; i + 28 <= CMD_LEN; i += 4) {
        unsigned int op;
        std::memcpy(&op, &CMD_BUF[i], 4);
        if (op != static_cast<unsigned int>(Cmd::HitRegion))
            continue;
        float hx = 0.0f;
        float hy = 0.0f;
        float hw = 0.0f;
        float hh = 0.0f;
        std::memcpy(&hx, &CMD_BUF[i + 4], 4);
        std::memcpy(&hy, &CMD_BUF[i + 8], 4);
        std::memcpy(&hw, &CMD_BUF[i + 12], 4);
        std::memcpy(&hh, &CMD_BUF[i + 16], 4);
        assert(hx == btn.x - expected_slop_x);
        assert(hy == btn.y - expected_slop_y);
        assert(hw == minimum_button_activation_size);
        assert(hh == minimum_button_activation_size);
        found = true;
        break;
    }
    assert(found);
    std::puts("PASS: canvas_button minimum hit region contract");
}

void test_canvas_button_focus_visible_contract() {
    ButtonStyleOptions options;
    options.max_width = 44.0f;
    options.fixed_height = 36.0f;

    auto keyboard_h = button_test::build_canvas_button_with_options(
        options,
        0xCAFEu,
        /*hovered_id=*/0xFFFFFFFFu,
        /*focused_id=*/0u,
        /*pressed_id=*/0xFFFFFFFFu,
        /*focus_visible=*/true);
    auto& keyboard = detail::node_at(keyboard_h);
    auto const& t = detail::g_app.theme;
    assert(keyboard.callback_id == 0u);
    assert(keyboard.border_width == t.state_focus_ring_width);
    assert(keyboard.border_color.r == t.state_focus_ring.r);

    auto pointer_h = button_test::build_canvas_button_with_options(
        options,
        0xCAFEu,
        /*hovered_id=*/0xFFFFFFFFu,
        /*focused_id=*/0u,
        /*pressed_id=*/0xFFFFFFFFu,
        /*focus_visible=*/false);
    auto& pointer = detail::node_at(pointer_h);
    assert(pointer.callback_id == 0u);
    assert(pointer.border_width == 1.0f);
    assert(pointer.border_color.r == t.border.r);
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;

    std::puts("PASS: canvas_button focus-visible contract");
}

void test_canvas_button_visual_state_contract() {
    ButtonStyleOptions options;
    options.max_width = 44.0f;
    options.fixed_height = 36.0f;
    options.has_pressed_background = true;
    options.pressed_background = Color{9, 10, 11, 255};

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = 0u;
    detail::g_app.focused_id = 0u;
    detail::g_app.pressed_id = 0u;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    bool observed_pressed = false;
    bool observed_hovered = false;
    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::canvas_button<button_test::ButtonMsg>(
        "Stateful",
        44.0f,
        36.0f,
        [&observed_pressed, &observed_hovered](
                Painter& p,
                ButtonVisualState state) {
            observed_pressed = state.pressed;
            observed_hovered = state.hovered;
            p.line(2.0f, 2.0f, state.pressed ? 24.0f : 12.0f, 12.0f,
                   1.0f, Color{20, 20, 20, 255});
        },
        button_test::Click{},
        options,
        0xCAFEu);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto& btn = detail::node_at(root.children[0]);
    assert(btn.callback_id == 0u);
    assert(btn.children.size() == 1);
    auto& canvas = detail::node_at(btn.children[0]);
    assert(canvas.paint_token != 0xCAFEu);

    LAYOUT_NODE(root_h, 100.0f);
    CMD_LEN = 0;
    PAINT_NODE(root_h, 0, 0, 0, 100.0f);
    assert(observed_pressed);
    assert(observed_hovered);
    detail::g_app.hovered_id = 0xFFFFFFFFu;
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.pressed_id = 0xFFFFFFFFu;

    std::puts("PASS: canvas_button visual state contract");
}

void test_canvas_button_disabled_contract() {
    ButtonStyleOptions options;
    options.disabled = true;
    options.max_width = 44.0f;
    options.fixed_height = 36.0f;

    auto btn_h = button_test::build_canvas_button_with_options(options);
    auto& btn = detail::node_at(btn_h);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.debug_semantic_label == "Grid View");
    assert(btn.debug_semantic_enabled == false);
    assert(btn.callback_id == 0xFFFFFFFFu);
    assert(btn.focusable == false);
    assert(detail::g_app.callbacks.empty());
    assert(btn.children.size() == 1);
    assert(detail::node_at(btn.children[0]).paint_fn);

    std::puts("PASS: canvas_button disabled contract");
}

void test_canvas_button_disabled_custom_chrome() {
    ButtonStyleOptions options;
    options.disabled = true;
    options.has_background = true;
    options.background = Color{1, 2, 3, 0};
    options.has_border_color = true;
    options.border_color = Color{4, 5, 6, 0};
    options.border_width = 0.0f;
    options.max_width = 44.0f;
    options.fixed_height = 36.0f;

    auto btn_h = button_test::build_canvas_button_with_options(options);
    auto& btn = detail::node_at(btn_h);
    assert(btn.background.r == 1 && btn.background.a == 0);
    assert(btn.border_color.r == 4 && btn.border_color.a == 0);
    assert(btn.border_width == 0.0f);
    assert(btn.debug_semantic_enabled == false);
    assert(btn.callback_id == 0xFFFFFFFFu);
    assert(btn.focusable == false);
    assert(detail::g_app.callbacks.empty());
    assert(btn.children.size() == 1);

    std::puts("PASS: canvas_button disabled custom chrome");
}

void test_symbol_button_macos_contract() {
    icons::SymbolButtonOptions options;
    options.role = icons::SymbolPresentationRole::Toolbar;
    options.selected = true;
    options.token_salt = 0xA11CEu;

    auto btn_h = button_test::build_symbol_button_with_options(options);
    LAYOUT_NODE(btn_h, 100.0f);

    auto& btn = detail::node_at(btn_h);
    assert(btn.width == 36.0f);
    assert(btn.height == 36.0f);
    assert(btn.min_hit_width == minimum_button_activation_size);
    assert(btn.min_hit_height == minimum_button_activation_size);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.debug_semantic_label == "Grid View");
    assert(btn.callback_id == 0u);
    assert(btn.background.a == 150);
    assert(btn.border_radius == 15.0f);
    assert(btn.children.size() == 1);

    auto& canvas = detail::node_at(btn.children[0]);
    assert(canvas.width == 36.0f);
    assert(canvas.height == 36.0f);
    assert(canvas.debug_semantic_hidden == true);
    assert(canvas.paint_token
           == icons::symbol_button_paint_token(icons::Symbol::Grid, options));

    std::puts("PASS: symbol_button macOS chrome contract");
}

void test_macos_control_button_style_contract() {
    icons::ControlButtonStyleOptions options;
    options.role = icons::SymbolPresentationRole::Sidebar;
    options.selected = true;
    options.width = 180.0f;
    options.height = 28.0f;
    options.border_radius = 8.0f;

    auto style = icons::macos_control_button_style(options);
    assert(style.has_background);
    assert(style.background.r == 236);
    assert(style.background.a == 150);
    assert(style.has_hover_background);
    assert(style.hover_background.r == 232);
    assert(style.hover_background.a == 176);
    assert(style.has_pressed_background);
    assert(style.pressed_background.a == 196);
    assert(style.border_width == 0.0f);
    assert(style.border_radius == 8.0f);
    assert(style.max_width == 180.0f);
    assert(style.fixed_height == 28.0f);
    assert(style.min_hit_width == minimum_button_activation_size);
    assert(style.min_hit_height == minimum_button_activation_size);

    icons::SymbolButtonOptions symbol_options;
    symbol_options.role = icons::SymbolPresentationRole::Toolbar;
    symbol_options.selected = true;
    auto symbol_style = icons::macos_symbol_button_style(symbol_options);
    auto control_style = icons::macos_control_button_style(
        icons::ControlButtonStyleOptions{
            .role = symbol_options.role,
            .selected = symbol_options.selected,
            .width = icons::symbol_button_width(symbol_options),
            .height = icons::symbol_button_height(symbol_options),
        });
    assert(symbol_style.background == control_style.background);
    assert(symbol_style.hover_background == control_style.hover_background);
    assert(symbol_style.pressed_background == control_style.pressed_background);
    assert(symbol_style.border_radius == control_style.border_radius);
    assert(symbol_style.max_width == control_style.max_width);
    assert(symbol_style.fixed_height == control_style.fixed_height);

    std::puts("PASS: macOS control button style contract");
}

void test_glass_control_button_style_material_contract() {
    set_theme(Theme{});

    auto style = widget::glass_control_button_style(GlassControlStyleOptions{
        .kind = MaterialKind::Thin,
        .role = MaterialSurfaceRole::Toolbar,
        .selected = true,
        .width = 104.0f,
        .height = 32.0f,
    });
    assert(style.has_material);
    assert(style.material.kind == MaterialKind::Thin);
    assert(style.material.role == MaterialSurfaceRole::Toolbar);
    assert(style.material.fallback);
    assert(style.material.container.interactive);
    assert(style.material.container.morph_transitions);
    assert(style.has_background);
    assert(style.has_hover_background);
    assert(style.has_pressed_background);
    assert(style.max_width == 104.0f);
    assert(style.fixed_height == 32.0f);
    assert(style.shape == MaterialSurfaceShape::Capsule);
    assert(style.text_align == TextAlign::Center);

    auto btn_h = button_test::build_button_with_options(style);
    auto& btn = detail::node_at(btn_h);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.material.kind == MaterialKind::Thin);
    assert(btn.material.role == MaterialSurfaceRole::Toolbar);
    assert(btn.material.fallback);
    assert(btn.material.container.interactive);
    assert(btn.material.tint == btn.background);
    assert(btn.material.border == btn.border_color);
    assert(btn.material.foreground == btn.text_color);
    assert(btn.material_shape == MaterialSurfaceShape::Capsule);
    assert(btn.min_hit_width == minimum_button_activation_size);
    assert(btn.min_hit_height == minimum_button_activation_size);

    auto disabled_style = widget::glass_control_button_style(
        GlassControlStyleOptions{
            .kind = MaterialKind::Regular,
            .role = MaterialSurfaceRole::Navigation,
            .disabled = true,
        });
    auto disabled_h = button_test::build_button_with_options(disabled_style);
    auto& disabled = detail::node_at(disabled_h);
    assert(disabled.material.kind == MaterialKind::Regular);
    assert(disabled.material.role == MaterialSurfaceRole::Navigation);
    assert(!disabled.material.container.interactive);
    assert(disabled.material_shape == MaterialSurfaceShape::Capsule);
    assert(!disabled.focusable);
    assert(!disabled.debug_semantic_enabled);

    std::puts("PASS: glass control button style emits material contract");
}

void assert_material_interaction_state(MaterialStyle const& material,
                                       bool hovered,
                                       bool pressed,
                                       bool focused,
                                       bool pointer_inside);

void test_glass_button_accepts_configurable_glass_style() {
    set_theme(Theme{});

    auto const tint = Color{255, 149, 0, 132};
    auto const border = Color{255, 149, 0, 196};
    auto glass = layout::glass_regular().tint(tint).border(border);
    auto options = GlassControlStyleOptions{
        .role = MaterialSurfaceRole::Control,
        .width = 116.0f,
        .height = 34.0f,
    };

    auto converted = widget::glass_control_style_options(glass, options);
    assert(converted.kind == MaterialKind::Regular);
    assert(converted.has_tint);
    assert(converted.tint == tint);
    assert(converted.has_border);
    assert(converted.border == border);

    auto style = widget::glass_button_style(glass, options);
    assert(style.has_material);
    assert(style.material.kind == MaterialKind::Regular);
    assert(style.material.role == MaterialSurfaceRole::Control);
    assert(style.material.tint == tint);
    assert(style.material.border == border);
    assert(style.background == tint);
    assert(style.hover_background.r == tint.r);
    assert(style.hover_background.g == tint.g);
    assert(style.hover_background.b == tint.b);
    assert(style.hover_background.a >= 174);
    assert(style.pressed_background.a >= 204);
    assert(style.text_color == Theme{}.foreground);
    assert(std::string_view(style.material.contrast_intent)
           == "tinted-glass-control");
    assert(std::string_view(style.material.plan_id)
           == "material.glass.control.tinted");
    assert(style.max_width == 116.0f);
    assert(style.fixed_height == 34.0f);

    auto const effect_identity =
        layout::glass_effect_identity("button", "primary");
    auto effect_glass = layout::glass_regular()
        .tint(tint)
        .border(border)
        .effect_id(effect_identity)
        .matched_geometry(0.55f, false)
        .effect_union("button.toolbar", "primary-actions", 10.0f);
    auto effect_style = widget::glass_button_style(effect_glass, options);
    assert(effect_style.material.glass_identity == effect_identity);
    assert(effect_style.material.transition.kind
           == MaterialGlassTransitionKind::MatchedGeometry);
    assert(std::fabs(effect_style.material.transition.progress - 0.55f)
           < 0.0001f);
    assert(!effect_style.material.transition.appearing);
    assert(effect_style.material.container.container_id
           == layout::glass_effect_stable_id("button.toolbar"));
    assert(effect_style.material.container.union_id
           == layout::glass_effect_stable_id("primary-actions"));
    assert(std::fabs(effect_style.material.container.spacing - 10.0f)
           < 0.0001f);
    assert(effect_style.material.container.interactive);
    assert(effect_style.material.container.morph_transitions);

    auto interacted_h = button_test::build_button_with_options(
        effect_style,
        /*hovered_id=*/0u,
        /*focused_id=*/0u,
        /*pressed_id=*/0u,
        /*focus_visible=*/true);
    auto const& interacted_button = detail::node_at(interacted_h);
    assert(interacted_button.material.kind == MaterialKind::Regular);
    assert_material_interaction_state(
        interacted_button.material,
        true,
        true,
        true,
        true);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = 0xFFFFFFFFu;
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.pressed_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::glass_button<button_test::ButtonMsg>(
        "Tinted",
        button_test::Click{},
        effect_glass,
        options);
    Scope::set_current(nullptr);

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 1u);
    auto const& wrapped = detail::node_at(root.children[0]);
    assert(wrapped.interaction_role == InteractionRole::Button);
    assert(wrapped.material.kind == MaterialKind::Regular);
    assert(wrapped.material.role == MaterialSurfaceRole::Control);
    assert(wrapped.material.tint == tint);
    assert(wrapped.material.border == border);
    assert(wrapped.material.container.interactive);
    assert(wrapped.material.container.container_id
           == layout::glass_effect_stable_id("button.toolbar"));
    assert(wrapped.material.container.union_id
           == layout::glass_effect_stable_id("primary-actions"));
    assert(std::fabs(wrapped.material.container.spacing - 10.0f)
           < 0.0001f);
    assert(wrapped.material.transition.kind
           == MaterialGlassTransitionKind::MatchedGeometry);
    assert(std::fabs(wrapped.material.transition.progress - 0.55f)
           < 0.0001f);
    assert(wrapped.material.glass_identity == effect_identity);
    assert(wrapped.style.max_width == 116.0f);
    assert(wrapped.style.fixed_height == 34.0f);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    CMD_LEN = 0;

    auto scoped_root_h = detail::alloc_node();
    detail::node_at(scoped_root_h).style.flex_direction = FlexDirection::Column;
    Scope scoped_scope(scoped_root_h);
    Scope::set_current(&scoped_scope);
    layout::glass_effect_container(
        layout::GlassEffectContainerOptions{
            .container_id = 804u,
            .union_id = 3u,
            .spacing = 11.0f,
            .interactive = false,
            .morph_transitions = true,
        },
        [&] {
            layout::glass_effect_transition(
                layout::glass_materialize_transition(0.25f, false),
                [&] {
                    widget::glass_button<button_test::ButtonMsg>(
                        "Scoped",
                        button_test::Click{},
                        layout::glass_regular()
                            .effect_id("button", "scoped"),
                        GlassControlStyleOptions{
                            .role = MaterialSurfaceRole::Toolbar,
                            .height = 32.0f,
                        });
                });
        });
    Scope::set_current(nullptr);

    auto const& scoped_root = detail::node_at(scoped_root_h);
    assert(scoped_root.children.size() == 1u);
    auto const& scoped_button = detail::node_at(scoped_root.children[0]);
    assert(scoped_button.material.container.container_id == 804u);
    assert(scoped_button.material.container.union_id == 3u);
    assert(std::fabs(scoped_button.material.container.spacing - 11.0f)
           < 0.0001f);
    assert(scoped_button.material.container.interactive);
    assert(scoped_button.material.container.morph_transitions);
    assert(scoped_button.material.transition.kind
           == MaterialGlassTransitionKind::Materialize);
    assert(std::fabs(scoped_button.material.transition.progress - 0.25f)
           < 0.0001f);
    assert(!scoped_button.material.transition.appearing);
    assert(scoped_button.material.glass_identity
           == layout::glass_effect_identity("button", "scoped"));

    auto clear = widget::glass_button_style(layout::glass_clear(), {});
    assert(clear.has_material);
    assert(clear.material.kind == MaterialKind::Clear);

    auto identity = widget::glass_button_style(layout::glass_identity(), {});
    assert(!identity.has_material);
    assert(identity.material.kind == MaterialKind::None);

    std::puts("PASS: glass button accepts configurable glass style");
}

void test_glass_prominent_button_style_material_contract() {
    set_theme(Theme{});

    auto style = widget::glass_prominent_button_style(
        GlassControlStyleOptions{
            .prominence = 1.0f,
            .width = 120.0f,
            .height = 36.0f,
        });
    assert(style.has_material);
    assert(style.material.kind == MaterialKind::Regular);
    assert(style.material.role == MaterialSurfaceRole::Control);
    assert(style.material.container.interactive);
    assert(style.material.container.morph_transitions);
    assert(style.material.prominence.enabled);
    assert(std::fabs(style.material.prominence.intensity - 1.0f) < 0.0001f);
    assert(std::string_view(style.material.contrast_intent)
        == "prominent-action");
    assert(style.material.tint.a > 0);
    assert(style.has_background);
    assert(style.has_hover_background);
    assert(style.has_pressed_background);
    assert(style.has_border_color);
    assert(style.border_width == 1.0f);
    assert(style.max_width == 120.0f);
    assert(style.fixed_height == 36.0f);
    assert(style.shape == MaterialSurfaceShape::Capsule);

    auto btn_h = button_test::build_button_with_options(style);
    auto& btn = detail::node_at(btn_h);
    assert(btn.material.kind == MaterialKind::Regular);
    assert(btn.material.role == MaterialSurfaceRole::Control);
    assert(btn.material.prominence.enabled);
    assert(btn.material_shape == MaterialSurfaceShape::Capsule);
    assert(btn.material.tint == btn.background);
    assert(btn.material.border == btn.border_color);
    assert(btn.material.foreground == btn.text_color);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::glass_prominent_button<button_test::ButtonMsg>(
        "Prominent",
        button_test::Click{},
        GlassControlStyleOptions{
            .width = 132.0f,
            .height = 40.0f,
        });
    Scope::set_current(nullptr);

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 1u);
    auto const& wrapped = detail::node_at(root.children[0]);
    assert(wrapped.callback_id == 0u);
    assert(wrapped.interaction_role == InteractionRole::Button);
    assert(wrapped.material.kind == MaterialKind::Regular);
    assert(wrapped.material.role == MaterialSurfaceRole::Control);
    assert(wrapped.material.prominence.enabled);
    assert(wrapped.material.container.interactive);
    assert(wrapped.style.max_width == 132.0f);
    assert(wrapped.style.fixed_height == 40.0f);

    std::puts("PASS: glass prominent button style emits material contract");
}

void test_glass_split_button_style_material_contract() {
    set_theme(Theme{});

    auto style = widget::glass_split_button_style(
        GlassSplitButtonStyleOptions{
            .kind = MaterialKind::Clear,
            .role = MaterialSurfaceRole::Toolbar,
            .segment = GlassSplitButtonSegment::Leading,
            .selected = true,
            .disabled = false,
            .container_id = 77u,
            .union_id = 12u,
            .spacing = 6.0f,
            .width = 44.0f,
            .height = 36.0f,
            .border_radius = 14.0f,
        });
    assert(style.has_material);
    assert(style.material.kind == MaterialKind::Clear);
    assert(style.material.role == MaterialSurfaceRole::Toolbar);
    assert(style.material.fallback);
    assert(style.material.container.container_id == 77u);
    assert(style.material.container.union_id == 12u);
    assert(style.material.container.spacing == 6.0f);
    assert(style.material.container.interactive);
    assert(style.material.container.morph_transitions);
    assert(style.border_width == 1.0f);
    assert(style.border_radius == 14.0f);
    assert(style.shape == MaterialSurfaceShape::Capsule);
    assert(style.max_width == 44.0f);
    assert(style.fixed_height == 36.0f);
    assert(style.text_align == TextAlign::Center);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::symbol_button<button_test::ButtonMsg>(
        "Sort",
        icons::Symbol::SortGroup,
        button_test::Click{},
        icons::SymbolButtonOptions{
            .role = icons::SymbolPresentationRole::Toolbar,
            .width = 44.0f,
            .height = 36.0f,
            .token_salt = 0x7712u,
        },
        style);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto& btn = detail::node_at(root.children[0]);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.debug_semantic_label == "Sort");
    assert(btn.material.kind == MaterialKind::Clear);
    assert(btn.material.role == MaterialSurfaceRole::Toolbar);
    assert(btn.material.container.container_id == 77u);
    assert(btn.material.container.union_id == 12u);
    assert(btn.material.container.interactive);
    assert(btn.material.container.morph_transitions);
    assert(btn.material.tint == btn.background);
    assert(btn.material_shape == MaterialSurfaceShape::Capsule);
    assert(btn.children.size() == 1);

    auto disabled = widget::glass_split_button_style(
        GlassSplitButtonStyleOptions{
            .kind = MaterialKind::Clear,
            .role = MaterialSurfaceRole::Toolbar,
            .segment = GlassSplitButtonSegment::Trailing,
            .selected = false,
            .disabled = true,
            .container_id = 77u,
            .union_id = 12u,
        });
    assert(disabled.disabled);
    assert(!disabled.has_material);

    std::puts("PASS: glass split button style emits material contract");
}

void test_glass_selection_button_style_material_contract() {
    set_theme(Theme{});

    auto style = widget::glass_selection_button_style(
        GlassSelectionStyleOptions{
            .chrome = GlassSelectionChrome::SidebarPill,
            .role = MaterialSurfaceRole::Sidebar,
            .selected = true,
            .width = 188.0f,
            .height = 30.0f,
            .border_radius = 9.0f,
        });
    assert(style.has_material);
    assert(style.material.kind == MaterialKind::Thin);
    assert(style.material.role == MaterialSurfaceRole::Sidebar);
    assert(style.material.fallback);
    assert(style.material.container.interactive);
    assert(style.has_background);
    assert(style.has_hover_background);
    assert(style.has_pressed_background);
    assert(style.background.a == 150);
    assert(style.hover_background.a == 176);
    assert(style.pressed_background.a == 196);
    assert(style.border_width == 0.0f);
    assert(style.border_radius == 9.0f);
    assert(style.shape == MaterialSurfaceShape::Capsule);
    assert(style.max_width == 188.0f);
    assert(style.fixed_height == 30.0f);
    assert(style.text_color == detail::g_app.theme.accent);

    auto btn_h = button_test::build_canvas_button_with_options(style);
    auto& btn = detail::node_at(btn_h);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.material.kind == MaterialKind::Thin);
    assert(btn.material.role == MaterialSurfaceRole::Sidebar);
    assert(btn.material.fallback);
    assert(btn.material.container.interactive);
    assert(btn.material.tint == btn.background);
    assert(btn.material.border == btn.border_color);
    assert(btn.material.foreground == btn.text_color);
    assert(btn.material_shape == MaterialSurfaceShape::Capsule);
    assert(btn.children.size() == 1);

    auto unselected = widget::glass_selection_button_style(
        GlassSelectionStyleOptions{
            .role = MaterialSurfaceRole::Surface,
            .selected = false,
            .width = 160.0f,
            .height = 28.0f,
        });
    assert(!unselected.has_material);
    auto unselected_h =
        button_test::build_canvas_button_with_options(unselected);
    auto& unselected_btn = detail::node_at(unselected_h);
    assert(unselected_btn.material.kind == MaterialKind::None);
    assert(unselected_btn.style.max_width == 160.0f);
    assert(unselected_btn.style.fixed_height == 28.0f);

    std::puts("PASS: glass selection button style emits material contract");
}

void test_glass_outline_row_button_style_material_contract() {
    set_theme(Theme{});

    auto selected = widget::glass_outline_row_button_style(
        GlassOutlineRowStyleOptions{
            .chrome = GlassOutlineRowChrome::ListRow,
            .selected_kind = MaterialKind::Thin,
            .expanded_kind = MaterialKind::Clear,
            .unselected_kind = MaterialKind::None,
            .role = MaterialSurfaceRole::Surface,
            .selected = true,
            .expanded = false,
            .disabled = false,
            .depth = 2u,
            .width = 220.0f,
            .height = 28.0f,
            .border_radius = 8.0f,
            .font_size = 13.0f,
        });
    assert(selected.has_material);
    assert(selected.material.kind == MaterialKind::Thin);
    assert(selected.material.role == MaterialSurfaceRole::Surface);
    assert(selected.material.fallback);
    assert(selected.material.container.interactive);
    assert(selected.background == detail::g_app.theme.accent);
    assert(selected.border_width == 1.0f);
    assert(selected.border_radius == 8.0f);
    assert(selected.font_size == 13.0f);
    assert(selected.max_width == 220.0f);
    assert(selected.fixed_height == 28.0f);
    assert(selected.padding_left == 28.0f);

    auto row_h = button_test::build_canvas_button_with_options(selected);
    auto& row = detail::node_at(row_h);
    assert(row.interaction_role == InteractionRole::Button);
    assert(row.material.kind == MaterialKind::Thin);
    assert(row.material.role == MaterialSurfaceRole::Surface);
    assert(row.material.tint == row.background);
    assert(row.material.foreground == row.text_color);
    assert(row.children.size() == 1);

    auto sidebar = widget::glass_outline_row_button_style(
        GlassOutlineRowStyleOptions{
            .chrome = GlassOutlineRowChrome::SidebarPill,
            .role = MaterialSurfaceRole::Sidebar,
            .selected = true,
        });
    assert(sidebar.has_material);
    assert(sidebar.material.kind == MaterialKind::Thin);
    assert(sidebar.material.role == MaterialSurfaceRole::Sidebar);
    assert(sidebar.text_color == detail::g_app.theme.accent);
    assert(sidebar.border_width == 0.0f);
    assert(sidebar.shape == MaterialSurfaceShape::Capsule);

    auto expanded = widget::glass_outline_row_button_style(
        GlassOutlineRowStyleOptions{
            .chrome = GlassOutlineRowChrome::ColumnRow,
            .role = MaterialSurfaceRole::Content,
            .selected = false,
            .expanded = true,
        });
    assert(expanded.has_material);
    assert(expanded.material.kind == MaterialKind::Clear);
    assert(expanded.material.role == MaterialSurfaceRole::Content);
    assert(expanded.material.container.interactive);
    assert(expanded.background.a == 122);

    auto unselected = widget::glass_outline_row_button_style(
        GlassOutlineRowStyleOptions{.selected = false});
    assert(!unselected.has_material);
    assert(unselected.background.a == 0);

    std::puts("PASS: glass outline row style emits material contract");
}

void test_glass_menu_item_symbol_button_material_contract() {
    set_theme(Theme{});

    auto style = widget::glass_menu_item_button_style(
        GlassMenuItemStyleOptions{
            .width = 44.0f,
            .height = 36.0f,
            .border_radius = 11.0f,
        });
    assert(style.has_material);
    assert(style.material.kind == MaterialKind::Clear);
    assert(style.material.role == MaterialSurfaceRole::Overlay);
    assert(style.material.fallback);
    assert(style.material.container.interactive);
    assert(style.border_width == 0.0f);
    assert(style.border_radius == 11.0f);
    assert(style.shape == MaterialSurfaceShape::Default);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::symbol_button<button_test::ButtonMsg>(
        "New File",
        icons::Symbol::NewDocument,
        button_test::Click{},
        icons::SymbolButtonOptions{
            .role = icons::SymbolPresentationRole::Navigation,
            .width = 44.0f,
            .height = 36.0f,
            .token_salt = 0x6711u,
        },
        style);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto& btn = detail::node_at(root.children[0]);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.debug_semantic_label == "New File");
    assert(btn.material.kind == MaterialKind::Clear);
    assert(btn.material.role == MaterialSurfaceRole::Overlay);
    assert(btn.material.container.interactive);
    assert(btn.material.tint == btn.background);
    assert(btn.material_shape == MaterialSurfaceShape::Default);
    assert(btn.children.size() == 1);

    auto disabled = widget::glass_menu_item_button_style(
        GlassMenuItemStyleOptions{.disabled = true});
    assert(disabled.disabled);
    assert(!disabled.has_material);

    std::puts("PASS: glass menu item symbol button emits material contract");
}

void test_glass_table_header_button_material_contract() {
    set_theme(Theme{});

    auto style = widget::glass_table_header_button_style(
        GlassTableHeaderStyleOptions{
            .sorted = true,
            .width = 160.0f,
            .height = 28.0f,
            .border_radius = 8.0f,
            .font_size = 12.0f,
        });
    assert(style.has_material);
    assert(style.material.kind == MaterialKind::Clear);
    assert(style.material.role == MaterialSurfaceRole::Content);
    assert(style.material.fallback);
    assert(style.material.container.interactive);
    assert(style.border_width == 1.0f);
    assert(style.border_radius == 8.0f);
    assert(style.shape == MaterialSurfaceShape::Default);
    assert(style.font_size == 12.0f);
    assert(style.max_width == 160.0f);
    assert(style.fixed_height == 28.0f);
    assert(style.text_align == TextAlign::Start);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::button<button_test::ButtonMsg>(
        "Name",
        button_test::Click{},
        style);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    auto& btn = detail::node_at(root.children[0]);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.text == "Name");
    assert(btn.material.kind == MaterialKind::Clear);
    assert(btn.material.role == MaterialSurfaceRole::Content);
    assert(btn.material.container.interactive);
    assert(btn.material.tint == btn.background);
    assert(btn.material_shape == MaterialSurfaceShape::Default);

    auto unsorted = widget::glass_table_header_button_style(
        GlassTableHeaderStyleOptions{.sorted = false});
    assert(unsorted.has_material);
    assert(unsorted.border_width == 0.0f);

    auto disabled = widget::glass_table_header_button_style(
        GlassTableHeaderStyleOptions{.disabled = true});
    assert(disabled.disabled);
    assert(!disabled.has_material);

    std::puts("PASS: glass table header button emits material contract");
}

void test_glass_disclosure_header_style_material_contract() {
    set_theme(Theme{});

    auto collapsed = widget::glass_disclosure_header_style(
        GlassDisclosureStyleOptions{
            .expanded = false,
            .width = 240.0f,
            .height = 34.0f,
            .border_radius = 9.0f,
            .font_size = 13.0f,
        });
    assert(collapsed.has_material);
    assert(collapsed.material.kind == MaterialKind::Clear);
    assert(collapsed.material.role == MaterialSurfaceRole::Content);
    assert(collapsed.material.fallback);
    assert(collapsed.material.container.interactive);
    assert(collapsed.border_width == 1.0f);
    assert(collapsed.border_radius == 9.0f);
    assert(collapsed.shape == MaterialSurfaceShape::Default);
    assert(collapsed.font_size == 13.0f);
    assert(collapsed.max_width == 240.0f);
    assert(collapsed.fixed_height == 34.0f);

    auto expanded = widget::glass_disclosure_header_style(
        GlassDisclosureStyleOptions{.expanded = true});
    assert(expanded.has_material);
    assert(expanded.material.tint.a > collapsed.material.tint.a);

    auto disabled = widget::glass_disclosure_header_style(
        GlassDisclosureStyleOptions{.disabled = true});
    assert(disabled.disabled);
    assert(!disabled.has_material);

    std::puts("PASS: glass disclosure header style emits material contract");
}

void test_glass_widget_helpers_emit_material_buttons() {
    set_theme(Theme{});
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.callback_roles.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();

    auto const identity =
        layout::glass_effect_identity("widget-helpers", "outline-row");
    auto effect_glass = layout::glass_regular()
        .effect_id(identity)
        .matched_geometry(0.5f, true)
        .effect_union("widget-helpers", "rows", 12.0f);

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::glass_selection_button<button_test::ButtonMsg>(
        "Recents",
        button_test::Click{},
        GlassSelectionStyleOptions{
            .chrome = GlassSelectionChrome::SidebarPill,
            .role = MaterialSurfaceRole::Sidebar,
            .selected = true,
            .width = 188.0f,
            .height = 30.0f,
        });
    widget::glass_outline_row_button<button_test::ButtonMsg>(
        "Documents",
        button_test::Click{},
        effect_glass,
        GlassOutlineRowStyleOptions{
            .chrome = GlassOutlineRowChrome::ColumnRow,
            .role = MaterialSurfaceRole::Content,
            .selected = true,
            .width = 220.0f,
            .height = 28.0f,
        });
    widget::glass_menu_item_button<button_test::ButtonMsg>(
        "Rename",
        button_test::Click{},
        GlassMenuItemStyleOptions{
            .width = 180.0f,
            .height = 30.0f,
        });
    widget::glass_table_header_button<button_test::ButtonMsg>(
        "Name",
        button_test::Click{},
        GlassTableHeaderStyleOptions{
            .sorted = true,
            .width = 160.0f,
            .height = 28.0f,
        });
    widget::glass_disclosure_header_button<button_test::ButtonMsg>(
        "Details",
        button_test::Click{},
        GlassDisclosureStyleOptions{
            .expanded = true,
            .width = 240.0f,
            .height = 34.0f,
        });
    Scope::set_current(nullptr);

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 5u);
    assert(detail::g_app.callbacks.size() == 5u);
    assert(detail::g_app.callback_roles.size() == 5u);

    auto const& selection = detail::node_at(root.children[0]);
    assert(selection.text == "Recents");
    assert(selection.interaction_role == InteractionRole::Button);
    assert(selection.material.kind == MaterialKind::Thin);
    assert(selection.material.role == MaterialSurfaceRole::Sidebar);
    assert(selection.material_shape == MaterialSurfaceShape::Capsule);
    assert(selection.style.max_width == 188.0f);
    assert(selection.style.fixed_height == 30.0f);

    auto const& outline = detail::node_at(root.children[1]);
    assert(outline.text == "Documents");
    assert(outline.material.kind == MaterialKind::Thin);
    assert(outline.material.role == MaterialSurfaceRole::Content);
    assert(outline.material.glass_identity == identity);
    assert(outline.material.transition.kind
           == MaterialGlassTransitionKind::MatchedGeometry);
    assert(std::fabs(outline.material.transition.progress - 0.5f) < 0.0001f);
    assert(outline.material.container.container_id
           == layout::glass_effect_stable_id("widget-helpers"));
    assert(outline.material.container.union_id
           == layout::glass_effect_stable_id("rows"));
    assert(outline.material.container.interactive);

    auto const& menu = detail::node_at(root.children[2]);
    assert(menu.text == "Rename");
    assert(menu.material.kind == MaterialKind::Clear);
    assert(menu.material.role == MaterialSurfaceRole::Overlay);
    assert(menu.style.max_width == 180.0f);
    assert(menu.style.fixed_height == 30.0f);

    auto const& header = detail::node_at(root.children[3]);
    assert(header.text == "Name");
    assert(header.material.kind == MaterialKind::Clear);
    assert(header.material.role == MaterialSurfaceRole::Content);
    assert(header.border_width == 1.0f);

    auto const& disclosure = detail::node_at(root.children[4]);
    assert(disclosure.text == "Details");
    assert(disclosure.material.kind == MaterialKind::Clear);
    assert(disclosure.material.role == MaterialSurfaceRole::Content);
    assert(disclosure.style.fixed_height == 34.0f);

    std::puts("PASS: glass widget helpers emit material buttons");
}

void assert_glass_effect_context(
        MaterialStyle const& material,
        MaterialGlassIdentityDescriptor identity,
        std::uint32_t container_id,
        std::uint32_t union_id,
        float spacing,
        MaterialGlassTransitionKind transition_kind,
        float transition_progress,
        bool transition_appearing) {
    assert(material.glass_identity == identity);
    assert(material.transition.kind == transition_kind);
    assert(std::fabs(material.transition.progress - transition_progress)
           < 0.0001f);
    assert(material.transition.appearing == transition_appearing);
    assert(material.container.container_id == container_id);
    assert(material.container.union_id == union_id);
    assert(std::fabs(material.container.spacing - spacing) < 0.0001f);
    assert(material.container.interactive);
    assert(material.container.morph_transitions);
}

void assert_material_interaction_state(MaterialStyle const& material,
                                       bool hovered,
                                       bool pressed,
                                       bool focused,
                                       bool pointer_inside) {
    assert(material.interaction.hovered == hovered);
    assert(material.interaction.pressed == pressed);
    assert(material.interaction.focused == focused);
    assert(material.interaction.pointer_inside == pointer_inside);
    assert(std::fabs(material.interaction.pointer_x - 0.5f) < 0.0001f);
    assert(std::fabs(material.interaction.pointer_y - 0.5f) < 0.0001f);
}

void test_glass_effect_context_reaches_control_styles() {
    set_theme(Theme{});

    auto const identity =
        layout::glass_effect_identity("controls", "primary");
    auto const container_id =
        layout::glass_effect_stable_id("controls.scope");
    auto const union_id =
        layout::glass_effect_stable_id("controls.cluster");
    auto effect_glass = layout::glass_clear()
        .effect_id(identity)
        .materialize(0.4f, false)
        .effect_union(
            "controls.scope",
            "controls.cluster",
            13.0f,
            true,
            true);

    auto prominent = widget::glass_prominent_button_style(
        effect_glass,
        GlassControlStyleOptions{
            .role = MaterialSurfaceRole::Control,
            .width = 124.0f,
            .height = 36.0f,
        });
    assert(prominent.has_material);
    assert(prominent.material.kind == MaterialKind::Regular);
    assert(prominent.material.prominence.enabled);
    assert(prominent.max_width == 124.0f);
    assert_glass_effect_context(
        prominent.material,
        identity,
        container_id,
        union_id,
        13.0f,
        MaterialGlassTransitionKind::Materialize,
        0.4f,
        false);

    auto split = widget::glass_split_button_style(
        effect_glass,
        GlassSplitButtonStyleOptions{
            .segment = GlassSplitButtonSegment::Middle,
            .selected = true,
            .width = 44.0f,
            .height = 32.0f,
        });
    assert(split.has_material);
    assert(split.material.kind == MaterialKind::Clear);
    assert_glass_effect_context(
        split.material,
        identity,
        container_id,
        union_id,
        13.0f,
        MaterialGlassTransitionKind::Materialize,
        0.4f,
        false);

    auto selection = widget::glass_selection_button_style(
        effect_glass,
        GlassSelectionStyleOptions{
            .chrome = GlassSelectionChrome::SidebarPill,
            .selected = true,
        });
    assert(selection.has_material);
    assert(selection.material.kind == MaterialKind::Thin);
    assert_glass_effect_context(
        selection.material,
        identity,
        container_id,
        union_id,
        13.0f,
        MaterialGlassTransitionKind::Materialize,
        0.4f,
        false);

    auto outline = widget::glass_outline_row_button_style(
        effect_glass,
        GlassOutlineRowStyleOptions{
            .chrome = GlassOutlineRowChrome::ColumnRow,
            .selected = false,
            .expanded = true,
        });
    assert(outline.has_material);
    assert(outline.material.kind == MaterialKind::Clear);
    assert_glass_effect_context(
        outline.material,
        identity,
        container_id,
        union_id,
        13.0f,
        MaterialGlassTransitionKind::Materialize,
        0.4f,
        false);

    auto menu = widget::glass_menu_item_button_style(effect_glass);
    assert(menu.has_material);
    assert(menu.material.kind == MaterialKind::Clear);
    assert_glass_effect_context(
        menu.material,
        identity,
        container_id,
        union_id,
        13.0f,
        MaterialGlassTransitionKind::Materialize,
        0.4f,
        false);

    auto table = widget::glass_table_header_button_style(
        effect_glass,
        GlassTableHeaderStyleOptions{.sorted = true});
    assert(table.has_material);
    assert(table.material.kind == MaterialKind::Clear);
    assert_glass_effect_context(
        table.material,
        identity,
        container_id,
        union_id,
        13.0f,
        MaterialGlassTransitionKind::Materialize,
        0.4f,
        false);

    auto disclosure = widget::glass_disclosure_header_style(
        effect_glass,
        GlassDisclosureStyleOptions{.expanded = true});
    assert(disclosure.has_material);
    assert(disclosure.material.kind == MaterialKind::Clear);
    assert_glass_effect_context(
        disclosure.material,
        identity,
        container_id,
        union_id,
        13.0f,
        MaterialGlassTransitionKind::Materialize,
        0.4f,
        false);

    auto disabled = widget::glass_menu_item_button_style(
        effect_glass,
        GlassMenuItemStyleOptions{.disabled = true});
    assert(disabled.disabled);
    assert(!disabled.has_material);
    assert(disabled.material.kind == MaterialKind::None);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::glass_prominent_button<button_test::ButtonMsg>(
        "Primary",
        button_test::Click{},
        effect_glass);
    Scope::set_current(nullptr);

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 1u);
    auto const& wrapped = detail::node_at(root.children[0]);
    assert(wrapped.interaction_role == InteractionRole::Button);
    assert(wrapped.material.prominence.enabled);
    assert_glass_effect_context(
        wrapped.material,
        identity,
        container_id,
        union_id,
        13.0f,
        MaterialGlassTransitionKind::Materialize,
        0.4f,
        false);

    layout::glass_effect_container(
        layout::GlassEffectContainerOptions{
            .container_id = 901u,
            .union_id = 7u,
            .spacing = 15.0f,
            .interactive = false,
            .morph_transitions = true,
        },
        [&] {
            layout::glass_effect_transition(
                layout::glass_matched_geometry_transition(0.6f, true),
                [&] {
                    auto scoped = widget::glass_table_header_button_style(
                        layout::glass_regular()
                            .effect_id("controls", "scoped-header"),
                        GlassTableHeaderStyleOptions{.sorted = true});
                    assert(scoped.has_material);
                    assert_glass_effect_context(
                        scoped.material,
                        layout::glass_effect_identity(
                            "controls",
                            "scoped-header"),
                        901u,
                        7u,
                        15.0f,
                        MaterialGlassTransitionKind::MatchedGeometry,
                        0.6f,
                        true);
                });
        });

    std::puts("PASS: glass effect context reaches control styles");
}

void test_glass_effect_context_reaches_indicator_controls() {
    set_theme(Theme{});

    struct ToggleA {};
    struct PickB { int idx; };
    using Msg = std::variant<ToggleA, PickB>;

    auto const identity =
        layout::glass_effect_identity("indicators", "checkbox");
    auto const container_id =
        layout::glass_effect_stable_id("indicators.scope");
    auto const union_id =
        layout::glass_effect_stable_id("indicators.cluster");
    auto const tint = Color{64, 156, 255, 128};
    auto const border = Color{64, 156, 255, 220};
    auto effect_glass = layout::glass_clear()
        .tint(tint)
        .border(border)
        .effect_id(identity)
        .matched_geometry(0.7f, false)
        .effect_union(
            "indicators.scope",
            "indicators.cluster",
            9.0f,
            false,
            true);

    auto toggle_options = widget::glass_toggle_style_options(
        effect_glass,
        GlassToggleStyleOptions{.role = MaterialSurfaceRole::Toolbar});
    assert(toggle_options.kind == MaterialKind::Clear);
    assert(toggle_options.role == MaterialSurfaceRole::Toolbar);
    assert(toggle_options.has_tint);
    assert(toggle_options.tint == tint);
    assert(toggle_options.has_border);
    assert(toggle_options.border == border);
    assert(toggle_options.has_effect_context);
    assert(toggle_options.transition.kind
           == MaterialGlassTransitionKind::MatchedGeometry);
    assert(std::fabs(toggle_options.transition.progress - 0.7f) < 0.0001f);
    assert(!toggle_options.transition.appearing);
    assert(toggle_options.glass_identity == identity);
    assert(toggle_options.container.container_id == container_id);
    assert(toggle_options.container.union_id == union_id);
    assert(std::fabs(toggle_options.container.spacing - 9.0f) < 0.0001f);

    auto switch_options = widget::glass_switch_style_options(
        effect_glass,
        GlassSwitchStyleOptions{.role = MaterialSurfaceRole::Control});
    assert(switch_options.kind == MaterialKind::Clear);
    assert(switch_options.has_effect_context);
    assert(switch_options.container.container_id == container_id);
    assert(switch_options.container.union_id == union_id);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::glass_checkbox<Msg>(
        "Glass Check",
        true,
        ToggleA{},
        effect_glass,
        GlassToggleStyleOptions{.role = MaterialSurfaceRole::Toolbar});
    widget::glass_radio<Msg>(
        "Glass Radio",
        false,
        PickB{1},
        effect_glass.effect_id("indicators", "radio"));
    widget::glass_switch<Msg>(
        "Glass Switch",
        true,
        ToggleA{},
        effect_glass.effect_id("indicators", "switch"));
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 400.0f);

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 3u);

    auto const& check_row = detail::node_at(root.children[0]);
    auto const& check_box = detail::node_at(check_row.children[0]);
    assert(check_row.interaction_role == InteractionRole::Checkbox);
    assert(check_box.material.kind == MaterialKind::Clear);
    assert(check_box.material.role == MaterialSurfaceRole::Toolbar);
    assert(check_box.material.tint == tint);
    assert(check_box.material.border == border);
    assert(check_box.debug_semantic_label == "Glass Check Indicator");
    assert_glass_effect_context(
        check_box.material,
        identity,
        container_id,
        union_id,
        9.0f,
        MaterialGlassTransitionKind::MatchedGeometry,
        0.7f,
        false);

    auto const& radio_row = detail::node_at(root.children[1]);
    auto const& radio_box = detail::node_at(radio_row.children[0]);
    assert(radio_row.interaction_role == InteractionRole::Radio);
    assert(radio_box.material.kind == MaterialKind::Clear);
    assert(radio_box.material.glass_identity
           == layout::glass_effect_identity("indicators", "radio"));
    assert(radio_box.material.container.container_id == container_id);
    assert(radio_box.material.container.union_id == union_id);
    assert(radio_box.material.transition.kind
           == MaterialGlassTransitionKind::MatchedGeometry);

    auto const& switch_row = detail::node_at(root.children[2]);
    auto const& switch_track = detail::node_at(switch_row.children[0]);
    assert(switch_row.debug_semantic_role == "switch");
    assert(switch_track.material.kind == MaterialKind::Clear);
    assert(switch_track.material.tint == tint);
    assert(switch_track.material.border == border);
    assert(switch_track.material.glass_identity
           == layout::glass_effect_identity("indicators", "switch"));
    assert_glass_effect_context(
        switch_track.material,
        layout::glass_effect_identity("indicators", "switch"),
        container_id,
        union_id,
        9.0f,
        MaterialGlassTransitionKind::MatchedGeometry,
        0.7f,
        false);

    assert(check_row.callback_id != 0xFFFFFFFFu);
    assert(check_row.focusable);
    assert(radio_row.callback_id != 0xFFFFFFFFu);
    assert(radio_row.focusable);
    assert(switch_row.callback_id != 0xFFFFFFFFu);
    assert(switch_row.focusable);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();

    auto scoped_root_h = detail::alloc_node();
    detail::node_at(scoped_root_h).style.flex_direction = FlexDirection::Column;
    Scope scoped_scope(scoped_root_h);
    Scope::set_current(&scoped_scope);
    layout::glass_effect_container(
        layout::GlassEffectContainerOptions{
            .container_id = 704u,
            .union_id = 22u,
            .spacing = 14.0f,
            .interactive = false,
            .morph_transitions = true,
        },
        [&] {
            layout::glass_effect_transition(
                layout::glass_materialize_transition(0.35f, true),
                [&] {
                    widget::glass_checkbox<Msg>(
                        "Scoped Check",
                        true,
                        ToggleA{},
                        layout::glass_regular()
                            .effect_id("indicators", "scoped"));
                });
        });
    Scope::set_current(nullptr);

    auto const& scoped_root = detail::node_at(scoped_root_h);
    assert(scoped_root.children.size() == 1u);
    auto const& scoped_row = detail::node_at(scoped_root.children[0]);
    auto const& scoped_box = detail::node_at(scoped_row.children[0]);
    assert(scoped_box.material.kind == MaterialKind::Regular);
    assert_glass_effect_context(
        scoped_box.material,
        layout::glass_effect_identity("indicators", "scoped"),
        704u,
        22u,
        14.0f,
        MaterialGlassTransitionKind::Materialize,
        0.35f,
        true);

    std::puts("PASS: glass effect context reaches indicator controls");
}

void test_glass_effect_context_reaches_tabs() {
    set_theme(Theme{});

    struct Select { std::size_t index; };
    using Msg = std::variant<Select>;

    auto const pill_identity =
        layout::glass_effect_identity("tabs", "pill");
    auto const indicator_identity =
        layout::glass_effect_identity("tabs", "selection");
    auto const container_id =
        layout::glass_effect_stable_id("tabs.scope");
    auto const union_id =
        layout::glass_effect_stable_id("tabs.cluster");
    auto const tint = Color{64, 156, 255, 96};
    auto const border = Color{64, 156, 255, 160};
    auto effect_glass = layout::glass_regular()
        .tint(tint)
        .border(border)
        .effect_id(pill_identity)
        .matched_geometry(0.55f, false)
        .effect_union(
            "tabs.scope",
            "tabs.cluster",
            17.0f,
            true,
            true);

    TabsStyleOptions tabs_options{};
    tabs_options.indicator_glass_identity = indicator_identity;
    tabs_options = widget::glass_tabs_style_options(
        effect_glass,
        tabs_options);
    assert(tabs_options.kind == MaterialKind::Regular);
    assert(tabs_options.has_tint);
    assert(tabs_options.tint == tint);
    assert(tabs_options.has_border);
    assert(tabs_options.border == border);
    assert(tabs_options.has_effect_context);
    assert(tabs_options.glass_identity == pill_identity);
    assert(tabs_options.indicator_glass_identity == indicator_identity);
    assert(tabs_options.container.container_id == container_id);
    assert(tabs_options.container.union_id == union_id);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = 1u;
    detail::g_app.focused_id = 1u;
    detail::g_app.pressed_id = 1u;
    detail::g_app.focus_visible = true;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    std::vector<str> items;
    items.emplace_back("Overview", 8u);
    items.emplace_back("Settings", 8u);
    items.emplace_back("Activity", 8u);
    widget::tabs<Msg>(
        items,
        1u,
        [](std::size_t index) -> Msg { return Select{index}; },
        effect_glass,
        TabsStyleOptions{.indicator_glass_identity = indicator_identity});
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 420.0f);

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 1u);
    auto const& pill = detail::node_at(root.children[0]);
    assert(pill.material.kind == MaterialKind::Regular);
    assert(pill.material.role == MaterialSurfaceRole::Navigation);
    assert(pill.material.tint == tint);
    assert(pill.material.border == border);
    assert(pill.material_shape == MaterialSurfaceShape::Capsule);
    assert(pill.debug_semantic_label == "Segmented Control");
    assert_glass_effect_context(
        pill.material,
        pill_identity,
        container_id,
        union_id,
        17.0f,
        MaterialGlassTransitionKind::MatchedGeometry,
        0.55f,
        false);

    assert(pill.children.size() == 2u);
    auto const& row = detail::node_at(pill.children[0]);
    assert(row.children.size() == 3u);
    auto const& first_tab = detail::node_at(row.children[0]);
    auto const& selected_tab = detail::node_at(row.children[1]);
    auto const& third_tab = detail::node_at(row.children[2]);
    assert(first_tab.material.kind == MaterialKind::None);
    assert(third_tab.material.kind == MaterialKind::None);
    assert(selected_tab.debug_semantic_role == "tab");
    assert(selected_tab.debug_semantic_label == "Settings");
    assert(selected_tab.material.kind == MaterialKind::Clear);
    assert(selected_tab.material.role == MaterialSurfaceRole::Navigation);
    assert(selected_tab.material_shape == MaterialSurfaceShape::Capsule);
    assert_glass_effect_context(
        selected_tab.material,
        indicator_identity,
        container_id,
        union_id,
        17.0f,
        MaterialGlassTransitionKind::MatchedGeometry,
        0.55f,
        false);
    assert_material_interaction_state(
        selected_tab.material,
        true,
        true,
        true,
        true);

    auto const& track = detail::node_at(pill.children[1]);
    assert(track.children.size() == 3u);
    auto const& indicator = detail::node_at(track.children[1]);
    auto const indicator_tint = Color{64, 156, 255, 206};
    assert(indicator.background == indicator_tint);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = 0xFFFFFFFFu;
    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.pressed_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;

    auto scoped_root_h = detail::alloc_node();
    detail::node_at(scoped_root_h).style.flex_direction = FlexDirection::Column;
    Scope scoped_scope(scoped_root_h);
    Scope::set_current(&scoped_scope);
    layout::glass_effect_container(
        layout::GlassEffectContainerOptions{
            .container_id = 812u,
            .union_id = 31u,
            .spacing = 19.0f,
            .interactive = false,
            .morph_transitions = true,
        },
        [&] {
            layout::glass_effect_transition(
                layout::glass_materialize_transition(0.25f, true),
                [&] {
                    std::vector<str> scoped_items;
                    scoped_items.emplace_back("One", 3u);
                    scoped_items.emplace_back("Two", 3u);
                    widget::glass_tabs<Msg>(
                        scoped_items,
                        0u,
                        [](std::size_t index) -> Msg {
                            return Select{index};
                        },
                        layout::glass_clear()
                            .effect_id("tabs", "scoped-pill"),
                        TabsStyleOptions{
                            .indicator_glass_identity =
                                layout::glass_effect_identity(
                                    "tabs",
                                    "scoped-selection"),
                        });
                });
        });
    Scope::set_current(nullptr);

    auto const& scoped_root = detail::node_at(scoped_root_h);
    assert(scoped_root.children.size() == 1u);
    auto const& scoped_pill = detail::node_at(scoped_root.children[0]);
    assert_glass_effect_context(
        scoped_pill.material,
        layout::glass_effect_identity("tabs", "scoped-pill"),
        812u,
        31u,
        19.0f,
        MaterialGlassTransitionKind::Materialize,
        0.25f,
        true);
    auto const& scoped_row = detail::node_at(scoped_pill.children[0]);
    auto const& scoped_selected = detail::node_at(scoped_row.children[0]);
    assert_glass_effect_context(
        scoped_selected.material,
        layout::glass_effect_identity("tabs", "scoped-selection"),
        812u,
        31u,
        19.0f,
        MaterialGlassTransitionKind::Materialize,
        0.25f,
        true);

    std::puts("PASS: glass effect context reaches tabs");
}

void test_symbol_button_minimum_hit_region_contract() {
    icons::SymbolButtonOptions options;
    options.role = icons::SymbolPresentationRole::Toolbar;

    auto btn_h = button_test::build_symbol_button_with_options(options);
    LAYOUT_NODE(btn_h, 100.0f);
    auto const& btn = detail::node_at(btn_h);
    float const expected_slop_x =
        (icons::activation_hit_target_size(options.role) - btn.width) * 0.5f;
    float const expected_slop_y =
        (icons::activation_hit_target_size(options.role) - btn.height) * 0.5f;
    CMD_LEN = 0;
    PAINT_NODE(btn_h, 0, 0, 0, 100.0f);

    bool found = false;
    for (unsigned int i = 0; i + 28 <= CMD_LEN; i += 4) {
        unsigned int op;
        std::memcpy(&op, &CMD_BUF[i], 4);
        if (op != static_cast<unsigned int>(Cmd::HitRegion))
            continue;
        float hx = 0.0f;
        float hy = 0.0f;
        float hw = 0.0f;
        float hh = 0.0f;
        std::memcpy(&hx, &CMD_BUF[i + 4], 4);
        std::memcpy(&hy, &CMD_BUF[i + 8], 4);
        std::memcpy(&hw, &CMD_BUF[i + 12], 4);
        std::memcpy(&hh, &CMD_BUF[i + 16], 4);
        assert(hx == btn.x - expected_slop_x);
        assert(hy == btn.y - expected_slop_y);
        assert(hw == icons::activation_hit_target_size(options.role));
        assert(hh == icons::activation_hit_target_size(options.role));
        found = true;
        break;
    }
    assert(found);
    std::puts("PASS: symbol_button minimum hit region contract");
}

void test_symbol_button_visual_state_token_contract() {
    icons::SymbolButtonOptions options;
    options.role = icons::SymbolPresentationRole::Toolbar;
    options.token_salt = 0x51A7Eu;

    auto normal_h = button_test::build_symbol_button_with_options(options);
    auto const normal_token =
        detail::node_at(detail::node_at(normal_h).children[0]).paint_token;

    auto pressed_h = button_test::build_symbol_button_with_options(
        options,
        /*hovered_id=*/0u,
        /*focused_id=*/0xFFFFFFFFu,
        /*pressed_id=*/0u);
    auto& pressed_btn = detail::node_at(pressed_h);
    auto const pressed_token =
        detail::node_at(pressed_btn.children[0]).paint_token;
    assert(pressed_btn.background.a == 150);
    assert(pressed_token != normal_token);

    detail::g_app.hovered_id = 0xFFFFFFFFu;
    detail::g_app.pressed_id = 0xFFFFFFFFu;
    std::puts("PASS: symbol_button visual-state paint token contract");
}

void test_symbol_button_disabled_contract() {
    icons::SymbolButtonOptions options;
    options.role = icons::SymbolPresentationRole::Navigation;
    options.disabled = true;

    auto btn_h = button_test::build_symbol_button_with_options(options);
    auto& btn = detail::node_at(btn_h);
    assert(btn.style.max_width == 36.0f);
    assert(btn.style.fixed_height == 36.0f);
    assert(btn.interaction_role == InteractionRole::Button);
    assert(btn.debug_semantic_label == "Grid View");
    assert(btn.debug_semantic_enabled == false);
    assert(btn.callback_id == 0xFFFFFFFFu);
    assert(btn.focusable == false);
    assert(detail::g_app.callbacks.empty());
    assert(btn.children.size() == 1);

    auto& canvas = detail::node_at(btn.children[0]);
    assert(canvas.paint_token != 0);
    std::puts("PASS: symbol_button disabled contract");
}

namespace text_field_test {
struct Changed { std::string text; };
using TfMsg = std::variant<Changed>;
inline TfMsg make_changed(std::string s) { return Changed{std::move(s)}; }

inline NodeHandle build_text_field_with_options(
        std::string const& current,
        TextFieldStyleOptions options,
        unsigned int focused_id = 0xFFFFFFFFu,
        bool focus_visible = false,
        unsigned int hovered_id = 0xFFFFFFFFu,
        unsigned int pressed_id = 0xFFFFFFFFu) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.input_handlers.clear();
    detail::g_app.input_nodes.clear();
    detail::msg_queue().clear();
    // Wipe per-call-site animation state so the first `animate_float`
    // / `animate_color` inside `widget::text_field` snaps to its
    // target instead of inheriting the previous test's interpolation
    // (default → error swap would otherwise return mid-fade colours).
    detail::local_store().clear();
    detail::bump_local_gen();
    detail::g_app.hovered_id = hovered_id;
    detail::g_app.focused_id = focused_id;
    detail::g_app.pressed_id = pressed_id;
    detail::g_app.focus_visible = focus_visible;

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::text_field<TfMsg>("Name", current, &make_changed, options);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}

inline NodeHandle build_text_field(std::string const& current,
                                   bool error,
                                   bool disabled,
                                   unsigned int focused_id = 0xFFFFFFFFu,
                                   bool focus_visible = false) {
    TextFieldStyleOptions options;
    options.error = error;
    options.disabled = disabled;
    return build_text_field_with_options(
        current,
        options,
        focused_id,
        focus_visible);
}
} // namespace text_field_test

void test_text_field_default() {
    auto h = text_field_test::build_text_field("hello", /*error=*/false, /*disabled=*/false);
    auto& f = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    assert(f.background.r == t.surface.r);
    assert(f.text_color.r == t.foreground.r); // current non-empty -> foreground
    assert(f.border_color.r == t.border.r);
    assert(f.border_width == 1);
    assert(f.border_radius == t.radius_sm);
    assert(f.cursor_type == 1);
    assert(f.focusable == true);
    assert(f.is_input == true);
    assert(f.callback_id != 0xFFFFFFFFu);
    assert(f.interaction_role == InteractionRole::TextField);
    assert(detail::g_app.input_handlers.size() == 1);
    std::puts("PASS: text_field default state");
}

void test_text_field_default_placeholder() {
    auto h = text_field_test::build_text_field("", /*error=*/false, /*disabled=*/false);
    auto& f = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    assert(f.text_color.r == t.muted.r); // empty -> muted placeholder color
    assert(f.text == f.placeholder);
    std::puts("PASS: text_field default placeholder color");
}

void test_text_field_error() {
    auto h = text_field_test::build_text_field("oops", /*error=*/true, /*disabled=*/false);
    auto& f = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    assert(f.background.r == t.state_error_bg.r);
    assert(f.text_color.r == t.state_error_fg.r);
    assert(f.border_color.r == t.state_error_border.r);
    assert(f.is_input == true);     // error stays interactive
    assert(f.focusable == true);
    assert(f.cursor_type == 1);
    assert(f.callback_id != 0xFFFFFFFFu);
    std::puts("PASS: text_field error state");
}

void test_text_field_disabled() {
    auto h = text_field_test::build_text_field("anything", /*error=*/false, /*disabled=*/true);
    auto& f = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    assert(f.background.r == t.state_disabled_bg.r);
    assert(f.text_color.r == t.state_disabled_fg.r);
    assert(f.border_color.r == t.state_disabled_border.r);
    assert(f.is_input == false);
    assert(f.focusable == false);
    assert(f.cursor_type == 0);
    assert(f.callback_id == 0xFFFFFFFFu);
    // Disabled fields skip the input-handler registration entirely.
    assert(detail::g_app.input_handlers.empty());
    std::puts("PASS: text_field disabled state");
}

void test_text_field_focus_ring_uses_keyboard_modality() {
    auto const& t = detail::g_app.theme;
    auto keyboard_h = text_field_test::build_text_field(
        "hello",
        /*error=*/false,
        /*disabled=*/false,
        /*focused_id=*/0u,
        /*focus_visible=*/true);
    auto& keyboard = detail::node_at(keyboard_h);
    assert(keyboard.callback_id == 0u);
    assert(keyboard.border_width == t.state_focus_ring_width);
    assert(keyboard.border_color.r == t.state_focus_ring.r);

    auto pointer_h = text_field_test::build_text_field(
        "hello",
        /*error=*/false,
        /*disabled=*/false,
        /*focused_id=*/0u,
        /*focus_visible=*/false);
    auto& pointer = detail::node_at(pointer_h);
    assert(pointer.callback_id == 0u);
    assert(pointer.border_width == 1.0f);
    assert(pointer.border_color.r == t.border.r);

    detail::g_app.focused_id = 0xFFFFFFFFu;
    detail::g_app.focus_visible = false;
    std::puts("PASS: text_field focus ring uses keyboard modality");
}

void test_glass_text_field_style_material_contract() {
    auto style = widget::glass_text_field_style(GlassTextFieldStyleOptions{
        .kind = MaterialKind::Regular,
        .role = MaterialSurfaceRole::Toolbar,
        .width = 220.0f,
        .height = 34.0f,
        .semantic_label = "Search Field",
    });
    assert(style.has_material);
    assert(style.material.kind == MaterialKind::Regular);
    assert(style.material.role == MaterialSurfaceRole::Toolbar);
    assert(style.material.fallback);
    assert(style.material.container.interactive);
    assert(style.has_background);
    assert(style.has_border_color);
    assert(style.border_width == 0.0f);
    assert(style.max_width == 220.0f);
    assert(style.fixed_height == 34.0f);
    assert(style.shape == MaterialSurfaceShape::Capsule);

    auto h = text_field_test::build_text_field_with_options(
        "query",
        style,
        /*focused_id=*/0u,
        /*focus_visible=*/false);
    auto& f = detail::node_at(h);
    assert(f.callback_id == 0u);
    assert(f.is_input);
    assert(f.focusable);
    assert(f.interaction_role == InteractionRole::TextField);
    assert(f.material.kind == MaterialKind::Regular);
    assert(f.material.role == MaterialSurfaceRole::Toolbar);
    assert(f.material.container.interactive);
    assert(f.material.tint == f.background);
    assert(f.material.border == f.border_color);
    assert(f.material.foreground == f.text_color);
    assert(f.material_shape == MaterialSurfaceShape::Capsule);
    assert(f.border_width == 0.0f);
    assert(f.style.max_width == 220.0f);
    assert(f.style.fixed_height == 34.0f);
    assert(f.debug_semantic_label == "Search Field");

    auto const field_identity =
        layout::glass_effect_identity("fields", "search");
    auto const field_container =
        layout::glass_effect_stable_id("fields.scope");
    auto const field_union =
        layout::glass_effect_stable_id("fields.cluster");
    auto effect_style = widget::glass_text_field_style(
        layout::glass_regular()
            .effect_id(field_identity)
            .matched_geometry(0.75f, true)
            .effect_union("fields.scope", "fields.cluster", 18.0f),
        GlassTextFieldStyleOptions{
            .kind = MaterialKind::Regular,
            .role = MaterialSurfaceRole::Toolbar,
            .width = 240.0f,
            .height = 36.0f,
            .semantic_label = "Effect Search",
        });
    assert(effect_style.has_material);
    assert(effect_style.max_width == 240.0f);
    assert(effect_style.fixed_height == 36.0f);
    assert_glass_effect_context(
        effect_style.material,
        field_identity,
        field_container,
        field_union,
        18.0f,
        MaterialGlassTransitionKind::MatchedGeometry,
        0.75f,
        true);

    auto effect_h = text_field_test::build_text_field_with_options(
        "effect query",
        effect_style,
        /*focused_id=*/0u,
        /*focus_visible=*/true,
        /*hovered_id=*/0u,
        /*pressed_id=*/0u);
    auto& effect_field = detail::node_at(effect_h);
    assert(effect_field.is_input);
    assert(effect_field.focusable);
    assert(effect_field.material.kind == MaterialKind::Regular);
    assert(effect_field.material.role == MaterialSurfaceRole::Toolbar);
    assert(effect_field.material_shape == MaterialSurfaceShape::Capsule);
    assert(effect_field.debug_semantic_label == "Effect Search");
    assert_glass_effect_context(
        effect_field.material,
        field_identity,
        field_container,
        field_union,
        18.0f,
        MaterialGlassTransitionKind::MatchedGeometry,
        0.75f,
        true);
    assert_material_interaction_state(
        effect_field.material,
        true,
        true,
        true,
        true);

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.input_handlers.clear();
    detail::g_app.input_nodes.clear();
    detail::msg_queue().clear();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::glass_text_field<text_field_test::TfMsg>(
        "Effect Search",
        std::string{"query"},
        &text_field_test::make_changed,
        layout::glass_regular()
            .effect_id(field_identity)
            .matched_geometry(0.75f, true)
            .effect_union("fields.scope", "fields.cluster", 18.0f),
        GlassTextFieldStyleOptions{
            .role = MaterialSurfaceRole::Toolbar,
            .width = 240.0f,
            .height = 36.0f,
            .semantic_label = "Effect Search",
        });
    Scope::set_current(nullptr);

    auto const& root = detail::node_at(root_h);
    assert(root.children.size() == 1u);
    auto const& wrapped_field = detail::node_at(root.children[0]);
    assert(wrapped_field.interaction_role == InteractionRole::TextField);
    assert(wrapped_field.is_input);
    assert(wrapped_field.focusable);
    assert_glass_effect_context(
        wrapped_field.material,
        field_identity,
        field_container,
        field_union,
        18.0f,
        MaterialGlassTransitionKind::MatchedGeometry,
        0.75f,
        true);

    auto disabled_style = widget::glass_text_field_style(
        GlassTextFieldStyleOptions{
            .kind = MaterialKind::Thin,
            .role = MaterialSurfaceRole::Navigation,
            .disabled = true,
            .semantic_label = "Disabled Search",
        });
    auto disabled_h = text_field_test::build_text_field_with_options(
        "locked",
        disabled_style);
    auto& disabled = detail::node_at(disabled_h);
    assert(disabled.material.kind == MaterialKind::Thin);
    assert(disabled.material.role == MaterialSurfaceRole::Navigation);
    assert(!disabled.material.container.interactive);
    assert(disabled.material_shape == MaterialSurfaceShape::Capsule);
    assert(!disabled.focusable);
    assert(!disabled.is_input);
    assert(disabled.debug_semantic_label == "Disabled Search");

    std::puts("PASS: glass text field style emits material contract");
}

namespace text_variant_test {
inline NodeHandle build_text(str content, TextSize size, TextColor color) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::text(content, size, color);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
}
} // namespace text_variant_test

void test_text_size_variants() {
    auto const& t = detail::g_app.theme;
    auto& body = detail::node_at(text_variant_test::build_text(
        "body", TextSize::Body, TextColor::Default));
    assert(body.font_size == t.body_font_size);
    assert(body.mono == false);

    auto& heading = detail::node_at(text_variant_test::build_text(
        "heading", TextSize::Heading, TextColor::Default));
    assert(heading.font_size == t.heading_font_size);

    auto& small = detail::node_at(text_variant_test::build_text(
        "small", TextSize::Small, TextColor::Default));
    assert(small.font_size == t.small_font_size);

    auto& hero_title = detail::node_at(text_variant_test::build_text(
        "title", TextSize::HeroTitle, TextColor::Default));
    assert(hero_title.font_size == t.hero_title_size);

    auto& hero_sub = detail::node_at(text_variant_test::build_text(
        "subtitle", TextSize::HeroSubtitle, TextColor::Default));
    assert(hero_sub.font_size == t.hero_subtitle_size);

    std::puts("PASS: text size variants pick the right typography token");
}

void test_text_color_variants() {
    auto const& t = detail::g_app.theme;
    auto& def = detail::node_at(text_variant_test::build_text(
        "x", TextSize::Body, TextColor::Default));
    assert(def.text_color.r == t.foreground.r);

    auto& muted = detail::node_at(text_variant_test::build_text(
        "x", TextSize::Body, TextColor::Muted));
    assert(muted.text_color.r == t.muted.r);

    auto& accent = detail::node_at(text_variant_test::build_text(
        "x", TextSize::Body, TextColor::Accent));
    assert(accent.text_color.r == t.accent.r);

    auto& hero_fg = detail::node_at(text_variant_test::build_text(
        "x", TextSize::Body, TextColor::HeroFg));
    assert(hero_fg.text_color.r == t.hero_fg.r);

    auto& hero_muted = detail::node_at(text_variant_test::build_text(
        "x", TextSize::Body, TextColor::HeroMuted));
    assert(hero_muted.text_color.r == t.hero_muted.r);

    std::puts("PASS: text color variants pick the right color token");
}

void test_text_inline_code_chrome() {
    auto const& t = detail::g_app.theme;
    auto& code = detail::node_at(text_variant_test::build_text(
        "fn()", TextSize::Code, TextColor::Default));
    assert(code.font_size == t.code_font_size);
    assert(code.mono == true);
    assert(code.background.r == t.code_bg.r);
    assert(code.border_color.r == t.border.r);
    assert(code.border_width == 1);
    assert(code.border_radius == t.radius_md);
    assert(code.style.padding[0] == t.space_xs);
    assert(code.style.padding[1] == t.space_sm);
    assert(code.style.padding[2] == t.space_xs);
    assert(code.style.padding[3] == t.space_sm);
    std::puts("PASS: text(Code) inline chrome (mono + border + xs/sm padding)");
}

void test_space_value_resolves_each_token() {
    auto const& t = detail::g_app.theme;
    assert(layout::space_value(SpaceToken::Xs)  == t.space_xs);
    assert(layout::space_value(SpaceToken::Sm)  == t.space_sm);
    assert(layout::space_value(SpaceToken::Md)  == t.space_md);
    assert(layout::space_value(SpaceToken::Lg)  == t.space_lg);
    assert(layout::space_value(SpaceToken::Xl)  == t.space_xl);
    assert(layout::space_value(SpaceToken::Xl2) == t.space_2xl);
    std::puts("PASS: space_value resolves every SpaceToken rung");
}

int main() {
    test_button_default_variant();
    test_button_primary_variant();
    test_button_default_hovered_snaps_to_hover_bg();
    test_button_pressed_snaps_to_pressed_bg();
    test_button_primary_hovered_snaps_to_hover_bg();
    test_button_focused_snaps_to_focus_ring_width();
    test_button_pointer_focused_hides_focus_ring();
    test_button_pointer_focus_reset_snaps_focus_ring_off();
    test_focus_visible_tracks_keyboard_modality();
    test_button_defocused_resting_border_width();
    test_button_disabled();
    test_button_disabled_custom_chrome();
    test_button_style_options_custom_chrome();
    test_canvas_button_semantic_and_layout_contract();
    test_canvas_button_minimum_hit_region_contract();
    test_canvas_button_focus_visible_contract();
    test_canvas_button_visual_state_contract();
    test_canvas_button_disabled_contract();
    test_canvas_button_disabled_custom_chrome();
    test_symbol_button_macos_contract();
    test_macos_control_button_style_contract();
    test_glass_control_button_style_material_contract();
    test_glass_button_accepts_configurable_glass_style();
    test_glass_prominent_button_style_material_contract();
    test_glass_split_button_style_material_contract();
    test_glass_selection_button_style_material_contract();
    test_glass_outline_row_button_style_material_contract();
    test_glass_menu_item_symbol_button_material_contract();
    test_glass_table_header_button_material_contract();
    test_glass_disclosure_header_style_material_contract();
    test_glass_widget_helpers_emit_material_buttons();
    test_glass_effect_context_reaches_control_styles();
    test_glass_effect_context_reaches_indicator_controls();
    test_glass_effect_context_reaches_tabs();
    test_symbol_button_minimum_hit_region_contract();
    test_symbol_button_visual_state_token_contract();
    test_symbol_button_disabled_contract();
    test_text_field_default();
    test_text_field_default_placeholder();
    test_text_field_error();
    test_text_field_disabled();
    test_text_field_focus_ring_uses_keyboard_modality();
    test_glass_text_field_style_material_contract();
    test_text_size_variants();
    test_text_color_variants();
    test_text_inline_code_chrome();
    test_space_value_resolves_each_token();
    std::puts("\nAll phenotype widget style tests passed.");
    return 0;
}

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <variant>
#include <vector>
import phenotype;

using namespace phenotype;

#ifndef __wasi__
static null_host host;
#define RUN_APP(S, M, V, U)              run<S, M>(host, V, U)
#define LAYOUT_NODE(h, w)                detail::layout_node(host, h, w)
#define PAINT_NODE(h, ox, oy, sy, vh)    detail::paint_node(host, host, h, ox, oy, sy, vh)
#define CMD_BUF                          host.buf()
#define CMD_LEN                          host.buf_len()
#else
extern "C" {
    extern unsigned char phenotype_cmd_buf[];
    extern unsigned int phenotype_cmd_len;
    // Stubs for WASM host imports — wasmtime has no JS shim.
    void phenotype_flush() {}
    float phenotype_measure_text(float fs, unsigned int, char const*, unsigned int len) {
        return static_cast<float>(len) * fs * 0.6f;
    }
    float phenotype_get_canvas_width() { return 800.0f; }
    float phenotype_get_canvas_height() { return 600.0f; }
    void phenotype_open_url(char const*, unsigned int) {}
}
#define RUN_APP(S, M, V, U)              run<S, M>(V, U)
#define LAYOUT_NODE(h, w)                detail::layout_node(h, w)
#define PAINT_NODE(h, ox, oy, sy, vh)    detail::wasi_paint_node(h, ox, oy, sy, vh)
#define CMD_BUF                          phenotype_cmd_buf
#define CMD_LEN                          phenotype_cmd_len
#endif

// ============================================================
// Layout tests
// ============================================================

void test_column_layout() {
    detail::g_app.arena.reset();
    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;
    root.style.gap = 10;
    root.style.padding[0] = 5;
    root.style.padding[2] = 5;

    auto a_h = detail::alloc_node();
    auto& a = detail::node_at(a_h);
    a.text = "hello";
    a.font_size = 16.0f;
    detail::node_at(root_h).children.push_back(a_h);

    auto b_h = detail::alloc_node();
    auto& b = detail::node_at(b_h);
    b.text = "world";
    b.font_size = 16.0f;
    detail::node_at(root_h).children.push_back(b_h);

    LAYOUT_NODE(root_h, 400.0f);

    assert(root.width == 400.0f);
    assert(a.y == 5.0f);
    assert(b.y > a.y + a.height);
    assert(a.height > 0);
    assert(b.height > 0);
    std::puts("PASS: column layout");
}

void test_row_intrinsic_width() {
    detail::g_app.arena.reset();
    auto row_h = detail::alloc_node();
    auto& row = detail::node_at(row_h);
    row.style.flex_direction = FlexDirection::Row;
    row.style.gap = 8;

    auto bullet_h = detail::alloc_node();
    auto& bullet = detail::node_at(bullet_h);
    bullet.text = "*";
    bullet.font_size = 16.0f;
    detail::node_at(row_h).children.push_back(bullet_h);

    auto text_h = detail::alloc_node();
    auto& text = detail::node_at(text_h);
    text.text = "List item text here";
    text.font_size = 16.0f;
    detail::node_at(row_h).children.push_back(text_h);

    LAYOUT_NODE(row_h, 400.0f);

    assert(bullet.width < 50.0f);
    assert(text.width > bullet.width);
    assert(text.x > bullet.x + bullet.width);
    std::puts("PASS: row intrinsic width");
}

void test_row_wraps_last_text_leaf() {
    detail::g_app.arena.reset();
    auto row_h = detail::alloc_node();
    auto& row = detail::node_at(row_h);
    row.style.flex_direction = FlexDirection::Row;
    row.style.gap = 8;

    auto bullet_h = detail::alloc_node();
    auto& bullet = detail::node_at(bullet_h);
    bullet.text = "*";
    bullet.font_size = 16.0f;
    detail::node_at(row_h).children.push_back(bullet_h);

    auto text_h = detail::alloc_node();
    auto& text = detail::node_at(text_h);
    text.text = "List item text that should wrap inside a narrow card row";
    text.font_size = 16.0f;
    detail::node_at(row_h).children.push_back(text_h);

    LAYOUT_NODE(row_h, 120.0f);

    assert(text.text_lines.size() > 1);
    assert(text.x > bullet.x + bullet.width);
    assert(text.width <= 120.0f - bullet.width - row.style.gap + 0.01f);
    std::puts("PASS: row wraps last text leaf");
}

void test_containment_invariant() {
    detail::g_app.arena.reset();
    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;
    root.style.padding[0] = 10;
    root.style.padding[1] = 10;
    root.style.padding[2] = 10;
    root.style.padding[3] = 10;

    for (int i = 0; i < 20; ++i) {
        auto child_h = detail::alloc_node();
        auto& child = detail::node_at(child_h);
        child.text = "test item " + std::to_string(i);
        child.font_size = 16.0f;
        detail::node_at(root_h).children.push_back(child_h);
    }

    LAYOUT_NODE(root_h, 300.0f);

    for (auto child_h : detail::node_at(root_h).children) {
        auto& child = detail::node_at(child_h);
        assert(child.x >= root.style.padding[3]);
        assert(child.x + child.width <= root.width - root.style.padding[1]);
        assert(child.height > 0);
    }
    std::puts("PASS: containment invariant");
}

void test_alignment_center() {
    detail::g_app.arena.reset();
    auto col_h = detail::alloc_node();
    auto& col = detail::node_at(col_h);
    col.style.flex_direction = FlexDirection::Column;
    col.style.cross_align = CrossAxisAlignment::Center;

    auto inner_h = detail::alloc_node();
    auto& inner = detail::node_at(inner_h);
    inner.style.max_width = 100;
    inner.style.fixed_height = 20;
    detail::node_at(col_h).children.push_back(inner_h);

    LAYOUT_NODE(col_h, 400.0f);

    assert(inner.width <= 100.0f);
    assert(inner.x > 0);
    std::puts("PASS: cross-axis center alignment");
}

void test_max_width_centering() {
    detail::g_app.arena.reset();
    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    auto child_h = detail::alloc_node();
    auto& child = detail::node_at(child_h);
    child.style.max_width = 200;
    child.style.fixed_height = 30;
    detail::node_at(root_h).children.push_back(child_h);

    LAYOUT_NODE(root_h, 800.0f);

    assert(child.width <= 200.0f);
    assert(child.x >= 0);
    (void)root;
    std::puts("PASS: max-width centering");
}

// ============================================================
// Text layout tests
// ============================================================

void test_word_wrap() {
    detail::g_app.arena.reset();
    auto node_h = detail::alloc_node();
    auto& node = detail::node_at(node_h);
    node.text = "hello world this is a longer text that should wrap";
    node.font_size = 16.0f;

    LAYOUT_NODE(node_h, 100.0f);

    assert(!node.text_lines.empty());
    assert(node.text_lines.size() > 1);
    assert(node.height > 0);
    std::puts("PASS: word wrap");
}

void test_newline_handling() {
    detail::g_app.arena.reset();
    auto node_h = detail::alloc_node();
    auto& node = detail::node_at(node_h);
    node.text = "line1\nline2\nline3";
    node.font_size = 16.0f;

    LAYOUT_NODE(node_h, 800.0f);

    assert(node.text_lines.size() == 3);
    assert(node.text_lines[0] == "line1");
    assert(node.text_lines[1] == "line2");
    assert(node.text_lines[2] == "line3");
    std::puts("PASS: newline handling");
}

void test_measure_text_cache_dedup() {
    detail::g_app.arena.reset();
    metrics::reset_all();
    detail::clear_measure_cache();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    for (int i = 0; i < 5; ++i) {
        auto h = detail::alloc_node();
        auto& n = detail::node_at(h);
        n.text = "hello world hello";
        n.font_size = 16.0f;
        detail::node_at(root_h).children.push_back(h);
    }

    LAYOUT_NODE(root_h, 800.0f);

    auto host_calls_first = metrics::inst::measure_text_calls.total();
    auto cache_hits_first = metrics::inst::measure_text_cache_hits.total();
    assert(host_calls_first > 0);
    assert(host_calls_first < 10);
    assert(cache_hits_first > host_calls_first);

    metrics::inst::measure_text_calls.reset();
    metrics::inst::measure_text_cache_hits.reset();
    LAYOUT_NODE(root_h, 800.0f);
    assert(metrics::inst::measure_text_calls.total() == 0);
    assert(metrics::inst::measure_text_cache_hits.total() > 0);

    std::puts("PASS: measure_text cache deduplicates across rebuilds");
}

void test_set_theme_updates_and_invalidates_cache() {
    detail::g_app.arena.reset();
    metrics::reset_all();
    detail::clear_measure_cache();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;
    auto leaf_h = detail::alloc_node();
    auto& leaf = detail::node_at(leaf_h);
    leaf.text = "theme demo";
    leaf.font_size = current_theme().body_font_size;
    detail::node_at(root_h).children.push_back(leaf_h);
    LAYOUT_NODE(root_h, 400.0f);
    assert(metrics::inst::measure_text_cache_hits.total()
           + metrics::inst::measure_text_calls.total() > 0);

    Theme dark{};
    dark.background = {10,  10,  10, 255};
    dark.foreground = {240, 240, 240, 255};
    dark.body_font_size = 18.0f;
    set_theme(dark);

    assert(current_theme().body_font_size == 18.0f);
    assert(current_theme().background.r == 10);
    assert(current_theme().foreground.r == 240);

    metrics::inst::measure_text_calls.reset();
    metrics::inst::measure_text_cache_hits.reset();
    LAYOUT_NODE(root_h, 400.0f);
    assert(metrics::inst::measure_text_calls.total() > 0);

    set_theme(Theme{});

    std::puts("PASS: set_theme swaps theme and invalidates measure cache");
}

void test_sized_box_in_row() {
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::row([&] {
        layout::sized_box(200.0f, [&] {
            widget::text("left");
        });
        layout::sized_box(300.0f, [&] {
            widget::text("right");
        });
    });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 800.0f);

    assert(root.children.size() == 1);
    auto& row = detail::node_at(root.children[0]);
    assert(row.children.size() == 2);

    auto& left = detail::node_at(row.children[0]);
    auto& right = detail::node_at(row.children[1]);

    assert(left.width == 200.0f);
    assert(right.width == 300.0f);
    assert(right.x >= left.x + left.width);

    std::puts("PASS: sized_box honors max_width inside row layout");
}

void test_image_widget_layout_and_emit() {
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Row;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::image({"logo.png", 8}, 64.0f, 48.0f);
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 200.0f);

    assert(root.children.size() == 1);
    auto& img = detail::node_at(root.children[0]);
    assert(img.image_url == std::string("logo.png"));
    assert(img.width == 64.0f);
    assert(img.height == 48.0f);

    CMD_LEN = 0;
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    bool found_opcode = false;
    bool found_url = false;
    for (unsigned int i = 0; i + 4 <= CMD_LEN; i += 4) {
        unsigned int word;
        std::memcpy(&word, &CMD_BUF[i], 4);
        if (word == static_cast<unsigned int>(Cmd::DrawImage)) {
            found_opcode = true;
        }
    }
    char const* needle = "logo.png";
    for (unsigned int i = 0; i + 8 <= CMD_LEN; ++i) {
        if (std::memcmp(&CMD_BUF[i], needle, 8) == 0) {
            found_url = true;
            break;
        }
    }
    assert(found_opcode);
    assert(found_url);

    std::puts("PASS: widget::image lays out and emits DrawImage");
}

void test_checkbox_and_radio_widgets() {
    struct ToggleA {};
    struct PickB { int idx; };
    using Msg = std::variant<ToggleA, PickB>;

    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::checkbox<Msg>("Subscribe",  false, ToggleA{});
    widget::checkbox<Msg>("Subscribe!", true,  ToggleA{});
    widget::radio<Msg>   ("Option A",   false, PickB{0});
    widget::radio<Msg>   ("Option B",   true,  PickB{1});
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 400.0f);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 4);

    auto check_widget = [](NodeHandle row_h, bool active, float radius,
                           Decoration expected_decoration) {
        auto& row = detail::node_at(row_h);
        assert(row.style.flex_direction == FlexDirection::Row);
        assert(row.callback_id == 0xFFFFFFFF);
        assert(row.children.size() == 2);

        auto& box = detail::node_at(row.children[0]);
        assert(box.style.max_width == 16.0f);
        assert(box.style.fixed_height == 16.0f);
        assert(box.border_radius == radius);
        assert(box.callback_id != 0xFFFFFFFF);
        assert(box.cursor_type == 1);
        assert(box.focusable == true);
        if (active) {
            assert(box.background.r == detail::g_app.theme.accent.r);
            assert(box.background.g == detail::g_app.theme.accent.g);
            assert(box.background.b == detail::g_app.theme.accent.b);
            assert(box.background.a == 255);
            assert(box.decoration == expected_decoration);
        } else {
            assert(box.background.r == 255);
            assert(box.background.g == 255);
            assert(box.background.b == 255);
            assert(box.border_color.r == detail::g_app.theme.border.r);
            assert(box.decoration == Decoration::None);
        }

        auto& lbl = detail::node_at(row.children[1]);
        assert(!lbl.text.empty());
        assert(lbl.callback_id == box.callback_id);
        assert(lbl.cursor_type == 1);
        assert(lbl.focusable == false);
    };

    check_widget(root.children[0], false, 3.0f, Decoration::Check);
    check_widget(root.children[1], true,  3.0f, Decoration::Check);
    check_widget(root.children[2], false, 8.0f, Decoration::Dot);
    check_widget(root.children[3], true,  8.0f, Decoration::Dot);

    auto cb_id_a = detail::node_at(detail::node_at(root.children[0]).children[0]).callback_id;
    detail::g_app.callbacks[cb_id_a]();
    auto msgs = detail::drain<Msg>();
    assert(msgs.size() == 1);
    assert(std::holds_alternative<ToggleA>(msgs[0]));

    auto lbl_id_b = detail::node_at(detail::node_at(root.children[3]).children[1]).callback_id;
    detail::g_app.callbacks[lbl_id_b]();
    auto msgs2 = detail::drain<Msg>();
    assert(msgs2.size() == 1);
    assert(std::holds_alternative<PickB>(msgs2[0]));
    assert(std::get<PickB>(msgs2[0]).idx == 1);

    CMD_LEN = 0;
    detail::g_app.focusable_ids.clear();
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);
    int hit_regions = 0;
    for (unsigned int i = 0; i + 4 <= CMD_LEN; i += 4) {
        unsigned int word;
        std::memcpy(&word, &CMD_BUF[i], 4);
        if (word == static_cast<unsigned int>(Cmd::HitRegion)) ++hit_regions;
    }
    assert(hit_regions == 8);
    assert(detail::g_app.focusable_ids.size() == 4);

    std::puts("PASS: checkbox + radio widgets");
}

void test_frame_skip_on_identical_cmd_buffer() {
    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    metrics::reset_all();
    detail::clear_measure_cache();
    detail::g_app.last_paint_hash = 0;
    CMD_LEN = 0;

    RUN_APP(int, int,
        [](int const&) { widget::text("hello frame skip"); },
        [](int& s, int m) { s = m; });

    auto initial_flushes = metrics::inst::flush_calls.total();
    auto initial_skips   = metrics::inst::frames_skipped.total();
    assert(initial_flushes >= 1);
    assert(initial_skips == 0);

    detail::trigger_rebuild();
    assert(metrics::inst::flush_calls.total() == initial_flushes);
    assert(metrics::inst::frames_skipped.total() == initial_skips + 1);

    detail::trigger_rebuild();
    assert(metrics::inst::flush_calls.total() == initial_flushes);
    assert(metrics::inst::frames_skipped.total() == initial_skips + 2);

    std::puts("PASS: frame skip when cmd buffer is identical");
}

void test_row_cross_align_center_default() {
    detail::g_app.arena.reset();
    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::row([&] {
        widget::text("plain");
        widget::code("code");
    });
    Scope::set_current(nullptr);

    assert(root.children.size() == 1);
    auto& row = detail::node_at(root.children[0]);
    assert(row.style.cross_align == CrossAxisAlignment::Center);
    assert(row.children.size() == 2);

    LAYOUT_NODE(root_h, 800.0f);

    auto& tnode = detail::node_at(row.children[0]);
    auto& cnode = detail::node_at(row.children[1]);

    assert(cnode.height > tnode.height + 1.0f);
    assert(tnode.y > 0.5f);

    float t_center = tnode.y + tnode.height / 2.0f;
    float c_center = cnode.y + cnode.height / 2.0f;
    float diff = t_center - c_center;
    if (diff < 0) diff = -diff;
    assert(diff < 0.5f);
    std::puts("PASS: row cross-align center default");
}

void test_theme_json_roundtrip() {
    Theme original{};
    original.accent = {255, 0, 0, 255};

    auto json_str = theme_to_json(original);
    auto parsed = theme_from_json(json_str);
    assert(parsed.has_value());
    assert(parsed->accent.r == 255);
    assert(parsed->accent.g == 0);
    assert(parsed->accent.b == 0);
    assert(parsed->accent.a == 255);
    assert(parsed->background.r == original.background.r);
    assert(parsed->background.g == original.background.g);
    assert(parsed->foreground.r == original.foreground.r);
    assert(parsed->body_font_size == original.body_font_size);
    assert(parsed->line_height_ratio == original.line_height_ratio);
    assert(parsed->max_content_width == original.max_content_width);

    auto bad = theme_from_json(R"({"background":{"r":0,"g":0,"b":0,"a":255}})");
    assert(!bad.has_value());

    std::puts("PASS: theme JSON roundtrip");
}

// ============================================================
// Runner
// ============================================================

int main() {
    test_column_layout();
    test_row_intrinsic_width();
    test_row_wraps_last_text_leaf();
    test_containment_invariant();
    test_alignment_center();
    test_max_width_centering();
    test_word_wrap();
    test_newline_handling();
    test_measure_text_cache_dedup();
    test_set_theme_updates_and_invalidates_cache();
    test_sized_box_in_row();
    test_image_widget_layout_and_emit();
    test_checkbox_and_radio_widgets();
    test_frame_skip_on_identical_cmd_buffer();
    test_row_cross_align_center_default();
    test_theme_json_roundtrip();
    std::puts("\nAll tests passed.");
    return 0;
}

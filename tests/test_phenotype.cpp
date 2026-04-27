#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <variant>
#include <vector>
import phenotype;

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
    float phenotype_measure_text(float fs, unsigned int, char const*, unsigned int len) {
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

void test_grid_cell_text_is_vertically_centered() {
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    layout::grid({80.0f}, 40.0f, [] {
        widget::cell("A", true, 80.0f, 40.0f);
    }, 0.0f);
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 200.0f);
    auto const& grid = detail::node_at(detail::node_at(root_h).children[0]);
    auto const& cell = detail::node_at(grid.children[0]);
    assert(cell.height == 40.0f);

    CMD_LEN = 0;
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);

    bool found_text = false;
    float text_y = 0.0f;
    for (unsigned int i = 0; i + 28 <= CMD_LEN;) {
        unsigned int op;
        std::memcpy(&op, &CMD_BUF[i], 4);
        if (op == static_cast<unsigned int>(Cmd::DrawText)) {
            std::memcpy(&text_y, &CMD_BUF[i + 8], 4);
            found_text = true;
            break;
        }
        i += 4;
    }
    assert(found_text);

    auto line_height = cell.font_size * detail::g_app.theme.line_height_ratio;
    auto expected_y = (cell.height - line_height) / 2.0f;
    assert(text_y > 0.0f);
    assert(std::fabs(text_y - expected_y) < 0.01f);

    std::puts("PASS: grid cell text is vertically centered");
}

void test_canvas_widget_invokes_painter() {
    detail::g_app.arena.reset();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    int paint_calls = 0;
    char const* sample_text = "cad++";
    widget::canvas(200.0f, 100.0f, [&paint_calls, sample_text](Painter& p) {
        ++paint_calls;
        p.line(10.0f, 20.0f, 60.0f, 70.0f, 1.0f, Color{255, 0, 0, 255});
        p.text(80.0f, 30.0f, sample_text, 5u, 14.0f, Color{0, 0, 0, 255});
    });
    Scope::set_current(nullptr);

    LAYOUT_NODE(root_h, 400.0f);

    assert(root.children.size() == 1);
    auto& cv = detail::node_at(root.children[0]);
    assert(cv.width == 200.0f);
    assert(cv.height == 100.0f);
    assert(cv.paint_fn);

    CMD_LEN = 0;
    PAINT_NODE(root_h, 0, 0, 0, 600.0f);
    assert(paint_calls == 1);

    bool found_line = false;
    bool found_text = false;
    for (unsigned int i = 0; i + 4 <= CMD_LEN; i += 4) {
        unsigned int word;
        std::memcpy(&word, &CMD_BUF[i], 4);
        if (word == static_cast<unsigned int>(Cmd::DrawLine)) found_line = true;
        if (word == static_cast<unsigned int>(Cmd::DrawText)) found_text = true;
    }
    assert(found_line);
    assert(found_text);

    bool found_text_payload = false;
    for (unsigned int i = 0; i + 5 <= CMD_LEN; ++i) {
        if (std::memcmp(&CMD_BUF[i], "cad++", 5) == 0) {
            found_text_payload = true;
            break;
        }
    }
    assert(found_text_payload);

    std::puts("PASS: widget::canvas invokes painter and emits DrawLine + DrawText");
}

void test_canvas_bypasses_paint_cache_after_diff() {
    auto make_canvas_tree = [](int& paint_calls, Color color, bool paint) {
        auto root_h = detail::alloc_node();
        detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

        Scope scope(root_h);
        Scope::set_current(&scope);
        widget::canvas(200.0f, 100.0f, [&paint_calls, color](Painter& p) {
            ++paint_calls;
            p.line(10.0f, 20.0f, 60.0f, 70.0f, 1.0f, color);
        });
        Scope::set_current(nullptr);

        if (paint) {
            LAYOUT_NODE(root_h, 400.0f);
            CMD_LEN = 0;
            PAINT_NODE(root_h, 0, 0, 0, 600.0f);
            std::memcpy(detail::g_app.prev_cmd_buf, CMD_BUF, CMD_LEN);
            detail::g_app.prev_cmd_len = CMD_LEN;
        }
        return root_h;
    };

    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.prev_cmd_len = 0;

    int old_calls = 0;
    auto old_root = make_canvas_tree(old_calls, Color{255, 0, 0, 255}, true);
    assert(old_calls == 1);
    assert(detail::node_at(old_root).paint_dynamic);
    assert(!detail::node_at(old_root).paint_valid);

    detail::g_app.prev_root = old_root;
    std::swap(detail::g_app.arena, detail::g_app.prev_arena);
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();

    int new_calls = 0;
    auto new_root = make_canvas_tree(new_calls, Color{0, 0, 255, 255}, false);
    auto matched = detail::diff_and_copy_layout(
        detail::g_app.prev_root,
        new_root,
        detail::g_app.prev_arena,
        detail::g_app.arena);
    assert(matched);

    LAYOUT_NODE(new_root, 400.0f);
    CMD_LEN = 0;
    PAINT_NODE(new_root, 0, 0, 0, 600.0f);
    assert(new_calls == 1);
    assert(detail::node_at(new_root).paint_dynamic);
    assert(!detail::node_at(new_root).paint_valid);

    std::puts("PASS: widget::canvas bypasses paint cache after diff");
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

    check_widget(root.children[0], false, detail::g_app.theme.radius_xs, Decoration::Check);
    check_widget(root.children[1], true,  detail::g_app.theme.radius_xs, Decoration::Check);
    check_widget(root.children[2], false, detail::g_app.theme.radius_lg, Decoration::Dot);
    check_widget(root.children[3], true,  detail::g_app.theme.radius_lg, Decoration::Dot);

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
    detail::collect_focusable_ids(root_h);
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

void test_paint_only_props_invalidate_diff_cache() {
    auto make_radio_tree = [](bool selected, bool paint) {
        auto root_h = detail::alloc_node();
        detail::node_at(root_h).style.flex_direction = FlexDirection::Column;
        Scope scope(root_h);
        Scope::set_current(&scope);
        widget::radio<int>("Base", selected, 1);
        Scope::set_current(nullptr);
        if (paint) {
            LAYOUT_NODE(root_h, 400.0f);
            PAINT_NODE(root_h, 0, 0, 0, 600.0f);
        }
        return root_h;
    };

    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    detail::g_app.callbacks.clear();
    CMD_LEN = 0;
    auto old_root = make_radio_tree(true, true);

    detail::g_app.prev_root = old_root;
    std::swap(detail::g_app.arena, detail::g_app.prev_arena);
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    CMD_LEN = 0;
    auto new_root = make_radio_tree(false, false);

    auto matched = detail::diff_and_copy_layout(
        detail::g_app.prev_root,
        new_root,
        detail::g_app.prev_arena,
        detail::g_app.arena);

    assert(!matched);
    auto const& radio_row = detail::node_at(detail::node_at(new_root).children[0]);
    auto const& radio_box = detail::node_at(radio_row.children[0]);
    assert(radio_box.decoration == Decoration::None);
    assert(!radio_box.layout_valid);
    assert(!radio_box.paint_valid);

    std::puts("PASS: paint-only prop changes invalidate diff/paint cache");
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

    std::puts("PASS: theme JSON roundtrip");
}

void test_theme_json_partial_keeps_defaults() {
    Theme defaults{};
    auto parsed = theme_from_json(R"({"accent":{"r":10,"g":186,"b":181,"a":255}})");
    assert(parsed.has_value());

    // Overridden field reflects the JSON.
    assert(parsed->accent.r == 10);
    assert(parsed->accent.g == 186);
    assert(parsed->accent.b == 181);
    assert(parsed->accent.a == 255);

    // Every other field falls back to the C++ default.
    assert(parsed->background.r == defaults.background.r);
    assert(parsed->foreground.r == defaults.foreground.r);
    assert(parsed->muted.r       == defaults.muted.r);
    assert(parsed->border.r      == defaults.border.r);
    assert(parsed->code_bg.r     == defaults.code_bg.r);
    assert(parsed->hero_bg.r     == defaults.hero_bg.r);
    assert(parsed->body_font_size    == defaults.body_font_size);
    assert(parsed->heading_font_size == defaults.heading_font_size);
    assert(parsed->line_height_ratio == defaults.line_height_ratio);
    assert(parsed->max_content_width == defaults.max_content_width);

    // An empty overlay yields the unmodified Theme defaults.
    auto empty_overlay = theme_from_json("{}");
    assert(empty_overlay.has_value());
    assert(empty_overlay->accent.r == defaults.accent.r);
    assert(empty_overlay->background.r == defaults.background.r);
    assert(empty_overlay->body_font_size == defaults.body_font_size);

    std::puts("PASS: theme JSON partial keeps defaults");
}

void test_theme_json_color_string_forms() {
    // 6-digit hex (Tiffany).
    {
        auto parsed = theme_from_json(R"({"accent":"#0abab5"})");
        assert(parsed.has_value());
        assert(parsed->accent.r == 0x0a);
        assert(parsed->accent.g == 0xba);
        assert(parsed->accent.b == 0xb5);
        assert(parsed->accent.a == 0xff);
    }
    // 3-digit hex shorthand (#fff -> 0xff 0xff 0xff).
    {
        auto parsed = theme_from_json(R"({"background":"#fff"})");
        assert(parsed.has_value());
        assert(parsed->background.r == 0xff);
        assert(parsed->background.g == 0xff);
        assert(parsed->background.b == 0xff);
        assert(parsed->background.a == 0xff);
    }
    // 4-digit hex shorthand with alpha (#0a0f -> r=0,g=0xaa,b=0,a=0xff).
    {
        auto parsed = theme_from_json(R"({"foreground":"#0a0f"})");
        assert(parsed.has_value());
        assert(parsed->foreground.r == 0x00);
        assert(parsed->foreground.g == 0xaa);
        assert(parsed->foreground.b == 0x00);
        assert(parsed->foreground.a == 0xff);
    }
    // 8-digit hex with alpha.
    {
        auto parsed = theme_from_json(R"({"foreground":"#11223344"})");
        assert(parsed.has_value());
        assert(parsed->foreground.r == 0x11);
        assert(parsed->foreground.g == 0x22);
        assert(parsed->foreground.b == 0x33);
        assert(parsed->foreground.a == 0x44);
    }
    // Uppercase hex digits.
    {
        auto parsed = theme_from_json(R"({"accent":"#ABCDEF"})");
        assert(parsed.has_value());
        assert(parsed->accent.r == 0xab);
        assert(parsed->accent.g == 0xcd);
        assert(parsed->accent.b == 0xef);
    }
    // CSS rgb() function.
    {
        auto parsed = theme_from_json(R"json({"accent":"rgb(10, 186, 181)"})json");
        assert(parsed.has_value());
        assert(parsed->accent.r == 10);
        assert(parsed->accent.g == 186);
        assert(parsed->accent.b == 181);
        assert(parsed->accent.a == 255);
    }
    // CSS rgba() function with fractional alpha.
    {
        auto parsed = theme_from_json(R"json({"accent":"rgba(255, 0, 0, 0.5)"})json");
        assert(parsed.has_value());
        assert(parsed->accent.r == 255);
        assert(parsed->accent.g == 0);
        assert(parsed->accent.b == 0);
        // 0.5 * 255 = 127.5 -> 128 (round-to-nearest).
        assert(parsed->accent.a == 128);
    }
    // "transparent" keyword.
    {
        auto parsed = theme_from_json(R"({"accent":"transparent"})");
        assert(parsed.has_value());
        assert(parsed->accent.r == 0);
        assert(parsed->accent.g == 0);
        assert(parsed->accent.b == 0);
        assert(parsed->accent.a == 0);
    }
    // Invalid color string -> error reports the offending value and field.
    {
        auto parsed = theme_from_json(R"({"accent":"not-a-color"})");
        assert(!parsed.has_value());
        assert(parsed.error().find("accent") != std::string::npos);
        assert(parsed.error().find("not-a-color") != std::string::npos);
    }
    // Out-of-range rgb component -> error.
    {
        auto parsed = theme_from_json(R"json({"accent":"rgb(999, 0, 0)"})json");
        assert(!parsed.has_value());
    }
    // Hex with non-hex digits -> error.
    {
        auto parsed = theme_from_json(R"({"accent":"#zzzzzz"})");
        assert(!parsed.has_value());
    }
    std::puts("PASS: theme JSON color string forms");
}

void test_theme_json_color_object_back_compat() {
    auto parsed = theme_from_json(
        R"({"accent":{"r":10,"g":186,"b":181,"a":255}})");
    assert(parsed.has_value());
    assert(parsed->accent.r == 10);
    assert(parsed->accent.g == 186);
    assert(parsed->accent.b == 181);
    assert(parsed->accent.a == 255);

    // theme_to_json emits the object form, and that output round-trips
    // through theme_from_json — covering JSON files that were written
    // by an older phenotype build before the string forms existed.
    Theme original{};
    original.accent = {12, 34, 56, 78};
    auto reparsed = theme_from_json(theme_to_json(original));
    assert(reparsed.has_value());
    assert(reparsed->accent.r == 12);
    assert(reparsed->accent.g == 34);
    assert(reparsed->accent.b == 56);
    assert(reparsed->accent.a == 78);

    // A Color object with a wrong field type still errors.
    auto bad = theme_from_json(
        R"({"accent":{"r":"oops","g":0,"b":0,"a":0}})");
    assert(!bad.has_value());

    std::puts("PASS: theme JSON Color object back-compat");
}

void test_theme_json_mixed_overlay() {
    Theme defaults{};
    auto parsed = theme_from_json(R"({
        "accent": "#0abab5",
        "background": {"r":244,"g":244,"b":245,"a":255},
        "body_font_size": 18.0,
        "max_content_width": 960
    })");
    assert(parsed.has_value());

    // String form for accent.
    assert(parsed->accent.r == 0x0a);
    assert(parsed->accent.g == 0xba);
    assert(parsed->accent.b == 0xb5);
    assert(parsed->accent.a == 0xff);
    // Object form for background.
    assert(parsed->background.r == 244);
    assert(parsed->background.g == 244);
    assert(parsed->background.b == 245);
    assert(parsed->background.a == 255);
    // Float overrides.
    assert(parsed->body_font_size == 18.0f);
    assert(parsed->max_content_width == 960.0f);
    // Untouched fields still hold defaults.
    assert(parsed->foreground.r == defaults.foreground.r);
    assert(parsed->heading_font_size == defaults.heading_font_size);

    std::puts("PASS: theme JSON mixed overlay");
}

namespace button_test {
struct Click {};
using ButtonMsg = std::variant<Click>;

inline NodeHandle build_button(ButtonVariant variant, bool disabled) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::msg_queue().clear();

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
} // namespace button_test

void test_button_default_variant() {
    auto btn_h = button_test::build_button(ButtonVariant::Default, /*disabled=*/false);
    auto& btn = detail::node_at(btn_h);
    auto const& t = detail::g_app.theme;
    assert(btn.background.r == t.surface.r &&
           btn.background.g == t.surface.g &&
           btn.background.b == t.surface.b);
    assert(btn.text_color.r == t.foreground.r);
    assert(btn.hover_background.r == t.state_hover_bg.r);
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
    assert(btn.hover_background.r == t.accent_strong.r);
    assert(btn.border_color.r == t.accent.r);
    assert(btn.border_width == 1);
    assert(btn.cursor_type == 1);
    assert(btn.focusable == true);
    assert(btn.callback_id != 0xFFFFFFFFu);
    std::puts("PASS: button primary variant");
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

namespace text_field_test {
struct Changed { std::string text; };
using TfMsg = std::variant<Changed>;
inline TfMsg make_changed(std::string s) { return Changed{std::move(s)}; }

inline NodeHandle build_text_field(std::string const& current,
                                   bool error, bool disabled) {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.input_handlers.clear();
    detail::g_app.input_nodes.clear();
    detail::msg_queue().clear();

    auto root_h = detail::alloc_node();
    detail::node_at(root_h).style.flex_direction = FlexDirection::Column;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::text_field<TfMsg>("Name", current, &make_changed, error, disabled);
    Scope::set_current(nullptr);

    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 1);
    return root.children[0];
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

void test_column_props_default_and_override() {
    auto const& t = detail::g_app.theme;

    // Default builder overload — gap=Md, cross=Start, main=Start.
    {
        detail::g_app.arena.reset();
        auto root_h = detail::alloc_node();
        Scope scope(root_h);
        Scope::set_current(&scope);
        layout::column([&]{});
        Scope::set_current(nullptr);

        auto& root = detail::node_at(root_h);
        assert(root.children.size() == 1);
        auto& col = detail::node_at(root.children[0]);
        assert(col.style.flex_direction == FlexDirection::Column);
        assert(col.style.gap == t.space_md);
        assert(col.style.cross_align == CrossAxisAlignment::Start);
        assert(col.style.main_align == MainAxisAlignment::Start);
    }
    // Explicit overrides.
    {
        detail::g_app.arena.reset();
        auto root_h = detail::alloc_node();
        Scope scope(root_h);
        Scope::set_current(&scope);
        layout::column([&]{}, SpaceToken::Lg, CrossAxisAlignment::Center,
                       MainAxisAlignment::SpaceBetween);
        Scope::set_current(nullptr);

        auto& root = detail::node_at(root_h);
        auto& col = detail::node_at(root.children[0]);
        assert(col.style.gap == t.space_lg);
        assert(col.style.cross_align == CrossAxisAlignment::Center);
        assert(col.style.main_align == MainAxisAlignment::SpaceBetween);
    }
    std::puts("PASS: column props (default + override)");
}

void test_row_props_default_and_override() {
    auto const& t = detail::g_app.theme;

    // Default builder overload — gap=Sm, cross=Center, main=Start.
    {
        detail::g_app.arena.reset();
        auto root_h = detail::alloc_node();
        Scope scope(root_h);
        Scope::set_current(&scope);
        layout::row([&]{});
        Scope::set_current(nullptr);

        auto& root = detail::node_at(root_h);
        auto& r = detail::node_at(root.children[0]);
        assert(r.style.flex_direction == FlexDirection::Row);
        assert(r.style.gap == t.space_sm);
        assert(r.style.cross_align == CrossAxisAlignment::Center);
        assert(r.style.main_align == MainAxisAlignment::Start);
    }
    // Explicit overrides.
    {
        detail::g_app.arena.reset();
        auto root_h = detail::alloc_node();
        Scope scope(root_h);
        Scope::set_current(&scope);
        layout::row([&]{}, SpaceToken::Xl, CrossAxisAlignment::Stretch,
                    MainAxisAlignment::End);
        Scope::set_current(nullptr);

        auto& root = detail::node_at(root_h);
        auto& r = detail::node_at(root.children[0]);
        assert(r.style.gap == t.space_xl);
        assert(r.style.cross_align == CrossAxisAlignment::Stretch);
        assert(r.style.main_align == MainAxisAlignment::End);
    }
    std::puts("PASS: row props (default + override)");
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
    test_grid_cell_text_is_vertically_centered();
    test_canvas_widget_invokes_painter();
    test_canvas_bypasses_paint_cache_after_diff();
    test_checkbox_and_radio_widgets();
    test_frame_skip_on_identical_cmd_buffer();
    test_paint_only_props_invalidate_diff_cache();
    test_row_cross_align_center_default();
    test_theme_json_roundtrip();
    test_theme_json_partial_keeps_defaults();
    test_theme_json_color_string_forms();
    test_theme_json_color_object_back_compat();
    test_theme_json_mixed_overlay();
    test_button_default_variant();
    test_button_primary_variant();
    test_button_disabled();
    test_text_field_default();
    test_text_field_default_placeholder();
    test_text_field_error();
    test_text_field_disabled();
    test_text_size_variants();
    test_text_color_variants();
    test_text_inline_code_chrome();
    test_space_value_resolves_each_token();
    test_column_props_default_and_override();
    test_row_props_default_and_override();
    std::puts("\nAll tests passed.");
    return 0;
}

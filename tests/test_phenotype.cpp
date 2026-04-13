#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <variant>
#include <vector>
import phenotype;

// Mock WASM imports — wasm-ld uses local definitions instead of imports
extern "C" {
    void phenotype_flush() {}

    float phenotype_measure_text(float font_size, unsigned int /*mono*/,
                                 char const* /*text*/, unsigned int len) {
        return static_cast<float>(len) * font_size * 0.6f;
    }

    float phenotype_get_canvas_width() { return 800.0f; }
    float phenotype_get_canvas_height() { return 600.0f; }

    void phenotype_open_url(char const* /*url*/, unsigned int /*len*/) {}

    // Reach into phenotype.paint's cmd buffer via linkage so the
    // image-emit regression test can scan it. The buffer / length are
    // declared in src/phenotype_paint.cppm as extern "C" exports for
    // the JS shim; they have C linkage so the test process can pick
    // them up directly without going through the module export path.
    extern unsigned char phenotype_cmd_buf[];
    extern unsigned int phenotype_cmd_len;
}

using namespace phenotype;

// ============================================================
// Layout tests
// ============================================================

void test_column_layout() {
    detail::g_app.arena.reset();
    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;
    root.style.gap = 10;
    root.style.padding[0] = 5; // top
    root.style.padding[2] = 5; // bottom

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

    detail::layout_node(root_h, 400.0f);

    assert(root.width == 400.0f);
    assert(a.y == 5.0f);            // padding top
    assert(b.y > a.y + a.height);   // gap between children
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

    detail::layout_node(row_h, 400.0f);

    assert(bullet.width < 50.0f);
    assert(text.width > bullet.width);
    assert(text.x > bullet.x + bullet.width); // gap
    std::puts("PASS: row intrinsic width");
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

    detail::layout_node(root_h, 300.0f);

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

    // Use a nested container with fixed width so centering is visible
    auto inner_h = detail::alloc_node();
    auto& inner = detail::node_at(inner_h);
    inner.style.max_width = 100;
    inner.style.fixed_height = 20;
    detail::node_at(col_h).children.push_back(inner_h);

    detail::layout_node(col_h, 400.0f);

    // Centered child with max_width=100 in 400px parent
    assert(inner.width <= 100.0f);
    assert(inner.x > 0); // not left-aligned
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

    detail::layout_node(root_h, 800.0f);

    // Child with max_width=200 in 800px parent
    assert(child.width <= 200.0f);
    // max_width auto-centering adjusts node->x inside layout_node
    // The x offset is relative to parent padding
    // With max_width < available_width, x should be shifted right
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

    detail::layout_node(node_h, 100.0f); // narrow width forces wrapping

    assert(!node.text_lines.empty());
    assert(node.text_lines.size() > 1); // should wrap
    assert(node.height > 0);
    std::puts("PASS: word wrap");
}

void test_newline_handling() {
    detail::g_app.arena.reset();
    auto node_h = detail::alloc_node();
    auto& node = detail::node_at(node_h);
    node.text = "line1\nline2\nline3";
    node.font_size = 16.0f;

    detail::layout_node(node_h, 800.0f);

    assert(node.text_lines.size() == 3);
    assert(node.text_lines[0] == "line1");
    assert(node.text_lines[1] == "line2");
    assert(node.text_lines[2] == "line3");
    std::puts("PASS: newline handling");
}

// The host `measure_text` cache deduplicates (font_size, mono, text)
// triples across rebuilds. After laying out a tree with repeated text,
// the number of host calls (cache misses) must be far below the total
// number of measurement requests, and a second layout pass on the same
// tree must produce zero new host calls — every measurement is a hit.
void test_measure_text_cache_dedup() {
    detail::g_app.arena.reset();
    metrics::reset_all();
    detail::clear_measure_cache();

    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;

    // Five leaves with the same content. Word-wrap measures every word
    // and every space; without the cache that's roughly five leaves *
    // (two words + one space) = ~15 host calls. With the cache the
    // distinct tuples are { "hello", "world", " " } at one font size,
    // so misses should stay in the single digits.
    for (int i = 0; i < 5; ++i) {
        auto h = detail::alloc_node();
        auto& n = detail::node_at(h);
        n.text = "hello world hello";
        n.font_size = 16.0f;
        detail::node_at(root_h).children.push_back(h);
    }

    detail::layout_node(root_h, 800.0f);

    auto host_calls_first = metrics::inst::measure_text_calls.total();
    auto cache_hits_first = metrics::inst::measure_text_cache_hits.total();
    assert(host_calls_first > 0);
    assert(host_calls_first < 10);
    assert(cache_hits_first > host_calls_first);

    // Second pass on the same tree: every measurement is a cache hit.
    metrics::inst::measure_text_calls.reset();
    metrics::inst::measure_text_cache_hits.reset();
    detail::layout_node(root_h, 800.0f);
    assert(metrics::inst::measure_text_calls.total() == 0);
    assert(metrics::inst::measure_text_cache_hits.total() > 0);

    std::puts("PASS: measure_text cache deduplicates across rebuilds");
}

// set_theme replaces the global Theme and clears the measure_text
// cache so that fonts or font sizes changing in the new theme don't
// keep returning stale widths.
void test_set_theme_updates_and_invalidates_cache() {
    detail::g_app.arena.reset();
    metrics::reset_all();
    detail::clear_measure_cache();

    // Prime the cache with a layout pass at the default theme.
    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Column;
    auto leaf_h = detail::alloc_node();
    auto& leaf = detail::node_at(leaf_h);
    leaf.text = "theme demo";
    leaf.font_size = current_theme().body_font_size;
    detail::node_at(root_h).children.push_back(leaf_h);
    detail::layout_node(root_h, 400.0f);
    assert(metrics::inst::measure_text_cache_hits.total()
           + metrics::inst::measure_text_calls.total() > 0);

    // Switch to a different theme (only the fields we care about
    // are overridden; the rest keep defaults).
    Theme dark{};
    dark.background = {10,  10,  10, 255};
    dark.foreground = {240, 240, 240, 255};
    dark.body_font_size = 18.0f;
    set_theme(dark);

    // current_theme reflects the new values.
    assert(current_theme().body_font_size == 18.0f);
    assert(current_theme().background.r == 10);
    assert(current_theme().foreground.r == 240);

    // Measure cache is empty — the next measurement re-hits the host
    // trampoline because the old entries (keyed on the old font size
    // and string content) are gone.
    metrics::inst::measure_text_calls.reset();
    metrics::inst::measure_text_cache_hits.reset();
    detail::layout_node(root_h, 400.0f);
    assert(metrics::inst::measure_text_calls.total() > 0);

    // Reset to default so later tests see the stock theme again.
    set_theme(Theme{});

    std::puts("PASS: set_theme swaps theme and invalidates measure cache");
}

// layout::sized_box exposes Style::max_width on a container, and the
// row layout treats container children with max_width > 0 as having
// a fixed intrinsic width (instead of the default "you're a flex
// child, take what's left"). The test puts two sized_boxes inside a
// row and asserts each one is exactly its max_width pixels wide,
// regardless of the row's available width.
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

    detail::layout_node(root_h, 800.0f);

    assert(root.children.size() == 1);
    auto& row = detail::node_at(root.children[0]);
    assert(row.children.size() == 2);

    auto& left = detail::node_at(row.children[0]);
    auto& right = detail::node_at(row.children[1]);

    // Each sized_box must be sized to exactly its max_width.
    assert(left.width == 200.0f);
    assert(right.width == 300.0f);

    // The row must respect the gap between fixed-width children.
    // (default row gap = 8)
    assert(right.x >= left.x + left.width);

    std::puts("PASS: sized_box honors max_width inside row layout");
}

// widget::image stores its URL in node.image_url, sizes the leaf
// to (width, height) via max_width + fixed_height, and the paint
// pass emits a DrawImage opcode containing the URL bytes. The shim
// renders the actual texture; this test only verifies the C++ side
// of the contract.
void test_image_widget_layout_and_emit() {
    detail::g_app.arena.reset();

    // Build a row containing one image leaf so we go through the
    // row layout path that PR #35 added max_width-as-fixed-intrinsic
    // semantics to.
    auto root_h = detail::alloc_node();
    auto& root = detail::node_at(root_h);
    root.style.flex_direction = FlexDirection::Row;

    Scope scope(root_h);
    Scope::set_current(&scope);
    widget::image({"logo.png", 8}, 64.0f, 48.0f);
    Scope::set_current(nullptr);

    detail::layout_node(root_h, 200.0f);

    assert(root.children.size() == 1);
    auto& img = detail::node_at(root.children[0]);
    assert(img.image_url == std::string("logo.png"));
    assert(img.width == 64.0f);
    assert(img.height == 48.0f);

    // Paint the tree and scan the cmd buffer for the DrawImage
    // opcode + the URL bytes that follow.
    phenotype_cmd_len = 0;
    detail::paint_node(root_h, 0, 0, 0, 600.0f);

    bool found_opcode = false;
    bool found_url = false;
    for (unsigned int i = 0; i + 4 <= phenotype_cmd_len; i += 4) {
        unsigned int word;
        std::memcpy(&word, &phenotype_cmd_buf[i], 4);
        if (word == static_cast<unsigned int>(Cmd::DrawImage)) {
            found_opcode = true;
        }
    }
    // The URL bytes are emitted right after the header — search for
    // the literal "logo.png" anywhere in the buffer.
    char const* needle = "logo.png";
    for (unsigned int i = 0; i + 8 <= phenotype_cmd_len; ++i) {
        if (std::memcmp(&phenotype_cmd_buf[i], needle, 8) == 0) {
            found_url = true;
            break;
        }
    }
    assert(found_opcode);
    assert(found_url);

    std::puts("PASS: widget::image lays out and emits DrawImage");
}

// widget::checkbox<Msg> and widget::radio<Msg> share an internal helper
// that builds a row container holding a 16x16 indicator box plus a text
// label. The visual + interaction contract:
//   * border_radius = 3 for checkbox, 8 for radio (the latter rounds
//     into a circle via the existing SDF rounded-rect shader)
//   * active   → background = theme.accent + decoration glyph (Check or Dot)
//   * inactive → background = white, border = theme.border, no decoration
//   * indicator and label SHARE one callback id, so clicks on either
//     dispatch the same Msg
//   * indicator is `focusable = true` (default) so Tab focus + the focus
//     ring land on the snug 16x16 indicator
//   * label is `focusable = false` so Tab skips it and the focus ring
//     never draws around the label text
// This test exercises both states of both widgets and verifies that
// clicks routed through both the indicator's and the label's hit
// regions dispatch the message of the user's Msg type.
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
    widget::checkbox<Msg>("Subscribe",  /*checked=*/false, ToggleA{});
    widget::checkbox<Msg>("Subscribe!", /*checked=*/true,  ToggleA{});
    widget::radio<Msg>   ("Option A",   /*selected=*/false, PickB{0});
    widget::radio<Msg>   ("Option B",   /*selected=*/true,  PickB{1});
    Scope::set_current(nullptr);

    detail::layout_node(root_h, 400.0f);

    // Each widget produces a row container (no callback) with two
    // children: an indicator box and a label leaf, both holding the
    // same shared callback id.
    auto& root = detail::node_at(root_h);
    assert(root.children.size() == 4);

    auto check_widget = [](NodeHandle row_h, bool active, float radius,
                           Decoration expected_decoration) {
        auto& row = detail::node_at(row_h);
        assert(row.style.flex_direction == FlexDirection::Row);
        assert(row.callback_id == 0xFFFFFFFF); // row itself is not clickable
        assert(row.children.size() == 2);

        auto& box = detail::node_at(row.children[0]);
        assert(box.style.max_width == 16.0f);
        assert(box.style.fixed_height == 16.0f);
        assert(box.border_radius == radius);
        assert(box.callback_id != 0xFFFFFFFF);
        assert(box.cursor_type == 1);
        assert(box.focusable == true); // indicator is the Tab target
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
        // Label shares the indicator's callback id and is non-focusable.
        assert(lbl.callback_id == box.callback_id);
        assert(lbl.cursor_type == 1);
        assert(lbl.focusable == false);
    };

    check_widget(root.children[0], /*active=*/false, 3.0f, Decoration::Check);
    check_widget(root.children[1], /*active=*/true,  3.0f, Decoration::Check);
    check_widget(root.children[2], /*active=*/false, 8.0f, Decoration::Dot);
    check_widget(root.children[3], /*active=*/true,  8.0f, Decoration::Dot);

    // Click the unchecked checkbox's indicator → posts ToggleA.
    auto cb_id_a = detail::node_at(detail::node_at(root.children[0]).children[0]).callback_id;
    detail::g_app.callbacks[cb_id_a]();
    auto msgs = detail::drain<Msg>();
    assert(msgs.size() == 1);
    assert(std::holds_alternative<ToggleA>(msgs[0]));

    // Click the selected radio's LABEL (not the indicator) → also
    // posts PickB{1} because they share a callback id.
    auto lbl_id_b = detail::node_at(detail::node_at(root.children[3]).children[1]).callback_id;
    detail::g_app.callbacks[lbl_id_b]();
    auto msgs2 = detail::drain<Msg>();
    assert(msgs2.size() == 1);
    assert(std::holds_alternative<PickB>(msgs2[0]));
    assert(std::get<PickB>(msgs2[0]).idx == 1);

    // Paint pass: 4 widgets × 2 hit regions each (indicator + label) = 8.
    // Clear cmd buffer + focusable_ids so we measure only this paint pass.
    phenotype_cmd_len = 0;
    detail::g_app.focusable_ids.clear();
    detail::paint_node(root_h, 0, 0, 0, 600.0f);
    int hit_regions = 0;
    for (unsigned int i = 0; i + 4 <= phenotype_cmd_len; i += 4) {
        unsigned int word;
        std::memcpy(&word, &phenotype_cmd_buf[i], 4);
        if (word == static_cast<unsigned int>(Cmd::HitRegion)) ++hit_regions;
    }
    assert(hit_regions == 8);

    // focusable_ids should hold exactly 4 entries — one per indicator,
    // because the labels are non-focusable. The labels share the
    // indicators' callback ids, so the unique-id count is also 4.
    assert(detail::g_app.focusable_ids.size() == 4);

    std::puts("PASS: checkbox + radio widgets");
}

// flush_if_changed hashes the cmd buffer after each paint pass and
// skips the (mocked) phenotype_flush() trampoline when the bytes are
// identical to the previous frame. This is the v1 partial-paint
// optimization: caret blinks, idle repaints, and any rebuild that
// happens to produce the same draw commands collapse to a hash +
// return rather than a full GPU upload + draw.
void test_frame_skip_on_identical_cmd_buffer() {
    detail::g_app.arena.reset();
    metrics::reset_all();
    detail::clear_measure_cache();
    detail::g_app.last_paint_hash = 0;
    // Earlier tests in this binary call paint_node directly without
    // going through flush(), so the cmd buffer can carry stale bytes
    // from the previous test. The runner relies on flush() to reset
    // it, so in production this is fine; here we reset it manually
    // so the first rebuild starts from a clean buffer.
    phenotype_cmd_len = 0;

    // Install a runner with a trivially-stable view.
    run<int, int>(
        [](int const&) {
            widget::text("hello frame skip");
        },
        [](int& s, int m) { s = m; });

    // run<>() triggered the initial rebuild — that's flush #1.
    auto initial_flushes = metrics::inst::flush_calls.total();
    auto initial_skips   = metrics::inst::frames_skipped.total();
    assert(initial_flushes >= 1);
    assert(initial_skips == 0);

    // Manually re-trigger the runner without changing state. The
    // view body is identical, so the cmd buffer is identical, and
    // the second flush should be skipped.
    detail::trigger_rebuild();
    assert(metrics::inst::flush_calls.total() == initial_flushes);
    assert(metrics::inst::frames_skipped.total() == initial_skips + 1);

    // A third rebuild should also skip.
    detail::trigger_rebuild();
    assert(metrics::inst::flush_calls.total() == initial_flushes);
    assert(metrics::inst::frames_skipped.total() == initial_skips + 2);

    std::puts("PASS: frame skip when cmd buffer is identical");
}

// `layout::row` defaults cross_align to Center so that mixed-height inline
// content (e.g. widget::text alongside widget::code) lines up on a shared
// centerline instead of leaving the shorter child dangling at the top of
// the row. The test puts a plain text widget next to a padded code widget
// inside layout::row and asserts both children share the same vertical
// midpoint.
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

    detail::layout_node(root_h, 800.0f);

    auto& tnode = detail::node_at(row.children[0]);
    auto& cnode = detail::node_at(row.children[1]);

    // Padded code widget is taller than plain text — that's what makes the
    // alignment visible in the first place.
    assert(cnode.height > tnode.height + 1.0f);

    // The shorter text child is pushed down by Center alignment instead of
    // sitting at the top of the row.
    assert(tnode.y > 0.5f);

    // Centerlines align within sub-pixel tolerance.
    float t_center = tnode.y + tnode.height / 2.0f;
    float c_center = cnode.y + cnode.height / 2.0f;
    float diff = t_center - c_center;
    if (diff < 0) diff = -diff;
    assert(diff < 0.5f);
    std::puts("PASS: row cross-align center default");
}

// Theme JSON roundtrip: serialize a Theme to JSON, deserialize it back,
// verify all fields survived. Also test error path for bad input.
void test_theme_json_roundtrip() {
    Theme original{};
    original.accent = {255, 0, 0, 255}; // red override

    auto json_str = theme_to_json(original);
    auto parsed = theme_from_json(json_str);
    assert(parsed.has_value());
    // Overridden field
    assert(parsed->accent.r == 255);
    assert(parsed->accent.g == 0);
    assert(parsed->accent.b == 0);
    assert(parsed->accent.a == 255);
    // Default fields survived roundtrip
    assert(parsed->background.r == original.background.r);
    assert(parsed->background.g == original.background.g);
    assert(parsed->foreground.r == original.foreground.r);
    assert(parsed->body_font_size == original.body_font_size);
    assert(parsed->line_height_ratio == original.line_height_ratio);
    assert(parsed->max_content_width == original.max_content_width);

    // Bad JSON — missing required fields
    auto bad = theme_from_json(R"({"background":{"r":0,"g":0,"b":0,"a":255}})");
    assert(!bad.has_value());

    std::puts("PASS: theme JSON roundtrip");
}

// vDOM diff v2: the runner uses double-buffer arenas. After view()
// builds the new tree, diff_and_copy_layout compares against the
// previous frame's tree and copies layout for subtrees where all
// layout-affecting properties match. layout_node then skips nodes
// with layout_valid = true.
void test_vdom_diff_layout_skip() {
    detail::g_app.arena.reset();
    detail::g_app.prev_arena.reset();
    metrics::reset_all();
    detail::clear_measure_cache();
    detail::g_app.last_paint_hash = 0;
    detail::g_app.prev_root = NodeHandle::null();
    phenotype_cmd_len = 0;

    static int rebuild_count = 0;
    rebuild_count = 0;

    // View that renders a stable structure + one dynamic text.
    run<int, int>(
        [](int const& s) {
            widget::text("static label");
            widget::text(std::string("count=") + std::to_string(s));
            widget::text("another static");
        },
        [](int& s, int m) { s += m; ++rebuild_count; });

    // run<>() triggered the initial rebuild (first frame).
    // First frame: prev_root is null → diff is skipped → full layout.
    auto initial_computed = metrics::inst::layout_nodes_computed.total();
    auto initial_skipped  = metrics::inst::layout_nodes_skipped.total();
    assert(initial_computed > 0); // all nodes laid out fresh
    assert(initial_skipped == 0); // no previous tree to diff against

    // Second rebuild: same state → identical tree → diff copies ALL layout.
    detail::trigger_rebuild();
    auto after2_computed = metrics::inst::layout_nodes_computed.total();
    auto after2_skipped  = metrics::inst::layout_nodes_skipped.total();
    // layout_node should have skipped everything (root is layout_valid).
    assert(after2_computed == initial_computed); // no new computed nodes
    assert(after2_skipped > initial_skipped);   // all nodes skipped

    // Third rebuild: post a message that changes the count.
    // "count=0" → "count=1" — the text node's content differs.
    // Diff marks the text node + root dirty (subtree-level diff),
    // but the two static siblings are inside the same root subtree
    // so they can't be individually skipped (root is dirty because
    // one child's text changed). All nodes get recomputed.
    detail::post<int>(1);
    detail::trigger_rebuild();
    auto after3_computed = metrics::inst::layout_nodes_computed.total();
    // After the state change, layout recomputes at least the changed nodes.
    assert(after3_computed > after2_computed);

    std::puts("PASS: vDOM diff layout skip");
}

// ============================================================
// Runner
// ============================================================

int main() {
    test_column_layout();
    test_row_intrinsic_width();
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
    test_vdom_diff_layout_skip();
    std::puts("\nAll tests passed.");
    return 0;
}

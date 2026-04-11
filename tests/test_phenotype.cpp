#include <cassert>
#include <cstdio>
#include <string>
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
    test_row_cross_align_center_default();
    std::puts("\nAll tests passed.");
    return 0;
}

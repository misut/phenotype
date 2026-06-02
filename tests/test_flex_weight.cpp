#include <cassert>
#include <cmath>
#include <cstdio>
import phenotype;

using namespace phenotype;

#if !defined(__wasi__) && !defined(__ANDROID__)
static null_host host;
#define LAYOUT_NODE(h, w)              detail::layout_node(host, h, w)
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
#endif

namespace {

bool nearly(float a, float b, float eps = 0.5f) {
    return std::fabs(a - b) <= eps;
}

template <typename View>
NodeHandle build(View&& view) {
    detail::bump_local_gen();
    detail::g_app().arena.reset();
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

// Two `weighted(1)` children inside a row split the remaining width
// 50/50 around fixed siblings, regardless of declaration order.
void test_two_equal_weights_split_evenly() {
    auto root_h = build([&] {
        layout::row([&] {
            widget::image("ignored", 100.0f, 50.0f);
            layout::weighted(1.0f, [&] { layout::box([&]{}); });
            layout::weighted(1.0f, [&] { layout::box([&]{}); });
        });
    });

    LAYOUT_NODE(root_h, 600.0f);
    auto& root = detail::node_at(root_h);
    auto& row = detail::node_at(root.children[0]);
    auto& fixed = detail::node_at(row.children[0]);
    auto& a = detail::node_at(row.children[1]);
    auto& b = detail::node_at(row.children[2]);

    assert(fixed.width == 100.0f);
    float remaining = 600.0f - 100.0f - row.style.gap * 2.0f;
    assert(nearly(a.width, remaining / 2.0f));
    assert(nearly(b.width, remaining / 2.0f));
    std::puts("PASS: two equal weights split remaining width evenly");
}

// `weighted(2)` next to `weighted(1)` claims a 2:1 ratio of the
// remaining width.
void test_unequal_weights_split_proportionally() {
    auto root_h = build([&] {
        layout::row([&] {
            layout::weighted(2.0f, [&] { layout::box([&]{}); });
            layout::weighted(1.0f, [&] { layout::box([&]{}); });
        });
    });

    LAYOUT_NODE(root_h, 600.0f);
    auto& root = detail::node_at(root_h);
    auto& row = detail::node_at(root.children[0]);
    auto& a = detail::node_at(row.children[0]);
    auto& b = detail::node_at(row.children[1]);

    float total = a.width + b.width;
    assert(nearly(a.width / total, 2.0f / 3.0f, 0.01f));
    assert(nearly(b.width / total, 1.0f / 3.0f, 0.01f));
    std::puts("PASS: unequal weights split proportionally");
}

// A row with no `weighted` children keeps the implicit "last unspecified
// child fills" behavior so existing layouts don't shift.
void test_implicit_single_flex_unaffected() {
    auto root_h = build([&] {
        layout::row([&] {
            widget::image("a", 80.0f, 30.0f);
            layout::box([&]{});  // implicit fill
        });
    });

    LAYOUT_NODE(root_h, 400.0f);
    auto& root = detail::node_at(root_h);
    auto& row = detail::node_at(root.children[0]);
    auto& fixed = detail::node_at(row.children[0]);
    auto& fill = detail::node_at(row.children[1]);

    assert(fixed.width == 80.0f);
    float remaining = 400.0f - 80.0f - row.style.gap;
    assert(nearly(fill.width, remaining));
    std::puts("PASS: row without weighted children keeps implicit fill behavior");
}

// When at least one child carries flex_grow, the implicit fill is
// suppressed — the unspecified box gets 0 width and the explicit
// weighted child eats the entire remainder.
void test_explicit_weight_suppresses_implicit_fill() {
    auto root_h = build([&] {
        layout::row([&] {
            widget::image("a", 50.0f, 30.0f);
            layout::box([&]{});                           // unspecified (no longer implicit fill)
            layout::weighted(1.0f, [&] { layout::box([&]{}); });
        });
    });

    LAYOUT_NODE(root_h, 500.0f);
    auto& root = detail::node_at(root_h);
    auto& row = detail::node_at(root.children[0]);
    auto& fixed = detail::node_at(row.children[0]);
    auto& unspec = detail::node_at(row.children[1]);
    auto& weighted = detail::node_at(row.children[2]);

    assert(fixed.width == 50.0f);
    assert(unspec.width == 0.0f);
    float remaining = 500.0f - 50.0f - row.style.gap * 2.0f;
    assert(nearly(weighted.width, remaining));
    std::puts("PASS: explicit weight suppresses implicit fill on siblings");
}

// A bounded column uses weighted children to absorb vertical remainder.
// This lets shell-like surfaces fill their parent while keeping fixed
// toolbar/status siblings pinned to the top/bottom edges.
void test_bounded_column_weight_splits_height() {
    auto root_h = build([&] {
        layout::material_surface(
            layout::MaterialSurfaceOptions{
                .kind = MaterialKind::None,
                .direction = FlexDirection::Column,
                .padding = SpaceToken::Xs,
                .gap = SpaceToken::Xs,
                .fixed_height = 300.0f,
            },
            [&] {
                widget::image("header", 100.0f, 40.0f);
                layout::weighted(1.0f, [&] { layout::box([&]{}); });
                widget::image("footer", 100.0f, 30.0f);
            });
    });

    LAYOUT_NODE(root_h, 400.0f);
    auto& root = detail::node_at(root_h);
    auto& column = detail::node_at(root.children[0]);
    auto& header = detail::node_at(column.children[0]);
    auto& weighted = detail::node_at(column.children[1]);
    auto& footer = detail::node_at(column.children[2]);

    float const pad_y = column.style.padding[0] + column.style.padding[2];
    float const expected_weighted =
        300.0f - header.height - footer.height - column.style.gap * 2.0f;
    assert(nearly(column.height, 300.0f + pad_y));
    assert(nearly(weighted.height, expected_weighted));
    assert(nearly(footer.y,
                  column.style.padding[0]
                      + header.height
                      + column.style.gap
                      + weighted.height
                      + column.style.gap));
    std::puts("PASS: bounded column weights split remaining height");
}

int main() {
    test_two_equal_weights_split_evenly();
    test_unequal_weights_split_proportionally();
    test_implicit_single_flex_unaffected();
    test_explicit_weight_suppresses_implicit_fill();
    test_bounded_column_weight_splits_height();
    std::puts("\nAll flex_weight tests passed.");
    return 0;
}

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
    detail::g_app().arena.reset();
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

}  // namespace

// A fresh `accordion` starts collapsed: header only, no body row.
void test_accordion_starts_collapsed() {
    detail::local_store().clear();
    auto root_h = build([&] {
        layout::accordion("section", [&] {
            widget::text("hidden body");
        });
    });

    auto& root = detail::node_at(root_h);
    auto& col = detail::node_at(root.children[0]);
    // Just the header — body skipped while collapsed.
    assert(col.children.size() == 1);
    auto& header = detail::node_at(col.children[0]);
    assert(header.debug_semantic_role.compare("accordion-header") == 0);
    assert(header.debug_semantic_label.compare("section") == 0);
    assert(header.material.kind == MaterialKind::Clear);
    assert(header.material.role == MaterialSurfaceRole::Content);
    assert(header.material.container.interactive);
    std::puts("PASS: accordion starts collapsed (no body)");
}

// Invoking the registered click callback flips the framework_local
// boolean, so the next rebuild renders both the header and the body.
void test_accordion_click_expands_body() {
    detail::local_store().clear();
    // Reuse one lambda for both builds — lifts the accordion's call
    // site to a single line of source code, so MSVC and Clang agree on
    // `std::source_location::current()` even if their default-arg
    // call-site reporting differs at the lambda definition site.
    auto run = [] {
        layout::accordion("section", [&] {
            widget::text("body");
        });
    };

    build(run);
    // Simulate the click that the shell would post on the header's
    // callback — this is exactly what `dispatch_mouse_button` does
    // when it routes a hit into the registered slot.
    assert(detail::g_app().callbacks.size() == 1);
    detail::g_app().callbacks[0]();   // toggle expanded → true

    auto root_h = build(run);

    auto& root = detail::node_at(root_h);
    auto& col = detail::node_at(root.children[0]);
    assert(col.children.size() == 2);                  // header + body
    auto& header = detail::node_at(col.children[0]);
    assert(header.material.kind == MaterialKind::Clear);
    assert(header.material.role == MaterialSurfaceRole::Content);
    assert(header.material.container.interactive);
    auto& body = detail::node_at(col.children[1]);
    assert(body.children.size() == 1);                 // user content
    std::puts("PASS: click on accordion header expands body");
}

// Two accordions in one view stay independent — clicking one does
// not flip the other's state.
void test_two_accordions_stay_independent() {
    detail::local_store().clear();
    auto run = [] {
        layout::accordion("first",  [&] { widget::text("a"); });
        layout::accordion("second", [&] { widget::text("b"); });
    };

    build(run);
    assert(detail::g_app().callbacks.size() == 2);
    detail::g_app().callbacks[0]();   // expand first only

    auto root_h = build(run);
    auto& root = detail::node_at(root_h);
    auto& col0 = detail::node_at(root.children[0]);
    auto& col1 = detail::node_at(root.children[1]);
    assert(col0.children.size() == 2);  // first expanded
    assert(col1.children.size() == 1);  // second still collapsed
    std::puts("PASS: two accordions keep independent expanded state");
}

// Rebuilding the view without a flip preserves the prior expanded
// state — the framework_local entry survives prune because the
// accordion was visited again.
void test_accordion_state_persists_across_frames() {
    detail::local_store().clear();
    auto run = [] {
        layout::accordion("section", [&] { widget::text("body"); });
    };
    build(run);
    detail::g_app().callbacks[0]();   // expand
    build(run);
    auto root_h = build(run);       // re-render, no flip in between
    auto& col = detail::node_at(detail::node_at(root_h).children[0]);
    assert(col.children.size() == 2);
    std::puts("PASS: accordion expanded state persists across rebuilds");
}

int main() {
    test_accordion_starts_collapsed();
    test_accordion_click_expands_body();
    test_two_accordions_stay_independent();
    test_accordion_state_persists_across_frames();
    std::puts("\nAll accordion tests passed.");
    return 0;
}

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
import phenotype;

using namespace phenotype;

#if !defined(__wasi__) && !defined(__ANDROID__)
static null_host host;
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
#endif

namespace {

bool contains_text(NodeHandle handle, std::string const& needle) {
    auto* node = detail::g_app().arena.get(handle);
    if (!node)
        return false;
    if (node->text == needle)
        return true;
    for (auto child : node->children) {
        if (contains_text(child, needle))
            return true;
    }
    return false;
}

struct CounterApp {
    ui::View body(ui::Context& cx) const {
        auto count = cx.state<int>("count", 0);

        return ui::VStack(
            ui::Text("Counter").font(ui::Font::title),
            ui::Text("count=" + std::to_string(count.get())),
            ui::Button("+")
                .role(ui::ButtonRole::primary)
                .on_click([count] {
                    count.mutate([](int& value) { ++value; });
                }));
    }
};

struct TextInputApp {
    ui::View body(ui::Context& cx) const {
        auto query = cx.state<std::string>("query", {});

        return ui::VStack(
            ui::TextField("Search", query.binding()),
            ui::Text("query=" + query.get()));
    }
};

struct SelectionControlsApp {
    ui::View body(ui::Context& cx) const {
        auto accepted = cx.state<bool>("accepted", false);
        auto detailed = cx.state<bool>("detailed", false);
        auto tab = cx.state<std::size_t>("tab", 0);

        return ui::VStack(
            ui::Text(
                "accepted=" + std::string(accepted.get() ? "true" : "false")
                + " detailed=" + std::string(detailed.get() ? "true" : "false")
                + " tab=" + std::to_string(tab.get())),
            ui::Checkbox("Accept", accepted.get())
                .on_toggle([accepted] {
                    accepted.mutate([](bool& value) { value = !value; });
                }),
            ui::Switch("Details", detailed.get())
                .on_toggle([detailed] {
                    detailed.mutate([](bool& value) { value = !value; });
                }),
            ui::Tabs(std::vector<std::string>{"Intro", "API"}, tab.get())
                .on_select([tab](std::size_t index) {
                    tab.set(index);
                }),
            ui::Progress(tab.get() == 0 ? 0.35f : 0.85f).width(240.0f));
    }
};

struct ViewportApp {
    ui::View body(ui::Context& cx) const {
        return ui::VStack(
            ui::Text(
                "viewport="
                + std::to_string(static_cast<int>(cx.viewport_width()))
                + "x"
                + std::to_string(static_cast<int>(cx.viewport_height()))),
            ui::Text(cx.compact_width(900.0f) ? "compact=true" : "compact=false"));
    }
};

void test_local_state_and_callback() {
#if !defined(__wasi__) && !defined(__ANDROID__)
    ui::run<CounterApp>(host);
#else
    ui::run<CounterApp>();
#endif
    assert(contains_text(detail::g_app().root, "count=0"));
    assert(detail::g_app().callbacks.size() == 1u);

    detail::g_app().callbacks[0]();

    assert(contains_text(detail::g_app().root, "count=1"));
    std::puts("PASS: ui local state and callback");
}

void test_text_field_binding() {
#if !defined(__wasi__) && !defined(__ANDROID__)
    ui::run<TextInputApp>(host);
#else
    ui::run<TextInputApp>();
#endif
    assert(contains_text(detail::g_app().root, "query="));
    assert(detail::g_app().input_handlers.size() == 1u);

    auto& handler = detail::g_app().input_handlers[0].second;
    handler.invoke(handler.state, "phenotype");

    assert(contains_text(detail::g_app().root, "query=phenotype"));
    std::puts("PASS: ui text field binding");
}

void test_foreach_keys() {
    auto view = ui::ForEach<int>(
        std::vector<int>{1, 2, 3},
        [](int value) { return value; },
        [](int value) {
            return ui::Text("item=" + std::to_string(value));
        });

    detail::g_app().arena.reset();
    auto root = detail::alloc_node();
    Scope scope(root);
    Scope::set_current(&scope);
    view.render();
    Scope::set_current(nullptr);

    auto const& root_node = detail::node_at(root);
    assert(root_node.children.size() == 3u);
    assert(root_node.children_keyed);
    assert(contains_text(root, "item=2"));
    std::puts("PASS: ui foreach keyed children");
}

void test_selection_control_wrappers() {
#if !defined(__wasi__) && !defined(__ANDROID__)
    ui::run<SelectionControlsApp>(host);
#else
    ui::run<SelectionControlsApp>();
#endif
    assert(contains_text(
        detail::g_app().root,
        "accepted=false detailed=false tab=0"));
    assert(detail::g_app().callbacks.size() >= 4u);

    detail::g_app().callbacks[0]();
    assert(contains_text(
        detail::g_app().root,
        "accepted=true detailed=false tab=0"));

    detail::g_app().callbacks[1]();
    assert(contains_text(
        detail::g_app().root,
        "accepted=true detailed=true tab=0"));

    detail::g_app().callbacks[3]();
    assert(contains_text(
        detail::g_app().root,
        "accepted=true detailed=true tab=1"));
    std::puts("PASS: ui selection control wrappers");
}

void test_context_viewport_helpers() {
#if !defined(__wasi__) && !defined(__ANDROID__)
    ui::run<ViewportApp>(host);
#else
    ui::run<ViewportApp>();
#endif
    assert(contains_text(detail::g_app().root, "viewport=800x600"));
    assert(contains_text(detail::g_app().root, "compact=true"));
    std::puts("PASS: ui context viewport helpers");
}

} // namespace

int main() {
    test_local_state_and_callback();
    test_text_field_binding();
    test_foreach_keys();
    test_selection_control_wrappers();
    test_context_viewport_helpers();
    std::puts("\nAll modern ui tests passed.");
}

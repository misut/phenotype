// Memory-safety regression tests for the component runner.
//
// These exercise the runner, callbacks, input handlers, and view paths. They
// are intentionally cheap asserts on top of `phenotype`; the real signal comes
// from AddressSanitizer/UBSan, which is enabled in CI for the native target.

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>
import phenotype;

using namespace phenotype;

#if !defined(__wasi__) && !defined(__ANDROID__)
static null_host host;
#define RUN_APP(APP) phenotype::ui::run(host, APP)
#define REPAINT(sy) detail::repaint(host, 0.0f, sy)
#else
extern "C" {
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
#define RUN_APP(APP) phenotype::ui::run(APP)
#define REPAINT(sy) detail::repaint(0.0f, sy)
#endif

// ============================================================
// Shared app types
// ============================================================

namespace m {

struct Model {
    int count = 0;
    std::string input;
};

struct CounterApp {
    std::shared_ptr<Model> model;

    ui::View body(ui::Context&) {
        return ui::View{[model = model] {
            layout::column([&] {
                widget::text("count=" + std::to_string(model->count));
                widget::button("inc", [model] { ++model->count; });
            });
        }};
    }
};

struct FullApp {
    std::shared_ptr<Model> model;

    ui::View body(ui::Context&) {
        return ui::View{[model = model] {
            layout::column([&] {
                widget::text("count=" + std::to_string(model->count));
                widget::button("inc", [model] { ++model->count; });
                widget::text_field("type", model->input, [model](std::string s) {
                    model->input = std::move(s);
                });
            });
        }};
    }
};

} // namespace m

void reset_app() {
    auto& app = detail::g_app();
    detail::install_app_runner(nullptr, nullptr);
    app.callbacks.clear();
    app.callback_roles.clear();
    app.input_handlers.clear();
    app.input_nodes.clear();
    app.focusable_ids.clear();
    app.root = NodeHandle::null();
    app.prev_root = NodeHandle::null();
    app.scroll_y = 0;
    app.hovered_id = 0xFFFFFFFF;
    app.focused_id = 0xFFFFFFFF;
    app.focus_visible = false;
    app.prev_focus_visible = false;
    app.caret_pos = 0xFFFFFFFF;
    app.caret_visible = true;
    app.last_paint_hash = 0;
    app.debug_viewport_width = 0.0f;
    app.debug_viewport_height = 0.0f;
    app.input_debug = {};
    app.arena.reset();
    app.prev_arena.reset();
}

// ============================================================
// 1. Run cycle — 100 click+rebuild rounds via the component runner
// ============================================================

void test_run_cycle() {
    reset_app();

    auto model = std::make_shared<m::Model>();
    RUN_APP(m::CounterApp{model});

    for (int i = 0; i < 100; ++i) {
        detail::handle_event(0);
    }

    assert(model->count == 100);
    assert(detail::g_app().callbacks.size() == 1);

    reset_app();
    std::puts("PASS: run cycle (100 clicks)");
}

// ============================================================
// 2. Callback identity across rebuilds
// ============================================================

void test_callback_identity() {
    reset_app();

    auto model = std::make_shared<m::Model>();
    RUN_APP(m::CounterApp{model});
    for (int i = 0; i < 5; ++i)
        detail::handle_event(0);

    for (int i = 0; i < 5; ++i)
        detail::handle_event(0);
    assert(detail::g_app().callbacks.size() == 1);
    assert(model->count == 10);

    reset_app();
    std::puts("PASS: callback identity");
}

// ============================================================
// 3. TextField callback via handle_key
// ============================================================

void test_textfield_dispatch() {
    reset_app();

    auto model = std::make_shared<m::Model>();
    RUN_APP(m::FullApp{model});
    detail::set_focus_id(1);

    detail::handle_key(/*char*/ 0, 'h');
    detail::handle_key(/*char*/ 0, 'i');
    detail::handle_key(/*backspace*/ 1, 0);

    assert(!detail::g_app().input_handlers.empty());
    assert(model->input == "h");

    reset_app();
    std::puts("PASS: textfield dispatch");
}

// ============================================================
// 4. Callback rebuild no-leak
// ============================================================

void test_callback_rebuild_no_leak() {
    reset_app();

    auto model = std::make_shared<m::Model>();
    RUN_APP(m::CounterApp{model});
    for (int i = 0; i < 50; ++i) {
        detail::handle_event(0);
        assert(detail::g_app().callbacks.size() == 1);
    }
    assert(model->count == 50);

    reset_app();
    std::puts("PASS: callback rebuild no leak");
}

// ============================================================
// 5. Stress: random click/type/repaint sequence
// ============================================================

void test_stress_random_events() {
    reset_app();

    auto model = std::make_shared<m::Model>();
    RUN_APP(m::FullApp{model});

    std::mt19937 rng(0xC0FFEE);
    std::uniform_int_distribution<int> action(0, 4);
    std::uniform_int_distribution<unsigned int> letter('a', 'z');
    std::uniform_real_distribution<float> scroll(0.0f, 200.0f);

    for (int i = 0; i < 1000; ++i) {
        switch (action(rng)) {
        case 0:
            detail::handle_event(0);
            break;
        case 1:
            detail::set_focus_id(1);
            break;
        case 2:
            detail::set_focus_id(1);
            detail::handle_key(0, letter(rng));
            break;
        case 3:
            detail::set_focus_id(1);
            detail::handle_key(1, 0);
            break;
        case 4:
            REPAINT(scroll(rng));
            break;
        }
    }

    assert(!detail::g_app().callbacks.empty());
    assert(!detail::g_app().input_handlers.empty());

    reset_app();
    std::puts("PASS: stress random events (1000 ops)");
}

// ============================================================
// 6 + 7. Negative tests — Arena guards must trap
// ============================================================

#if !defined(__wasi__) && !defined(_WIN32)
#include <csignal>
#include <sys/wait.h>
extern "C" {
    int fork(void);
    int waitpid(int pid, int* status, int options);
    [[noreturn]] void _exit(int status);
}

template<typename F>
bool runs_to_abort(F body) {
    std::fflush(stdout);
    std::fflush(stderr);
    int pid = fork();
    if (pid == 0) {
        std::fclose(stderr);
        body();
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

void test_arena_overflow_graceful() {
    reset_app();

    bool aborted = runs_to_abort([] {
        auto& a = detail::g_app().arena;
        for (unsigned int i = 0; i < detail::g_app().arena.MAX_NODES + 1; ++i)
            a.alloc_node();
    });
    assert(aborted);

    reset_app();
    std::puts("PASS: arena overflow traps");
}

void test_node_handle_stale_traps() {
    reset_app();

    auto h = detail::g_app().arena.alloc_node();
    assert(detail::g_app().arena.get(h) != nullptr);

    detail::g_app().arena.reset();
    assert(detail::g_app().arena.get(h) == nullptr);

    bool aborted = runs_to_abort([h] {
        (void)detail::g_app().arena.alloc_node();
        detail::g_app().arena.must_get(h);
    });
    assert(aborted);

    reset_app();
    std::puts("PASS: stale NodeHandle traps");
}
#endif

// ============================================================
// Runner
// ============================================================

int main() {
    test_run_cycle();
    test_callback_identity();
    test_textfield_dispatch();
    test_callback_rebuild_no_leak();
    test_stress_random_events();
#if !defined(__wasi__) && !defined(_WIN32)
    test_arena_overflow_graceful();
    test_node_handle_stale_traps();
#endif
    std::puts("\nAll memory-safety tests passed.");
    return 0;
}

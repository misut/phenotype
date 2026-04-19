// Memory-safety regression tests for the message DSL.
//
// These exercise the runner / dispatcher / view paths that previously
// broke silently inside dlmalloc's linear memory. They are intentionally
// cheap asserts on top of `phenotype` — the real signal comes from
// AddressSanitizer/UBSan, which is enabled in CI for the native target.

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <utility>
#include <variant>
#include <vector>
import phenotype;

using namespace phenotype;

#ifndef __wasi__
static null_host host;
#define RUN_APP(S, M, V, U) phenotype::run<S, M>(host, V, U)
#define REPAINT(sy) detail::repaint(host, sy)
#else
extern "C" {
    void phenotype_flush() {}
    float phenotype_measure_text(float fs, unsigned int, char const*, unsigned int len) {
        return static_cast<float>(len) * fs * 0.6f;
    }
    float phenotype_get_canvas_width() { return 800.0f; }
    float phenotype_get_canvas_height() { return 600.0f; }
    void phenotype_open_url(char const*, unsigned int) {}
}
#define RUN_APP(S, M, V, U) phenotype::run<S, M>(V, U)
#define REPAINT(sy) detail::repaint(sy)
#endif

// ============================================================
// Shared message + state types
// ============================================================

namespace m {

struct Increment {};
struct Decrement {};
struct InputChanged { std::string text; };

using Msg = std::variant<Increment, Decrement, InputChanged>;

struct State {
    int count = 0;
    std::string input;
};

inline void update(State& state, Msg msg) {
    std::visit([&](auto const& x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::same_as<T, Increment>)         state.count += 1;
        else if constexpr (std::same_as<T, Decrement>)    state.count -= 1;
        else if constexpr (std::same_as<T, InputChanged>) state.input = x.text;
    }, msg);
}

inline Msg input_to_msg(std::string s) { return InputChanged{std::move(s)}; }

inline void view_counter(State const& s) {
    layout::column([&] {
        widget::text("count=" + std::to_string(s.count));
        widget::button<Msg>("inc", Increment{});
    });
}

inline void view_full(State const& s) {
    layout::column([&] {
        widget::text("count=" + std::to_string(s.count));
        widget::button<Msg>("inc", Increment{});
        widget::text_field<Msg>("type", s.input, +input_to_msg);
    });
}

} // namespace m

void reset_app() {
    auto& app = detail::g_app;
    app.app_runner = nullptr;
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
    app.caret_pos = 0xFFFFFFFF;
    app.caret_visible = true;
    app.last_paint_hash = 0;
    app.debug_viewport_width = 0.0f;
    app.debug_viewport_height = 0.0f;
    app.input_debug = {};
    app.arena.reset();
    app.prev_arena.reset();
    detail::msg_queue().clear();
}

// ============================================================
// 1. Run cycle — 100 click+rebuild rounds via run<>
// ============================================================

void test_run_cycle() {
    reset_app();

    RUN_APP(m::State, m::Msg, m::view_counter, m::update);

    for (int i = 0; i < 100; ++i) {
        detail::handle_event(0);
    }

    assert(detail::msg_queue().empty());
    assert(detail::g_app.callbacks.size() == 1);

    reset_app();
    std::puts("PASS: run cycle (100 clicks)");
}

// ============================================================
// 2. Callback identity across rebuilds
// ============================================================

void test_message_dispatch_identity() {
    reset_app();

    RUN_APP(m::State, m::Msg, m::view_counter, m::update);
    for (int i = 0; i < 5; ++i)
        detail::handle_event(0);

    for (int i = 0; i < 5; ++i)
        detail::handle_event(0);
    assert(detail::g_app.callbacks.size() == 1);
    assert(detail::msg_queue().empty());

    reset_app();
    std::puts("PASS: message dispatch identity");
}

// ============================================================
// 3. TextField dispatch via handle_key
// ============================================================

void test_textfield_dispatch() {
    reset_app();

    RUN_APP(m::State, m::Msg, m::view_full, m::update);
    detail::set_focus_id(1);

    detail::handle_key(/*char*/ 0, 'h');
    detail::handle_key(/*char*/ 0, 'i');
    detail::handle_key(/*backspace*/ 1, 0);

    assert(!detail::g_app.input_handlers.empty());
    assert(detail::msg_queue().empty());

    reset_app();
    std::puts("PASS: textfield dispatch");
}

// ============================================================
// 4. Message queue no-leak
// ============================================================

void test_msg_queue_no_leak() {
    reset_app();

    RUN_APP(m::State, m::Msg, m::view_counter, m::update);
    for (int i = 0; i < 50; ++i) {
        detail::handle_event(0);
        assert(detail::msg_queue().empty());
    }

    reset_app();
    std::puts("PASS: msg queue no leak");
}

// ============================================================
// 5. Stress: random click/type/repaint sequence
// ============================================================

void test_stress_random_events() {
    reset_app();

    RUN_APP(m::State, m::Msg, m::view_full, m::update);

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

    assert(detail::msg_queue().empty());
    assert(!detail::g_app.callbacks.empty());

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
        auto& a = detail::g_app.arena;
        for (unsigned int i = 0; i < detail::g_app.arena.MAX_NODES + 1; ++i)
            a.alloc_node();
    });
    assert(aborted);

    reset_app();
    std::puts("PASS: arena overflow traps");
}

void test_node_handle_stale_traps() {
    reset_app();

    auto h = detail::g_app.arena.alloc_node();
    assert(detail::g_app.arena.get(h) != nullptr);

    detail::g_app.arena.reset();
    assert(detail::g_app.arena.get(h) == nullptr);

    bool aborted = runs_to_abort([h] {
        (void)detail::g_app.arena.alloc_node();
        detail::g_app.arena.must_get(h);
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
    test_message_dispatch_identity();
    test_textfield_dispatch();
    test_msg_queue_no_leak();
    test_stress_random_events();
#if !defined(__wasi__) && !defined(_WIN32)
    test_arena_overflow_graceful();
    test_node_handle_stale_traps();
#endif
    std::puts("\nAll memory-safety tests passed.");
    return 0;
}

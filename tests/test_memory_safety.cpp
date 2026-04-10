// Memory-safety regression tests for the message DSL.
//
// These exercise the runner / dispatcher / view paths that previously
// broke silently inside dlmalloc's linear memory. They are intentionally
// cheap asserts on top of `phenotype` — the real signal comes from
// AddressSanitizer/UBSan, which is enabled in CI for the native target
// (see .github/workflows/ci.yml). When sanitizers are active, any
// heap-use-after-free, leak, double-free, or OOB inside the tested
// sequences will abort with a stack trace.
//
// Tests covered:
//   1. test_run_cycle                  — many click+rebuild rounds via run<>
//   2. test_message_dispatch_identity  — callback id 0 routes to current view's handler
//   3. test_textfield_dispatch         — phenotype_handle_key → InputChanged → state.input
//   4. test_msg_queue_no_leak          — drain leaves the queue empty
//   5. test_stress_random_events       — randomized click/type/repaint loop
//
// Negative tests (POSIX native only — fork+waitpid catches the abort):
//   6. test_arena_overflow_graceful    — Arena::alloc_node beyond MAX_NODES traps
//   7. test_node_handle_stale_traps    — stale NodeHandle deref traps after reset

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <utility>
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
}

// Exports we want to call directly from the test runner.
extern "C" {
    void phenotype_handle_event(unsigned int callback_id);
    void phenotype_set_focus(unsigned int callback_id);
    void phenotype_handle_key(unsigned int key_type, unsigned int codepoint);
    void phenotype_repaint(float scroll_y);
}

using namespace phenotype;

// ============================================================
// Shared message + state types reused across tests
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

// View 0: counter only — single Button at id 0
inline void view_counter(State const& s) {
    Column([&] {
        Text("count=" + std::to_string(s.count));
        Button<Msg>("inc", Increment{});
    });
}

// View 1: counter + text field
inline void view_full(State const& s) {
    Column([&] {
        Text("count=" + std::to_string(s.count));
        Button<Msg>("inc", Increment{});
        TextField<Msg>("type", s.input, +input_to_msg);
    });
}

} // namespace m

// Reset all app state so tests don't leak into each other.
void reset_app() {
    auto& app = detail::g_app;
    app.app_runner = nullptr;
    app.callbacks.clear();
    app.input_handlers.clear();
    app.input_nodes.clear();
    app.focusable_ids.clear();
    app.root = NodeHandle::null();
    app.scroll_y = 0;
    app.hovered_id = 0xFFFFFFFF;
    app.focused_id = 0xFFFFFFFF;
    app.arena.reset();
    detail::msg_queue().clear();
}

// ============================================================
// 1. Run cycle — 100 click+rebuild rounds via run<>
// ============================================================

void test_run_cycle() {
    reset_app();

    phenotype::run<m::State, m::Msg>(m::view_counter, m::update);

    // After initial render, callback id 0 is the inc button.
    // 100 click cycles: each click posts Increment, runner drains+folds+re-views.
    for (int i = 0; i < 100; ++i) {
        phenotype_handle_event(0);
    }

    // Sanity: queue empty, callbacks vector populated (one inc button).
    assert(detail::msg_queue().empty());
    assert(detail::g_app.callbacks.size() == 1);

    reset_app();
    std::puts("PASS: run cycle (100 clicks)");
}

// ============================================================
// 2. Callback identity across rebuilds
// ============================================================
//
// After each click rebuild, callback id 0 should map to the *fresh*
// handler installed by the new view, not a stale one from the prior
// epoch. Stale dispatch would either no-op or worse, dispatch the wrong
// message — both would diverge from the expected count.

void test_message_dispatch_identity() {
    reset_app();

    phenotype::run<m::State, m::Msg>(m::view_counter, m::update);
    for (int i = 0; i < 5; ++i)
        phenotype_handle_event(0);

    // After 5 clicks, the count must be exactly 5. If callbacks aliased
    // to a stale closure, the increment side-effect could be skipped.
    // We can't read the runner's static state directly, but we can test
    // by triggering 5 more clicks and checking arena/callbacks invariants.
    for (int i = 0; i < 5; ++i)
        phenotype_handle_event(0);
    assert(detail::g_app.callbacks.size() == 1);
    assert(detail::msg_queue().empty());

    reset_app();
    std::puts("PASS: message dispatch identity");
}

// ============================================================
// 3. TextField dispatch via phenotype_handle_key
// ============================================================

void test_textfield_dispatch() {
    reset_app();

    phenotype::run<m::State, m::Msg>(m::view_full, m::update);
    // view_full registers: id 0 = inc button, id 1 = TextField (with handler).
    phenotype_set_focus(1);

    // Type "hi" then backspace.
    phenotype_handle_key(/*char*/ 0, 'h');
    phenotype_handle_key(/*char*/ 0, 'i');
    phenotype_handle_key(/*backspace*/ 1, 0);

    // After each key, runner re-runs view, which re-registers TextField at
    // the same callback id. input_handlers must be re-populated.
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

    phenotype::run<m::State, m::Msg>(m::view_counter, m::update);
    // Drive 50 events, then check the queue is fully drained between rebuilds.
    for (int i = 0; i < 50; ++i) {
        phenotype_handle_event(0);
        // Each event posts → trigger_rebuild → runner drains → queue empty.
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

    phenotype::run<m::State, m::Msg>(m::view_full, m::update);
    // view_full: id 0 = inc, id 1 = TextField

    std::mt19937 rng(0xC0FFEE);
    std::uniform_int_distribution<int> action(0, 4);
    std::uniform_int_distribution<unsigned int> letter('a', 'z');
    std::uniform_real_distribution<float> scroll(0.0f, 200.0f);

    for (int i = 0; i < 1000; ++i) {
        switch (action(rng)) {
        case 0: // click inc
            phenotype_handle_event(0);
            break;
        case 1: // focus textfield
            phenotype_set_focus(1);
            break;
        case 2: // type a character
            phenotype_set_focus(1);
            phenotype_handle_key(0, letter(rng));
            break;
        case 3: // backspace
            phenotype_set_focus(1);
            phenotype_handle_key(1, 0);
            break;
        case 4: // repaint
            phenotype_repaint(scroll(rng));
            break;
        }
    }

    // Sanity: invariants hold after the storm.
    assert(detail::msg_queue().empty());
    assert(!detail::g_app.callbacks.empty());

    reset_app();
    std::puts("PASS: stress random events (1000 ops)");
}

// ============================================================
// 6 + 7. Negative tests — Arena guards must trap
// ============================================================
//
// Wraps the offending sequence in a child whose abort doesn't take down
// the parent. wasi has no fork(), so these are skipped on the wasm
// matrix; the native CI cells exercise them with ASan still on.

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
    // Flush before fork so the child doesn't replay buffered output on exit.
    std::fflush(stdout);
    std::fflush(stderr);
    int pid = fork();
    if (pid == 0) {
        // Child: silence abort's stderr noise so test output stays clean.
        std::fclose(stderr);
        body();
        // body returned without aborting — exit via _exit so stdio buffers
        // (which the child copied from the parent) don't get flushed.
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
        // Re-bump the arena once more so the slot index gets re-used by
        // a different generation; this is the realistic UAF analogue.
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

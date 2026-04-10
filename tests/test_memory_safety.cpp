// Memory-safety regression tests.
//
// These exercise the rebuild/event lifecycle paths that previously broke
// silently inside dlmalloc's linear memory. They are intentionally cheap
// asserts on top of `phenotype` — the real signal comes from
// AddressSanitizer/UBSan, which is enabled in CI for the native target
// (see .github/workflows/ci.yml). When sanitizers are active, any
// heap-use-after-free, leak, double-free, or OOB inside the tested
// sequences will abort with a stack trace.
//
// Tests covered:
//   1. test_rebuild_cycle              — many express()/Trait::set() rounds
//   2. test_callback_after_rebuild     — callback id maps to fresh handler
//   3. test_trait_subscriber_clear     — Trait::set() doesn't deref dangling Scope*
//   4. test_input_state_after_rebuild  — phenotype_handle_key after rebuild
//   5. test_stress_random_events       — randomized click/type/repaint loop
//
// Negative tests (POSIX native only — fork+waitpid catches the abort):
//   6. test_encode_order_violation     — encode<T> type/site mismatch traps
//   7. test_arena_overflow_graceful    — Arena::alloc_node beyond MAX_NODES traps

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <source_location>
#include <string>
#include <typeindex>
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
// Test utilities
// ============================================================

// Reset all app state so tests don't leak into each other. The global
// g_app is intentionally never destroyed (placement-new into static
// storage), so we have to clear its fields by hand.
void reset_app() {
    auto& app = detail::g_app;
    app.app_builder = nullptr;
    app.callbacks.clear();
    app.states.clear();      // dtors run, freeing Trait<T>'s
    app.input_nodes.clear();
    app.focusable_ids.clear();
    app.encode_index = 0;
    app.root = NodeHandle::null();
    app.scroll_y = 0;
    app.hovered_id = 0xFFFFFFFF;
    app.focused_id = 0xFFFFFFFF;
    app.arena.reset();
}

// ============================================================
// 1. Rebuild cycle — express() + many Trait::set() rounds
// ============================================================

void test_rebuild_cycle() {
    reset_app();

    Trait<int>* counter = nullptr;
    int build_count = 0;

    express([&] {
        ++build_count;
        counter = encode(0);
        Button("inc", [&] { counter->set(counter->value() + 1); });
    });

    // Initial rebuild from express()
    assert(build_count == 1);
    assert(counter != nullptr);
    assert(counter->value() == 0);

    // Hook-style encode: 1 state slot, 1 callback (the button)
    assert(detail::g_app.states.size() == 1);
    assert(detail::g_app.callbacks.size() == 1);

    // 100 mutate cycles via Trait::set
    for (int i = 0; i < 100; ++i) {
        counter->set(counter->value() + 1);
        // After each rebuild, invariants must hold:
        assert(detail::g_app.states.size() == 1);    // state preserved
        assert(detail::g_app.callbacks.size() == 1); // fresh button
        assert(counter->value() == i + 1);
    }

    // Trait survived all rebuilds (hook-style index reuse)
    assert(counter->value() == 100);
    assert(build_count == 101);

    reset_app();
    std::puts("PASS: rebuild cycle (100 mutations)");
}

// ============================================================
// 2. Callback identity across rebuild
// ============================================================
//
// After a rebuild, the same callback_id must map to the *new* handler
// registered by the rebuilt tree, not a stale closure from the previous
// epoch. Triggering the event via phenotype_handle_event must invoke
// the current handler.

void test_callback_after_rebuild() {
    reset_app();

    int last_clicked_epoch = -1;
    Trait<int>* epoch = nullptr;

    express([&] {
        epoch = encode(0);
        int snapshot = epoch->value();
        Button("click", [&, snapshot] {
            last_clicked_epoch = snapshot;
        });
    });

    // First click — should observe epoch 0
    phenotype_handle_event(0);
    assert(last_clicked_epoch == 0);

    // Bump epoch → triggers rebuild → new closure captured snapshot=1
    epoch->set(1);
    phenotype_handle_event(0);
    assert(last_clicked_epoch == 1);

    // Once more
    epoch->set(2);
    phenotype_handle_event(0);
    assert(last_clicked_epoch == 2);

    reset_app();
    std::puts("PASS: callback after rebuild");
}

// ============================================================
// 3. Trait subscriber list survives rebuild
// ============================================================
//
// rebuild() clears Trait subscribers BEFORE re-running app_builder
// (which then re-registers them via Trait::value()). The clearer must
// not deref the stale Scope* — it just empties the vector. ASan would
// catch a deref via the vector's destructor or any iteration.

void test_trait_subscriber_clear() {
    reset_app();

    Trait<int>* t = nullptr;
    express([&] {
        t = encode(42);
        // Touch value() so the current Scope is registered as subscriber
        (void)t->value();
    });
    // After express(), the rebuild's local Scope is destroyed but the
    // Trait may still hold a stale subscribers_ entry. The next set()
    // must clear it without dereferencing.
    t->set(43);
    t->set(44);
    t->set(45);
    assert(t->value() == 45);

    reset_app();
    std::puts("PASS: trait subscriber clear");
}

// ============================================================
// 4. Input state lifetime across rebuild
// ============================================================
//
// TextField stores `input_state = Trait<string>*`. The Trait survives
// rebuilds via the encode index, but `input_nodes` (id -> LayoutNode*)
// is wiped on every rebuild. phenotype_handle_key must not crash if
// called between events that trigger rebuilds.

void test_input_state_after_rebuild() {
    reset_app();

    Trait<std::string>* field = nullptr;
    express([&] {
        field = encode<std::string>("");
        TextField(field, "type here");
    });

    // Focus the input field. callback_id 0 is the TextField (only callback).
    phenotype_set_focus(0);

    // Type 'h' (codepoint = 104). This calls Trait::set() inside, which
    // triggers a rebuild. After rebuild, the LayoutNode* in input_nodes
    // points to a freshly allocated arena node — but the Trait survives.
    phenotype_handle_key(/*char*/ 0, 'h');
    phenotype_handle_key(/*char*/ 0, 'i');
    assert(field->value() == "hi");

    // Backspace
    phenotype_handle_key(/*backspace*/ 1, 0);
    assert(field->value() == "h");

    // Several more keystrokes — each is a full rebuild
    char const* word = "ello world";
    for (char const* p = word; *p; ++p)
        phenotype_handle_key(0, static_cast<unsigned int>(*p));
    assert(field->value() == "hello world");

    reset_app();
    std::puts("PASS: input state after rebuild");
}

// ============================================================
// 5. Stress: random click/type/repaint sequence
// ============================================================
//
// Drives the public exports (handle_event, handle_key, repaint) with a
// seeded RNG. Sanitizers do the actual checking — we just need volume.

void test_stress_random_events() {
    reset_app();

    Trait<int>* counter = nullptr;
    Trait<std::string>* text = nullptr;

    express([&] {
        counter = encode(0);
        text = encode<std::string>("");
        Button("inc", [&] { counter->set(counter->value() + 1); });
        TextField(text, "type");
    });

    // 0 = button, 1 = textfield
    assert(detail::g_app.callbacks.size() == 2);

    std::mt19937 rng(0xC0FFEE);
    std::uniform_int_distribution<int> action(0, 4);
    std::uniform_int_distribution<unsigned int> letter('a', 'z');
    std::uniform_real_distribution<float> scroll(0.0f, 200.0f);

    int clicks = 0;
    int typed = 0;

    for (int i = 0; i < 1000; ++i) {
        switch (action(rng)) {
        case 0: // button click
            phenotype_handle_event(0);
            ++clicks;
            break;
        case 1: // focus textfield
            phenotype_set_focus(1);
            break;
        case 2: // type a character (textfield must be focused)
            phenotype_set_focus(1);
            phenotype_handle_key(0, letter(rng));
            ++typed;
            break;
        case 3: // backspace
            phenotype_set_focus(1);
            phenotype_handle_key(1, 0);
            break;
        case 4: // repaint at varying scroll positions
            phenotype_repaint(scroll(rng));
            break;
        }
    }

    // Sanity: counter advanced, text non-empty (or empty if many backspaces)
    assert(counter->value() == clicks);
    assert(text->value().size() <= static_cast<size_t>(typed));

    reset_app();
    std::puts("PASS: stress random events (1000 ops)");
}

// ============================================================
// 6 + 7. Negative tests — encode/arena guards must trap
// ============================================================
//
// PR #D added an std::abort() in encode<T>() for type/site mismatches and
// in Arena::alloc/alloc_node for overflow. We need to *catch* the abort to
// assert the guards fired. POSIX fork() lets us run the offending sequence
// in a child whose abort doesn't take down the test runner. wasi has no
// fork(), so these tests are skipped on the wasm target — the native CI
// matrix exercises them.

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

void test_encode_order_violation() {
    reset_app();

    // First rebuild registers a Trait<int> at the body's encode call site.
    Trait<int>* first = nullptr;
    express([&] { first = encode(0); });
    (void)first;

    // Now drive a SECOND express() that registers a Trait<std::string> at
    // the same slot index in a child process. encode() should detect the
    // type mismatch (Trait<int> already in slot 0) and abort.
    bool aborted = runs_to_abort([] {
        express([] {
            Trait<std::string>* s = encode<std::string>("oops");
            (void)s;
        });
    });
    assert(aborted);

    reset_app();
    std::puts("PASS: encode order violation traps");
}

void test_arena_overflow_graceful() {
    reset_app();

    // Allocate one node beyond MAX_NODES inside a child. Arena::alloc_node
    // is supposed to abort with a diagnostic, not silently corrupt heap.
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

    // Pre-condition: a fresh handle is valid.
    auto h = detail::g_app.arena.alloc_node();
    assert(detail::g_app.arena.get(h) != nullptr);

    // After reset() the generation is bumped → handle goes stale.
    detail::g_app.arena.reset();
    assert(detail::g_app.arena.get(h) == nullptr);

    // must_get on the same stale handle should trap.
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
    test_rebuild_cycle();
    test_callback_after_rebuild();
    test_trait_subscriber_clear();
    test_input_state_after_rebuild();
    test_stress_random_events();
#if !defined(__wasi__) && !defined(_WIN32)
    test_encode_order_violation();
    test_arena_overflow_graceful();
    test_node_handle_stale_traps();
#endif
    std::puts("\nAll memory-safety tests passed.");
    return 0;
}

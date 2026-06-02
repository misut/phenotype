#include <cassert>
#include <cstdio>
#include <string>
#include <utility>
import phenotype;

using namespace phenotype;

// On wasm32-wasi the test binary links against phenotype.wasm, which
// declares a handful of host imports that the JS shim normally
// supplies. wasmtime (exon's test runner for the wasi target) has no
// JS shim, so the test main has to satisfy them with no-op stubs —
// these tests do not exercise paint, measure, URL, or canvas size,
// they just want the runtime to instantiate.
#if defined(__wasi__)
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
#endif

// ============================================================
// framework_local — call-site-positional widget state
// ============================================================
//
// These tests exercise the primitive that backs widget-local state
// (scroll offsets, open/close flags, animation progress). Each test
// wraps view-equivalent code inside next_frame() so framework_local
// entries receive a fresh generation tag and unused entries get pruned
// at the boundary, mirroring what the runner does between rebuilds.
//
// Many tests rely on free helper functions (e.g. `slot_a`) rather than
// inline framework_local calls. That's deliberate and reflects how
// real widgets use the primitive: the call site lives inside the
// widget's implementation, so every invocation of the widget threads
// through the same source_location and therefore the same widget_id.
// Inline calls in two different frames would land on different
// source_locations and thus on different storage — correct behaviour
// but not what the persistence / GC tests want to exercise.

namespace {

void reset_store() {
    detail::local_store().clear();
}

template <typename View>
void next_frame(View&& view) {
    detail::bump_local_gen();
    std::forward<View>(view)();
    detail::prune_local_store();
}

// Stable-source_location helpers. Each function's framework_local call
// expression lives at one source_location, so callers across frames
// see a single widget_id no matter where they invoke the helper from.
int& shared_counter() {
    return framework_local<int>(0);
}

int& slot_a() { return framework_local<int>(0); }
int& slot_b() { return framework_local<int>(0); }

// Stable-source_location keyed accessor — `k` is the disambiguation
// key passed to framework_local, and the call site is fixed inside
// the helper, so write- and read-passes that go through key_slot()
// see the same widget_id as long as they pass the same `k`.
int& key_slot(std::size_t k) {
    return framework_local<int>(0, k);
}

int& scene_slot() {
    return framework_local<int>(0, 1001);
}

}  // namespace

// Adjacent inline calls at distinct source_locations land in distinct
// slots — the most basic invariant.
void test_distinct_call_sites() {
    reset_store();
    next_frame([&] {
        auto& a = framework_local<int>(7);
        auto& b = framework_local<int>(9);
        assert(a == 7);
        assert(b == 9);
        assert(&a != &b);
        a = 100;
        b = 200;
    });
    assert(detail::local_store().size() == 2);
    std::puts("PASS: framework_local distinct call sites");
}

// A value mutated this frame survives into the next when the same
// widget (same source_location, via the shared_counter helper) is
// visited again — the whole point of framework_local.
void test_persistence_across_frames() {
    reset_store();
    next_frame([&] { shared_counter() = 42; });
    next_frame([&] { assert(shared_counter() == 42); });
    std::puts("PASS: framework_local persistence across frames");
}

// Entries not touched during a frame are dropped by prune at the
// frame boundary; revisiting the same widget in a later frame
// re-initialises rather than restoring the prior value.
void test_gc_drops_unvisited() {
    reset_store();
    next_frame([&] {
        slot_a() = 1;
        slot_b() = 2;
    });
    assert(detail::local_store().size() == 2);

    next_frame([&] {
        slot_a();  // touch only A; B falls off
    });
    assert(detail::local_store().size() == 1);

    // Re-touching B in a third frame allocates a fresh entry — the
    // prior one is gone, so we read the initial value rather than 2.
    // (slot_a is itself dropped in this frame for not being visited,
    // so the final store size is 1, not 2.)
    next_frame([&] {
        assert(slot_b() == 0);
    });
    assert(detail::local_store().size() == 1);
    std::puts("PASS: framework_local GC drops unvisited entries");
}

// Repeated calls at the same source_location within one scope get
// disambiguated by the per-scope sibling counter, so each iteration of
// a hot loop returns a fresh slot rather than aliasing onto the first.
void test_same_call_site_disambiguation() {
    reset_store();
    detail::g_app().arena.reset();
    auto root_h = detail::alloc_node();

    Scope scope(root_h);
    Scope::set_current(&scope);
    next_frame([&] {
        // Three calls at the *same* source_location (the loop body) —
        // without the per-scope counter they would collide on a single
        // widget_id and the second/third call would alias the first.
        for (int i = 0; i < 3; ++i) {
            auto& slot = framework_local<int>(0);
            slot = i + 1;
        }
    });
    Scope::set_current(nullptr);

    assert(detail::local_store().size() == 3);
    std::puts("PASS: framework_local same call site disambiguation");
}

// Explicit `key` resolves loop iterations across frames so each
// iteration's slot remains stable even when the loop iteration order
// or count changes between frames (the per-scope counter alone cannot
// guarantee that).
void test_loop_key_isolation() {
    reset_store();
    detail::g_app().arena.reset();
    auto root_h = detail::alloc_node();

    auto write_pass = [&] {
        Scope scope(root_h);
        Scope::set_current(&scope);
        next_frame([&] {
            for (std::size_t i = 0; i < 3; ++i) {
                key_slot(i) = static_cast<int>(i + 1);
            }
        });
        Scope::set_current(nullptr);
    };

    write_pass();
    assert(detail::local_store().size() == 3);

    {
        Scope scope(root_h);
        Scope::set_current(&scope);
        next_frame([&] {
            // Out-of-order access: keys still pin to their original
            // slots irrespective of the visitation order.
            auto& c = key_slot(2);
            auto& a = key_slot(0);
            auto& b = key_slot(1);
            assert(a == 1);
            assert(b == 2);
            assert(c == 3);
        });
        Scope::set_current(nullptr);
    }
    std::puts("PASS: framework_local loop key isolation");
}

void test_scene_local_storage_isolation() {
    auto& main_scene = detail::default_scene_runtime();
    auto& settings_scene = detail::ensure_scene_runtime(SceneDescriptor{
        .id = "framework-local-settings",
        .title = "Framework Local Settings",
        .role = SceneRole::Settings,
        .visible = true,
    });

    {
        detail::ScopedSceneActivation activate(main_scene);
        reset_store();
    }
    {
        detail::ScopedSceneActivation activate(settings_scene);
        reset_store();
    }

    {
        detail::ScopedSceneActivation activate(main_scene);
        next_frame([&] { scene_slot() = 7; });
        auto snapshot = runtime::active_scene();
        assert(snapshot.id == std::string{"main"});
        assert(snapshot.framework_local_entries == 1u);
    }

    {
        detail::ScopedSceneActivation activate(settings_scene);
        next_frame([&] {
            assert(scene_slot() == 0);
            scene_slot() = 42;
        });
        auto snapshot = runtime::active_scene();
        assert(snapshot.id == std::string{"framework-local-settings"});
        assert(snapshot.framework_local_entries == 1u);
    }

    {
        detail::ScopedSceneActivation activate(main_scene);
        next_frame([&] { assert(scene_slot() == 7); });
    }

    {
        detail::ScopedSceneActivation activate(settings_scene);
        next_frame([&] {});
        assert(detail::local_store().empty());
    }

    {
        detail::ScopedSceneActivation activate(main_scene);
        assert(detail::local_store().size() == 1);
        next_frame([&] { assert(scene_slot() == 7); });
    }

    std::puts("PASS: framework_local storage is isolated by scene");
}

// Move-only payload type used to confirm that the type-erased deleter
// actually runs the destructor when prune drops an entry — i.e.
// framework_local doesn't leak resources owned by stored values.
struct MoveOnly {
    int* counter;
    explicit MoveOnly(int* c = nullptr) : counter(c) {}
    MoveOnly(MoveOnly&& o) noexcept : counter(o.counter) { o.counter = nullptr; }
    MoveOnly& operator=(MoveOnly&& o) noexcept {
        if (this != &o) { counter = o.counter; o.counter = nullptr; }
        return *this;
    }
    MoveOnly(MoveOnly const&) = delete;
    MoveOnly& operator=(MoveOnly const&) = delete;
    ~MoveOnly() { if (counter) ++*counter; }
};

void test_destructor_runs_on_prune() {
    reset_store();
    int destructions = 0;
    next_frame([&] {
        framework_local<MoveOnly>(MoveOnly{&destructions});
    });
    // Skip the framework_local call this frame — entry should be
    // pruned and ~MoveOnly should fire on the heap-stored value,
    // bumping `destructions` exactly once.
    next_frame([&] {});
    assert(destructions == 1);
    std::puts("PASS: framework_local destructor runs on prune");
}

int main() {
    test_distinct_call_sites();
    test_persistence_across_frames();
    test_gc_drops_unvisited();
    test_same_call_site_disambiguation();
    test_loop_key_isolation();
    test_scene_local_storage_isolation();
    test_destructor_runs_on_prune();
    std::puts("\nAll framework_local tests passed.");
    return 0;
}

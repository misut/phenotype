module;
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <source_location>
#include <typeindex>
#include <utility>
#include <vector>
export module phenotype.state;

import phenotype.types;

export namespace phenotype {

// ============================================================
// Arena — generational, fixed-capacity slot pool for LayoutNode
// ============================================================
//
// Stores LayoutNodes in a contiguous typed array with a parallel
// generations array. alloc_node() returns a NodeHandle = (index, gen);
// reset() bumps current_gen so prior handles fail get() deref. Storage
// addresses are stable for the lifetime of one epoch (between resets),
// so layout/paint can hold transient `LayoutNode&` references during
// a build pass.

struct Arena {
    static constexpr std::uint32_t MAX_NODES = 4096;

    LayoutNode* nodes = nullptr;
    std::uint32_t* generations = nullptr;
    std::uint32_t node_count = 0;
    std::uint32_t current_gen = 0;

    void ensure_init() {
        if (!nodes) {
            nodes = static_cast<LayoutNode*>(
                ::operator new(sizeof(LayoutNode) * MAX_NODES));
            generations = new std::uint32_t[MAX_NODES]{};
        }
    }

    NodeHandle alloc_node() {
        ensure_init();
        if (node_count >= MAX_NODES) {
            std::fprintf(stderr,
                "phenotype: arena node count exhausted (max %u)\n",
                MAX_NODES);
            std::abort();
        }
        new (&nodes[node_count]) LayoutNode();
        generations[node_count] = current_gen;
        return {node_count++, current_gen};
    }

    LayoutNode* get(NodeHandle h) noexcept {
        if (!h.valid() || h.index >= node_count) return nullptr;
        if (generations[h.index] != h.generation) return nullptr;
        return &nodes[h.index];
    }

    LayoutNode& must_get(NodeHandle h) {
        auto* p = get(h);
        if (!p) {
            std::fprintf(stderr,
                "phenotype: stale NodeHandle (index=%u gen=%u current_gen=%u)\n",
                h.index, h.generation, current_gen);
            std::abort();
        }
        return *p;
    }

    void reset() {
        for (std::uint32_t i = 0; i < node_count; ++i) nodes[i].~LayoutNode();
        node_count = 0;
        ++current_gen; // invalidate all outstanding handles
    }
};

// ============================================================
// StateSlot — type-erased slot for hook-style Trait persistence
// ============================================================

struct StateSlot {
    void* ptr;
    void (*deleter)(void*);
    std::type_index type;          // recorded at first encode<T>(), checked on reuse
    std::source_location origin;   // call site of the first encode<T>()

    StateSlot(void* p, void (*d)(void*),
              std::type_index t, std::source_location o)
        : ptr(p), deleter(d), type(t), origin(o) {}
    StateSlot(StateSlot&& o) noexcept
        : ptr(o.ptr), deleter(o.deleter),
          type(o.type), origin(o.origin) {
        o.ptr = nullptr; o.deleter = nullptr;
    }
    StateSlot& operator=(StateSlot&& o) noexcept {
        if (this != &o) {
            if (deleter) deleter(ptr);
            ptr = o.ptr; deleter = o.deleter;
            type = o.type; origin = o.origin;
            o.ptr = nullptr; o.deleter = nullptr;
        }
        return *this;
    }
    ~StateSlot() { if (deleter) deleter(ptr); }

    StateSlot(StateSlot const&) = delete;
    StateSlot& operator=(StateSlot const&) = delete;
};

// ============================================================
// AppState — consolidated global state
// ============================================================

struct AppState {
    Theme theme;
    Arena arena;
    NodeHandle root = NodeHandle::null();
    float scroll_y = 0;
    unsigned int hovered_id = 0xFFFFFFFF;
    unsigned int focused_id = 0xFFFFFFFF;
    unsigned int caret_pos = 0;
    bool caret_visible = true;
    std::vector<std::function<void()>> callbacks;
    std::vector<StateSlot> states;
    std::vector<unsigned int> focusable_ids;
    std::vector<std::pair<unsigned int, NodeHandle>> input_nodes;
    std::function<void()> app_builder;
    unsigned int encode_index = 0; // hook-style index for encode() across rebuilds
    // Installed by express() in the umbrella module so the engine layer
    // (Trait::set / Scope::mutate) can re-enter rebuild without
    // depending on layout/paint at the type level.
    void (*rebuild_fn)() = nullptr;
};

namespace detail {
    // Placement-new into static storage to avoid global destructor.
    // wasi-sdk's crt1-command.o calls __cxa_atexit on exit, which destroys
    // global C++ objects. Without this trick, g_app's destructors run after
    // _start() and clear the callbacks vector, breaking event handling.
    alignas(AppState) inline unsigned char g_app_storage[sizeof(AppState)];
    inline AppState& g_app = *new (&g_app_storage) AppState();
    inline NodeHandle alloc_node() { return g_app.arena.alloc_node(); }
    inline LayoutNode& node_at(NodeHandle h) { return g_app.arena.must_get(h); }
} // namespace detail

// ============================================================
// Scope — implicit parent tracking for declarative DSL
// ============================================================

struct Scope {
    NodeHandle node;
    std::function<void()> builder;

    Scope(NodeHandle n) : node(n) {}
    Scope(NodeHandle n, std::function<void()> b) : node(n), builder(std::move(b)) {}

    void mutate() {
        if (detail::g_app.rebuild_fn) detail::g_app.rebuild_fn();
    }

    static Scope*& current() {
        static Scope* s = nullptr;
        return s;
    }

    static void set_current(Scope* s) { current() = s; }
};

// ============================================================
// Trait<T> — reactive state with full-rebuild on mutation
// ============================================================

template<typename T>
class Trait {
    T value_;

public:
    explicit Trait(T initial) : value_(std::move(initial)) {}

    T const& value() noexcept { return value_; }

    void set(T new_val) {
        if (value_ == new_val) return;
        value_ = std::move(new_val);
        if (detail::g_app.rebuild_fn) detail::g_app.rebuild_fn();
    }
};

template<typename T>
Trait<T>* encode(T initial,
                 std::source_location loc = std::source_location::current()) {
    auto& app = detail::g_app;
    // Hook-style: reuse existing Trait on rebuild (preserves state).
    // Validates type AND call site so a reordered or conditional encode()
    // chain trips an immediate abort instead of silently aliasing slots.
    // Mirrors React's "Rendered fewer/more hooks than expected" guard,
    // strengthened with std::source_location.
    if (app.encode_index < app.states.size()) {
        auto& slot = app.states[app.encode_index];
        auto want = std::type_index(typeid(Trait<T>));
        if (slot.type != want) {
            std::fprintf(stderr,
                "phenotype: encode() type mismatch at index %u "
                "(slot is %s, encode<T> got %s) — hook order changed at %s:%u "
                "(slot first registered at %s:%u)\n",
                app.encode_index, slot.type.name(), want.name(),
                loc.file_name(), loc.line(),
                slot.origin.file_name(), slot.origin.line());
            std::abort();
        }
        if (slot.origin.line() != loc.line() ||
            std::strcmp(slot.origin.file_name(), loc.file_name()) != 0) {
            std::fprintf(stderr,
                "phenotype: encode() call site moved at index %u "
                "(was %s:%u, now %s:%u)\n",
                app.encode_index,
                slot.origin.file_name(), slot.origin.line(),
                loc.file_name(), loc.line());
            std::abort();
        }
        ++app.encode_index;
        return static_cast<Trait<T>*>(slot.ptr);
    }
    auto* p = new Trait<T>(std::move(initial));
    app.states.push_back(StateSlot{
        p,
        [](void* ptr) { delete static_cast<Trait<T>*>(ptr); },
        std::type_index(typeid(Trait<T>)),
        loc,
    });
    app.encode_index++;
    return p;
}

} // namespace phenotype

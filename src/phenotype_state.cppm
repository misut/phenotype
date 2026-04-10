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
// Arena allocator
// ============================================================

struct Arena {
    static constexpr unsigned int STORAGE_SIZE = 512 * 1024; // 512KB
    static constexpr unsigned int MAX_NODES = 4096;

    unsigned char* storage = nullptr;
    unsigned int offset = 0;
    unsigned int node_count = 0;
    LayoutNode** nodes = nullptr;

    void ensure_init() {
        if (!storage) {
            storage = new unsigned char[STORAGE_SIZE];
            nodes = new LayoutNode*[MAX_NODES];
        }
    }

    void* alloc(unsigned int size) {
        ensure_init();
        size = (size + 15) & ~15u;
        if (offset + size > STORAGE_SIZE) {
            std::fprintf(stderr,
                "phenotype: arena storage exhausted "
                "(requested %u, used %u, capacity %u)\n",
                size, offset, STORAGE_SIZE);
            std::abort();
        }
        auto* p = &storage[offset];
        offset += size;
        return p;
    }

    LayoutNode* alloc_node() {
        if (node_count >= MAX_NODES) {
            std::fprintf(stderr,
                "phenotype: arena node count exhausted (max %u)\n",
                MAX_NODES);
            std::abort();
        }
        auto* mem = alloc(sizeof(LayoutNode));
        auto* node = new (mem) LayoutNode();
        nodes[node_count++] = node;
        return node;
    }

    void reset() {
        for (unsigned int i = 0; i < node_count; ++i) nodes[i]->~LayoutNode();
        node_count = 0;
        offset = 0;
    }
};

// ============================================================
// StateSlot — type-erased slot for hook-style Trait persistence
// ============================================================

struct StateSlot {
    void* ptr;
    void (*deleter)(void*);
    void (*clearer)(void*); // clears subscribers on rebuild
    std::type_index type;          // recorded at first encode<T>(), checked on reuse
    std::source_location origin;   // call site of the first encode<T>()

    StateSlot(void* p, void (*d)(void*), void (*c)(void*),
              std::type_index t, std::source_location o)
        : ptr(p), deleter(d), clearer(c), type(t), origin(o) {}
    StateSlot(StateSlot&& o) noexcept
        : ptr(o.ptr), deleter(o.deleter), clearer(o.clearer),
          type(o.type), origin(o.origin) {
        o.ptr = nullptr; o.deleter = nullptr; o.clearer = nullptr;
    }
    StateSlot& operator=(StateSlot&& o) noexcept {
        if (this != &o) {
            if (deleter) deleter(ptr);
            ptr = o.ptr; deleter = o.deleter; clearer = o.clearer;
            type = o.type; origin = o.origin;
            o.ptr = nullptr; o.deleter = nullptr; o.clearer = nullptr;
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
    LayoutNode* root = nullptr;
    float scroll_y = 0;
    unsigned int hovered_id = 0xFFFFFFFF;
    unsigned int focused_id = 0xFFFFFFFF;
    unsigned int caret_pos = 0;
    bool caret_visible = true;
    std::vector<std::function<void()>> callbacks;
    std::vector<StateSlot> states;
    std::vector<unsigned int> focusable_ids;
    std::vector<std::pair<unsigned int, LayoutNode*>> input_nodes;
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
    inline LayoutNode* alloc_node() { return g_app.arena.alloc_node(); }
} // namespace detail

// ============================================================
// Scope — implicit parent tracking for declarative DSL
// ============================================================

struct Scope {
    LayoutNode* node;
    std::function<void()> builder;

    Scope(LayoutNode* n) : node(n) {}
    Scope(LayoutNode* n, std::function<void()> b) : node(n), builder(std::move(b)) {}

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
    std::vector<Scope*> subscribers_;

public:
    explicit Trait(T initial) : value_(std::move(initial)) {}

    T const& value() {
        if (Scope::current()) {
            auto* s = Scope::current();
            bool found = false;
            for (auto* sub : subscribers_)
                if (sub == s) { found = true; break; }
            if (!found)
                subscribers_.push_back(s);
        }
        return value_;
    }

    void set(T new_val) {
        if (value_ == new_val) return;
        value_ = std::move(new_val);
        if (detail::g_app.rebuild_fn) detail::g_app.rebuild_fn();
    }

    void clear_subscribers() { subscribers_.clear(); }
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
        [](void* ptr) { static_cast<Trait<T>*>(ptr)->clear_subscribers(); },
        std::type_index(typeid(Trait<T>)),
        loc,
    });
    app.encode_index++;
    return p;
}

} // namespace phenotype

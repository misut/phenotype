module;
#include <functional>
#include <string>
#include <vector>
export module phenotype;

// ============================================================
// Low-level command buffer — JS shim reads and executes these
// ============================================================

extern "C" __attribute__((import_module("phenotype"), import_name("flush")))
void phenotype_flush(void);

extern "C" {
    alignas(4) unsigned char phenotype_cmd_buf[65536];
    unsigned int phenotype_cmd_len = 0;
    unsigned int phenotype_next_handle = 1; // 0 = document.body

    __attribute__((export_name("phenotype_get_cmd_buf")))
    unsigned char* phenotype_get_cmd_buf(void) { return phenotype_cmd_buf; }

    __attribute__((export_name("phenotype_get_cmd_len")))
    unsigned int phenotype_get_cmd_len(void) { return phenotype_cmd_len; }
}

// Callback registry for event handling (survives _start() return)
namespace {
    std::vector<std::function<void()>> g_callbacks;
}

extern "C" {
    __attribute__((export_name("phenotype_handle_event")))
    void phenotype_handle_event(unsigned int callback_id) {
        if (callback_id < g_callbacks.size())
            g_callbacks[callback_id]();
    }
}

constexpr unsigned int BUF_SIZE = 65536;

export namespace phenotype {

// ============================================================
// str — compile-time strlen for string literals, runtime for std::string
// ============================================================

struct str {
    char const* data;
    unsigned int len;

    template<unsigned int N>
    consteval str(char const (&s)[N]) : data(s), len(N - 1) {}
    str(char const* s, unsigned int l) : data(s), len(l) {}
    str(std::string const& s) : data(s.c_str()), len(static_cast<unsigned int>(s.size())) {}
};

// ============================================================
// Command buffer internals
// ============================================================

enum class Cmd : unsigned int {
    CreateElement  = 1,
    SetText        = 2,
    AppendChild    = 3,
    SetAttribute   = 4,
    SetStyle       = 5,
    SetInnerHTML   = 6,
    AddClass       = 7,
    RemoveChild    = 8,
    ClearChildren  = 9,
    RegisterClick  = 10,
};

namespace detail {

void ensure(unsigned int needed) {
    if (phenotype_cmd_len + needed > BUF_SIZE) {
        phenotype_flush();
        phenotype_cmd_len = 0;
    }
}

void write_u32(unsigned int value) {
    auto* p = reinterpret_cast<unsigned int*>(&phenotype_cmd_buf[phenotype_cmd_len]);
    *p = value;
    phenotype_cmd_len += 4;
}

void write_bytes(char const* data, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i)
        phenotype_cmd_buf[phenotype_cmd_len++] = static_cast<unsigned char>(data[i]);
    while (phenotype_cmd_len % 4 != 0)
        phenotype_cmd_buf[phenotype_cmd_len++] = 0;
}

unsigned int padded(unsigned int len) {
    return (len + 3) & ~3u;
}

void cmd_str(Cmd op, unsigned int handle, str s) {
    ensure(12 + padded(s.len));
    write_u32(static_cast<unsigned int>(op));
    write_u32(handle);
    write_u32(s.len);
    write_bytes(s.data, s.len);
}

void cmd_str2(Cmd op, unsigned int handle, str a, str b) {
    ensure(16 + padded(a.len) + padded(b.len));
    write_u32(static_cast<unsigned int>(op));
    write_u32(handle);
    write_u32(a.len);
    write_bytes(a.data, a.len);
    write_u32(b.len);
    write_bytes(b.data, b.len);
}

void cmd_u32x2(Cmd op, unsigned int a, unsigned int b) {
    ensure(12);
    write_u32(static_cast<unsigned int>(op));
    write_u32(a);
    write_u32(b);
}

} // namespace detail

// ============================================================
// Low-level API
// ============================================================

void flush() {
    if (phenotype_cmd_len == 0)
        return;
    phenotype_flush();
    phenotype_cmd_len = 0;
}

unsigned int create_element(str tag) {
    detail::ensure(12 + detail::padded(tag.len));
    auto handle = phenotype_next_handle++;
    detail::write_u32(static_cast<unsigned int>(Cmd::CreateElement));
    detail::write_u32(handle);
    detail::write_u32(tag.len);
    detail::write_bytes(tag.data, tag.len);
    return handle;
}

void set_text(unsigned int handle, str text) {
    detail::cmd_str(Cmd::SetText, handle, text);
}

void append_child(unsigned int parent, unsigned int child) {
    detail::cmd_u32x2(Cmd::AppendChild, parent, child);
}

void set_attribute(unsigned int handle, str key, str val) {
    detail::cmd_str2(Cmd::SetAttribute, handle, key, val);
}

void set_style(unsigned int handle, str prop, str val) {
    detail::cmd_str2(Cmd::SetStyle, handle, prop, val);
}

void set_inner_html(unsigned int handle, str html) {
    detail::cmd_str(Cmd::SetInnerHTML, handle, html);
}

void add_class(unsigned int handle, str cls) {
    detail::cmd_str(Cmd::AddClass, handle, cls);
}

void remove_child(unsigned int parent, unsigned int child) {
    detail::cmd_u32x2(Cmd::RemoveChild, parent, child);
}

void clear_children(unsigned int handle) {
    detail::ensure(8);
    detail::write_u32(static_cast<unsigned int>(Cmd::ClearChildren));
    detail::write_u32(handle);
}

void register_click(unsigned int handle, unsigned int callback_id) {
    detail::cmd_u32x2(Cmd::RegisterClick, handle, callback_id);
}

// ============================================================
// Scope — implicit parent tracking for declarative DSL
// ============================================================

struct Scope {
    unsigned int handle;
    std::function<void()> builder;

    Scope(unsigned int h) : handle(h) {}
    Scope(unsigned int h, std::function<void()> b) : handle(h), builder(std::move(b)) {}

    void mutate() {
        if (!builder) return;
        clear_children(handle);
        auto* prev = current();
        set_current(this);
        builder();
        set_current(prev);
        flush();
    }

    static Scope*& current() {
        static Scope* s = nullptr;
        return s;
    }

    static void set_current(Scope* s) { current() = s; }
};

// ============================================================
// Trait<T> — reactive state with partial mutation
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
            // avoid duplicates
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
        for (auto* scope : subscribers_)
            scope->mutate();
    }
};

// Global state storage (heap-allocated, survives _start())
namespace detail {
    struct StateSlot {
        void* ptr;
        void (*deleter)(void*);
        ~StateSlot() { if (deleter) deleter(ptr); }
    };
    std::vector<StateSlot> g_states;
}

template<typename T>
Trait<T>* encode(T initial) {
    auto* p = new Trait<T>(std::move(initial));
    detail::g_states.push_back({p, [](void* ptr) { delete static_cast<Trait<T>*>(ptr); }});
    return p;
}

// ============================================================
// DSL Components
// ============================================================

// Text — renders a <span> with text content
void Text(str content) {
    auto h = create_element("span");
    set_text(h, content);
    append_child(Scope::current()->handle, h);
}

// Button — renders a <button>, registers click callback
void Button(str label, std::function<void()> on_click = {}) {
    auto h = create_element("button");
    set_text(h, label);
    append_child(Scope::current()->handle, h);
    if (on_click) {
        auto id = static_cast<unsigned int>(g_callbacks.size());
        g_callbacks.push_back(std::move(on_click));
        register_click(h, id);
    }
}

// Layout helper — creates a container div, runs children inside its scope
namespace detail {

void open_container(str tag, std::function<void()> builder) {
    auto h = create_element(tag);
    append_child(Scope::current()->handle, h);
    // Heap-allocate scope so it survives for mutation
    auto* scope = new Scope(h, std::move(builder));
    auto* prev = Scope::current();
    Scope::set_current(scope);
    scope->builder();
    Scope::set_current(prev);
}

// Variadic: render each argument as a child
template<typename Arg>
void render_one(Arg&& arg) {
    // If arg is a std::function/lambda, call it (it will emit commands)
    if constexpr (std::is_invocable_v<Arg>) {
        arg();
    }
    // otherwise it's already been rendered by its own constructor
}

template<typename... Args>
void open_container_variadic(str tag, Args&&... args) {
    auto h = create_element(tag);
    append_child(Scope::current()->handle, h);
    Scope child_scope(h);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (render_one(std::forward<Args>(args)), ...);
    Scope::set_current(prev);
}

} // namespace detail

// Layout helper — wraps children in a flex div
namespace detail {

template<typename... Args>
void flex_container(str flex_dir, str gap, Args&&... args) {
    auto h = create_element("div");
    append_child(Scope::current()->handle, h);
    set_style(h, "display", "flex");
    set_style(h, "flexDirection", flex_dir);
    if (gap.len > 0)
        set_style(h, "gap", gap);
    Scope child_scope(h);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (render_one(std::forward<Args>(args)), ...);
    Scope::set_current(prev);
}

template<typename F>
void flex_container_builder(str flex_dir, str gap, F&& builder) {
    auto wrapped = [&builder, flex_dir, gap] {
        set_style(Scope::current()->handle, "display", "flex");
        set_style(Scope::current()->handle, "flexDirection", flex_dir);
        if (gap.len > 0)
            set_style(Scope::current()->handle, "gap", gap);
        builder();
    };
    open_container("div", std::move(wrapped));
}

} // namespace detail (layout)

// Column — vertical flex container
// Single lambda: Column([&] { ... })
template<typename F>
    requires std::is_invocable_v<F>
void Column(F&& builder) {
    detail::flex_container_builder("column", "", std::forward<F>(builder));
}

// Multiple children: Column(child1, child2, ...)
template<typename A, typename B, typename... Rest>
void Column(A&& a, B&& b, Rest&&... rest) {
    detail::flex_container("column", "", std::forward<A>(a),
                           std::forward<B>(b), std::forward<Rest>(rest)...);
}

// Row — horizontal flex container
template<typename F>
    requires std::is_invocable_v<F>
void Row(F&& builder) {
    detail::flex_container_builder("row", "8px", std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void Row(A&& a, B&& b, Rest&&... rest) {
    detail::flex_container("row", "8px", std::forward<A>(a),
                           std::forward<B>(b), std::forward<Rest>(rest)...);
}

// Box — generic container
template<typename F>
    requires std::is_invocable_v<F>
void Box(F&& builder) {
    detail::open_container("div", std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void Box(A&& a, B&& b, Rest&&... rest) {
    detail::open_container_variadic("div", std::forward<A>(a),
                                    std::forward<B>(b), std::forward<Rest>(rest)...);
}

// ============================================================
// express() — application entry point
// ============================================================

template<typename F>
void express(F&& app_fn) {
    static Scope root(0); // handle 0 = document.body
    Scope::set_current(&root);
    app_fn();
    flush();
    Scope::set_current(nullptr);
}

} // namespace phenotype

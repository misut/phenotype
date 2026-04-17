module;
#include <algorithm>
#include <concepts>
#include <cstring>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#ifdef __wasi__
#include "phenotype_host.h"
#endif
export module phenotype;

export import phenotype.diag;
export import phenotype.types;
export import phenotype.state;
export import phenotype.layout;
export import phenotype.paint;
export import phenotype.commands;
export import phenotype.theme_json;
export import phenotype.host;

export namespace phenotype {

namespace detail {
// Append a freshly allocated node to the current scope's children, if any.
inline void attach_to_scope(NodeHandle h) {
    if (auto* s = Scope::current())
        node_at(s->node).children.push_back(h);
}
} // namespace detail

// ============================================================
// widget:: — leaf components (text, code, link, button, text_field)
// ============================================================

namespace widget {

// text — single string label, inherits text_align from current scope.
inline void text(str content) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.text = std::string(content.data, content.len);
    node.font_size = detail::g_app.theme.body_font_size;
    node.text_color = detail::g_app.theme.foreground;

    if (auto* s = Scope::current()) {
        node.style.text_align = detail::node_at(s->node).style.text_align;
        detail::node_at(s->node).children.push_back(h);
    }
}

// link — clickable hyperlink. URL opening is dispatched via
// detail::g_open_url, a function pointer set by the backend module.
inline void link(str label, str href) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.text = std::string(label.data, label.len);
    node.font_size = detail::g_app.theme.small_font_size;
    node.text_color = detail::g_app.theme.accent;
    node.hover_text_color = {29, 78, 216, 255};
    node.url = std::string(href.data, href.len);
    node.cursor_type = 1;
    node.interaction_role = InteractionRole::Link;

    auto url_copy = node.url;
    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([url_copy] {
    #ifdef __wasi__
        phenotype_open_url(url_copy.c_str(),
                           static_cast<unsigned int>(url_copy.size()));
#else
        if (detail::g_open_url)
            detail::g_open_url(url_copy.c_str(),
                               static_cast<unsigned int>(url_copy.size()));
#endif
    });
    detail::g_app.callback_roles.push_back(InteractionRole::Link);
    node.callback_id = id;

    detail::attach_to_scope(h);
}

// code — monospaced code block with subtle background and border.
inline void code(str content) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.text = std::string(content.data, content.len);
    node.font_size = detail::g_app.theme.code_font_size;
    node.text_color = detail::g_app.theme.foreground;
    node.mono = true;
    node.background = detail::g_app.theme.code_bg;
    node.border_color = detail::g_app.theme.border;
    node.border_width = 1;
    node.border_radius = 6;
    node.style.padding[0] = 16; node.style.padding[1] = 16;
    node.style.padding[2] = 16; node.style.padding[3] = 16;

    detail::attach_to_scope(h);
}

// button<Msg> — click posts a copy of `msg` and triggers a rebuild.
template<typename Msg>
inline void button(str label, Msg msg) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.text = std::string(label.data, label.len);
    node.font_size = detail::g_app.theme.body_font_size;
    node.text_color = detail::g_app.theme.foreground;
    node.background = detail::g_app.theme.code_bg;
    node.hover_background = detail::g_app.theme.border;
    node.border_radius = 4;
    node.style.padding[0] = 6; node.style.padding[1] = 12;
    node.style.padding[2] = 6; node.style.padding[3] = 12;
    node.cursor_type = 1;
    node.interaction_role = InteractionRole::Button;

    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([msg = std::move(msg)] {
        detail::post<Msg>(msg);
        detail::trigger_rebuild();
    });
    detail::g_app.callback_roles.push_back(InteractionRole::Button);
    node.callback_id = id;

    detail::attach_to_scope(h);
}

namespace _impl {
template<typename Msg>
inline void toggle(str label, bool active, Msg msg,
                   float corner_radius, Decoration active_decoration,
                   InteractionRole role) {
    auto row_h = ::phenotype::detail::alloc_node();
    {
        auto& row = ::phenotype::detail::node_at(row_h);
        row.style.flex_direction = FlexDirection::Row;
        row.style.cross_align    = CrossAxisAlignment::Center;
        row.style.gap            = 8;
    }
    ::phenotype::detail::attach_to_scope(row_h);

    auto id = static_cast<unsigned int>(
        ::phenotype::detail::g_app.callbacks.size());
    ::phenotype::detail::g_app.callbacks.push_back([msg = std::move(msg)] {
        ::phenotype::detail::post<Msg>(msg);
        ::phenotype::detail::trigger_rebuild();
    });
    ::phenotype::detail::g_app.callback_roles.push_back(role);

    {
        auto box_h = ::phenotype::detail::alloc_node();
        auto& box  = ::phenotype::detail::node_at(box_h);
        box.style.max_width    = 16;
        box.style.fixed_height = 16;
        box.border_radius      = corner_radius;
        box.cursor_type        = 1;
        box.callback_id        = id;
        box.interaction_role   = role;
        if (active) {
            box.background   = ::phenotype::detail::g_app.theme.accent;
            box.border_color = ::phenotype::detail::g_app.theme.accent;
            box.decoration   = active_decoration;
        } else {
            box.background   = {255, 255, 255, 255};
            box.border_color = ::phenotype::detail::g_app.theme.border;
        }
        box.border_width = 1;
        ::phenotype::detail::node_at(row_h).children.push_back(box_h);
    }

    {
        auto lbl_h = ::phenotype::detail::alloc_node();
        auto& lbl  = ::phenotype::detail::node_at(lbl_h);
        lbl.text        = std::string(label.data, label.len);
        lbl.font_size   = ::phenotype::detail::g_app.theme.body_font_size;
        lbl.text_color  = ::phenotype::detail::g_app.theme.foreground;
        lbl.cursor_type = 1;
        lbl.callback_id = id;
        lbl.focusable   = false;
        lbl.interaction_role = role;
        ::phenotype::detail::node_at(row_h).children.push_back(lbl_h);
    }
}
} // namespace _impl

template<typename Msg>
inline void checkbox(str label, bool checked, Msg msg) {
    _impl::toggle(label, checked, std::move(msg),
                  /*corner_radius=*/3.0f, Decoration::Check,
                  InteractionRole::Checkbox);
}

template<typename Msg>
inline void radio(str label, bool selected, Msg msg) {
    _impl::toggle(label, selected, std::move(msg),
                  /*corner_radius=*/8.0f, Decoration::Dot,
                  InteractionRole::Radio);
}

template<typename Msg>
inline void text_field(str hint, std::string const& current,
                       Msg(*mapper)(std::string)) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.is_input = true;
    node.interaction_role = InteractionRole::TextField;
    node.placeholder = std::string(hint.data, hint.len);

    node.text = current.empty() ? node.placeholder : current;
    node.font_size = detail::g_app.theme.body_font_size;
    node.text_color = current.empty() ? detail::g_app.theme.muted
                                      : detail::g_app.theme.foreground;
    node.background = {255, 255, 255, 255};
    node.border_color = detail::g_app.theme.border;
    node.border_width = 1;
    node.border_radius = 4;
    node.style.padding[0] = 8; node.style.padding[1] = 12;
    node.style.padding[2] = 8; node.style.padding[3] = 12;
    node.cursor_type = 1;

    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([] {});
    detail::g_app.callback_roles.push_back(InteractionRole::TextField);
    using MapperFn = Msg(*)(std::string);
    auto* mapper_storage = new MapperFn(mapper);
    detail::g_app.input_handlers.push_back({
        id,
        InputHandler{
            current,
            mapper_storage,
            [](void* state, std::string s) {
                auto fn = *static_cast<MapperFn*>(state);
                detail::post<Msg>(fn(std::move(s)));
                detail::trigger_rebuild();
            },
            [](void* state) {
                delete static_cast<MapperFn*>(state);
            }
        }
    });
    node.callback_id = id;
    detail::g_app.input_nodes.push_back({id, h});

    detail::attach_to_scope(h);
}

inline void image(str url, float width, float height) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.image_url = std::string(url.data, url.len);
    node.style.max_width = width;
    node.style.fixed_height = height;
    detail::attach_to_scope(h);
}

} // namespace widget

// ============================================================
// Theme
// ============================================================

inline void set_theme(Theme const& theme) {
    detail::g_app.theme = theme;
    detail::clear_measure_cache();
}

inline Theme const& current_theme() noexcept {
    return detail::g_app.theme;
}

// ============================================================
// run<State, Msg> — application entry point.
// ============================================================
//
// Native: run(host, view, update) — host satisfies host_platform concept.
// WASM:   run(view, update)       — uses phenotype_host.h C linkage.

#ifndef __wasi__
template<typename State, typename Msg, host_platform Host,
         typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
void run(Host& host, View view, Update update) {
    static Host* saved_host = nullptr;
    static State state{};
    static View saved_view{view};
    static Update saved_update{update};
    saved_host = &host;
    state = State{};
    saved_view = std::move(view);
    saved_update = std::move(update);
    detail::g_app.input_debug = {};
    detail::g_app.callback_roles.clear();

    detail::install_app_runner([] {
        auto& host = *saved_host;
        auto t0 = metrics::detail::now_ns();

        auto msgs = detail::drain<Msg>();
        for (auto& m : msgs)
            saved_update(state, std::move(m));
        auto t1 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t1 - t0, {{"phase", "update"}});

        auto& app = detail::g_app;
        app.prev_root = app.root;
        std::swap(app.arena, app.prev_arena);
        app.arena.reset();
        app.callbacks.clear();
        app.callback_roles.clear();
        app.input_handlers.clear();
        app.input_nodes.clear();

        auto root_h = detail::alloc_node();
        {
            auto& root = detail::node_at(root_h);
            root.style.flex_direction = FlexDirection::Column;
            root.background = app.theme.background;
        }
        app.root = root_h;

        Scope scope(root_h);
        Scope::set_current(&scope);
        saved_view(state);
        Scope::set_current(nullptr);
        auto t2 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t2 - t1, {{"phase", "view"}});

        if (app.prev_root.valid())
            detail::diff_and_copy_layout(app.prev_root, root_h,
                                         app.prev_arena, app.arena);

        float cw = host.canvas_width();
        detail::layout_node(host, root_h, cw);
        auto t3 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t3 - t2, {{"phase", "layout"}});

        app.focusable_ids.clear();
        emit_clear(host, app.theme.background);
        float vh = host.canvas_height();
        detail::paint_node(host, host, root_h, 0, 0, app.scroll_y, vh);
        auto t4 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t4 - t3, {{"phase", "paint"}});

        flush_if_changed(host);
        auto t5 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t5 - t4, {{"phase", "flush"}});

        auto total = t5 - t0;
        metrics::inst::frame_duration.record(total);
        metrics::inst::rebuilds.add();
        log::debug("phenotype.runner",
            "rebuild #{} total={}us update={}us view={}us layout={}us paint={}us flush={}us",
            metrics::inst::rebuilds.total(), total / 1000,
            (t1 - t0) / 1000, (t2 - t1) / 1000, (t3 - t2) / 1000,
            (t4 - t3) / 1000, (t5 - t4) / 1000);
    });
    detail::trigger_rebuild();
}
#else // __wasi__
template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
void run(View view, Update update) {
    static State state{};
    static View saved_view{view};
    static Update saved_update{update};
    state = State{};
    saved_view = std::move(view);
    saved_update = std::move(update);
    detail::g_app.input_debug = {};
    detail::g_app.callback_roles.clear();

    detail::install_app_runner([] {
        auto t0 = metrics::detail::now_ns();

        auto msgs = detail::drain<Msg>();
        for (auto& m : msgs)
            saved_update(state, std::move(m));
        auto t1 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t1 - t0, {{"phase", "update"}});

        auto& app = detail::g_app;
        app.prev_root = app.root;
        std::swap(app.arena, app.prev_arena);
        app.arena.reset();
        app.callbacks.clear();
        app.callback_roles.clear();
        app.input_handlers.clear();
        app.input_nodes.clear();

        auto root_h = detail::alloc_node();
        {
            auto& root = detail::node_at(root_h);
            root.style.flex_direction = FlexDirection::Column;
            root.background = app.theme.background;
        }
        app.root = root_h;

        Scope scope(root_h);
        Scope::set_current(&scope);
        saved_view(state);
        Scope::set_current(nullptr);
        auto t2 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t2 - t1, {{"phase", "view"}});

        if (app.prev_root.valid())
            detail::diff_and_copy_layout(app.prev_root, root_h,
                                         app.prev_arena, app.arena);

        float cw = phenotype_get_canvas_width();
        detail::layout_node(root_h, cw);
        auto t3 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t3 - t2, {{"phase", "layout"}});

        app.focusable_ids.clear();
        detail::wasi_emit_clear(app.theme.background);
        float vh = phenotype_get_canvas_height();
        detail::wasi_paint_node(root_h, 0, 0, app.scroll_y, vh);
        auto t4 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t4 - t3, {{"phase", "paint"}});

        detail::wasi_flush_if_changed();
        auto t5 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t5 - t4, {{"phase", "flush"}});

        auto total = t5 - t0;
        metrics::inst::frame_duration.record(total);
        metrics::inst::rebuilds.add();
        log::debug("phenotype.runner",
            "rebuild #{} total={}us update={}us view={}us layout={}us paint={}us flush={}us",
            metrics::inst::rebuilds.total(), total / 1000,
            (t1 - t0) / 1000, (t2 - t1) / 1000, (t3 - t2) / 1000,
            (t4 - t3) / 1000, (t5 - t4) / 1000);
    });
    detail::trigger_rebuild();
}
#endif

// ============================================================
// Container helpers
// ============================================================

namespace detail {

inline void open_container(NodeHandle container, std::function<void()> builder) {
    attach_to_scope(container);
    Scope scope(container);
    auto* prev = Scope::current();
    Scope::set_current(&scope);
    builder();
    Scope::set_current(prev);
}

template<typename Arg>
void render_one(Arg&& arg) {
    if constexpr (std::is_invocable_v<Arg>) {
        arg();
    }
}

} // namespace detail

// ============================================================
// layout:: — containers, spacing, and structural components
// ============================================================

namespace layout {

template<typename F>
    requires std::is_invocable_v<F>
void column(F&& builder) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Column;
    node.style.gap = 12;
    detail::open_container(h, std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void column(A&& a, B&& b, Rest&&... rest) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Column;
    node.style.gap = 12;
    detail::attach_to_scope(h);
    Scope child_scope(h);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (detail::render_one(std::forward<A>(a)),
     detail::render_one(std::forward<B>(b)),
     (detail::render_one(std::forward<Rest>(rest)), ...));
    Scope::set_current(prev);
}

template<typename F>
    requires std::is_invocable_v<F>
void row(F&& builder) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Row;
    node.style.cross_align = CrossAxisAlignment::Center;
    node.style.gap = 8;
    detail::open_container(h, std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void row(A&& a, B&& b, Rest&&... rest) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Row;
    node.style.cross_align = CrossAxisAlignment::Center;
    node.style.gap = 8;
    detail::attach_to_scope(h);
    Scope child_scope(h);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (detail::render_one(std::forward<A>(a)),
     detail::render_one(std::forward<B>(b)),
     (detail::render_one(std::forward<Rest>(rest)), ...));
    Scope::set_current(prev);
}

template<typename F>
    requires std::is_invocable_v<F>
void box(F&& builder) {
    auto h = detail::alloc_node();
    detail::open_container(h, std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void box(A&& a, B&& b, Rest&&... rest) {
    auto h = detail::alloc_node();
    detail::attach_to_scope(h);
    Scope child_scope(h);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (detail::render_one(std::forward<A>(a)),
     detail::render_one(std::forward<B>(b)),
     (detail::render_one(std::forward<Rest>(rest)), ...));
    Scope::set_current(prev);
}

template<typename F>
    requires std::is_invocable_v<F>
void sized_box(float max_width, F&& builder) {
    auto h = detail::alloc_node();
    detail::node_at(h).style.max_width = max_width;
    detail::open_container(h, std::forward<F>(builder));
}

inline void scaffold(std::function<void()> top_bar,
                     std::function<void()> content,
                     std::function<void()> bottom_bar = {}) {
    if (top_bar) {
        auto header_h = detail::alloc_node();
        {
            auto& header = detail::node_at(header_h);
            header.style.flex_direction = FlexDirection::Column;
            header.style.gap = 8;
            header.style.cross_align = CrossAxisAlignment::Center;
            header.style.text_align = TextAlign::Center;
            header.background = detail::g_app.theme.hero_bg;
            header.style.padding[0] = 48; header.style.padding[1] = 24;
            header.style.padding[2] = 48; header.style.padding[3] = 24;
        }

        detail::open_container(header_h, std::move(top_bar));
        auto const& t = detail::g_app.theme;
        auto& header = detail::node_at(header_h);
        if (header.children.size() >= 1) {
            auto& c0 = detail::node_at(header.children[0]);
            c0.font_size = t.hero_title_size;
            c0.text_color = t.hero_fg;
        }
        if (header.children.size() >= 2) {
            auto& c1 = detail::node_at(header.children[1]);
            c1.font_size = t.hero_subtitle_size;
            c1.text_color = t.hero_muted;
        }
    }

    if (content) {
        auto main_h = detail::alloc_node();
        {
            auto& main = detail::node_at(main_h);
            main.style.flex_direction = FlexDirection::Column;
            main.style.gap = 32;
            main.style.max_width = detail::g_app.theme.max_content_width;
            main.style.padding[0] = 32; main.style.padding[1] = 24;
            main.style.padding[2] = 32; main.style.padding[3] = 24;
        }

        detail::open_container(main_h, std::move(content));
    }

    if (bottom_bar) {
        auto footer_h = detail::alloc_node();
        {
            auto& footer = detail::node_at(footer_h);
            footer.style.flex_direction = FlexDirection::Row;
            footer.style.gap = 0;
            footer.style.main_align = MainAxisAlignment::Center;
            footer.style.cross_align = CrossAxisAlignment::Center;
            footer.style.padding[0] = 32; footer.style.padding[1] = 24;
            footer.style.padding[2] = 32; footer.style.padding[3] = 24;
            footer.border_color = detail::g_app.theme.border;
            footer.border_width = 1;
        }

        detail::open_container(footer_h, std::move(bottom_bar));
        auto const& t = detail::g_app.theme;
        for (auto child_h : detail::node_at(footer_h).children) {
            auto& child = detail::node_at(child_h);
            child.font_size = t.small_font_size;
            child.text_color = t.muted;
        }
    }
}

template<typename F>
    requires std::is_invocable_v<F>
void surface(F&& builder) {
    auto h = detail::alloc_node();
    detail::open_container(h, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void card(F&& builder) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.border_radius = 8;
    node.background = {255, 255, 255, 255};
    node.style.padding[0] = 16; node.style.padding[1] = 16;
    node.style.padding[2] = 16; node.style.padding[3] = 16;
    detail::open_container(h, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void list_items(F&& builder) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Column;
    node.style.gap = 6;
    node.style.padding[3] = 24;
    detail::open_container(h, std::forward<F>(builder));
}

inline void item(str content) {
    auto row_h = detail::alloc_node();
    {
        auto& row = detail::node_at(row_h);
        row.style.flex_direction = FlexDirection::Row;
        row.style.gap = 8;
    }

    auto bullet_h = detail::alloc_node();
    {
        auto& bullet = detail::node_at(bullet_h);
        bullet.text = "\xe2\x80\xa2";
        bullet.font_size = detail::g_app.theme.body_font_size;
        bullet.text_color = detail::g_app.theme.foreground;
    }
    detail::node_at(row_h).children.push_back(bullet_h);

    auto text_h = detail::alloc_node();
    {
        auto& text = detail::node_at(text_h);
        text.text = std::string(content.data, content.len);
        text.font_size = detail::g_app.theme.body_font_size;
        text.text_color = detail::g_app.theme.foreground;
    }
    detail::node_at(row_h).children.push_back(text_h);

    detail::attach_to_scope(row_h);
}

inline void divider() {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.fixed_height = 1;
    node.background = detail::g_app.theme.border;

    detail::attach_to_scope(h);
}

inline void spacer(unsigned int height_px) {
    auto h = detail::alloc_node();
    detail::node_at(h).style.fixed_height = static_cast<float>(height_px);
    detail::attach_to_scope(h);
}

} // namespace layout

// ============================================================
// Event dispatch — pure functions (no host dependency).
// Backend modules (phenotype.wasm / phenotype.native) wire
// these into their platform event loops.
// ============================================================

namespace detail {

inline bool is_utf8_continuation(unsigned char byte) {
    return (byte & 0xC0u) == 0x80u;
}

inline std::size_t clamp_utf8_boundary(std::string const& value, std::size_t pos) {
    pos = std::min(pos, value.size());
    while (pos > 0 && pos < value.size()
           && is_utf8_continuation(static_cast<unsigned char>(value[pos]))) {
        --pos;
    }
    return pos;
}

inline std::size_t prev_utf8_boundary(std::string const& value, std::size_t pos) {
    pos = clamp_utf8_boundary(value, pos);
    if (pos == 0) return 0;
    --pos;
    while (pos > 0
           && is_utf8_continuation(static_cast<unsigned char>(value[pos]))) {
        --pos;
    }
    return pos;
}

inline std::size_t next_utf8_boundary(std::string const& value, std::size_t pos) {
    pos = clamp_utf8_boundary(value, pos);
    if (pos >= value.size()) return value.size();
    ++pos;
    while (pos < value.size()
           && is_utf8_continuation(static_cast<unsigned char>(value[pos]))) {
        ++pos;
    }
    return pos;
}

inline InteractionRole callback_role(unsigned int callback_id) {
    if (callback_id < g_app.callback_roles.size())
        return g_app.callback_roles[callback_id];
    return InteractionRole::None;
}

inline InputHandler* find_input_handler(unsigned int callback_id) {
    for (auto& [id, handler] : g_app.input_handlers) {
        if (id == callback_id)
            return &handler;
    }
    return nullptr;
}

inline unsigned int resolved_caret_pos(std::string const& value) {
    if (g_app.caret_pos == 0xFFFFFFFFu)
        return static_cast<unsigned int>(value.size());
    return static_cast<unsigned int>(
        clamp_utf8_boundary(value, static_cast<std::size_t>(g_app.caret_pos)));
}

inline void assign_focus(unsigned int callback_id) {
    g_app.focused_id = callback_id;
    if (auto const* handler = find_input_handler(callback_id)) {
        g_app.caret_pos = static_cast<unsigned int>(
            clamp_utf8_boundary(handler->current, handler->current.size()));
    } else {
        g_app.caret_pos = 0xFFFFFFFFu;
    }
    g_app.caret_visible = true;
}

inline char const* input_key_detail_name(unsigned int key_type) {
    switch (key_type) {
        case 0: return "char";
        case 1: return "backspace";
        case 2: return "arrow-left";
        case 3: return "arrow-right";
        case 4: return "home";
        case 5: return "end";
        default: return "key";
    }
}

inline std::string callback_id_text(unsigned int callback_id) {
    if (callback_id == 0xFFFFFFFFu)
        return "none";
    return std::to_string(callback_id);
}

inline void note_input_event(char const* event,
                             char const* source,
                             char const* detail,
                             char const* result,
                             unsigned int callback_id) {
    auto role = callback_role(callback_id);
    auto focused_role = callback_role(g_app.focused_id);
    auto& snapshot = g_app.input_debug;
    snapshot.event = event ? event : "";
    snapshot.source = source ? source : "";
    snapshot.detail = detail ? detail : "";
    snapshot.result = result ? result : "";
    snapshot.callback_id = callback_id;
    snapshot.role = interaction_role_name(role);
    snapshot.focused_id = g_app.focused_id;
    snapshot.focused_role = interaction_role_name(focused_role);
    snapshot.hovered_id = g_app.hovered_id;
    snapshot.scroll_y = g_app.scroll_y;
    snapshot.caret_pos = g_app.caret_pos;
    snapshot.focused_is_input = find_input_handler(g_app.focused_id) != nullptr;

    metrics::inst::input_events.add(1, {
        {"event", snapshot.event},
        {"source", snapshot.source},
        {"detail", snapshot.detail},
        {"result", snapshot.result},
        {"callback_id", callback_id_text(callback_id)},
        {"role", snapshot.role},
    });
}

inline bool handle_event(unsigned int callback_id,
                         char const* source = "core",
                         char const* detail = "activate") {
    if (callback_id < g_app.callbacks.size()) {
        log::trace("phenotype.input", "click id={}", callback_id);
        g_app.callbacks[callback_id]();
        note_input_event("click", source, detail, "handled", callback_id);
        return true;
    }
    log::warn("phenotype.input",
        "click id={} out of range (size={})",
        callback_id, g_app.callbacks.size());
    note_input_event("click", source, detail, "ignored", callback_id);
    return false;
}

inline bool set_hover_id(unsigned int callback_id,
                         char const* source = "core",
                         char const* detail = "pointer-move") {
    if (g_app.hovered_id == callback_id) {
        note_input_event("hover", source, detail, "ignored", callback_id);
        return false;
    }
    g_app.hovered_id = callback_id;
    note_input_event("hover", source, detail, "handled", callback_id);
    return true;
}

inline bool set_focus_id(unsigned int callback_id,
                         char const* source = "core",
                         char const* detail = "focus-change") {
    if (g_app.focused_id == callback_id) {
        note_input_event("focus", source, detail, "ignored", callback_id);
        return false;
    }
    assign_focus(callback_id);
    note_input_event("focus", source, detail, "handled", callback_id);
    return true;
}

inline unsigned int get_focused_id() {
    return g_app.focused_id;
}

inline float get_total_height() {
    if (auto* root_ptr = g_app.arena.get(g_app.root))
        return root_ptr->height;
    return 0;
}

inline bool handle_tab(unsigned int reverse,
                       char const* source = "core",
                       char const* detail = nullptr) {
    auto detail_name = detail ? detail : (reverse ? "shift-tab" : "tab");
    if (g_app.focusable_ids.empty()) {
        note_input_event("tab", source, detail_name, "ignored", g_app.focused_id);
        return false;
    }
    int n = static_cast<int>(g_app.focusable_ids.size());
    int idx = -1;
    for (int i = 0; i < n; ++i) {
        if (g_app.focusable_ids[i] == g_app.focused_id) { idx = i; break; }
    }
    if (reverse)
        idx = (idx <= 0) ? n - 1 : idx - 1;
    else
        idx = (idx < 0 || idx >= n - 1) ? 0 : idx + 1;
    assign_focus(g_app.focusable_ids[idx]);
    note_input_event("tab", source, detail_name, "handled", g_app.focused_id);
    return true;
}

inline void toggle_caret() {
    g_app.caret_visible = !g_app.caret_visible;
}

inline bool apply_key_to_string(unsigned int key_type,
                                unsigned int codepoint,
                                std::string& value,
                                std::size_t& caret,
                                bool& text_changed) {
    text_changed = false;
    switch (key_type) {
        case 0: {
            char buf[4];
            unsigned int len = 0;
            if (codepoint < 0x80) {
                buf[0] = static_cast<char>(codepoint); len = 1;
            } else if (codepoint < 0x800) {
                buf[0] = static_cast<char>(0xC0 | (codepoint >> 6));
                buf[1] = static_cast<char>(0x80 | (codepoint & 0x3F)); len = 2;
            } else if (codepoint < 0x10000) {
                buf[0] = static_cast<char>(0xE0 | (codepoint >> 12));
                buf[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                buf[2] = static_cast<char>(0x80 | (codepoint & 0x3F)); len = 3;
            } else {
                buf[0] = static_cast<char>(0xF0 | (codepoint >> 18));
                buf[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                buf[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                buf[3] = static_cast<char>(0x80 | (codepoint & 0x3F)); len = 4;
            }
            value.insert(caret, buf, len);
            caret += len;
            text_changed = true;
            return true;
        }
        case 1: {
            if (caret == 0)
                return false;
            auto start = prev_utf8_boundary(value, caret);
            value.erase(start, caret - start);
            caret = start;
            text_changed = true;
            return true;
        }
        case 2:
            if (caret == 0)
                return false;
            caret = prev_utf8_boundary(value, caret);
            return true;
        case 3:
            if (caret >= value.size())
                return false;
            caret = next_utf8_boundary(value, caret);
            return true;
        case 4:
            if (caret == 0)
                return false;
            caret = 0;
            return true;
        case 5:
            if (caret >= value.size())
                return false;
            caret = value.size();
            return true;
        default:
            return false;
    }
}

inline bool handle_key(unsigned int key_type, unsigned int codepoint,
                       char const* source = "core",
                       char const* detail = nullptr) {
    auto detail_name = detail ? detail : input_key_detail_name(key_type);
    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler) {
        note_input_event("key", source, detail_name, "ignored", g_app.focused_id);
        return false;
    }

    std::string value = handler->current;
    std::size_t caret = resolved_caret_pos(value);
    bool text_changed = false;
    if (!apply_key_to_string(key_type, codepoint, value, caret, text_changed)) {
        note_input_event("key", source, detail_name, "ignored", g_app.focused_id);
        return false;
    }

    g_app.caret_pos = static_cast<unsigned int>(caret);
    g_app.caret_visible = true;
    if (text_changed)
        handler->invoke(handler->state, std::move(value));
    note_input_event("key", source, detail_name, "handled", g_app.focused_id);
    return true;
}

#ifndef __wasi__
template<host_platform Host>
void repaint(Host& host, float scroll_y) {
    auto& app = g_app;
    app.scroll_y = scroll_y;
    if (!app.root.valid()) return;
    auto* root_ptr = app.arena.get(app.root);
    if (!root_ptr) return;
    float cw = host.canvas_width();
    if (cw != root_ptr->width)
        layout_node(host, app.root, cw);
    app.focusable_ids.clear();
    emit_clear(host, app.theme.background);
    float vh = host.canvas_height();
    paint_node(host, host, app.root, 0, 0, scroll_y, vh);
    flush_if_changed(host);
}
#else
inline void repaint(float scroll_y) {
    auto& app = g_app;
    app.scroll_y = scroll_y;
    if (!app.root.valid()) return;
    auto* root_ptr = app.arena.get(app.root);
    if (!root_ptr) return;
    float cw = phenotype_get_canvas_width();
    if (cw != root_ptr->width)
        layout_node(app.root, cw);
    app.focusable_ids.clear();
    wasi_emit_clear(app.theme.background);
    float vh = phenotype_get_canvas_height();
    wasi_paint_node(app.root, 0, 0, scroll_y, vh);
    wasi_flush_if_changed();
}
#endif

// Input buffer helpers — used by WASM exports for text input overlay.
inline bool focused_is_input() {
    return find_input_handler(g_app.focused_id) != nullptr;
}

inline unsigned int input_load_focused(unsigned char* buf, unsigned int buf_size) {
    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler)
        return 0;
    unsigned int n = static_cast<unsigned int>(handler->current.size());
    if (n > buf_size) n = buf_size;
    std::memcpy(buf, handler->current.data(), n);
    return n;
}

inline diag::InputDebugSnapshot const& current_input_debug_snapshot() {
    return g_app.input_debug;
}

inline InteractionRole focused_role() {
    return callback_role(g_app.focused_id);
}

inline unsigned int get_hovered_id() {
    return g_app.hovered_id;
}

inline float get_scroll_y() {
    return g_app.scroll_y;
}

inline unsigned int get_caret_pos() {
    return g_app.caret_pos;
}

inline void set_scroll_y(float scroll_y) {
    g_app.scroll_y = scroll_y;
}

inline void set_hover_id_without_event(unsigned int callback_id) {
    g_app.hovered_id = callback_id;
}

inline void input_commit(unsigned char const* buf, unsigned int len) {
    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler)
        return;
    std::string value(reinterpret_cast<char const*>(buf), len);
    g_app.caret_pos = static_cast<unsigned int>(value.size());
    handler->invoke(handler->state, std::move(value));
}

} // namespace detail

namespace diag {

inline InputDebugSnapshot input_debug_snapshot() {
    return detail::current_input_debug_snapshot();
}

} // namespace diag

} // namespace phenotype

// ============================================================
// WASM exports — called from JS
// ============================================================
#ifdef __wasi__
extern "C" {
    __attribute__((export_name("phenotype_repaint")))
    void phenotype_repaint(float scroll_y) {
        phenotype::detail::repaint(scroll_y);
    }

    __attribute__((export_name("phenotype_get_total_height")))
    float phenotype_get_total_height(void) {
        return phenotype::detail::get_total_height();
    }

    __attribute__((export_name("phenotype_handle_event")))
    void phenotype_handle_event(unsigned int callback_id) {
        phenotype::detail::handle_event(callback_id);
    }

    __attribute__((export_name("phenotype_set_hover")))
    void phenotype_set_hover(unsigned int callback_id) {
        if (phenotype::detail::set_hover_id(callback_id))
            phenotype_repaint(phenotype::detail::g_app.scroll_y);
    }

    __attribute__((export_name("phenotype_set_focus")))
    void phenotype_set_focus(unsigned int callback_id) {
        phenotype::detail::set_focus_id(callback_id);
        phenotype_repaint(phenotype::detail::g_app.scroll_y);
    }

    __attribute__((export_name("phenotype_get_focused_id")))
    unsigned int phenotype_get_focused_id(void) {
        return phenotype::detail::get_focused_id();
    }

    __attribute__((export_name("phenotype_handle_tab")))
    void phenotype_handle_tab(unsigned int reverse) {
        phenotype::detail::handle_tab(reverse);
        phenotype_repaint(phenotype::detail::g_app.scroll_y);
    }

    __attribute__((export_name("phenotype_toggle_caret")))
    void phenotype_toggle_caret(void) {
        phenotype::detail::toggle_caret();
        phenotype_repaint(phenotype::detail::g_app.scroll_y);
    }

    __attribute__((export_name("phenotype_handle_key")))
    void phenotype_handle_key(unsigned int key_type, unsigned int codepoint) {
        phenotype::detail::handle_key(key_type, codepoint);
    }

    // ---- Diagnostics ----

    constexpr unsigned int PHENOTYPE_DIAG_BUF_SIZE = 64u * 1024u;
    alignas(4) unsigned char phenotype_diag_buf[PHENOTYPE_DIAG_BUF_SIZE];
    unsigned int phenotype_diag_buf_len = 0;

    __attribute__((export_name("phenotype_diag_get_buf")))
    unsigned char* phenotype_diag_get_buf(void) { return phenotype_diag_buf; }
    __attribute__((export_name("phenotype_diag_get_buf_size")))
    unsigned int phenotype_diag_get_buf_size(void) { return PHENOTYPE_DIAG_BUF_SIZE; }
    __attribute__((export_name("phenotype_diag_get_len")))
    unsigned int phenotype_diag_get_len(void) { return phenotype_diag_buf_len; }

    __attribute__((export_name("phenotype_diag_export")))
    unsigned int phenotype_diag_export(void) {
        auto json = phenotype::diag::serialize_snapshot();
        if (json.size() > PHENOTYPE_DIAG_BUF_SIZE) { phenotype_diag_buf_len = 0; return 0; }
        std::memcpy(phenotype_diag_buf, json.data(), json.size());
        phenotype_diag_buf_len = static_cast<unsigned int>(json.size());
        return phenotype_diag_buf_len;
    }

    __attribute__((export_name("phenotype_diag_set_log_level")))
    void phenotype_diag_set_log_level(unsigned int level) {
        phenotype::log::set_level(static_cast<phenotype::log::Severity>(level));
    }

    __attribute__((export_name("phenotype_diag_reset")))
    void phenotype_diag_reset(void) { phenotype::metrics::reset_all(); }

    // ---- Theme from JSON ----

    unsigned char* phenotype_input_buf(void);

    __attribute__((export_name("phenotype_set_theme_json")))
    unsigned int phenotype_set_theme_json(unsigned int len) {
        auto* buf = phenotype_input_buf();
        std::string_view json_str(reinterpret_cast<char const*>(buf), len);
        auto result = phenotype::theme_from_json(json_str);
        if (!result) { phenotype::log::warn("phenotype.theme", "theme_from_json: {}", result.error()); return 0; }
        phenotype::set_theme(*result);
        return 1;
    }

    // ---- Text input buffer ----

    constexpr unsigned int PHENOTYPE_INPUT_BUF_SIZE = 4u * 1024u;
    alignas(4) unsigned char phenotype_input_buf_storage[PHENOTYPE_INPUT_BUF_SIZE];
    unsigned int phenotype_input_buf_len_value = 0;

    __attribute__((export_name("phenotype_input_buf")))
    unsigned char* phenotype_input_buf(void) { return phenotype_input_buf_storage; }
    __attribute__((export_name("phenotype_input_buf_size")))
    unsigned int phenotype_input_buf_size(void) { return PHENOTYPE_INPUT_BUF_SIZE; }
    __attribute__((export_name("phenotype_input_buf_len")))
    unsigned int phenotype_input_buf_len(void) { return phenotype_input_buf_len_value; }

    __attribute__((export_name("phenotype_focused_is_input")))
    unsigned int phenotype_focused_is_input(void) {
        return phenotype::detail::focused_is_input() ? 1 : 0;
    }

    __attribute__((export_name("phenotype_input_load_focused")))
    unsigned int phenotype_input_load_focused(void) {
        phenotype_input_buf_len_value = phenotype::detail::input_load_focused(
            phenotype_input_buf_storage, PHENOTYPE_INPUT_BUF_SIZE);
        return phenotype_input_buf_len_value;
    }

    __attribute__((export_name("phenotype_input_commit")))
    void phenotype_input_commit(unsigned int len) {
        if (len > PHENOTYPE_INPUT_BUF_SIZE) return;
        phenotype::detail::input_commit(phenotype_input_buf_storage, len);
    }
}
#endif // __wasi__

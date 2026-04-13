module;
#include <concepts>
#include <cstring>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
export module phenotype;

export import phenotype.diag;
export import phenotype.types;
export import phenotype.state;
export import phenotype.layout;
export import phenotype.paint;
export import phenotype.theme_json;

// ============================================================
// WASM imports — JS shim implements these
// ============================================================

extern "C" __attribute__((import_module("phenotype"), import_name("flush")))
void phenotype_flush(void);

extern "C" __attribute__((import_module("phenotype"), import_name("get_canvas_width")))
float phenotype_get_canvas_width(void);

extern "C" __attribute__((import_module("phenotype"), import_name("get_canvas_height")))
float phenotype_get_canvas_height(void);

extern "C" __attribute__((import_module("phenotype"), import_name("open_url")))
void phenotype_open_url(char const* url, unsigned int len);

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
//
// Widgets are non-container components that produce a single LayoutNode.
// Interactive widgets (button, text_field) are templated on the user's
// Msg type so the click closure carries only the message *value* — there
// is no caller-scope capture.

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

// link — clickable hyperlink that opens via the host JS shim.
inline void link(str label, str href) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.text = std::string(label.data, label.len);
    node.font_size = detail::g_app.theme.small_font_size;
    node.text_color = detail::g_app.theme.accent;
    node.hover_text_color = {29, 78, 216, 255}; // darker blue on hover
    node.url = std::string(href.data, href.len);
    node.cursor_type = 1; // pointer

    // Register click callback to open URL. The closure captures only the
    // immutable URL string (a value), not any caller-scope reference.
    auto url_copy = node.url;
    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([url_copy] {
        phenotype_open_url(url_copy.c_str(), static_cast<unsigned int>(url_copy.size()));
    });
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

    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([msg = std::move(msg)] {
        detail::post<Msg>(msg);
        detail::trigger_rebuild();
    });
    node.callback_id = id;

    detail::attach_to_scope(h);
}

// checkbox<Msg> / radio<Msg> share this body — checkbox uses border_radius
// 3 + Decoration::Check, radio uses border_radius 8 (half of the 16x16
// indicator, rounding it into a circle via the SDF rounded-rect shader)
// + Decoration::Dot.
//
// The widget is built as a row with two leaves:
//
//   * An indicator box — 16x16, owns the click callback id, is Tab-
//     focusable, draws the focus ring around its own snug bounds. When
//     active, it fills with theme.accent and the Decoration field tells
//     paint_node to draw a white V (checkbox) or white inner dot
//     (radio) on top of the fill.
//
//   * A label text leaf — shares the SAME callback id as the indicator
//     so clicks on the label dispatch the same Msg, but is marked
//     `focusable = false` so Tab does not visit it and the focus ring
//     never draws around the label text. The two LayoutNode fields
//     enabling this design (`focusable` and `decoration`) live in
//     phenotype.types.
namespace _impl {
template<typename Msg>
inline void toggle(str label, bool active, Msg msg,
                   float corner_radius, Decoration active_decoration) {
    // Outer row — pure layout, no callback. Children handle the click.
    auto row_h = ::phenotype::detail::alloc_node();
    {
        auto& row = ::phenotype::detail::node_at(row_h);
        row.style.flex_direction = FlexDirection::Row;
        row.style.cross_align    = CrossAxisAlignment::Center;
        row.style.gap            = 8;
    }
    ::phenotype::detail::attach_to_scope(row_h);

    // One callback closure shared by indicator + label. Both nodes
    // emit hit regions with this id, so clicking either dispatches
    // the same Msg; the indicator alone is in focusable_ids.
    auto id = static_cast<unsigned int>(
        ::phenotype::detail::g_app.callbacks.size());
    ::phenotype::detail::g_app.callbacks.push_back([msg = std::move(msg)] {
        ::phenotype::detail::post<Msg>(msg);
        ::phenotype::detail::trigger_rebuild();
    });

    // Indicator box — focus target, draws background + decoration glyph.
    {
        auto box_h = ::phenotype::detail::alloc_node();
        auto& box  = ::phenotype::detail::node_at(box_h);
        box.style.max_width    = 16;
        box.style.fixed_height = 16;
        box.border_radius      = corner_radius;
        box.cursor_type        = 1; // pointer
        box.callback_id        = id;
        // box.focusable defaults to true
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

    // Label — clickable via the same callback id, but NOT focusable so
    // Tab skips it and the focus ring never draws around the label text.
    {
        auto lbl_h = ::phenotype::detail::alloc_node();
        auto& lbl  = ::phenotype::detail::node_at(lbl_h);
        lbl.text        = std::string(label.data, label.len);
        lbl.font_size   = ::phenotype::detail::g_app.theme.body_font_size;
        lbl.text_color  = ::phenotype::detail::g_app.theme.foreground;
        lbl.cursor_type = 1; // pointer
        lbl.callback_id = id;
        lbl.focusable   = false;
        ::phenotype::detail::node_at(row_h).children.push_back(lbl_h);
    }
}
} // namespace _impl

// checkbox<Msg> — square 16x16 indicator + label. Click on either the
// indicator or the label posts `msg`; the update function is responsible
// for toggling state. Active state renders as an accent fill with a
// white V checkmark glyph drawn on top.
template<typename Msg>
inline void checkbox(str label, bool checked, Msg msg) {
    _impl::toggle(label, checked, std::move(msg),
                  /*corner_radius=*/3.0f, Decoration::Check);
}

// radio<Msg> — circular 16x16 indicator + label. Same click semantics
// as checkbox; the update function sets the new selection in state.
// Active state renders as an accent fill with a small white inner dot.
template<typename Msg>
inline void radio(str label, bool selected, Msg msg) {
    _impl::toggle(label, selected, std::move(msg),
                  /*corner_radius=*/8.0f, Decoration::Dot);
}

// text_field<Msg> — typing posts mapper(new_value)-derived messages.
// `mapper` is a stateless function pointer (stateless lambdas auto-convert)
// so we never store a closure that could capture caller-scope state.
template<typename Msg>
inline void text_field(str hint, std::string const& current,
                       Msg(*mapper)(std::string)) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.is_input = true;
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
    // Click callback is a no-op; focus is set via phenotype_set_focus.
    detail::g_app.callbacks.push_back([] {});
    // Input handler stores the current value, function pointer, and a
    // trampoline that calls mapper + post + trigger. Sparse by callback_id.
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

// image — display a bitmap (PNG, JPEG, GIF, SVG) at fixed dimensions.
// The URL is fetched lazily by the JS shim's persistent image cache;
// the first paint shows a neutral grey placeholder while the load is
// in flight, and the cache triggers a repaint once the texture is
// uploaded to the GPU. The image is stretched to fill (width, height)
// — aspect-ratio-correct fitting is the user's responsibility for
// now (compute both dimensions before calling).
inline void image(str url, float width, float height) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.image_url = std::string(url.data, url.len);
    // max_width is treated as a fixed intrinsic width by row layout
    // (PR #35), and as a max-clamp by column layout. Either way the
    // image ends up exactly `width` pixels wide.
    node.style.max_width = width;
    node.style.fixed_height = height;
    detail::attach_to_scope(h);
}

} // namespace widget

// ============================================================
// Theme — runtime-configurable design tokens
// ============================================================
//
// set_theme replaces the current Theme in the global app state and
// invalidates per-theme caches (currently just the measure_text
// cache, which is keyed on font size). Typical usage:
//
//   int main() {
//       phenotype::set_theme({
//           .background = {10,  10,  10, 255},
//           .foreground = {240, 240, 240, 255},
//           // any fields you omit keep their default value
//       });
//       phenotype::run<State, Msg>(view, update);
//       return 0;
//   }
//
// It can also be called from inside update() for dynamic theme
// switching (light/dark mode). set_theme never triggers a rebuild
// on its own — the next rebuild picks up the new values because
// paint / layout / widget code all read g_app.theme.X directly.
// Callers that need an immediate redraw from a non-message path
// (e.g. a timer) should post a no-op Msg afterwards.
inline void set_theme(Theme const& theme) {
    detail::g_app.theme = theme;
    detail::clear_measure_cache();
}

// Read-only accessor for use inside view() when an app needs to
// read a theme value (e.g. to compute a derived color or to pass
// a palette to a child view helper).
inline Theme const& current_theme() noexcept {
    return detail::g_app.theme;
}

// ============================================================
// run<State, Msg>(view, update) — application entry point.
// ============================================================
//
// Installs an app runner that drains the message queue, folds via
// update, and re-runs view to rebuild the tree. Initial render with
// State{} and an empty queue.
template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
void run(View view, Update update) {
    // Static storage for state and the user functions, captured by the
    // type-erased runner trampoline below. One instance per
    // (State, Msg, View, Update) instantiation; reassigned on each call
    // so re-running run<> from tests resets cleanly.
    static State state{};
    static View saved_view{view};
    static Update saved_update{update};
    state = State{};
    saved_view = std::move(view);
    saved_update = std::move(update);

    detail::install_app_runner([] {
        auto t0 = metrics::detail::now_ns();

        // 1. Drain pending messages and fold via update.
        auto msgs = detail::drain<Msg>();
        for (auto& m : msgs)
            saved_update(state, std::move(m));
        auto t1 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(
            t1 - t0, {{"phase", "update"}});

        // 2. Save previous tree, swap arenas, reset the fresh arena.
        auto& app = detail::g_app;
        app.prev_root = app.root;
        std::swap(app.arena, app.prev_arena);
        app.arena.reset();
        app.callbacks.clear();
        app.input_handlers.clear();
        app.input_nodes.clear();

        // 3. Allocate root and run the user view function.
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
        metrics::inst::phase_duration.record(
            t2 - t1, {{"phase", "view"}});

        // 4. Diff against previous tree — copy layout for unchanged
        //    subtrees so layout_node can skip them.
        if (app.prev_root.valid()) {
            detail::diff_and_copy_layout(
                app.prev_root, root_h,
                app.prev_arena, app.arena);
        }

        // 5. Layout + paint + flush. layout_node skips nodes with
        //    layout_valid = true (set by diff).
        float cw = phenotype_get_canvas_width();
        detail::layout_node(root_h, cw);
        auto t3 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(
            t3 - t2, {{"phase", "layout"}});

        app.focusable_ids.clear();
        emit_clear(app.theme.background);
        float vh = phenotype_get_canvas_height();
        detail::paint_node(root_h, 0, 0, app.scroll_y, vh);
        auto t4 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(
            t4 - t3, {{"phase", "paint"}});

        // Frame-skip-aware flush: hashes the cmd buffer and skips the
        // JS↔WASM phenotype_flush() round trip when the bytes are
        // identical to the previous frame (caret blink, idle repaint,
        // etc.). Diag counters are updated inside the helper.
        flush_if_changed();
        auto t5 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(
            t5 - t4, {{"phase", "flush"}});

        auto total = t5 - t0;
        metrics::inst::frame_duration.record(total);
        metrics::inst::rebuilds.add();
        log::debug("phenotype.runner",
            "rebuild #{} total={}us update={}us view={}us layout={}us paint={}us flush={}us",
            metrics::inst::rebuilds.total(), total / 1000,
            (t1 - t0) / 1000, (t2 - t1) / 1000, (t3 - t2) / 1000,
            (t4 - t3) / 1000, (t5 - t4) / 1000);
    });

    // Initial render — no messages yet, view runs against State{}.
    detail::trigger_rebuild();
}

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
//
// Layouts wrap a builder lambda (or a parameter pack of children-as-
// builder lambdas) and produce a parent LayoutNode with cross/main-axis
// flex semantics. Variadic overloads exist for the common
// `column(a, b, c)` shorthand alongside `column([&] { ... })`.

namespace layout {

// column — vertical flex container.
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

// row — horizontal flex container. Cross-axis defaults to Center so that
// inline patterns (icon + text, button + label, code + text) line up on
// a shared centerline; rows of unequal-height children otherwise stack
// from the top, leaving short text dangling above its taller siblings.
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

// box — generic single-child wrapper (no inherent direction or gap).
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

// sized_box — box with an explicit max_width. In a row, this becomes
// a fixed-intrinsic-width child (the row layout treats containers
// with style.max_width > 0 as having a known width, so they don't
// compete with the flex slot). Used to build columns of equal-width
// row children — e.g. the docs Examples section's code-on-left /
// live-on-right pairs.
template<typename F>
    requires std::is_invocable_v<F>
void sized_box(float max_width, F&& builder) {
    auto h = detail::alloc_node();
    detail::node_at(h).style.max_width = max_width;
    detail::open_container(h, std::forward<F>(builder));
}

// scaffold — page-level layout with hero header, max-width content,
// and footer. Children of the top_bar slot get hero styling auto-applied.
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
        // Post-build: apply hero styles to children
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
        // Post-build: apply footer styles to children
        auto const& t = detail::g_app.theme;
        for (auto child_h : detail::node_at(footer_h).children) {
            auto& child = detail::node_at(child_h);
            child.font_size = t.small_font_size;
            child.text_color = t.muted;
        }
    }
}

// surface — bare wrapper, no styling. Useful as a stable parent slot.
template<typename F>
    requires std::is_invocable_v<F>
void surface(F&& builder) {
    auto h = detail::alloc_node();
    detail::open_container(h, std::forward<F>(builder));
}

// card — rounded white container with padding.
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

// list_items — vertical container with left indent for bullet items.
template<typename F>
    requires std::is_invocable_v<F>
void list_items(F&& builder) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Column;
    node.style.gap = 6;
    node.style.padding[3] = 24; // left indent for bullets
    detail::open_container(h, std::forward<F>(builder));
}

// item — bullet + text row, designed to live inside list_items.
inline void item(str content) {
    auto row_h = detail::alloc_node();
    {
        auto& row = detail::node_at(row_h);
        row.style.flex_direction = FlexDirection::Row;
        row.style.gap = 8;
    }

    // Bullet
    auto bullet_h = detail::alloc_node();
    {
        auto& bullet = detail::node_at(bullet_h);
        bullet.text = "\xe2\x80\xa2"; // bullet character "•"
        bullet.font_size = detail::g_app.theme.body_font_size;
        bullet.text_color = detail::g_app.theme.foreground;
    }
    detail::node_at(row_h).children.push_back(bullet_h);

    // Text
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

// divider — full-width 1px horizontal rule.
inline void divider() {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.fixed_height = 1;
    node.background = detail::g_app.theme.border;

    detail::attach_to_scope(h);
}

// spacer — fixed-height vertical gap.
inline void spacer(unsigned int height_px) {
    auto h = detail::alloc_node();
    detail::node_at(h).style.fixed_height = static_cast<float>(height_px);
    detail::attach_to_scope(h);
}

} // namespace layout

} // namespace phenotype

// ============================================================
// WASM exports — called from JS
// ============================================================

extern "C" {
    __attribute__((export_name("phenotype_repaint")))
    void phenotype_repaint(float scroll_y) {
        auto& app = phenotype::detail::g_app;
        app.scroll_y = scroll_y;
        if (!app.root.valid()) return;

        auto* root_ptr = app.arena.get(app.root);
        if (!root_ptr) return;

        // Relayout only on resize (canvas width changed)
        float cw = phenotype_get_canvas_width();
        if (cw != root_ptr->width)
            phenotype::detail::layout_node(app.root, cw);

        app.focusable_ids.clear();
        phenotype::emit_clear(app.theme.background);
        float vh = phenotype_get_canvas_height();
        phenotype::detail::paint_node(app.root, 0, 0, scroll_y, vh);
        // flush_if_changed instead of flush so caret-blink and other
        // no-visible-change repaints collapse to a hash + return.
        phenotype::flush_if_changed();
    }

    __attribute__((export_name("phenotype_get_total_height")))
    float phenotype_get_total_height(void) {
        auto& app = phenotype::detail::g_app;
        if (auto* root_ptr = app.arena.get(app.root))
            return root_ptr->height;
        return 0;
    }

    __attribute__((export_name("phenotype_handle_event")))
    void phenotype_handle_event(unsigned int callback_id) {
        phenotype::metrics::inst::input_events.add(1, {{"event", "click"}});
        if (callback_id < phenotype::detail::g_app.callbacks.size()) {
            phenotype::log::trace("phenotype.input", "click id={}", callback_id);
            phenotype::detail::g_app.callbacks[callback_id]();
        } else {
            phenotype::log::warn("phenotype.input",
                "click id={} out of range (size={})",
                callback_id, phenotype::detail::g_app.callbacks.size());
        }
    }

    __attribute__((export_name("phenotype_set_hover")))
    void phenotype_set_hover(unsigned int callback_id) {
        if (phenotype::detail::g_app.hovered_id == callback_id) return;
        phenotype::metrics::inst::input_events.add(1, {{"event", "hover"}});
        phenotype::detail::g_app.hovered_id = callback_id;
        phenotype_repaint(phenotype::detail::g_app.scroll_y);
    }

    __attribute__((export_name("phenotype_set_focus")))
    void phenotype_set_focus(unsigned int callback_id) {
        phenotype::metrics::inst::input_events.add(1, {{"event", "focus"}});
        phenotype::detail::g_app.focused_id = callback_id;
        phenotype::detail::g_app.caret_pos = 0xFFFFFFFF; // end of text
        phenotype::detail::g_app.caret_visible = true;
        phenotype_repaint(phenotype::detail::g_app.scroll_y);
    }

    __attribute__((export_name("phenotype_get_focused_id")))
    unsigned int phenotype_get_focused_id(void) {
        return phenotype::detail::g_app.focused_id;
    }

    __attribute__((export_name("phenotype_handle_tab")))
    void phenotype_handle_tab(unsigned int reverse) {
        phenotype::metrics::inst::input_events.add(1, {{"event", "tab"}});
        auto& app = phenotype::detail::g_app;
        if (app.focusable_ids.empty()) return;
        int n = static_cast<int>(app.focusable_ids.size());
        // Find current index
        int idx = -1;
        for (int i = 0; i < n; ++i) {
            if (app.focusable_ids[i] == app.focused_id) { idx = i; break; }
        }
        if (reverse)
            idx = (idx <= 0) ? n - 1 : idx - 1;
        else
            idx = (idx < 0 || idx >= n - 1) ? 0 : idx + 1;
        app.focused_id = app.focusable_ids[idx];
        app.caret_visible = true;
        phenotype_repaint(app.scroll_y);
    }

    __attribute__((export_name("phenotype_toggle_caret")))
    void phenotype_toggle_caret() {
        auto& app = phenotype::detail::g_app;
        app.caret_visible = !app.caret_visible;
        phenotype_repaint(app.scroll_y);
    }

    // Apply a key event to a string value (used by both new and legacy
    // text-input dispatch paths). Returns true if `val` was modified.
    static bool phenotype_apply_key_to_string(unsigned int key_type,
                                              unsigned int codepoint,
                                              std::string& val) {
        switch (key_type) {
            case 0: { // character
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
                val.append(buf, len);
                return true;
            }
            case 1: // backspace
                if (!val.empty()) {
                    // Remove last UTF-8 character
                    auto i = val.size() - 1;
                    while (i > 0 && (static_cast<unsigned char>(val[i]) & 0xC0) == 0x80) --i;
                    val.erase(i);
                    return true;
                }
                return false;
            default:
                return false;
        }
    }

    __attribute__((export_name("phenotype_handle_key")))
    void phenotype_handle_key(unsigned int key_type, unsigned int codepoint) {
        phenotype::metrics::inst::input_events.add(1, {{"event", "key"}});
        auto& app = phenotype::detail::g_app;
        for (auto& [id, handler] : app.input_handlers) {
            if (id != app.focused_id) continue;
            std::string val = handler.current;
            if (!phenotype_apply_key_to_string(key_type, codepoint, val))
                return;
            app.caret_visible = true;
            handler.invoke(handler.state, std::move(val));
            return;
        }
    }

    // ============================================================
    // Diagnostics — JSON snapshot exposed via shared linear memory.
    // JS reads (phenotype_diag_get_buf, phenotype_diag_get_len) after
    // calling phenotype_diag_export() and JSON.parse()s the result.
    // ============================================================

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
        if (json.size() > PHENOTYPE_DIAG_BUF_SIZE) {
            phenotype_diag_buf_len = 0;
            return 0;
        }
        std::memcpy(phenotype_diag_buf, json.data(), json.size());
        phenotype_diag_buf_len = static_cast<unsigned int>(json.size());
        return phenotype_diag_buf_len;
    }

    __attribute__((export_name("phenotype_diag_set_log_level")))
    void phenotype_diag_set_log_level(unsigned int level) {
        phenotype::log::set_level(
            static_cast<phenotype::log::Severity>(level));
    }

    __attribute__((export_name("phenotype_diag_reset")))
    void phenotype_diag_reset(void) {
        phenotype::metrics::reset_all();
    }

    // ============================================================
    // Theme from JSON — JS side calls this to push a theme JSON
    // string into the framework. Reads `len` UTF-8 bytes from the
    // input buffer, parses as JSON, deserializes to Theme via txn,
    // and calls set_theme(). Returns 1 on success, 0 on failure
    // (with a warn log).
    // ============================================================

    // Forward-declare the input buffer function — it lives further down
    // in the same extern "C" block, after the text input section.
    unsigned char* phenotype_input_buf(void);

    __attribute__((export_name("phenotype_set_theme_json")))
    unsigned int phenotype_set_theme_json(unsigned int len) {
        auto* buf = phenotype_input_buf();
        std::string_view json_str(reinterpret_cast<char const*>(buf), len);
        auto result = phenotype::theme_from_json(json_str);
        if (!result) {
            phenotype::log::warn("phenotype.theme",
                "theme_from_json: {}", result.error());
            return 0;
        }
        phenotype::set_theme(*result);
        return 1;
    }

    // ============================================================
    // Text input value buffer — JS uses this to read / write the
    // currently focused text field's value so it can render an HTML
    // <input> overlay over the canvas-painted field. The overlay is
    // what makes OS IME (Korean / Japanese / Chinese) inline
    // composition visible to the user; the C++ side just owns the
    // canonical value via the existing InputHandler::current state.
    // ============================================================

    constexpr unsigned int PHENOTYPE_INPUT_BUF_SIZE = 4u * 1024u;
    alignas(4) unsigned char phenotype_input_buf_storage[PHENOTYPE_INPUT_BUF_SIZE];
    unsigned int phenotype_input_buf_len_value = 0;

    __attribute__((export_name("phenotype_input_buf")))
    unsigned char* phenotype_input_buf(void) { return phenotype_input_buf_storage; }

    __attribute__((export_name("phenotype_input_buf_size")))
    unsigned int phenotype_input_buf_size(void) { return PHENOTYPE_INPUT_BUF_SIZE; }

    __attribute__((export_name("phenotype_input_buf_len")))
    unsigned int phenotype_input_buf_len(void) { return phenotype_input_buf_len_value; }

    // Returns 1 if the currently focused id belongs to a text input
    // node (i.e. is registered in app.input_handlers), 0 otherwise.
    // The JS shim uses this to decide whether to show the HTML
    // overlay — load_focused() alone is ambiguous because it also
    // returns length 0 for an empty (but real) text field.
    __attribute__((export_name("phenotype_focused_is_input")))
    unsigned int phenotype_focused_is_input(void) {
        auto& app = phenotype::detail::g_app;
        for (auto const& [id, handler] : app.input_handlers) {
            (void)handler;
            if (id == app.focused_id) return 1;
        }
        return 0;
    }

    // Copy the focused text field's current value into the shared
    // buffer so JS can populate hiddenInput.value on focus / re-focus.
    // Returns the byte length (also stored in phenotype_input_buf_len).
    // If no text field is focused, length is 0.
    __attribute__((export_name("phenotype_input_load_focused")))
    unsigned int phenotype_input_load_focused(void) {
        auto& app = phenotype::detail::g_app;
        phenotype_input_buf_len_value = 0;
        for (auto& [id, handler] : app.input_handlers) {
            if (id != app.focused_id) continue;
            unsigned int n = static_cast<unsigned int>(handler.current.size());
            if (n > PHENOTYPE_INPUT_BUF_SIZE) n = PHENOTYPE_INPUT_BUF_SIZE;
            std::memcpy(phenotype_input_buf_storage, handler.current.data(), n);
            phenotype_input_buf_len_value = n;
            return n;
        }
        return 0;
    }

    // Commit a new value (UTF-8 bytes the JS side wrote into
    // phenotype_input_buf_storage) for the currently focused text
    // field. Goes through the existing InputHandler::invoke trampoline
    // so the user's update() runs and the runner rebuilds the view
    // exactly the way today's per-character path does.
    __attribute__((export_name("phenotype_input_commit")))
    void phenotype_input_commit(unsigned int len) {
        if (len > PHENOTYPE_INPUT_BUF_SIZE) return;
        auto& app = phenotype::detail::g_app;
        for (auto& [id, handler] : app.input_handlers) {
            if (id != app.focused_id) continue;
            std::string val(
                reinterpret_cast<char const*>(phenotype_input_buf_storage), len);
            handler.invoke(handler.state, std::move(val));
            return;
        }
    }
}

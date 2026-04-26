module;
#include <algorithm>
#include <concepts>
#include <cstring>
#include <functional>
#include <optional>
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

import json;
#ifdef __wasi__
import phenotype.wasm;
#endif

export namespace phenotype {

namespace detail {
// Append a freshly allocated node to the current scope's children, if any.
inline void append_child(NodeHandle parent, NodeHandle child) {
    node_at(child).parent = parent;
    node_at(parent).children.push_back(child);
}

// Pending key for the next node to be attached to the current scope —
// set by phenotype::keyed(id, builder), consumed (and cleared) by
// attach_to_scope when that builder's root widget lands. If the key is
// consumed on a child, the child's parent is flipped to
// `children_keyed = true` so diff's keyed salvage pass runs on that
// parent next frame.
inline std::uint32_t& pending_child_key() {
    static std::uint32_t k = LayoutNode::unkeyed_key;
    return k;
}

inline void attach_to_scope(NodeHandle h) {
    auto* s = Scope::current();
    if (!s) return;
    auto& parent = node_at(s->node);
    auto& child = node_at(h);
    auto& pending = pending_child_key();
    if (pending != LayoutNode::unkeyed_key) {
        child.key = pending;
        parent.children_keyed = true;
        pending = LayoutNode::unkeyed_key;
    }
    append_child(s->node, h);
}
} // namespace detail

// phenotype::keyed(id, builder) — opt in to keyed reconciliation for
// the next widget emitted by `builder`. Typically used inside a list
// loop so that rows survive reorder / insert / delete without losing
// their per-row layout + paint cache:
//
//   layout::column([&] {
//       for (auto const& item : items) {
//           phenotype::keyed(item.id, [&] {
//               widget::text(item.label);
//           });
//       }
//   });
//
// Nested keyed() calls: the inner call overrides the outer until the
// next attach. Duplicate keys among siblings: first attach wins; later
// ones still get the key stored but the parent's keyed salvage map
// only holds one entry per key. The DSL doesn't validate uniqueness —
// that's on the caller, like React's `key` prop.
template <typename F>
    requires std::invocable<F>
inline void keyed(std::uint32_t id, F&& builder) {
    auto prev = detail::pending_child_key();
    detail::pending_child_key() = id;
    std::forward<F>(builder)();
    // If the builder didn't attach anything (or attached multiple
    // widgets and the first consumed the key), restore the prior
    // pending key so an enclosing keyed() block isn't lost.
    detail::pending_child_key() = prev;
}

// ============================================================
// widget:: — leaf components (text, code, link, button, text_field)
// ============================================================

namespace widget {

// text — single string label, inherits text_align from current scope.
//
// `size` picks one of phenotype-web's typography rungs (Body / Heading /
// Small / Code / HeroTitle / HeroSubtitle). `Code` additionally swaps
// the node into the inline code-block chrome (mono font, code_bg
// background, 1px border, radius_md, space_xs / space_sm padding) —
// matches phenotype-web's <Text size="code">.
//
// `color` picks one of the foreground / muted / accent / hero color
// tokens. Defaults to foreground.
//
// For block-level code with bigger padding, prefer widget::code below.
inline void text(str content,
                 TextSize size = TextSize::Body,
                 TextColor color = TextColor::Default) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    node.text = std::string(content.data, content.len);

    switch (size) {
        case TextSize::Heading:      node.font_size = t.heading_font_size; break;
        case TextSize::Small:        node.font_size = t.small_font_size; break;
        case TextSize::Code:         node.font_size = t.code_font_size; node.mono = true; break;
        case TextSize::HeroTitle:    node.font_size = t.hero_title_size; break;
        case TextSize::HeroSubtitle: node.font_size = t.hero_subtitle_size; break;
        case TextSize::Body:
        default:                     node.font_size = t.body_font_size; break;
    }

    switch (color) {
        case TextColor::Muted:     node.text_color = t.muted; break;
        case TextColor::Accent:    node.text_color = t.accent; break;
        case TextColor::HeroFg:    node.text_color = t.hero_fg; break;
        case TextColor::HeroMuted: node.text_color = t.hero_muted; break;
        case TextColor::Default:
        default:                   node.text_color = t.foreground; break;
    }

    // Inline code chrome — matches phenotype-web's Text.css L17-24.
    if (size == TextSize::Code) {
        node.background = t.code_bg;
        node.border_color = t.border;
        node.border_width = 1;
        node.border_radius = t.radius_md;
        node.style.padding[0] = t.space_xs;
        node.style.padding[1] = t.space_sm;
        node.style.padding[2] = t.space_xs;
        node.style.padding[3] = t.space_sm;
    }

    if (auto* s = Scope::current()) {
        node.style.text_align = detail::node_at(s->node).style.text_align;
    }
    detail::attach_to_scope(h);
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
    node.border_radius = detail::g_app.theme.radius_md;
    node.style.padding[0] = detail::g_app.theme.space_lg;
    node.style.padding[1] = detail::g_app.theme.space_lg;
    node.style.padding[2] = detail::g_app.theme.space_lg;
    node.style.padding[3] = detail::g_app.theme.space_lg;

    detail::attach_to_scope(h);
}

// cell — compact bordered grid slot for a single piece of content.
//
// Mirrors phenotype-web's <Cell> widget. The first concrete spreadsheet
// primitive used by examples/seven_guis/cells. Header variant flips the
// background to code_bg and centers content — used for column letters
// and row numbers. Width / height default to 0 / -1 (no cap), so the
// caller is responsible for uniform sizing if a true grid alignment
// is needed (currently a fidelity gap; M8-3 closes it).
inline void cell(str content,
                 bool header = false,
                 float width = 0,
                 float height = -1) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    node.text = std::string(content.data, content.len);
    node.font_size = t.small_font_size;
    node.text_color = header ? t.muted : t.foreground;
    node.background = header ? t.code_bg : t.surface;
    node.border_color = t.border;
    node.border_width = 1;
    node.style.padding[0] = 0;
    node.style.padding[1] = t.space_xs;
    node.style.padding[2] = 0;
    node.style.padding[3] = t.space_xs;
    node.style.max_width = width;
    node.style.fixed_height = height;
    if (header) {
        node.style.text_align = TextAlign::Center;
    }
    detail::attach_to_scope(h);
}

// button<Msg> — click posts a copy of `msg` and triggers a rebuild.
//
//   variant=Default  — surface chrome with hover_bg fallback.
//   variant=Primary  — accent-filled, white text, hover darkens to accent_strong.
//   disabled=true    — chrome switches to state_disabled_*, click and Tab focus
//                      are suppressed (no callback registered, focusable=false).
template<typename Msg>
inline void button(str label, Msg msg,
                   ButtonVariant variant = ButtonVariant::Default,
                   bool disabled = false) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    node.text = std::string(label.data, label.len);
    node.font_size = t.body_font_size;
    node.border_width = 1;
    node.border_radius = t.radius_sm;
    node.style.padding[0] = t.space_sm;
    node.style.padding[1] = t.space_md;
    node.style.padding[2] = t.space_sm;
    node.style.padding[3] = t.space_md;
    node.interaction_role = InteractionRole::Button;

    if (disabled) {
        node.background = t.state_disabled_bg;
        node.text_color = t.state_disabled_fg;
        node.border_color = t.state_disabled_border;
        node.cursor_type = 0;
        node.focusable = false;
        // No callback — the button is non-interactive. Skip the
        // callback registration entirely so click dispatch finds
        // node.callback_id == 0xFFFFFFFF and falls through.
        detail::attach_to_scope(h);
        return;
    }

    switch (variant) {
        case ButtonVariant::Primary:
            node.background = t.accent;
            node.text_color = t.state_active_fg;
            node.hover_background = t.accent_strong;
            node.border_color = t.accent;
            break;
        case ButtonVariant::Default:
        default:
            node.background = t.surface;
            node.text_color = t.foreground;
            node.hover_background = t.state_hover_bg;
            node.border_color = t.border;
            break;
    }
    node.cursor_type = 1;

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
    auto id = static_cast<unsigned int>(
        ::phenotype::detail::g_app.callbacks.size());
    auto row_h = ::phenotype::detail::alloc_node();
    {
        auto& row = ::phenotype::detail::node_at(row_h);
        row.style.flex_direction = FlexDirection::Row;
        row.style.cross_align    = CrossAxisAlignment::Center;
        row.style.gap            = ::phenotype::detail::g_app.theme.space_sm;
        row.debug_semantic_role = interaction_role_name(role);
        row.debug_semantic_label = std::string(label.data, label.len);
        row.debug_semantic_callback_id = id;
        row.debug_semantic_focusable = true;
    }
    ::phenotype::detail::attach_to_scope(row_h);

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
        box.debug_semantic_hidden = true;
        if (active) {
            box.background   = ::phenotype::detail::g_app.theme.accent;
            box.border_color = ::phenotype::detail::g_app.theme.accent;
            box.decoration   = active_decoration;
        } else {
            box.background   = ::phenotype::detail::g_app.theme.surface;
            box.border_color = ::phenotype::detail::g_app.theme.border;
        }
        box.border_width = 1;
        ::phenotype::detail::append_child(row_h, box_h);
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
        lbl.debug_semantic_hidden = true;
        ::phenotype::detail::append_child(row_h, lbl_h);
    }
}
} // namespace _impl

template<typename Msg>
inline void checkbox(str label, bool checked, Msg msg) {
    _impl::toggle(label, checked, std::move(msg),
                  detail::g_app.theme.radius_xs, Decoration::Check,
                  InteractionRole::Checkbox);
}

template<typename Msg>
inline void radio(str label, bool selected, Msg msg) {
    _impl::toggle(label, selected, std::move(msg),
                  detail::g_app.theme.radius_lg, Decoration::Dot,
                  InteractionRole::Radio);
}

// text_field<Msg> — single-line text input.
//
//   error=true     — chrome switches to state_error_* (border / bg / fg).
//                    The field stays interactive; users can keep editing
//                    while the error sits.
//   disabled=true  — chrome switches to state_disabled_*. is_input,
//                    cursor, focus, and the input-handler registration
//                    all turn off; typing is dropped.
template<typename Msg>
inline void text_field(str hint, std::string const& current,
                       Msg(*mapper)(std::string),
                       bool error = false, bool disabled = false) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    node.interaction_role = InteractionRole::TextField;
    node.placeholder = std::string(hint.data, hint.len);
    node.text = current.empty() ? node.placeholder : current;
    node.font_size = t.body_font_size;
    node.border_width = 1;
    node.border_radius = t.radius_sm;
    node.style.padding[0] = t.space_sm;
    node.style.padding[1] = t.space_md;
    node.style.padding[2] = t.space_sm;
    node.style.padding[3] = t.space_md;

    if (disabled) {
        node.is_input = false;
        node.background = t.state_disabled_bg;
        node.text_color = t.state_disabled_fg;
        node.border_color = t.state_disabled_border;
        node.cursor_type = 0;
        node.focusable = false;
        // No callback / input handler — the field is non-interactive.
        detail::attach_to_scope(h);
        return;
    }

    node.is_input = true;
    if (error) {
        node.background = t.state_error_bg;
        node.text_color = current.empty() ? t.muted : t.state_error_fg;
        node.border_color = t.state_error_border;
    } else {
        node.background = t.surface;
        node.text_color = current.empty() ? t.muted : t.foreground;
        node.border_color = t.border;
    }
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

        // Paint-cache invalidation mask — subtrees whose callback_mask
        // intersects this bitset must re-walk instead of blitting from
        // prev_cmd_buf. We include both the old and new hover/focus ids
        // so that the subtree that USED to be hovered/focused redraws
        // without its hover background, and the subtree that is now
        // hovered/focused redraws WITH it. The focused id is always
        // included because focused inputs can receive caret/selection/
        // text changes that alter emitted bytes without the id itself
        // transitioning.
        auto mask_bit = [](unsigned int id) -> std::uint64_t {
            return id == 0xFFFFFFFFu ? 0ULL : (1ULL << (id & 63u));
        };
        std::uint64_t inv = 0;
        if (app.hovered_id != app.prev_hovered_id) {
            inv |= mask_bit(app.hovered_id) | mask_bit(app.prev_hovered_id);
        }
        if (app.focused_id != app.prev_focused_id) {
            inv |= mask_bit(app.focused_id) | mask_bit(app.prev_focused_id);
        }
        inv |= mask_bit(app.focused_id);
        app.paint_invalidation_mask = inv;

        app.focusable_ids.clear();
        detail::collect_focusable_ids(root_h);
        emit_clear(host, app.theme.background);
        float vh = host.canvas_height();
        app.debug_viewport_width = cw;
        app.debug_viewport_height = vh;
        detail::paint_node(host, host, root_h, 0, 0, app.scroll_y, vh);
        auto t4 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t4 - t3, {{"phase", "paint"}});

        flush_if_changed(host);
        auto t5 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t5 - t4, {{"phase", "flush"}});

        // Persist the ambient paint inputs for next frame's blit guard.
        app.prev_scroll_y   = app.scroll_y;
        app.prev_hovered_id = app.hovered_id;
        app.prev_focused_id = app.focused_id;

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

        // Paint-cache invalidation mask — see the native runner for a
        // detailed explanation; the WASI path mirrors that logic.
        auto mask_bit = [](unsigned int id) -> std::uint64_t {
            return id == 0xFFFFFFFFu ? 0ULL : (1ULL << (id & 63u));
        };
        std::uint64_t inv = 0;
        if (app.hovered_id != app.prev_hovered_id) {
            inv |= mask_bit(app.hovered_id) | mask_bit(app.prev_hovered_id);
        }
        if (app.focused_id != app.prev_focused_id) {
            inv |= mask_bit(app.focused_id) | mask_bit(app.prev_focused_id);
        }
        inv |= mask_bit(app.focused_id);
        app.paint_invalidation_mask = inv;

        app.focusable_ids.clear();
        detail::collect_focusable_ids(root_h);
        detail::wasi_emit_clear(app.theme.background);
        float vh = phenotype_get_canvas_height();
        app.debug_viewport_width = cw;
        app.debug_viewport_height = vh;
        detail::wasi_paint_node(root_h, 0, 0, app.scroll_y, vh);
        auto t4 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t4 - t3, {{"phase", "paint"}});

        detail::wasi_flush_if_changed();
        auto t5 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t5 - t4, {{"phase", "flush"}});

        // Persist the ambient paint inputs for next frame's blit guard.
        app.prev_scroll_y   = app.scroll_y;
        app.prev_hovered_id = app.hovered_id;
        app.prev_focused_id = app.focused_id;

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

// Resolve a SpaceToken against the active Theme's spacing scale.
inline float space_value(SpaceToken token) noexcept {
    auto const& t = detail::g_app.theme;
    switch (token) {
        case SpaceToken::Xs:  return t.space_xs;
        case SpaceToken::Sm:  return t.space_sm;
        case SpaceToken::Md:  return t.space_md;
        case SpaceToken::Lg:  return t.space_lg;
        case SpaceToken::Xl:  return t.space_xl;
        case SpaceToken::Xl2: return t.space_2xl;
    }
    return t.space_md;
}

// column — vertical flex container.
//
// Builder overload accepts gap / cross-axis / main-axis props that
// mirror phenotype-web's <Column> attributes. The variadic overload
// keeps its M0-3 behaviour (defaults only) so existing call sites stay
// terse; pass a builder lambda when you need the props.
template<typename F>
    requires std::is_invocable_v<F>
void column(F&& builder,
            SpaceToken gap = SpaceToken::Md,
            CrossAxisAlignment cross = CrossAxisAlignment::Start,
            MainAxisAlignment main = MainAxisAlignment::Start) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Column;
    node.style.gap = space_value(gap);
    node.style.cross_align = cross;
    node.style.main_align = main;
    detail::open_container(h, std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void column(A&& a, B&& b, Rest&&... rest) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Column;
    node.style.gap = detail::g_app.theme.space_md;
    detail::attach_to_scope(h);
    Scope child_scope(h);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (detail::render_one(std::forward<A>(a)),
     detail::render_one(std::forward<B>(b)),
     (detail::render_one(std::forward<Rest>(rest)), ...));
    Scope::set_current(prev);
}

// row — horizontal flex container. Defaults match phenotype-web's
// <Row> (gap=Sm, cross=Center). Builder overload exposes all three
// props; the variadic overload keeps the defaults.
template<typename F>
    requires std::is_invocable_v<F>
void row(F&& builder,
         SpaceToken gap = SpaceToken::Sm,
         CrossAxisAlignment cross = CrossAxisAlignment::Center,
         MainAxisAlignment main = MainAxisAlignment::Start) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Row;
    node.style.gap = space_value(gap);
    node.style.cross_align = cross;
    node.style.main_align = main;
    detail::open_container(h, std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void row(A&& a, B&& b, Rest&&... rest) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Row;
    node.style.cross_align = CrossAxisAlignment::Center;
    node.style.gap = detail::g_app.theme.space_sm;
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

// padded — uniform-padding container. Useful as a window-edge gutter
// when the root view should breathe inside the native shell instead
// of butting up against the window borders.
template<typename F>
    requires std::is_invocable_v<F>
void padded(SpaceToken padding, F&& builder) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    float p = space_value(padding);
    node.style.padding[0] = p;
    node.style.padding[1] = p;
    node.style.padding[2] = p;
    node.style.padding[3] = p;
    detail::open_container(h, std::forward<F>(builder));
}

// grid — rigid track container. Mirrors CSS Grid's
// `grid-template-columns: <fixed widths>` + `grid-auto-rows: <h>` for
// the common spreadsheet / table case. Children are placed row-major
// into a fixed `columns × n_rows` matrix, with `gap` separating tracks
// in both directions. Pass `row_height = 0` to let each row size to
// its tallest child.
template<typename F>
    requires std::is_invocable_v<F>
void grid(std::vector<float> columns, float row_height, F&& builder,
          float gap = 0.0f) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.is_grid_container = true;
    node.grid_columns = std::move(columns);
    node.grid_row_height = row_height;
    node.style.gap = gap;
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
            header.style.gap = detail::g_app.theme.space_sm;
            header.style.cross_align = CrossAxisAlignment::Center;
            header.style.text_align = TextAlign::Center;
            header.background = detail::g_app.theme.hero_bg;
            header.style.padding[0] = detail::g_app.theme.space_3xl;
            header.style.padding[1] = detail::g_app.theme.space_xl;
            header.style.padding[2] = detail::g_app.theme.space_3xl;
            header.style.padding[3] = detail::g_app.theme.space_xl;
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
            main.style.gap = detail::g_app.theme.space_2xl;
            main.style.max_width = detail::g_app.theme.max_content_width;
            main.style.padding[0] = detail::g_app.theme.space_2xl;
            main.style.padding[1] = detail::g_app.theme.space_xl;
            main.style.padding[2] = detail::g_app.theme.space_2xl;
            main.style.padding[3] = detail::g_app.theme.space_xl;
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
            footer.style.padding[0] = detail::g_app.theme.space_2xl;
            footer.style.padding[1] = detail::g_app.theme.space_xl;
            footer.style.padding[2] = detail::g_app.theme.space_2xl;
            footer.style.padding[3] = detail::g_app.theme.space_xl;
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
    node.border_radius = detail::g_app.theme.radius_lg;
    node.background = detail::g_app.theme.surface;
    node.border_color = detail::g_app.theme.border;
    node.border_width = 1;
    node.style.padding[0] = detail::g_app.theme.space_lg;
    node.style.padding[1] = detail::g_app.theme.space_lg;
    node.style.padding[2] = detail::g_app.theme.space_lg;
    node.style.padding[3] = detail::g_app.theme.space_lg;
    detail::open_container(h, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void list_items(F&& builder) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Column;
    node.style.gap = 6;
    node.style.padding[3] = detail::g_app.theme.space_xl;
    detail::open_container(h, std::forward<F>(builder));
}

inline void item(str content) {
    auto row_h = detail::alloc_node();
    {
        auto& row = detail::node_at(row_h);
        row.style.flex_direction = FlexDirection::Row;
        row.style.gap = detail::g_app.theme.space_sm;
    }

    auto bullet_h = detail::alloc_node();
    {
        auto& bullet = detail::node_at(bullet_h);
        bullet.text = "\xe2\x80\xa2";
        bullet.font_size = detail::g_app.theme.body_font_size;
        bullet.text_color = detail::g_app.theme.foreground;
    }
    detail::append_child(row_h, bullet_h);

    auto text_h = detail::alloc_node();
    {
        auto& text = detail::node_at(text_h);
        text.text = std::string(content.data, content.len);
        text.font_size = detail::g_app.theme.body_font_size;
        text.text_color = detail::g_app.theme.foreground;
    }
    detail::append_child(row_h, text_h);

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

inline unsigned int resolved_selection_anchor(std::string const& value) {
    if (g_app.selection_anchor == 0xFFFFFFFFu)
        return resolved_caret_pos(value);
    return static_cast<unsigned int>(
        clamp_utf8_boundary(value, static_cast<std::size_t>(g_app.selection_anchor)));
}

struct ResolvedSelectionState {
    std::size_t anchor = 0;
    std::size_t caret = 0;
    std::size_t start = 0;
    std::size_t end = 0;
    bool active = false;
};

inline ResolvedSelectionState resolved_selection_state(std::string const& value) {
    auto caret = static_cast<std::size_t>(resolved_caret_pos(value));
    auto anchor = static_cast<std::size_t>(resolved_selection_anchor(value));
    return {
        anchor,
        caret,
        (anchor < caret) ? anchor : caret,
        (anchor > caret) ? anchor : caret,
        anchor != caret,
    };
}

inline void sync_input_debug_selection_state(std::string const* value_override = nullptr) {
    auto& snapshot = g_app.input_debug;
    snapshot.selection_active = false;
    snapshot.selection_start = 0xFFFFFFFFu;
    snapshot.selection_end = 0xFFFFFFFFu;

    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler)
        return;

    auto const& value = value_override ? *value_override : handler->current;
    auto selection = resolved_selection_state(value);
    snapshot.selection_active = selection.active;
    snapshot.selection_start = static_cast<unsigned int>(selection.start);
    snapshot.selection_end = static_cast<unsigned int>(selection.end);
}

inline void sync_input_debug_caret_state(std::string const* value_override = nullptr) {
    auto& snapshot = g_app.input_debug;
    snapshot.caret_pos = g_app.caret_pos;
    snapshot.caret_visible = g_app.caret_visible;
    snapshot.focused_is_input = find_input_handler(g_app.focused_id) != nullptr;
    sync_input_debug_selection_state(value_override);
    if (!snapshot.focused_is_input) {
        snapshot.caret_renderer = "hidden";
        snapshot.caret_rect = {};
        snapshot.caret_draw_rect = {};
        snapshot.caret_host_rect = {};
        snapshot.caret_screen_rect = {};
        snapshot.caret_host_bounds = {};
        snapshot.caret_host_flipped = false;
        snapshot.caret_geometry_source = "draw";
    }
}

inline diag::RectSnapshot make_debug_rect_snapshot(float x,
                                                   float y,
                                                   float w,
                                                   float h) {
    return {
        true,
        x,
        y,
        w,
        h,
    };
}

inline void set_input_debug_caret_presentation(char const* renderer,
                                               float x,
                                               float y,
                                               float w,
                                               float h) {
    auto& snapshot = g_app.input_debug;
    snapshot.caret_renderer = renderer ? renderer : "hidden";
    snapshot.caret_rect = make_debug_rect_snapshot(x, y, w, h);
    snapshot.caret_draw_rect = snapshot.caret_rect;
    snapshot.caret_host_rect = {};
    snapshot.caret_screen_rect = {};
    snapshot.caret_host_bounds = {};
    snapshot.caret_host_flipped = false;
    snapshot.caret_geometry_source = "draw";
}

inline void set_input_debug_caret_geometry(char const* renderer,
                                           diag::RectSnapshot draw_rect,
                                           diag::RectSnapshot host_rect,
                                           diag::RectSnapshot screen_rect,
                                           diag::RectSnapshot host_bounds,
                                           bool host_flipped,
                                           char const* geometry_source = "draw") {
    auto& snapshot = g_app.input_debug;
    snapshot.caret_renderer = renderer ? renderer : "hidden";
    snapshot.caret_rect = draw_rect;
    snapshot.caret_draw_rect = draw_rect;
    snapshot.caret_host_rect = host_rect;
    snapshot.caret_screen_rect = screen_rect;
    snapshot.caret_host_bounds = host_bounds;
    snapshot.caret_host_flipped = host_flipped;
    snapshot.caret_geometry_source = geometry_source ? geometry_source : "draw";
}

inline void clear_input_debug_caret_presentation(char const* renderer = "hidden") {
    auto& snapshot = g_app.input_debug;
    snapshot.caret_renderer = renderer ? renderer : "hidden";
    snapshot.caret_rect = {};
    snapshot.caret_draw_rect = {};
    snapshot.caret_host_rect = {};
    snapshot.caret_screen_rect = {};
    snapshot.caret_host_bounds = {};
    snapshot.caret_host_flipped = false;
    snapshot.caret_geometry_source = "draw";
}

inline void clear_caret_state(bool visible = true) {
    g_app.caret_pos = 0xFFFFFFFFu;
    g_app.selection_anchor = 0xFFFFFFFFu;
    g_app.caret_visible = visible;
    g_app.last_paint_hash = 0;
    sync_input_debug_caret_state();
}

inline unsigned int set_selection_state(std::string const& value,
                                        std::size_t anchor,
                                        std::size_t caret,
                                        bool visible = true) {
    g_app.selection_anchor = static_cast<unsigned int>(clamp_utf8_boundary(value, anchor));
    g_app.caret_pos = static_cast<unsigned int>(clamp_utf8_boundary(value, caret));
    g_app.caret_visible = visible;
    g_app.last_paint_hash = 0;
    sync_input_debug_caret_state(&value);
    return g_app.caret_pos;
}

inline unsigned int set_caret_state(std::string const& value,
                                    std::size_t pos,
                                    bool visible = true) {
    return set_selection_state(value, pos, pos, visible);
}

inline bool set_focused_input_caret_pos(std::size_t pos, bool visible = true) {
    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler)
        return false;
    set_caret_state(handler->current, pos, visible);
    return true;
}

inline bool set_focused_input_selection(std::size_t anchor,
                                        std::size_t caret,
                                        bool visible = true) {
    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler)
        return false;
    set_selection_state(handler->current, anchor, caret, visible);
    return true;
}

inline bool select_all_focused_input(bool visible = true) {
    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler)
        return false;
    set_selection_state(handler->current, 0, handler->current.size(), visible);
    return true;
}

inline void assign_focus(unsigned int callback_id) {
    g_app.focused_id = callback_id;
    if (auto const* handler = find_input_handler(callback_id)) {
        set_caret_state(handler->current, handler->current.size());
    } else {
        clear_caret_state();
    }
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
    snapshot.caret_visible = g_app.caret_visible;
    snapshot.focused_is_input = find_input_handler(g_app.focused_id) != nullptr;
    sync_input_debug_selection_state();
    if (!snapshot.focused_is_input) {
        snapshot.caret_renderer = "hidden";
        snapshot.caret_rect = {};
        snapshot.caret_draw_rect = {};
        snapshot.caret_host_rect = {};
        snapshot.caret_screen_rect = {};
        snapshot.caret_host_bounds = {};
        snapshot.caret_host_flipped = false;
        snapshot.caret_geometry_source = "draw";
    }

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
    g_app.last_paint_hash = 0;
    sync_input_debug_caret_state();
}

inline std::string encode_utf8_codepoint(unsigned int codepoint) {
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
    return {buf, buf + len};
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

inline bool replace_focused_input_text(std::size_t start,
                                       std::size_t end,
                                       std::string_view replacement) {
    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler)
        return false;

    std::string value = handler->current;
    start = clamp_utf8_boundary(value, start);
    end = clamp_utf8_boundary(value, end);
    if (end < start)
        std::swap(start, end);

    value.replace(start, end - start, replacement);
    set_caret_state(value, start + replacement.size());
    handler->invoke(handler->state, std::move(value));
    return true;
}

inline bool insert_focused_input_text(std::string_view replacement) {
    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler)
        return false;
    auto selection = resolved_selection_state(handler->current);
    return replace_focused_input_text(
        selection.start,
        selection.end,
        replacement);
}

inline void set_input_composition_state(bool active,
                                        std::string_view text,
                                        unsigned int cursor) {
    auto& snapshot = g_app.input_debug;
    snapshot.composition_active = active;
    snapshot.composition_text.assign(text.data(), text.size());
    snapshot.composition_cursor = cursor;
    g_app.last_paint_hash = 0;
}

inline void clear_input_composition_state() {
    set_input_composition_state(false, {}, 0);
}

inline void clear_input_debug_caret_fields(diag::InputDebugSnapshot& snapshot) {
    snapshot.caret_renderer = "hidden";
    snapshot.caret_rect = {};
    snapshot.caret_draw_rect = {};
    snapshot.caret_host_rect = {};
    snapshot.caret_screen_rect = {};
    snapshot.caret_host_bounds = {};
    snapshot.caret_host_flipped = false;
    snapshot.caret_geometry_source = "draw";
}

inline diag::InputDebugSnapshot materialize_input_debug_snapshot() {
    auto snapshot = g_app.input_debug;
    snapshot.focused_id = g_app.focused_id;
    snapshot.focused_role = interaction_role_name(callback_role(g_app.focused_id));
    snapshot.hovered_id = g_app.hovered_id;
    snapshot.scroll_y = g_app.scroll_y;
    snapshot.caret_pos = g_app.caret_pos;
    sync_input_debug_selection_state();
    snapshot.selection_active = g_app.input_debug.selection_active;
    snapshot.selection_start = g_app.input_debug.selection_start;
    snapshot.selection_end = g_app.input_debug.selection_end;
    snapshot.caret_visible = g_app.caret_visible;
    snapshot.focused_is_input = find_input_handler(g_app.focused_id) != nullptr;
    if (!snapshot.focused_is_input)
        clear_input_debug_caret_fields(snapshot);
    return snapshot;
}

inline bool handle_key(unsigned int key_type, unsigned int codepoint,
                       bool extend_selection = false,
                       char const* source = "core",
                       char const* detail = nullptr) {
    auto detail_name = detail ? detail : input_key_detail_name(key_type);
    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler) {
        note_input_event("key", source, detail_name, "ignored", g_app.focused_id);
        return false;
    }

    std::string value = handler->current;
    auto selection = resolved_selection_state(value);
    auto previous_anchor = selection.anchor;
    auto previous_caret = selection.caret;
    std::size_t caret = selection.caret;
    bool text_changed = false;
    if (key_type == 0 && selection.active) {
        auto replacement = encode_utf8_codepoint(codepoint);
        value.replace(
            selection.start,
            selection.end - selection.start,
            replacement);
        caret = selection.start + replacement.size();
        text_changed = true;
        set_caret_state(value, caret);
    } else if (key_type == 1 && selection.active) {
        value.erase(selection.start, selection.end - selection.start);
        caret = selection.start;
        text_changed = true;
        set_caret_state(value, caret);
    } else if (key_type >= 2 && key_type <= 5) {
        if (selection.active && !extend_selection) {
            std::size_t collapsed = selection.caret;
            switch (key_type) {
                case 2: collapsed = selection.start; break;
                case 3: collapsed = selection.end; break;
                case 4: collapsed = 0; break;
                case 5: collapsed = value.size(); break;
                default: break;
            }
            set_caret_state(value, collapsed);
            if (previous_anchor == collapsed && previous_caret == collapsed) {
                note_input_event("key", source, detail_name, "ignored", g_app.focused_id);
                return false;
            }
            note_input_event("key", source, detail_name, "handled", g_app.focused_id);
            return true;
        }

        if (!apply_key_to_string(key_type, codepoint, value, caret, text_changed)) {
            note_input_event("key", source, detail_name, "ignored", g_app.focused_id);
            return false;
        }
        if (extend_selection) {
            set_selection_state(value, selection.anchor, caret);
        } else {
            set_caret_state(value, caret);
        }
    } else {
        if (!apply_key_to_string(key_type, codepoint, value, caret, text_changed)) {
            note_input_event("key", source, detail_name, "ignored", g_app.focused_id);
            return false;
        }
        set_caret_state(value, caret);
    }

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
    collect_focusable_ids(app.root);
    emit_clear(host, app.theme.background);
    float vh = host.canvas_height();
    app.debug_viewport_width = cw;
    app.debug_viewport_height = vh;
    paint_node(host, host, app.root, 0, 0, scroll_y, vh);
    flush_if_changed(host);
    app.prev_scroll_y   = app.scroll_y;
    app.prev_hovered_id = app.hovered_id;
    app.prev_focused_id = app.focused_id;
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
    collect_focusable_ids(app.root);
    wasi_emit_clear(app.theme.background);
    float vh = phenotype_get_canvas_height();
    app.debug_viewport_width = cw;
    app.debug_viewport_height = vh;
    wasi_paint_node(app.root, 0, 0, scroll_y, vh);
    wasi_flush_if_changed();
    app.prev_scroll_y   = app.scroll_y;
    app.prev_hovered_id = app.hovered_id;
    app.prev_focused_id = app.focused_id;
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

inline std::optional<unsigned int> optional_callback_id(unsigned int callback_id) {
    if (callback_id == 0xFFFFFFFFu)
        return std::nullopt;
    return callback_id;
}

inline bool rect_intersects_viewport(diag::RectSnapshot const& rect) {
    if (!rect.valid)
        return false;
    auto viewport_width = g_app.debug_viewport_width;
    auto viewport_height = g_app.debug_viewport_height;
    if (viewport_width <= 0.0f || viewport_height <= 0.0f)
        return false;
    if (rect.x + rect.w <= 0.0f || rect.x >= viewport_width)
        return false;
    float viewport_top = g_app.scroll_y;
    float viewport_bottom = g_app.scroll_y + viewport_height;
    return rect.y + rect.h > viewport_top && rect.y < viewport_bottom;
}

inline diag::RectSnapshot node_bounds_snapshot(float x, float y, float w, float h) {
    return {
        true,
        x,
        y,
        w,
        h,
    };
}

inline void collect_semantic_nodes(NodeHandle node_h,
                                   float ox,
                                   float oy,
                                   std::vector<diag::SemanticNodeSnapshot>& out) {
    auto* node_ptr = g_app.arena.get(node_h);
    if (!node_ptr)
        return;

    auto const& node = *node_ptr;
    if (node.debug_semantic_hidden)
        return;

    float ax = ox + node.x;
    float ay = oy + node.y;
    bool is_root = !node.parent.valid();
    bool explicit_semantics = !node.debug_semantic_role.empty()
        || node.debug_semantic_callback_id != 0xFFFFFFFFu;
    bool auto_semantics = is_root
        || node.is_input
        || (node.callback_id != 0xFFFFFFFFu
            && node.interaction_role != InteractionRole::None)
        || !node.image_url.empty()
        || !node.text.empty();

    if (!explicit_semantics && !auto_semantics) {
        for (auto child_h : node.children)
            collect_semantic_nodes(child_h, ax, ay, out);
        return;
    }

    unsigned int semantic_callback_id = explicit_semantics
        && node.debug_semantic_callback_id != 0xFFFFFFFFu
        ? node.debug_semantic_callback_id
        : node.callback_id;

    diag::SemanticNodeSnapshot semantic{};
    semantic.callback_id = optional_callback_id(semantic_callback_id);
    if (!node.debug_semantic_role.empty()) {
        semantic.role = node.debug_semantic_role;
    } else if (is_root) {
        semantic.role = "root";
    } else if (node.is_input) {
        semantic.role = "text_field";
    } else if (node.callback_id != 0xFFFFFFFFu
               && node.interaction_role != InteractionRole::None) {
        semantic.role = interaction_role_name(node.interaction_role);
    } else if (!node.image_url.empty()) {
        semantic.role = "image";
    } else {
        semantic.role = "text";
    }

    if (!node.debug_semantic_label.empty()) {
        semantic.label = node.debug_semantic_label;
    } else if (!node.text.empty()) {
        semantic.label = node.text;
    }

    semantic.bounds = node_bounds_snapshot(ax, ay, node.width, node.height);
    semantic.visible = is_root || rect_intersects_viewport(semantic.bounds);
    semantic.enabled = true;
    semantic.focusable = explicit_semantics
        ? node.debug_semantic_focusable
        : (semantic_callback_id != 0xFFFFFFFFu && node.focusable);
    semantic.focused = semantic_callback_id != 0xFFFFFFFFu
        && semantic_callback_id == g_app.focused_id;
    semantic.scroll_container = is_root;

    for (auto child_h : node.children)
        collect_semantic_nodes(child_h, ax, ay, semantic.children);
    out.push_back(std::move(semantic));
}

inline std::optional<diag::SemanticNodeSnapshot> build_semantic_tree_snapshot() {
    std::vector<diag::SemanticNodeSnapshot> nodes;
    collect_semantic_nodes(g_app.root, 0.0f, 0.0f, nodes);
    if (nodes.empty())
        return std::nullopt;
    return std::move(nodes.front());
}

inline char const* default_debug_platform_name() {
#ifdef __wasi__
    return "wasi";
#else
    return "native";
#endif
}

inline char const* debug_backend_name() {
#ifdef __wasi__
    return "wasi";
#else
    return "native";
#endif
}

inline bool is_empty_runtime_details(json::Value const& value) {
    return value.is_object() && value.as_object().empty();
}

inline diag::PlatformCapabilitiesSnapshot build_platform_capabilities_snapshot(
        std::optional<diag::PlatformCapabilitiesSnapshot> platform_override = std::nullopt) {
    auto snapshot = platform_override.has_value()
        ? *platform_override
        : diag::detail::current_platform_capabilities();
#ifdef __wasi__
    if (!platform_override.has_value() && snapshot.platform.empty()) {
        snapshot = ::phenotype::wasi::detail::debug_capabilities();
    }
#endif
    if (snapshot.platform.empty())
        snapshot.platform = default_debug_platform_name();
    snapshot.read_only = true;
    snapshot.snapshot_json = true;
    snapshot.semantic_tree = true;
    snapshot.input_debug = true;
    snapshot.platform_runtime = true;
    return snapshot;
}

inline diag::PlatformRuntimeSnapshot build_platform_runtime_snapshot(
        diag::PlatformCapabilitiesSnapshot const& capabilities,
        std::optional<json::Value> runtime_details_override = std::nullopt) {
    diag::PlatformRuntimeSnapshot runtime{};
    runtime.platform = capabilities.platform;
    runtime.backend = debug_backend_name();
    runtime.viewport = node_bounds_snapshot(
        0.0f,
        0.0f,
        g_app.debug_viewport_width,
        g_app.debug_viewport_height);
    runtime.scroll_y = g_app.scroll_y;
    runtime.content_height = get_total_height();
    runtime.focused_callback_id = optional_callback_id(g_app.focused_id);
    runtime.hovered_callback_id = optional_callback_id(g_app.hovered_id);
    runtime.details = runtime_details_override.has_value()
        ? *runtime_details_override
        : diag::detail::current_platform_runtime_details();
#ifdef __wasi__
    if (!runtime_details_override.has_value()
        && is_empty_runtime_details(runtime.details)) {
        runtime.details = ::phenotype::wasi::detail::platform_runtime_details_json();
    }
#endif
    return runtime;
}

inline diag::DebugPlaneSnapshot build_debug_plane_snapshot(
        std::optional<diag::PlatformCapabilitiesSnapshot> platform_override = std::nullopt,
        std::optional<json::Value> runtime_details_override = std::nullopt) {
    diag::DebugPlaneSnapshot snapshot{};
    snapshot.platform_capabilities = build_platform_capabilities_snapshot(
        std::move(platform_override));
    snapshot.input_debug = materialize_input_debug_snapshot();
    snapshot.semantic_tree = build_semantic_tree_snapshot();
    snapshot.platform_runtime = build_platform_runtime_snapshot(
        snapshot.platform_capabilities,
        std::move(runtime_details_override));
    return snapshot;
}

inline json::Value build_debug_plane_snapshot_json(
        std::optional<diag::PlatformCapabilitiesSnapshot> platform_override = std::nullopt,
        std::optional<json::Value> runtime_details_override = std::nullopt) {
    return diag::debug_plane_snapshot_to_json(build_debug_plane_snapshot(
        std::move(platform_override),
        std::move(runtime_details_override)));
}

inline json::Value build_diag_snapshot_with_debug(
        std::optional<diag::PlatformCapabilitiesSnapshot> platform_override = std::nullopt,
        std::optional<json::Value> runtime_details_override = std::nullopt) {
    auto snapshot = diag::build_snapshot();
    auto& root = snapshot.as_object();
    root.erase("debug");
    root.emplace(
        "debug",
        build_debug_plane_snapshot_json(
            std::move(platform_override),
            std::move(runtime_details_override)));
    return snapshot;
}

inline std::string serialize_diag_snapshot_with_debug(
        std::optional<diag::PlatformCapabilitiesSnapshot> platform_override = std::nullopt,
        std::optional<json::Value> runtime_details_override = std::nullopt) {
    return json::emit(build_diag_snapshot_with_debug(
        std::move(platform_override),
        std::move(runtime_details_override)));
}

inline diag::InputDebugSnapshot current_input_debug_snapshot() {
    return materialize_input_debug_snapshot();
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

inline unsigned int get_selection_anchor() {
    return g_app.selection_anchor;
}

inline bool selection_active() {
    auto* handler = find_input_handler(g_app.focused_id);
    return handler && resolved_selection_state(handler->current).active;
}

inline unsigned int selection_start() {
    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler)
        return 0xFFFFFFFFu;
    return static_cast<unsigned int>(resolved_selection_state(handler->current).start);
}

inline unsigned int selection_end() {
    auto* handler = find_input_handler(g_app.focused_id);
    if (!handler)
        return 0xFFFFFFFFu;
    return static_cast<unsigned int>(resolved_selection_state(handler->current).end);
}

inline bool get_caret_visible() {
    return g_app.caret_visible;
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
    set_caret_state(value, value.size());
    handler->invoke(handler->state, std::move(value));
}

} // namespace detail

namespace diag {

inline InputDebugSnapshot input_debug_snapshot() {
    return ::phenotype::detail::current_input_debug_snapshot();
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
        auto json = phenotype::detail::serialize_diag_snapshot_with_debug();
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

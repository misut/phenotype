module;
#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstring>
#include <functional>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
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
export import phenotype.resources;
export import phenotype.icon_catalog;
export import phenotype.svg;
export import phenotype.icons;
export import phenotype.material;
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

inline auto theme_with_resource_defaults(
        Theme theme,
        ResourceCatalog const& catalog) -> Theme {
    if (!catalog.default_font_family.empty())
        theme.default_font_family = catalog.default_font_family;
    return theme;
}

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

// Forward declarations so widgets defined below can call into the
// view-time animation primitives (`animate_color` / `animate_float`)
// whose full definitions live further down. Default arguments must
// sit on the first declaration that's visible at each call site,
// so they live here; the definitions below repeat the parameters
// without defaults.
inline float animate_float(float target, int duration_ms,
                           std::source_location loc =
                               std::source_location::current());
inline Color animate_color(Color target, int duration_ms,
                           std::source_location loc =
                               std::source_location::current());

namespace detail {
// Forward decl mirrors the definition further down so widgets can
// read the wall clock without pulling `<chrono>` into their TU
// (looping animations like `widget::progress_indeterminate` compute
// phase directly from this value).
std::int64_t steady_ms();
}

// ============================================================
// widget:: — leaf components (text, code, link, button, text_field)
// ============================================================

namespace widget {

// text — single string label, inherits text_align from current scope.
//
// `size` picks one of the Theme typography rungs (Body / Heading /
// Small / Code / HeroTitle / HeroSubtitle). `Code` additionally swaps
// the node into the inline code-block chrome (mono font, code_bg
// background, 1px border, radius_md, space_xs / space_sm padding).
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

    // Inline code chrome.
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
    auto const& t = detail::g_app.theme;
    node.text = std::string(label.data, label.len);
    node.font_size = t.small_font_size;
    node.url = std::string(href.data, href.len);
    node.cursor_type = 1;
    node.interaction_role = InteractionRole::Link;

    auto url_copy = node.url;
    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    bool const is_hovered = (id == detail::g_app.hovered_id);
    bool const is_focused = (id == detail::g_app.focused_id);
    // View-time hover fade between accent and the deeper hover blue.
    // `node.hover_text_color` stays unset so paint's
    // `(is_hovered && hover_text_color.a > 0)` guard falls through to
    // the animated `node.text_color`.
    Color const link_hover_color{29, 78, 216, 255};
    node.text_color = animate_color(
        is_hovered ? link_hover_color : t.accent, 150);
    // Focus ring grows from no border to `state_focus_ring_width` and
    // back. Link's resting border is zero so the ring appears entirely
    // because of focus. The colour interpolates from the focus-ring
    // RGB at alpha 0 (invisible) to full alpha so the stroke fades in
    // without drifting through an unrelated hue mid-animation.
    Color const ring_off{t.state_focus_ring.r, t.state_focus_ring.g,
                         t.state_focus_ring.b, 0};
    node.border_width = animate_float(
        is_focused ? t.state_focus_ring_width : 0.0f, 150);
    node.border_color = animate_color(
        is_focused ? t.state_focus_ring : ring_off, 150);

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
// Compact bordered grid slot. The first concrete spreadsheet primitive
// used by examples/seven_guis/cells. Header variant flips the background
// to code_bg and centers content — used for column letters and row numbers.
// Width / height default to 0 / -1 (no cap), so the caller is responsible
// for uniform sizing if a true grid alignment is needed.
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
    node.border_width = 0.5f;
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
//   variant=Default  — surface chrome that fades to state_hover_bg on hover.
//   variant=Primary  — accent-filled, white text, hover darkens to accent_strong.
//   disabled=true    — click and Tab focus are suppressed. Chrome defaults to
//                      state_disabled_* unless explicit ButtonStyleOptions
//                      override it.
//
// Hover is a view-time fade rather than a paint-time branch: the
// background colour interpolates over ~150ms via `animate_color`, which
// reads the same pattern as `widget::switch_`. Paint's hover_background
// fallback stays available for widgets (tabs, link) that haven't been
// migrated yet.
template<typename Msg>
inline void button(str label, Msg msg, ButtonStyleOptions options) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    node.text = std::string(label.data, label.len);
    node.font_size = options.font_size > 0.0f
        ? options.font_size
        : t.body_font_size;
    node.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_sm;
    float default_padding[4] = {
        t.space_sm,
        t.space_md,
        t.space_sm,
        t.space_md,
    };
    float option_padding[4] = {
        options.padding_top,
        options.padding_right,
        options.padding_bottom,
        options.padding_left,
    };
    for (int i = 0; i < 4; ++i) {
        node.style.padding[i] = option_padding[i] >= 0.0f
            ? option_padding[i]
            : default_padding[i];
    }
    node.style.max_width = options.max_width;
    node.style.fixed_height = options.fixed_height;
    node.style.text_align = options.text_align;
    node.interaction_role = InteractionRole::Button;

    if (options.disabled) {
        node.background = options.has_background
            ? options.background
            : t.state_disabled_bg;
        node.text_color = options.has_text_color
            ? options.text_color
            : t.state_disabled_fg;
        node.border_color = options.has_border_color
            ? options.border_color
            : t.state_disabled_border;
        node.border_width = options.border_width >= 0.0f
            ? options.border_width
            : 1.0f;
        node.cursor_type = 0;
        node.focusable = false;
        node.debug_semantic_enabled = false;
        // No callback — the button is non-interactive. Skip the
        // callback registration entirely so click dispatch finds
        // node.callback_id == 0xFFFFFFFF and falls through.
        detail::attach_to_scope(h);
        return;
    }

    // Predict the callback id we're about to push so the hover/focus
    // samples mirror paint's own checks (phenotype.paint.cppm:632-636)
    // at view time.
    auto const id = static_cast<unsigned int>(
        detail::g_app.callbacks.size());
    bool const is_hovered = (id == detail::g_app.hovered_id);
    bool const is_focused = (id == detail::g_app.focused_id);

    Color base_bg, hover_bg, base_border;
    switch (options.variant) {
        case ButtonVariant::Primary:
            base_bg = t.accent;
            hover_bg = t.accent_strong;
            base_border = t.accent;
            node.text_color = t.state_active_fg;
            break;
        case ButtonVariant::Default:
        default:
            base_bg = t.surface;
            hover_bg = t.state_hover_bg;
            base_border = t.border;
            node.text_color = t.foreground;
            break;
    }
    if (options.has_background)
        base_bg = options.background;
    if (options.has_hover_background)
        hover_bg = options.hover_background;
    if (options.has_border_color)
        base_border = options.border_color;
    if (options.has_text_color)
        node.text_color = options.text_color;
    float const base_border_width = options.border_width >= 0.0f
        ? options.border_width
        : 1.0f;

    // `node.hover_background` stays unset so paint's
    // `(is_hovered && hover_background.a > 0)` guard falls through to
    // the animated `node.background` we just produced.
    node.background = animate_color(is_hovered ? hover_bg : base_bg, 150);
    // View-time focus ring: width grows from 1px to
    // `state_focus_ring_width`, colour cross-fades from the variant's
    // resting border to `state_focus_ring`. Paint reads both fields
    // directly.
    node.border_width = animate_float(
        is_focused ? t.state_focus_ring_width : base_border_width, 150);
    node.border_color = animate_color(
        is_focused ? t.state_focus_ring : base_border, 150);
    node.cursor_type = 1;

    detail::g_app.callbacks.push_back([msg = std::move(msg)] {
        detail::post<Msg>(msg);
        detail::trigger_rebuild();
    });
    detail::g_app.callback_roles.push_back(InteractionRole::Button);
    node.callback_id = id;

    detail::attach_to_scope(h);
}

template<typename Msg>
inline void button(str label, Msg msg,
                   ButtonVariant variant = ButtonVariant::Default,
                   bool disabled = false) {
    ButtonStyleOptions options;
    options.variant = variant;
    options.disabled = disabled;
    button(label, std::move(msg), options);
}

namespace _impl {
template<typename Msg>
inline void toggle(str label, bool active, Msg msg,
                   float corner_radius, Decoration active_decoration,
                   InteractionRole role) {
    auto const& t = ::phenotype::detail::g_app.theme;
    auto id = static_cast<unsigned int>(
        ::phenotype::detail::g_app.callbacks.size());
    bool const is_hovered = (id == ::phenotype::detail::g_app.hovered_id);
    bool const is_focused = (id == ::phenotype::detail::g_app.focused_id);
    Color const ring_off{t.state_focus_ring.r, t.state_focus_ring.g,
                         t.state_focus_ring.b, 0};
    auto row_h = ::phenotype::detail::alloc_node();
    {
        auto& row = ::phenotype::detail::node_at(row_h);
        row.style.flex_direction = FlexDirection::Row;
        row.style.cross_align    = CrossAxisAlignment::Center;
        row.style.gap            = t.space_sm;
        row.cursor_type          = 1;
        row.callback_id          = id;
        row.interaction_role     = role;
        row.focusable            = true;
        // Hover highlight: cross-fade between the surrounding card
        // surface and the hover-bg tint, both fully opaque, so the
        // RGB transition is monotonic. Earlier draft used an
        // alpha-only fade (state_hover_bg RGB at alpha 0/255) but
        // intermediate alphas combined with the rounded-corner
        // anti-aliasing read as a visible "deep darken" mid-fade.
        // `border_radius` keeps the highlight rounded.
        row.background = ::phenotype::animate_color(
            is_hovered ? t.state_hover_bg : t.surface, 150);
        row.border_radius = t.radius_sm;
        // Focus chrome on the row: width grows from 0 to
        // `state_focus_ring_width`, colour fades from the focus-ring
        // RGB at alpha 0 (invisible) to full alpha so checkbox/radio
        // gain a halo around the box+label without any resting border.
        row.border_width = ::phenotype::animate_float(
            is_focused ? t.state_focus_ring_width : 0.0f, 150);
        row.border_color = ::phenotype::animate_color(
            is_focused ? t.state_focus_ring : ring_off, 150);
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
        float box_size = ::phenotype::detail::g_app.theme.toggle_box_size;
        box.style.max_width    = box_size;
        box.style.fixed_height = box_size;
        box.border_radius      = corner_radius;
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
        lbl.focusable   = false;
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
    node.debug_semantic_label = node.placeholder;
    node.font_size = t.body_font_size;
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
        node.border_width = 1;
        node.cursor_type = 0;
        node.focusable = false;
        node.debug_semantic_enabled = false;
        // No callback / input handler — the field is non-interactive.
        detail::attach_to_scope(h);
        return;
    }

    node.is_input = true;
    Color resting_border;
    if (error) {
        node.background = t.state_error_bg;
        node.text_color = current.empty() ? t.muted : t.state_error_fg;
        resting_border = t.state_error_border;
    } else {
        node.background = t.surface;
        node.text_color = current.empty() ? t.muted : t.foreground;
        resting_border = t.border;
    }
    node.cursor_type = 1;

    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    bool const is_focused = (id == detail::g_app.focused_id);
    // View-time focus ring: width grows from 1px to
    // `state_focus_ring_width`, colour cross-fades from the resting
    // border (or `state_error_border`) to `state_focus_ring`.
    node.border_width = animate_float(
        is_focused ? t.state_focus_ring_width : 1.0f, 150);
    node.border_color = animate_color(
        is_focused ? t.state_focus_ring : resting_border, 150);
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

// canvas — fixed-size leaf for arbitrary 2D drawing. The paint pass
// hands the registered Painter a coordinate system whose origin is the
// canvas's resolved top-left; the callback emits lines (and, in time,
// rects / arcs / text) directly. Used by apps that need positions
// outside the layout-tree model — CAD plans, charts, custom widgets.
//
// By default the paint cache is bypassed for canvas nodes (paint_fn is
// opaque to the layout-prop diff), so each frame re-runs the callback.
// Apps with expensive per-frame paint should either pre-compute
// geometry in update() and capture by reference into the lambda, or
// opt into the token cache below.
//
// `paint_token` (optional) — caller-supplied uint64 dirty hint. When
// the canvas's emitted bytes are a pure function of inputs the caller
// already tracks (entity buffer pointer, transform, layer visibility,
// selected layout, etc.), hash those inputs into a single uint64 and
// pass it here. phenotype records the value after each emit; on the
// next frame, if the recorded token still matches, the canvas blits
// its prev_cmd_buf byte range and skips paint_fn entirely — and the
// canvas's ancestors regain cache eligibility too (a token-stable
// canvas no longer poisons its parents with paint_dynamic).
//
// Sentinel `0` = "no token / always-dirty": preserves the pre-token
// behaviour and is the default for every existing call site. WARNING:
// any callback whose output depends on state outside the token (e.g.
// `std::chrono::now()` for an animating canvas) MUST either pass `0`
// or mix a time bucket into the token, otherwise the canvas freezes
// at last-emitted bytes for as long as the token stays equal.
inline void canvas(float width, float height,
                   std::function<void(Painter&)> paint_fn,
                   std::function<void(GestureEvent const&)> on_gesture = {},
                   std::uint64_t paint_token = 0) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.max_width = width;
    node.style.fixed_height = height;
    node.paint_fn = std::move(paint_fn);
    node.paint_token = paint_token;
    if (on_gesture) {
        auto gid = static_cast<unsigned int>(
            detail::g_app.gesture_callbacks.size());
        detail::g_app.gesture_callbacks.push_back(std::move(on_gesture));
        node.gesture_callback_id = gid;
    }
    detail::attach_to_scope(h);
}

inline void svg_image(svg::Document document,
                      float width,
                      float height,
                      Color current_color) {
    canvas(width, height,
           [document = std::move(document),
            width,
            height,
            current_color](Painter& painter) {
               svg::paint(painter, document, 0.0f, 0.0f, width, height,
                          svg::RenderOptions{current_color, true});
           });
}

inline void svg_image(svg::Document document,
                      float width,
                      float height) {
    svg_image(std::move(document), width, height,
              detail::g_app.theme.foreground);
}

inline void svg_image(str source,
                      float width,
                      float height,
                      Color current_color) {
    svg_image(svg::parse(std::string_view{source.data, source.len}),
              width, height, current_color);
}

inline void svg_image(str source,
                      float width,
                      float height) {
    svg_image(source, width, height, detail::g_app.theme.foreground);
}

inline void icon(icons::Symbol symbol,
                 float size,
                 Color color) {
    canvas(size, size,
           [symbol, size, color](Painter& painter) {
               icons::paint_symbol(painter, symbol, 0.0f, 0.0f, size, color);
           },
           {},
           icons::paint_token(symbol, size, color));
}

inline void icon(icons::SymbolPresentation presentation) {
    canvas(presentation.point_size, presentation.point_size,
           [presentation](Painter& painter) {
               icons::paint_symbol(painter, presentation, 0.0f, 0.0f);
           },
           {},
           icons::paint_token(presentation));
}

inline void icon(icons::Symbol symbol, float size) {
    icon(symbol, size, detail::g_app.theme.foreground);
}

template<typename Msg>
inline void canvas_button(str label,
                          float width,
                          float height,
                          std::function<void(Painter&)> paint_fn,
                          Msg msg,
                          ButtonStyleOptions options = {},
                          std::uint64_t paint_token = 0) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    auto const& t = detail::g_app.theme;

    float default_padding[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float option_padding[4] = {
        options.padding_top,
        options.padding_right,
        options.padding_bottom,
        options.padding_left,
    };
    for (int i = 0; i < 4; ++i) {
        node.style.padding[i] = option_padding[i] >= 0.0f
            ? option_padding[i]
            : default_padding[i];
    }

    float const outer_width = options.max_width > 0.0f
        ? options.max_width
        : width;
    float const outer_height = options.fixed_height >= 0.0f
        ? options.fixed_height
        : height;

    node.style.flex_direction = FlexDirection::Row;
    node.style.cross_align = CrossAxisAlignment::Center;
    node.style.main_align = MainAxisAlignment::Center;
    node.style.max_width = outer_width;
    node.style.fixed_height = outer_height;
    node.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_sm;
    node.interaction_role = InteractionRole::Button;
    node.debug_semantic_label = std::string(label.data, label.len);

    if (options.disabled) {
        node.background = options.has_background
            ? options.background
            : t.state_disabled_bg;
        node.text_color = options.has_text_color
            ? options.text_color
            : t.state_disabled_fg;
        node.border_color = options.has_border_color
            ? options.border_color
            : t.state_disabled_border;
        node.border_width = options.border_width >= 0.0f
            ? options.border_width
            : 1.0f;
        node.cursor_type = 0;
        node.focusable = false;
        node.debug_semantic_enabled = false;
        detail::attach_to_scope(h);
    } else {
        auto const id = static_cast<unsigned int>(
            detail::g_app.callbacks.size());
        bool const is_hovered = (id == detail::g_app.hovered_id);
        bool const is_focused = (id == detail::g_app.focused_id);

        Color base_bg, hover_bg, base_border;
        switch (options.variant) {
            case ButtonVariant::Primary:
                base_bg = t.accent;
                hover_bg = t.accent_strong;
                base_border = t.accent;
                node.text_color = t.state_active_fg;
                break;
            case ButtonVariant::Default:
            default:
                base_bg = t.surface;
                hover_bg = t.state_hover_bg;
                base_border = t.border;
                node.text_color = t.foreground;
                break;
        }
        if (options.has_background)
            base_bg = options.background;
        if (options.has_hover_background)
            hover_bg = options.hover_background;
        if (options.has_border_color)
            base_border = options.border_color;
        if (options.has_text_color)
            node.text_color = options.text_color;
        float const base_border_width = options.border_width >= 0.0f
            ? options.border_width
            : 1.0f;

        node.background = animate_color(is_hovered ? hover_bg : base_bg, 150);
        node.border_width = animate_float(
            is_focused ? t.state_focus_ring_width : base_border_width, 150);
        node.border_color = animate_color(
            is_focused ? t.state_focus_ring : base_border, 150);
        node.cursor_type = 1;

        detail::g_app.callbacks.push_back([msg = std::move(msg)] {
            detail::post<Msg>(msg);
            detail::trigger_rebuild();
        });
        detail::g_app.callback_roles.push_back(InteractionRole::Button);
        node.callback_id = id;

        detail::attach_to_scope(h);
    }

    float const child_width = std::max(
        0.0f,
        outer_width - node.style.padding[1] - node.style.padding[3]);
    float const child_height = std::max(
        0.0f,
        outer_height - node.style.padding[0] - node.style.padding[2]);

    auto canvas_h = detail::alloc_node();
    auto& canvas_node = detail::node_at(canvas_h);
    canvas_node.style.max_width = child_width;
    canvas_node.style.fixed_height = child_height;
    canvas_node.paint_fn = std::move(paint_fn);
    canvas_node.paint_token = paint_token;
    canvas_node.debug_semantic_hidden = true;
    detail::append_child(h, canvas_h);
}

// progress — horizontal determinate progress bar. `value` is clamped
// to [0, 1] and drives the filled portion's width as a fraction of
// `max_width`. Track is theme.border, fill is theme.accent. Both pieces
// share the same rounded-pill chrome so partial fills look correct.
inline void progress(float value, float max_width = 200.0f) {
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    auto const& t = detail::g_app.theme;

    auto outer_h = detail::alloc_node();
    {
        auto& outer = detail::node_at(outer_h);
        outer.style.flex_direction = FlexDirection::Row;
        outer.style.cross_align = CrossAxisAlignment::Center;
        outer.style.max_width = max_width;
        outer.style.fixed_height = 6.0f;
        outer.background = t.border;
        outer.border_radius = 3.0f;
    }
    detail::attach_to_scope(outer_h);

    if (value > 0.0f) {
        auto bar_h = detail::alloc_node();
        auto& bar = detail::node_at(bar_h);
        bar.style.max_width = max_width * value;
        bar.style.fixed_height = 6.0f;
        bar.background = t.accent;
        bar.border_radius = 3.0f;
        detail::append_child(outer_h, bar_h);
    }
}

// progress_indeterminate — looping bar for "we're working but the
// total isn't known". A small accent slug oscillates left↔right
// inside the same chrome `progress` uses. The slug position is
// computed directly from `steady_ms()` rather than from a finite
// `animate_value`, so the loop runs forever; raising the auto-tick
// flag (`has_active_animations = true`) every view keeps the host
// loop scheduling repaints at ~60 Hz, the same mechanism the
// determinate animations rely on.
//
// Pure view-time read of the wall clock — no `framework_local`
// state — so two indeterminate bars in the same view stay in
// lockstep, which reads as intentional uniformity in a list of
// pending operations rather than visual noise.
inline void progress_indeterminate(float max_width = 200.0f) {
    auto const& t = detail::g_app.theme;

    auto outer_h = detail::alloc_node();
    {
        auto& outer = detail::node_at(outer_h);
        outer.style.flex_direction = FlexDirection::Row;
        outer.style.cross_align = CrossAxisAlignment::Center;
        outer.style.main_align = MainAxisAlignment::Start;
        outer.style.max_width = max_width;
        outer.style.fixed_height = 6.0f;
        outer.background = t.border;
        outer.border_radius = 3.0f;
    }
    detail::attach_to_scope(outer_h);

    constexpr int cycle_ms = 1500;
    constexpr float bar_fraction = 0.35f;
    auto const now_ms = detail::steady_ms();
    auto const phase_ms = now_ms % cycle_ms;
    float phase = static_cast<float>(phase_ms)
                  / static_cast<float>(cycle_ms);
    // Triangular oscillation — slug travels left→right in the first
    // half of the cycle and right→left in the second so the motion
    // is continuous with no reset jump.
    float pos = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
    float lead = (max_width * (1.0f - bar_fraction)) * pos;
    // Flex layout treats `max_width <= 0` as "no limit"; clamp to a
    // sub-pixel positive value so the leading spacer always reads
    // as a fixed-width slot. (Same trick `widget::switch_` uses.)
    if (lead < 0.001f) lead = 0.001f;
    {
        auto sp_h = detail::alloc_node();
        auto& sp = detail::node_at(sp_h);
        sp.style.max_width = lead;
        sp.style.fixed_height = 6.0f;
        detail::append_child(outer_h, sp_h);
    }
    {
        auto bar_h = detail::alloc_node();
        auto& bar = detail::node_at(bar_h);
        bar.style.max_width = max_width * bar_fraction;
        bar.style.fixed_height = 6.0f;
        bar.background = t.accent;
        bar.border_radius = 3.0f;
        detail::append_child(outer_h, bar_h);
    }

    // Keep the auto-tick alive so the host loop keeps repainting and
    // the slug advances frame to frame.
    detail::g_app.has_active_animations = true;
}

// switch_ — labelled on/off toggle rendered as a track + sliding
// thumb. The trailing underscore avoids the C++ `switch` keyword.
// Click posts `msg` and triggers a rebuild, same contract as
// checkbox / radio.
//
// The thumb's x-offset and the track background colour both go
// through `animate_value`, so toggling slides the thumb (~150ms)
// and cross-fades the track between theme.border and theme.accent.
// The runner's auto-tick keeps the fade running between input
// events. Per-call-site widget ids let two switches in one view
// animate independently — see the framework_local sibling-counter
// mechanism.
template<typename Msg>
inline void switch_(str label, bool on, Msg msg) {
    auto const& t = detail::g_app.theme;
    auto id = static_cast<unsigned int>(
        detail::g_app.callbacks.size());
    bool const is_focused = (id == detail::g_app.focused_id);
    Color const ring_off{t.state_focus_ring.r, t.state_focus_ring.g,
                         t.state_focus_ring.b, 0};

    auto row_h = detail::alloc_node();
    {
        auto& row = detail::node_at(row_h);
        row.style.flex_direction = FlexDirection::Row;
        row.style.cross_align = CrossAxisAlignment::Center;
        row.style.gap = t.space_sm;
        row.cursor_type = 1;
        row.callback_id = id;
        row.interaction_role = InteractionRole::Checkbox;
        row.focusable = true;
        // Focus chrome on the outer row so the ring wraps around
        // track + label, not just the track. Width grows from 0 to
        // `state_focus_ring_width`, colour fades in/out via alpha.
        row.border_width = animate_float(
            is_focused ? t.state_focus_ring_width : 0.0f, 150);
        row.border_color = animate_color(
            is_focused ? t.state_focus_ring : ring_off, 150);
        row.debug_semantic_role = "switch";
        row.debug_semantic_label = std::string(label.data, label.len);
        row.debug_semantic_callback_id = id;
        row.debug_semantic_focusable = true;
    }
    detail::attach_to_scope(row_h);

    detail::g_app.callbacks.push_back([msg = std::move(msg)] {
        detail::post<Msg>(msg);
        detail::trigger_rebuild();
    });
    detail::g_app.callback_roles.push_back(InteractionRole::Checkbox);

    // Track (the row's first child). Always Row + Start aligned;
    // the thumb's position is driven by an animated leading spacer
    // rather than by `main_align`, so it can stop at any sub-pixel
    // offset between the two ends mid-fade.
    auto track_h = detail::alloc_node();
    {
        auto& tr = detail::node_at(track_h);
        tr.style.flex_direction = FlexDirection::Row;
        tr.style.cross_align = CrossAxisAlignment::Center;
        tr.style.main_align = MainAxisAlignment::Start;
        tr.style.max_width = 32.0f;
        tr.style.fixed_height = 18.0f;
        tr.style.padding[0] = 2.0f;
        tr.style.padding[1] = 2.0f;
        tr.style.padding[2] = 2.0f;
        tr.style.padding[3] = 2.0f;
        tr.background = animate_color(on ? t.accent : t.border, 150);
        tr.border_radius = 9.0f;
        tr.debug_semantic_hidden = true;
    }
    detail::append_child(row_h, track_h);

    // Leading spacer — its max_width animates between 0 (off) and
    // (track_inner − thumb) = 14 (on), pushing the thumb across.
    // We clamp the lower bound to a sub-pixel positive value so the
    // row layout still treats it as a fixed-width child rather than
    // a zero-width unspecified node (the existing flex pipeline
    // treats `max_width <= 0` as "no limit").
    {
        float thumb_offset = animate_float(on ? 14.0f : 0.0f, 150);
        if (thumb_offset < 0.001f) thumb_offset = 0.001f;
        auto sp_h = detail::alloc_node();
        auto& sp = detail::node_at(sp_h);
        sp.style.max_width = thumb_offset;
        sp.style.fixed_height = 14.0f;
        detail::append_child(track_h, sp_h);
    }

    // Thumb — circular by virtue of the border_radius matching half
    // its size.
    {
        auto thumb_h = detail::alloc_node();
        auto& th = detail::node_at(thumb_h);
        th.style.max_width = 14.0f;
        th.style.fixed_height = 14.0f;
        th.background = t.surface;
        th.border_radius = 7.0f;
        detail::append_child(track_h, thumb_h);
    }

    {
        auto lbl_h = detail::alloc_node();
        auto& lbl = detail::node_at(lbl_h);
        lbl.text = std::string(label.data, label.len);
        lbl.font_size = t.body_font_size;
        lbl.text_color = t.foreground;
        lbl.focusable = false;
        lbl.debug_semantic_hidden = true;
        detail::append_child(row_h, lbl_h);
    }
}

// tabs — segmented row of buttons that posts `on_select(i)` when the
// user picks tab `i`. The currently-selected index is supplied by the
// caller (typically lifted into user `State`), so the widget itself
// stays stateless — `Msg`/`update` round-trip is the source of truth.
//
// Visual chrome is the design system's "pill" treatment: a code_bg-
// filled outer container with rounded corners. The pill is now a
// Column wrapping (a) the row of tabs and (b) a sliding 2-pixel
// indicator track. Tabs use `flex_grow = 1` and `gap = 0` so each
// occupies exactly `inner_width / N` — the indicator track divides
// the same inner width into a leading spacer + 1-unit indicator +
// trailing spacer, with the spacer grows interpolated by
// `animate_float` so the indicator slides under the new tab when
// `selected` changes.
//
// The selected button still paints in `theme.accent` with
// `theme.state_active_fg` text, unselected buttons stay transparent
// with muted text and a hover background — adding the indicator
// supplements the pill highlight rather than replacing it.
//
// Each tab is focusable, so Tab/Enter cycling lands on a tab and
// activates it via the same handler the pointer click fires. The
// callback is a stateless lambda that decays to a function pointer
// so the `Msg(*)(std::size_t)` signature stays terse at the call
// site.
template<typename Msg>
inline void tabs(std::vector<str> const& items,
                 std::size_t selected,
                 Msg (*on_select)(std::size_t)) {
    auto const& t = detail::g_app.theme;

    auto pill_h = detail::alloc_node();
    {
        auto& pill = detail::node_at(pill_h);
        pill.style.flex_direction = FlexDirection::Column;
        pill.style.gap = 0;
        pill.background = t.code_bg;
        pill.border_radius = t.radius_md;
        pill.style.padding[0] = t.space_xs;
        pill.style.padding[1] = t.space_xs;
        pill.style.padding[2] = t.space_xs;
        pill.style.padding[3] = t.space_xs;
    }
    detail::attach_to_scope(pill_h);

    auto row_h = detail::alloc_node();
    {
        auto& row = detail::node_at(row_h);
        row.style.flex_direction = FlexDirection::Row;
        row.style.gap = 0;
    }
    detail::append_child(pill_h, row_h);

    Color const ring_off{t.state_focus_ring.r, t.state_focus_ring.g,
                         t.state_focus_ring.b, 0};
    for (std::size_t i = 0; i < items.size(); ++i) {
        bool is_selected = (i == selected);
        auto id = static_cast<unsigned int>(
            detail::g_app.callbacks.size());
        bool const is_focused = (id == detail::g_app.focused_id);

        auto btn_h = detail::alloc_node();
        {
            auto& btn = detail::node_at(btn_h);
            // Tab label lives on a child node (below) so this btn is
            // a flex container, not a text leaf — the flex_grow
            // distribution the indicator track relies on only fires
            // for non-text children (the row layout's text-leaf
            // branch always pins the natural measured width).
            // Active tab gets a `t.surface` bg over the pill's
            // `t.code_bg` so the selected tab reads as a raised slot;
            // the accent indicator underneath is the colour cue. The
            // active tab's hover_bg matches its resting bg so the
            // hover state doesn't visually fight the indicator.
            btn.background = is_selected ? t.surface : t.transparent;
            btn.hover_background = is_selected
                ? t.surface
                : t.state_hover_bg;
            btn.border_radius = t.radius_sm;
            btn.style.flex_grow = 1.0f;
            btn.style.flex_direction = FlexDirection::Column;
            btn.style.cross_align = CrossAxisAlignment::Center;
            // Focus ring grows from no border to
            // `state_focus_ring_width` and back, matching the rest of
            // the focusable widget set; colour fades in/out via alpha.
            btn.border_width = animate_float(
                is_focused ? t.state_focus_ring_width : 0.0f, 150);
            btn.border_color = animate_color(
                is_focused ? t.state_focus_ring : ring_off, 150);
            btn.style.padding[0] = t.space_xs;
            btn.style.padding[1] = t.space_sm;
            btn.style.padding[2] = t.space_xs;
            btn.style.padding[3] = t.space_sm;
            btn.cursor_type = 1;
            btn.callback_id = id;
            btn.interaction_role = InteractionRole::Button;
            btn.focusable = true;
            btn.debug_semantic_role = "tab";
            btn.debug_semantic_label =
                std::string(items[i].data, items[i].len);
            btn.debug_semantic_callback_id = id;
            btn.debug_semantic_focusable = true;
        }
        detail::append_child(row_h, btn_h);

        {
            auto lbl_h = detail::alloc_node();
            auto& lbl = detail::node_at(lbl_h);
            lbl.text = std::string(items[i].data, items[i].len);
            lbl.font_size = t.body_font_size;
            // Active label colour matches the indicator below so the
            // accent reads as the single selection cue.
            lbl.text_color = is_selected ? t.accent : t.muted;
            lbl.focusable = false;
            lbl.debug_semantic_hidden = true;
            detail::append_child(btn_h, lbl_h);
        }

        detail::g_app.callbacks.push_back([on_select, idx = i] {
            detail::post<Msg>(on_select(idx));
            detail::trigger_rebuild();
        });
        detail::g_app.callback_roles.push_back(InteractionRole::Button);
    }

    // Sliding indicator track. Children: leading spacer, 1-unit
    // indicator, trailing spacer. The leading and trailing
    // `flex_grow` values interpolate over ~150ms whenever `selected`
    // changes, so the indicator slides under the new tab while every
    // tab keeps its `1/N` slot.
    auto track_h = detail::alloc_node();
    {
        auto& tr = detail::node_at(track_h);
        tr.style.flex_direction = FlexDirection::Row;
        tr.style.gap = 0;
    }
    detail::append_child(pill_h, track_h);

    {
        float lead = animate_float(static_cast<float>(selected), 150);
        if (lead < 0.0f) lead = 0.0f;
        auto sp_h = detail::alloc_node();
        auto& sp = detail::node_at(sp_h);
        sp.style.flex_grow = lead;
        sp.style.fixed_height = 2.0f;
        detail::append_child(track_h, sp_h);
    }
    {
        auto line_h = detail::alloc_node();
        auto& ln = detail::node_at(line_h);
        ln.style.flex_grow = 1.0f;
        // `fixed_height` lives on the leaf so the row layout's
        // text-leaf branch picks it up; setting it on the parent
        // track wouldn't apply because that node has children.
        ln.style.fixed_height = 2.0f;
        ln.background = t.accent;
        detail::append_child(track_h, line_h);
    }
    {
        float trail = animate_float(
            static_cast<float>(items.size() - 1) -
            static_cast<float>(selected),
            150);
        if (trail < 0.0f) trail = 0.0f;
        auto sp_h = detail::alloc_node();
        auto& sp = detail::node_at(sp_h);
        sp.style.flex_grow = trail;
        sp.style.fixed_height = 2.0f;
        detail::append_child(track_h, sp_h);
    }
}

} // namespace widget

namespace app {

template<typename Msg>
inline void key_command(unsigned int key,
                        int modifiers,
                        Msg msg,
                        KeyCommandOptions options = {}) {
    if (options.disabled)
        return;
    auto const id = static_cast<unsigned int>(
        detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([msg = std::move(msg)] {
        detail::post<Msg>(msg);
        detail::trigger_rebuild();
    });
    detail::g_app.callback_roles.push_back(InteractionRole::Command);
    detail::g_app.key_commands.push_back(KeyCommand{
        .key = key,
        .modifiers = modifiers,
        .allow_when_input_focused = options.allow_when_input_focused,
        .callback_id = id,
        .debug_label = std::move(options.debug_label),
    });
}

} // namespace app

// ============================================================
// View-time animation
// ============================================================
//
// `animate_color` / `animate_float` (and `animate_value<T>`) interpolate
// from a stored start value toward a `target` over `duration_ms`. The
// per-call-site state lives in `framework_local`, so a hover transition
// reads as:
//
//   node.background = animate_color(
//       hovered ? theme.accent : theme.surface, 150);
//
// Each call records `(start_value, target, start_time)` and returns the
// time-interpolated value. When the target changes between frames the
// current interpolated value is captured as the new start, so an
// interruption mid-fade slides smoothly into the new direction rather
// than snapping.
//
// Limitation: the interpolation only advances when the runner actually
// rebuilds — usually whenever an input event triggers a repaint. Mouse
// movement during a hover transition keeps it animating, but a pure
// time-based fade with no input will freeze once the input that
// triggered it stops. A follow-up PR will wire a shell timer that
// schedules another paint while any animation is in progress.

namespace detail {

// Steady-clock milliseconds-since-epoch. Defined non-inline so the
// `<chrono>` types stay inside this module's translation unit —
// consumers that instantiate `animate_value<T>` (e.g. inside a
// widget template) only see an `std::int64_t` interface and don't
// need to import chrono themselves. MSVC enforces module type
// reachability strictly enough to surface this; Clang was lenient.
std::int64_t steady_ms();

// Per-call-site animation state, kept alive across frames by
// `framework_local`. `initialized` distinguishes the very first call
// (where we just snap to target, no animation) from later ones. The
// timestamp is plain int64 milliseconds for the same reachability
// reason as `steady_ms` above.
template <typename T>
struct AnimationState {
    T start_value{};
    T target{};
    std::int64_t start_time_ms = 0;
    bool initialized = false;
};

inline float anim_lerp(float a, float b, float t) noexcept {
    return a + (b - a) * t;
}

inline Color anim_lerp(Color a, Color b, float t) noexcept {
    auto mix = [t](unsigned char x, unsigned char y) -> unsigned char {
        float v = anim_lerp(static_cast<float>(x), static_cast<float>(y), t);
        if (v < 0.0f) v = 0.0f;
        if (v > 255.0f) v = 255.0f;
        return static_cast<unsigned char>(v + 0.5f);
    };
    return Color{ mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a) };
}

}  // namespace detail

template <typename T>
T animate_value(T target, int duration_ms,
                std::source_location loc = std::source_location::current()) {
    auto& s = framework_local<detail::AnimationState<T>>(
        detail::AnimationState<T>{}, 0, loc);
    auto now_ms = detail::steady_ms();

    if (!s.initialized || duration_ms <= 0) {
        // First call, or "instant" mode — snap to target without
        // interpolating. `duration_ms <= 0` is also the way callers
        // turn animation off entirely for a given site, so we keep
        // the bookkeeping consistent with start_value == target.
        s.start_value = target;
        s.target = target;
        s.start_time_ms = now_ms;
        s.initialized = true;
        return target;
    }

    auto elapsed_ms = now_ms - s.start_time_ms;
    if (elapsed_ms < 0) elapsed_ms = 0;
    float progress = std::min(1.0f,
        static_cast<float>(elapsed_ms)
        / static_cast<float>(duration_ms));
    T current = detail::anim_lerp(s.start_value, s.target, progress);

    // Signal to the host loop that we still want another frame so
    // the interpolation keeps advancing without needing input. The
    // runner clears the flag at the start of every view; native
    // shells re-enter `repaint_current` while it stays set.
    if (progress < 1.0f) {
        detail::g_app.has_active_animations = true;
    }

    if (!(s.target == target)) {
        // Target shifted mid-flight — capture the current interpolated
        // value as the new starting point so the new fade picks up
        // where the old one was, instead of snapping.
        s.start_value = current;
        s.target = target;
        s.start_time_ms = now_ms;
        // A new fade has just started, so it's definitionally
        // unfinished and we want another tick.
        detail::g_app.has_active_animations = true;
        return current;
    }
    return current;
}

inline float animate_float(float target, int duration_ms,
                           std::source_location loc) {
    return animate_value<float>(target, duration_ms, loc);
}

inline Color animate_color(Color target, int duration_ms,
                           std::source_location loc) {
    return animate_value<Color>(target, duration_ms, loc);
}

namespace detail {
// Out-of-line definition keeps `<chrono>` away from `animate_value`'s
// template body. Consumers that instantiate the template (any widget
// using animate_*) only see the `std::int64_t` return type and don't
// need to import / include chrono themselves.
std::int64_t steady_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

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
        app.key_commands.clear();
        app.gesture_callbacks.clear();
        app.gesture_target_id = 0xFFFFFFFFu;
        app.scroll_targets.clear();
        app.overlays.clear();
        app.input_handlers.clear();
        app.input_nodes.clear();

        auto root_h = detail::alloc_node();
        {
            auto& root = detail::node_at(root_h);
            root.style.flex_direction = FlexDirection::Column;
            root.background = app.theme.background;
        }
        app.root = root_h;

        // Tag this frame's framework_local accesses so prune can drop
        // entries that disappeared from the view tree. Bumped before
        // saved_view runs; entries written/read during view stamp the
        // current generation; prune erases anything still on the prior
        // generation.
        detail::bump_local_gen();
        // Reset the auto-tick flag — animate_value re-sets it during
        // view if any interpolation is still in flight.
        app.has_active_animations = false;
        float cw = host.canvas_width();
        float vh = host.canvas_height();
        app.debug_viewport_width = cw;
        app.debug_viewport_height = vh;

        Scope scope(root_h);
        Scope::set_current(&scope);
        saved_view(state);
        Scope::set_current(nullptr);
        detail::prune_local_store();
        auto t2 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t2 - t1, {{"phase", "view"}});

        if (app.prev_root.valid())
            detail::diff_and_copy_layout(app.prev_root, root_h,
                                         app.prev_arena, app.arena);

        detail::layout_node(host, root_h, cw);
        for (auto overlay_h : app.overlays)
            detail::layout_node(host, overlay_h, cw);
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
        // While any view-time interpolation is still advancing, the
        // animated `node.*` values change every frame without the
        // owning callback_id appearing in the diff above (e.g. a
        // widget that just lost focus is no longer in `focused_id`,
        // but its border_width is still fading out). Force-invalidate
        // every cached subtree until everything converges; the flag
        // self-clears at the start of the next view.
        if (app.has_active_animations) {
            inv = ~static_cast<std::uint64_t>(0);
        }
        app.paint_invalidation_mask = inv;

        app.focusable_ids.clear();
        detail::collect_focusable_ids(root_h);
        for (auto overlay_h : app.overlays)
            detail::collect_focusable_ids(overlay_h);
        app.paint_scissor_depth = 0;
        emit_clear(host, app.theme.background);
        detail::paint_node(host, host, root_h, 0, 0,
                           app.scroll_x, app.scroll_y, cw, vh);
        detail::reset_paint_scissor_boundary(host);
        // Overlays paint after the main tree with no ambient scroll
        // applied — they sit on top of everything and stay fixed when
        // the document scrolls underneath.
        for (auto overlay_h : app.overlays) {
            detail::paint_node(host, host, overlay_h, 0, 0,
                               0.0f, 0.0f, cw, vh);
        }
        auto t4 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t4 - t3, {{"phase", "paint"}});

        flush_if_changed(host);
        auto t5 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t5 - t4, {{"phase", "flush"}});

        // Persist the ambient paint inputs for next frame's blit guard.
        app.prev_scroll_x   = app.scroll_x;
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
        app.key_commands.clear();
        app.gesture_callbacks.clear();
        app.gesture_target_id = 0xFFFFFFFFu;
        app.scroll_targets.clear();
        app.overlays.clear();
        app.input_handlers.clear();
        app.input_nodes.clear();

        auto root_h = detail::alloc_node();
        {
            auto& root = detail::node_at(root_h);
            root.style.flex_direction = FlexDirection::Column;
            root.background = app.theme.background;
        }
        app.root = root_h;

        // See native runner — framework_local generation handling.
        detail::bump_local_gen();
        app.has_active_animations = false;
        float cw = phenotype_get_canvas_width();
        float vh = phenotype_get_canvas_height();
        app.debug_viewport_width = cw;
        app.debug_viewport_height = vh;

        Scope scope(root_h);
        Scope::set_current(&scope);
        saved_view(state);
        Scope::set_current(nullptr);
        detail::prune_local_store();
        auto t2 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t2 - t1, {{"phase", "view"}});

        if (app.prev_root.valid())
            detail::diff_and_copy_layout(app.prev_root, root_h,
                                         app.prev_arena, app.arena);

        detail::layout_node(root_h, cw);
        for (auto overlay_h : app.overlays)
            detail::layout_node(overlay_h, cw);
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
        // While any view-time interpolation is still advancing, the
        // animated `node.*` values change every frame without the
        // owning callback_id appearing in the diff above (e.g. a
        // widget that just lost focus is no longer in `focused_id`,
        // but its border_width is still fading out). Force-invalidate
        // every cached subtree until everything converges; the flag
        // self-clears at the start of the next view.
        if (app.has_active_animations) {
            inv = ~static_cast<std::uint64_t>(0);
        }
        app.paint_invalidation_mask = inv;

        app.focusable_ids.clear();
        detail::collect_focusable_ids(root_h);
        for (auto overlay_h : app.overlays)
            detail::collect_focusable_ids(overlay_h);
        app.paint_scissor_depth = 0;
        detail::wasi_emit_clear(app.theme.background);
        detail::wasi_paint_node(root_h, 0, 0,
                                app.scroll_x, app.scroll_y, cw, vh);
        detail::wasi_reset_paint_scissor_boundary();
        // Overlays paint on top of the main tree with no ambient
        // scroll — see native runner for the same logic.
        for (auto overlay_h : app.overlays) {
            detail::wasi_paint_node(overlay_h, 0, 0,
                                    0.0f, 0.0f, cw, vh);
        }
        auto t4 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t4 - t3, {{"phase", "paint"}});

        detail::wasi_flush_if_changed();
        auto t5 = metrics::detail::now_ns();
        metrics::inst::phase_duration.record(t5 - t4, {{"phase", "flush"}});

        // Persist the ambient paint inputs for next frame's blit guard.
        app.prev_scroll_x   = app.scroll_x;
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

// Mix the parent scope's widget_id_seed with the container node's
// arena index so each container gets a stable, parent-aware seed —
// what makes `framework_local` calls inside sibling containers (e.g.
// two `column`s in the same parent) hash to distinct widget_ids
// instead of all colliding at seed=0. NodeHandle.index is sequential
// within a frame and reproduces frame to frame as long as the tree
// shape is stable, so animation slots persist across frames.
//
// Without this, every framework_local at counter=0 inside a sibling
// scope shared the same slot — the colorwh.dwg view picker showed
// the symptom (Open / AutoCAD Color Index / 2D View buttons all
// hovered together, and a stale Primary background leaked into the
// Color Index button's animated fade). Documented limitation in
// phenotype.state.cppm:714 was the same root cause.
inline std::size_t derived_scope_seed(NodeHandle h) noexcept {
    auto* prev = Scope::current();
    std::size_t parent_seed = prev ? prev->widget_id_seed : 0;
    return hash_combine(parent_seed, static_cast<std::size_t>(h.index));
}

inline void open_container(NodeHandle container, std::function<void()> builder) {
    attach_to_scope(container);
    Scope scope(container, derived_scope_seed(container));
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

inline MaterialStyle material_style(MaterialKind kind) noexcept {
    return material_style_for_kind(kind, detail::g_app.theme);
}

struct MaterialContainerOptions {
    std::uint32_t container_id = 0;
    std::uint32_t union_id = 0;
    float spacing = 20.0f;
    bool interactive = false;
    bool morph_transitions = true;
};

inline MaterialContainerDescriptor material_container_descriptor(
        MaterialContainerOptions const& options) noexcept {
    return MaterialContainerDescriptor{
        options.container_id,
        options.union_id,
        options.spacing,
        options.interactive,
        options.morph_transitions};
}

inline std::vector<MaterialContainerDescriptor>& material_container_stack() {
    static std::vector<MaterialContainerDescriptor>& stack =
        *new std::vector<MaterialContainerDescriptor>();
    return stack;
}

inline MaterialContainerDescriptor current_material_container() {
    auto& stack = material_container_stack();
    return stack.empty() ? MaterialContainerDescriptor{} : stack.back();
}

template<typename F>
    requires std::is_invocable_v<F>
void material_container(MaterialContainerOptions options, F&& builder) {
    auto& stack = material_container_stack();
    stack.push_back(material_container_descriptor(options));
    builder();
    stack.pop_back();
}

// column — vertical flex container.
//
// Builder overload accepts gap / cross-axis / main-axis props. The
// variadic overload keeps its defaults-only behaviour so existing call
// sites stay terse; pass a builder lambda when you need the props.
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
    auto* prev = Scope::current();
    Scope child_scope(h, detail::derived_scope_seed(h));
    Scope::set_current(&child_scope);
    (detail::render_one(std::forward<A>(a)),
     detail::render_one(std::forward<B>(b)),
     (detail::render_one(std::forward<Rest>(rest)), ...));
    Scope::set_current(prev);
}

// row — horizontal flex container. Builder overload exposes gap and
// alignment props; the variadic overload keeps the defaults.
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
    auto* prev = Scope::current();
    Scope child_scope(h, detail::derived_scope_seed(h));
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
    auto* prev = Scope::current();
    Scope child_scope(h, detail::derived_scope_seed(h));
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

// weighted — wrap a builder in a container that claims a proportional
// share of the row's leftover space. `grow` is the weight; sibling
// weighted children split the remainder of the row's main axis in
// proportion to their values (e.g. `weighted(2, ...)` next to
// `weighted(1, ...)` gets twice the width). Children without a
// weighted wrapper keep their intrinsic / max_width sizing and are
// laid out before the split runs, so weights apply only to whatever
// remains after fixed siblings have taken their share.
//
// Currently honoured by row layout only — column flex distribution
// waits on a bounded-height story (the parent's height is usually
// content-derived, so there's no remainder to split).
template<typename F>
    requires std::is_invocable_v<F>
void weighted(float grow, F&& builder) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_grow = grow;
    detail::open_container(h, std::forward<F>(builder));
}

struct MaterialSurfaceOptions {
    MaterialKind kind = MaterialKind::Regular;
    MaterialSurfaceRole role = MaterialSurfaceRole::Surface;
    FlexDirection direction = FlexDirection::Column;
    SpaceToken padding = SpaceToken::Lg;
    SpaceToken gap = SpaceToken::Md;
    CrossAxisAlignment cross_align = CrossAxisAlignment::Start;
    MainAxisAlignment main_align = MainAxisAlignment::Start;
    float max_width = 0.0f;
    float fixed_height = -1.0f;
    float border_radius = -1.0f;
    float border_width = -1.0f;
    char const* semantic_label = "";
    bool inherit_material_container = true;
    MaterialContainerDescriptor container{};
};

enum class GlassSurfacePreset {
    Window,
    Toolbar,
    ToolbarGroup,
    Navigation,
    Sidebar,
    Content,
    StatusBar,
};

inline char const* glass_surface_preset_name(
        GlassSurfacePreset preset) noexcept {
    switch (preset) {
        case GlassSurfacePreset::Window:       return "window";
        case GlassSurfacePreset::Toolbar:      return "toolbar";
        case GlassSurfacePreset::ToolbarGroup: return "toolbar_group";
        case GlassSurfacePreset::Navigation:   return "navigation";
        case GlassSurfacePreset::Sidebar:      return "sidebar";
        case GlassSurfacePreset::Content:      return "content";
        case GlassSurfacePreset::StatusBar:    return "status_bar";
    }
    return "content";
}

inline void configure_material_surface(LayoutNode& node,
                                       MaterialSurfaceOptions const& options) {
    auto const& t = detail::g_app.theme;
    node.material = material_style(options.kind);
    node.material.role = options.role;
    node.material.container = options.inherit_material_container
        ? current_material_container()
        : options.container;
    node.background = node.material.tint;
    node.border_color = node.material.border;
    node.border_width = options.border_width >= 0.0f
        ? options.border_width
        : (options.kind == MaterialKind::None ? 0.0f : 1.0f);
    node.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_lg;
    node.style.flex_direction = options.direction;
    node.style.gap = space_value(options.gap);
    node.style.cross_align = options.cross_align;
    node.style.main_align = options.main_align;
    node.style.max_width = options.max_width;
    node.style.fixed_height = options.fixed_height;
    float p = space_value(options.padding);
    node.style.padding[0] = p;
    node.style.padding[1] = p;
    node.style.padding[2] = p;
    node.style.padding[3] = p;
    if (options.semantic_label && options.semantic_label[0] != '\0')
        node.debug_semantic_label = options.semantic_label;
}

template<typename F>
    requires std::is_invocable_v<F>
void material_surface(MaterialSurfaceOptions options, F&& builder) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    configure_material_surface(node, options);
    detail::open_container(h, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void material_surface(MaterialKind kind, F&& builder,
                      SpaceToken padding = SpaceToken::Lg,
                      SpaceToken gap = SpaceToken::Md) {
    material_surface(
        MaterialSurfaceOptions{
            .kind = kind,
            .padding = padding,
            .gap = gap,
        },
        std::forward<F>(builder));
}

inline char const* chrome_label_or(char const* label,
                                   char const* fallback) noexcept {
    return label && label[0] != '\0' ? label : fallback;
}

inline MaterialSurfaceOptions glass_surface_options(
        GlassSurfacePreset preset,
        char const* semantic_label = "") {
    auto const& t = detail::g_app.theme;
    MaterialSurfaceOptions options{};
    options.border_width = 0.0f;
    options.border_radius = t.radius_lg;

    switch (preset) {
        case GlassSurfacePreset::Window:
            options.kind = MaterialKind::Clear;
            options.role = MaterialSurfaceRole::Surface;
            options.direction = FlexDirection::Row;
            options.padding = SpaceToken::Xs;
            options.gap = SpaceToken::Sm;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Glass Window");
            break;
        case GlassSurfacePreset::Toolbar:
            options.kind = MaterialKind::Clear;
            options.role = MaterialSurfaceRole::Toolbar;
            options.direction = FlexDirection::Row;
            options.padding = SpaceToken::Xs;
            options.gap = SpaceToken::Xs;
            options.cross_align = CrossAxisAlignment::Center;
            options.border_radius = 0.0f;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Toolbar");
            break;
        case GlassSurfacePreset::ToolbarGroup:
            options.kind = MaterialKind::Thick;
            options.role = MaterialSurfaceRole::Toolbar;
            options.direction = FlexDirection::Row;
            options.padding = SpaceToken::Xs;
            options.gap = SpaceToken::Xs;
            options.cross_align = CrossAxisAlignment::Center;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Toolbar Group");
            break;
        case GlassSurfacePreset::Navigation:
            options.kind = MaterialKind::Thin;
            options.role = MaterialSurfaceRole::Navigation;
            options.direction = FlexDirection::Row;
            options.padding = SpaceToken::Sm;
            options.gap = SpaceToken::Xs;
            options.cross_align = CrossAxisAlignment::Center;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Navigation");
            break;
        case GlassSurfacePreset::Sidebar:
            options.kind = MaterialKind::Thin;
            options.role = MaterialSurfaceRole::Sidebar;
            options.direction = FlexDirection::Column;
            options.padding = SpaceToken::Md;
            options.gap = SpaceToken::Sm;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Sidebar");
            break;
        case GlassSurfacePreset::Content:
            options.kind = MaterialKind::Regular;
            options.role = MaterialSurfaceRole::Content;
            options.direction = FlexDirection::Column;
            options.padding = SpaceToken::Xl;
            options.gap = SpaceToken::Md;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Content");
            break;
        case GlassSurfacePreset::StatusBar:
            options.kind = MaterialKind::Clear;
            options.role = MaterialSurfaceRole::StatusBar;
            options.direction = FlexDirection::Column;
            options.padding = SpaceToken::Sm;
            options.gap = SpaceToken::Xs;
            options.border_radius = t.radius_md;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Status Bar");
            break;
    }
    return options;
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_surface(GlassSurfacePreset preset,
                   F&& builder,
                   char const* semantic_label = "") {
    material_surface(
        glass_surface_options(preset, semantic_label),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void toolbar(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Toolbar;
    options.semantic_label = chrome_label_or(options.semantic_label, "Toolbar");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void toolbar(F&& builder,
             MaterialKind kind = MaterialKind::Clear,
             SpaceToken padding = SpaceToken::Md,
             SpaceToken gap = SpaceToken::Sm) {
    material_surface(
        MaterialSurfaceOptions{
            .kind = kind,
            .role = MaterialSurfaceRole::Toolbar,
            .direction = FlexDirection::Row,
            .padding = padding,
            .gap = gap,
            .cross_align = CrossAxisAlignment::Center,
            .main_align = MainAxisAlignment::Start,
            .semantic_label = "Toolbar",
        },
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void navigation(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Navigation;
    options.semantic_label = chrome_label_or(options.semantic_label,
                                             "Navigation");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void navigation(F&& builder,
                MaterialKind kind = MaterialKind::Thin,
                SpaceToken padding = SpaceToken::Sm,
                SpaceToken gap = SpaceToken::Xs) {
    material_surface(
        MaterialSurfaceOptions{
            .kind = kind,
            .role = MaterialSurfaceRole::Navigation,
            .direction = FlexDirection::Row,
            .padding = padding,
            .gap = gap,
            .cross_align = CrossAxisAlignment::Center,
            .main_align = MainAxisAlignment::Start,
            .semantic_label = "Navigation",
        },
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void sidebar(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Sidebar;
    options.semantic_label = chrome_label_or(options.semantic_label, "Sidebar");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void sidebar(float max_width,
             F&& builder,
             MaterialKind kind = MaterialKind::Thin,
             SpaceToken padding = SpaceToken::Md,
             SpaceToken gap = SpaceToken::Sm) {
    material_surface(
        MaterialSurfaceOptions{
            .kind = kind,
            .role = MaterialSurfaceRole::Sidebar,
            .direction = FlexDirection::Column,
            .padding = padding,
            .gap = gap,
            .max_width = max_width,
            .semantic_label = "Sidebar",
        },
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void status_bar(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::StatusBar;
    options.semantic_label = chrome_label_or(options.semantic_label,
                                             "Status Bar");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void status_bar(F&& builder,
                MaterialKind kind = MaterialKind::Clear,
                SpaceToken padding = SpaceToken::Sm,
                SpaceToken gap = SpaceToken::Xs) {
    material_surface(
        MaterialSurfaceOptions{
            .kind = kind,
            .role = MaterialSurfaceRole::StatusBar,
            .direction = FlexDirection::Column,
            .padding = padding,
            .gap = gap,
            .semantic_label = "Status Bar",
        },
        std::forward<F>(builder));
}

// overlay — render `builder`'s contents above the main tree, after
// the rest of the UI has been laid out and painted. The overlay is
// the foundation for dialogs, popovers, tooltips, snackbars, and
// dropdowns: HitRegions emitted from inside it land last in the
// command buffer so reverse-iteration hit_test finds them first
// (clicks on the overlay are not stolen by underlying widgets), and
// draw commands paint over the main content.
//
// Overlays are *not* attached to the parent scope — they're root-
// level alternates collected on `g_app.overlays`. The runner lays
// each one out at the canvas width and paints them with no scroll
// applied, so an overlay stays fixed on screen even when the root
// document is scrolled.
//
// Positioning is up to the caller for now: an overlay starts at
// (0, 0) with the canvas width and lays out its children in column
// order. Higher-level wrappers (`layout::dialog`,
// `layout::tooltip`, `layout::snackbar`) for the common positioning
// patterns will land in follow-up PRs once the foundation here is
// exercised.
template<typename F>
    requires std::is_invocable_v<F>
void overlay(F&& builder) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Column;
    detail::g_app.overlays.push_back(h);
    auto* prev = Scope::current();
    Scope scope(h, detail::derived_scope_seed(h));
    Scope::set_current(&scope);
    builder();
    Scope::set_current(prev);
}

// dialog — modal helper built on top of `overlay`. Pushes a top-
// padding spacer, then a row whose `MainAxisAlignment::Center`
// horizontally centres a `max_width`-bounded card hosting the
// builder's content. The card chrome (rounded corners, surface
// background, subtle border, generous inner padding) matches the
// rest of the design system's surface treatment.
//
// Visibility is the caller's call: invoke `layout::dialog(...)`
// when your `State` says the dialog is open, skip it when it's
// closed. The Esc key already drops focus through the existing
// shell handler — wiring "Esc closes the dialog" still belongs in
// the user's `update`, but it's a one-line `Msg::Close` post.
//
// Backdrop / dismiss-on-backdrop-click waits on the animation
// auto-tick and per-overlay click handlers — a follow-up PR will
// layer those features under the same `dialog` name without
// breaking this signature.
template<typename F>
    requires std::is_invocable_v<F>
void dialog(F&& builder,
            float max_width = 360.0f,
            unsigned int top_padding = 96) {
    overlay([&] {
        // Inline spacer — `layout::spacer` is defined later in the
        // file and unqualified lookup at template-parse time can't
        // see it from here.
        {
            auto sp_h = detail::alloc_node();
            detail::node_at(sp_h).style.fixed_height
                = static_cast<float>(top_padding);
            detail::attach_to_scope(sp_h);
        }
        row([&] {
            sized_box(max_width, [&] {
                auto card_h = detail::alloc_node();
                auto& card = detail::node_at(card_h);
                auto const& t = detail::g_app.theme;
                card.style.flex_direction = FlexDirection::Column;
                card.style.gap = t.space_md;
                card.style.padding[0] = t.space_lg;
                card.style.padding[1] = t.space_lg;
                card.style.padding[2] = t.space_lg;
                card.style.padding[3] = t.space_lg;
                card.background = t.surface;
                card.border_color = t.border;
                card.border_width = 1;
                card.border_radius = t.radius_lg;
                detail::open_container(card_h, std::forward<F>(builder));
            });
        }, SpaceToken::Md, CrossAxisAlignment::Start, MainAxisAlignment::Center);
    });
}

// accordion — collapsible section with a clickable header. Persists
// its expanded/collapsed state in `framework_local<bool>` keyed by
// the call site, so neither the user `State` nor a `Msg` round-trip
// is required to remember whether each accordion is open. The click
// handler captures a pointer into the framework_local store and
// flips the boolean directly — the runner's existing trigger_rebuild
// hook is what turns the change into a re-render. Two accordions in
// one view get independent state via the same per-call-site counter
// that disambiguates sibling scroll_views.
//
// Visual chrome: a 1px-bordered surface row for the header (chevron
// glyph + title text), with the body's content rendered below in a
// padded column when expanded. The body is emitted only on expanded
// frames so collapsed accordions cost a single header row.
//
// Snap-only for now — animated height collapse waits on the
// animation auto-tick PR (see `animate_value`'s known limitation).
template<typename F>
    requires std::is_invocable_v<F>
void accordion(str title, F&& builder) {
    auto& expanded = framework_local<bool>(false);
    auto const& t = detail::g_app.theme;

    auto col_h = detail::alloc_node();
    detail::node_at(col_h).style.flex_direction = FlexDirection::Column;
    detail::node_at(col_h).style.gap = t.space_xs;
    detail::attach_to_scope(col_h);

    Scope col_scope(col_h);
    auto* prev = Scope::current();
    Scope::set_current(&col_scope);

    // ---- Header row ----------------------------------------------
    {
        auto id = static_cast<unsigned int>(
            detail::g_app.callbacks.size());
        bool const is_focused = (id == detail::g_app.focused_id);

        auto header_h = detail::alloc_node();
        {
            auto& hd = detail::node_at(header_h);
            hd.style.flex_direction = FlexDirection::Row;
            hd.style.cross_align = CrossAxisAlignment::Center;
            hd.style.gap = t.space_sm;
            hd.style.padding[0] = t.space_sm;
            hd.style.padding[1] = t.space_md;
            hd.style.padding[2] = t.space_sm;
            hd.style.padding[3] = t.space_md;
            hd.cursor_type = 1;
            hd.callback_id = id;
            hd.interaction_role = InteractionRole::Button;
            hd.focusable = true;
            hd.background = t.surface;
            hd.hover_background = t.state_hover_bg;
            // Focus ring: width grows from 1px to
            // `state_focus_ring_width`, colour cross-fades from
            // `t.border` to `state_focus_ring`.
            hd.border_width = animate_float(
                is_focused ? t.state_focus_ring_width : 1.0f, 150);
            hd.border_color = animate_color(
                is_focused ? t.state_focus_ring : t.border, 150);
            hd.border_radius = t.radius_sm;
            hd.debug_semantic_role = "accordion-header";
            hd.debug_semantic_label = std::string(title.data, title.len);
            hd.debug_semantic_callback_id = id;
            hd.debug_semantic_focusable = true;
        }
        detail::attach_to_scope(header_h);

        // Capture the framework_local entry's heap pointer (stable
        // across frames as long as the entry isn't pruned) and flip
        // the value when the click fires. trigger_rebuild then re-
        // runs view, which sees the new value.
        bool* exp_ptr = &expanded;
        detail::g_app.callbacks.push_back([exp_ptr] {
            *exp_ptr = !*exp_ptr;
            detail::trigger_rebuild();
        });
        detail::g_app.callback_roles.push_back(InteractionRole::Button);

        // Chevron glyph — UTF-8 right-pointing triangle when collapsed,
        // down-pointing when expanded. Rotation animation lands with
        // the auto-tick.
        {
            auto chev_h = detail::alloc_node();
            auto& ch = detail::node_at(chev_h);
            // U+25BC ▼ = "\xE2\x96\xBC", U+25B6 ▶ = "\xE2\x96\xB6"
            ch.text = expanded ? "\xE2\x96\xBC" : "\xE2\x96\xB6";
            ch.font_size = t.small_font_size;
            ch.text_color = t.muted;
            ch.focusable = false;
            ch.debug_semantic_hidden = true;
            detail::append_child(header_h, chev_h);
        }

        // Title text
        {
            auto title_h = detail::alloc_node();
            auto& tn = detail::node_at(title_h);
            tn.text = std::string(title.data, title.len);
            tn.font_size = t.body_font_size;
            tn.text_color = t.foreground;
            tn.focusable = false;
            tn.debug_semantic_hidden = true;
            detail::append_child(header_h, title_h);
        }
    }

    // ---- Body (only when expanded) -------------------------------
    if (expanded) {
        auto body_h = detail::alloc_node();
        {
            auto& body = detail::node_at(body_h);
            body.style.flex_direction = FlexDirection::Column;
            body.style.gap = t.space_sm;
            body.style.padding[0] = t.space_sm;
            body.style.padding[1] = t.space_md;
            body.style.padding[2] = t.space_md;
            body.style.padding[3] = t.space_md;
        }
        detail::attach_to_scope(body_h);

        Scope body_scope(body_h);
        Scope::set_current(&body_scope);
        std::forward<F>(builder)();
    }

    Scope::set_current(prev);
}

// scroll_view — fixed-height vertical viewport whose contents scroll
// when their natural total exceeds the viewport. The scroll offset is
// kept in framework_local keyed by this call site, so each view rebuild
// reads back the same offset and persistence is automatic. Wheel
// events with the cursor over the viewport route here (clamped to
// [0, content_height - height]); wheel events outside fall back to
// the root scroll. Padding is applied inside the viewport and counts
// toward the viewport's total height — `fixed_height` measures the
// inner content area, matching the rest of the DSL's `style.fixed_height`
// convention.
//
// Limitations (deferred to follow-up PRs):
//   * vertical only — horizontal wheel/x scroll is not propagated yet.
//   * Nested scroll_views work for hit-test routing but the inner one
//     loses its scissor when an outer scissor is already active
//     (graphics backends don't stack scissor regions).
//   * Keyboard-driven scroll (PageUp/Down, arrow keys) targets only
//     the root document; per-viewport keyboard scroll lands later.
template<typename F>
    requires std::is_invocable_v<F>
void scroll_view(float fixed_height, F&& builder,
                 SpaceToken gap = SpaceToken::Md) {
    // Reading framework_local in the *parent* scope (before
    // open_container pushes a new one) lets sibling scroll_views at
    // the same source_location disambiguate via the parent's
    // per-call-site counter, the same way sibling button/text would.
    auto& state = framework_local<ScrollState>();

    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Column;
    node.style.gap = space_value(gap);
    node.style.fixed_height = fixed_height;
    node.is_scroll_container = true;
    node.scroll_state = &state;
    node.scroll_offset_y = state.offset_y;
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
        case 6: return "delete";
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
    snapshot.scroll_x = g_app.scroll_x;
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

inline bool handle_key_command(unsigned int key,
                               int modifiers,
                               bool focused_is_input,
                               char const* source = "core",
                               char const* detail = "command") {
    for (auto it = g_app.key_commands.rbegin();
         it != g_app.key_commands.rend();
         ++it) {
        if (it->key != key || it->modifiers != modifiers)
            continue;
        if (focused_is_input && !it->allow_when_input_focused)
            return false;
        auto const callback_id = it->callback_id;
        if (callback_id < g_app.callbacks.size()) {
            g_app.callbacks[callback_id]();
            note_input_event(
                "key",
                source,
                detail,
                "handled",
                callback_id);
            return true;
        }
        note_input_event(
            "key",
            source,
            detail,
            "ignored",
            callback_id);
        return false;
    }
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

// Recursively compute max(node.x + node.width) across the subtree, in
// the layout coordinate space rooted at `origin_x`. Used by get_total_width
// so a layout::grid (or anything else) wider than its parent's available
// width still reports the correct horizontal extent for scroll-x clamping.
//
// When a descendant overflows its container's own right edge, the
// container's right padding is added back so that scrolling fully right
// still leaves the same breathing room the user sees on the left.
inline float subtree_right_extent(NodeHandle h, float origin_x) {
    auto* np = g_app.arena.get(h);
    if (!np) return origin_x;
    float self_right = origin_x + np->x + np->width;
    float max_right = self_right;
    for (auto child_h : np->children) {
        float child_right = subtree_right_extent(child_h, origin_x + np->x);
        if (child_right > max_right) max_right = child_right;
    }
    if (max_right > self_right && np->style.padding[1] > 0) {
        max_right += np->style.padding[1];
    }
    return max_right;
}

inline float get_total_width() {
    auto* root_ptr = g_app.arena.get(g_app.root);
    if (!root_ptr) return 0;
    float root_right = root_ptr->width;
    float subtree_right = subtree_right_extent(g_app.root, 0.0f);
    return (subtree_right > root_right) ? subtree_right : root_right;
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
        case 6: {
            if (caret >= value.size())
                return false;
            auto end = next_utf8_boundary(value, caret);
            value.erase(caret, end - caret);
            text_changed = true;
            return true;
        }
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
    snapshot.scroll_x = g_app.scroll_x;
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
    } else if ((key_type == 1 || key_type == 6) && selection.active) {
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
void repaint(Host& host, float scroll_x, float scroll_y) {
    auto& app = g_app;
    app.scroll_x = scroll_x;
    app.scroll_y = scroll_y;
    if (!app.root.valid()) return;
    auto* root_ptr = app.arena.get(app.root);
    if (!root_ptr) return;
    float cw = host.canvas_width();
    if (cw != root_ptr->width) {
        layout_node(host, app.root, cw);
        for (auto overlay_h : app.overlays)
            layout_node(host, overlay_h, cw);
    }
    app.focusable_ids.clear();
    collect_focusable_ids(app.root);
    for (auto overlay_h : app.overlays)
        collect_focusable_ids(overlay_h);
    app.paint_scissor_depth = 0;
    emit_clear(host, app.theme.background);
    float vh = host.canvas_height();
    app.debug_viewport_width = cw;
    app.debug_viewport_height = vh;
    paint_node(host, host, app.root, 0, 0, scroll_x, scroll_y, cw, vh);
    reset_paint_scissor_boundary(host);
    for (auto overlay_h : app.overlays)
        paint_node(host, host, overlay_h, 0, 0, 0.0f, 0.0f, cw, vh);
    flush_if_changed(host);
    app.prev_scroll_x   = app.scroll_x;
    app.prev_scroll_y   = app.scroll_y;
    app.prev_hovered_id = app.hovered_id;
    app.prev_focused_id = app.focused_id;
}
#else
inline void repaint(float scroll_x, float scroll_y) {
    auto& app = g_app;
    app.scroll_x = scroll_x;
    app.scroll_y = scroll_y;
    if (!app.root.valid()) return;
    auto* root_ptr = app.arena.get(app.root);
    if (!root_ptr) return;
    float cw = phenotype_get_canvas_width();
    if (cw != root_ptr->width) {
        layout_node(app.root, cw);
        for (auto overlay_h : app.overlays)
            layout_node(overlay_h, cw);
    }
    app.focusable_ids.clear();
    collect_focusable_ids(app.root);
    for (auto overlay_h : app.overlays)
        collect_focusable_ids(overlay_h);
    app.paint_scissor_depth = 0;
    wasi_emit_clear(app.theme.background);
    float vh = phenotype_get_canvas_height();
    app.debug_viewport_width = cw;
    app.debug_viewport_height = vh;
    wasi_paint_node(app.root, 0, 0, scroll_x, scroll_y, cw, vh);
    wasi_reset_paint_scissor_boundary();
    for (auto overlay_h : app.overlays)
        wasi_paint_node(overlay_h, 0, 0, 0.0f, 0.0f, cw, vh);
    wasi_flush_if_changed();
    app.prev_scroll_x   = app.scroll_x;
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

inline bool rect_intersects_viewport(diag::RectSnapshot const& rect,
                                     bool screen_fixed = false) {
    if (!rect.valid)
        return false;
    auto viewport_width = g_app.debug_viewport_width;
    auto viewport_height = g_app.debug_viewport_height;
    if (viewport_width <= 0.0f || viewport_height <= 0.0f)
        return false;
    float viewport_left  = screen_fixed ? 0.0f : g_app.scroll_x;
    float viewport_right = viewport_left + viewport_width;
    if (rect.x + rect.w <= viewport_left || rect.x >= viewport_right)
        return false;
    float viewport_top = screen_fixed ? 0.0f : g_app.scroll_y;
    float viewport_bottom = viewport_top + viewport_height;
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
                                   std::vector<diag::SemanticNodeSnapshot>& out,
                                   bool screen_fixed = false) {
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
    bool const has_interaction_role =
        node.interaction_role != InteractionRole::None;
    bool const has_material = node.material.kind != MaterialKind::None;
    bool auto_semantics = is_root
        || has_material
        || node.is_input
        || has_interaction_role
        || !node.image_url.empty()
        || !node.text.empty();

    if (!explicit_semantics && !auto_semantics) {
        for (auto child_h : node.children)
            collect_semantic_nodes(child_h, ax, ay, out, screen_fixed);
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
    } else if (has_interaction_role) {
        semantic.role = interaction_role_name(node.interaction_role);
    } else if (!node.image_url.empty()) {
        semantic.role = "image";
    } else if (has_material) {
        semantic.role = "material";
    } else {
        semantic.role = "text";
    }

    if (has_material) {
        semantic.material = diag::SemanticNodeSnapshot::MaterialSnapshot{
            .kind = material_kind_name(node.material.kind),
            .role = material_surface_role_name(node.material.role),
            .opacity = node.material.opacity,
            .blur_radius = node.material.blur_radius,
            .tint = node.material.tint,
            .saturation = node.material.saturation,
            .luminance_floor = node.material.luminance_floor,
            .luminance_gain = node.material.luminance_gain,
            .edge_highlight = node.material.edge_highlight,
            .edge_width = node.material.edge_width,
            .noise_opacity = node.material.noise_opacity,
            .shadow_alpha = node.material.shadow_alpha,
            .shadow_radius = node.material.shadow_radius,
            .container = node.material.container,
            .fallback = node.material.fallback,
            .fallback_reason = node.material.fallback_reason,
            .contrast_intent = node.material.contrast_intent,
            .plan_id = node.material.plan_id,
            .verifier_profile = node.material.verifier_profile,
        };
    }

    if (!node.debug_semantic_label.empty()) {
        semantic.label = node.debug_semantic_label;
    } else if (!node.text.empty()) {
        semantic.label = node.text;
    }

    semantic.bounds = node_bounds_snapshot(ax, ay, node.width, node.height);
    semantic.visible = is_root || rect_intersects_viewport(semantic.bounds, screen_fixed);
    semantic.enabled = node.debug_semantic_enabled;
    semantic.focusable = explicit_semantics
        ? (node.debug_semantic_focusable && semantic.enabled)
        : (semantic_callback_id != 0xFFFFFFFFu && node.focusable);
    semantic.focused = semantic_callback_id != 0xFFFFFFFFu
        && semantic_callback_id == g_app.focused_id;
    semantic.scroll_container = is_root;

    for (auto child_h : node.children)
        collect_semantic_nodes(child_h, ax, ay, semantic.children, screen_fixed);
    out.push_back(std::move(semantic));
}

inline void append_overlay_semantic_nodes(diag::SemanticNodeSnapshot& root) {
    for (auto overlay_h : g_app.overlays) {
        std::vector<diag::SemanticNodeSnapshot> overlay_nodes;
        collect_semantic_nodes(overlay_h, 0.0f, 0.0f, overlay_nodes, true);
        for (auto& overlay_node : overlay_nodes) {
            if (overlay_node.role == "root" && overlay_node.scroll_container) {
                for (auto& child : overlay_node.children)
                    root.children.push_back(std::move(child));
            } else {
                root.children.push_back(std::move(overlay_node));
            }
        }
    }
}

inline std::optional<diag::SemanticNodeSnapshot> build_semantic_tree_snapshot() {
    std::vector<diag::SemanticNodeSnapshot> nodes;
    collect_semantic_nodes(g_app.root, 0.0f, 0.0f, nodes);
    if (nodes.empty())
        return std::nullopt;
    auto root = std::move(nodes.front());
    append_overlay_semantic_nodes(root);
    return root;
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
    runtime.scroll_x = g_app.scroll_x;
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
    auto debug = diag::debug_plane_snapshot_to_json(build_debug_plane_snapshot(
        std::move(platform_override),
        std::move(runtime_details_override)));
    if (auto provider = diag::detail::current_application_debug_provider()) {
        debug.as_object().emplace("application", provider());
    }
    return debug;
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

inline float get_scroll_x() {
    return g_app.scroll_x;
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

inline void set_scroll_x(float scroll_x) {
    g_app.scroll_x = scroll_x;
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
        phenotype::detail::repaint(phenotype::detail::g_app.scroll_x, scroll_y);
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

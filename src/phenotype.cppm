module;
#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstring>
#include <functional>
#include <limits>
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
export import phenotype.theme_contract;
export import phenotype.types;
export import phenotype.resources;
export import phenotype.icon_catalog;
export import phenotype.svg;
export import phenotype.icons;
export import phenotype.io;
export import phenotype.material;
export import phenotype.state;
export import phenotype.layout;
export import phenotype.widget.glass;
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

inline bool focus_ring_visible(unsigned int callback_id) {
    return callback_id != 0xFFFFFFFFu
        && callback_id == g_app.focused_id
        && g_app.focus_visible;
}

inline char const* input_modality_name(InputModality modality) {
    switch (modality) {
        case InputModality::Keyboard: return "keyboard";
        case InputModality::Pointer: return "pointer";
        case InputModality::Programmatic: return "programmatic";
        case InputModality::None:
        default: return "none";
    }
}

inline char const* focus_visibility_reason_name(
        FocusVisibilityReason reason) {
    switch (reason) {
        case FocusVisibilityReason::KeyboardFocusNavigation:
            return "keyboard_focus_navigation";
        case FocusVisibilityReason::PointerInputHidesFocusRing:
            return "pointer_input_hides_focus_ring";
        case FocusVisibilityReason::ProgrammaticFocusHidden:
            return "programmatic_focus_hidden";
        case FocusVisibilityReason::NoFocus:
        default:
            return "no_focus";
    }
}

inline icons::SymbolDocumentCache const& icon_document_cache() {
    static auto const cache = icons::make_symbol_document_cache();
    return cache;
}

inline bool focus_ring_hidden_by_pointer_modality(unsigned int callback_id) {
    return callback_id != 0xFFFFFFFFu
        && !g_app.focus_visible
        && g_app.prev_focus_visible
        && (callback_id == g_app.focused_id
            || callback_id == g_app.prev_focused_id);
}

inline int focus_ring_transition_ms(unsigned int callback_id,
                                    int normal_duration_ms) {
    return focus_ring_hidden_by_pointer_modality(callback_id)
        ? 0
        : normal_duration_ms;
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
    bool const is_focused = detail::focus_ring_visible(id);
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
    int const focus_ms = detail::focus_ring_transition_ms(id, 150);
    node.border_width = animate_float(
        is_focused ? t.state_focus_ring_width : 0.0f, focus_ms);
    node.border_color = animate_color(
        is_focused ? t.state_focus_ring : ring_off, focus_ms);

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
inline void apply_button_material(LayoutNode& node,
                                  ButtonStyleOptions const& options) {
    if (!options.has_material || options.material.kind == MaterialKind::None)
        return;
    node.material_shape = options.shape;
    node.material = options.material;
    node.material.tint = node.background;
    node.material.border = node.border_color;
    node.material.foreground = node.text_color;
}

inline void apply_text_field_material(LayoutNode& node,
                                      TextFieldStyleOptions const& options) {
    if (!options.has_material || options.material.kind == MaterialKind::None)
        return;
    node.material_shape = options.shape;
    node.material = options.material;
    node.material.tint = node.background;
    node.material.border = node.border_color;
    node.material.foreground = node.text_color;
}

inline void apply_material_interaction_state(LayoutNode& node,
                                             bool hovered,
                                             bool pressed,
                                             bool focused) {
    if (node.material.kind == MaterialKind::None)
        return;
    node.material.interaction = MaterialInteractionDescriptor{
        .hovered = hovered,
        .pressed = pressed,
        .focused = focused,
        .pointer_inside = hovered || pressed,
        .pointer_x = 0.5f,
        .pointer_y = 0.5f,
    };
}

namespace _impl {

inline void action_button(str label,
                          ButtonStyleOptions options,
                          std::function<void()> callback) {
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
    node.min_hit_width = options.min_hit_width;
    node.min_hit_height = options.min_hit_height;

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
        apply_button_material(node, options);
        node.cursor_type = 0;
        node.focusable = false;
        node.debug_semantic_enabled = false;
        // No callback — the button is non-interactive. Skip the
        // callback registration entirely so click dispatch finds
        // node.callback_id == 0xFFFFFFFF and falls through.
        detail::attach_to_scope(h);
        return;
    }

    // Predict the callback id we're about to push so the hover/focus/press
    // samples mirror paint's own checks (phenotype.paint.cppm:632-636)
    // at view time.
    auto const id = static_cast<unsigned int>(
        detail::g_app.callbacks.size());
    bool const is_hovered = (id == detail::g_app.hovered_id);
    bool const is_focused =
        options.focus_ring && detail::focus_ring_visible(id);
    bool const is_pressed = (id == detail::g_app.pressed_id);

    Color base_bg, hover_bg, pressed_bg, base_border;
    switch (options.variant) {
        case ButtonVariant::Primary:
            base_bg = t.accent;
            hover_bg = t.accent_strong;
            pressed_bg = t.accent_strong;
            base_border = t.accent;
            node.text_color = t.state_active_fg;
            break;
        case ButtonVariant::Default:
        default:
            base_bg = t.surface;
            hover_bg = t.state_hover_bg;
            pressed_bg = t.state_hover_bg;
            base_border = t.border;
            node.text_color = t.foreground;
            break;
    }
    if (options.has_background)
        base_bg = options.background;
    if (options.has_hover_background)
        hover_bg = options.hover_background;
    if (options.has_pressed_background)
        pressed_bg = options.pressed_background;
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
    auto const target_bg = is_pressed ? pressed_bg
        : (is_hovered ? hover_bg : base_bg);
    node.background = animate_color(target_bg, 150);
    // View-time focus ring: width grows from 1px to
    // `state_focus_ring_width`, colour cross-fades from the variant's
    // resting border to `state_focus_ring`. Paint reads both fields
    // directly.
    int const focus_ms = detail::focus_ring_transition_ms(id, 150);
    node.border_width = animate_float(
        is_focused ? t.state_focus_ring_width : base_border_width, focus_ms);
    node.border_color = animate_color(
        is_focused ? t.state_focus_ring : base_border, focus_ms);
    apply_button_material(node, options);
    apply_material_interaction_state(
        node,
        is_hovered,
        is_pressed,
        is_focused);
    node.cursor_type = 1;

    detail::g_app.callbacks.push_back(std::move(callback));
    detail::g_app.callback_roles.push_back(InteractionRole::Button);
    node.callback_id = id;

    detail::attach_to_scope(h);
}

} // namespace _impl

template<typename Msg>
inline void button(str label, Msg msg, ButtonStyleOptions options) {
    _impl::action_button(label, options, [msg = std::move(msg)] {
        detail::post<Msg>(msg);
        detail::trigger_rebuild();
    });
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

template<typename Msg>
inline void glass_button(str label,
                         Msg msg,
                         GlassControlStyleOptions options = {}) {
    button(label, std::move(msg), glass_control_button_style(options));
}

template<typename Msg>
inline void glass_prominent_button(str label,
                                   Msg msg,
                                   GlassControlStyleOptions options = {}) {
    button(label, std::move(msg), glass_prominent_button_style(options));
}

namespace _impl {
inline MaterialStyle glass_control_indicator_material(
        MaterialKind kind,
        MaterialSurfaceRole role,
        bool active,
        bool hovered,
        bool focused,
        bool pressed,
        Color active_tint,
        Color idle_tint,
        Color active_border,
        Color idle_border) {
    auto const& t = ::phenotype::detail::g_app.theme;
    auto material = material_style_for_kind(kind, t);
    material.role = role;
    material.fallback = kind != MaterialKind::None;
    material.tint = active ? active_tint : idle_tint;
    material.border = active ? active_border : idle_border;
    material.foreground = active ? t.state_active_fg : t.foreground;
    material.accent_foreground = t.accent;
    material.strong_accent_foreground = t.accent_strong;
    material.interaction = MaterialInteractionDescriptor{
        .hovered = hovered,
        .pressed = pressed,
        .focused = focused,
        .pointer_inside = hovered,
        .pointer_x = 0.5f,
        .pointer_y = 0.5f,
    };
    return material;
}

inline void apply_glass_toggle_effect_context(
        MaterialStyle& material,
        GlassToggleStyleOptions const& options) {
    if (!options.has_effect_context || material.kind == MaterialKind::None)
        return;

    material.transition = options.transition;
    material.glass_identity = options.glass_identity;
    if (options.container.participates()) {
        auto const keep_interaction =
            material.container.interactive || options.container.interactive;
        material.container = options.container;
        if (keep_interaction)
            material.container.interactive = true;
    }
}

inline void apply_glass_switch_effect_context(
        MaterialStyle& material,
        GlassSwitchStyleOptions const& options) {
    if (!options.has_effect_context || material.kind == MaterialKind::None)
        return;

    material.transition = options.transition;
    material.glass_identity = options.glass_identity;
    if (options.container.participates()) {
        auto const keep_interaction =
            material.container.interactive || options.container.interactive;
        material.container = options.container;
        if (keep_interaction)
            material.container.interactive = true;
    }
}

inline MaterialGlassIdentityDescriptor glass_tabs_indicator_identity(
        TabsStyleOptions const& options) noexcept {
    return options.indicator_glass_identity.participates()
        ? options.indicator_glass_identity
        : options.glass_identity;
}

inline void apply_glass_tabs_effect_context(
        MaterialStyle& material,
        TabsStyleOptions const& options,
        MaterialGlassIdentityDescriptor identity) {
    if (!options.has_effect_context || material.kind == MaterialKind::None)
        return;

    material.transition = options.transition;
    material.glass_identity = identity;
    if (options.container.participates()) {
        auto const keep_interaction =
            material.container.interactive
            || options.container.interactive
            || options.interactive;
        material.container = options.container;
        if (keep_interaction)
            material.container.interactive = true;
    }
}

template<typename Msg>
inline void toggle(str label, bool active, Msg msg,
                   float corner_radius, Decoration active_decoration,
                   InteractionRole role,
                   GlassToggleStyleOptions const* glass_options = nullptr) {
    auto const& t = ::phenotype::detail::g_app.theme;
    auto id = static_cast<unsigned int>(
        ::phenotype::detail::g_app.callbacks.size());
    bool const is_hovered = (id == ::phenotype::detail::g_app.hovered_id);
    bool const is_focused = ::phenotype::detail::focus_ring_visible(id);
    bool const is_pressed = (id == ::phenotype::detail::g_app.pressed_id);
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
        int const focus_ms =
            ::phenotype::detail::focus_ring_transition_ms(id, 150);
        row.border_width = ::phenotype::animate_float(
            is_focused ? t.state_focus_ring_width : 0.0f, focus_ms);
        row.border_color = ::phenotype::animate_color(
            is_focused ? t.state_focus_ring : ring_off, focus_ms);
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
        if (glass_options && glass_options->kind != MaterialKind::None) {
            auto const active_blue =
                lerp_color(t.accent_strong, Color{0, 0, 0, 255}, 0.16f);
            auto active_tint = material_with_alpha(
                active_blue,
                static_cast<unsigned char>(
                    is_pressed ? 232 : (is_hovered ? 214 : 196)));
            auto idle_tint = material_with_alpha(
                t.surface,
                static_cast<unsigned char>(
                    is_pressed ? 190 : (is_hovered ? 166 : 136)));
            auto material = glass_control_indicator_material(
                glass_options->kind,
                glass_options->role,
                active,
                is_hovered,
                is_focused,
                is_pressed,
                active_tint,
                idle_tint,
                material_with_alpha(t.accent_strong, 210),
                material_with_alpha(t.border, 140));
            material.container = MaterialContainerDescriptor{
                0u,
                0u,
                8.0f,
                true,
                true};
            if (glass_options->has_tint)
                material.tint = glass_options->tint;
            if (glass_options->has_border)
                material.border = glass_options->border;
            apply_glass_toggle_effect_context(material, *glass_options);
            box.material = material;
            box.background = material.tint;
            box.border_color = material.border;
            box.debug_semantic_hidden = false;
            box.debug_semantic_role = "material";
            box.debug_semantic_label =
                std::string(label.data, label.len) + " Indicator";
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
inline void glass_checkbox(str label, bool checked, Msg msg,
                           GlassToggleStyleOptions options = {}) {
    _impl::toggle(label, checked, std::move(msg),
                  detail::g_app.theme.radius_xs, Decoration::Check,
                  InteractionRole::Checkbox, &options);
}

template<typename Msg>
inline void radio(str label, bool selected, Msg msg) {
    _impl::toggle(label, selected, std::move(msg),
                  detail::g_app.theme.radius_lg, Decoration::Dot,
                  InteractionRole::Radio);
}

template<typename Msg>
inline void glass_radio(str label, bool selected, Msg msg,
                        GlassToggleStyleOptions options = {}) {
    _impl::toggle(label, selected, std::move(msg),
                  detail::g_app.theme.radius_lg, Decoration::Dot,
                  InteractionRole::Radio, &options);
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
                       TextFieldStyleOptions options) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    node.interaction_role = InteractionRole::TextField;
    node.placeholder = std::string(hint.data, hint.len);
    node.text = current.empty() ? node.placeholder : current;
    node.debug_semantic_label =
        options.semantic_label ? options.semantic_label : node.placeholder;
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

    if (options.disabled) {
        node.is_input = false;
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
        apply_text_field_material(node, options);
        node.cursor_type = 0;
        node.focusable = false;
        node.debug_semantic_enabled = false;
        // No callback / input handler — the field is non-interactive.
        detail::attach_to_scope(h);
        return;
    }

    node.is_input = true;
    Color resting_border;
    if (options.error) {
        node.background = t.state_error_bg;
        node.text_color = current.empty() ? t.muted : t.state_error_fg;
        resting_border = t.state_error_border;
    } else {
        node.background = options.has_background ? options.background : t.surface;
        node.text_color = current.empty() ? t.muted : t.foreground;
        resting_border = options.has_border_color ? options.border_color : t.border;
    }
    if (options.has_text_color)
        node.text_color = options.text_color;
    node.cursor_type = 1;

    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    bool const is_hovered = id == detail::g_app.hovered_id;
    bool const is_focused = detail::focus_ring_visible(id);
    bool const is_pressed = id == detail::g_app.pressed_id;
    float const base_border_width = options.border_width >= 0.0f
        ? options.border_width
        : 1.0f;
    // View-time focus ring: width grows from 1px to
    // `state_focus_ring_width`, colour cross-fades from the resting
    // border (or `state_error_border`) to `state_focus_ring`. Pointer
    // focus still moves the caret, but only keyboard focus paints the ring.
    int const focus_ms = detail::focus_ring_transition_ms(id, 150);
    node.border_width = animate_float(
        is_focused ? t.state_focus_ring_width : base_border_width, focus_ms);
    node.border_color = animate_color(
        is_focused ? t.state_focus_ring : resting_border, focus_ms);
    apply_text_field_material(node, options);
    apply_material_interaction_state(
        node,
        is_hovered,
        is_pressed,
        is_focused);
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

template<typename Msg>
inline void text_field(str hint, std::string const& current,
                       Msg(*mapper)(std::string),
                       bool error = false, bool disabled = false) {
    TextFieldStyleOptions options;
    options.error = error;
    options.disabled = disabled;
    text_field<Msg>(hint, current, mapper, options);
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

// Canvas rendering with an explicit semantic node. Use this when custom
// painting must stay visually bounded while artifacts still need a readable
// label for verifier/debug workflows.
inline void semantic_canvas(float width, float height,
                            str semantic_label,
                            std::function<void(Painter&)> paint_fn,
                            std::function<void(GestureEvent const&)> on_gesture = {},
                            std::uint64_t paint_token = 0,
                            str semantic_role = str{"text"}) {
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.max_width = width;
    node.style.fixed_height = height;
    node.debug_semantic_role =
        std::string{semantic_role.data, semantic_role.len};
    node.debug_semantic_label =
        std::string{semantic_label.data, semantic_label.len};
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
                      SvgImageOptions options) {
    auto const current_color = options.has_current_color
        ? options.current_color
        : detail::g_app.theme.foreground;
    auto const paint_token =
        svg::paint_token(
            document,
            width,
            height,
            current_color,
            options.preserve_aspect_ratio);
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.max_width = width;
    node.style.fixed_height = height;
    node.debug_semantic_role = "image";
    if (!options.semantic_label.empty())
        node.debug_semantic_label = std::string{options.semantic_label};
    node.paint_fn = [
        document = std::move(document),
        width,
        height,
        current_color,
        preserve_aspect_ratio = options.preserve_aspect_ratio](
            Painter& painter) {
        svg::paint(painter, document, 0.0f, 0.0f, width, height,
                   svg::RenderOptions{current_color, preserve_aspect_ratio});
    };
    node.paint_token = paint_token;
    detail::attach_to_scope(h);
}

inline void svg_image(svg::Document document,
                      float width,
                      float height,
                      Color current_color) {
    svg_image(
        std::move(document),
        width,
        height,
        SvgImageOptions{
            .has_current_color = true,
            .current_color = current_color,
        });
}

inline void svg_image(svg::Document document,
                      float width,
                      float height) {
    svg_image(std::move(document), width, height, SvgImageOptions{});
}

inline void svg_image(str source,
                      float width,
                      float height,
                      SvgImageOptions options) {
    svg_image(svg::parse(std::string_view{source.data, source.len}),
              width, height, options);
}

inline void svg_image(str source,
                      float width,
                      float height,
                      Color current_color) {
    svg_image(
        source,
        width,
        height,
        SvgImageOptions{
            .has_current_color = true,
            .current_color = current_color,
        });
}

inline void svg_image(str source,
                      float width,
                      float height) {
    svg_image(source, width, height, SvgImageOptions{});
}

inline void icon(icons::Symbol symbol,
                 float size,
                 Color color) {
    canvas(size, size,
           [symbol, size, color](Painter& painter) {
               icons::paint_symbol(
                   painter,
                   detail::icon_document_cache(),
                   symbol,
                   0.0f,
                   0.0f,
                   size,
                   color);
           },
           {},
           icons::paint_token(symbol, size, color));
}

inline void icon(icons::SymbolPresentation presentation) {
    canvas(presentation.point_size, presentation.point_size,
           [presentation](Painter& painter) {
               icons::paint_symbol(
                   painter,
                   detail::icon_document_cache(),
                   presentation,
                   0.0f,
                   0.0f);
           },
           {},
           icons::paint_token(presentation));
}

inline void icon(icons::Symbol symbol, float size) {
    icon(symbol, size, detail::g_app.theme.foreground);
}

inline std::uint64_t button_visual_state_token(
        std::uint64_t paint_token,
        ButtonVisualState state) noexcept {
    if (paint_token == 0)
        return 0;
    std::uint64_t token = paint_token;
    if (state.hovered)
        token ^= 0x9E3779B97F4A7C15ull;
    if (state.focused)
        token ^= 0xC2B2AE3D27D4EB4Full;
    if (state.pressed)
        token ^= 0x165667B19E3779F9ull;
    if (!state.enabled)
        token ^= 0x85EBCA77C2B2AE63ull;
    return token;
}

template<typename Msg, typename PaintFn>
inline void canvas_button(str label,
                          float width,
                          float height,
                          PaintFn paint_fn,
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
    node.min_hit_width = options.min_hit_width;
    node.min_hit_height = options.min_hit_height;
    node.debug_semantic_label = std::string(label.data, label.len);
    ButtonVisualState visual_state{
        .hovered = false,
        .focused = false,
        .pressed = false,
        .enabled = !options.disabled,
    };

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
        apply_button_material(node, options);
        node.cursor_type = 0;
        node.focusable = false;
        node.debug_semantic_enabled = false;
        detail::attach_to_scope(h);
    } else {
        auto const id = static_cast<unsigned int>(
            detail::g_app.callbacks.size());
        bool const is_hovered = (id == detail::g_app.hovered_id);
        bool const is_focused =
            options.focus_ring && detail::focus_ring_visible(id);
        bool const is_pressed = (id == detail::g_app.pressed_id);
        visual_state = ButtonVisualState{
            .hovered = is_hovered,
            .focused = is_focused,
            .pressed = is_pressed,
            .enabled = true,
        };

        Color base_bg, hover_bg, pressed_bg, base_border;
        switch (options.variant) {
            case ButtonVariant::Primary:
                base_bg = t.accent;
                hover_bg = t.accent_strong;
                pressed_bg = t.accent_strong;
                base_border = t.accent;
                node.text_color = t.state_active_fg;
                break;
            case ButtonVariant::Default:
            default:
                base_bg = t.surface;
                hover_bg = t.state_hover_bg;
                pressed_bg = t.state_hover_bg;
                base_border = t.border;
                node.text_color = t.foreground;
                break;
        }
        if (options.has_background)
            base_bg = options.background;
        if (options.has_hover_background)
            hover_bg = options.hover_background;
        if (options.has_pressed_background)
            pressed_bg = options.pressed_background;
        if (options.has_border_color)
            base_border = options.border_color;
        if (options.has_text_color)
            node.text_color = options.text_color;
        float const base_border_width = options.border_width >= 0.0f
            ? options.border_width
            : 1.0f;

        auto const target_bg = is_pressed ? pressed_bg
            : (is_hovered ? hover_bg : base_bg);
        node.background = animate_color(target_bg, 150);
        int const focus_ms = detail::focus_ring_transition_ms(id, 150);
        node.border_width = animate_float(
            is_focused ? t.state_focus_ring_width : base_border_width, focus_ms);
        node.border_color = animate_color(
            is_focused ? t.state_focus_ring : base_border, focus_ms);
        apply_button_material(node, options);
        apply_material_interaction_state(
            node,
            is_hovered,
            is_pressed,
            is_focused);
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
    canvas_node.paint_fn = [paint_fn = std::move(paint_fn), visual_state](
            Painter& painter) mutable {
        if constexpr (std::invocable<PaintFn&, Painter&, ButtonVisualState>) {
            paint_fn(painter, visual_state);
        } else {
            paint_fn(painter);
        }
    };
    canvas_node.paint_token =
        button_visual_state_token(paint_token, visual_state);
    canvas_node.debug_semantic_hidden = true;
    detail::append_child(h, canvas_h);
}

template<typename Msg>
inline void symbol_button(str label,
                          icons::Symbol symbol,
                          Msg msg,
                          icons::SymbolButtonOptions options,
                          ButtonStyleOptions style) {
    auto const width = icons::symbol_button_width(options);
    auto const height = icons::symbol_button_height(options);
    canvas_button<Msg>(
        label,
        width,
        height,
        [symbol, options, width, height](
                Painter& painter,
                ButtonVisualState state) {
            auto const presentation = icons::macos_presentation(
                symbol,
                options.role,
                options.selected,
                state);
            icons::paint_symbol_centered(
                painter,
                detail::icon_document_cache(),
                presentation,
                0.0f,
                0.0f,
                width,
                height);
        },
        std::move(msg),
        style,
        icons::symbol_button_paint_token(symbol, options));
}

template<typename Msg>
inline void symbol_button(str label,
                          icons::Symbol symbol,
                          Msg msg,
                          icons::SymbolButtonOptions options = {}) {
    symbol_button<Msg>(
        label,
        symbol,
        std::move(msg),
        options,
        icons::macos_symbol_button_style(options));
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
namespace _impl {
template<typename Msg>
inline void switch_control(str label, bool on, Msg msg,
                           GlassSwitchStyleOptions const* glass_options = nullptr) {
    auto const& t = detail::g_app.theme;
    auto id = static_cast<unsigned int>(
        detail::g_app.callbacks.size());
    bool const is_hovered = (id == detail::g_app.hovered_id);
    bool const is_focused = detail::focus_ring_visible(id);
    bool const is_pressed = (id == detail::g_app.pressed_id);
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
        int const focus_ms = detail::focus_ring_transition_ms(id, 150);
        row.border_width = animate_float(
            is_focused ? t.state_focus_ring_width : 0.0f, focus_ms);
        row.border_color = animate_color(
            is_focused ? t.state_focus_ring : ring_off, focus_ms);
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
        if (glass_options && glass_options->kind != MaterialKind::None) {
            auto const active_blue =
                lerp_color(t.accent_strong, Color{0, 0, 0, 255}, 0.16f);
            auto active_tint = material_with_alpha(
                active_blue,
                static_cast<unsigned char>(
                    is_pressed ? 236 : (is_hovered ? 218 : 198)));
            auto idle_tint = material_with_alpha(
                t.surface,
                static_cast<unsigned char>(
                    is_pressed ? 192 : (is_hovered ? 166 : 136)));
            auto animated_tint = animate_color(
                on ? active_tint : idle_tint,
                150);
            auto material = glass_control_indicator_material(
                glass_options->kind,
                glass_options->role,
                on,
                is_hovered,
                is_focused,
                is_pressed,
                animated_tint,
                animated_tint,
                material_with_alpha(t.accent_strong, 210),
                material_with_alpha(t.border, 150));
            material.container = MaterialContainerDescriptor{
                0u,
                0u,
                8.0f,
                true,
                true};
            if (glass_options->has_tint)
                material.tint = glass_options->tint;
            if (glass_options->has_border)
                material.border = glass_options->border;
            apply_glass_switch_effect_context(material, *glass_options);
            tr.material = material;
            tr.background = material.tint;
            tr.border_color = material.border;
            tr.border_width = 0.5f;
            tr.debug_semantic_hidden = false;
            tr.debug_semantic_role = "material";
            tr.debug_semantic_label =
                std::string(label.data, label.len) + " Track";
        }
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
} // namespace _impl

template<typename Msg>
inline void switch_(str label, bool on, Msg msg) {
    _impl::switch_control(label, on, std::move(msg));
}

template<typename Msg>
inline void glass_switch(str label, bool on, Msg msg,
                         GlassSwitchStyleOptions options = {}) {
    _impl::switch_control(label, on, std::move(msg), &options);
}

// tabs — segmented row of buttons that posts `on_select(i)` when the
// user picks tab `i`. The currently-selected index is supplied by the
// caller (typically lifted into user `State`), so the widget itself
// stays stateless — `Msg`/`update` round-trip is the source of truth.
//
// Visual chrome is the design system's material segmented-control
// treatment: the outer pill emits a `MaterialRect` using
// `TabsStyleOptions`, so the same pure Liquid Glass plan powers
// text tabs, mobile mode pickers, and native examples. The pill is a
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
                 Msg (*on_select)(std::size_t),
                 TabsStyleOptions options = {}) {
    auto const& t = detail::g_app.theme;

    auto pill_h = detail::alloc_node();
    {
        auto& pill = detail::node_at(pill_h);
        auto material = material_style_for_kind(options.kind, t);
        material.role = options.role;
        material.fallback = options.kind != MaterialKind::None;
        material.container = MaterialContainerDescriptor{
            0u,
            0u,
            20.0f,
            options.interactive,
            true};
        if (options.has_tint && options.kind != MaterialKind::None)
            material.tint = options.tint;
        if (options.has_border && options.kind != MaterialKind::None)
            material.border = options.border;
        _impl::apply_glass_tabs_effect_context(
            material,
            options,
            options.glass_identity);
        pill.material = material;
        pill.style.flex_direction = FlexDirection::Column;
        pill.style.gap = 0;
        pill.background = options.kind == MaterialKind::None
            ? t.code_bg
            : material.tint;
        pill.border_color = options.kind == MaterialKind::None
            ? t.transparent
            : material.border;
        pill.border_width = options.kind == MaterialKind::None
            ? 0.0f
            : options.border_width;
        pill.border_radius = options.border_radius >= 0.0f
            ? options.border_radius
            : t.radius_md;
        pill.material_shape = options.shape;
        pill.style.padding[0] = t.space_xs;
        pill.style.padding[1] = t.space_xs;
        pill.style.padding[2] = t.space_xs;
        pill.style.padding[3] = t.space_xs;
        if (options.semantic_label && options.semantic_label[0] != '\0')
            pill.debug_semantic_label = options.semantic_label;
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
        bool const is_hovered = id == detail::g_app.hovered_id;
        bool const is_focused = detail::focus_ring_visible(id);
        bool const is_pressed = id == detail::g_app.pressed_id;

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
            int const focus_ms = detail::focus_ring_transition_ms(id, 150);
            btn.border_width = animate_float(
                is_focused ? t.state_focus_ring_width : 0.0f, focus_ms);
            btn.border_color = animate_color(
                is_focused ? t.state_focus_ring : ring_off, focus_ms);
            if (is_selected && options.kind != MaterialKind::None) {
                auto selected_material =
                    material_style_for_kind(MaterialKind::Clear, t);
                selected_material.role = options.role;
                selected_material.fallback = true;
                selected_material.tint = material_with_alpha(t.surface, 186);
                selected_material.border = options.has_border
                    ? options.border
                    : material_with_alpha(t.border, 112);
                selected_material.foreground = t.accent;
                selected_material.accent_foreground = t.accent;
                selected_material.strong_accent_foreground = t.accent_strong;
                selected_material.container = MaterialContainerDescriptor{
                    0u,
                    0u,
                    12.0f,
                    options.interactive,
                    true};
                _impl::apply_glass_tabs_effect_context(
                    selected_material,
                    options,
                    _impl::glass_tabs_indicator_identity(options));
                btn.material = selected_material;
                apply_material_interaction_state(
                    btn,
                    is_hovered,
                    is_pressed,
                    is_focused);
                btn.material_shape = MaterialSurfaceShape::Capsule;
                btn.background = selected_material.tint;
                if (!is_focused)
                    btn.border_color = selected_material.border;
            }
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
        ln.background = options.has_tint && options.tint.a > 0
            ? glass_control_state_color(options.tint, 206)
            : t.accent;
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

inline int scaled_animation_duration_ms(int duration_ms) noexcept {
    if (duration_ms <= 0)
        return 0;
    float multiplier = g_app.theme.motion_duration_multiplier;
    if (!(multiplier > 0.0f))
        return 0;
    if (multiplier > 4.0f)
        multiplier = 4.0f;
    float const scaled = static_cast<float>(duration_ms) * multiplier;
    if (scaled < 1.0f)
        return 1;
    if (scaled > 60000.0f)
        return 60000;
    return static_cast<int>(scaled + 0.5f);
}

}  // namespace detail

template <typename T>
T animate_value(T target, int duration_ms,
                std::source_location loc = std::source_location::current()) {
    auto& s = framework_local<detail::AnimationState<T>>(
        detail::AnimationState<T>{}, 0, loc);
    auto now_ms = detail::steady_ms();
    auto const effective_duration_ms =
        detail::scaled_animation_duration_ms(duration_ms);

    if (!s.initialized || effective_duration_ms <= 0) {
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
        / static_cast<float>(effective_duration_ms));
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

inline std::uint64_t paint_invalidation_bit(unsigned int id) noexcept {
    return id == 0xFFFFFFFFu ? 0ULL : (1ULL << (id & 63u));
}

inline ActionPerfStats& action_perf_stats(std::string_view action) noexcept {
    if (action == "hover")
        return g_app.action_perf.hover;
    if (action == "scroll")
        return g_app.action_perf.scroll;
    if (action == "click")
        return g_app.action_perf.click;
    if (action == "key")
        return g_app.action_perf.key;
    if (action == "gesture")
        return g_app.action_perf.gesture;
    return g_app.action_perf.other;
}

inline std::uint64_t action_perf_recent_percentile(
        ActionPerfStats const& stats,
        double percentile) {
    if (stats.recent_count == 0)
        return 0;
    std::array<std::uint64_t, ActionPerfStats::RECENT_CAPACITY> sorted{};
    for (std::size_t i = 0; i < stats.recent_count; ++i)
        sorted[i] = stats.recent_ns[i];
    auto first = sorted.begin();
    auto last = sorted.begin() + static_cast<std::ptrdiff_t>(stats.recent_count);
    std::sort(first, last);
    auto const index = static_cast<std::size_t>(
        std::lround(percentile * static_cast<double>(stats.recent_count - 1)));
    return sorted[std::min(index, stats.recent_count - 1)];
}

inline void record_action_duration(char const* action,
                                   std::uint64_t duration_ns) noexcept {
    auto& stats = action_perf_stats(action ? std::string_view{action} : "other");
    ++stats.count;
    stats.total_ns += duration_ns;
    stats.last_ns = duration_ns;
    if (stats.min_ns == 0 || duration_ns < stats.min_ns)
        stats.min_ns = duration_ns;
    if (duration_ns > stats.max_ns)
        stats.max_ns = duration_ns;
    if (duration_ns > 16'666'667ull)
        ++stats.over_60fps_budget;
    stats.recent_ns[stats.recent_next] = duration_ns;
    stats.recent_next = (stats.recent_next + 1)
        % ActionPerfStats::RECENT_CAPACITY;
    if (stats.recent_count < ActionPerfStats::RECENT_CAPACITY)
        ++stats.recent_count;
}

inline FrameTraceAction frame_trace_action_from_name(
        std::string_view action) noexcept {
    if (action == "hover")
        return FrameTraceAction::Hover;
    if (action == "scroll")
        return FrameTraceAction::Scroll;
    if (action == "click")
        return FrameTraceAction::Click;
    if (action == "key")
        return FrameTraceAction::Key;
    if (action == "gesture")
        return FrameTraceAction::Gesture;
    if (action.empty() || action == "none")
        return FrameTraceAction::None;
    return FrameTraceAction::Other;
}

inline char const* frame_trace_action_label(
        FrameTraceAction action) noexcept {
    switch (action) {
        case FrameTraceAction::None:    return "idle";
        case FrameTraceAction::Hover:   return "hover";
        case FrameTraceAction::Scroll:  return "scroll";
        case FrameTraceAction::Click:   return "click";
        case FrameTraceAction::Key:     return "key";
        case FrameTraceAction::Gesture: return "gesture";
        case FrameTraceAction::Other:   return "other";
    }
    return "other";
}

struct FrameTraceInputContext {
    bool active = false;
    FrameTraceAction action = FrameTraceAction::None;
    std::uint64_t start_ns = 0;
};

inline bool frame_trace_input_active() noexcept {
    return g_app.frame_trace_input_active;
}

inline FrameTraceInputContext begin_frame_trace_input(
        char const* action) noexcept {
    FrameTraceInputContext previous{
        g_app.frame_trace_input_active,
        g_app.frame_trace_input_action,
        g_app.frame_trace_input_start_ns,
    };
    g_app.frame_trace_input_active = true;
    g_app.frame_trace_input_action = frame_trace_action_from_name(
        action ? std::string_view{action} : std::string_view{});
    g_app.frame_trace_input_start_ns = metrics::detail::now_ns();
    return previous;
}

inline void restore_frame_trace_input(
        FrameTraceInputContext previous) noexcept {
    g_app.frame_trace_input_active = previous.active;
    g_app.frame_trace_input_action = previous.action;
    g_app.frame_trace_input_start_ns = previous.start_ns;
}

struct FrameTraceInputScope {
    FrameTraceInputContext previous{};

    explicit FrameTraceInputScope(char const* action) noexcept
        : previous(begin_frame_trace_input(action)) {}

    ~FrameTraceInputScope() {
        restore_frame_trace_input(previous);
    }
};

inline void record_frame_trace(FrameTraceSample sample,
                               std::uint64_t frame_start_ns) noexcept {
    if (g_app.frame_trace_input_active) {
        sample.action = g_app.frame_trace_input_action;
        if (g_app.frame_trace_input_start_ns > 0
            && g_app.frame_trace_input_start_ns <= frame_start_ns) {
            sample.input_ns =
                frame_start_ns - g_app.frame_trace_input_start_ns;
        }
    }

    auto& stats = g_app.frame_perf;
    ++stats.count;
    stats.total_ns += sample.total_ns;
    stats.last_ns = sample.total_ns;
    if (stats.min_ns == 0 || sample.total_ns < stats.min_ns)
        stats.min_ns = sample.total_ns;
    if (sample.total_ns > stats.max_ns)
        stats.max_ns = sample.total_ns;
    if (sample.total_ns > 16'666'667ull)
        ++stats.over_60fps_budget;
    stats.last = sample;
    stats.recent[stats.recent_next] = sample;
    stats.recent_next = (stats.recent_next + 1)
        % FrameTraceMonitor::RECENT_CAPACITY;
    if (stats.recent_count < FrameTraceMonitor::RECENT_CAPACITY)
        ++stats.recent_count;
}

inline bool pointer_snapshot_changed(AppState const& app) noexcept {
    return app.pointer_valid != app.prev_pointer_valid
        || (app.pointer_valid
            && (app.pointer_x != app.prev_pointer_x
                || app.pointer_y != app.prev_pointer_y));
}

inline std::uint64_t compute_paint_invalidation_mask(AppState const& app) noexcept {
    std::uint64_t inv = 0;
    if (app.hovered_id != app.prev_hovered_id) {
        inv |= paint_invalidation_bit(app.hovered_id)
            | paint_invalidation_bit(app.prev_hovered_id);
    }
    if (app.focused_id != app.prev_focused_id) {
        inv |= paint_invalidation_bit(app.focused_id)
            | paint_invalidation_bit(app.prev_focused_id);
    }
    if (app.focus_visible != app.prev_focus_visible) {
        inv |= paint_invalidation_bit(app.focused_id)
            | paint_invalidation_bit(app.prev_focused_id);
    }
    if (app.pressed_id != app.prev_pressed_id) {
        inv |= paint_invalidation_bit(app.pressed_id)
            | paint_invalidation_bit(app.prev_pressed_id);
    }
    inv |= paint_invalidation_bit(app.focused_id);
    inv |= paint_invalidation_bit(app.pressed_id);
    if (app.has_active_animations || app.scrollbar_animation_active
        || pointer_snapshot_changed(app)) {
        inv = ~static_cast<std::uint64_t>(0);
    }
    return inv;
}

inline void persist_paint_inputs(AppState& app) noexcept {
    app.prev_scroll_x = app.scroll_x;
    app.prev_scroll_y = app.scroll_y;
    app.prev_hovered_id = app.hovered_id;
    app.prev_focused_id = app.focused_id;
    app.prev_focus_visible = app.focus_visible;
    app.prev_pressed_id = app.pressed_id;
    app.prev_pointer_valid = app.pointer_valid;
    app.prev_pointer_x = app.pointer_x;
    app.prev_pointer_y = app.pointer_y;
}

inline void reset_pointer_inputs(AppState& app) noexcept {
    app.pointer_valid = false;
    app.prev_pointer_valid = false;
    app.pointer_x = 0.0f;
    app.pointer_y = 0.0f;
    app.prev_pointer_x = 0.0f;
    app.prev_pointer_y = 0.0f;
}

#ifndef NDEBUG
inline void render_debug_panel_overlay();
#endif
inline bool set_pointer_position(float x, float y) noexcept;
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
    detail::g_app.focus_visible = false;
    detail::g_app.prev_focus_visible = false;
    detail::g_app.pressed_id = 0xFFFFFFFFu;
    detail::g_app.prev_pressed_id = 0xFFFFFFFFu;
    detail::reset_pointer_inputs(detail::g_app);

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
#ifndef NDEBUG
        detail::render_debug_panel_overlay();
#endif
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

        app.paint_invalidation_mask =
            detail::compute_paint_invalidation_mask(app);

        app.focusable_ids.clear();
        detail::collect_focusable_ids(root_h);
        for (auto overlay_h : app.overlays)
            detail::collect_focusable_ids(overlay_h);
        app.paint_scissor_depth = 0;
        app.scrollbar_animation_active = false;
        emit_clear(host, app.theme.background);
        detail::paint_node(host, host, root_h, 0, 0,
                           app.scroll_x, app.scroll_y, cw, vh);
        detail::reset_paint_scissor_boundary(host);
        detail::paint_root_vertical_scroll_bar(host, cw, vh);
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
        detail::record_frame_trace(
            FrameTraceSample{
                .total_ns = t5 - t0,
                .update_ns = t1 - t0,
                .view_ns = t2 - t1,
                .layout_ns = t3 - t2,
                .paint_ns = t4 - t3,
                .flush_ns = t5 - t4,
                .rebuild = true,
            },
            t0);

        // Persist the ambient paint inputs for next frame's blit guard.
        detail::persist_paint_inputs(app);

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
    detail::g_app.focus_visible = false;
    detail::g_app.prev_focus_visible = false;
    detail::g_app.pressed_id = 0xFFFFFFFFu;
    detail::g_app.prev_pressed_id = 0xFFFFFFFFu;
    detail::reset_pointer_inputs(detail::g_app);

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
#ifndef NDEBUG
        detail::render_debug_panel_overlay();
#endif
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
        app.paint_invalidation_mask =
            detail::compute_paint_invalidation_mask(app);

        app.focusable_ids.clear();
        detail::collect_focusable_ids(root_h);
        for (auto overlay_h : app.overlays)
            detail::collect_focusable_ids(overlay_h);
        app.paint_scissor_depth = 0;
        app.scrollbar_animation_active = false;
        detail::wasi_emit_clear(app.theme.background);
        detail::wasi_paint_node(root_h, 0, 0,
                                app.scroll_x, app.scroll_y, cw, vh);
        detail::wasi_reset_paint_scissor_boundary();
        detail::wasi_paint_root_vertical_scroll_bar(cw, vh);
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
        detail::persist_paint_inputs(app);

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

struct GlassEffectContainerOptions {
    std::uint32_t container_id = 0;
    std::uint32_t union_id = 0;
    float spacing = 0.0f;
    bool interactive = false;
    bool morph_transitions = true;
};

struct GlassEffectUnionOptions {
    std::uint32_t namespace_id = 0;
    std::uint32_t union_id = 0;
    float spacing = 0.0f;
    bool interactive = false;
    bool morph_transitions = true;
};

enum class GlassBackgroundDisplayMode {
    Always,
    Implicit,
    Never,
};

inline char const* glass_background_display_mode_name(
        GlassBackgroundDisplayMode mode) noexcept {
    switch (mode) {
        case GlassBackgroundDisplayMode::Always:
            return "always";
        case GlassBackgroundDisplayMode::Implicit:
            return "implicit";
        case GlassBackgroundDisplayMode::Never:
            return "never";
    }
    return "always";
}

inline constexpr float glass_background_default_feather_padding = 12.0f;
inline constexpr float glass_background_default_soft_edge_radius = 18.0f;

struct GlassBackgroundEffectStyle {
    MaterialGlassBackgroundEffectKind kind =
        MaterialGlassBackgroundEffectKind::Automatic;
    float feather_padding = 0.0f;
    float soft_edge_radius = 0.0f;
};

inline constexpr GlassBackgroundEffectStyle
glass_background_automatic() noexcept {
    return GlassBackgroundEffectStyle{
        .kind = MaterialGlassBackgroundEffectKind::Automatic,
    };
}

inline constexpr GlassBackgroundEffectStyle glass_background_plate() noexcept {
    return GlassBackgroundEffectStyle{
        .kind = MaterialGlassBackgroundEffectKind::Plate,
    };
}

inline constexpr GlassBackgroundEffectStyle glass_background_feathered(
        float padding = glass_background_default_feather_padding,
        float soft_edge_radius =
            glass_background_default_soft_edge_radius) noexcept {
    return GlassBackgroundEffectStyle{
        .kind = MaterialGlassBackgroundEffectKind::Feathered,
        .feather_padding = padding,
        .soft_edge_radius = soft_edge_radius,
    };
}

inline MaterialGlassBackgroundDescriptor glass_background_descriptor(
        GlassBackgroundEffectStyle style) noexcept {
    return MaterialGlassBackgroundDescriptor{
        style.kind,
        material_glass_background_non_negative(style.feather_padding),
        material_glass_background_non_negative(style.soft_edge_radius)};
}

inline MaterialStyle glass_background_effect_material_style(
        MaterialKind kind,
        GlassBackgroundEffectStyle effect) noexcept {
    auto style = material_style(kind);
    style.glass_background = glass_background_descriptor(effect);
    switch (style.glass_background.kind) {
        case MaterialGlassBackgroundEffectKind::Plate:
            style.opacity = std::clamp(style.opacity + 0.08f, 0.0f, 1.0f);
            style.blur_radius =
                std::min(style.blur_radius + 4.0f, material_max_blur_radius);
            style.saturation = std::max(style.saturation, 1.08f);
            style.edge_highlight =
                std::clamp(style.edge_highlight + 0.08f, 0.0f, 1.0f);
            style.edge_width = std::max(style.edge_width, 1.25f);
            style.shadow_alpha =
                std::clamp(style.shadow_alpha + 0.05f, 0.0f, 0.4f);
            style.shadow_radius = std::max(style.shadow_radius, 12.0f);
            break;
        case MaterialGlassBackgroundEffectKind::Feathered:
            style.opacity = std::clamp(style.opacity - 0.03f, 0.0f, 1.0f);
            style.blur_radius =
                std::min(style.blur_radius + 2.0f, material_max_blur_radius);
            style.edge_highlight =
                std::clamp(style.edge_highlight + 0.03f, 0.0f, 1.0f);
            style.edge_width = std::max(
                style.edge_width,
                1.0f + style.glass_background.soft_edge_radius * 0.05f);
            style.shadow_radius = std::max(
                style.shadow_radius,
                8.0f + style.glass_background.feather_padding * 0.25f);
            break;
        case MaterialGlassBackgroundEffectKind::Automatic:
        case MaterialGlassBackgroundEffectKind::None:
            break;
    }
    return style;
}

struct GlassBackgroundEffectOptions {
    MaterialKind kind = MaterialKind::Regular;
    MaterialSurfaceRole role = MaterialSurfaceRole::Surface;
    GlassBackgroundDisplayMode display_mode =
        GlassBackgroundDisplayMode::Always;
    GlassBackgroundEffectStyle effect = glass_background_automatic();
    FlexDirection direction = FlexDirection::Column;
    SpaceToken padding = SpaceToken::Md;
    SpaceToken gap = SpaceToken::Md;
    CrossAxisAlignment cross_align = CrossAxisAlignment::Start;
    MainAxisAlignment main_align = MainAxisAlignment::Start;
    float max_width = 0.0f;
    float fixed_height = -1.0f;
    float border_radius = -1.0f;
    MaterialSurfaceShape shape = MaterialSurfaceShape::RoundedRectangle;
    float border_width = 0.0f;
    char const* semantic_label = "Glass Background";
    bool interactive = false;
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

inline MaterialContainerOptions glass_effect_container_options(
        GlassEffectContainerOptions const& options) noexcept {
    return MaterialContainerOptions{
        options.container_id,
        options.union_id,
        options.spacing,
        options.interactive,
        options.morph_transitions};
}

inline MaterialContainerDescriptor glass_effect_union_descriptor(
        GlassEffectUnionOptions const& options) noexcept {
    return MaterialContainerDescriptor{
        options.namespace_id,
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

inline bool material_transition_is_identity(
        MaterialTransitionDescriptor descriptor) noexcept {
    return descriptor.kind == MaterialGlassTransitionKind::Identity
        && descriptor.progress == 1.0f
        && descriptor.appearing;
}

inline std::vector<MaterialTransitionDescriptor>& material_transition_stack() {
    static std::vector<MaterialTransitionDescriptor>& stack =
        *new std::vector<MaterialTransitionDescriptor>();
    return stack;
}

inline MaterialTransitionDescriptor current_material_transition() {
    auto& stack = material_transition_stack();
    return stack.empty() ? MaterialTransitionDescriptor{} : stack.back();
}

inline unsigned int& material_surface_scope_depth() {
    static unsigned int& depth = *new unsigned int(0u);
    return depth;
}

inline bool current_material_surface_contains_glass() noexcept {
    return material_surface_scope_depth() != 0u;
}

inline MaterialTransitionDescriptor resolve_material_transition(
        MaterialTransitionDescriptor requested,
        bool inherit_transition) {
    if (!inherit_transition)
        return requested;
    if (!material_transition_is_identity(requested))
        return requested;
    return current_material_transition();
}

inline bool glass_effect_container_should_default_transition(
        GlassEffectContainerOptions const& options) noexcept {
    return options.morph_transitions
        && material_transition_is_identity(current_material_transition());
}

inline MaterialTransitionDescriptor
glass_effect_container_default_transition() noexcept {
    return MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::MatchedGeometry,
        .progress = 1.0f,
        .appearing = true,
    };
}

inline constexpr MaterialGlassIdentityDescriptor glass_effect_identity(
        std::uint32_t namespace_id,
        std::uint32_t effect_id) noexcept {
    return MaterialGlassIdentityDescriptor{namespace_id, effect_id};
}

inline std::uint32_t glass_effect_stable_id(
        std::string_view value) noexcept {
    if (value.empty())
        return 0u;

    auto hash = std::uint32_t{2166136261u};
    for (auto ch : value) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 16777619u;
    }
    return hash == 0u ? 1u : hash;
}

inline MaterialContainerDescriptor glass_effect_union_descriptor(
        std::uint32_t namespace_id,
        std::uint32_t union_id,
        float spacing = 0.0f) noexcept {
    return glass_effect_union_descriptor(
        GlassEffectUnionOptions{
            .namespace_id = namespace_id,
            .union_id = union_id,
            .spacing = spacing,
        });
}

inline MaterialContainerDescriptor glass_effect_union_descriptor(
        std::string_view namespace_id,
        std::string_view union_id,
        float spacing = 0.0f) noexcept {
    return glass_effect_union_descriptor(
        glass_effect_stable_id(namespace_id),
        glass_effect_stable_id(union_id),
        spacing);
}

inline MaterialGlassIdentityDescriptor glass_effect_identity(
        std::string_view namespace_id,
        std::string_view effect_id) noexcept {
    return glass_effect_identity(
        glass_effect_stable_id(namespace_id),
        glass_effect_stable_id(effect_id));
}

inline bool material_glass_identity_is_empty(
        MaterialGlassIdentityDescriptor descriptor) noexcept {
    return descriptor.namespace_id == 0u && descriptor.effect_id == 0u;
}

inline std::vector<MaterialGlassIdentityDescriptor>&
material_glass_identity_stack() {
    static std::vector<MaterialGlassIdentityDescriptor>& stack =
        *new std::vector<MaterialGlassIdentityDescriptor>();
    return stack;
}

inline MaterialGlassIdentityDescriptor current_material_glass_identity() {
    auto& stack = material_glass_identity_stack();
    return stack.empty() ? MaterialGlassIdentityDescriptor{} : stack.back();
}

inline MaterialGlassIdentityDescriptor resolve_material_glass_identity(
        MaterialGlassIdentityDescriptor requested,
        bool inherit_identity) {
    if (!inherit_identity)
        return requested;
    if (!material_glass_identity_is_empty(requested))
        return requested;
    return current_material_glass_identity();
}

template<typename F>
    requires std::is_invocable_v<F>
void material_container(MaterialContainerOptions options, F&& builder) {
    auto& stack = material_container_stack();
    stack.push_back(material_container_descriptor(options));
    builder();
    stack.pop_back();
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect_container(GlassEffectContainerOptions options, F&& builder) {
    auto const push_default_transition =
        glass_effect_container_should_default_transition(options);
    if (push_default_transition) {
        auto& transition_stack = material_transition_stack();
        transition_stack.push_back(glass_effect_container_default_transition());
    }
    material_container(
        glass_effect_container_options(options),
        std::forward<F>(builder));
    if (push_default_transition)
        material_transition_stack().pop_back();
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect_union(GlassEffectUnionOptions options, F&& builder) {
    material_container(
        MaterialContainerOptions{
            .container_id = options.namespace_id,
            .union_id = options.union_id,
            .spacing = options.spacing,
            .interactive = options.interactive,
            .morph_transitions = options.morph_transitions,
        },
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect_union(std::uint32_t union_id, F&& builder) {
    auto container = current_material_container();
    container.union_id = union_id;
    auto& stack = material_container_stack();
    stack.push_back(container);
    builder();
    stack.pop_back();
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect_union(std::uint32_t namespace_id,
                        std::uint32_t union_id,
                        F&& builder,
                        float spacing = 0.0f) {
    glass_effect_union(
        GlassEffectUnionOptions{
            .namespace_id = namespace_id,
            .union_id = union_id,
            .spacing = spacing,
        },
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect_union(std::string_view namespace_id,
                        std::string_view union_id,
                        F&& builder,
                        float spacing = 0.0f) {
    glass_effect_union(
        GlassEffectUnionOptions{
            .namespace_id = glass_effect_stable_id(namespace_id),
            .union_id = glass_effect_stable_id(union_id),
            .spacing = spacing,
        },
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect_container(std::uint32_t container_id,
                            F&& builder,
                            float spacing = 0.0f) {
    glass_effect_container(
        GlassEffectContainerOptions{
            .container_id = container_id,
            .spacing = spacing,
        },
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect_transition(MaterialTransitionDescriptor transition,
                             F&& builder) {
    auto& stack = material_transition_stack();
    stack.push_back(transition);
    builder();
    stack.pop_back();
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect_id(MaterialGlassIdentityDescriptor identity, F&& builder) {
    auto& stack = material_glass_identity_stack();
    stack.push_back(identity);
    builder();
    stack.pop_back();
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect_id(std::uint32_t namespace_id,
                     std::uint32_t effect_id,
                     F&& builder) {
    glass_effect_id(
        glass_effect_identity(namespace_id, effect_id),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect_id(std::string_view namespace_id,
                     std::string_view effect_id,
                     F&& builder) {
    glass_effect_id(
        glass_effect_identity(namespace_id, effect_id),
        std::forward<F>(builder));
}

inline constexpr MaterialTransitionDescriptor glass_materialize_transition(
        float progress,
        bool appearing = true) noexcept {
    return MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::Materialize,
        .progress = progress,
        .appearing = appearing,
    };
}

inline constexpr MaterialTransitionDescriptor glass_materialize_transition()
        noexcept {
    return glass_materialize_transition(1.0f);
}

inline constexpr MaterialTransitionDescriptor glass_matched_geometry_transition(
        float progress,
        bool appearing = true) noexcept {
    return MaterialTransitionDescriptor{
        .kind = MaterialGlassTransitionKind::MatchedGeometry,
        .progress = progress,
        .appearing = appearing,
    };
}

inline constexpr MaterialTransitionDescriptor glass_matched_geometry_transition()
        noexcept {
    return glass_matched_geometry_transition(1.0f);
}

inline constexpr MaterialTransitionDescriptor glass_identity_transition()
        noexcept {
    return MaterialTransitionDescriptor{};
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
// share of the parent's leftover space. `grow` is the weight; sibling
// weighted children split the remainder of the parent's main axis in
// proportion to their values (e.g. `weighted(2, ...)` next to
// `weighted(1, ...)` gets twice the available remainder). Children
// without a weighted wrapper keep their intrinsic / max_width sizing and
// are laid out before the split runs, so weights apply only to whatever
// remains after fixed siblings have taken their share. Columns distribute
// height only when the parent has a bounded `fixed_height`.
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
    MaterialInteractionDescriptor interaction{};
    MaterialTransitionDescriptor transition{};
    MaterialGlassIdentityDescriptor glass_identity{};
    bool inherit_material_transition = true;
    bool inherit_material_identity = true;
    FlexDirection direction = FlexDirection::Column;
    SpaceToken padding = SpaceToken::Lg;
    SpaceToken gap = SpaceToken::Md;
    CrossAxisAlignment cross_align = CrossAxisAlignment::Start;
    MainAxisAlignment main_align = MainAxisAlignment::Start;
    float max_width = 0.0f;
    float fixed_height = -1.0f;
    float border_radius = -1.0f;
    MaterialSurfaceShape shape = MaterialSurfaceShape::Default;
    float border_width = -1.0f;
    char const* semantic_label = "";
    bool inherit_material_container = true;
    bool interactive = false;
    bool capture_pointer = false;
    bool has_material_override = false;
    MaterialStyle material_override{};
    MaterialContainerDescriptor container{};
};

struct GlassEffectOptions {
    MaterialKind kind = MaterialKind::Regular;
    MaterialSurfaceRole role = MaterialSurfaceRole::Surface;
    MaterialInteractionDescriptor interaction{};
    MaterialTransitionDescriptor transition{};
    MaterialGlassIdentityDescriptor glass_identity{};
    bool inherit_material_transition = true;
    bool inherit_material_identity = true;
    FlexDirection direction = FlexDirection::Column;
    SpaceToken padding = SpaceToken::Md;
    SpaceToken gap = SpaceToken::Md;
    CrossAxisAlignment cross_align = CrossAxisAlignment::Start;
    MainAxisAlignment main_align = MainAxisAlignment::Start;
    float max_width = 0.0f;
    float fixed_height = -1.0f;
    float border_radius = -1.0f;
    MaterialSurfaceShape shape = MaterialSurfaceShape::Capsule;
    float border_width = 0.0f;
    char const* semantic_label = "";
    bool inherit_material_container = true;
    bool interactive = false;
    bool has_tint = false;
    Color tint = {};
    bool has_border = false;
    Color border = {};
    MaterialContainerDescriptor container{};
};

struct GlassEffectStyle {
    MaterialKind kind = MaterialKind::Regular;
    bool interactive_enabled = false;
    bool has_tint = false;
    Color tint_color = {};
    bool has_border = false;
    Color border_color = {};
    MaterialTransitionDescriptor transition_descriptor{};
    bool inherit_transition = true;
    MaterialGlassIdentityDescriptor glass_identity_descriptor{};
    bool inherit_identity = true;
    MaterialContainerDescriptor container_descriptor{};
    bool inherit_container = true;

    constexpr bool is_identity() const noexcept {
        return kind == MaterialKind::None;
    }

    constexpr GlassEffectStyle tint(Color color) const noexcept {
        auto copy = *this;
        if (!copy.is_identity()) {
            copy.has_tint = true;
            copy.tint_color = color;
        }
        return copy;
    }

    constexpr GlassEffectStyle border(Color color) const noexcept {
        auto copy = *this;
        if (!copy.is_identity()) {
            copy.has_border = true;
            copy.border_color = color;
        }
        return copy;
    }

    constexpr GlassEffectStyle interactive(bool enabled = true) const noexcept {
        auto copy = *this;
        copy.interactive_enabled = enabled && !copy.is_identity();
        return copy;
    }

    constexpr GlassEffectStyle effect_id(
            MaterialGlassIdentityDescriptor descriptor) const noexcept {
        auto copy = *this;
        if (!copy.is_identity() && descriptor.participates()) {
            copy.glass_identity_descriptor = descriptor;
            copy.inherit_identity = false;
        }
        return copy;
    }

    GlassEffectStyle effect_id(std::string_view namespace_id,
                               std::string_view effect_id) const noexcept {
        return this->effect_id(
            glass_effect_identity(namespace_id, effect_id));
    }

    constexpr GlassEffectStyle transition(
            MaterialTransitionDescriptor descriptor) const noexcept {
        auto copy = *this;
        if (!copy.is_identity()) {
            copy.transition_descriptor = descriptor;
            copy.inherit_transition = false;
        }
        return copy;
    }

    constexpr GlassEffectStyle matched_geometry(
            float progress = 1.0f,
            bool appearing = true) const noexcept {
        return transition(glass_matched_geometry_transition(progress, appearing));
    }

    constexpr GlassEffectStyle materialize(
            float progress = 1.0f,
            bool appearing = true) const noexcept {
        return transition(glass_materialize_transition(progress, appearing));
    }

    constexpr GlassEffectStyle identity_transition() const noexcept {
        return transition(glass_identity_transition());
    }

    constexpr GlassEffectStyle effect_union(
            MaterialContainerDescriptor descriptor) const noexcept {
        auto copy = *this;
        if (!copy.is_identity()
            && descriptor.mode() == MaterialContainerMode::Union) {
            copy.container_descriptor = descriptor;
            copy.inherit_container = false;
        }
        return copy;
    }

    constexpr GlassEffectStyle effect_union(
            std::uint32_t namespace_id,
            std::uint32_t union_id,
            float spacing = 0.0f,
            bool interactive = false,
            bool morph_transitions = true) const noexcept {
        return effect_union(
            MaterialContainerDescriptor{
                namespace_id,
                union_id,
                spacing,
                interactive,
                morph_transitions});
    }

    GlassEffectStyle effect_union(
            std::string_view namespace_id,
            std::string_view union_id,
            float spacing = 0.0f,
            bool interactive = false,
            bool morph_transitions = true) const noexcept {
        return effect_union(
            glass_effect_stable_id(namespace_id),
            glass_effect_stable_id(union_id),
            spacing,
            interactive,
            morph_transitions);
    }
};

inline constexpr GlassEffectStyle glass_regular() noexcept {
    return GlassEffectStyle{.kind = MaterialKind::Regular};
}

inline constexpr GlassEffectStyle glass_clear() noexcept {
    return GlassEffectStyle{.kind = MaterialKind::Clear};
}

inline constexpr GlassEffectStyle glass_identity() noexcept {
    return GlassEffectStyle{.kind = MaterialKind::None};
}

inline GlassEffectOptions glass_effect_options(
        GlassEffectStyle style) noexcept {
    GlassEffectOptions options{};
    options.kind = style.kind;
    if (style.is_identity()) {
        options.interactive = false;
        options.has_tint = false;
        options.has_border = false;
        return options;
    }
    options.interactive = style.interactive_enabled;
    options.has_tint = style.has_tint;
    options.tint = style.tint_color;
    options.has_border = style.has_border;
    options.border = style.border_color;
    options.transition = style.transition_descriptor;
    options.inherit_material_transition = style.inherit_transition;
    options.glass_identity = style.glass_identity_descriptor;
    options.inherit_material_identity = style.inherit_identity;
    options.container = style.container_descriptor;
    options.inherit_material_container = style.inherit_container;
    return options;
}

inline GlassEffectOptions glass_effect_options(
        GlassEffectStyle style,
        MaterialSurfaceShape shape) noexcept {
    auto options = glass_effect_options(style);
    options.shape = shape;
    return options;
}

inline MaterialSurfaceOptions glass_effect_surface_options(
        GlassEffectOptions const& options) {
    MaterialSurfaceOptions surface{};
    surface.kind = options.kind;
    surface.role = options.role;
    surface.interaction = options.interaction;
    surface.transition = options.transition;
    surface.glass_identity = options.glass_identity;
    surface.inherit_material_transition = options.inherit_material_transition;
    surface.inherit_material_identity = options.inherit_material_identity;
    surface.direction = options.direction;
    surface.padding = options.padding;
    surface.gap = options.gap;
    surface.cross_align = options.cross_align;
    surface.main_align = options.main_align;
    surface.max_width = options.max_width;
    surface.fixed_height = options.fixed_height;
    surface.border_radius = options.border_radius;
    surface.shape = options.shape;
    surface.border_width = options.border_width;
    surface.semantic_label = options.semantic_label;
    surface.inherit_material_container = options.inherit_material_container;
    surface.interactive = options.interactive;
    surface.container = options.container;

    if (options.has_tint || options.has_border) {
        surface.has_material_override = true;
        surface.material_override = material_style(options.kind);
        if (options.has_tint)
            surface.material_override.tint = options.tint;
        if (options.has_border)
            surface.material_override.border = options.border;
    }

    return surface;
}

inline bool glass_background_effect_should_display(
        GlassBackgroundDisplayMode mode,
        bool contained_in_glass = current_material_surface_contains_glass())
        noexcept {
    switch (mode) {
        case GlassBackgroundDisplayMode::Always:
            return true;
        case GlassBackgroundDisplayMode::Implicit:
            return !contained_in_glass;
        case GlassBackgroundDisplayMode::Never:
            return false;
    }
    return true;
}

inline MaterialSurfaceOptions glass_background_effect_surface_options(
        GlassBackgroundEffectOptions const& options) {
    auto surface = MaterialSurfaceOptions{};
    auto const displayed =
        glass_background_effect_should_display(options.display_mode);
    surface.kind = displayed ? options.kind : MaterialKind::None;
    surface.role = options.role;
    surface.direction = options.direction;
    surface.padding = options.padding;
    surface.gap = options.gap;
    surface.cross_align = options.cross_align;
    surface.main_align = options.main_align;
    surface.max_width = options.max_width;
    surface.fixed_height = options.fixed_height;
    surface.border_radius = options.border_radius;
    surface.shape = options.shape;
    surface.border_width = options.border_width;
    surface.semantic_label = options.semantic_label;
    surface.interactive = displayed && options.interactive;
    if (displayed && options.kind != MaterialKind::None) {
        surface.has_material_override = true;
        surface.material_override =
            glass_background_effect_material_style(options.kind, options.effect);
    }
    return surface;
}

enum class GlassSurfacePreset {
    Window,
    Toolbar,
    ToolbarGroup,
    SegmentedControl,
    Navigation,
    TabBar,
    Sidebar,
    Content,
    StatusBar,
    Popover,
    Tooltip,
    ContextMenu,
    Sheet,
    Inspector,
    CommandPalette,
    Snackbar,
    Toast,
};

inline char const* glass_surface_preset_name(
        GlassSurfacePreset preset) noexcept {
    switch (preset) {
        case GlassSurfacePreset::Window:       return "window";
        case GlassSurfacePreset::Toolbar:      return "toolbar";
        case GlassSurfacePreset::ToolbarGroup: return "toolbar_group";
        case GlassSurfacePreset::SegmentedControl:
            return "segmented_control";
        case GlassSurfacePreset::Navigation:   return "navigation";
        case GlassSurfacePreset::TabBar:       return "tab_bar";
        case GlassSurfacePreset::Sidebar:      return "sidebar";
        case GlassSurfacePreset::Content:      return "content";
        case GlassSurfacePreset::StatusBar:    return "status_bar";
        case GlassSurfacePreset::Popover:      return "popover";
        case GlassSurfacePreset::Tooltip:      return "tooltip";
        case GlassSurfacePreset::ContextMenu:  return "context_menu";
        case GlassSurfacePreset::Sheet:        return "sheet";
        case GlassSurfacePreset::Inspector:    return "inspector";
        case GlassSurfacePreset::CommandPalette:
            return "command_palette";
        case GlassSurfacePreset::Snackbar:     return "snackbar";
        case GlassSurfacePreset::Toast:        return "toast";
    }
    return "content";
}

inline void configure_material_surface(LayoutNode& node,
                                       MaterialSurfaceOptions const& options) {
    auto const& t = detail::g_app.theme;
    node.material = options.has_material_override
        ? options.material_override
        : material_style(options.kind);
    node.material.role = options.role;
    node.material.interaction = options.interaction;
    node.material.transition = resolve_material_transition(
        options.transition,
        options.inherit_material_transition);
    node.material.glass_identity = resolve_material_glass_identity(
        options.glass_identity,
        options.inherit_material_identity);
    node.material.container = options.inherit_material_container
        ? current_material_container()
        : options.container;
    if (options.interactive)
        node.material.container.interactive = true;
    node.background = node.material.tint;
    node.border_color = node.material.border;
    node.border_width = options.border_width >= 0.0f
        ? options.border_width
        : (options.kind == MaterialKind::None ? 0.0f : 1.0f);
    node.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_lg;
    node.material_shape = options.shape;
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
    if (options.capture_pointer) {
        auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
        detail::g_app.callbacks.push_back([] {});
        detail::g_app.callback_roles.push_back(InteractionRole::None);
        node.callback_id = id;
        node.focusable = false;
        node.cursor_type = 0;
        node.debug_semantic_hidden = true;
        node.debug_semantic_enabled = false;
    }
    auto& surface_depth = material_surface_scope_depth();
    auto const pushes_surface_scope = options.kind != MaterialKind::None;
    if (pushes_surface_scope)
        ++surface_depth;
    detail::open_container(h, std::forward<F>(builder));
    if (pushes_surface_scope)
        --surface_depth;
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

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect(GlassEffectOptions options, F&& builder) {
    material_surface(
        glass_effect_surface_options(options),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect(GlassEffectStyle style, F&& builder) {
    glass_effect(
        glass_effect_options(style),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect(GlassEffectStyle style,
                  MaterialSurfaceShape shape,
                  F&& builder) {
    glass_effect(
        glass_effect_options(style, shape),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_effect(F&& builder) {
    glass_effect(GlassEffectOptions{}, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_background_effect(GlassBackgroundEffectOptions options,
                             F&& builder) {
    material_surface(
        glass_background_effect_surface_options(options),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_background_effect(GlassBackgroundEffectStyle effect,
                             F&& builder,
                             GlassBackgroundDisplayMode display_mode =
                                 GlassBackgroundDisplayMode::Always) {
    glass_background_effect(
        GlassBackgroundEffectOptions{
            .display_mode = display_mode,
            .effect = effect,
        },
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_background_effect(GlassBackgroundEffectStyle effect,
                             MaterialSurfaceShape shape,
                             F&& builder,
                             GlassBackgroundDisplayMode display_mode =
                                 GlassBackgroundDisplayMode::Always) {
    glass_background_effect(
        GlassBackgroundEffectOptions{
            .display_mode = display_mode,
            .effect = effect,
            .shape = shape,
        },
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_background_effect(F&& builder,
                             GlassBackgroundDisplayMode display_mode =
                                 GlassBackgroundDisplayMode::Always) {
    glass_background_effect(
        GlassBackgroundEffectOptions{.display_mode = display_mode},
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_background_effect(MaterialSurfaceShape shape,
                             F&& builder,
                             GlassBackgroundDisplayMode display_mode =
                                 GlassBackgroundDisplayMode::Always) {
    glass_background_effect(
        GlassBackgroundEffectOptions{
            .display_mode = display_mode,
            .shape = shape,
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
            options.shape = MaterialSurfaceShape::Rectangle;
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
            options.interactive = true;
            options.shape = MaterialSurfaceShape::Capsule;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Toolbar Group");
            break;
        case GlassSurfacePreset::SegmentedControl:
            options.kind = MaterialKind::Regular;
            options.role = MaterialSurfaceRole::Navigation;
            options.direction = FlexDirection::Row;
            options.padding = SpaceToken::Xs;
            options.gap = SpaceToken::Xs;
            options.cross_align = CrossAxisAlignment::Center;
            options.interactive = true;
            options.border_radius = t.radius_md;
            options.shape = MaterialSurfaceShape::Capsule;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Segmented Control");
            break;
        case GlassSurfacePreset::Navigation:
            options.kind = MaterialKind::Thin;
            options.role = MaterialSurfaceRole::Navigation;
            options.direction = FlexDirection::Row;
            options.padding = SpaceToken::Sm;
            options.gap = SpaceToken::Xs;
            options.cross_align = CrossAxisAlignment::Center;
            options.interactive = true;
            options.shape = MaterialSurfaceShape::Capsule;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Navigation");
            break;
        case GlassSurfacePreset::TabBar:
            options.kind = MaterialKind::Regular;
            options.role = MaterialSurfaceRole::Navigation;
            options.direction = FlexDirection::Row;
            options.padding = SpaceToken::Xs;
            options.gap = SpaceToken::Xs;
            options.cross_align = CrossAxisAlignment::Center;
            options.main_align = MainAxisAlignment::Center;
            options.interactive = true;
            options.shape = MaterialSurfaceShape::Capsule;
            options.border_width = 1.0f;
            options.max_width = 720.0f;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Tab Bar");
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
        case GlassSurfacePreset::Popover:
            options.kind = MaterialKind::Regular;
            options.role = MaterialSurfaceRole::Overlay;
            options.direction = FlexDirection::Column;
            options.padding = SpaceToken::Md;
            options.gap = SpaceToken::Sm;
            options.interactive = true;
            options.border_radius = t.radius_lg;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Popover");
            break;
        case GlassSurfacePreset::Tooltip:
            options.kind = MaterialKind::Thin;
            options.role = MaterialSurfaceRole::Overlay;
            options.direction = FlexDirection::Column;
            options.padding = SpaceToken::Xs;
            options.gap = SpaceToken::Xs;
            options.interactive = false;
            options.border_radius = t.radius_sm;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Tooltip");
            break;
        case GlassSurfacePreset::ContextMenu:
            options.kind = MaterialKind::Regular;
            options.role = MaterialSurfaceRole::Overlay;
            options.direction = FlexDirection::Column;
            options.padding = SpaceToken::Xs;
            options.gap = SpaceToken::Xs;
            options.interactive = true;
            options.border_radius = t.radius_md;
            options.border_width = 1.0f;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Context Menu");
            break;
        case GlassSurfacePreset::Sheet:
            options.kind = MaterialKind::Regular;
            options.role = MaterialSurfaceRole::Overlay;
            options.direction = FlexDirection::Column;
            options.padding = SpaceToken::Lg;
            options.gap = SpaceToken::Md;
            options.interactive = true;
            options.border_radius = t.radius_lg;
            options.border_width = 1.0f;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Sheet");
            break;
        case GlassSurfacePreset::Inspector:
            options.kind = MaterialKind::Thin;
            options.role = MaterialSurfaceRole::Overlay;
            options.direction = FlexDirection::Column;
            options.padding = SpaceToken::Md;
            options.gap = SpaceToken::Sm;
            options.interactive = true;
            options.border_radius = t.radius_lg;
            options.border_width = 1.0f;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Inspector");
            break;
        case GlassSurfacePreset::CommandPalette:
            options.kind = MaterialKind::Thick;
            options.role = MaterialSurfaceRole::Overlay;
            options.direction = FlexDirection::Column;
            options.padding = SpaceToken::Md;
            options.gap = SpaceToken::Sm;
            options.interactive = true;
            options.border_radius = t.radius_lg;
            options.border_width = 1.0f;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Command Palette");
            break;
        case GlassSurfacePreset::Snackbar:
            options.kind = MaterialKind::Regular;
            options.role = MaterialSurfaceRole::Overlay;
            options.direction = FlexDirection::Row;
            options.padding = SpaceToken::Sm;
            options.gap = SpaceToken::Sm;
            options.cross_align = CrossAxisAlignment::Center;
            options.main_align = MainAxisAlignment::Start;
            options.interactive = true;
            options.shape = MaterialSurfaceShape::Capsule;
            options.border_width = 1.0f;
            options.max_width = 520.0f;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Snackbar");
            break;
        case GlassSurfacePreset::Toast:
            options.kind = MaterialKind::Thin;
            options.role = MaterialSurfaceRole::Overlay;
            options.direction = FlexDirection::Row;
            options.padding = SpaceToken::Xs;
            options.gap = SpaceToken::Xs;
            options.cross_align = CrossAxisAlignment::Center;
            options.main_align = MainAxisAlignment::Center;
            options.interactive = false;
            options.shape = MaterialSurfaceShape::Capsule;
            options.border_width = 1.0f;
            options.max_width = 360.0f;
            options.semantic_label = chrome_label_or(
                semantic_label,
                "Toast");
            break;
    }
    return options;
}

inline MaterialSurfaceOptions glass_surface_options(
        GlassSurfacePreset preset,
        GlassEffectStyle style,
        char const* semantic_label = "") {
    auto options = glass_surface_options(preset, semantic_label);
    options.kind = style.kind;
    if (style.is_identity()) {
        options.interactive = false;
        options.has_material_override = false;
        options.material_override = {};
        options.transition = {};
        options.glass_identity = {};
        options.inherit_material_transition = true;
        options.inherit_material_identity = true;
        options.inherit_material_container = true;
        options.container = {};
        return options;
    }

    if (style.interactive_enabled)
        options.interactive = true;
    options.transition = style.transition_descriptor;
    options.inherit_material_transition = style.inherit_transition;
    options.glass_identity = style.glass_identity_descriptor;
    options.inherit_material_identity = style.inherit_identity;
    options.container = style.container_descriptor;
    options.inherit_material_container = style.inherit_container;

    if (style.has_tint || style.has_border) {
        options.has_material_override = true;
        options.material_override = material_style(style.kind);
        if (style.has_tint)
            options.material_override.tint = style.tint_color;
        if (style.has_border)
            options.material_override.border = style.border_color;
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
void glass_surface(GlassSurfacePreset preset,
                   GlassEffectStyle style,
                   F&& builder,
                   char const* semantic_label = "") {
    material_surface(
        glass_surface_options(preset, style, semantic_label),
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
    options.interactive = true;
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
            .interactive = true,
        },
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void tab_bar(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Navigation;
    options.interactive = true;
    options.semantic_label = chrome_label_or(options.semantic_label, "Tab Bar");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void tab_bar(F&& builder,
             char const* semantic_label = "Tab Bar") {
    material_surface(
        glass_surface_options(GlassSurfacePreset::TabBar, semantic_label),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void segmented_control_surface(MaterialSurfaceOptions options, F&& builder) {
    options.interactive = true;
    options.semantic_label = chrome_label_or(options.semantic_label,
                                             "Segmented Control");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void segmented_control_surface(F&& builder,
                               char const* semantic_label =
                                   "Segmented Control") {
    material_surface(
        glass_surface_options(GlassSurfacePreset::SegmentedControl,
                              semantic_label),
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

template<typename F>
    requires std::is_invocable_v<F>
void popover(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Overlay;
    options.interactive = true;
    options.semantic_label = chrome_label_or(options.semantic_label, "Popover");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void popover(F&& builder,
             MaterialKind kind = MaterialKind::Regular,
             SpaceToken padding = SpaceToken::Md,
             SpaceToken gap = SpaceToken::Sm) {
    auto options = glass_surface_options(GlassSurfacePreset::Popover,
                                         "Popover");
    options.kind = kind;
    options.padding = padding;
    options.gap = gap;
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void tooltip(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Overlay;
    options.interactive = false;
    options.semantic_label = chrome_label_or(options.semantic_label, "Tooltip");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void tooltip(F&& builder,
             char const* semantic_label = "Tooltip") {
    material_surface(
        glass_surface_options(GlassSurfacePreset::Tooltip, semantic_label),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void context_menu(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Overlay;
    options.interactive = true;
    options.semantic_label = chrome_label_or(options.semantic_label,
                                             "Context Menu");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void context_menu(F&& builder,
                  char const* semantic_label = "Context Menu") {
    material_surface(
        glass_surface_options(GlassSurfacePreset::ContextMenu,
                              semantic_label),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void sheet(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Overlay;
    options.interactive = true;
    options.semantic_label = chrome_label_or(options.semantic_label, "Sheet");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void sheet(F&& builder,
           char const* semantic_label = "Sheet") {
    material_surface(
        glass_surface_options(GlassSurfacePreset::Sheet, semantic_label),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void inspector_panel(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Overlay;
    options.interactive = true;
    options.semantic_label = chrome_label_or(options.semantic_label,
                                             "Inspector");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void inspector_panel(F&& builder,
                     char const* semantic_label = "Inspector") {
    material_surface(
        glass_surface_options(GlassSurfacePreset::Inspector, semantic_label),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void command_palette(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Overlay;
    options.interactive = true;
    options.semantic_label = chrome_label_or(options.semantic_label,
                                             "Command Palette");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void command_palette(F&& builder,
                     char const* semantic_label = "Command Palette") {
    material_surface(
        glass_surface_options(GlassSurfacePreset::CommandPalette,
                              semantic_label),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void snackbar(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Overlay;
    options.interactive = true;
    options.semantic_label = chrome_label_or(options.semantic_label,
                                             "Snackbar");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void snackbar(F&& builder,
              char const* semantic_label = "Snackbar") {
    material_surface(
        glass_surface_options(GlassSurfacePreset::Snackbar, semantic_label),
        std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void toast(MaterialSurfaceOptions options, F&& builder) {
    options.role = MaterialSurfaceRole::Overlay;
    options.interactive = false;
    options.semantic_label = chrome_label_or(options.semantic_label, "Toast");
    material_surface(options, std::forward<F>(builder));
}

template<typename F>
    requires std::is_invocable_v<F>
void toast(F&& builder,
           char const* semantic_label = "Toast") {
    material_surface(
        glass_surface_options(GlassSurfacePreset::Toast, semantic_label),
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
// patterns continue to build on the same fixed overlay root.
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

namespace _impl {

inline void fixed_overlay_spacer(unsigned int top_padding) {
    if (top_padding == 0u)
        return;
    auto sp_h = detail::alloc_node();
    detail::node_at(sp_h).style.fixed_height =
        static_cast<float>(top_padding);
    detail::attach_to_scope(sp_h);
}

template<typename F>
    requires std::is_invocable_v<F>
void glass_overlay_surface(GlassSurfacePreset preset,
                           F&& builder,
                           float max_width,
                           unsigned int top_padding,
                           char const* semantic_label) {
    overlay([&] {
        fixed_overlay_spacer(top_padding);
        row([&] {
            sized_box(max_width, [&] {
                auto options = glass_surface_options(preset, semantic_label);
                options.max_width = max_width;
                switch (preset) {
                    case GlassSurfacePreset::Popover:
                        popover(options, std::forward<F>(builder));
                        break;
                    case GlassSurfacePreset::Tooltip:
                        tooltip(options, std::forward<F>(builder));
                        break;
                    case GlassSurfacePreset::ContextMenu:
                        context_menu(options, std::forward<F>(builder));
                        break;
                    case GlassSurfacePreset::CommandPalette:
                        command_palette(options, std::forward<F>(builder));
                        break;
                    case GlassSurfacePreset::Snackbar:
                        snackbar(options, std::forward<F>(builder));
                        break;
                    case GlassSurfacePreset::Toast:
                        toast(options, std::forward<F>(builder));
                        break;
                    case GlassSurfacePreset::Sheet:
                        sheet(options, std::forward<F>(builder));
                        break;
                    default:
                        material_surface(options, std::forward<F>(builder));
                        break;
                }
            });
        }, SpaceToken::Md, CrossAxisAlignment::Start, MainAxisAlignment::Center);
    });
}

} // namespace _impl

template<typename F>
    requires std::is_invocable_v<F>
void popover_overlay(F&& builder,
                     float max_width = 320.0f,
                     unsigned int top_padding = 72,
                     char const* semantic_label = "Popover") {
    _impl::glass_overlay_surface(
        GlassSurfacePreset::Popover,
        std::forward<F>(builder),
        max_width,
        top_padding,
        semantic_label);
}

template<typename F>
    requires std::is_invocable_v<F>
void tooltip_overlay(F&& builder,
                     float max_width = 260.0f,
                     unsigned int top_padding = 48,
                     char const* semantic_label = "Tooltip") {
    _impl::glass_overlay_surface(
        GlassSurfacePreset::Tooltip,
        std::forward<F>(builder),
        max_width,
        top_padding,
        semantic_label);
}

template<typename F>
    requires std::is_invocable_v<F>
void context_menu_overlay(F&& builder,
                          float max_width = 240.0f,
                          unsigned int top_padding = 72,
                          char const* semantic_label = "Context Menu") {
    _impl::glass_overlay_surface(
        GlassSurfacePreset::ContextMenu,
        std::forward<F>(builder),
        max_width,
        top_padding,
        semantic_label);
}

template<typename F>
    requires std::is_invocable_v<F>
void command_palette_overlay(F&& builder,
                             float max_width = 560.0f,
                             unsigned int top_padding = 72,
                             char const* semantic_label = "Command Palette") {
    _impl::glass_overlay_surface(
        GlassSurfacePreset::CommandPalette,
        std::forward<F>(builder),
        max_width,
        top_padding,
        semantic_label);
}

template<typename F>
    requires std::is_invocable_v<F>
void snackbar_overlay(F&& builder,
                      float max_width = 520.0f,
                      unsigned int top_padding = 24,
                      char const* semantic_label = "Snackbar") {
    _impl::glass_overlay_surface(
        GlassSurfacePreset::Snackbar,
        std::forward<F>(builder),
        max_width,
        top_padding,
        semantic_label);
}

template<typename F>
    requires std::is_invocable_v<F>
void toast_overlay(F&& builder,
                   float max_width = 360.0f,
                   unsigned int top_padding = 16,
                   char const* semantic_label = "Toast") {
    _impl::glass_overlay_surface(
        GlassSurfacePreset::Toast,
        std::forward<F>(builder),
        max_width,
        top_padding,
        semantic_label);
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
                sheet(
                    std::forward<F>(builder),
                    "Dialog Sheet");
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
        bool const is_focused = detail::focus_ring_visible(id);
        auto const chrome = widget::glass_disclosure_header_style(
            GlassDisclosureStyleOptions{.expanded = expanded});

        auto header_h = detail::alloc_node();
        {
            auto& hd = detail::node_at(header_h);
            hd.style.flex_direction = FlexDirection::Row;
            hd.style.cross_align = CrossAxisAlignment::Center;
            hd.style.gap = t.space_sm;
            hd.style.padding[0] = chrome.padding_top >= 0.0f
                ? chrome.padding_top
                : t.space_sm;
            hd.style.padding[1] = chrome.padding_right >= 0.0f
                ? chrome.padding_right
                : t.space_md;
            hd.style.padding[2] = chrome.padding_bottom >= 0.0f
                ? chrome.padding_bottom
                : t.space_sm;
            hd.style.padding[3] = chrome.padding_left >= 0.0f
                ? chrome.padding_left
                : t.space_md;
            hd.cursor_type = 1;
            hd.callback_id = id;
            hd.interaction_role = InteractionRole::Button;
            hd.focusable = true;
            hd.background = chrome.has_background
                ? chrome.background
                : t.surface;
            hd.hover_background = chrome.has_hover_background
                ? chrome.hover_background
                : t.state_hover_bg;
            // Focus ring: width grows from 1px to
            // `state_focus_ring_width`, colour cross-fades from
            // the disclosure border to `state_focus_ring`.
            float const base_border_width = chrome.border_width >= 0.0f
                ? chrome.border_width
                : 1.0f;
            Color const base_border = chrome.has_border_color
                ? chrome.border_color
                : t.border;
            int const focus_ms = detail::focus_ring_transition_ms(id, 150);
            hd.border_width = animate_float(
                is_focused ? t.state_focus_ring_width : base_border_width,
                focus_ms);
            hd.border_color = animate_color(
                is_focused ? t.state_focus_ring : base_border, focus_ms);
            hd.border_radius = chrome.border_radius >= 0.0f
                ? chrome.border_radius
                : t.radius_sm;
            if (chrome.has_material && chrome.material.kind != MaterialKind::None) {
                hd.material = chrome.material;
                hd.material.tint = hd.background;
                hd.material.border = hd.border_color;
                hd.material.foreground = chrome.has_text_color
                    ? chrome.text_color
                    : t.foreground;
            }
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

namespace widget {

inline GlassControlStyleOptions glass_control_style_options(
        layout::GlassEffectStyle glass,
        GlassControlStyleOptions options = {}) noexcept {
    options.kind = glass.kind;
    if (glass.has_tint) {
        options.has_tint = true;
        options.tint = glass.tint_color;
    }
    if (glass.has_border) {
        options.has_border = true;
        options.border = glass.border_color;
    }
    return options;
}

inline void apply_glass_effect_style_options(
        GlassToggleStyleOptions& options,
        layout::GlassEffectStyle glass) {
    options.kind = glass.kind;
    if (glass.is_identity()) {
        options.has_tint = false;
        options.has_border = false;
        options.has_effect_context = false;
        return;
    }
    if (glass.has_tint) {
        options.has_tint = true;
        options.tint = glass.tint_color;
    }
    if (glass.has_border) {
        options.has_border = true;
        options.border = glass.border_color;
    }
    options.has_effect_context = true;
    options.transition = layout::resolve_material_transition(
        glass.transition_descriptor,
        glass.inherit_transition);
    options.glass_identity = layout::resolve_material_glass_identity(
        glass.glass_identity_descriptor,
        glass.inherit_identity);
    options.container = glass.inherit_container
        ? layout::current_material_container()
        : glass.container_descriptor;
}

inline void apply_glass_effect_style_options(
        GlassSwitchStyleOptions& options,
        layout::GlassEffectStyle glass) {
    options.kind = glass.kind;
    if (glass.is_identity()) {
        options.has_tint = false;
        options.has_border = false;
        options.has_effect_context = false;
        return;
    }
    if (glass.has_tint) {
        options.has_tint = true;
        options.tint = glass.tint_color;
    }
    if (glass.has_border) {
        options.has_border = true;
        options.border = glass.border_color;
    }
    options.has_effect_context = true;
    options.transition = layout::resolve_material_transition(
        glass.transition_descriptor,
        glass.inherit_transition);
    options.glass_identity = layout::resolve_material_glass_identity(
        glass.glass_identity_descriptor,
        glass.inherit_identity);
    options.container = glass.inherit_container
        ? layout::current_material_container()
        : glass.container_descriptor;
}

inline GlassToggleStyleOptions glass_toggle_style_options(
        layout::GlassEffectStyle glass,
        GlassToggleStyleOptions options = {}) {
    apply_glass_effect_style_options(options, glass);
    return options;
}

inline GlassSwitchStyleOptions glass_switch_style_options(
        layout::GlassEffectStyle glass,
        GlassSwitchStyleOptions options = {}) {
    apply_glass_effect_style_options(options, glass);
    return options;
}

inline TabsStyleOptions glass_tabs_style_options(
        layout::GlassEffectStyle glass,
        TabsStyleOptions options = {}) {
    options.kind = glass.kind;
    if (glass.is_identity()) {
        options.has_tint = false;
        options.has_border = false;
        options.has_effect_context = false;
        options.glass_identity = {};
        options.indicator_glass_identity = {};
        options.container = {};
        return options;
    }
    if (glass.has_tint) {
        options.has_tint = true;
        options.tint = glass.tint_color;
    }
    if (glass.has_border) {
        options.has_border = true;
        options.border = glass.border_color;
    }
    options.has_effect_context = true;
    options.transition = layout::resolve_material_transition(
        glass.transition_descriptor,
        glass.inherit_transition);
    options.glass_identity = layout::resolve_material_glass_identity(
        glass.glass_identity_descriptor,
        glass.inherit_identity);
    options.container = glass.inherit_container
        ? layout::current_material_container()
        : glass.container_descriptor;
    return options;
}

inline void apply_glass_effect_style_material(
        MaterialStyle& material,
        bool has_material,
        bool disabled,
        layout::GlassEffectStyle glass) {
    if (!has_material
        || material.kind == MaterialKind::None
        || glass.is_identity()) {
        return;
    }

    material.transition = layout::resolve_material_transition(
        glass.transition_descriptor,
        glass.inherit_transition);
    material.glass_identity = layout::resolve_material_glass_identity(
        glass.glass_identity_descriptor,
        glass.inherit_identity);

    auto const container = glass.inherit_container
        ? layout::current_material_container()
        : glass.container_descriptor;
    if (container.participates()) {
        auto const keep_interaction =
            material.container.interactive || glass.interactive_enabled;
        material.container = container;
        if (!disabled && keep_interaction)
            material.container.interactive = true;
    }
}

inline void apply_glass_effect_style_material(
        ButtonStyleOptions& style,
        layout::GlassEffectStyle glass) {
    apply_glass_effect_style_material(
        style.material,
        style.has_material,
        style.disabled,
        glass);
}

inline void apply_glass_effect_style_material(
        TextFieldStyleOptions& style,
        layout::GlassEffectStyle glass) {
    apply_glass_effect_style_material(
        style.material,
        style.has_material,
        style.disabled,
        glass);
}

inline ButtonStyleOptions glass_control_button_style(
        layout::GlassEffectStyle glass,
        GlassControlStyleOptions options = {}) {
    auto style = glass_control_button_style(
        glass_control_style_options(glass, options));
    apply_glass_effect_style_material(style, glass);
    return style;
}

inline ButtonStyleOptions glass_prominent_button_style(
        layout::GlassEffectStyle glass,
        GlassControlStyleOptions options = {}) {
    auto style = glass_prominent_button_style(
        glass_control_style_options(glass, options));
    apply_glass_effect_style_material(style, glass);
    return style;
}

inline ButtonStyleOptions glass_split_button_style(
        layout::GlassEffectStyle glass,
        GlassSplitButtonStyleOptions options = {}) {
    auto style = glass_split_button_style(options);
    apply_glass_effect_style_material(style, glass);
    return style;
}

inline ButtonStyleOptions glass_selection_button_style(
        layout::GlassEffectStyle glass,
        GlassSelectionStyleOptions options = {}) {
    auto style = glass_selection_button_style(options);
    apply_glass_effect_style_material(style, glass);
    return style;
}

inline ButtonStyleOptions glass_outline_row_button_style(
        layout::GlassEffectStyle glass,
        GlassOutlineRowStyleOptions options = {}) {
    auto style = glass_outline_row_button_style(options);
    apply_glass_effect_style_material(style, glass);
    return style;
}

inline ButtonStyleOptions glass_menu_item_button_style(
        layout::GlassEffectStyle glass,
        GlassMenuItemStyleOptions options = {}) {
    auto style = glass_menu_item_button_style(options);
    apply_glass_effect_style_material(style, glass);
    return style;
}

inline ButtonStyleOptions glass_table_header_button_style(
        layout::GlassEffectStyle glass,
        GlassTableHeaderStyleOptions options = {}) {
    auto style = glass_table_header_button_style(options);
    apply_glass_effect_style_material(style, glass);
    return style;
}

inline ButtonStyleOptions glass_disclosure_header_style(
        layout::GlassEffectStyle glass,
        GlassDisclosureStyleOptions options = {}) {
    auto style = glass_disclosure_header_style(options);
    apply_glass_effect_style_material(style, glass);
    return style;
}

inline TextFieldStyleOptions glass_text_field_style(
        layout::GlassEffectStyle glass,
        GlassTextFieldStyleOptions options = {}) {
    auto style = glass_text_field_style(options);
    apply_glass_effect_style_material(style, glass);
    return style;
}

inline ButtonStyleOptions glass_button_style(
        layout::GlassEffectStyle glass,
        GlassControlStyleOptions options = {}) {
    return glass_control_button_style(glass, options);
}

template<typename Msg>
inline void glass_button(str label,
                         Msg msg,
                         layout::GlassEffectStyle glass,
                         GlassControlStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_button_style(glass, options));
}

template<typename Msg>
inline void glass_prominent_button(str label,
                                   Msg msg,
                                   layout::GlassEffectStyle glass,
                                   GlassControlStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_prominent_button_style(glass, options));
}

template<typename Msg>
inline void glass_selection_button(str label,
                                   Msg msg,
                                   GlassSelectionStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_selection_button_style(options));
}

template<typename Msg>
inline void glass_selection_button(str label,
                                   Msg msg,
                                   layout::GlassEffectStyle glass,
                                   GlassSelectionStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_selection_button_style(glass, options));
}

template<typename Msg>
inline void glass_outline_row_button(str label,
                                     Msg msg,
                                     GlassOutlineRowStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_outline_row_button_style(options));
}

template<typename Msg>
inline void glass_outline_row_button(str label,
                                     Msg msg,
                                     layout::GlassEffectStyle glass,
                                     GlassOutlineRowStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_outline_row_button_style(glass, options));
}

template<typename Msg>
inline void glass_menu_item_button(str label,
                                   Msg msg,
                                   GlassMenuItemStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_menu_item_button_style(options));
}

template<typename Msg>
inline void glass_menu_item_button(str label,
                                   Msg msg,
                                   layout::GlassEffectStyle glass,
                                   GlassMenuItemStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_menu_item_button_style(glass, options));
}

namespace _impl {

inline std::string dropdown_button_label(str label,
                                         std::vector<str> const& items,
                                         std::size_t selected) {
    std::string display(label.data, label.len);
    if (selected < items.size() && items[selected].len > 0) {
        if (!display.empty())
            display += ": ";
        display.append(items[selected].data, items[selected].len);
    }
    return display;
}

inline GlassControlStyleOptions dropdown_button_options(
        GlassDropdownStyleOptions const& options,
        bool open,
        bool disabled) {
    return GlassControlStyleOptions{
        .kind = options.button_kind,
        .role = options.button_role,
        .selected = open,
        .disabled = disabled,
        .width = options.button_width,
        .height = options.button_height,
        .text_align = TextAlign::Start};
}

inline ButtonStyleOptions dropdown_button_style(
        GlassDropdownStyleOptions const& options,
        bool open,
        bool disabled) {
    return glass_control_button_style(
        dropdown_button_options(options, open, disabled));
}

inline ButtonStyleOptions dropdown_button_style(
        layout::GlassEffectStyle glass,
        GlassDropdownStyleOptions const& options,
        bool open,
        bool disabled) {
    return glass_control_button_style(
        glass,
        dropdown_button_options(options, open, disabled));
}

inline ButtonStyleOptions dropdown_menu_item_style(
        GlassDropdownStyleOptions const& options,
        bool selected) {
    if (selected) {
        return glass_selection_button_style(GlassSelectionStyleOptions{
            .role = options.menu_item_role,
            .selected = true,
            .width = options.menu_width,
            .height = options.item_height,
            .text_align = TextAlign::Start});
    }

    auto style = glass_menu_item_button_style(GlassMenuItemStyleOptions{
        .kind = options.menu_item_kind,
        .role = options.menu_item_role,
        .width = options.menu_width,
        .height = options.item_height});
    style.text_align = TextAlign::Start;
    return style;
}

inline ButtonStyleOptions dropdown_menu_item_style(
        layout::GlassEffectStyle glass,
        GlassDropdownStyleOptions const& options,
        bool selected) {
    if (selected) {
        return glass_selection_button_style(
            glass,
            GlassSelectionStyleOptions{
                .role = options.menu_item_role,
                .selected = true,
                .width = options.menu_width,
                .height = options.item_height,
                .text_align = TextAlign::Start});
    }

    auto style = glass_menu_item_button_style(
        glass,
        GlassMenuItemStyleOptions{
            .kind = options.menu_item_kind,
            .role = options.menu_item_role,
            .width = options.menu_width,
            .height = options.item_height});
    style.text_align = TextAlign::Start;
    return style;
}

template<typename Msg, typename ButtonStyle, typename MenuItemStyle>
inline void glass_dropdown_button_impl(str label,
                                       std::vector<str> const& items,
                                       std::size_t selected,
                                       Msg (*on_select)(std::size_t),
                                       GlassDropdownStyleOptions options,
                                       ButtonStyle&& button_style,
                                       MenuItemStyle&& menu_item_style) {
    auto& open = framework_local<bool>(false);
    auto* open_ptr = &open;
    bool const disabled = options.disabled || items.empty();

    auto const display = dropdown_button_label(label, items, selected);
    action_button(
        str(display),
        button_style(open, disabled),
        [open_ptr] {
            *open_ptr = !*open_ptr;
            detail::trigger_rebuild();
        });

    if (!open || disabled || on_select == nullptr)
        return;

    layout::context_menu_overlay([&] {
        for (std::size_t i = 0; i < items.size(); ++i) {
            auto const item_label = std::string(items[i].data, items[i].len);
            action_button(
                str(item_label),
                menu_item_style(i == selected),
                [open_ptr, on_select, idx = i] {
                    *open_ptr = false;
                    detail::post<Msg>(on_select(idx));
                    detail::trigger_rebuild();
                });
        }
    }, options.menu_width, options.top_padding, options.semantic_label);
}

} // namespace _impl

template<typename Msg>
inline void glass_dropdown_button(str label,
                                  std::vector<str> const& items,
                                  std::size_t selected,
                                  Msg (*on_select)(std::size_t),
                                  GlassDropdownStyleOptions options = {}) {
    _impl::glass_dropdown_button_impl<Msg>(
        label,
        items,
        selected,
        on_select,
        options,
        [&](bool open, bool disabled) {
            return _impl::dropdown_button_style(options, open, disabled);
        },
        [&](bool is_selected) {
            return _impl::dropdown_menu_item_style(options, is_selected);
        });
}

template<typename Msg>
inline void glass_dropdown_button(str label,
                                  std::vector<str> const& items,
                                  std::size_t selected,
                                  Msg (*on_select)(std::size_t),
                                  layout::GlassEffectStyle glass,
                                  GlassDropdownStyleOptions options = {}) {
    _impl::glass_dropdown_button_impl<Msg>(
        label,
        items,
        selected,
        on_select,
        options,
        [&](bool open, bool disabled) {
            return _impl::dropdown_button_style(glass, options, open, disabled);
        },
        [&](bool is_selected) {
            return _impl::dropdown_menu_item_style(glass, options, is_selected);
        });
}

template<typename Msg>
inline void glass_table_header_button(str label,
                                      Msg msg,
                                      GlassTableHeaderStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_table_header_button_style(options));
}

template<typename Msg>
inline void glass_table_header_button(str label,
                                      Msg msg,
                                      layout::GlassEffectStyle glass,
                                      GlassTableHeaderStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_table_header_button_style(glass, options));
}

template<typename Msg>
inline void glass_disclosure_header_button(
        str label,
        Msg msg,
        GlassDisclosureStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_disclosure_header_style(options));
}

template<typename Msg>
inline void glass_disclosure_header_button(
        str label,
        Msg msg,
        layout::GlassEffectStyle glass,
        GlassDisclosureStyleOptions options = {}) {
    button(
        label,
        std::move(msg),
        glass_disclosure_header_style(glass, options));
}

template<typename Msg>
inline void glass_text_field(str hint,
                             std::string const& current,
                             Msg(*mapper)(std::string),
                             layout::GlassEffectStyle glass,
                             GlassTextFieldStyleOptions options = {}) {
    text_field<Msg>(
        hint,
        current,
        mapper,
        glass_text_field_style(glass, options));
}

template<typename Msg>
inline void glass_checkbox(str label,
                           bool checked,
                           Msg msg,
                           layout::GlassEffectStyle glass,
                           GlassToggleStyleOptions options = {}) {
    auto style = glass_toggle_style_options(glass, options);
    _impl::toggle(
        label,
        checked,
        std::move(msg),
        detail::g_app.theme.radius_xs,
        Decoration::Check,
        InteractionRole::Checkbox,
        &style);
}

template<typename Msg>
inline void glass_radio(str label,
                        bool selected,
                        Msg msg,
                        layout::GlassEffectStyle glass,
                        GlassToggleStyleOptions options = {}) {
    auto style = glass_toggle_style_options(glass, options);
    _impl::toggle(
        label,
        selected,
        std::move(msg),
        detail::g_app.theme.radius_lg,
        Decoration::Dot,
        InteractionRole::Radio,
        &style);
}

template<typename Msg>
inline void glass_switch(str label,
                         bool on,
                         Msg msg,
                         layout::GlassEffectStyle glass,
                         GlassSwitchStyleOptions options = {}) {
    auto style = glass_switch_style_options(glass, options);
    _impl::switch_control(label, on, std::move(msg), &style);
}

template<typename Msg>
inline void glass_tabs(std::vector<str> const& items,
                       std::size_t selected,
                       Msg (*on_select)(std::size_t),
                       layout::GlassEffectStyle glass,
                       TabsStyleOptions options = {}) {
    tabs<Msg>(
        items,
        selected,
        on_select,
        glass_tabs_style_options(glass, options));
}

template<typename Msg>
inline void tabs(std::vector<str> const& items,
                 std::size_t selected,
                 Msg (*on_select)(std::size_t),
                 layout::GlassEffectStyle glass,
                 TabsStyleOptions options = {}) {
    glass_tabs<Msg>(
        items,
        selected,
        on_select,
        glass,
        options);
}

} // namespace widget

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

inline FocusVisibilityReason resolved_focus_visibility_reason(
        unsigned int callback_id,
        bool focus_visible,
        InputModality modality) {
    if (callback_id == 0xFFFFFFFFu)
        return FocusVisibilityReason::NoFocus;
    if (focus_visible)
        return FocusVisibilityReason::KeyboardFocusNavigation;
    if (modality == InputModality::Pointer)
        return FocusVisibilityReason::PointerInputHidesFocusRing;
    return FocusVisibilityReason::ProgrammaticFocusHidden;
}

inline void assign_focus(
        unsigned int callback_id,
        bool focus_visible = false,
        InputModality modality = InputModality::Programmatic) {
    g_app.focused_id = callback_id;
    g_app.focus_visible = callback_id != 0xFFFFFFFFu && focus_visible;
    if (callback_id == 0xFFFFFFFFu)
        g_app.focus_input_modality = InputModality::None;
    else
        g_app.focus_input_modality = modality;
    g_app.focus_visibility_reason = resolved_focus_visibility_reason(
        callback_id,
        g_app.focus_visible,
        g_app.focus_input_modality);
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
    snapshot.focus_visible = g_app.focus_visible;
    snapshot.input_modality =
        input_modality_name(g_app.focus_input_modality);
    snapshot.focus_visibility_reason =
        focus_visibility_reason_name(g_app.focus_visibility_reason);
    snapshot.hovered_id = g_app.hovered_id;
    snapshot.pressed_id = g_app.pressed_id;
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

inline bool toggle_debug_panel(char const* source = "core",
                               char const* detail = "f12") {
#ifndef NDEBUG
    g_app.debug_panel_open = !g_app.debug_panel_open;
    g_app.debug_panel_warmup_frames = g_app.debug_panel_open ? 4u : 0u;
    g_app.last_paint_hash = 0;
    note_input_event("key", source, detail, "handled", 0xFFFFFFFFu);
    trigger_rebuild();
    return true;
#else
    (void)source;
    (void)detail;
    return false;
#endif
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

inline bool set_pressed_id(unsigned int callback_id,
                           char const* source = "core",
                           char const* detail = "pointer-press") {
    if (g_app.pressed_id == callback_id) {
        note_input_event("press", source, detail, "ignored", callback_id);
        return false;
    }
    g_app.pressed_id = callback_id;
    note_input_event("press", source, detail, "handled", callback_id);
    return true;
}

inline bool set_focus_id(unsigned int callback_id,
                         char const* source = "core",
                         char const* detail = "focus-change",
                         bool focus_visible = false,
                         InputModality modality = InputModality::Programmatic) {
    bool const next_focus_visible =
        callback_id != 0xFFFFFFFFu && focus_visible;
    auto const next_modality = callback_id == 0xFFFFFFFFu
        ? InputModality::None
        : (next_focus_visible && modality == InputModality::Programmatic
              ? InputModality::Keyboard
              : modality);
    auto const next_reason = resolved_focus_visibility_reason(
        callback_id,
        next_focus_visible,
        next_modality);
    if (g_app.focused_id == callback_id
        && g_app.focus_visible == next_focus_visible
        && g_app.focus_input_modality == next_modality
        && g_app.focus_visibility_reason == next_reason) {
        note_input_event("focus", source, detail, "ignored", callback_id);
        return false;
    }
    if (g_app.focused_id == callback_id) {
        g_app.focus_visible = next_focus_visible;
        g_app.focus_input_modality = next_modality;
        g_app.focus_visibility_reason = next_reason;
        sync_input_debug_caret_state();
    } else {
        assign_focus(callback_id, next_focus_visible, next_modality);
    }
    note_input_event("focus", source, detail, "handled", callback_id);
    return true;
}

inline bool clear_focus_visible_for_pointer(
        char const* source = "core",
        char const* detail = "pointer-focus-visible-reset") {
    if (!g_app.focus_visible || g_app.focused_id == 0xFFFFFFFFu)
        return false;
    return set_focus_id(
        g_app.focused_id,
        source,
        detail,
        false,
        InputModality::Pointer);
}

inline unsigned int get_focused_id() {
    return g_app.focused_id;
}

inline unsigned int get_pressed_id() {
    return g_app.pressed_id;
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
    assign_focus(g_app.focusable_ids[idx], true, InputModality::Keyboard);
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
    snapshot.focus_visible = g_app.focus_visible;
    snapshot.input_modality =
        input_modality_name(g_app.focus_input_modality);
    snapshot.focus_visibility_reason =
        focus_visibility_reason_name(g_app.focus_visibility_reason);
    snapshot.hovered_id = g_app.hovered_id;
    snapshot.pressed_id = g_app.pressed_id;
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
    auto const t0 = metrics::detail::now_ns();
    app.scroll_x = scroll_x;
    app.scroll_y = scroll_y;
    if (!app.root.valid()) return;
    auto* root_ptr = app.arena.get(app.root);
    if (!root_ptr) return;
    float cw = host.canvas_width();
    auto t1 = t0;
    if (cw != root_ptr->width) {
        layout_node(host, app.root, cw);
        for (auto overlay_h : app.overlays)
            layout_node(host, overlay_h, cw);
        t1 = metrics::detail::now_ns();
    }
    app.focusable_ids.clear();
    collect_focusable_ids(app.root);
    for (auto overlay_h : app.overlays)
        collect_focusable_ids(overlay_h);
    app.paint_invalidation_mask = compute_paint_invalidation_mask(app);
    app.paint_scissor_depth = 0;
    app.scrollbar_animation_active = false;
    emit_clear(host, app.theme.background);
    float vh = host.canvas_height();
    app.debug_viewport_width = cw;
    app.debug_viewport_height = vh;
    paint_node(host, host, app.root, 0, 0, scroll_x, scroll_y, cw, vh);
    reset_paint_scissor_boundary(host);
    paint_root_vertical_scroll_bar(host, cw, vh);
    for (auto overlay_h : app.overlays)
        paint_node(host, host, overlay_h, 0, 0, 0.0f, 0.0f, cw, vh);
    auto const t2 = metrics::detail::now_ns();
    flush_if_changed(host);
    auto const t3 = metrics::detail::now_ns();
    record_frame_trace(
        FrameTraceSample{
            .total_ns = t3 - t0,
            .layout_ns = t1 - t0,
            .paint_ns = t2 - t1,
            .flush_ns = t3 - t2,
            .rebuild = false,
        },
        t0);
    persist_paint_inputs(app);
}
#else
inline void repaint(float scroll_x, float scroll_y) {
    auto& app = g_app;
    auto const t0 = metrics::detail::now_ns();
    app.scroll_x = scroll_x;
    app.scroll_y = scroll_y;
    if (!app.root.valid()) return;
    auto* root_ptr = app.arena.get(app.root);
    if (!root_ptr) return;
    float cw = phenotype_get_canvas_width();
    auto t1 = t0;
    if (cw != root_ptr->width) {
        layout_node(app.root, cw);
        for (auto overlay_h : app.overlays)
            layout_node(overlay_h, cw);
        t1 = metrics::detail::now_ns();
    }
    app.focusable_ids.clear();
    collect_focusable_ids(app.root);
    for (auto overlay_h : app.overlays)
        collect_focusable_ids(overlay_h);
    app.paint_invalidation_mask = compute_paint_invalidation_mask(app);
    app.paint_scissor_depth = 0;
    app.scrollbar_animation_active = false;
    wasi_emit_clear(app.theme.background);
    float vh = phenotype_get_canvas_height();
    app.debug_viewport_width = cw;
    app.debug_viewport_height = vh;
    wasi_paint_node(app.root, 0, 0, scroll_x, scroll_y, cw, vh);
    wasi_reset_paint_scissor_boundary();
    wasi_paint_root_vertical_scroll_bar(cw, vh);
    for (auto overlay_h : app.overlays)
        wasi_paint_node(overlay_h, 0, 0, 0.0f, 0.0f, cw, vh);
    auto const t2 = metrics::detail::now_ns();
    wasi_flush_if_changed();
    auto const t3 = metrics::detail::now_ns();
    record_frame_trace(
        FrameTraceSample{
            .total_ns = t3 - t0,
            .layout_ns = t1 - t0,
            .paint_ns = t2 - t1,
            .flush_ns = t3 - t2,
            .rebuild = false,
        },
        t0);
    persist_paint_inputs(app);
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

inline void collect_semantic_nodes_from(
        Arena& arena,
        NodeHandle node_h,
        float ox,
        float oy,
        std::vector<diag::SemanticNodeSnapshot>& out,
        bool screen_fixed = false) {
    auto* node_ptr = arena.get(node_h);
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
            collect_semantic_nodes_from(
                arena,
                child_h,
                ax,
                ay,
                out,
                screen_fixed);
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
            .prominence = node.material.prominence,
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
        collect_semantic_nodes_from(
            arena,
            child_h,
            ax,
            ay,
            semantic.children,
            screen_fixed);
    out.push_back(std::move(semantic));
}

inline void collect_semantic_nodes(
        NodeHandle node_h,
        float ox,
        float oy,
        std::vector<diag::SemanticNodeSnapshot>& out,
        bool screen_fixed = false) {
    collect_semantic_nodes_from(
        g_app.arena,
        node_h,
        ox,
        oy,
        out,
        screen_fixed);
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
    runtime.focus_visible = g_app.focus_visible;
    runtime.input_modality = input_modality_name(g_app.focus_input_modality);
    runtime.focus_visibility_reason =
        focus_visibility_reason_name(g_app.focus_visibility_reason);
    runtime.hovered_callback_id = optional_callback_id(g_app.hovered_id);
    runtime.pressed_callback_id = optional_callback_id(g_app.pressed_id);
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

#ifndef NDEBUG
constexpr float DEBUG_PANEL_BODY_WIDTH = 424.0f;
constexpr float DEBUG_PERFORMANCE_CHART_HEIGHT = 192.0f;
constexpr float DEBUG_LAYOUT_OVERVIEW_HEIGHT = 220.0f;
constexpr float DEBUG_BOX_MODEL_HEIGHT = 156.0f;
constexpr float DEBUG_VIRTUAL_INPUT_HEIGHT = 160.0f;
constexpr unsigned int DEBUG_INVALID_ID = 0xFFFFFFFFu;

struct DebugPanelRect {
    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct DebugTextBlock {
    std::string text;

    void line(std::string_view key, std::string_view value) {
        if (!text.empty())
            text += "\n";
        text += key;
        text += ": ";
        text += value;
    }

    void line(std::string_view key, char const* value) {
        line(key, std::string_view{value});
    }

    void line(std::string_view key, std::string const& value) {
        line(key, std::string_view{value});
    }

    void line(std::string_view key, bool value) {
        line(key, value ? "true" : "false");
    }

    template<typename T>
        requires std::integral<T> && (!std::same_as<std::remove_cv_t<T>, bool>)
    void line(std::string_view key, T value) {
        line(key, std::to_string(value));
    }

    void line_ms(std::string_view key, std::uint64_t ns) {
        line(key, std::format("{:.3f}", static_cast<double>(ns) / 1'000'000.0));
    }
};

inline bool find_callback_rect(NodeHandle node_h,
                               unsigned int callback_id,
                               float ox,
                               float oy,
                               float scroll_x,
                               float scroll_y,
                               DebugPanelRect& out) {
    if (!node_h.valid())
        return false;
    auto& node = node_at(node_h);
    float ax = ox + node.x;
    float ay = oy + node.y;
    if (node.callback_id == callback_id) {
        out = DebugPanelRect{
            true,
            ax - scroll_x,
            ay - scroll_y,
            node.width,
            node.height,
        };
        return true;
    }

    float child_scroll_x = scroll_x;
    float child_scroll_y = scroll_y;
    if (node.is_scroll_container && node.scroll_state)
        child_scroll_y = scroll_y + node.scroll_offset_y;
    for (auto child_h : node.children) {
        if (find_callback_rect(
                child_h,
                callback_id,
                ax,
                ay,
                child_scroll_x,
                child_scroll_y,
                out)) {
            return true;
        }
    }
    return false;
}

inline DebugPanelRect callback_rect(unsigned int callback_id) {
    if (callback_id == DEBUG_INVALID_ID)
        return {};
    DebugPanelRect out;
    (void)find_callback_rect(
        g_app.root,
        callback_id,
        0.0f,
        0.0f,
        g_app.scroll_x,
        g_app.scroll_y,
        out);
    return out;
}

inline void paint_debug_hover_highlight(Painter& painter, unsigned int callback_id) {
    auto rect = callback_rect(callback_id);
    if (!rect.valid || rect.w <= 0.0f || rect.h <= 0.0f)
        return;

    PaintRect fill{
        rect.x,
        rect.y,
        rect.w,
        rect.h,
        Color{59, 130, 246, 34},
    };
    painter.fill_rects(&fill, 1);

    constexpr float t = 2.0f;
    PaintRect border[] = {
        {rect.x, rect.y, rect.w, t, Color{37, 99, 235, 210}},
        {rect.x, rect.y + rect.h - t, rect.w, t, Color{37, 99, 235, 210}},
        {rect.x, rect.y, t, rect.h, Color{37, 99, 235, 210}},
        {rect.x + rect.w - t, rect.y, t, rect.h, Color{37, 99, 235, 210}},
    };
    painter.fill_rects(border, 4);
}

inline std::string debug_id_text(unsigned int id) {
    if (id == DEBUG_INVALID_ID)
        return "none";
    return std::to_string(id);
}

inline std::string debug_caret_text(unsigned int pos) {
    if (pos == DEBUG_INVALID_ID)
        return "none";
    return std::to_string(pos);
}

inline std::string debug_rect_text(diag::RectSnapshot const& rect) {
    if (!rect.valid)
        return "(invalid)";
    return "(" + std::to_string(rect.x)
        + ", " + std::to_string(rect.y)
        + ", " + std::to_string(rect.w)
        + ", " + std::to_string(rect.h) + ")";
}

inline std::string debug_panel_shortcut_text() {
#ifdef __APPLE__
    return "Cmd+F12";
#else
    return "Ctrl+F12";
#endif
}

inline Color debug_with_alpha(Color color, unsigned char alpha) noexcept {
    color.a = alpha;
    return color;
}

inline char const* debug_panel_tab_label(DebugPanelTab tab) noexcept {
    switch (tab) {
        case DebugPanelTab::Performance: return "Performance";
        case DebugPanelTab::Layout:      return "Layout";
        case DebugPanelTab::Console:     return "Console";
        case DebugPanelTab::Input:       return "Input";
        case DebugPanelTab::Protocol:    return "Protocol";
    }
    return "Performance";
}

inline void set_debug_panel_tab(DebugPanelTab tab) {
    if (g_app.debug_panel_tab == tab)
        return;
    g_app.debug_panel_tab = tab;
    g_app.debug_panel_warmup_frames = 3u;
    g_app.last_paint_hash = 0;
    trigger_rebuild();
}

inline ButtonStyleOptions debug_panel_button_options(bool selected,
                                                     float fixed_height,
                                                     unsigned char idle_tint = 70,
                                                     unsigned char idle_border = 130) {
    auto const& t = g_app.theme;
    ButtonStyleOptions options{};
    options.has_material = true;
    options.material = layout::material_style(
        selected ? MaterialKind::Regular : MaterialKind::Clear);
    options.material.role = MaterialSurfaceRole::Control;
    options.material.tint = selected
        ? debug_with_alpha(t.accent, 218)
        : debug_with_alpha(t.surface, idle_tint);
    options.material.border = selected
        ? debug_with_alpha(t.accent_strong, 230)
        : debug_with_alpha(t.border, idle_border);
    options.has_background = true;
    options.background = options.material.tint;
    options.has_hover_background = true;
    options.hover_background = selected
        ? debug_with_alpha(t.accent_strong, 230)
        : debug_with_alpha(t.state_hover_bg, 190);
    options.has_pressed_background = true;
    options.pressed_background = selected
        ? debug_with_alpha(t.accent_strong, 245)
        : debug_with_alpha(t.state_hover_bg, 230);
    options.has_border_color = true;
    options.border_color = options.material.border;
    options.has_text_color = true;
    options.text_color = selected ? t.state_active_fg : t.foreground;
    options.border_radius = t.radius_sm;
    options.border_width = selected ? 1.0f : 0.5f;
    options.font_size = t.small_font_size;
    options.padding_top = 6.0f;
    options.padding_bottom = 6.0f;
    options.padding_left = 10.0f;
    options.padding_right = 10.0f;
    options.fixed_height = fixed_height;
    options.text_align = TextAlign::Center;
    return options;
}

inline void debug_panel_tab_button(DebugPanelTab tab) {
    bool const selected = g_app.debug_panel_tab == tab;
    auto const* label = debug_panel_tab_label(tab);
    widget::_impl::action_button(
        str{label, static_cast<unsigned int>(std::strlen(label))},
        debug_panel_button_options(selected, 34.0f),
        [tab] { set_debug_panel_tab(tab); });
}

inline void debug_panel_tab_bar() {
    layout::row([&] {
        debug_panel_tab_button(DebugPanelTab::Performance);
        debug_panel_tab_button(DebugPanelTab::Layout);
        debug_panel_tab_button(DebugPanelTab::Console);
        debug_panel_tab_button(DebugPanelTab::Input);
        debug_panel_tab_button(DebugPanelTab::Protocol);
    }, SpaceToken::Xs, CrossAxisAlignment::Center, MainAxisAlignment::Start);
}

template<typename F>
    requires std::is_invocable_v<F>
inline void debug_panel_action_button(char const* label, F&& action) {
    widget::_impl::action_button(
        str{label, static_cast<unsigned int>(std::strlen(label))},
        debug_panel_button_options(false, 30.0f, 80, 150),
        std::forward<F>(action));
}

inline std::string debug_protocol_block() {
    DebugTextBlock block;
    block.line("protocol_version", io::debug_protocol_contract_version);
    block.line("transport", io::debug_transport_policy());
    block.line("security", io::debug_security_policy());
    block.line("panel", io::debug_side_panel_policy());
    block.line("agent_control", io::debug_agent_control_policy());
    block.line("domains", io::debug_protocol_domains.size());
    block.line("commands", io::debug_protocol_commands.size());
    block.line("sections", io::debug_panel_sections.size());
    block.line("agent_complete", io::debug_protocol_is_agent_complete());
    return block.text;
}

inline std::string input_debug_block(diag::InputDebugSnapshot const& debug) {
    DebugTextBlock block;
    block.line("last_event", debug.event.empty() ? "none" : debug.event);
    block.line("source", debug.source.empty() ? "none" : debug.source);
    block.line("detail", debug.detail.empty() ? "none" : debug.detail);
    block.line("result", debug.result.empty() ? "none" : debug.result);
    block.line("callback_id", debug_id_text(debug.callback_id));
    block.line("role", debug.role);
    block.line("focused_id", debug_id_text(debug.focused_id));
    block.line("focused_role", debug.focused_role);
    block.line("focus_visible", debug.focus_visible);
    block.line("hovered_id", debug_id_text(debug.hovered_id));
    block.line("pressed_id", debug_id_text(debug.pressed_id));
    block.line("scroll_y", std::to_string(debug.scroll_y));
    block.line("text caret", debug_caret_text(debug.caret_pos));
    block.line("selection active", debug.selection_active);
    block.line("selection start", debug_caret_text(debug.selection_start));
    block.line("selection end", debug_caret_text(debug.selection_end));
    block.line("caret visible", debug.caret_visible);
    block.line("caret renderer", debug.caret_renderer.empty() ? "hidden" : debug.caret_renderer);
    block.line("caret geometry source", debug.caret_geometry_source.empty() ? "draw" : debug.caret_geometry_source);
    block.line("caret rect", debug_rect_text(debug.caret_rect));
    block.line("caret draw rect", debug_rect_text(debug.caret_draw_rect));
    block.line("caret host rect", debug_rect_text(debug.caret_host_rect));
    block.line("caret screen rect", debug_rect_text(debug.caret_screen_rect));
    block.line("composition active", debug.composition_active);
    block.line("composition cursor", std::to_string(debug.composition_cursor));
    block.line("composition text", debug.composition_text.empty() ? "(empty)" : debug.composition_text);
    return block.text;
}

inline std::optional<diag::SemanticNodeSnapshot>
debug_panel_app_semantic_tree_snapshot() {
    std::vector<diag::SemanticNodeSnapshot> nodes;
    if (g_app.prev_root.valid()) {
        collect_semantic_nodes_from(
            g_app.prev_arena,
            g_app.prev_root,
            0.0f,
            0.0f,
            nodes);
    } else {
        collect_semantic_nodes(g_app.root, 0.0f, 0.0f, nodes);
    }
    if (nodes.empty())
        return std::nullopt;
    return std::move(nodes.front());
}

inline unsigned int debug_semantic_node_count(
        diag::SemanticNodeSnapshot const& node) {
    unsigned int count = 1;
    for (auto const& child : node.children)
        count += debug_semantic_node_count(child);
    return count;
}

inline diag::SemanticNodeSnapshot const* find_semantic_node_by_callback(
        diag::SemanticNodeSnapshot const& node,
        unsigned int callback_id) {
    if (callback_id == DEBUG_INVALID_ID)
        return nullptr;
    if (node.callback_id.has_value() && *node.callback_id == callback_id)
        return &node;
    for (auto const& child : node.children) {
        if (auto const* found =
                find_semantic_node_by_callback(child, callback_id)) {
            return found;
        }
    }
    return nullptr;
}

inline std::string debug_compact_label(std::string label,
                                       std::size_t limit = 42) {
    for (auto& ch : label) {
        if (ch == '\n' || ch == '\r' || ch == '\t')
            ch = ' ';
    }
    if (label.size() <= limit)
        return label;
    return label.substr(0, limit > 3 ? limit - 3 : limit) + "...";
}

inline std::string debug_semantic_node_title(
        diag::SemanticNodeSnapshot const& node) {
    std::string out = node.role.empty() ? "node" : node.role;
    if (!node.label.empty()) {
        out += " \"";
        out += debug_compact_label(node.label);
        out += "\"";
    }
    if (node.callback_id.has_value()) {
        out += " #";
        out += std::to_string(*node.callback_id);
    }
    return out;
}

inline std::string debug_semantic_node_block(
        diag::SemanticNodeSnapshot const* node) {
    if (!node)
        return "hovered_node: none\nbox_model: unavailable";

    std::string block;
    block += "node: ";
    block += debug_semantic_node_title(*node);
    block += "\nbounds: ";
    block += debug_rect_text(node->bounds);
    block += "\nvisible: ";
    block += node->visible ? "true" : "false";
    block += "\nenabled: ";
    block += node->enabled ? "true" : "false";
    block += "\nfocusable: ";
    block += node->focusable ? "true" : "false";
    block += "\nfocused: ";
    block += node->focused ? "true" : "false";
    block += "\nchildren: ";
    block += std::to_string(node->children.size());
    if (node->material.has_value()) {
        block += "\nmaterial: ";
        block += node->material->kind;
        block += "/";
        block += node->material->role;
        block += " blur=";
        block += std::format("{:.1f}", node->material->blur_radius);
    }
    return block;
}

inline void debug_stroke_rect(Painter& painter,
                              float x,
                              float y,
                              float w,
                              float h,
                              float thickness,
                              Color color) {
    PaintRect rects[] = {
        {x, y, w, thickness, color},
        {x, y + h - thickness, w, thickness, color},
        {x, y, thickness, h, color},
        {x + w - thickness, y, thickness, h, color},
    };
    painter.fill_rects(rects, 4);
}

inline std::uint64_t debug_layout_diagram_token(
        diag::InputDebugSnapshot const& debug,
        diag::SemanticNodeSnapshot const* root) noexcept {
    std::uint64_t token = 0x646562756c61796fULL;
    token = static_cast<std::uint64_t>(hash_combine(token, debug.hovered_id));
    token = static_cast<std::uint64_t>(hash_combine(token, debug.focused_id));
    token = static_cast<std::uint64_t>(
        hash_combine(token, static_cast<unsigned int>(g_app.overlays.size())));
    token = static_cast<std::uint64_t>(
        hash_combine(token, static_cast<unsigned int>(g_app.debug_viewport_width)));
    token = static_cast<std::uint64_t>(
        hash_combine(token, static_cast<unsigned int>(g_app.debug_viewport_height)));
    if (root) {
        token = static_cast<std::uint64_t>(
            hash_combine(token, debug_semantic_node_count(*root)));
        token = static_cast<std::uint64_t>(
            hash_combine(token, static_cast<unsigned int>(root->bounds.valid)));
        token = static_cast<std::uint64_t>(
            hash_combine(token, static_cast<unsigned int>(root->bounds.x)));
        token = static_cast<std::uint64_t>(
            hash_combine(token, static_cast<unsigned int>(root->bounds.y)));
        token = static_cast<std::uint64_t>(
            hash_combine(token, static_cast<unsigned int>(root->bounds.w)));
        token = static_cast<std::uint64_t>(
            hash_combine(token, static_cast<unsigned int>(root->bounds.h)));
    }
    return token;
}

inline void paint_debug_layout_node_rects(
        Painter& painter,
        diag::SemanticNodeSnapshot const& node,
        float offset_x,
        float offset_y,
        float scale,
        unsigned int hovered_id,
        unsigned int focused_id,
        unsigned int depth,
        unsigned int& remaining) {
    if (remaining == 0)
        return;
    --remaining;
    if (node.bounds.valid && node.visible
        && node.bounds.w > 0.0f && node.bounds.h > 0.0f) {
        float const x = offset_x + node.bounds.x * scale;
        float const y = offset_y + node.bounds.y * scale;
        float const w = std::max(1.0f, node.bounds.w * scale);
        float const h = std::max(1.0f, node.bounds.h * scale);
        bool const hovered = node.callback_id.has_value()
            && *node.callback_id == hovered_id;
        bool const focused = node.callback_id.has_value()
            && *node.callback_id == focused_id;
        auto const& t = g_app.theme;
        Color fill = debug_with_alpha(
            depth % 2 == 0 ? t.accent : t.semantic_success_border,
            hovered ? 76 : 22);
        PaintRect bg{x, y, w, h, fill};
        painter.fill_rects(&bg, 1);
        debug_stroke_rect(
            painter,
            x,
            y,
            w,
            h,
            hovered ? 2.0f : 1.0f,
            hovered
                ? debug_with_alpha(t.accent_strong, 235)
                : (focused
                    ? debug_with_alpha(t.semantic_warning_border, 210)
                    : debug_with_alpha(t.border, 120)));
        if (hovered || (depth <= 1 && w > 36.0f && h > 14.0f)) {
            auto label = debug_compact_label(
                node.role.empty() ? "node" : node.role,
                18);
            painter.text(
                x + 4.0f,
                y + 3.0f,
                label.c_str(),
                static_cast<unsigned int>(label.size()),
                9.0f,
                hovered ? t.foreground : t.muted);
        }
    }
    for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
        paint_debug_layout_node_rects(
            painter,
            *it,
            offset_x,
            offset_y,
            scale,
            hovered_id,
            focused_id,
            depth + 1,
            remaining);
    }
}

inline void paint_debug_layout_diagram(
        Painter& painter,
        std::optional<diag::SemanticNodeSnapshot> root,
        diag::InputDebugSnapshot debug) {
    auto const& t = g_app.theme;
    constexpr float width = DEBUG_PANEL_BODY_WIDTH;
    constexpr float height = DEBUG_LAYOUT_OVERVIEW_HEIGHT;
    PaintRect bg{0.0f, 0.0f, width, height, debug_with_alpha(t.code_bg, 156)};
    painter.fill_rects(&bg, 1);
    if (!root || !root->bounds.valid) {
        char const* text = "No layout tree";
        painter.text(16.0f, 18.0f, text, static_cast<unsigned int>(std::strlen(text)),
                     12.0f, t.muted);
        return;
    }

    float const viewport_w = std::max(g_app.debug_viewport_width, root->bounds.w);
    float const viewport_h = std::max(g_app.debug_viewport_height, root->bounds.h);
    float const scale = std::min(
        (width - 32.0f) / std::max(1.0f, viewport_w),
        (height - 38.0f) / std::max(1.0f, viewport_h));
    float const diagram_w = viewport_w * scale;
    float const diagram_h = viewport_h * scale;
    float const offset_x = (width - diagram_w) * 0.5f;
    float const offset_y = 28.0f;

    debug_stroke_rect(
        painter,
        offset_x,
        offset_y,
        diagram_w,
        diagram_h,
        1.0f,
        debug_with_alpha(t.border, 160));
    painter.text(12.0f, 8.0f, "viewport", 8, 11.0f, t.muted);
    auto viewport_text = std::format(
        "{:.0f} x {:.0f}",
        viewport_w,
        viewport_h);
    auto viewport_text_w = painter.measure_text(
        viewport_text.c_str(),
        static_cast<unsigned int>(viewport_text.size()),
        11.0f);
    painter.text(
        width - 12.0f - viewport_text_w,
        8.0f,
        viewport_text.c_str(),
        static_cast<unsigned int>(viewport_text.size()),
        11.0f,
        t.muted);

    unsigned int remaining = 96;
    paint_debug_layout_node_rects(
        painter,
        *root,
        offset_x,
        offset_y,
        scale,
        debug.hovered_id,
        debug.focused_id,
        0,
        remaining);
}

inline void paint_debug_box_model_diagram(
        Painter& painter,
        diag::SemanticNodeSnapshot const* node) {
    auto const& t = g_app.theme;
    constexpr float width = DEBUG_PANEL_BODY_WIDTH;
    constexpr float height = DEBUG_BOX_MODEL_HEIGHT;
    PaintRect bg{0.0f, 0.0f, width, height, debug_with_alpha(t.code_bg, 156)};
    painter.fill_rects(&bg, 1);
    if (!node || !node->bounds.valid) {
        char const* text = "Hover an app element";
        painter.text(16.0f, 18.0f, text, static_cast<unsigned int>(std::strlen(text)),
                     12.0f, t.muted);
        return;
    }

    float const outer_x = 42.0f;
    float const outer_y = 18.0f;
    float const outer_w = width - 84.0f;
    float const outer_h = height - 42.0f;
    PaintRect margin{outer_x, outer_y, outer_w, outer_h,
                     debug_with_alpha(t.semantic_warning_border, 40)};
    PaintRect border{outer_x + 22.0f, outer_y + 18.0f,
                     outer_w - 44.0f, outer_h - 36.0f,
                     debug_with_alpha(t.accent, 46)};
    PaintRect padding{outer_x + 48.0f, outer_y + 36.0f,
                      outer_w - 96.0f, outer_h - 72.0f,
                      debug_with_alpha(t.semantic_success_border, 50)};
    PaintRect content{outer_x + 78.0f, outer_y + 52.0f,
                      outer_w - 156.0f, outer_h - 104.0f,
                      debug_with_alpha(t.surface, 210)};
    PaintRect rects[] = {margin, border, padding, content};
    painter.fill_rects(rects, 4);
    debug_stroke_rect(painter, margin.x, margin.y, margin.w, margin.h, 1.0f,
                      debug_with_alpha(t.semantic_warning_border, 190));
    debug_stroke_rect(painter, border.x, border.y, border.w, border.h, 1.0f,
                      debug_with_alpha(t.accent_strong, 190));
    debug_stroke_rect(painter, padding.x, padding.y, padding.w, padding.h, 1.0f,
                      debug_with_alpha(t.semantic_success_border, 190));
    debug_stroke_rect(painter, content.x, content.y, content.w, content.h, 1.0f,
                      debug_with_alpha(t.border, 220));
    painter.text(margin.x + 6.0f, margin.y + 4.0f, "margin", 6, 10.0f, t.muted);
    painter.text(border.x + 6.0f, border.y + 4.0f, "border", 6, 10.0f, t.muted);
    painter.text(padding.x + 6.0f, padding.y + 4.0f, "padding", 7, 10.0f, t.muted);
    auto label = std::format(
        "{:.0f} x {:.0f}",
        node->bounds.w,
        node->bounds.h);
    auto label_w = painter.measure_text(
        label.c_str(),
        static_cast<unsigned int>(label.size()),
        12.0f);
    painter.text(
        content.x + (content.w - label_w) * 0.5f,
        content.y + (content.h - 12.0f) * 0.5f,
        label.c_str(),
        static_cast<unsigned int>(label.size()),
        12.0f,
        t.foreground);
}

inline unsigned int debug_find_callback_at_point(
        NodeHandle node_h,
        float px,
        float py,
        float ox,
        float oy,
        float scroll_x,
        float scroll_y) {
    if (!node_h.valid())
        return DEBUG_INVALID_ID;
    auto& node = node_at(node_h);
    float ax = ox + node.x;
    float ay = oy + node.y;
    float draw_x = ax - scroll_x;
    float draw_y = ay - scroll_y;
    if (px < draw_x || px > draw_x + node.width
        || py < draw_y || py > draw_y + node.height) {
        return DEBUG_INVALID_ID;
    }

    float child_scroll_x = scroll_x;
    float child_scroll_y = scroll_y;
    if (node.is_scroll_container && node.scroll_state)
        child_scroll_y = scroll_y + node.scroll_offset_y;
    for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
        auto found = debug_find_callback_at_point(
            *it,
            px,
            py,
            ax,
            ay,
            child_scroll_x,
            child_scroll_y);
        if (found != DEBUG_INVALID_ID)
            return found;
    }
    return node.callback_id;
}

inline unsigned int debug_find_app_callback_at_point(float x, float y) {
    return debug_find_callback_at_point(
        g_app.root,
        x,
        y,
        0.0f,
        0.0f,
        g_app.scroll_x,
        g_app.scroll_y);
}

inline void debug_set_virtual_pointer(float x, float y) {
    g_app.debug_virtual_pointer_valid = true;
    g_app.debug_virtual_pointer_x = std::clamp(
        x,
        0.0f,
        std::max(0.0f, g_app.debug_viewport_width));
    g_app.debug_virtual_pointer_y = std::clamp(
        y,
        0.0f,
        std::max(0.0f, g_app.debug_viewport_height));
    g_app.debug_virtual_hit_id = debug_find_app_callback_at_point(
        g_app.debug_virtual_pointer_x,
        g_app.debug_virtual_pointer_y);
    set_pointer_position(
        g_app.debug_virtual_pointer_x,
        g_app.debug_virtual_pointer_y);
    trigger_rebuild();
}

inline void debug_center_virtual_pointer() {
    debug_set_virtual_pointer(
        std::max(1.0f, g_app.debug_viewport_width) * 0.5f,
        std::max(1.0f, g_app.debug_viewport_height) * 0.5f);
}

inline bool debug_set_virtual_pointer_to_semantic_node(
        diag::SemanticNodeSnapshot const& node) {
    if (!node.bounds.valid || node.bounds.w <= 0.0f || node.bounds.h <= 0.0f)
        return false;
    debug_set_virtual_pointer(
        node.bounds.x - g_app.scroll_x + node.bounds.w * 0.5f,
        node.bounds.y - g_app.scroll_y + node.bounds.h * 0.5f);
    return true;
}

inline bool debug_set_virtual_pointer_to_callback(unsigned int callback_id) {
    auto tree = debug_panel_app_semantic_tree_snapshot();
    if (!tree)
        return false;
    auto const* node = find_semantic_node_by_callback(*tree, callback_id);
    if (!node)
        return false;
    return debug_set_virtual_pointer_to_semantic_node(*node);
}

inline void debug_set_virtual_pointer_to_hovered() {
    if (!debug_set_virtual_pointer_to_callback(g_app.hovered_id))
        debug_center_virtual_pointer();
}

inline void debug_set_virtual_pointer_to_focused() {
    if (!debug_set_virtual_pointer_to_callback(g_app.focused_id))
        debug_center_virtual_pointer();
}

inline void debug_nudge_virtual_pointer(float dx, float dy) {
    if (!g_app.debug_virtual_pointer_valid)
        debug_center_virtual_pointer();
    else
        debug_set_virtual_pointer(
            g_app.debug_virtual_pointer_x + dx,
            g_app.debug_virtual_pointer_y + dy);
}

inline void debug_virtual_hover() {
    if (!g_app.debug_virtual_pointer_valid)
        debug_center_virtual_pointer();
    auto const hit = debug_find_app_callback_at_point(
        g_app.debug_virtual_pointer_x,
        g_app.debug_virtual_pointer_y);
    g_app.debug_virtual_hit_id = hit;
    set_hover_id(hit, "debug-panel", "virtual-hover");
    trigger_rebuild();
}

inline void debug_virtual_click() {
    if (!g_app.debug_virtual_pointer_valid)
        debug_center_virtual_pointer();
    auto const hit = debug_find_app_callback_at_point(
        g_app.debug_virtual_pointer_x,
        g_app.debug_virtual_pointer_y);
    g_app.debug_virtual_hit_id = hit;
    set_hover_id(hit, "debug-panel", "virtual-click-hover");
    if (hit != DEBUG_INVALID_ID) {
        set_pressed_id(hit, "debug-panel", "virtual-click-press");
        set_focus_id(
            hit,
            "debug-panel",
            "virtual-click-focus",
            false,
            InputModality::Programmatic);
        handle_event(hit, "debug-panel", "virtual-click");
    } else {
        set_focus_id(DEBUG_INVALID_ID, "debug-panel", "virtual-click-clear");
    }
    set_pressed_id(DEBUG_INVALID_ID, "debug-panel", "virtual-click-release");
    trigger_rebuild();
}

inline void debug_virtual_clear() {
    g_app.debug_virtual_pointer_valid = false;
    g_app.debug_virtual_hit_id = DEBUG_INVALID_ID;
    set_hover_id(DEBUG_INVALID_ID, "debug-panel", "virtual-clear");
    set_pressed_id(DEBUG_INVALID_ID, "debug-panel", "virtual-clear");
    trigger_rebuild();
}

inline std::uint64_t debug_virtual_input_token() noexcept {
    std::uint64_t token = 0x64656275696e7074ULL;
    token = static_cast<std::uint64_t>(
        hash_combine(token, g_app.debug_virtual_pointer_valid ? 1u : 0u));
    token = static_cast<std::uint64_t>(
        hash_combine(token, static_cast<unsigned int>(g_app.debug_virtual_pointer_x)));
    token = static_cast<std::uint64_t>(
        hash_combine(token, static_cast<unsigned int>(g_app.debug_virtual_pointer_y)));
    token = static_cast<std::uint64_t>(
        hash_combine(token, g_app.debug_virtual_hit_id));
    return token;
}

inline void paint_debug_virtual_input_target(
        Painter& painter,
        diag::SemanticNodeSnapshot const* node,
        float rect_x,
        float rect_y,
        float scale,
        Color color) {
    if (!node || !node->bounds.valid)
        return;
    float const x = rect_x + (node->bounds.x - g_app.scroll_x) * scale;
    float const y = rect_y + (node->bounds.y - g_app.scroll_y) * scale;
    float const w = std::max(1.0f, node->bounds.w * scale);
    float const h = std::max(1.0f, node->bounds.h * scale);
    debug_stroke_rect(painter, x, y, w, h, 1.5f, color);
}

inline void paint_debug_virtual_input_pad(
        Painter& painter,
        std::optional<diag::SemanticNodeSnapshot> semantic_tree,
        diag::InputDebugSnapshot input_debug) {
    auto const& t = g_app.theme;
    constexpr float width = DEBUG_PANEL_BODY_WIDTH;
    constexpr float height = DEBUG_VIRTUAL_INPUT_HEIGHT;
    PaintRect bg{0.0f, 0.0f, width, height, debug_with_alpha(t.code_bg, 156)};
    painter.fill_rects(&bg, 1);
    float const viewport_w = std::max(1.0f, g_app.debug_viewport_width);
    float const viewport_h = std::max(1.0f, g_app.debug_viewport_height);
    float const scale = std::min(
        (width - 28.0f) / viewport_w,
        (height - 28.0f) / viewport_h);
    float const rect_w = viewport_w * scale;
    float const rect_h = viewport_h * scale;
    float const rect_x = (width - rect_w) * 0.5f;
    float const rect_y = (height - rect_h) * 0.5f;
    debug_stroke_rect(
        painter,
        rect_x,
        rect_y,
        rect_w,
        rect_h,
        1.0f,
        debug_with_alpha(t.border, 180));
    painter.line(rect_x, rect_y + rect_h * 0.5f,
                 rect_x + rect_w, rect_y + rect_h * 0.5f,
                 1.0f, debug_with_alpha(t.border, 85));
    painter.line(rect_x + rect_w * 0.5f, rect_y,
                 rect_x + rect_w * 0.5f, rect_y + rect_h,
                 1.0f, debug_with_alpha(t.border, 85));
    auto const* root = semantic_tree ? &*semantic_tree : nullptr;
    auto const* hovered = root
        ? find_semantic_node_by_callback(*root, input_debug.hovered_id)
        : nullptr;
    auto const* focused = root
        ? find_semantic_node_by_callback(*root, input_debug.focused_id)
        : nullptr;
    auto const* virtual_hit = root
        ? find_semantic_node_by_callback(*root, g_app.debug_virtual_hit_id)
        : nullptr;
    paint_debug_virtual_input_target(
        painter,
        focused,
        rect_x,
        rect_y,
        scale,
        debug_with_alpha(t.semantic_warning_border, 210));
    paint_debug_virtual_input_target(
        painter,
        hovered,
        rect_x,
        rect_y,
        scale,
        debug_with_alpha(t.accent_strong, 210));
    paint_debug_virtual_input_target(
        painter,
        virtual_hit,
        rect_x,
        rect_y,
        scale,
        debug_with_alpha(t.semantic_success_border, 220));
    if (g_app.debug_virtual_pointer_valid) {
        float const px = rect_x + g_app.debug_virtual_pointer_x * scale;
        float const py = rect_y + g_app.debug_virtual_pointer_y * scale;
        Color pointer_color = g_app.debug_virtual_hit_id == DEBUG_INVALID_ID
            ? debug_with_alpha(t.semantic_warning_border, 235)
            : debug_with_alpha(t.accent_strong, 240);
        painter.line(px - 8.0f, py, px + 8.0f, py, 2.0f, pointer_color);
        painter.line(px, py - 8.0f, px, py + 8.0f, 2.0f, pointer_color);
        painter.arc(px, py, 7.0f, 0.0f, 6.2831853f, 1.5f, pointer_color);
    }
    auto label = std::format(
        "x {:.0f}  y {:.0f}  hit {}",
        g_app.debug_virtual_pointer_x,
        g_app.debug_virtual_pointer_y,
        debug_id_text(g_app.debug_virtual_hit_id));
    painter.text(
        10.0f,
        8.0f,
        label.c_str(),
        static_cast<unsigned int>(label.size()),
        10.0f,
        t.muted);
}

inline std::string virtual_input_debug_block() {
    DebugTextBlock block;
    block.line(
        "virtual_pointer",
        g_app.debug_virtual_pointer_valid ? "active" : "inactive");
    block.line("x", std::format("{:.1f}", g_app.debug_virtual_pointer_x));
    block.line("y", std::format("{:.1f}", g_app.debug_virtual_pointer_y));
    block.line("hit_callback_id", debug_id_text(g_app.debug_virtual_hit_id));
    return block.text;
}

inline std::string debug_console_time_text(std::uint64_t unix_ns) {
    if (unix_ns == 0)
        return "--:--:--.---";
    auto const total_ms = unix_ns / 1'000'000ull;
    auto const ms = total_ms % 1000ull;
    auto const total_s = total_ms / 1000ull;
    auto const sec = total_s % 60ull;
    auto const min = (total_s / 60ull) % 60ull;
    auto const hour = (total_s / 3600ull) % 24ull;
    return std::format("{:02}:{:02}:{:02}.{:03}", hour, min, sec, ms);
}

inline std::string debug_console_record_text(log::Record const& record) {
    std::string out;
    out += debug_console_time_text(record.time_unix_nano);
    out += " ";
    out += log::severity_text(record.severity);
    out += " ";
    out += record.scope_name.empty() ? "phenotype" : record.scope_name;
    out += " ";
    out += record.body.empty() ? "(empty)" : record.body;
    return out;
}

inline std::string debug_console_records_text(
        std::vector<log::Record> const& records) {
    std::string out;
    for (auto const& record : records) {
        if (!out.empty())
            out += "\n";
        out += debug_console_record_text(record);
    }
    return out;
}

inline void debug_console_copy(std::string text) {
    g_app.debug_console_copy_buffer = std::move(text);
    ++g_app.debug_console_copy_serial;
    trigger_rebuild();
}

inline void debug_console_clear_copy() {
    g_app.debug_console_copy_buffer.clear();
    ++g_app.debug_console_copy_serial;
    trigger_rebuild();
}

inline void render_debug_console_record(log::Record const& record) {
    auto const& t = g_app.theme;
    auto time_text = debug_console_time_text(record.time_unix_nano);
    auto level_text = std::string{log::severity_text(record.severity)};
    auto scope_text = record.scope_name.empty()
        ? std::string{"phenotype"}
        : record.scope_name;
    auto body = record.body.empty() ? std::string{"(empty)"} : record.body;
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Clear,
            .role = MaterialSurfaceRole::Content,
            .direction = FlexDirection::Column,
            .padding = SpaceToken::Sm,
            .gap = SpaceToken::Xs,
            .border_radius = t.radius_sm,
            .border_width = 0.5f,
            .semantic_label = "Console Record",
            .has_material_override = true,
            .material_override = [&] {
                auto style = layout::material_style(MaterialKind::Clear);
                style.tint = debug_with_alpha(t.surface, 54);
                style.border = debug_with_alpha(t.border, 110);
                return style;
            }(),
        },
        [&] {
            layout::row([&] {
                widget::text(time_text, TextSize::Small, TextColor::Muted);
                widget::text(
                    level_text,
                    TextSize::Small,
                    record.severity == log::Severity::info
                        ? TextColor::Accent
                        : TextColor::Muted);
                widget::text(scope_text, TextSize::Small, TextColor::Muted);
                debug_panel_action_button(
                    "Copy",
                    [text = debug_console_record_text(record)]() mutable {
                        debug_console_copy(std::move(text));
                    });
            }, SpaceToken::Xs, CrossAxisAlignment::Center,
               MainAxisAlignment::SpaceBetween);
            widget::code(body);
        });
}

inline FrameTraceSample debug_frame_recent_sample(
        FrameTraceMonitor const& stats,
        std::size_t index) noexcept {
    if (index >= stats.recent_count)
        return {};
    auto const start = stats.recent_count == FrameTraceMonitor::RECENT_CAPACITY
        ? stats.recent_next
        : 0u;
    return stats.recent[
        (start + index) % FrameTraceMonitor::RECENT_CAPACITY];
}

struct DebugFrameSegment {
    char const* label;
    std::uint64_t ns;
    Color color;
};

inline std::array<DebugFrameSegment, 5> debug_frame_segments(
        FrameTraceSample const& frame) {
    return {{
        {"input", frame.input_ns, Color{168, 85, 247, 210}},
        {"view", frame.view_ns + frame.update_ns, Color{59, 130, 246, 210}},
        {"layout", frame.layout_ns, Color{16, 185, 129, 210}},
        {"paint", frame.paint_ns, Color{245, 158, 11, 220}},
        {"flush", frame.flush_ns, Color{236, 72, 153, 210}},
    }};
}

struct DebugFrameChartScale {
    double min_ms = 0.0;
    double max_ms = 16.666667;
};

inline std::uint64_t debug_frame_recent_percentile(
        FrameTraceMonitor const& stats,
        double percentile) {
    if (stats.recent_count == 0)
        return 0;
    std::array<std::uint64_t, FrameTraceMonitor::RECENT_CAPACITY> sorted{};
    for (std::size_t i = 0; i < stats.recent_count; ++i)
        sorted[i] = debug_frame_recent_sample(stats, i).total_ns;
    auto first = sorted.begin();
    auto last = sorted.begin() + static_cast<std::ptrdiff_t>(stats.recent_count);
    std::sort(first, last);
    auto const index = static_cast<std::size_t>(
        std::lround(percentile * static_cast<double>(stats.recent_count - 1)));
    return sorted[std::min(index, stats.recent_count - 1)];
}

inline DebugFrameChartScale debug_frame_chart_scale(
        FrameTraceMonitor const& stats,
        std::size_t start,
        std::size_t count) noexcept {
    if (count == 0)
        return {};

    double min_ms = std::numeric_limits<double>::max();
    double max_ms = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        auto const sample = debug_frame_recent_sample(stats, start + i);
        double const sample_ms =
            static_cast<double>(sample.total_ns) / 1'000'000.0;
        min_ms = std::min(min_ms, sample_ms);
        max_ms = std::max(max_ms, sample_ms);
    }

    if (min_ms == std::numeric_limits<double>::max())
        return {};

    double const span = std::max(0.001, max_ms - min_ms);
    double const padding = std::max(0.25, span * 0.14);
    min_ms = min_ms > padding ? min_ms - padding : 0.0;
    max_ms += padding;

    if (max_ms - min_ms < 0.5) {
        double const mid = (min_ms + max_ms) * 0.5;
        min_ms = mid > 0.25 ? mid - 0.25 : 0.0;
        max_ms = min_ms + 0.5;
    }

    return DebugFrameChartScale{
        .min_ms = min_ms,
        .max_ms = std::max(max_ms, min_ms + 0.5),
    };
}

inline void debug_mix_frame_perf_token(std::uint64_t& token,
                                       FrameTraceMonitor const& stats) noexcept {
    token = static_cast<std::uint64_t>(hash_combine(token, stats.count));
    token = static_cast<std::uint64_t>(hash_combine(token, stats.last_ns));
    token = static_cast<std::uint64_t>(
        hash_combine(token, stats.over_60fps_budget));
    token = static_cast<std::uint64_t>(
        hash_combine(token, stats.recent_count));
    token = static_cast<std::uint64_t>(hash_combine(token, stats.recent_next));
}

inline std::uint64_t debug_performance_chart_token() noexcept {
    std::uint64_t token = 0x6475626770657266ULL;
    debug_mix_frame_perf_token(token, g_app.frame_perf);
    token = static_cast<std::uint64_t>(
        hash_combine(token, static_cast<unsigned int>(g_app.frame_perf.last.action)));
    token = static_cast<std::uint64_t>(
        hash_combine(token, g_app.frame_perf.last.paint_ns));
    token = static_cast<std::uint64_t>(
        hash_combine(token, g_app.frame_perf.last.flush_ns));
    return token;
}

inline void paint_debug_performance_chart(Painter& painter) {
    auto const& t = g_app.theme;
    constexpr float width = DEBUG_PANEL_BODY_WIDTH;
    constexpr float height = DEBUG_PERFORMANCE_CHART_HEIGHT;
    constexpr double budget_60_ms = 16.666667;
    constexpr double budget_240_ms = 4.166667;
    PaintRect bg{0.0f, 0.0f, width, height, debug_with_alpha(t.code_bg, 178)};
    painter.fill_rects(&bg, 1);

    float const left = 54.0f;
    float const top = 22.0f;
    float const right = width - 12.0f;
    float const bottom = 128.0f;
    float const graph_w = right - left;
    float const graph_h = bottom - top;
    auto const& stats = g_app.frame_perf;
    auto const p95 = debug_frame_recent_percentile(stats, 0.95);
    auto const count = std::min<std::size_t>(stats.recent_count, 96);
    auto const start = stats.recent_count > count
        ? stats.recent_count - count
        : 0u;
    auto const scale = debug_frame_chart_scale(stats, start, count);
    auto y_for_ms = [&](double ms) {
        double const ratio = std::clamp(
            (ms - scale.min_ms) / (scale.max_ms - scale.min_ms),
            0.0,
            1.0);
        return bottom - static_cast<float>(ratio) * graph_h;
    };
    auto draw_ms_tick = [&](double ms, Color color) {
        float const y = y_for_ms(ms);
        painter.line(left, y, right, y, 1.0f, color);
        auto label = std::format("{:.2f}ms", ms);
        painter.text(
            6.0f,
            y - 6.0f,
            label.c_str(),
            static_cast<unsigned int>(label.size()),
            10.0f,
            t.muted);
    };
    auto draw_budget_line = [&](double ms, Color color, char const* label) {
        if (ms < scale.min_ms || ms > scale.max_ms)
            return;
        float const y = y_for_ms(ms);
        painter.line(left, y, right, y, 1.0f, color);
        painter.text(
            right - 58.0f,
            y - 11.0f,
            label,
            static_cast<unsigned int>(std::strlen(label)),
            10.0f,
            t.muted);
    };

    painter.text(8.0f, 6.0f, "frame render time (ms)", 22, 11.0f, t.muted);
    painter.line(left, bottom, right, bottom, 1.0f, debug_with_alpha(t.border, 160));
    painter.line(left, top, left, bottom, 1.0f, debug_with_alpha(t.border, 120));
    draw_ms_tick(scale.max_ms, debug_with_alpha(t.border, 120));
    draw_ms_tick((scale.min_ms + scale.max_ms) * 0.5,
                 debug_with_alpha(t.border, 85));
    draw_ms_tick(scale.min_ms, debug_with_alpha(t.border, 120));
    draw_budget_line(
        budget_240_ms,
        debug_with_alpha(t.semantic_success_border, 145),
        "240fps");
    draw_budget_line(
        budget_60_ms,
        debug_with_alpha(t.semantic_warning_border, 170),
        "60fps");

    Color const line_color = Color{37, 99, 235, 230};
    if (count > 1) {
        float const step = graph_w / static_cast<float>(count - 1);
        PathBuilder area;
        PathBuilder line;
        area.move_to(left, bottom);
        for (std::size_t i = 0; i < count; ++i) {
            auto const sample = debug_frame_recent_sample(stats, start + i);
            float const x = left + static_cast<float>(i) * step;
            float const y = y_for_ms(
                static_cast<double>(sample.total_ns) / 1'000'000.0);
            if (i == 0)
                line.move_to(x, y);
            else
                line.line_to(x, y);
            area.line_to(x, y);
        }
        area.line_to(right, bottom);
        area.close();
        painter.fill_path(area, debug_with_alpha(line_color, 38));
        painter.stroke_path(line, 2.0f, line_color);
    }

    auto const last = stats.last;
    std::string summary = std::format(
        "{:.2f}ms  p95 {:.2f}ms  {}",
        static_cast<double>(last.total_ns) / 1'000'000.0,
        static_cast<double>(p95) / 1'000'000.0,
        frame_trace_action_label(last.action));
    auto const summary_w = painter.measure_text(
        summary.c_str(),
        static_cast<unsigned int>(summary.size()),
        11.0f);
    painter.text(
        right - summary_w,
        6.0f,
        summary.c_str(),
        static_cast<unsigned int>(summary.size()),
        11.0f,
        t.foreground);

    auto const segments = debug_frame_segments(last);
    float const bar_x = left;
    float const bar_y = 150.0f;
    float const bar_h = 12.0f;
    float cursor = bar_x;
    std::vector<PaintRect> rects;
    rects.reserve(5);
    std::uint64_t const total = std::max<std::uint64_t>(last.total_ns, 1);
    for (auto const& segment : segments) {
        if (segment.ns == 0)
            continue;
        float const w = std::max(
            1.0f,
            graph_w * static_cast<float>(
                static_cast<double>(segment.ns)
                / static_cast<double>(total)));
        rects.push_back(PaintRect{cursor, bar_y, w, bar_h, segment.color});
        cursor += w;
    }
    if (!rects.empty()) {
        painter.fill_rects(
            rects.data(),
            static_cast<unsigned int>(rects.size()));
    }

    float label_x = left;
    for (auto const& segment : segments) {
        std::string label = std::format(
            "{} {:.2f}",
            segment.label,
            static_cast<double>(segment.ns) / 1'000'000.0);
        painter.text(
            label_x,
            170.0f,
            label.c_str(),
            static_cast<unsigned int>(label.size()),
            10.0f,
            t.muted);
        label_x += 78.0f;
    }
}

inline std::string performance_debug_block() {
    auto const& frame = g_app.frame_perf;
    auto const p95 = debug_frame_recent_percentile(frame, 0.95);
    auto const average_ms = frame.count == 0
        ? 0.0
        : (static_cast<double>(frame.total_ns)
           / static_cast<double>(frame.count))
            / 1'000'000.0;
    auto const& last = frame.last;
    DebugTextBlock block;
    block.line("frames", frame.count);
    block.line_ms("last_frame_ms", last.total_ns);
    block.line(
        "last_frame_fps",
        last.total_ns == 0
            ? "0.0"
            : std::format("{:.1f}", 1'000'000'000.0 / static_cast<double>(last.total_ns)));
    block.line_ms("p95_frame_ms", p95);
    block.line("avg_frame_ms", std::format("{:.3f}", average_ms));
    block.line("frames_over_60fps_budget", frame.over_60fps_budget);
    block.line("last_trace_action", frame_trace_action_label(last.action));
    block.line_ms("last_input_ms", last.input_ns);
    block.line_ms("last_update_ms", last.update_ns);
    block.line_ms("last_view_ms", last.view_ns);
    block.line_ms("last_layout_ms", last.layout_ns);
    block.line_ms("last_paint_ms", last.paint_ns);
    block.line_ms("last_flush_ms", last.flush_ns);
    block.line("rebuilds", metrics::inst::rebuilds.total());
    block.line("input_events", metrics::inst::input_events.total());
    block.line("flush_calls", metrics::inst::flush_calls.total());
    block.line("frames_skipped", metrics::inst::frames_skipped.total());
    block.line("measure_text_calls", metrics::inst::measure_text_calls.total());
    block.line("measure_text_cache_hits", metrics::inst::measure_text_cache_hits.total());
    block.line("layout_nodes_computed", metrics::inst::layout_nodes_computed.total());
    block.line("layout_nodes_skipped", metrics::inst::layout_nodes_skipped.total());
    block.line("paint_subtrees_blitted", metrics::inst::paint_subtrees_blitted.total());
    block.line("native_texture_upload_bytes", metrics::inst::native_texture_upload_bytes.total());
    block.line("native_buffer_reallocations", metrics::inst::native_buffer_reallocations.total());
    block.line("frame_60fps_budget_ms", "16.667");
    block.line("frame_240fps_budget_ms", "4.167");
    return block.text;
}

inline void render_debug_performance_tab() {
    g_app.has_active_animations = true;
    widget::text("Frame Timeline", TextSize::Small, TextColor::Muted);
    widget::semantic_canvas(
        DEBUG_PANEL_BODY_WIDTH,
        DEBUG_PERFORMANCE_CHART_HEIGHT,
        "Debug Performance Chart",
        [](Painter& painter) {
            paint_debug_performance_chart(painter);
        },
        {},
        debug_performance_chart_token(),
        "chart");
    layout::spacer(8);
    widget::text("Counters", TextSize::Small, TextColor::Muted);
    widget::code(performance_debug_block());
}

inline void render_debug_layout_tab(
        diag::InputDebugSnapshot const& input_debug) {
    auto semantic_tree = debug_panel_app_semantic_tree_snapshot();
    auto const* root = semantic_tree ? &*semantic_tree : nullptr;
    auto const* hovered = root
        ? find_semantic_node_by_callback(*root, input_debug.hovered_id)
        : nullptr;
    auto const* selected_node = hovered ? hovered : root;
    std::optional<diag::SemanticNodeSnapshot> selected_snapshot;
    if (selected_node)
        selected_snapshot = *selected_node;
    widget::text("Layout Overview", TextSize::Small, TextColor::Muted);
    widget::semantic_canvas(
        DEBUG_PANEL_BODY_WIDTH,
        DEBUG_LAYOUT_OVERVIEW_HEIGHT,
        "Debug Layout Overview",
        [semantic_tree, input_debug](Painter& painter) {
            paint_debug_layout_diagram(painter, semantic_tree, input_debug);
        },
        {},
        debug_layout_diagram_token(input_debug, root),
        "diagram");
    layout::spacer(8);
    widget::text("Box Model", TextSize::Small, TextColor::Muted);
    widget::semantic_canvas(
        DEBUG_PANEL_BODY_WIDTH,
        DEBUG_BOX_MODEL_HEIGHT,
        "Debug Box Model",
        [selected_snapshot](Painter& painter) {
            paint_debug_box_model_diagram(
                painter,
                selected_snapshot ? &*selected_snapshot : nullptr);
        },
        {},
        debug_layout_diagram_token(input_debug, root)
            ^ 0x626f786d6f64656cULL,
        "diagram");
    layout::spacer(8);
    widget::text("Selected Node", TextSize::Small, TextColor::Muted);
    widget::code(debug_semantic_node_block(selected_node));
}

inline void render_debug_console_tab() {
    auto records = log::recent_records(60);
    widget::text("Live Console", TextSize::Small, TextColor::Muted);
    layout::row([&] {
        debug_panel_action_button(
            "Copy visible",
            [records] {
                debug_console_copy(debug_console_records_text(records));
            });
        debug_panel_action_button(
            "Copy latest",
            [records] {
                if (!records.empty())
                    debug_console_copy(debug_console_record_text(records.back()));
            });
        debug_panel_action_button(
            "Clear copy",
            [] { debug_console_clear_copy(); });
    }, SpaceToken::Xs, CrossAxisAlignment::Center, MainAxisAlignment::Start);
    if (!g_app.debug_console_copy_buffer.empty()) {
        widget::text(
            std::string{"Copy Buffer #"}
                + std::to_string(g_app.debug_console_copy_serial),
            TextSize::Small,
            TextColor::Muted);
        widget::code(g_app.debug_console_copy_buffer);
    }
    if (records.empty()) {
        widget::code("live_tail: true\nrecords: 0\n\n(no console records yet)");
        return;
    }
    for (auto const& record : records)
        render_debug_console_record(record);
}

inline void render_debug_input_tab(
        diag::InputDebugSnapshot const& input_debug) {
    auto semantic_tree = debug_panel_app_semantic_tree_snapshot();
    widget::text("Virtual Pointer", TextSize::Small, TextColor::Muted);
    widget::semantic_canvas(
        DEBUG_PANEL_BODY_WIDTH,
        DEBUG_VIRTUAL_INPUT_HEIGHT,
        "Debug Virtual Input Pad",
        [semantic_tree, input_debug](Painter& painter) {
            paint_debug_virtual_input_pad(painter, semantic_tree, input_debug);
        },
        [](GestureEvent const& ev) {
            float const viewport_w = std::max(1.0f, g_app.debug_viewport_width);
            float const viewport_h = std::max(1.0f, g_app.debug_viewport_height);
            debug_set_virtual_pointer(
                std::clamp(ev.focus_x / DEBUG_PANEL_BODY_WIDTH, 0.0f, 1.0f)
                    * viewport_w,
                std::clamp(ev.focus_y / DEBUG_VIRTUAL_INPUT_HEIGHT, 0.0f, 1.0f)
                    * viewport_h);
        },
        debug_virtual_input_token()
            ^ static_cast<std::uint64_t>(input_debug.hovered_id)
            ^ (static_cast<std::uint64_t>(input_debug.focused_id) << 1u),
        "input");
    layout::row([&] {
        debug_panel_action_button("Center", [] { debug_center_virtual_pointer(); });
        debug_panel_action_button("To hover", [] { debug_set_virtual_pointer_to_hovered(); });
        debug_panel_action_button("To focus", [] { debug_set_virtual_pointer_to_focused(); });
        debug_panel_action_button("Clear", [] { debug_virtual_clear(); });
    }, SpaceToken::Xs, CrossAxisAlignment::Center, MainAxisAlignment::Start);
    layout::row([&] {
        debug_panel_action_button("Hover", [] { debug_virtual_hover(); });
        debug_panel_action_button("Click", [] { debug_virtual_click(); });
        debug_panel_action_button("Left 8", [] { debug_nudge_virtual_pointer(-8.0f, 0.0f); });
        debug_panel_action_button("Right 8", [] { debug_nudge_virtual_pointer(8.0f, 0.0f); });
    }, SpaceToken::Xs, CrossAxisAlignment::Center, MainAxisAlignment::Start);
    layout::row([&] {
        debug_panel_action_button("Up 8", [] { debug_nudge_virtual_pointer(0.0f, -8.0f); });
        debug_panel_action_button("Down 8", [] { debug_nudge_virtual_pointer(0.0f, 8.0f); });
        debug_panel_action_button("Left 32", [] { debug_nudge_virtual_pointer(-32.0f, 0.0f); });
        debug_panel_action_button("Right 32", [] { debug_nudge_virtual_pointer(32.0f, 0.0f); });
    }, SpaceToken::Xs, CrossAxisAlignment::Center, MainAxisAlignment::Start);
    widget::code(virtual_input_debug_block());
    layout::spacer(8);
    widget::text("Input Routing", TextSize::Small, TextColor::Muted);
    widget::code(input_debug_block(input_debug));
}

inline void render_debug_protocol_tab() {
    widget::text("Agent Protocol", TextSize::Small, TextColor::Muted);
    widget::code(debug_protocol_block());
}

inline void render_debug_panel_tab_content(
        diag::InputDebugSnapshot const& input_debug) {
    switch (g_app.debug_panel_tab) {
        case DebugPanelTab::Performance:
            render_debug_performance_tab();
            break;
        case DebugPanelTab::Layout:
            render_debug_layout_tab(input_debug);
            break;
        case DebugPanelTab::Console:
            render_debug_console_tab();
            break;
        case DebugPanelTab::Input:
            render_debug_input_tab(input_debug);
            break;
        case DebugPanelTab::Protocol:
            render_debug_protocol_tab();
            break;
    }
}

struct DebugPanelGeometry {
    float viewport_width = 1280.0f;
    float viewport_height = 720.0f;
    float panel_width = 480.0f;
    float panel_height = 620.0f;
    float content_height = 502.0f;
};

inline DebugPanelGeometry debug_panel_geometry() {
    DebugPanelGeometry geometry{};
    geometry.viewport_width = g_app.debug_viewport_width > 1.0f
        ? g_app.debug_viewport_width
        : 1280.0f;
    geometry.viewport_height = g_app.debug_viewport_height > 1.0f
        ? g_app.debug_viewport_height
        : 720.0f;
    geometry.panel_width = geometry.viewport_width > 540.0f
        ? 480.0f
        : std::max(320.0f, geometry.viewport_width - 24.0f);
    geometry.panel_height = geometry.viewport_height > 120.0f
        ? geometry.viewport_height - 32.0f
        : 620.0f;
    geometry.content_height = std::max(260.0f, geometry.panel_height - 118.0f);
    return geometry;
}

inline void render_debug_hover_highlight(DebugPanelGeometry const& geometry,
                                         unsigned int hovered_id) {
    if (hovered_id != DEBUG_INVALID_ID) {
        layout::overlay([&] {
            widget::semantic_canvas(
                geometry.viewport_width,
                geometry.viewport_height,
                "Debug Layout Hover Highlight",
                [hovered_id](Painter& painter) {
                    paint_debug_hover_highlight(painter, hovered_id);
                },
                {},
                0x6465627567486f76ULL ^ static_cast<std::uint64_t>(hovered_id),
                "debug_overlay");
        });
    }
}

inline layout::MaterialSurfaceOptions debug_panel_surface_options(
        float panel_width) {
    auto panel_options = layout::glass_surface_options(
        layout::GlassSurfacePreset::Inspector,
        "Debug Panel");
    panel_options.kind = MaterialKind::Thick;
    panel_options.role = MaterialSurfaceRole::Sidebar;
    panel_options.interactive = false;
    panel_options.capture_pointer = true;
    panel_options.max_width = panel_width;
    panel_options.border_width = 1.0f;
    panel_options.has_material_override = true;
    panel_options.material_override =
        layout::glass_background_effect_material_style(
            MaterialKind::Thick,
            layout::glass_background_feathered(18.0f, 28.0f));
    panel_options.material_override.tint =
        debug_with_alpha(g_app.theme.surface, 226);
    panel_options.material_override.border =
        debug_with_alpha(g_app.theme.border, 236);
    panel_options.material_override.blur_radius = 64.0f;
    panel_options.material_override.opacity = 0.94f;
    panel_options.material_override.saturation = 1.08f;
    return panel_options;
}

inline void render_debug_panel_header() {
    layout::row([&] {
        widget::text("Debug", TextSize::Heading);
        widget::text(
            std::string{"Toggle: "} + debug_panel_shortcut_text(),
            TextSize::Small,
            TextColor::Muted);
    }, SpaceToken::Sm, CrossAxisAlignment::Center,
       MainAxisAlignment::SpaceBetween);
}

inline void render_debug_panel_overlay() {
    if (!g_app.debug_panel_open)
        return;
    if (g_app.debug_panel_warmup_frames > 0u) {
        g_app.has_active_animations = true;
        --g_app.debug_panel_warmup_frames;
    }

    auto input_debug = current_input_debug_snapshot();
    auto const geometry = debug_panel_geometry();
    render_debug_hover_highlight(geometry, input_debug.hovered_id);

    layout::overlay([&] {
        layout::row([&] {
            layout::sized_box(geometry.panel_width, [&] {
                layout::material_surface(
                    debug_panel_surface_options(geometry.panel_width),
                    [&] {
                        render_debug_panel_header();
                        debug_panel_tab_bar();
                        layout::divider();
                        layout::scroll_view(geometry.content_height, [&] {
                            render_debug_panel_tab_content(input_debug);
                        }, SpaceToken::Sm);
                    });
            });
        }, SpaceToken::Md, CrossAxisAlignment::Start, MainAxisAlignment::End);
    });
}
#endif

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

inline bool set_pointer_position(float x, float y) noexcept {
    bool const changed = !g_app.pointer_valid
        || g_app.pointer_x != x
        || g_app.pointer_y != y;
    g_app.pointer_valid = true;
    g_app.pointer_x = x;
    g_app.pointer_y = y;
    return changed;
}

inline bool clear_pointer_position() noexcept {
    if (!g_app.pointer_valid)
        return false;
    g_app.pointer_valid = false;
    return true;
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

    __attribute__((export_name("phenotype_set_pointer")))
    unsigned int phenotype_set_pointer(float x, float y) {
        return phenotype::detail::set_pointer_position(x, y) ? 1u : 0u;
    }

    __attribute__((export_name("phenotype_clear_pointer")))
    unsigned int phenotype_clear_pointer(void) {
        return phenotype::detail::clear_pointer_position() ? 1u : 0u;
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

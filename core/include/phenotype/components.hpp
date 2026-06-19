#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <functional>
#include <string_view>
#include <utility>
#endif

#include "phenotype/tokens.hpp"
#include "phenotype/ui.hpp"

// Component builders for the modern UI surface.
//
// These are thin, token-speaking conveniences that assemble the same value-tree
// ui::View the layout engine already consumes — they add no new ViewKind and no
// new scene records, so everything here renders on the kinds main already
// supports (text, icon button, panel, stack, grid, scroll). Richer controls
// that need new layout/renderer support (text fields, checkboxes, tabs, glass,
// keyed lists) are intentionally deferred to a later slice.
//
// Two entry styles over the same primitives:
//   - ui::Text / ui::Button / ui::VStack / ... : value builders that compose
//     and chain like the rest of ui::.
//   - widget::button / widget::label / ...     : callback-first wrappers for
//     imperative call sites.
namespace phenotype::ui {

// Options for the stack builders. spacing is a semantic rung; pass children
// after it. Kept as a tag struct so VStack(StackOptions{...}, a, b) reads
// naturally and stays distinct from a child view.
struct StackOptions {
  SpaceToken spacing = SpaceToken::none;
  bool centers_children = false;
};

namespace detail {

inline View ApplyFont(View view, Font font) {
  FontMetrics metrics = Resolve(font);
  view.font_size_value = metrics.size;
  view.font_weight_value = metrics.weight;
  return view;
}

} // namespace detail

// --- Text -------------------------------------------------------------------

// A text run with token-driven type ramp and color. Returns a plain text View,
// so all existing text modifiers (line_limit, overflow, center_text, ...) still
// chain off it.
inline View Text(std::string_view content) { return text(content); }

inline View Text(std::string_view content, Font font, TextColor color = TextColor::primary) {
  return detail::ApplyFont(text(content), font).foreground(Resolve(color));
}

// --- Stacks -----------------------------------------------------------------

template <ViewValue... Children> View VStack(StackOptions options, Children &&...children) {
  View view = layout::vstack(std::forward<Children>(children)...);
  view.child_spacing = Space(options.spacing);
  view.centers_children = options.centers_children;
  return view;
}

template <ViewValue... Children> View VStack(Children &&...children) {
  return layout::vstack(std::forward<Children>(children)...);
}

template <ViewValue... Children> View HStack(StackOptions options, Children &&...children) {
  View view = layout::hstack(std::forward<Children>(children)...);
  view.child_spacing = Space(options.spacing);
  view.centers_children = options.centers_children;
  return view;
}

template <ViewValue... Children> View HStack(Children &&...children) {
  return layout::hstack(std::forward<Children>(children)...);
}

template <ViewValue... Children> View ZStack(Children &&...children) {
  return layout::zstack(std::forward<Children>(children)...);
}

// --- Box / Card -------------------------------------------------------------

// A panel-backed container: a rounded surface with content overlaid on top.
// Renders as a zstack of a panel under the content, which the overlay axis and
// existing panel kind already support.
inline View Box(View content, Color background = control_background()) {
  return layout::zstack(panel(background).expand(), std::move(content));
}

inline View Card(View content) {
  return layout::zstack(
      panel(control_background()).corner_radius(default_surface_corner_radius()).expand(),
      std::move(content));
}

// --- Button -----------------------------------------------------------------

[[nodiscard]] inline Color ButtonBackground(ButtonRole role) noexcept {
  switch (role) {
  case ButtonRole::normal:
  case ButtonRole::back:
  case ButtonRole::forward:
    return control_background();
  }
  return control_background();
}

// A text button. main's `button` kind is icon-only, so a labelled button is
// composed from renderable parts: a clickable rounded panel with the label
// centered over it. The click action lives on the outer view, which produces a
// hit target for any kind. role currently selects the background fill.
inline View Button(std::string_view label, std::function<void()> on_click,
    ButtonRole role = ButtonRole::normal) {
  View text_label = Text(label).center_text();
  return layout::zstack(panel(ButtonBackground(role))
                            .corner_radius(8.0f)
                            .expand(),
      std::move(text_label))
      .on_click(std::move(on_click))
      .role(role);
}

// Label-only overload for call sites that wire the click separately.
inline View Button(std::string_view label) {
  return layout::zstack(
      panel(ButtonBackground(ButtonRole::normal)).corner_radius(8.0f).expand(),
      Text(label).center_text());
}

// An icon button reuses the native icon-button kind directly.
inline View IconButton(Symbol symbol, std::function<void()> on_click,
    SymbolOptions options = navigation_symbol_options()) {
  return button(icon(symbol, options)).on_click(std::move(on_click));
}

} // namespace phenotype::ui

// Callback-first wrappers over the same builders, for imperative call sites.
namespace phenotype::widget {

inline ui::View label(std::string_view content) { return ui::Text(content); }

inline ui::View button(std::string_view title, std::function<void()> on_click) {
  return ui::Button(title, std::move(on_click));
}

inline ui::View icon_button(ui::Symbol symbol, std::function<void()> on_click) {
  return ui::IconButton(symbol, std::move(on_click));
}

} // namespace phenotype::widget

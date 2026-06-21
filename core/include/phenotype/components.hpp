#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#endif

#include "phenotype/runtime.hpp"
#include "phenotype/tokens.hpp"
#include "phenotype/ui.hpp"

// Component builders for the modern UI surface.
//
// These are thin, token-speaking conveniences that assemble the same value-tree
// ui::View the layout engine already consumes — they add no new ViewKind and no
// new scene records, so everything here renders on the kinds main already
// supports (text, icon button, panel, stack, grid, scroll). ForEach adds keyed
// list children — pure identity metadata (View::view_key) the layout engine
// ignores today and the retained-tree reconciliation slice consumes later.
// Checkbox / Radio / Switch add a ViewKind::toggle the layout pass lowers into
// plain panels, and Tabs is composed from panel/text/stack kinds — both need no
// renderer support. Richer controls (text fields, glass) are intentionally
// deferred to a later slice.
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

// --- Toggles (Checkbox / Radio / Switch) ------------------------------------

namespace detail {

// A labelled toggle row: the control box, an optional label, and a click action
// over the whole row that flips the bound value. The control reflects the bound
// value, so the next rebuild redraws it in the new state.
inline View ToggleRow(ToggleStyle style, Binding<bool> value, std::string_view label) {
  View control = toggle(style, value.valid() && value.get());
  View row = label.empty()
                 ? layout::hstack(std::move(control))
                 : layout::hstack(std::move(control), Text(label));
  row.child_spacing = Space(SpaceToken::sm);
  return std::move(row).on_click([value] {
    if (value.valid()) {
      value.set(!value.get());
    }
  });
}

} // namespace detail

// A checkbox bound to a bool. Toggling the row flips the binding; the layout
// pass renders the box from plain panels, so it needs no renderer support.
inline View Checkbox(Binding<bool> value, std::string_view label = {}) {
  return detail::ToggleRow(ToggleStyle::checkbox, value, label);
}

// A radio control bound to a bool (the caller drives mutual exclusion by setting
// the shared selection and binding each option to its own predicate).
inline View Radio(Binding<bool> value, std::string_view label = {}) {
  return detail::ToggleRow(ToggleStyle::radio, value, label);
}

// A switch bound to a bool, drawn as a capsule track with a sliding knob.
inline View Switch(Binding<bool> value, std::string_view label = {}) {
  return detail::ToggleRow(ToggleStyle::switcher, value, label);
}

// --- Tabs (segmented control) -----------------------------------------------

// A segmented control bound to a selected index. Each segment is a clickable
// label; the selected one carries a rounded highlight panel behind it. The bar
// sits on a track panel. Composed entirely from panel/text/stack kinds, so it
// needs no new scene record or renderer support — the same strategy as Button
// and the toggles. Clicking a segment sets the bound index, and the next
// rebuild moves the highlight.
inline View Tabs(Binding<int> selection, std::span<const std::string_view> labels) {
  int selected = selection.valid() ? selection.get() : 0;

  std::vector<View> segments;
  segments.reserve(labels.size());
  for (std::size_t index = 0; index < labels.size(); ++index) {
    bool is_selected = static_cast<int>(index) == selected;
    View label = Text(labels[index]).center_text();
    if (is_selected) {
      label.foreground_color = primary_label();
    }

    View segment = is_selected
                       ? layout::zstack(panel(white()).corner_radius(7.0f).expand(),
                             std::move(label))
                       : layout::zstack(std::move(label));
    int target = static_cast<int>(index);
    segment = std::move(segment).expand_width().on_click([selection, target] {
      if (selection.valid()) {
        selection.set(target);
      }
    });
    segments.push_back(std::move(segment));
  }

  View bar;
  bar.kind = ViewKind::stack;
  bar.axis = LayoutAxis::horizontal;
  bar.children = std::move(segments);
  bar.child_spacing = 2.0f;
  bar.content_padding = SpaceInsets(SpaceToken::xs);

  return layout::zstack(panel(control_background()).corner_radius(9.0f).expand(), std::move(bar));
}

// --- ForEach ----------------------------------------------------------------

namespace detail {

// FNV-1a over the bytes of a trivially-copyable key, so ForEach can derive a
// stable view_key from common key types (integers, enums) without the caller
// hashing by hand. String keys go through the string_view overload below.
template <typename T>
[[nodiscard]] std::uint64_t HashKey(const T &value) noexcept
  requires std::is_trivially_copyable_v<T>
{
  const auto *bytes = reinterpret_cast<const unsigned char *>(&value);
  std::uint64_t hash = 1469598103934665603ull;
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    hash ^= static_cast<std::uint64_t>(bytes[index]);
    hash *= 1099511628211ull;
  }
  // 0 means "no key" on a View, so never collapse a real key onto it.
  return hash == 0 ? 1ull : hash;
}

[[nodiscard]] inline std::uint64_t HashKey(std::string_view value) noexcept {
  std::uint64_t hash = 1469598103934665603ull;
  for (char character : value) {
    hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
    hash *= 1099511628211ull;
  }
  return hash == 0 ? 1ull : hash;
}

} // namespace detail

// Build one keyed child View per item in a range.
//
// Each item is turned into a View by item_fn, and tagged with a stable view_key
// derived from key_fn(item). The children live under a single container (a
// vstack by default), so the keys only need to be unique among siblings — the
// same per-parent identity scope React/SwiftUI/Flutter use for list diffing.
// This is the identity the retained-tree reconciliation slice keys off, so two
// ForEach loops elsewhere in the tree never collide.
//
// key_fn returns either something hashable here (integer/enum or a
// string_view) or a std::uint64_t that is used verbatim. item_fn returns the
// item's View; its view_key is overwritten with the derived key.
template <typename Range, typename KeyFn, typename ItemFn>
[[nodiscard]] View ForEach(Range &&range, KeyFn &&key_fn, ItemFn &&item_fn) {
  std::vector<View> children;
  for (auto &&item : std::forward<Range>(range)) {
    auto key = key_fn(item);
    std::uint64_t view_key;
    if constexpr (std::is_same_v<std::decay_t<decltype(key)>, std::uint64_t>) {
      view_key = key == 0 ? 1ull : key;
    } else {
      view_key = detail::HashKey(key);
    }
    View child = item_fn(item);
    child.view_key = view_key;
    children.push_back(std::move(child));
  }
  View container;
  container.kind = ViewKind::stack;
  container.axis = LayoutAxis::vertical;
  container.children = std::move(children);
  return container;
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

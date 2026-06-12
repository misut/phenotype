#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#endif

#include "phenotype/material_symbols.hpp"

namespace phenotype::ui {

struct Size {
  float width = 0.0f;
  float height = 0.0f;
};

struct Insets {
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
};

struct LeadingWindowControlsPlacement {
  bool is_enabled = false;
  float spacing = 12.0f;
  bool aligns_vertical_center = true;
};

using SymbolOptions = phenotype::MaterialSymbolOptions;

enum class Symbol {
  chevron_left,
  chevron_right,
};

enum class ButtonRole {
  normal,
  back,
  forward,
};

enum class ControlShape {
  square_circle,
  capsule,
};

enum class ViewKind {
  empty,
  stack,
  spacer,
  button,
  button_group,
  icon,
};

enum class LayoutAxis {
  horizontal,
  vertical,
  overlay,
};

class View {
public:
  ViewKind kind = ViewKind::empty;
  LayoutAxis axis = LayoutAxis::vertical;
  std::vector<View> children;
  Symbol symbol = Symbol::chevron_left;
  SymbolOptions symbol_options;
  ButtonRole button_role = ButtonRole::normal;
  bool is_enabled = true;
  std::string accessibility_label_text;
  Size preferred_size;
  Insets content_padding;
  LeadingWindowControlsPlacement leading_window_controls_placement;
  ControlShape control_shape = ControlShape::square_circle;
  float child_spacing = 0.0f;

  [[nodiscard]] View spacing(float value) && {
    child_spacing = value;
    return std::move(*this);
  }

  View &spacing(float value) & {
    child_spacing = value;
    return *this;
  }

  [[nodiscard]] View padding(Insets value) && {
    content_padding = value;
    return std::move(*this);
  }

  View &padding(Insets value) & {
    content_padding = value;
    return *this;
  }

  [[nodiscard]] View after_leading_window_controls(float spacing = 12.0f) && {
    leading_window_controls_placement = {
        .is_enabled = true,
        .spacing = spacing,
        .aligns_vertical_center = true,
    };
    return std::move(*this);
  }

  View &after_leading_window_controls(float spacing = 12.0f) & {
    leading_window_controls_placement = {
        .is_enabled = true,
        .spacing = spacing,
        .aligns_vertical_center = true,
    };
    return *this;
  }

  [[nodiscard]] View size(Size value) && {
    preferred_size = value;
    return std::move(*this);
  }

  View &size(Size value) & {
    preferred_size = value;
    return *this;
  }

  [[nodiscard]] View shape(ControlShape value) && {
    control_shape = value;
    return std::move(*this);
  }

  View &shape(ControlShape value) & {
    control_shape = value;
    return *this;
  }

  [[nodiscard]] View role(ButtonRole value) && {
    button_role = value;
    return std::move(*this);
  }

  View &role(ButtonRole value) & {
    button_role = value;
    return *this;
  }

  [[nodiscard]] View enabled(bool value) && {
    is_enabled = value;
    return std::move(*this);
  }

  View &enabled(bool value) & {
    is_enabled = value;
    return *this;
  }

  [[nodiscard]] View accessibility_label(std::string_view value) && {
    accessibility_label_text = value;
    return std::move(*this);
  }

  View &accessibility_label(std::string_view value) & {
    accessibility_label_text = value;
    return *this;
  }
};

inline constexpr MaterialSymbolIcon
ToMaterialSymbolIcon(Symbol symbol) noexcept {
  switch (symbol) {
  case Symbol::chevron_left:
    return MaterialSymbolIcon::chevron_left;
  case Symbol::chevron_right:
    return MaterialSymbolIcon::chevron_right;
  }
  return MaterialSymbolIcon::chevron_left;
}

inline View empty() { return {}; }

inline View spacer() {
  View view;
  view.kind = ViewKind::spacer;
  return view;
}

inline View icon(Symbol symbol, SymbolOptions options = {}) {
  View view;
  view.kind = ViewKind::icon;
  view.symbol = symbol;
  view.symbol_options = options;
  view.preferred_size = {options.optical_size, options.optical_size};
  return view;
}

inline View button(View label) {
  View view;
  view.kind = ViewKind::button;
  view.children.emplace_back(std::move(label));
  view.preferred_size = {40.0f, 36.0f};
  return view;
}

template <typename... Children> View button_group(Children &&...children) {
  View view;
  view.kind = ViewKind::button_group;
  view.axis = LayoutAxis::horizontal;
  view.control_shape = ControlShape::square_circle;
  (view.children.emplace_back(std::forward<Children>(children)), ...);
  return view;
}

namespace layout {

template <typename... Children>
View stack(LayoutAxis axis, Children &&...children) {
  View view;
  view.kind = ViewKind::stack;
  view.axis = axis;
  (view.children.emplace_back(std::forward<Children>(children)), ...);
  return view;
}

template <typename... Children> View hstack(Children &&...children) {
  return stack(LayoutAxis::horizontal, std::forward<Children>(children)...);
}

template <typename... Children> View vstack(Children &&...children) {
  return stack(LayoutAxis::vertical, std::forward<Children>(children)...);
}

template <typename... Children> View zstack(Children &&...children) {
  return stack(LayoutAxis::overlay, std::forward<Children>(children)...);
}

} // namespace layout

} // namespace phenotype::ui

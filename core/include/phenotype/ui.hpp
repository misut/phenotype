#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <functional>
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

struct Color {
  float red = 0.0f;
  float green = 0.0f;
  float blue = 0.0f;
  float alpha = 1.0f;
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
  folder,
  description,
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
  panel,
  text,
  grid,
};

enum class LayoutAxis {
  horizontal,
  vertical,
  overlay,
};

enum class TextOverflow {
  clip,
  ellipsis,
};

enum class TextTruncation {
  head,
  middle,
  tail,
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
  std::string text_content;
  std::string accessibility_label_text;
  Size preferred_size;
  Insets content_padding;
  Color background_color;
  Color foreground_color = {0.13f, 0.15f, 0.18f, 1.0f};
  LeadingWindowControlsPlacement leading_window_controls_placement;
  ControlShape control_shape = ControlShape::square_circle;
  float font_size_value = 17.0f;
  float font_weight_value = 400.0f;
  float corner_radius_value = 0.0f;
  float child_spacing = 0.0f;
  float grid_min_column_width = 112.0f;
  float grid_row_height = 104.0f;
  float grid_column_gap = 20.0f;
  float grid_row_gap = 22.0f;
  int text_line_limit = 0;
  TextOverflow text_overflow = TextOverflow::clip;
  TextTruncation text_truncation = TextTruncation::tail;
  bool centers_children = false;
  bool centers_text = false;
  bool expands_width = false;
  bool expands_height = false;
  std::function<void()> click_action;

  [[nodiscard]] View spacing(float value) && {
    child_spacing = value;
    return std::move(*this);
  }

  View &spacing(float value) & {
    child_spacing = value;
    return *this;
  }

  [[nodiscard]] View grid_metrics(float min_column_width, float row_height,
                                  float column_gap = 20.0f,
                                  float row_gap = 22.0f) && {
    grid_min_column_width = min_column_width;
    grid_row_height = row_height;
    grid_column_gap = column_gap;
    grid_row_gap = row_gap;
    return std::move(*this);
  }

  View &grid_metrics(float min_column_width, float row_height,
                     float column_gap = 20.0f, float row_gap = 22.0f) & {
    grid_min_column_width = min_column_width;
    grid_row_height = row_height;
    grid_column_gap = column_gap;
    grid_row_gap = row_gap;
    return *this;
  }

  [[nodiscard]] View center_children() && {
    centers_children = true;
    return std::move(*this);
  }

  View &center_children() & {
    centers_children = true;
    return *this;
  }

  [[nodiscard]] View center_text() && {
    centers_text = true;
    return std::move(*this);
  }

  View &center_text() & {
    centers_text = true;
    return *this;
  }

  [[nodiscard]] View line_limit(int value) && {
    text_line_limit = value < 0 ? 0 : value;
    return std::move(*this);
  }

  View &line_limit(int value) & {
    text_line_limit = value < 0 ? 0 : value;
    return *this;
  }

  [[nodiscard]] View overflow(TextOverflow value) && {
    text_overflow = value;
    return std::move(*this);
  }

  View &overflow(TextOverflow value) & {
    text_overflow = value;
    return *this;
  }

  [[nodiscard]] View truncation(TextTruncation value) && {
    text_truncation = value;
    return std::move(*this);
  }

  View &truncation(TextTruncation value) & {
    text_truncation = value;
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

  [[nodiscard]] View expand() && {
    expands_width = true;
    expands_height = true;
    return std::move(*this);
  }

  View &expand() & {
    expands_width = true;
    expands_height = true;
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

  [[nodiscard]] View corner_radius(float value) && {
    corner_radius_value = value;
    return std::move(*this);
  }

  View &corner_radius(float value) & {
    corner_radius_value = value;
    return *this;
  }

  [[nodiscard]] View font_size(float value) && {
    font_size_value = value;
    return std::move(*this);
  }

  View &font_size(float value) & {
    font_size_value = value;
    return *this;
  }

  [[nodiscard]] View font_weight(float value) && {
    font_weight_value = value;
    return std::move(*this);
  }

  View &font_weight(float value) & {
    font_weight_value = value;
    return *this;
  }

  [[nodiscard]] View foreground(Color value) && {
    foreground_color = value;
    return std::move(*this);
  }

  View &foreground(Color value) & {
    foreground_color = value;
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

  [[nodiscard]] View on_click(std::function<void()> action) && {
    click_action = std::move(action);
    return std::move(*this);
  }

  View &on_click(std::function<void()> action) & {
    click_action = std::move(action);
    return *this;
  }
};

class Block {
public:
  std::vector<View> children;

  Block &operator<<(View value) & {
    children.emplace_back(std::move(value));
    return *this;
  }
};

template <typename T>
concept ViewValue = requires(T &&value) { View{std::forward<T>(value)}; };

template <typename T>
concept BlockContent =
    requires(T &&content, Block &block) { std::forward<T>(content)(block); };

template <BlockContent Content>
std::vector<View> BuildChildren(Content &&content) {
  Block block;
  std::forward<Content>(content)(block);
  return std::move(block.children);
}

inline constexpr MaterialSymbolIcon
ToMaterialSymbolIcon(Symbol symbol) noexcept {
  switch (symbol) {
  case Symbol::chevron_left:
    return MaterialSymbolIcon::chevron_left;
  case Symbol::chevron_right:
    return MaterialSymbolIcon::chevron_right;
  case Symbol::folder:
    return MaterialSymbolIcon::folder;
  case Symbol::description:
    return MaterialSymbolIcon::description;
  }
  return MaterialSymbolIcon::chevron_left;
}

inline View empty() { return {}; }

inline View spacer() {
  View view;
  view.kind = ViewKind::spacer;
  view.expands_width = true;
  view.expands_height = true;
  return view;
}

inline constexpr Color white() noexcept { return {1.0f, 1.0f, 1.0f, 1.0f}; }

inline constexpr Color control_background() noexcept {
  return {0.985f, 0.988f, 0.992f, 0.72f};
}

inline constexpr Color primary_label() noexcept {
  return {0.13f, 0.15f, 0.18f, 1.0f};
}

inline View panel(Color color) {
  View view;
  view.kind = ViewKind::panel;
  view.background_color = color;
  return view;
}

inline View text(std::string_view content) {
  View view;
  view.kind = ViewKind::text;
  view.text_content = content;
  view.foreground_color = primary_label();
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

template <BlockContent Content> View button(Content &&content) {
  View view;
  view.kind = ViewKind::button;
  view.children = BuildChildren(std::forward<Content>(content));
  view.preferred_size = {40.0f, 36.0f};
  return view;
}

template <ViewValue... Children> View button_group(Children &&...children) {
  View view;
  view.kind = ViewKind::button_group;
  view.axis = LayoutAxis::horizontal;
  view.control_shape = ControlShape::square_circle;
  (view.children.emplace_back(std::forward<Children>(children)), ...);
  return view;
}

template <BlockContent Content> View button_group(Content &&content) {
  View view;
  view.kind = ViewKind::button_group;
  view.axis = LayoutAxis::horizontal;
  view.control_shape = ControlShape::square_circle;
  view.children = BuildChildren(std::forward<Content>(content));
  return view;
}

namespace layout {

template <ViewValue... Children>
View stack(LayoutAxis axis, Children &&...children) {
  View view;
  view.kind = ViewKind::stack;
  view.axis = axis;
  (view.children.emplace_back(std::forward<Children>(children)), ...);
  return view;
}

template <BlockContent Content> View stack(LayoutAxis axis, Content &&content) {
  View view;
  view.kind = ViewKind::stack;
  view.axis = axis;
  view.children = BuildChildren(std::forward<Content>(content));
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

template <ViewValue... Children> View grid(Children &&...children) {
  View view;
  view.kind = ViewKind::grid;
  (view.children.emplace_back(std::forward<Children>(children)), ...);
  return view;
}

template <BlockContent Content> View grid(Content &&content) {
  View view;
  view.kind = ViewKind::grid;
  view.children = BuildChildren(std::forward<Content>(content));
  return view;
}

} // namespace layout

} // namespace phenotype::ui

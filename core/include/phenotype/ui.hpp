#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <cstddef>
#include <cstdint>
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
  search,
  close,
};

enum class ButtonRole {
  normal,
  back,
  forward,
};

enum class ToggleStyle {
  checkbox,
  radio,
  switcher,
};

// Glass material thickness for a visual_effect_panel. Thinner materials let
// more of the sharp backdrop through; thicker ones frost it more and tint
// harder. `none` is an opaque fallback fill (no backdrop sampling).
enum class MaterialKind {
  none,
  clear,
  thin,
  regular,
  thick,
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
  visual_effect_panel,
  text,
  grid,
  scroll,
  toggle,
  text_field,
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

// UTF-8 boundary helpers for text-field caret math. A byte is a continuation
// byte when its top bits are 10xxxxxx; a caret may only sit on a lead byte or
// the end of the string, so edits never split a multi-byte codepoint. Defined
// ahead of View because its caret/selection builders clamp through them.
inline bool IsUtf8Continuation(char byte) noexcept {
  return (static_cast<unsigned char>(byte) & 0xC0u) == 0x80u;
}

// Snap a byte offset down to the nearest codepoint boundary, clamped to [0,len].
inline std::size_t ClampToCharBoundary(std::string_view text, std::size_t index) noexcept {
  if (index >= text.size()) {
    return text.size();
  }
  while (index > 0 && IsUtf8Continuation(text[index])) {
    --index;
  }
  return index;
}

// The boundary one codepoint before `index` (for a left-arrow / backspace).
inline std::size_t PrevCharBoundary(std::string_view text, std::size_t index) noexcept {
  index = ClampToCharBoundary(text, index);
  if (index == 0) {
    return 0;
  }
  --index;
  while (index > 0 && IsUtf8Continuation(text[index])) {
    --index;
  }
  return index;
}

// The boundary one codepoint after `index` (for a right-arrow / delete).
inline std::size_t NextCharBoundary(std::string_view text, std::size_t index) noexcept {
  index = ClampToCharBoundary(text, index);
  if (index >= text.size()) {
    return text.size();
  }
  ++index;
  while (index < text.size() && IsUtf8Continuation(text[index])) {
    ++index;
  }
  return index;
}

// A single edit intent the platform shell delivers to a focused text field. The
// shell translates raw key / IME events into these; a component applies them to
// its bound text with the UTF-8 caret helpers above.
struct TextEdit {
  enum class Kind {
    insert,          // insert `text` at the caret, replacing any selection
    delete_backward, // backspace
    delete_forward,  // forward delete
    move_left,       // caret one codepoint left
    move_right,      // caret one codepoint right
    move_home,       // caret to the start
    move_end,        // caret to the end
    select_all,      // select the whole field
  };
  Kind kind = Kind::insert;
  std::string text;              // payload for insert
  bool extend_selection = false; // shift held, for the move_* kinds
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
  std::string placeholder_text;
  std::string accessibility_label_text;
  Size preferred_size;
  Insets content_padding;
  Color background_color;
  Color foreground_color = {0.13f, 0.15f, 0.18f, 1.0f};
  LeadingWindowControlsPlacement leading_window_controls_placement;
  ControlShape control_shape = ControlShape::square_circle;
  ToggleStyle toggle_style = ToggleStyle::checkbox;
  MaterialKind material = MaterialKind::regular;
  bool is_on = false;
  // Text-field editing state. caret_position / selection_anchor are byte
  // offsets into text_content; equal means a collapsed caret (no selection).
  std::size_t caret_position = 0;
  std::size_t selection_anchor = 0;
  bool is_focused = false;
  // Whether the caret is drawn this frame. The field stays focused (and an
  // input target) regardless; the component toggles this for a blink.
  bool caret_visible = true;
  float font_size_value = 17.0f;
  float font_weight_value = 400.0f;
  float corner_radius_value = 0.0f;
  float child_spacing = 0.0f;
  float grid_min_column_width = 112.0f;
  float grid_row_height = 104.0f;
  float grid_column_gap = 20.0f;
  float grid_row_gap = 22.0f;
  std::size_t grid_item_offset_value = 0;
  std::size_t grid_total_item_count_value = 0;
  float scroll_offset_y_value = 0.0f;
  float scroll_content_offset_y_value = 0.0f;
  float scroll_range_headroom_y_value = 0.0f;
  int text_line_limit = 0;
  TextOverflow text_overflow = TextOverflow::clip;
  TextTruncation text_truncation = TextTruncation::tail;
  bool centers_children = false;
  bool centers_text = false;
  bool expands_width = false;
  bool expands_height = false;
  bool rounds_top_corners_only = false;
  bool rounds_bottom_corners_only = false;
  std::function<void()> click_action;
  std::function<void(float)> scroll_action;
  std::function<void(const TextEdit &)> text_edit_action;
  std::uint64_t view_key = 0;

  [[nodiscard]] View key(std::uint64_t value) && {
    view_key = value;
    return std::move(*this);
  }

  View &key(std::uint64_t value) & {
    view_key = value;
    return *this;
  }

  [[nodiscard]] View spacing(float value) && {
    child_spacing = value;
    return std::move(*this);
  }

  View &spacing(float value) & {
    child_spacing = value;
    return *this;
  }

  [[nodiscard]] View grid_metrics(float min_column_width, float row_height,
      float column_gap = 20.0f, float row_gap = 22.0f) && {
    grid_min_column_width = min_column_width;
    grid_row_height = row_height;
    grid_column_gap = column_gap;
    grid_row_gap = row_gap;
    return std::move(*this);
  }

  View &grid_metrics(
      float min_column_width, float row_height, float column_gap = 20.0f, float row_gap = 22.0f) & {
    grid_min_column_width = min_column_width;
    grid_row_height = row_height;
    grid_column_gap = column_gap;
    grid_row_gap = row_gap;
    return *this;
  }

  [[nodiscard]] View grid_virtual_range(std::size_t item_offset, std::size_t total_item_count) && {
    grid_item_offset_value = item_offset;
    grid_total_item_count_value = total_item_count;
    return std::move(*this);
  }

  View &grid_virtual_range(std::size_t item_offset, std::size_t total_item_count) & {
    grid_item_offset_value = item_offset;
    grid_total_item_count_value = total_item_count;
    return *this;
  }

  [[nodiscard]] View scroll_offset(float value) && {
    scroll_offset_y_value = value;
    return std::move(*this);
  }

  View &scroll_offset(float value) & {
    scroll_offset_y_value = value;
    return *this;
  }

  [[nodiscard]] View scroll_content_offset(float value) && {
    scroll_content_offset_y_value = value;
    return std::move(*this);
  }

  View &scroll_content_offset(float value) & {
    scroll_content_offset_y_value = value;
    return *this;
  }

  [[nodiscard]] View scroll_range_headroom(float value) && {
    scroll_range_headroom_y_value = value;
    return std::move(*this);
  }

  View &scroll_range_headroom(float value) & {
    scroll_range_headroom_y_value = value;
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

  [[nodiscard]] View expand_width() && {
    expands_width = true;
    return std::move(*this);
  }

  View &expand_width() & {
    expands_width = true;
    return *this;
  }

  [[nodiscard]] View expand_height() && {
    expands_height = true;
    return std::move(*this);
  }

  View &expand_height() & {
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

  [[nodiscard]] View material_kind(MaterialKind value) && {
    material = value;
    return std::move(*this);
  }

  View &material_kind(MaterialKind value) & {
    material = value;
    return *this;
  }

  [[nodiscard]] View focused(bool value) && {
    is_focused = value;
    return std::move(*this);
  }

  View &focused(bool value) & {
    is_focused = value;
    return *this;
  }

  [[nodiscard]] View show_caret(bool value) && {
    caret_visible = value;
    return std::move(*this);
  }

  View &show_caret(bool value) & {
    caret_visible = value;
    return *this;
  }

  // Place the caret and selection from byte offsets, snapped to UTF-8
  // boundaries and clamped to the current text. A single argument collapses the
  // selection at the caret.
  [[nodiscard]] View selection(std::size_t caret, std::size_t anchor) && {
    caret_position = ClampToCharBoundary(text_content, caret);
    selection_anchor = ClampToCharBoundary(text_content, anchor);
    return std::move(*this);
  }

  [[nodiscard]] View caret(std::size_t position) && {
    position = ClampToCharBoundary(text_content, position);
    caret_position = position;
    selection_anchor = position;
    return std::move(*this);
  }

  [[nodiscard]] View corner_radius(float value) && {
    corner_radius_value = value;
    return std::move(*this);
  }

  View &corner_radius(float value) & {
    corner_radius_value = value;
    return *this;
  }

  [[nodiscard]] View top_corners_only() && {
    rounds_top_corners_only = true;
    rounds_bottom_corners_only = false;
    return std::move(*this);
  }

  View &top_corners_only() & {
    rounds_top_corners_only = true;
    rounds_bottom_corners_only = false;
    return *this;
  }

  [[nodiscard]] View bottom_corners_only() && {
    rounds_top_corners_only = false;
    rounds_bottom_corners_only = true;
    return std::move(*this);
  }

  View &bottom_corners_only() & {
    rounds_top_corners_only = false;
    rounds_bottom_corners_only = true;
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

  [[nodiscard]] View on_scroll(std::function<void(float)> action) && {
    scroll_action = std::move(action);
    return std::move(*this);
  }

  View &on_scroll(std::function<void(float)> action) & {
    scroll_action = std::move(action);
    return *this;
  }

  [[nodiscard]] View on_text_edit(std::function<void(const TextEdit &)> action) && {
    text_edit_action = std::move(action);
    return std::move(*this);
  }

  View &on_text_edit(std::function<void(const TextEdit &)> action) & {
    text_edit_action = std::move(action);
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
concept BlockContent = requires(T &&content, Block &block) { std::forward<T>(content)(block); };

template <BlockContent Content> std::vector<View> BuildChildren(Content &&content) {
  Block block;
  std::forward<Content>(content)(block);
  return std::move(block.children);
}

inline constexpr MaterialSymbolIcon ToMaterialSymbolIcon(Symbol symbol) noexcept {
  switch (symbol) {
  case Symbol::chevron_left:
    return MaterialSymbolIcon::chevron_left;
  case Symbol::chevron_right:
    return MaterialSymbolIcon::chevron_right;
  case Symbol::folder:
    return MaterialSymbolIcon::folder;
  case Symbol::description:
    return MaterialSymbolIcon::description;
  case Symbol::search:
    return MaterialSymbolIcon::search;
  case Symbol::close:
    return MaterialSymbolIcon::close;
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

// A toggle control (checkbox, radio, or switch). The control box is fixed-size
// per style; the layout pass lowers it into plain panel draw commands, so it
// renders on every backend without a dedicated scene record. click_action is
// wired by the component builder to flip the bound value.
inline View toggle(ToggleStyle style, bool is_on) {
  View view;
  view.kind = ViewKind::toggle;
  view.toggle_style = style;
  view.is_on = is_on;
  view.preferred_size = style == ToggleStyle::switcher ? Size{36.0f, 22.0f} : Size{18.0f, 18.0f};
  return view;
}

// A single-line text input bound by the caller to a string value. caret defaults
// to the end of the text; selection is collapsed. Editing/focus is wired by the
// platform shell in a later slice; this slice renders the value, placeholder,
// caret, and selection from the editing state.
inline View text_field(std::string_view value, std::string_view placeholder = {}) {
  View view;
  view.kind = ViewKind::text_field;
  view.text_content = value;
  view.placeholder_text = placeholder;
  view.caret_position = value.size();
  view.selection_anchor = value.size();
  view.font_size_value = 16.0f;
  // A field fills the width it is given; its intrinsic size sets the height.
  view.expands_width = true;
  return view;
}

inline constexpr Color white() noexcept { return {1.0f, 1.0f, 1.0f, 1.0f}; }

inline constexpr Color control_background() noexcept { return {0.985f, 0.988f, 0.992f, 0.72f}; }

inline constexpr Color primary_label() noexcept { return {0.13f, 0.15f, 0.18f, 1.0f}; }

inline constexpr Color disabled_label() noexcept { return {0.62f, 0.65f, 0.70f, 1.0f}; }

inline constexpr Color toolbar_material() noexcept { return {0.985f, 0.988f, 0.992f, 0.38f}; }

// Tint for an engaged control (a checked box, a switched-on track). A saturated
// system blue that reads against both the light track and white knob/marks.
inline constexpr Color control_accent() noexcept { return {0.0f, 0.48f, 1.0f, 1.0f}; }

// Outline tint for a disengaged control's track.
inline constexpr Color control_outline() noexcept { return {0.78f, 0.80f, 0.84f, 1.0f}; }

// Fill for a text field's track.
inline constexpr Color field_background() noexcept { return {1.0f, 1.0f, 1.0f, 0.92f}; }

// Selection highlight behind selected text (a translucent accent).
inline constexpr Color selection_highlight() noexcept { return {0.0f, 0.48f, 1.0f, 0.28f}; }

// How strongly a material frosts its backdrop: 0 leaves the sharp backdrop
// visible, 1 fully blurs it. The renderer lerps the sampled scene toward its
// blurred copy by this amount, so a single blur pass serves every thickness.
// regular and thick fully frost (the default look before materials existed);
// clear and thin let progressively more of the sharp backdrop through.
inline constexpr float MaterialBlurAmount(MaterialKind kind) noexcept {
  switch (kind) {
  case MaterialKind::none:
    return 0.0f;
  case MaterialKind::clear:
    return 0.5f;
  case MaterialKind::thin:
    return 0.8f;
  case MaterialKind::regular:
    return 1.0f;
  case MaterialKind::thick:
    return 1.0f;
  }
  return 1.0f;
}

// How hard a material tints the frosted backdrop toward the panel color.
// regular matches the pre-material tint (the control_background alpha the
// effect shader used), so a default glass panel looks unchanged; thicker leans
// harder on the tint for contrast, thinner backs off.
inline constexpr float MaterialTintStrength(MaterialKind kind) noexcept {
  switch (kind) {
  case MaterialKind::none:
    return 1.0f;
  case MaterialKind::clear:
    return 0.45f;
  case MaterialKind::thin:
    return 0.6f;
  case MaterialKind::regular:
    return 0.72f;
  case MaterialKind::thick:
    return 0.85f;
  }
  return 0.72f;
}

inline constexpr float default_chrome_margin() noexcept { return 12.0f; }

inline constexpr float default_toolbar_height() noexcept { return 36.0f; }

inline constexpr float default_toolbar_spacing() noexcept { return 24.0f; }

inline constexpr float default_surface_corner_radius() noexcept { return 18.0f; }

inline constexpr float default_toolbar_blur_height() noexcept {
  return default_toolbar_height() + (default_chrome_margin() * 2.0f);
}

inline constexpr SymbolOptions navigation_symbol_options() noexcept {
  return {
      .fill = false,
      .weight = 200.0f,
      .grade = 0.0f,
      .optical_size = 30.0f,
  };
}

inline constexpr Color navigation_chevron_color(bool is_enabled) noexcept {
  return is_enabled ? primary_label() : disabled_label();
}

inline View panel(Color color) {
  View view;
  view.kind = ViewKind::panel;
  view.background_color = color;
  return view;
}

inline View visual_effect_panel(Color fallback_color = control_background()) {
  View view;
  view.kind = ViewKind::visual_effect_panel;
  view.background_color = fallback_color;
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

inline View content_surface_panel(Color color = control_background()) {
  return panel(color).corner_radius(default_surface_corner_radius());
}

inline View toolbar_blur_panel(Color color = toolbar_material()) {
  return visual_effect_panel(color)
      .corner_radius(default_surface_corner_radius())
      .top_corners_only()
      .size({0.0f, default_toolbar_blur_height()})
      .expand_width();
}

inline View toolbar_title(std::string_view content) {
  return text(content).font_size(16.0f).font_weight(500.0f).foreground(primary_label());
}

inline View navigation_button(Symbol symbol, ButtonRole role, bool is_enabled,
    std::function<void()> action, std::string_view accessibility_label) {
  return button(
      icon(symbol, navigation_symbol_options()).foreground(navigation_chevron_color(is_enabled)))
      .role(role)
      .enabled(is_enabled)
      .on_click(std::move(action))
      .accessibility_label(accessibility_label);
}

inline View navigation_button_group(bool can_navigate_back, std::function<void()> back_action,
    bool can_navigate_forward, std::function<void()> forward_action) {
  return button_group([&](Block &group) {
    group << navigation_button(
        Symbol::chevron_left, ButtonRole::back, can_navigate_back, std::move(back_action), "Back");
    group << navigation_button(Symbol::chevron_right, ButtonRole::forward, can_navigate_forward,
        std::move(forward_action), "Forward");
  }).shape(ControlShape::capsule);
}

namespace layout {

template <ViewValue... Children> View stack(LayoutAxis axis, Children &&...children) {
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

inline View scroll(View content) {
  View view;
  view.kind = ViewKind::scroll;
  view.children.emplace_back(std::move(content));
  view.expands_width = true;
  view.expands_height = true;
  return view;
}

template <BlockContent Content> View scroll(Content &&content) {
  View view;
  view.kind = ViewKind::scroll;
  view.children = BuildChildren(std::forward<Content>(content));
  view.expands_width = true;
  view.expands_height = true;
  return view;
}

} // namespace layout

template <BlockContent Content> View toolbar(Content &&content) {
  return layout::hstack(std::forward<Content>(content))
      .spacing(default_toolbar_spacing())
      .after_leading_window_controls(default_chrome_margin())
      .size({0.0f, default_toolbar_height()})
      .expand_width();
}

} // namespace phenotype::ui

#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#endif

#include "phenotype/ui.hpp"

// Platform-neutral scene contract for phenotype.
//
// A SceneLayout is the resolved, ready-to-paint description of one frame: draw
// layers (panels, symbol buttons, text runs), blur effect panels, and the
// interaction targets (hit and scroll). The layout engine in
// phenotype/layout.hpp produces it; platform backends (Dawn on macOS, GDI on
// Windows) consume it. These "*Layout" records are the draw-command vocabulary
// — they carry geometry already resolved to device-independent points, so a
// backend never needs to know how the tree was laid out.
//
// This header also hosts the geometry and scene helpers backends need (clip
// intersection, hit testing, blur bounds, uniform-packing enum mappings),
// kept free of any ui::View traversal so the contract stays minimal.
namespace phenotype::scene {

struct LayoutRect {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

struct LayoutWindowControls {
  bool has_leading_controls = false;
  LayoutRect leading_controls;
};

struct LayoutContext {
  LayoutWindowControls window_controls;
  std::optional<LayoutRect> clip_rect;
};

struct SymbolButtonLayout {
  LayoutRect frame;
  LayoutRect control_frame;
  ui::Symbol symbol = ui::Symbol::chevron_left;
  ui::SymbolOptions options;
  ui::ControlShape control_shape = ui::ControlShape::square_circle;
  ui::Color color = ui::primary_label();
  bool is_enabled = true;
  bool draws_control = true;
  bool draws_divider = false;
  float divider_x = 0.0f;
  std::optional<LayoutRect> clip_rect;
};

struct PanelLayout {
  LayoutRect frame;
  ui::Color color;
  float corner_radius = 0.0f;
  bool rounds_top_corners_only = false;
  bool rounds_bottom_corners_only = false;
  std::optional<LayoutRect> clip_rect;
};

struct EffectPanelLayout {
  LayoutRect frame;
  ui::Color color;
  float corner_radius = 0.0f;
  bool rounds_top_corners_only = false;
  bool rounds_bottom_corners_only = false;
  // 0 leaves the sharp backdrop, 1 fully frosts it (see ui::MaterialBlurAmount).
  // color.alpha carries the tint strength (ui::MaterialTintStrength).
  float blur_amount = 0.8f;
  std::optional<LayoutRect> clip_rect;
};

struct TextLayout {
  LayoutRect frame;
  std::string content;
  ui::Color color;
  float font_size = 17.0f;
  float font_weight = 400.0f;
  int line_limit = 0;
  ui::TextOverflow overflow = ui::TextOverflow::clip;
  ui::TextTruncation truncation = ui::TextTruncation::tail;
  bool centers_text = false;
  std::optional<LayoutRect> clip_rect;
};

struct HitTargetLayout {
  LayoutRect frame;
  std::function<void()> action;
  bool is_enabled = true;
  std::optional<LayoutRect> clip_rect;
};

// A focused text field's input sink: the shell routes key events into action.
// Emitted only for the focused field, so the shell drives at most one. The
// edit command vocabulary (ui::TextEdit) lives in ui.hpp next to the View.
struct TextInputTargetLayout {
  LayoutRect frame;
  std::function<void(const ui::TextEdit &)> action;
  // The field's current text + selection (byte offsets into text), so the shell
  // can read them for clipboard copy/cut and report positions to the IME.
  std::string text;
  std::size_t selection_begin = 0;
  std::size_t selection_end = 0;
  // Pixel rect of the caret (or selection start) in absolute coordinates, for
  // placing the IME candidate window.
  LayoutRect caret_rect;
  std::optional<LayoutRect> clip_rect;
};

struct ScrollTargetLayout {
  LayoutRect frame;
  float offset_y = 0.0f;
  float content_height = 0.0f;
  float max_offset_y = 0.0f;
  std::function<void(float)> action;
};

struct SceneDrawLayer {
  std::vector<PanelLayout> panels;
  std::vector<SymbolButtonLayout> buttons;
  std::vector<TextLayout> texts;
};

struct SceneLayout {
  SceneDrawLayer background;
  SceneDrawLayer foreground;
  std::vector<EffectPanelLayout> effects;
  std::vector<HitTargetLayout> hit_targets;
  std::vector<ScrollTargetLayout> scroll_targets;
  std::vector<TextInputTargetLayout> text_input_targets;
  bool uses_foreground_layer = false;
};

// Measures the natural pixel size of a text run. Injected by the platform
// shell (Core Text on macOS, GDI on Windows) so the layout engine and scene
// contract stay free of any platform text engine.
using MeasureTextFn =
    std::function<ui::Size(std::string_view content, float font_size, float font_weight)>;

// Initial reserve hints for the per-kind scene vectors. These are no longer
// hard caps: the layout pass pushes every visible record and the macOS renderer
// uploads them to growable storage buffers (the panel/button/text limits were
// removed in the storage-buffer slice). They stay as reserve sizes so a typical
// frame avoids reallocating. kMaxEffectPanelCount IS still a hard cap, because
// the effect/blur shader keeps a fixed-size uniform array.
inline constexpr std::size_t kSymbolButtonReserve = 128;
inline constexpr std::size_t kPanelReserve = 16;
inline constexpr std::size_t kTextReserve = 128;
inline constexpr std::size_t kMaxEffectPanelCount = 8;

inline bool NearlyEqual(float lhs, float rhs) noexcept { return std::abs(lhs - rhs) < 0.5f; }

inline bool NearlyEqual(LayoutRect lhs, LayoutRect rhs) noexcept {
  return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) &&
         NearlyEqual(lhs.width, rhs.width) && NearlyEqual(lhs.height, rhs.height);
}

inline bool NearlyEqual(ui::Size lhs, ui::Size rhs) noexcept {
  return NearlyEqual(lhs.width, rhs.width) && NearlyEqual(lhs.height, rhs.height);
}

inline bool NearlyEqual(std::optional<LayoutRect> lhs, std::optional<LayoutRect> rhs) noexcept {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }
  if (!lhs) {
    return true;
  }
  return NearlyEqual(*lhs, *rhs);
}

inline bool NearlyEqual(LayoutWindowControls lhs, LayoutWindowControls rhs) noexcept {
  if (lhs.has_leading_controls != rhs.has_leading_controls) {
    return false;
  }
  if (!lhs.has_leading_controls) {
    return true;
  }
  return NearlyEqual(lhs.leading_controls, rhs.leading_controls);
}

inline bool NearlyEqual(LayoutContext lhs, LayoutContext rhs) noexcept {
  return NearlyEqual(lhs.window_controls, rhs.window_controls) &&
         NearlyEqual(lhs.clip_rect, rhs.clip_rect);
}

inline LayoutRect Intersect(LayoutRect lhs, LayoutRect rhs) noexcept {
  float min_x = std::max(lhs.x, rhs.x);
  float min_y = std::max(lhs.y, rhs.y);
  float max_x = std::min(lhs.x + lhs.width, rhs.x + rhs.width);
  float max_y = std::min(lhs.y + lhs.height, rhs.y + rhs.height);
  return {
      min_x,
      min_y,
      std::max(0.0f, max_x - min_x),
      std::max(0.0f, max_y - min_y),
  };
}

inline bool HasArea(LayoutRect rect) noexcept { return rect.width > 0.0f && rect.height > 0.0f; }

inline std::optional<LayoutRect> IntersectClip(
    std::optional<LayoutRect> existing, LayoutRect rect) noexcept {
  if (!existing) {
    return rect;
  }
  return Intersect(*existing, rect);
}

inline bool IsVisibleInClip(LayoutRect rect, std::optional<LayoutRect> clip) noexcept {
  if (rect.width <= 0.0f || rect.height <= 0.0f) {
    return false;
  }
  if (!clip) {
    return true;
  }
  return HasArea(Intersect(rect, *clip));
}

inline LayoutRect Union(LayoutRect lhs, LayoutRect rhs) noexcept {
  float min_x = std::min(lhs.x, rhs.x);
  float min_y = std::min(lhs.y, rhs.y);
  float max_x = std::max(lhs.x + lhs.width, rhs.x + rhs.width);
  float max_y = std::max(lhs.y + lhs.height, rhs.y + rhs.height);
  return {
      min_x,
      min_y,
      std::max(0.0f, max_x - min_x),
      std::max(0.0f, max_y - min_y),
  };
}

inline std::optional<LayoutRect> EffectBounds(const std::vector<EffectPanelLayout> &effects) noexcept {
  std::optional<LayoutRect> bounds;
  for (const EffectPanelLayout &effect : effects) {
    LayoutRect frame = effect.frame;
    if (effect.clip_rect) {
      frame = Intersect(frame, *effect.clip_rect);
    }
    if (!HasArea(frame)) {
      continue;
    }
    bounds = bounds ? Union(*bounds, frame) : frame;
  }
  return bounds;
}

inline float ControlShapeValue(ui::ControlShape shape) noexcept {
  switch (shape) {
  case ui::ControlShape::square_circle:
    return 0.0f;
  case ui::ControlShape::capsule:
    return 1.0f;
  }
  return 0.0f;
}

inline float CornerMode(bool top_only, bool bottom_only) noexcept {
  if (bottom_only) {
    return 2.0f;
  }
  if (top_only) {
    return 1.0f;
  }
  return 0.0f;
}

inline bool Contains(LayoutRect rect, ui::Size point) noexcept {
  return point.width >= rect.x && point.width <= rect.x + rect.width && point.height >= rect.y &&
         point.height <= rect.y + rect.height;
}

inline SceneDrawLayer &ActiveDrawLayer(SceneLayout &scene) noexcept {
  return scene.uses_foreground_layer ? scene.foreground : scene.background;
}

} // namespace phenotype::scene

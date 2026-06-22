#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>
#endif

#include "phenotype/scene.hpp"
#include "phenotype/ui.hpp"

// Platform-independent layout engine for phenotype.
//
// This owns the constraints/flex/grid/scroll math: it walks a ui::View tree
// and emits the resolved scene::SceneLayout that platform backends paint. The
// scene data contract and geometry helpers live in phenotype/scene.hpp; this
// header is the algorithm that fills it. Text measurement is the only
// platform-specific input, injected as a scene::MeasureTextFn callback.
namespace phenotype::layout {

// Bring the scene contract (LayoutRect, SceneLayout, MeasureTextFn, the
// geometry helpers, ...) into scope so the algorithm below reads naturally.
using namespace phenotype::scene;

inline ui::Size IntrinsicSize(const MeasureTextFn &measure, const ui::View &view) {
  if (view.preferred_size.width > 0.0f || view.preferred_size.height > 0.0f) {
    return view.preferred_size;
  }

  switch (view.kind) {
  case ui::ViewKind::button:
    return {40.0f, 36.0f};
  case ui::ViewKind::icon:
    return {view.symbol_options.optical_size, view.symbol_options.optical_size};
  case ui::ViewKind::text:
    return measure(view.text_content, view.font_size_value, view.font_weight_value);
  case ui::ViewKind::grid:
    return {view.grid_min_column_width, view.grid_row_height};
  case ui::ViewKind::toggle:
    // The factory always sets preferred_size, handled above; fall back to the
    // style's intrinsic box if a caller cleared it.
    return view.toggle_style == ui::ToggleStyle::switcher ? ui::Size{36.0f, 22.0f}
                                                          : ui::Size{18.0f, 18.0f};
  case ui::ViewKind::scroll:
    break;
  case ui::ViewKind::spacer:
  case ui::ViewKind::empty:
    return {};
  case ui::ViewKind::button_group:
  case ui::ViewKind::panel:
  case ui::ViewKind::visual_effect_panel:
  case ui::ViewKind::stack:
    break;
  }

  ui::Size size;
  bool has_visible_child = false;
  for (const ui::View &child : view.children) {
    ui::Size child_size = IntrinsicSize(measure, child);
    if (view.axis == ui::LayoutAxis::horizontal) {
      if (has_visible_child) {
        size.width += view.child_spacing;
      }
      size.width += child_size.width;
      size.height = std::max(size.height, child_size.height);
    } else if (view.axis == ui::LayoutAxis::vertical) {
      if (has_visible_child) {
        size.height += view.child_spacing;
      }
      size.width = std::max(size.width, child_size.width);
      size.height += child_size.height;
    } else {
      size.width = std::max(size.width, child_size.width);
      size.height = std::max(size.height, child_size.height);
    }
    has_visible_child = true;
  }

  size.width += view.content_padding.left + view.content_padding.right;
  size.height += view.content_padding.top + view.content_padding.bottom;
  return size;
}

inline ui::Size NaturalContentSize(
    const MeasureTextFn &measure, const ui::View &view, float available_width) {
  if (view.preferred_size.width > 0.0f || view.preferred_size.height > 0.0f) {
    return view.preferred_size;
  }

  switch (view.kind) {
  case ui::ViewKind::empty:
  case ui::ViewKind::spacer:
    return {};
  case ui::ViewKind::button:
  case ui::ViewKind::icon:
  case ui::ViewKind::text:
  case ui::ViewKind::button_group:
  case ui::ViewKind::toggle:
    return IntrinsicSize(measure, view);
  case ui::ViewKind::panel:
  case ui::ViewKind::visual_effect_panel:
    return {};
  case ui::ViewKind::scroll:
    if (view.children.empty()) {
      return {};
    }
    return NaturalContentSize(measure, view.children.front(), available_width);
  case ui::ViewKind::grid: {
    float content_width =
        std::max(0.0f, available_width - view.content_padding.left - view.content_padding.right);
    float min_column_width = std::max(1.0f, view.grid_min_column_width);
    float column_gap = std::max(0.0f, view.grid_column_gap);
    float row_height = std::max(1.0f, view.grid_row_height);
    std::size_t column_count = std::max<std::size_t>(
        1, static_cast<std::size_t>((content_width + column_gap) / (min_column_width + column_gap)));
    std::size_t item_count = std::max(
        view.grid_total_item_count_value, view.grid_item_offset_value + view.children.size());
    std::size_t row_count = item_count == 0 ? 0 : (item_count + column_count - 1) / column_count;
    float height = row_count == 0
                       ? 0.0f
                       : row_height * static_cast<float>(row_count) +
                             std::max(0.0f, view.grid_row_gap) * static_cast<float>(row_count - 1);
    return {
        available_width,
        height + view.content_padding.top + view.content_padding.bottom,
    };
  }
  case ui::ViewKind::stack:
    break;
  }

  ui::Size size;
  bool has_visible_child = false;
  float child_width =
      std::max(0.0f, available_width - view.content_padding.left - view.content_padding.right);
  for (const ui::View &child : view.children) {
    ui::Size child_size = NaturalContentSize(measure, child, child_width);
    if (view.axis == ui::LayoutAxis::horizontal) {
      if (has_visible_child) {
        size.width += view.child_spacing;
      }
      size.width += child_size.width;
      size.height = std::max(size.height, child_size.height);
    } else if (view.axis == ui::LayoutAxis::vertical) {
      if (has_visible_child) {
        size.height += view.child_spacing;
      }
      size.width = std::max(size.width, child_size.width);
      size.height += child_size.height;
    } else {
      size.width = std::max(size.width, child_size.width);
      size.height = std::max(size.height, child_size.height);
    }
    has_visible_child = true;
  }

  size.width += view.content_padding.left + view.content_padding.right;
  size.height += view.content_padding.top + view.content_padding.bottom;
  return size;
}

inline const ui::View *FindIconContent(const ui::View &view) {
  if (view.kind == ui::ViewKind::icon) {
    return &view;
  }
  for (const ui::View &child : view.children) {
    if (const ui::View *icon = FindIconContent(child)) {
      return icon;
    }
  }
  return nullptr;
}

inline LayoutRect ContentRect(const ui::View &view, LayoutRect rect) {
  return {
      rect.x + view.content_padding.left,
      rect.y + view.content_padding.top,
      std::max(0.0f, rect.width - view.content_padding.left - view.content_padding.right),
      std::max(0.0f, rect.height - view.content_padding.top - view.content_padding.bottom),
  };
}

void LayoutView(const MeasureTextFn &measure, const ui::View &view, LayoutRect rect,
    const LayoutContext &context, SceneLayout &scene);

inline void LayoutButtonGroup(const MeasureTextFn &measure, const ui::View &view, LayoutRect rect,
    const LayoutContext &context, SceneLayout &scene) {
  LayoutRect content_rect = ContentRect(view, rect);
  float cursor_x = content_rect.x;
  bool draws_control = true;
  std::size_t visible_button_index = 0;
  for (const ui::View &child : view.children) {
    ui::Size child_size = IntrinsicSize(measure, child);
    LayoutRect child_rect{cursor_x, content_rect.y, child_size.width, child_size.height};

    if (child.kind == ui::ViewKind::button) {
      if (child.click_action && child_rect.width > 0.0f && child_rect.height > 0.0f) {
        scene.hit_targets.push_back({
            child_rect,
            child.click_action,
            child.is_enabled,
            context.clip_rect,
        });
      }

      if (!IsVisibleInClip(child_rect, context.clip_rect)) {
        cursor_x += child_size.width + view.child_spacing;
        continue;
      }
      SceneDrawLayer &layer = ActiveDrawLayer(scene);
      if (layer.buttons.size() >= kMaxSymbolButtonCount) {
        return;
      }
      const ui::View *icon = FindIconContent(child);
      if (icon) {
        bool draws_divider = visible_button_index == 0 && view.children.size() > 1;
        layer.buttons.push_back({
            child_rect,
            content_rect,
            icon->symbol,
            icon->symbol_options,
            view.control_shape,
            icon->foreground_color,
            child.is_enabled,
            draws_control,
            draws_divider,
            child_rect.x + child_rect.width + (view.child_spacing * 0.5f),
            context.clip_rect,
        });
        draws_control = false;
        ++visible_button_index;
      }
    }

    cursor_x += child_size.width + view.child_spacing;
  }
}

// Lower a toggle control into plain panel draw commands so it renders on every
// backend with no dedicated scene record. A checkbox/radio is a track panel
// with a centered inner mark when on; a switch is a capsule track with a knob
// panel slid to the trailing edge when on. The caller (LayoutView) has already
// pushed the hit target from view.click_action.
inline void LayoutToggle(const ui::View &view, LayoutRect rect, const LayoutContext &context,
    SceneLayout &scene) {
  if (!IsVisibleInClip(rect, context.clip_rect)) {
    return;
  }
  SceneDrawLayer &layer = ActiveDrawLayer(scene);

  auto push_panel = [&](LayoutRect frame, ui::Color color, float corner_radius) {
    if (layer.panels.size() >= kMaxPanelCount) {
      return;
    }
    layer.panels.push_back({frame, color, corner_radius, false, false, context.clip_rect});
  };

  if (view.toggle_style == ui::ToggleStyle::switcher) {
    float track_radius = rect.height * 0.5f;
    push_panel(rect, view.is_on ? ui::control_accent() : ui::control_outline(), track_radius);
    float inset = std::max(1.0f, rect.height * 0.1f);
    float knob_diameter = rect.height - inset * 2.0f;
    float knob_x = view.is_on ? rect.x + rect.width - inset - knob_diameter : rect.x + inset;
    push_panel({knob_x, rect.y + inset, knob_diameter, knob_diameter}, ui::white(),
        knob_diameter * 0.5f);
    return;
  }

  // Checkbox and radio share a square box; radio rounds to a full circle.
  float box_radius = view.toggle_style == ui::ToggleStyle::radio ? rect.height * 0.5f : 4.0f;
  push_panel(rect, view.is_on ? ui::control_accent() : ui::control_outline(), box_radius);
  if (view.is_on) {
    float mark_inset = std::max(2.0f, rect.width * 0.28f);
    LayoutRect mark{rect.x + mark_inset, rect.y + mark_inset, rect.width - mark_inset * 2.0f,
        rect.height - mark_inset * 2.0f};
    float mark_radius = view.toggle_style == ui::ToggleStyle::radio ? mark.height * 0.5f : 1.0f;
    push_panel(mark, ui::white(), mark_radius);
  }
}

inline void LayoutGrid(const MeasureTextFn &measure, const ui::View &view, LayoutRect rect,
    const LayoutContext &context, SceneLayout &scene) {
  LayoutRect content_rect = ContentRect(view, rect);
  float min_column_width = std::max(1.0f, view.grid_min_column_width);
  float column_gap = std::max(0.0f, view.grid_column_gap);
  float row_gap = std::max(0.0f, view.grid_row_gap);
  std::size_t column_count = std::max<std::size_t>(
      1, static_cast<std::size_t>((content_rect.width + column_gap) / (min_column_width + column_gap)));
  float total_gap = column_gap * static_cast<float>(column_count - 1);
  float cell_width =
      std::max(1.0f, (content_rect.width - total_gap) / static_cast<float>(column_count));
  float row_height = std::max(1.0f, view.grid_row_height);
  float row_stride = row_height + row_gap;
  std::size_t first_visible_item = 0;
  std::size_t last_visible_item = view.grid_item_offset_value + view.children.size();

  if (context.clip_rect && row_stride > 0.0f) {
    const LayoutRect &clip = *context.clip_rect;
    float clip_top = clip.y;
    float clip_bottom = clip.y + clip.height;
    if (clip_bottom <= content_rect.y) {
      last_visible_item = 0;
    } else {
      float first_row_value = std::floor((clip_top - content_rect.y) / row_stride);
      std::size_t first_row = first_row_value <= 0.0f ? 0 : static_cast<std::size_t>(first_row_value);
      if (first_row > 0) {
        --first_row;
      }

      float last_row_value = std::floor((clip_bottom - content_rect.y) / row_stride);
      std::size_t last_row = last_row_value <= 0.0f ? 0 : static_cast<std::size_t>(last_row_value);
      last_row += 2;

      first_visible_item = first_row * column_count;
      last_visible_item = std::min(last_visible_item, last_row * column_count);
    }
  }

  for (std::size_t index = 0; index < view.children.size(); ++index) {
    std::size_t item_index = view.grid_item_offset_value + index;
    if (item_index < first_visible_item || item_index >= last_visible_item) {
      continue;
    }
    std::size_t row = item_index / column_count;
    std::size_t column = item_index % column_count;
    LayoutRect child_rect{
        content_rect.x + (static_cast<float>(column) * (cell_width + column_gap)),
        content_rect.y + (static_cast<float>(row) * (row_height + row_gap)),
        cell_width,
        row_height,
    };
    LayoutView(measure, view.children[index], child_rect, context, scene);
  }
}

inline void LayoutScroll(const MeasureTextFn &measure, const ui::View &view, LayoutRect rect,
    const LayoutContext &context, SceneLayout &scene) {
  LayoutRect viewport = ContentRect(view, rect);
  if (!HasArea(viewport)) {
    return;
  }

  std::optional<LayoutRect> viewport_clip = IntersectClip(context.clip_rect, viewport);
  if (!viewport_clip || !HasArea(*viewport_clip)) {
    return;
  }

  ui::Size natural_size = {};
  for (const ui::View &child : view.children) {
    ui::Size child_size = NaturalContentSize(measure, child, viewport.width);
    natural_size.width = std::max(natural_size.width, child_size.width);
    natural_size.height = std::max(natural_size.height, child_size.height);
  }
  float content_height = std::max(viewport.height, natural_size.height);
  float content_max_offset = std::max(0.0f, content_height - viewport.height);
  bool can_scroll_content = content_max_offset > 0.0f;
  float headroom_offset =
      can_scroll_content ? std::max(0.0f, view.scroll_range_headroom_y_value) : 0.0f;
  float max_offset = content_max_offset + headroom_offset;
  float offset_y = std::clamp(view.scroll_offset_y_value, 0.0f, max_offset);
  float content_offset_y = std::clamp(view.scroll_content_offset_y_value, 0.0f, content_max_offset);

  if (view.scroll_action && max_offset > 0.0f) {
    scene.scroll_targets.push_back({
        viewport,
        offset_y,
        content_height,
        max_offset,
        view.scroll_action,
    });
  }

  LayoutContext child_context = context;
  child_context.clip_rect = viewport_clip;
  LayoutRect child_rect{
      viewport.x,
      viewport.y - content_offset_y,
      viewport.width,
      content_height,
  };
  for (const ui::View &child : view.children) {
    LayoutView(measure, child, child_rect, child_context, scene);
  }
}

inline void LayoutView(const MeasureTextFn &measure, const ui::View &view, LayoutRect rect,
    const LayoutContext &context, SceneLayout &scene) {
  if (view.leading_window_controls_placement.is_enabled &&
      context.window_controls.has_leading_controls) {
    const LayoutRect &controls = context.window_controls.leading_controls;
    ui::Size view_size = IntrinsicSize(measure, view);
    rect.x = std::max(
        rect.x, controls.x + controls.width + view.leading_window_controls_placement.spacing);
    if (view.leading_window_controls_placement.aligns_vertical_center) {
      rect.y = controls.y + (controls.height * 0.5f) - (view_size.height * 0.5f);
    }
  }

  if (view.click_action && rect.width > 0.0f && rect.height > 0.0f &&
      IsVisibleInClip(rect, context.clip_rect)) {
    scene.hit_targets.push_back({
        rect,
        view.click_action,
        view.is_enabled,
        context.clip_rect,
    });
  }

  switch (view.kind) {
  case ui::ViewKind::empty:
  case ui::ViewKind::spacer:
    return;
  case ui::ViewKind::icon:
    if (ActiveDrawLayer(scene).buttons.size() >= kMaxSymbolButtonCount ||
        !IsVisibleInClip(rect, context.clip_rect)) {
      return;
    }
    ActiveDrawLayer(scene).buttons.push_back({
        rect,
        rect,
        view.symbol,
        view.symbol_options,
        view.control_shape,
        view.foreground_color,
        true,
        false,
        false,
        0.0f,
        context.clip_rect,
    });
    return;
  case ui::ViewKind::text:
    if (ActiveDrawLayer(scene).texts.size() >= kMaxTextCount ||
        !IsVisibleInClip(rect, context.clip_rect)) {
      return;
    }
    ActiveDrawLayer(scene).texts.push_back({
        rect,
        view.text_content,
        view.foreground_color,
        view.font_size_value,
        view.font_weight_value,
        view.text_line_limit,
        view.text_overflow,
        view.text_truncation,
        view.centers_text,
        context.clip_rect,
    });
    return;
  case ui::ViewKind::panel:
    if (ActiveDrawLayer(scene).panels.size() >= kMaxPanelCount ||
        !IsVisibleInClip(rect, context.clip_rect)) {
      return;
    }
    ActiveDrawLayer(scene).panels.push_back({
        rect,
        view.background_color,
        view.corner_radius_value,
        view.rounds_top_corners_only,
        view.rounds_bottom_corners_only,
        context.clip_rect,
    });
    return;
  case ui::ViewKind::visual_effect_panel:
    if (scene.effects.size() < kMaxEffectPanelCount && IsVisibleInClip(rect, context.clip_rect)) {
      // The material thickness drives how much the backdrop is frosted and how
      // hard it is tinted; the tint strength rides on the color alpha, which is
      // how the effect shader already reads it.
      ui::Color tint = view.background_color;
      tint.alpha = ui::MaterialTintStrength(view.material);
      scene.effects.push_back({
          rect,
          tint,
          view.corner_radius_value,
          view.rounds_top_corners_only,
          view.rounds_bottom_corners_only,
          ui::MaterialBlurAmount(view.material),
          context.clip_rect,
      });
    }
    scene.uses_foreground_layer = true;
    return;
  case ui::ViewKind::button: {
    if (ActiveDrawLayer(scene).buttons.size() >= kMaxSymbolButtonCount ||
        !IsVisibleInClip(rect, context.clip_rect)) {
      return;
    }
    const ui::View *icon = FindIconContent(view);
    if (!icon) {
      return;
    }
    ActiveDrawLayer(scene).buttons.push_back({
        rect,
        rect,
        icon->symbol,
        icon->symbol_options,
        view.control_shape,
        icon->foreground_color,
        view.is_enabled,
        true,
        false,
        0.0f,
        context.clip_rect,
    });
    return;
  }
  case ui::ViewKind::button_group:
    LayoutButtonGroup(measure, view, rect, context, scene);
    return;
  case ui::ViewKind::grid:
    LayoutGrid(measure, view, rect, context, scene);
    return;
  case ui::ViewKind::scroll:
    LayoutScroll(measure, view, rect, context, scene);
    return;
  case ui::ViewKind::toggle:
    LayoutToggle(view, rect, context, scene);
    return;
  case ui::ViewKind::stack:
    break;
  }

  LayoutRect content_rect = ContentRect(view, rect);

  if (view.axis == ui::LayoutAxis::overlay) {
    for (const ui::View &child : view.children) {
      LayoutView(measure, child, content_rect, context, scene);
    }
    return;
  }

  float available_main =
      view.axis == ui::LayoutAxis::horizontal ? content_rect.width : content_rect.height;
  std::size_t flexible_child_count = 0;
  float fixed_main = view.children.empty()
                         ? 0.0f
                         : view.child_spacing * static_cast<float>(view.children.size() - 1);

  for (const ui::View &child : view.children) {
    ui::Size child_size = IntrinsicSize(measure, child);
    bool expands_on_axis =
        view.axis == ui::LayoutAxis::horizontal ? child.expands_width : child.expands_height;
    if (expands_on_axis) {
      ++flexible_child_count;
    } else {
      fixed_main += view.axis == ui::LayoutAxis::horizontal ? child_size.width : child_size.height;
    }
  }

  float flexible_main = 0.0f;
  if (flexible_child_count > 0) {
    flexible_main =
        std::max(0.0f, available_main - fixed_main) / static_cast<float>(flexible_child_count);
  }

  float cursor_x = content_rect.x;
  float cursor_y = content_rect.y;
  for (const ui::View &child : view.children) {
    ui::Size child_size = IntrinsicSize(measure, child);
    if (view.axis == ui::LayoutAxis::horizontal) {
      if (child.expands_width) {
        child_size.width = flexible_main;
      }
      if (child.expands_height) {
        child_size.height = content_rect.height;
      }
    } else {
      if (child.expands_width) {
        child_size.width = content_rect.width;
      }
      if (child.expands_height) {
        child_size.height = flexible_main;
      }
    }

    float child_x = cursor_x;
    float child_y = cursor_y;
    if (view.axis == ui::LayoutAxis::horizontal && !child.expands_height) {
      child_y = content_rect.y + std::max(0.0f, content_rect.height - child_size.height) * 0.5f;
    } else if (view.axis == ui::LayoutAxis::vertical && view.centers_children &&
               !child.expands_width) {
      child_x = content_rect.x + std::max(0.0f, content_rect.width - child_size.width) * 0.5f;
    }

    LayoutRect child_rect{child_x, child_y, child_size.width, child_size.height};
    LayoutView(measure, child, child_rect, context, scene);
    if (view.axis == ui::LayoutAxis::horizontal) {
      cursor_x += child_size.width + view.child_spacing;
    } else {
      cursor_y += child_size.height + view.child_spacing;
    }
  }
}

inline SceneLayout LayoutScene(const MeasureTextFn &measure, const ui::View &root, float width,
    float height, const LayoutContext &context) {
  SceneLayout scene;
  scene.background.panels.reserve(kMaxPanelCount);
  scene.background.buttons.reserve(kMaxSymbolButtonCount);
  scene.background.texts.reserve(kMaxTextCount);
  scene.foreground.panels.reserve(kMaxPanelCount);
  scene.foreground.buttons.reserve(kMaxSymbolButtonCount);
  scene.foreground.texts.reserve(kMaxTextCount);
  scene.effects.reserve(kMaxEffectPanelCount);
  scene.hit_targets.reserve(64);
  scene.scroll_targets.reserve(8);
  LayoutContext root_context = context;
  if (!root_context.clip_rect) {
    root_context.clip_rect = LayoutRect{0.0f, 0.0f, width, height};
  }
  LayoutView(measure, root,
      {
          0.0f,
          0.0f,
          width,
          height,
      },
      root_context, scene);
  return scene;
}

} // namespace phenotype::layout

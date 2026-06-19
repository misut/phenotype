#pragma once

#include "phenotype/ui.hpp"

// Design tokens for the component API.
//
// The component builders (phenotype/components.hpp) speak in semantic tokens —
// spacing rungs, type ramp, foreground roles — rather than raw floats and
// colors. Each token resolves to the plain ui::View fields the layout engine
// and renderers already consume, so tokens add vocabulary without expanding the
// scene contract. Keeping them in one header makes the ramp easy to retune in
// one place.
namespace phenotype::ui {

// Spacing ramp, in device-independent points. Used for stack gaps and padding.
enum class SpaceToken {
  none,
  xs,
  sm,
  md,
  lg,
  xl,
};

[[nodiscard]] inline constexpr float Space(SpaceToken token) noexcept {
  switch (token) {
  case SpaceToken::none:
    return 0.0f;
  case SpaceToken::xs:
    return 4.0f;
  case SpaceToken::sm:
    return 8.0f;
  case SpaceToken::md:
    return 12.0f;
  case SpaceToken::lg:
    return 20.0f;
  case SpaceToken::xl:
    return 32.0f;
  }
  return 0.0f;
}

[[nodiscard]] inline constexpr Insets SpaceInsets(SpaceToken token) noexcept {
  float value = Space(token);
  return {value, value, value, value};
}

// Foreground role for text. Maps onto the existing label color helpers.
enum class TextColor {
  primary,
  muted,
};

[[nodiscard]] inline constexpr Color Resolve(TextColor color) noexcept {
  switch (color) {
  case TextColor::primary:
    return primary_label();
  case TextColor::muted:
    return disabled_label();
  }
  return primary_label();
}

// Type ramp: a font size paired with a weight. Resolves to the
// font_size_value / font_weight_value fields a TextLayout already carries.
enum class Font {
  caption,
  body,
  title,
};

struct FontMetrics {
  float size = 17.0f;
  float weight = 400.0f;
};

[[nodiscard]] inline constexpr FontMetrics Resolve(Font font) noexcept {
  switch (font) {
  case Font::caption:
    return {13.0f, 450.0f};
  case Font::body:
    return {17.0f, 400.0f};
  case Font::title:
    return {28.0f, 600.0f};
  }
  return {17.0f, 400.0f};
}

} // namespace phenotype::ui

#pragma once

namespace phenotype {

enum class MaterialSymbolIcon {
  chevron_left,
  chevron_right,
};

struct MaterialSymbolOptions {
  bool fill = false;
  float weight = 400.0f;
  float grade = 0.0f;
  float optical_size = 24.0f;
};

struct MaterialSymbolChevronGeometry {
  float outer_x = 0.0f;
  float center_x = 0.0f;
  float top_y = 0.0f;
  float bottom_y = 0.0f;
};

constexpr MaterialSymbolChevronGeometry
MaterialSymbolChevronGeometryFor(MaterialSymbolIcon icon) noexcept {
  switch (icon) {
  case MaterialSymbolIcon::chevron_left:
    return {2.0f, -4.0f, -6.0f, 6.0f};
  case MaterialSymbolIcon::chevron_right:
    return {-2.0f, 4.0f, -6.0f, 6.0f};
  }
  return {};
}

} // namespace phenotype

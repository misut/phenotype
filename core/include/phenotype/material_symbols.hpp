#pragma once

namespace phenotype {

enum class MaterialSymbolIcon {
  chevron_left,
  chevron_right,
  folder,
  description,
  search,
};

struct MaterialSymbolOptions {
  bool fill = false;
  float weight = 400.0f;
  float grade = 0.0f;
  float optical_size = 24.0f;
};

} // namespace phenotype

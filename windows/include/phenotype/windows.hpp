#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <functional>
#include <string>
#include <utility>
#endif

#ifndef PHENOTYPE_WINDOWS_IMPORTS_PHENOTYPE_MODULE
#include "phenotype/ui.hpp"
#endif

namespace phenotype::windows::window {

enum class VisualMaterial {
  desktop_acrylic,
  mica,
  mica_alt,
};

struct BlurBackground {
  VisualMaterial material = VisualMaterial::desktop_acrylic;
};

struct Background {
  enum class Kind {
    system,
    blurred,
  };

  Kind kind = Kind::system;
  BlurBackground blur;

  static constexpr Background system() noexcept { return {}; }

  static constexpr Background blurred(BlurBackground value = {}) noexcept {
    return {Kind::blurred, value};
  }
};

struct Options {
  std::string title;
  ui::Size size = {960.0f, 640.0f};
  Background background = Background::system();
};

struct Spec {
  Options options;
  std::function<ui::View()> content;
};

template <typename Content> Spec create(Options options, Content &&content) {
  return {std::move(options),
          std::function<ui::View()>(std::forward<Content>(content))};
}

} // namespace phenotype::windows::window

extern "C" int phenotype_windows_app_run(
    int argc, char *argv[], phenotype::windows::window::Spec *spec);

namespace phenotype::windows::app {

inline int run(int argc, char *argv[], window::Spec spec) {
  return phenotype_windows_app_run(argc, argv, &spec);
}

} // namespace phenotype::windows::app

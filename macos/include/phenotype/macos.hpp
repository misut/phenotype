#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <functional>
#include <string>
#include <utility>
#endif

#ifndef PHENOTYPE_MACOS_IMPORTS_PHENOTYPE_MODULE
#include "phenotype/ui.hpp"
#endif

namespace phenotype::macos::window {

enum class VisualMaterial {
  under_window_background,
};

enum class TitleBarStyle {
  visible,
  hidden,
};

struct BlurBackground {
  VisualMaterial material = VisualMaterial::under_window_background;
  float opacity = 1.0f;
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

struct WindowControls {
  float vertical_offset = 0.0f;
};

struct Options {
  std::string title;
  ui::Size size = {960.0f, 640.0f};
  TitleBarStyle title_bar = TitleBarStyle::visible;
  Background background = Background::system();
  WindowControls window_controls;
};

struct Spec {
  Options options;
  std::function<ui::View()> content;
};

template <typename Content> Spec create(Options options, Content &&content) {
  return {std::move(options),
          std::function<ui::View()>(std::forward<Content>(content))};
}

} // namespace phenotype::macos::window

extern "C" int phenotype_macos_app_run(int argc, char *argv[],
                                       phenotype::macos::window::Spec *spec);

namespace phenotype::macos::app {

inline int run(int argc, char *argv[], window::Spec spec) {
  return phenotype_macos_app_run(argc, argv, &spec);
}

} // namespace phenotype::macos::app

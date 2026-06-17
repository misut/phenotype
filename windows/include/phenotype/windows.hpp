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

enum class TitleBarStyle {
  visible,
  hidden,
};

struct Background {
  enum class Kind {
    system,
    blurred,
  };

  Kind kind = Kind::system;

  static constexpr Background system() noexcept {
    return {Kind::system};
  }

  static constexpr Background blurred() noexcept {
    return {Kind::blurred};
  }
};

struct Options {
  std::string title;
  ui::Size size = {960.0f, 640.0f};
  TitleBarStyle title_bar = TitleBarStyle::visible;
  Background background = Background::system();
};

struct Spec {
  TitleBarStyle title_bar = TitleBarStyle::visible;
  Options options;
  std::function<ui::View()> content;
};

template <typename Content> Spec create(Options options, Content &&content) {
  Spec spec;
  spec.title_bar = options.title_bar;
  spec.options = std::move(options);
  spec.content = std::function<ui::View()>(std::forward<Content>(content));
  return spec;
}

} // namespace phenotype::windows::window

extern "C" int phenotype_windows_app_run(
    int argc, char *argv[], phenotype::windows::window::Spec *spec,
    int title_bar);

namespace phenotype::windows::app {

inline int run(int argc, char *argv[], window::Spec spec) {
  return phenotype_windows_app_run(argc, argv, &spec,
                                   static_cast<int>(spec.title_bar));
}

} // namespace phenotype::windows::app

#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <functional>
#include <string>
#include <utility>
#endif

#ifndef PHENOTYPE_MACOS_IMPORTS_PHENOTYPE_MODULE
#include "phenotype/runtime.hpp"
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

// Component-app entry point: the modern surface over the window shell.
//
// run_app owns one ui::Runtime for the surface and drives the App's
// body(Context&) through it. The shell already re-invokes the content closure
// after each click/scroll, so a state mutation inside an event handler is
// reflected on the next frame without extra plumbing; the Runtime's rebuild
// hook is wired to the same content path so a future out-of-band State::set
// (e.g. from a timer) can request a frame too.
//
// The App and its Runtime are held by shared_ptr so the content closure (stored
// in the Spec and outliving this call) keeps them alive for the app's lifetime.
namespace phenotype::native {

template <ui::Component App> int run_app(App app, macos::window::Options options) {
  auto runtime = std::make_shared<ui::Runtime>();
  auto holder = std::make_shared<App>(std::move(app));
  macos::window::Spec spec = macos::window::create(options, [runtime, holder] {
    runtime->BeginFrame();
    ui::Context context{*runtime};
    ui::View view = holder->body(context);
    runtime->Prune();
    return view;
  });
  return macos::app::run(0, nullptr, std::move(spec));
}

template <ui::Component App> int run_app(App app, float width, float height, std::string title) {
  macos::window::Options options;
  options.title = std::move(title);
  options.size = {width, height};
  return run_app<App>(std::move(app), std::move(options));
}

} // namespace phenotype::native

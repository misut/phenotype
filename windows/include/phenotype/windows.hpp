#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <functional>
#include <string>
#include <utility>
#endif

#ifndef PHENOTYPE_WINDOWS_IMPORTS_PHENOTYPE_MODULE
#include "phenotype/runtime.hpp"
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

// Component-app entry point: the modern surface over the window shell.
//
// run_app owns one ui::Runtime for the surface and drives the App's
// body(Context&) through it. The shell already re-invokes the content closure
// after each click/scroll, so a state mutation inside an event handler is
// reflected on the next frame without extra plumbing.
//
// The App and its Runtime are held by shared_ptr so the content closure (stored
// in the Spec and outliving this call) keeps them alive for the app's lifetime.
namespace phenotype::native {

template <ui::Component App>
int run_app(App app, windows::window::Options options) {
  auto runtime = std::make_shared<ui::Runtime>();
  auto holder = std::make_shared<App>(std::move(app));
  windows::window::Spec spec =
      windows::window::create(std::move(options), [runtime, holder] {
        runtime->BeginFrame();
        ui::Context context{*runtime};
        ui::View view = holder->body(context);
        runtime->Prune();
        return view;
      });
  return windows::app::run(0, nullptr, std::move(spec));
}

template <ui::Component App>
int run_app(App app, float width, float height, std::string title) {
  windows::window::Options options;
  options.title = std::move(title);
  options.size = {width, height};
  return run_app<App>(std::move(app), std::move(options));
}

} // namespace phenotype::native

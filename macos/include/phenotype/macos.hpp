#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <chrono>
#include <functional>
#include <memory>
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
    solid,
  };

  Kind kind = Kind::system;
  BlurBackground blur;
  ui::Color color = ui::white();

  static constexpr Background system() noexcept { return {}; }

  static constexpr Background blurred(BlurBackground value = {}) noexcept {
    return {Kind::blurred, value};
  }

  // An opaque window backed by a single fill color. Use this when the app
  // wants a flat, non-vibrant surface (no behind-window blur) — the renderer
  // clears the scene to `value` and the window is marked opaque.
  static constexpr Background solid(ui::Color value = ui::white()) noexcept {
    return {Kind::solid, {}, value};
  }
};

// solid() must carry its fill through to the color field, defaulting to opaque
// white — the native shell reads Background::color only for Kind::solid.
static_assert(Background::solid().kind == Background::Kind::solid);
static_assert(Background::solid().color.red == 1.0f &&
              Background::solid().color.green == 1.0f &&
              Background::solid().color.blue == 1.0f &&
              Background::solid().color.alpha == 1.0f);

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
  // Optional: returns true when the last content() build left an animation in
  // flight, so the shell should schedule another frame. Empty for static apps.
  std::function<bool()> wants_animation_frame;
};

template <typename Content> Spec create(Options options, Content &&content) {
  return {std::move(options),
          std::function<ui::View()>(std::forward<Content>(content)), {}};
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
  // A steady clock started at launch supplies body()'s frame time in seconds, so
  // animate_* interpolations advance with wall time. Shared so every rebuild
  // reads the same origin.
  auto epoch = std::make_shared<std::chrono::steady_clock::time_point>(
      std::chrono::steady_clock::now());
  macos::window::Spec spec = macos::window::create(options, [runtime, holder, epoch] {
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - *epoch).count();
    runtime->BeginFrame();
    ui::Context context{*runtime, now};
    ui::View view = holder->body(context);
    runtime->Prune();
    return view;
  });
  // The shell polls this after each build: while an animation is in flight the
  // runtime asks for another frame, and the shell idles once they all settle.
  spec.wants_animation_frame = [runtime] { return runtime->needs_tick(); };
  return macos::app::run(0, nullptr, std::move(spec));
}

template <ui::Component App> int run_app(App app, float width, float height, std::string title) {
  macos::window::Options options;
  options.title = std::move(title);
  options.size = {width, height};
  return run_app<App>(std::move(app), std::move(options));
}

} // namespace phenotype::native

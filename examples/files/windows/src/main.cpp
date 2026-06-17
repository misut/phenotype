import phenotype;
import phenotype.windows;
import std;

namespace ui = phenotype::ui;
namespace windows = phenotype::windows;

struct NavigationState {
  std::vector<std::string> path_history{"Home", "Documents", "Projects"};
  std::size_t history_index = 1;
};

bool CanNavigateBack(const NavigationState &state) {
  return !state.path_history.empty() && state.history_index > 0;
}

bool CanNavigateForward(const NavigationState &state) {
  return state.history_index + 1 < state.path_history.size();
}

void NavigateBack(NavigationState &state) {
  if (!CanNavigateBack(state)) {
    return;
  }
  --state.history_index;
}

void NavigateForward(NavigationState &state) {
  if (!CanNavigateForward(state)) {
    return;
  }
  ++state.history_index;
}

constexpr ui::Color NavigationChevronColor(bool is_enabled) noexcept {
  if (is_enabled) {
    return ui::primary_label();
  }
  return {0.62f, 0.65f, 0.70f, 1.0f};
}

ui::View FilesView(const std::shared_ptr<NavigationState> &state) {
  constexpr float chrome_margin = 12.0f;
  constexpr ui::SymbolOptions navigation_icon_options{
      .fill = false,
      .weight = 200.0f,
      .grade = 0.0f,
      .optical_size = 30.0f,
  };
  bool can_navigate_back = CanNavigateBack(*state);
  bool can_navigate_forward = CanNavigateForward(*state);

  return ui::layout::vstack([&](ui::Block &body) {
    body << ui::layout::hstack([&](ui::Block &toolbar) {
              toolbar << ui::button_group([&](ui::Block &group) {
                group << ui::button(ui::icon(ui::Symbol::chevron_left,
                                             navigation_icon_options)
                                         .foreground(NavigationChevronColor(
                                             can_navigate_back)))
                             .role(ui::ButtonRole::back)
                             .enabled(can_navigate_back)
                             .on_click([state] { NavigateBack(*state); })
                             .accessibility_label("Back");
                group << ui::button(ui::icon(ui::Symbol::chevron_right,
                                             navigation_icon_options)
                                         .foreground(NavigationChevronColor(
                                             can_navigate_forward)))
                             .role(ui::ButtonRole::forward)
                             .enabled(can_navigate_forward)
                             .on_click([state] { NavigateForward(*state); })
                             .accessibility_label("Forward");
              }).shape(ui::ControlShape::capsule);
            })
                .spacing(24.0f)
                .after_leading_window_controls(chrome_margin);
  })
      .padding({chrome_margin, chrome_margin, chrome_margin, chrome_margin});
}

int main(int argc, char *argv[]) {
  auto state = std::make_shared<NavigationState>();
  constexpr windows::window::TitleBarStyle title_bar =
      windows::window::TitleBarStyle::hidden;
  windows::window::Options options;
  options.title = "Files";
  options.size = {960.0f, 640.0f};
  options.title_bar = title_bar;
  options.background = windows::window::Background::blurred();

  return windows::app::run(
      argc, argv,
      windows::window::create(std::move(options), [state] {
        return FilesView(state);
      }));
}

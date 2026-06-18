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

ui::View ContentSurface() {
  return ui::layout::zstack([&](ui::Block &surface) {
    surface << ui::content_surface_panel().expand();
  }).expand();
}

ui::View FilesView(const std::shared_ptr<NavigationState> &state) {
  constexpr float chrome_margin = ui::default_chrome_margin();
  bool can_navigate_back = CanNavigateBack(*state);
  bool can_navigate_forward = CanNavigateForward(*state);

  return ui::layout::vstack([&](ui::Block &body) {
    body << ui::toolbar([&](ui::Block &toolbar) {
      toolbar << ui::navigation_button_group(
          can_navigate_back, [state] { NavigateBack(*state); }, can_navigate_forward,
          [state] { NavigateForward(*state); });
    });
    body << ContentSurface();
  })
      .spacing(chrome_margin)
      .padding({chrome_margin, chrome_margin, chrome_margin, chrome_margin});
}

int main(int argc, char *argv[]) {
  auto state = std::make_shared<NavigationState>();
  constexpr windows::window::TitleBarStyle title_bar = windows::window::TitleBarStyle::hidden;
  windows::window::Options options;
  options.title = "Files";
  options.size = {960.0f, 640.0f};
  options.title_bar = title_bar;
  options.background = windows::window::Background::blurred();

  return windows::app::run(argc, argv,
      windows::window::create(std::move(options), [state] { return FilesView(state); }));
}

import phenotype;
import phenotype.macos;

namespace ui = phenotype::ui;
namespace macos = phenotype::macos;

ui::View FilesView() {
  constexpr ui::SymbolOptions navigation_icon_options{
      .fill = false,
      .weight = 500.0f,
      .grade = 0.0f,
      .optical_size = 24.0f,
  };

  return ui::layout::vstack(
      ui::layout::hstack(ui::button(ui::icon(ui::Symbol::chevron_left,
                                             navigation_icon_options))
                             .role(ui::ButtonRole::back)
                             .accessibility_label("Back"),
                         ui::button(ui::icon(ui::Symbol::chevron_right,
                                             navigation_icon_options))
                             .role(ui::ButtonRole::forward)
                             .accessibility_label("Forward"))
          .spacing(8.0f)
          .padding({.left = 48.0f, .top = 36.0f}),
      ui::spacer());
}

int main(int argc, char *argv[]) {
  return macos::app::run(
      argc, argv,
      macos::window::create(
          {
              .title = "Files",
              .size = {960.0f, 640.0f},
              .title_bar = macos::window::TitleBarStyle::hidden,
              .background = macos::window::Background::blurred(),
          },
          FilesView));
}

import phenotype;
import phenotype.windows;

namespace ui = phenotype::ui;
namespace windows = phenotype::windows;

ui::View FilesView() { return ui::empty(); }

int main(int argc, char *argv[]) {
  return windows::app::run(
      argc, argv,
      windows::window::create(
          {
              .title = "Files",
              .size = {960.0f, 640.0f},
          },
          FilesView));
}

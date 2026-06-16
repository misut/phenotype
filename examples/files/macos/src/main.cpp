import phenotype;
import phenotype.macos;
import std;

namespace ui = phenotype::ui;
namespace macos = phenotype::macos;

struct FileItem {
  std::string name;
  bool is_directory = false;
};

std::filesystem::path HomeDirectory() {
  const char *home = std::getenv("HOME");
  if (home == nullptr || *home == '\0') {
    return std::filesystem::path("~");
  }
  return std::filesystem::path(home);
}

std::string DirectoryName(const std::filesystem::path &path) {
  std::string name = path.filename().string();
  if (!name.empty()) {
    return name;
  }
  return path.string();
}

std::vector<FileItem> DirectoryItems(const std::filesystem::path &path) {
  std::vector<FileItem> items;
  std::error_code error;
  std::filesystem::directory_iterator iterator(
      path, std::filesystem::directory_options::skip_permission_denied, error);
  if (error) {
    return items;
  }

  for (const std::filesystem::directory_entry &entry : iterator) {
    std::string name = entry.path().filename().string();
    if (name.empty() || name.starts_with(".")) {
      continue;
    }

    std::error_code status_error;
    items.push_back({
        .name = std::move(name),
        .is_directory = entry.is_directory(status_error),
    });
  }

  std::ranges::sort(items, [](const FileItem &lhs, const FileItem &rhs) {
    if (lhs.is_directory != rhs.is_directory) {
      return lhs.is_directory && !rhs.is_directory;
    }
    return lhs.name < rhs.name;
  });
  if (items.size() > 36) {
    items.resize(36);
  }
  return items;
}

ui::View FileTile(const FileItem &item) {
  constexpr ui::SymbolOptions folder_icon_options{
      .fill = true,
      .weight = 400.0f,
      .grade = 0.0f,
      .optical_size = 42.0f,
  };
  constexpr ui::SymbolOptions file_icon_options{
      .fill = false,
      .weight = 400.0f,
      .grade = 0.0f,
      .optical_size = 42.0f,
  };
  constexpr ui::Color folder_color{0.0f, 0.48f, 1.0f, 1.0f};
  constexpr ui::Color file_color{0.42f, 0.47f, 0.55f, 1.0f};

  return ui::layout::vstack([&](ui::Block &tile) {
    tile << ui::icon(item.is_directory ? ui::Symbol::folder
                                       : ui::Symbol::description,
                     item.is_directory ? folder_icon_options : file_icon_options)
                .foreground(item.is_directory ? folder_color : file_color);
    tile << ui::text(item.name)
                .font_size(13.0f)
                .font_weight(450.0f)
                .foreground(ui::primary_label())
                .center_text()
                .line_limit(1)
                .overflow(ui::TextOverflow::ellipsis)
                .truncation(ui::TextTruncation::tail)
                .size({96.0f, 18.0f});
  }).spacing(8.0f)
      .center_children()
      .padding({8.0f, 4.0f, 8.0f, 4.0f});
}

ui::View ContentSurface(const std::vector<FileItem> &items) {
  return ui::layout::zstack([&](ui::Block &surface) {
    surface << ui::panel(ui::control_background()).corner_radius(18.0f).expand();
    surface << ui::layout::grid([&](ui::Block &grid) {
                 for (const FileItem &item : items) {
                   grid << FileTile(item);
                 }
               })
                   .grid_metrics(112.0f, 92.0f, 18.0f, 20.0f)
                   .padding({28.0f, 26.0f, 28.0f, 26.0f})
                   .expand();
  }).expand();
}

ui::View FilesView(std::string current_path_name, std::vector<FileItem> items) {
  constexpr ui::SymbolOptions navigation_icon_options{
      .fill = false,
      .weight = 300.0f,
      .grade = 0.0f,
      .optical_size = 24.0f,
  };

  return ui::layout::vstack([&](ui::Block &body) {
    body << ui::layout::hstack([&](ui::Block &toolbar) {
              toolbar << ui::button_group([&](ui::Block &group) {
                group << ui::button(ui::icon(ui::Symbol::chevron_left,
                                             navigation_icon_options))
                             .role(ui::ButtonRole::back)
                             .accessibility_label("Back");
                group << ui::button(ui::icon(ui::Symbol::chevron_right,
                                             navigation_icon_options))
                             .role(ui::ButtonRole::forward)
                             .accessibility_label("Forward");
              }).shape(ui::ControlShape::capsule);
              toolbar << ui::text(current_path_name)
                             .font_size(20.0f)
                             .font_weight(550.0f)
                             .foreground(ui::primary_label());
            })
                .spacing(24.0f)
                .after_leading_window_controls(12.0f);
    body << ContentSurface(items);
  }).spacing(20.0f)
      .padding({24.0f, 0.0f, 24.0f, 24.0f});
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
              .window_controls = {.vertical_offset = 8.0f},
          },
          [] {
            std::filesystem::path home = HomeDirectory();
            return FilesView(DirectoryName(home), DirectoryItems(home));
          }));
}

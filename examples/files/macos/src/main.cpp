import phenotype;
import phenotype.macos;
import std;

namespace ui = phenotype::ui;
namespace macos = phenotype::macos;

struct FileItem {
  std::string name;
  std::filesystem::path path;
  bool is_directory = false;
};

struct FilesState {
  std::filesystem::path current_path;
  std::optional<std::filesystem::path> focused_path;
  std::vector<FileItem> items;
  std::vector<std::filesystem::path> path_history;
  std::size_t history_index = 0;
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
        .path = entry.path(),
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

void SetDirectory(FilesState &state, const std::filesystem::path &path) {
  state.current_path = path;
  state.focused_path.reset();
  state.items = DirectoryItems(state.current_path);
}

void OpenDirectory(FilesState &state, std::filesystem::path path) {
  if (state.path_history.empty()) {
    state.path_history.push_back(path);
    state.history_index = 0;
    SetDirectory(state, state.path_history[state.history_index]);
    return;
  }

  if (state.history_index + 1 < state.path_history.size()) {
    auto next_history =
        state.path_history.begin() +
        static_cast<std::ptrdiff_t>(state.history_index + 1);
    state.path_history.erase(next_history, state.path_history.end());
  }

  if (state.path_history[state.history_index] != path) {
    state.path_history.push_back(std::move(path));
    state.history_index = state.path_history.size() - 1;
  }

  SetDirectory(state, state.path_history[state.history_index]);
}

bool CanNavigateBack(const FilesState &state) {
  return !state.path_history.empty() && state.history_index > 0;
}

bool CanNavigateForward(const FilesState &state) {
  return state.history_index + 1 < state.path_history.size();
}

void NavigateBack(FilesState &state) {
  if (!CanNavigateBack(state)) {
    return;
  }
  --state.history_index;
  SetDirectory(state, state.path_history[state.history_index]);
}

void NavigateForward(FilesState &state) {
  if (!CanNavigateForward(state)) {
    return;
  }
  ++state.history_index;
  SetDirectory(state, state.path_history[state.history_index]);
}

bool IsFocused(const FilesState &state, const FileItem &item) {
  return state.focused_path.has_value() && *state.focused_path == item.path;
}

void FocusOrActivate(FilesState &state, const FileItem &item) {
  if (!IsFocused(state, item)) {
    state.focused_path = item.path;
    return;
  }

  if (item.is_directory) {
    OpenDirectory(state, item.path);
  }
}

constexpr ui::Color NavigationChevronColor(bool is_enabled) noexcept {
  if (is_enabled) {
    return ui::primary_label();
  }
  return {0.62f, 0.65f, 0.70f, 1.0f};
}

ui::View FileTileContent(const FileItem &item) {
  constexpr ui::SymbolOptions folder_icon_options{
      .fill = true,
      .weight = 400.0f,
      .grade = 0.0f,
      .optical_size = 58.0f,
  };
  constexpr ui::SymbolOptions file_icon_options{
      .fill = false,
      .weight = 400.0f,
      .grade = 0.0f,
      .optical_size = 58.0f,
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

ui::View FileTile(const FileItem &item, bool is_focused) {
  if (!is_focused) {
    return FileTileContent(item);
  }

  constexpr ui::Color focus_color{0.0f, 0.48f, 1.0f, 0.12f};
  return ui::layout::zstack([&](ui::Block &tile) {
    tile << ui::panel(focus_color).corner_radius(12.0f).expand();
    tile << FileTileContent(item);
  });
}

ui::View ContentSurface(const std::shared_ptr<FilesState> &state) {
  return ui::layout::zstack([&](ui::Block &surface) {
    surface << ui::panel(ui::control_background()).corner_radius(18.0f).expand();
    surface << ui::layout::grid([&](ui::Block &grid) {
                 for (const FileItem &item : state->items) {
                   ui::View tile = FileTile(item, IsFocused(*state, item));
                   tile.on_click(
                       [state, item] { FocusOrActivate(*state, item); });
                   grid << std::move(tile);
                 }
               })
                   .grid_metrics(112.0f, 92.0f, 18.0f, 20.0f)
                   .padding({28.0f, 26.0f, 28.0f, 26.0f})
                   .expand();
  }).expand();
}

ui::View FilesView(const std::shared_ptr<FilesState> &state) {
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
              toolbar << ui::text(DirectoryName(state->current_path))
                             .font_size(16.0f)
                             .font_weight(500.0f)
                             .foreground(ui::primary_label());
            })
                .spacing(24.0f)
                .after_leading_window_controls(chrome_margin);
    body << ContentSurface(state);
  }).spacing(chrome_margin)
      .padding({chrome_margin, 0.0f, chrome_margin, chrome_margin});
}

int main(int argc, char *argv[]) {
  auto state = std::make_shared<FilesState>();
  OpenDirectory(*state, HomeDirectory());

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
          [state] {
            return FilesView(state);
          }));
}

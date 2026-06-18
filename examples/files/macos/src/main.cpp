import phenotype;
import phenotype.macos;
import std;

namespace ui = phenotype::ui;
namespace macos = phenotype::macos;

constexpr ui::Size FilesWindowSize{960.0f, 640.0f};
constexpr float FileGridMinColumnWidth = 112.0f;
constexpr float FileGridRowHeight = 92.0f;
constexpr float FileGridColumnGap = 18.0f;
constexpr float FileGridRowGap = 20.0f;
constexpr ui::Insets FileGridPadding{28.0f, 26.0f, 28.0f, 26.0f};
constexpr bool ExpandSurfaceSideEdgesWhileScrolling = false;

struct FileItem {
  std::string name;
  std::filesystem::path path;
  bool is_directory = false;
};

struct FileGridVisibleRange {
  std::size_t start = 0;
  std::size_t end = 0;
};

struct FilesState {
  std::filesystem::path current_path;
  std::optional<std::filesystem::path> focused_path;
  std::vector<FileItem> items;
  std::vector<std::filesystem::path> path_history;
  std::size_t history_index = 0;
  float scroll_offset_y = 0.0f;
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
  return items;
}

void SetDirectory(FilesState &state, const std::filesystem::path &path) {
  state.current_path = path;
  state.focused_path.reset();
  state.scroll_offset_y = 0.0f;
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
        state.path_history.begin() + static_cast<std::ptrdiff_t>(state.history_index + 1);
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

std::size_t FileGridColumnCount(float surface_width) {
  float content_width =
      std::max(0.0f, surface_width - FileGridPadding.left - FileGridPadding.right);
  return std::max<std::size_t>(
      1, static_cast<std::size_t>(
             (content_width + FileGridColumnGap) / (FileGridMinColumnWidth + FileGridColumnGap)));
}

float FileGridNaturalHeight(std::size_t item_count, float surface_width) {
  std::size_t column_count = FileGridColumnCount(surface_width);
  std::size_t row_count = item_count == 0 ? 0 : (item_count + column_count - 1) / column_count;
  float rows_height = row_count == 0 ? 0.0f
                                     : FileGridRowHeight * static_cast<float>(row_count) +
                                           FileGridRowGap * static_cast<float>(row_count - 1);
  return rows_height + FileGridPadding.top + FileGridPadding.bottom;
}

FileGridVisibleRange VisibleFileRange(std::size_t item_count, float surface_width,
    float surface_height, float content_scroll_offset) {
  if (item_count == 0 || surface_height <= 0.0f) {
    return {};
  }

  constexpr std::size_t overscan_rows = 2;
  std::size_t column_count = FileGridColumnCount(surface_width);
  std::size_t row_count = (item_count + column_count - 1) / column_count;
  float row_stride = FileGridRowHeight + FileGridRowGap;
  float visible_top = std::max(0.0f, content_scroll_offset - FileGridPadding.top);
  float visible_bottom =
      std::max(0.0f, content_scroll_offset + surface_height - FileGridPadding.top);

  std::size_t first_row =
      std::min(row_count, static_cast<std::size_t>(std::floor(visible_top / row_stride)));
  first_row = first_row > overscan_rows ? first_row - overscan_rows : 0;

  std::size_t last_row = std::min(row_count,
      static_cast<std::size_t>(std::ceil(visible_bottom / row_stride)) + overscan_rows + 1);

  return {
      first_row * column_count,
      std::min(item_count, last_row * column_count),
  };
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
    tile << ui::icon(item.is_directory ? ui::Symbol::folder : ui::Symbol::description,
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
  })
      .spacing(8.0f)
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

ui::View ContentSurface(const std::shared_ptr<FilesState> &state, float content_scroll_offset,
    float scroll_range_headroom, FileGridVisibleRange visible_range) {
  ui::View content = ui::layout::grid([&](ui::Block &grid) {
    for (std::size_t index = visible_range.start; index < visible_range.end; ++index) {
      const FileItem &item = state->items[index];
      ui::View tile = FileTile(item, IsFocused(*state, item));
      tile.on_click([state, item] { FocusOrActivate(*state, item); });
      grid << std::move(tile);
    }
  })
                         .grid_metrics(FileGridMinColumnWidth, FileGridRowHeight, FileGridColumnGap,
                             FileGridRowGap)
                         .grid_virtual_range(visible_range.start, state->items.size())
                         .padding(FileGridPadding)
                         .expand_width();

  ui::View background_panel = ui::content_surface_panel();
  if (content_scroll_offset > ui::default_chrome_margin()) {
    background_panel.bottom_corners_only();
  }
  float side_extension = ExpandSurfaceSideEdgesWhileScrolling
                             ? std::clamp(content_scroll_offset - ui::default_chrome_margin(), 0.0f,
                                   ui::default_chrome_margin())
                             : 0.0f;

  ui::View surface_panel =
      ui::layout::vstack(
          [&](ui::Block &background) { background << std::move(background_panel).expand(); })
          .padding({-side_extension, -content_scroll_offset, -side_extension, 0.0f})
          .expand();

  return ui::layout::zstack([&](ui::Block &surface) {
    surface << std::move(surface_panel);
    surface << ui::layout::scroll(std::move(content))
                   .scroll_offset(state->scroll_offset_y)
                   .scroll_content_offset(content_scroll_offset)
                   .scroll_range_headroom(scroll_range_headroom)
                   .on_scroll([state](float offset_y) { state->scroll_offset_y = offset_y; })
                   .expand();
  }).expand();
}

ui::View FilesView(const std::shared_ptr<FilesState> &state) {
  constexpr float chrome_margin = ui::default_chrome_margin();
  constexpr float toolbar_height = ui::default_toolbar_height();
  constexpr float surface_collapse_distance = toolbar_height + chrome_margin;
  bool can_navigate_back = CanNavigateBack(*state);
  bool can_navigate_forward = CanNavigateForward(*state);
  float surface_width = FilesWindowSize.width - chrome_margin - chrome_margin;
  float content_height = FileGridNaturalHeight(state->items.size(), surface_width);
  float content_max_offset_without_bottom_margin =
      std::max(0.0f, content_height - FilesWindowSize.height);
  float max_offset_without_bottom_margin =
      content_max_offset_without_bottom_margin + surface_collapse_distance;
  float surface_collapse_offset = std::min(state->scroll_offset_y, surface_collapse_distance);
  float content_scroll_offset = std::max(0.0f, state->scroll_offset_y - surface_collapse_distance);
  float toolbar_clearance = surface_collapse_distance - surface_collapse_offset;
  float bottom_margin_while_collapsing = std::max(0.0f, chrome_margin - surface_collapse_offset);
  float bottom_margin_at_scroll_end =
      std::clamp(state->scroll_offset_y - max_offset_without_bottom_margin, 0.0f, chrome_margin);
  float bottom_margin = std::max(bottom_margin_while_collapsing, bottom_margin_at_scroll_end);
  float scroll_range_headroom = surface_collapse_distance + chrome_margin;
  float surface_height =
      std::max(0.0f, FilesWindowSize.height - chrome_margin - bottom_margin - toolbar_clearance);
  FileGridVisibleRange visible_range =
      VisibleFileRange(state->items.size(), surface_width, surface_height, content_scroll_offset);
  bool has_toolbar_backdrop = toolbar_clearance < toolbar_height;

  auto make_toolbar_content = [&] {
    return ui::toolbar([&](ui::Block &toolbar) {
      toolbar << ui::navigation_button_group(
          can_navigate_back, [state] { NavigateBack(*state); }, can_navigate_forward,
          [state] { NavigateForward(*state); });
      toolbar << ui::toolbar_title(DirectoryName(state->current_path));
    });
  };

  return ui::layout::zstack([&](ui::Block &root) {
    root << ui::layout::vstack([&](ui::Block &body) {
      if (toolbar_clearance > 0.0f) {
        body << ui::empty().size({0.0f, toolbar_clearance});
      }
      body << ContentSurface(state, content_scroll_offset, scroll_range_headroom, visible_range);
    })
                .padding({chrome_margin, 0.0f, chrome_margin, bottom_margin})
                .expand();

    root << ui::layout::vstack([&](ui::Block &chrome) {
      ui::View toolbar_content = make_toolbar_content();
      if (has_toolbar_backdrop) {
        chrome << ui::layout::zstack([&](ui::Block &toolbar) {
          toolbar << ui::toolbar_blur_panel();
          toolbar << ui::layout::vstack([&](ui::Block &content) {
            content << std::move(toolbar_content);
            content << ui::spacer();
          }).expand();
        })
                      .size({0.0f, ui::default_toolbar_blur_height()})
                      .expand_width();
      } else {
        chrome << std::move(toolbar_content);
      }
      chrome << ui::spacer();
    })
                .padding({chrome_margin, 0.0f, chrome_margin, 0.0f})
                .expand();
  }).expand();
}

int main(int argc, char *argv[]) {
  auto state = std::make_shared<FilesState>();
  OpenDirectory(*state, HomeDirectory());

  return macos::app::run(argc, argv,
      macos::window::create(
          {
              .title = "Files",
              .size = FilesWindowSize,
              .title_bar = macos::window::TitleBarStyle::hidden,
              .background = macos::window::Background::blurred(),
              .window_controls = {.vertical_offset = 8.0f},
          },
          [state] { return FilesView(state); }));
}

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
  bool searching = false;
  std::string search_query;
  std::size_t search_caret = 0;        // byte offset into search_query
  std::size_t search_anchor = 0;       // selection anchor; == caret when collapsed
  std::string search_marked;           // IME composition in progress (uncommitted)
  std::size_t search_marked_caret = 0; // caret within search_marked
  std::uint64_t search_edit_seq = 0;   // bumped on each edit, to reset the blink
  std::uint64_t blink_seen_seq = 0;    // last edit seq the view observed
  double caret_blink_since = 0.0;      // clock time the caret last moved
};

// Toggle the search affordance: clicking the search button expands it into a
// focused field; collapsing clears the query, caret, and selection.
void ToggleSearch(FilesState &state) {
  state.searching = !state.searching;
  if (!state.searching) {
    state.search_query.clear();
    state.search_caret = 0;
    state.search_anchor = 0;
  }
}

// Apply one edit command from the shell to the search query, caret, and
// selection, using the UTF-8 boundary helpers so multi-byte codepoints are
// never split. A non-empty selection is replaced/removed by inserts and
// deletes, and any caret move collapses it.
void ApplySearchEdit(FilesState &state, const ui::TextEdit &edit) {
  std::string &query = state.search_query;
  std::size_t caret = std::min(state.search_caret, query.size());
  std::size_t anchor = std::min(state.search_anchor, query.size());
  bool has_selection = caret != anchor;
  std::size_t sel_begin = std::min(caret, anchor);
  std::size_t sel_end = std::max(caret, anchor);

  // Drop the selected range and place the caret at its start. Shared by insert
  // and backspace/forward-delete when something is selected.
  auto erase_selection = [&] {
    query.erase(sel_begin, sel_end - sel_begin);
    caret = sel_begin;
  };

  switch (edit.kind) {
  case ui::TextEdit::Kind::set_marked:
    // A composition is in progress: drop any selection on first marked input,
    // then stash the marked string + its caret. The query is untouched until the
    // IME commits (which arrives as an insert).
    if (has_selection && state.search_marked.empty()) {
      erase_selection();
      state.search_caret = caret;
      state.search_anchor = caret;
    }
    state.search_marked = edit.text;
    state.search_marked_caret = std::min(edit.marked_caret, edit.text.size());
    ++state.search_edit_seq;
    return;
  case ui::TextEdit::Kind::unmark:
    state.search_marked.clear();
    state.search_marked_caret = 0;
    ++state.search_edit_seq;
    return;
  case ui::TextEdit::Kind::insert:
    // Committed text (typing or IME commit) ends any composition.
    state.search_marked.clear();
    state.search_marked_caret = 0;
    if (has_selection) {
      erase_selection();
    }
    query.insert(caret, edit.text);
    caret += edit.text.size();
    break;
  case ui::TextEdit::Kind::delete_backward:
    if (has_selection) {
      erase_selection();
    } else {
      std::size_t prev = ui::PrevCharBoundary(query, caret);
      query.erase(prev, caret - prev);
      caret = prev;
    }
    break;
  case ui::TextEdit::Kind::delete_forward:
    if (has_selection) {
      erase_selection();
    } else {
      std::size_t next = ui::NextCharBoundary(query, caret);
      query.erase(caret, next - caret);
    }
    break;
  case ui::TextEdit::Kind::move_left:
    caret = has_selection ? sel_begin : ui::PrevCharBoundary(query, caret);
    break;
  case ui::TextEdit::Kind::move_right:
    caret = has_selection ? sel_end : ui::NextCharBoundary(query, caret);
    break;
  case ui::TextEdit::Kind::move_home:
    caret = 0;
    break;
  case ui::TextEdit::Kind::move_end:
    caret = query.size();
    break;
  case ui::TextEdit::Kind::select_all:
    // Select the whole field: anchor at the start, caret at the end.
    state.search_anchor = 0;
    state.search_caret = query.size();
    ++state.search_edit_seq;
    return;
  }
  // Every other command collapses the selection at the new caret.
  state.search_caret = caret;
  state.search_anchor = caret;
  // Mark that the caret moved, so the next build resets the blink to solid-on.
  ++state.search_edit_seq;
}

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

// ASCII case-insensitive substring test, enough for filtering file names by the
// typed query (non-ASCII matches fall back to exact bytes, which still works).
bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  auto lower = [](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); };
  if (needle.size() > haystack.size()) {
    return false;
  }
  for (std::size_t start = 0; start + needle.size() <= haystack.size(); ++start) {
    bool match = true;
    for (std::size_t i = 0; i < needle.size(); ++i) {
      if (lower(haystack[start + i]) != lower(needle[i])) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }
  return false;
}

// The items shown in the grid: every item, or — while searching with a non-empty
// query — only those whose name matches. Returns pointers into state.items so no
// copies are made; the grid indexes into this filtered list.
std::vector<const FileItem *> FilteredItems(const FilesState &state) {
  std::vector<const FileItem *> result;
  result.reserve(state.items.size());
  bool filtering = state.searching && !state.search_query.empty();
  for (const FileItem &item : state.items) {
    if (!filtering || ContainsCaseInsensitive(item.name, state.search_query)) {
      result.push_back(&item);
    }
  }
  return result;
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

ui::View ContentSurface(const std::shared_ptr<FilesState> &state,
    const std::vector<const FileItem *> &items, float content_scroll_offset,
    float scroll_range_headroom, float content_below_viewport, FileGridVisibleRange visible_range) {
  ui::View content = ui::layout::grid([&](ui::Block &grid) {
    for (std::size_t index = visible_range.start; index < visible_range.end; ++index) {
      const FileItem &item = *items[index];
      ui::View tile = FileTile(item, IsFocused(*state, item));
      tile.on_click([state, item] { FocusOrActivate(*state, item); });
      grid << std::move(tile);
    }
  })
                         .grid_metrics(FileGridMinColumnWidth, FileGridRowHeight, FileGridColumnGap,
                             FileGridRowGap)
                         .grid_virtual_range(visible_range.start, items.size())
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

  // The card is the height of the content, not the viewport: while more rows
  // remain below the fold it runs off the bottom of the window (clipped, so no
  // bottom edge or shadow shows). A negative bottom inset stretches it past the
  // slot by exactly the unscrolled remainder, which reaches 0 at the scroll end
  // — only then does the bottom edge (and its shadow) come into view.
  ui::View surface_panel = ui::layout::vstack(
      [&](ui::Block &background) { background << std::move(background_panel).expand(); })
                               .padding({-side_extension, -content_scroll_offset, -side_extension,
                                   -content_below_viewport})
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

// The trailing search affordance, sized to the animated `width`. Collapsed it
// is a capsule search button; expanded it is a rounded search field with a
// leading magnifier and a trailing clear button. The outer box is pinned to the
// animated width so the surrounding toolbar layout sees a smoothly growing item.
ui::View MakeSearchAffordance(
    ui::Context &context, const std::shared_ptr<FilesState> &state, float width, bool expanded) {
  constexpr float height = 36.0f;

  if (!expanded) {
    // Capsule icon button — the collapsed entry point.
    return ui::layout::zstack(
        ui::button(ui::icon(ui::Symbol::search, ui::navigation_symbol_options())
                       .foreground(ui::navigation_chevron_color(true)))
            .shape(ui::ControlShape::capsule)
            .on_click([state] { ToggleSearch(*state); })
            .accessibility_label("Search"))
        .size({width, height});
  }

  // Expanded field: a single rounded capsule bar containing a leading magnifier,
  // the query (or placeholder), and a trailing clear icon. Everything sits
  // inside the one bar — no nested control backgrounds — so it reads as one
  // contained search field rather than separate buttons. The icons are plain
  // (no button control fill); the clear icon still carries a click action, which
  // produces a hit target for any kind.
  // Smaller, lighter glyphs than the toolbar navigation icons so they sit
  // inside the field rather than reading as full-size buttons.
  ui::SymbolOptions field_icon_options = ui::navigation_symbol_options();
  field_icon_options.optical_size = 20.0f;

  // An edit since the last build resets the blink origin to now, so the caret
  // shows solid right after typing/moving and only then resumes blinking.
  if (state->search_edit_seq != state->blink_seen_seq) {
    state->blink_seen_seq = state->search_edit_seq;
    state->caret_blink_since = context.now();
  }

  // The text field IS the capsule bar: a transparent-cornered field styled to
  // match, focused so the shell routes keys to it, with left/right padding that
  // leaves room for the overlaid magnifier and clear icons. on_text_edit applies
  // each shell command to the query + caret.
  ui::View field =
      ui::text_field(state->search_query, "Search")
          .focused(true)
          .show_caret(context.caret_blink_visible(state->caret_blink_since))
          .selection(state->search_caret, state->search_anchor)
          .marked(state->search_marked, state->search_marked_caret)
          .padding({34.0f, 0.0f, 30.0f, 0.0f})
          .corner_radius(height * 0.5f)
          .expand()
          .on_text_edit([state](const ui::TextEdit &edit) { ApplySearchEdit(*state, edit); });

  // Overlay the leading magnifier and trailing clear X on top of the field.
  ui::View icons = ui::layout::hstack([&](ui::Block &content) {
    content << ui::icon(ui::Symbol::search, field_icon_options).foreground(ui::disabled_label());
    content << ui::spacer();
    content << ui::icon(ui::Symbol::close, field_icon_options)
                   .foreground(ui::disabled_label())
                   .on_click([state] { ToggleSearch(*state); });
  })
                       .padding({10.0f, 0.0f, 9.0f, 0.0f})
                       .expand();

  return ui::layout::zstack(std::move(field), std::move(icons)).size({width, height});
}

ui::View FilesView(ui::Context &context, const std::shared_ptr<FilesState> &state) {
  constexpr float chrome_margin = ui::default_chrome_margin();
  constexpr float toolbar_height = ui::default_toolbar_height();
  constexpr float surface_collapse_distance = toolbar_height + chrome_margin;
  bool can_navigate_back = CanNavigateBack(*state);
  bool can_navigate_forward = CanNavigateForward(*state);
  float surface_width = FilesWindowSize.width - chrome_margin - chrome_margin;
  // The grid shows the filtered items (all of them when not searching).
  std::vector<const FileItem *> items = FilteredItems(*state);
  float content_height = FileGridNaturalHeight(items.size(), surface_width);
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
  // The ContentSurface's scroll viewport fills the body slot, whose vertical
  // extent is the window minus the bottom margin and the toolbar clearance (the
  // body vstack has no top padding). This is the height the scroll container
  // virtualizes against — chrome_margin smaller surface_height is only the
  // visible inset and must not enter the card-stretch math.
  float surface_viewport_height =
      std::max(0.0f, FilesWindowSize.height - bottom_margin - toolbar_clearance);
  // How much content still sits below the visible viewport. The surface card is
  // stretched past the bottom of its slot by this much, so it stays clipped by
  // the window edge until the scroll reaches the end and this falls to zero —
  // only then does the card's bottom edge (and its shadow) come into view.
  float content_below_viewport =
      std::max(0.0f, content_height - surface_viewport_height - content_scroll_offset);
  FileGridVisibleRange visible_range =
      VisibleFileRange(items.size(), surface_width, surface_height, content_scroll_offset);
  bool has_toolbar_backdrop = toolbar_clearance < toolbar_height;
  // Fade the toolbar backdrop in and out instead of popping it: animate a 0->1
  // factor toward its target on every rebuild. The animation tick keeps
  // rebuilding while it is mid-flight, so the panel stays mounted (and visible)
  // until the fade-out completes.
  float toolbar_backdrop_opacity =
      context.animate_float(has_toolbar_backdrop ? 1.0f : 0.0f, 180.0f);
  bool show_toolbar_backdrop = toolbar_backdrop_opacity > 0.001f;

  auto make_toolbar_content = [&] {
    return ui::toolbar([&](ui::Block &toolbar) {
      toolbar << ui::navigation_button_group(
          can_navigate_back, [state] { NavigateBack(*state); }, can_navigate_forward,
          [state] { NavigateForward(*state); });
      toolbar << ui::toolbar_title(DirectoryName(state->current_path));
      // Search affordance at the trailing edge: a button that expands into a
      // search field. The width animates between the two so the transition
      // glides; once past the collapsed width we swap the icon button for the
      // field (the icon stays as the field's leading adornment).
      constexpr float collapsed_width = 40.0f;
      constexpr float expanded_width = 260.0f;
      float target_width = state->searching ? expanded_width : collapsed_width;
      float width = context.animate_float(target_width, 140.0f, ui::Easing::ease_out, "search");
      bool expanded = width > collapsed_width + 1.0f;

      toolbar << ui::spacer();
      toolbar << MakeSearchAffordance(context, state, width, expanded);
    });
  };

  return ui::layout::zstack([&](ui::Block &root) {
    root << ui::layout::vstack([&](ui::Block &body) {
      if (toolbar_clearance > 0.0f) {
        body << ui::empty().size({0.0f, toolbar_clearance});
      }
      body << ContentSurface(state, items, content_scroll_offset, scroll_range_headroom,
          content_below_viewport, visible_range);
    })
                .padding({chrome_margin, 0.0f, chrome_margin, bottom_margin})
                .expand();

    root << ui::layout::vstack([&](ui::Block &chrome) {
      ui::View toolbar_content = make_toolbar_content();
      if (show_toolbar_backdrop) {
        ui::Color backdrop = ui::toolbar_material();
        backdrop.alpha *= toolbar_backdrop_opacity;
        chrome << ui::layout::zstack([&](ui::Block &toolbar) {
          toolbar << ui::toolbar_blur_panel(backdrop);
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

// The Files window as a component app. The domain model lives in a shared
// FilesState that the event callbacks mutate; body() is the declarative pass
// the runtime re-runs after each interaction. State is held as an app member
// rather than ctx.state<T> because it is a single app-lifetime model — the
// per-call-site state cells earn their keep with dynamic widgets, not here.
struct FilesApp {
  std::shared_ptr<FilesState> state = std::make_shared<FilesState>();

  ui::View body(ui::Context &context) const { return FilesView(context, state); }
};

int main() {
  FilesApp app;
  OpenDirectory(*app.state, HomeDirectory());

  return phenotype::native::run_app(
      std::move(app), macos::window::Options{
                          .title = "Files",
                          .size = FilesWindowSize,
                          .title_bar = macos::window::TitleBarStyle::hidden,
                          .background = macos::window::Background::solid(ui::white()),
                          .window_controls = {.vertical_offset = 8.0f},
                      });
}

void finder_button(std::string label,
                   Msg msg,
                   bool selected = false,
                   float max_width = 0.0f,
                   float font_size = 15.0f,
                   bool centered = false,
                   bool subtle_chrome = false) {
    auto const& t = phenotype::current_theme();
    phenotype::ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected
        ? t.accent
        : (subtle_chrome
              ? (finder_dark_palette() ? rgba(255, 255, 255, 28)
                                       : rgba(255, 255, 255, 92))
              : t.transparent);
    options.has_hover_background = true;
    options.hover_background = selected
        ? t.accent_strong
        : (finder_dark_palette() ? rgba(255, 255, 255, 44)
                                 : rgba(255, 255, 255, 110));
    options.has_border_color = true;
    options.border_color = t.transparent;
    options.has_text_color = true;
    options.text_color = selected ? t.state_active_fg : t.foreground;
    options.border_width = 0.0f;
    options.border_radius = 10.0f;
    options.font_size = font_size;
    options.padding_top = 6.0f;
    options.padding_right = 10.0f;
    options.padding_bottom = 6.0f;
    options.padding_left = 10.0f;
    options.max_width = max_width;
    if (centered)
        options.text_align = phenotype::TextAlign::Center;

    phenotype::widget::button<Msg>(label, std::move(msg), options);
}

void finder_column_location_button(std::string label,
                                   std::string icon,
                                   Msg msg,
                                   bool selected,
                                   float max_width,
                                   float font_size,
                                   phenotype::icons::SymbolDocumentCache const& cache) {
    auto options = phenotype::widget::glass_outline_row_button_style(
        phenotype::GlassOutlineRowStyleOptions{
            .chrome = phenotype::GlassOutlineRowChrome::ColumnRow,
            .role = phenotype::MaterialSurfaceRole::Sidebar,
            .selected = selected,
            .width = max_width,
            .height = k_column_location_row_height,
            .border_radius = 8.0f,
        });

    phenotype::widget::canvas_button<Msg>(
        phenotype::str{label},
        max_width,
        k_column_location_row_height,
        [label, icon, selected, max_width, font_size, &cache](
                phenotype::Painter& painter,
                phenotype::ButtonVisualState state) {
            float const icon_box = 22.0f;
            float const icon_x = 7.0f;
            float const icon_y =
                (k_column_location_row_height - icon_box) * 0.5f;
            paint_finder_symbol_centered(
                painter,
                cache,
                icon_presentation_for_state(
                    sidebar_symbol(icon),
                    phenotype::icons::SymbolPresentationRole::Sidebar,
                    selected,
                    state),
                icon_x,
                icon_y,
                icon_box,
                icon_box);

            auto const ink = selected ? phenotype::current_theme().accent
                                      : finder_primary_ink();
            float const text_y =
                std::max(0.0f, (k_column_location_row_height - 18.0f) * 0.5f);
            painter.text(34.0f,
                         text_y,
                         label.c_str(),
                         static_cast<unsigned int>(label.size()),
                         font_size,
                         ink,
                         finder_font());
            paint_finder_symbol_centered(
                painter,
                cache,
                phenotype::icons::Symbol::Forward,
                max_width - 22.0f,
                (k_column_location_row_height - 18.0f) * 0.5f,
                18.0f,
                18.0f,
                14.0f,
                selected ? with_alpha(phenotype::current_theme().accent, 210)
                         : with_alpha(finder_tertiary_ink(), 210));
        },
        std::move(msg),
        options,
        stable_token(label) ^ stable_token(icon) ^ 0x3f7000u
            ^ (selected ? 0x3f1f00u : 0u));
}

void finder_icon_label_button(std::string const& label,
                              bool selected,
                              float max_width,
                              float font_size,
                              float fixed_height) {
    phenotype::ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected ? rgba(0, 122, 255, 232)
                                  : rgba(0, 0, 0, 0);
    options.has_hover_background = true;
    options.hover_background = selected ? rgba(0, 122, 255, 242)
                                        : (finder_dark_palette()
                                              ? rgba(255, 255, 255, 24)
                                              : rgba(0, 0, 0, 20));
    options.has_pressed_background = true;
    options.pressed_background = selected ? rgba(0, 96, 212, 250)
                                          : (finder_dark_palette()
                                                ? rgba(255, 255, 255, 36)
                                                : rgba(0, 0, 0, 34));
    options.has_border_color = true;
    options.border_color = rgba(0, 0, 0, 0);
    options.has_text_color = true;
    options.text_color = selected ? rgba(255, 255, 255)
                                  : finder_primary_ink();
    options.border_width = 0.0f;
    options.border_radius = 8.0f;
    options.font_size = font_size;
    options.max_width = max_width;
    options.fixed_height = fixed_height;
    options.min_hit_width = max_width;
    options.min_hit_height = fixed_height;
    options.text_align = phenotype::TextAlign::Center;
    options.focus_ring = false;
    phenotype::widget::canvas_button<Msg>(
        phenotype::str{label},
        max_width,
        fixed_height,
        [label, selected, max_width, font_size, fixed_height](
                phenotype::Painter& painter) {
            auto const ink = selected ? rgba(255, 255, 255)
                                      : finder_primary_ink();
            auto const lines = finder_icon_label_lines(
                painter,
                label,
                max_width - 12.0f,
                font_size);
            float y = lines.size() == 1
                ? std::max(0.0f, (fixed_height - 20.0f) * 0.5f)
                : 4.0f;
            for (auto const& line : lines) {
                auto const width = painter.measure_text(
                    line.c_str(),
                    static_cast<unsigned int>(line.size()),
                    font_size,
                    finder_font());
                painter.text(
                    std::max(0.0f, (max_width - width) * 0.5f),
                    y,
                    line.c_str(),
                    static_cast<unsigned int>(line.size()),
                    font_size,
                    ink,
                    finder_font());
                y += 19.0f;
            }
        },
        SelectEntry{label},
        options,
        stable_token(label) ^ (selected ? 0x1f1f00u : 0x1f1000u));
}

void finder_entry_row_button(file_explorer_demo::Entry const& entry,
                             bool selected,
                             float max_width,
                             float font_size,
                             float fixed_height,
                             phenotype::icons::SymbolDocumentCache const& cache) {
    auto options = phenotype::widget::glass_outline_row_button_style(
        phenotype::GlassOutlineRowStyleOptions{
            .chrome = phenotype::GlassOutlineRowChrome::ListRow,
            .role = phenotype::MaterialSurfaceRole::Surface,
            .selected = selected,
            .expanded = entry.folder,
            .width = max_width,
            .height = fixed_height,
            .border_radius = 8.0f,
            .font_size = font_size,
        });

    phenotype::widget::canvas_button<Msg>(
        phenotype::str{entry.name},
        max_width,
        fixed_height,
        [entry, selected, max_width, font_size, fixed_height, &cache](
                phenotype::Painter& painter) {
            float const icon_box = 24.0f;
            float const icon_size = 18.0f;
            float const icon_x = 8.0f;
            float const icon_y = (fixed_height - icon_box) * 0.5f;
            paint_entry_symbol(
                painter,
                cache,
                entry,
                selected,
                icon_x,
                icon_y,
                icon_box,
                icon_size);

            auto const ink = selected ? rgba(255, 255, 255)
                                      : finder_primary_ink();
            float const text_x = 36.0f;
            float const text_y = std::max(0.0f, (fixed_height - 18.0f) * 0.5f);
            painter.text(text_x,
                         text_y,
                         entry.name.c_str(),
                         static_cast<unsigned int>(entry.name.size()),
                         font_size,
                         ink,
                         finder_font());
            if (entry.folder) {
                auto const chevron = ">";
                painter.text(max_width - 20.0f,
                             text_y,
                             chevron,
                             1,
                             font_size,
                             selected ? rgba(255, 255, 255, 220)
                                      : finder_tertiary_ink(),
                             finder_font());
            }
        },
        SelectEntry{entry.name},
        options,
        stable_token(entry.name) ^ (selected ? 0x2f1f00u : 0x2f1000u));
}

void sidebar_row(std::string_view label,
                 std::string_view icon,
                 std::string location_id,
                 bool selected,
                 phenotype::icons::SymbolDocumentCache const& cache) {
    using namespace phenotype;
    bool const dark = finder_dark_palette();
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected
        ? (dark ? rgba(255, 255, 255, 34) : rgba(255, 255, 255, 232))
        : rgba(0, 0, 0, 0);
    options.has_hover_background = true;
    options.hover_background = selected
        ? (dark ? rgba(255, 255, 255, 48) : rgba(255, 255, 255, 242))
        : (dark ? rgba(255, 255, 255, 22) : rgba(255, 255, 255, 64));
    options.has_pressed_background = true;
    options.pressed_background = selected
        ? (dark ? rgba(255, 255, 255, 62) : rgba(255, 255, 255, 250))
        : (dark ? rgba(255, 255, 255, 36) : rgba(255, 255, 255, 92));
    options.has_border_color = true;
    options.border_color = rgba(255, 255, 255, 0);
    options.border_width = 0.0f;
    options.border_radius = k_sidebar_selected_row_radius;
    options.max_width = k_sidebar_row_width;
    options.fixed_height = k_sidebar_row_height;
    options.min_hit_width = k_sidebar_row_width;
    options.min_hit_height = k_sidebar_row_height;
    options.focus_ring = false;

    std::string label_text(label);
    std::string icon_name(icon);
    widget::canvas_button<Msg>(
        str{label_text},
        k_sidebar_row_width,
        k_sidebar_row_height,
        [label_text, icon_name, selected, &cache](
                Painter& painter,
                phenotype::ButtonVisualState state) {
            float const icon_top =
                (k_sidebar_row_height - k_sidebar_icon_size) * 0.5f;
            paint_sidebar_icon(
                painter,
                cache,
                icon_name,
                selected,
                state,
                k_sidebar_icon_leading,
                icon_top);
            auto ink = selected ? phenotype::current_theme().accent
                                : finder_primary_ink();
            painter.text(k_sidebar_label_leading,
                         k_sidebar_label_top,
                         label_text.c_str(),
                         static_cast<unsigned int>(label_text.size()),
                         15.0f,
                         ink,
                         finder_font());
        },
        SelectLocation{std::move(location_id)},
        options,
        stable_token(label_text) ^ stable_token(icon_name)
            ^ (selected ? 0x511f00u : 0x510000u));
}

void sidebar_heading(std::string_view label,
                     bool expanded,
                     std::string section_id) {
    using namespace phenotype;
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = rgba(0, 0, 0, 0);
    options.has_hover_background = true;
    options.hover_background = rgba(0, 0, 0, 0);
    options.has_border_color = true;
    options.border_color = rgba(0, 0, 0, 0);
    options.border_width = 0.0f;
    options.border_radius = 0.0f;
    options.max_width = k_sidebar_row_width;
    options.fixed_height = k_sidebar_heading_height;
    options.focus_ring = false;

    std::string label_text(label);
    std::string id_text(std::move(section_id));
    widget::canvas_button<Msg>(
        str{label_text},
        k_sidebar_row_width,
        k_sidebar_heading_height,
        [label_text, expanded](Painter& painter) {
            painter.text(k_sidebar_heading_label_leading,
                         k_sidebar_heading_label_top,
                         label_text.c_str(),
                         static_cast<unsigned int>(label_text.size()),
                         13.0f,
                         finder_secondary_ink(),
                         finder_font(phenotype::FontWeight::Bold));
            char const* chevron = expanded ? "v" : ">";
            painter.text(k_sidebar_row_width - 20.0f,
                         k_sidebar_heading_label_top,
                         chevron,
                         1,
                         13.0f,
                         finder_secondary_ink(),
                         finder_font(phenotype::FontWeight::Bold));
        },
        ToggleSidebarSection{id_text},
        options,
        stable_token(label_text) ^ stable_token(id_text) ^ 0x520000u);
}

void native_window_control_reserve_slot() {
    phenotype::widget::canvas(
        k_sidebar_row_width,
        k_native_window_control_reserve_height,
        [](phenotype::Painter&) {},
        {},
        0x530000u);
}

void finder_sidebar(State const& state) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    auto relative = file_explorer_demo::relative_location(explorer);
    bool const in_root = relative == explorer.root_label;
    auto const& labels = state.labels;
    auto const& icon_cache = state.icon_cache;
    layout::sidebar(finder_sidebar_options(), [&] {
        native_window_control_reserve_slot();
        sidebar_row(labels.sidebar_recents, "recents", "root", in_root, icon_cache);
        sidebar_row(labels.sidebar_shared, "shared", "shared",
                    relative == explorer.root_label + "/Shared", icon_cache);
        layout::spacer(k_sidebar_section_gap);
        sidebar_heading(labels.favorites, state.favorites_expanded, "favorites");
        float const favorite_progress =
            phenotype::animate_float(state.favorites_expanded ? 1.0f : 0.0f, 180);
        if (favorite_progress > 0.05f) {
            sidebar_row(labels.applications, "app", "root", false, icon_cache);
            sidebar_row(labels.desktop, "desktop", "root", false, icon_cache);
            sidebar_row(labels.documents, "doc", "documents",
                        relative == explorer.root_label + "/Documents", icon_cache);
            sidebar_row(labels.downloads, "download", "root", false, icon_cache);
        }
        layout::spacer(k_sidebar_section_gap * std::max(favorite_progress, 0.35f));
        sidebar_heading(labels.locations, state.locations_expanded, "locations");
        float const locations_progress =
            phenotype::animate_float(state.locations_expanded ? 1.0f : 0.0f, 180);
        if (locations_progress > 0.05f) {
            sidebar_row(labels.icloud_drive, "cloud", "root", false, icon_cache);
            sidebar_row(labels.home, "home", "root", false, icon_cache);
            sidebar_row(labels.airdrop, "airdrop", "shared", false, icon_cache);
            sidebar_row(labels.trash, "trash", "trash", relative == "Trash", icon_cache);
        }
    });
}


                            0x6711u);
                        more_action_item(
                            state.labels.preferences_package_font.c_str(),
                            font_family_message("pretendard"),
                            true,
                            icons::Symbol::TextDocument,
                            0x6712u);
                        more_action_item(
                            state.labels.preferences_system_text_size.c_str(),
                            system_font_metrics_message("system"),
                            true,
                            icons::Symbol::Search,
                            0x671du);
                        more_action_item(
                            state.labels.preferences_package_text_size.c_str(),
                            system_font_metrics_message("package"),
                            true,
                            icons::Symbol::Document,
                            0x671eu);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_text_larger.c_str(),
                            font_scale_step_message(state.explorer, 0.1f),
                            true,
                            icons::Symbol::Plus,
                            0x6713u);
                        more_action_item(
                            state.labels.preferences_text_smaller.c_str(),
                            font_scale_step_message(state.explorer, -0.1f),
                            true,
                            icons::Symbol::ChevronDown,
                            0x6714u);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_body_larger.c_str(),
                            body_font_size_step_message(state.explorer, 1.0f),
                            true,
                            icons::Symbol::Plus,
                            0x6721u);
                        more_action_item(
                            state.labels.preferences_body_smaller.c_str(),
                            body_font_size_step_message(state.explorer, -1.0f),
                            true,
                            icons::Symbol::ChevronDown,
                            0x6722u);
                        more_action_item(
                            state.labels.preferences_line_height_taller.c_str(),
                            line_height_step_message(state.explorer, 0.1f),
                            true,
                            icons::Symbol::List,
                            0x6723u);
                        more_action_item(
                            state.labels.preferences_line_height_tighter.c_str(),
                            line_height_step_message(state.explorer, -0.1f),
                            true,
                            icons::Symbol::Columns,
                            0x6724u);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_heading_larger.c_str(),
                            heading_font_size_step_message(state.explorer, 1.0f),
                            true,
                            icons::Symbol::TextDocument,
                            0x6725u);
                        more_action_item(
                            state.labels.preferences_heading_smaller.c_str(),
                            heading_font_size_step_message(state.explorer, -1.0f),
                            true,
                            icons::Symbol::ChevronDown,
                            0x6726u);
                        more_action_item(
                            state.labels.preferences_small_larger.c_str(),
                            small_font_size_step_message(state.explorer, 1.0f),
                            true,
                            icons::Symbol::Plus,
                            0x6727u);
                        more_action_item(
                            state.labels.preferences_small_smaller.c_str(),
                            small_font_size_step_message(state.explorer, -1.0f),
                            true,
                            icons::Symbol::ChevronDown,
                            0x6728u);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        auto const* horizontal_faster =
                            state.labels.preferences_horizontal_scroll_faster.c_str();
                        auto const* horizontal_slower =
                            state.labels.preferences_horizontal_scroll_slower.c_str();
                        more_action_item(
                            state.labels.preferences_scroll_faster.c_str(),
                            scroll_speed_step_message(state.explorer, 0.25f),
                            true,
                            icons::Symbol::ChevronDown,
                            0x6715u);
                        more_action_item(
                            state.labels.preferences_scroll_slower.c_str(),
                            scroll_speed_step_message(state.explorer, -0.25f),
                            true,
                            icons::Symbol::ChevronUp,
                            0x6716u);
                        more_action_item(
                            horizontal_faster,
                            horizontal_scroll_speed_step_message(
                                state.explorer,
                                0.25f),
                            true,
                            icons::Symbol::Columns,
                            0x6717u);
                        more_action_item(
                            horizontal_slower,
                            horizontal_scroll_speed_step_message(
                                state.explorer,
                                -0.25f),
                            true,
                            icons::Symbol::List,
                            0x6718u);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_system_scroll.c_str(),
                            system_scroll_metrics_message("system"),
                            true,
                            icons::Symbol::Columns,
                            0x671bu);
                        more_action_item(
                            state.labels.preferences_app_scroll.c_str(),
                            system_scroll_metrics_message("app"),
                            true,
                            icons::Symbol::List,
                            0x671cu);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_scrollbar_auto.c_str(),
                            scroll_bar_visibility_message("auto"),
                            true,
                            icons::Symbol::More,
                            0x6729u);
                        more_action_item(
                            state.labels.preferences_scrollbar_always.c_str(),
                            scroll_bar_visibility_message("always"),
                            true,
                            icons::Symbol::List,
                            0x672au);
                        more_action_item(
                            state.labels.preferences_scrollbar_hidden.c_str(),
                            scroll_bar_visibility_message("hidden"),
                            true,
                            icons::Symbol::Columns,
                            0x672bu);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_system_appearance.c_str(),
                            color_scheme_message("system"),
                            true,
                            icons::Symbol::Gallery,
                            0x6719u);
                        more_action_item(
                            state.labels.preferences_dark_appearance.c_str(),
                            color_scheme_message("dark"),
                            true,
                            icons::Symbol::Movie,
                            0x6720u);
                    }, SpaceToken::Xs);
                }, SpaceToken::Xs);
            });
    },
    SpaceToken::Xs,
    CrossAxisAlignment::Center,
    MainAxisAlignment::Start);
}

void finder_grid(State const& state,
                 file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    auto const chrome = file_explorer_demo::explorer_chrome_metrics(
        state.explorer,
        "desktop");
    auto const& icon_cache = state.icon_cache;
    layout::material_surface(
        content_section_options(),
        [&] {
            if (entries.empty()) {
                widget::text(state.labels.no_matching_files);
                return;
            }
            layout::spacer(chrome.icon_grid_top_inset);
            layout::scroll_view(chrome.icon_grid_scroll_height, [&] {
                auto columns = file_explorer_demo::explorer_icon_grid_columns(chrome);
                layout::grid(std::move(columns), chrome.icon_grid_row_height, [&] {
                    for (auto const& entry : entries) {
                        bool const selected = snap.has_selection
                            && snap.selected.name == entry.name;
                        auto const preview_source = selected
                            ? snap.preview
                            : std::string{};
                        layout::column([&] {
                            widget::canvas(chrome.icon_grid_thumbnail_width,
                                chrome.icon_grid_thumbnail_height,
                                [entry, selected, preview_source, &icon_cache](
                                        Painter& painter) {
                                    paint_item_thumbnail(
                                        painter,
                                        icon_cache,
                                        entry,
                                        selected,
                                        preview_source);
                                },
                                {},
                                thumbnail_paint_token(
                                    entry,
                                    selected,
                                    preview_source));
                            finder_icon_label_button(
                                entry.name,
                                selected,
                                chrome.icon_grid_column_width,
                                chrome.icon_grid_label_font_size,
                                chrome.icon_grid_label_height);
                        }, SpaceToken::Xs,
                           CrossAxisAlignment::Center,
                           MainAxisAlignment::Start);
                    }
                }, chrome.icon_grid_gap);
            }, SpaceToken::Sm);
        });
}

void settings_choice_button(std::string_view label,
                            Msg msg,
                            bool selected,
                            std::uint64_t token) {
    using namespace phenotype;
    auto const& t = current_theme();
    bool const dark = finder_dark_palette();
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected ? with_alpha(t.accent, 230)
                                  : (dark ? rgba(255, 255, 255, 24)
                                          : rgba(255, 255, 255, 210));
    options.has_hover_background = true;
    options.hover_background = selected ? with_alpha(t.accent_strong, 240)
                                        : (dark ? rgba(255, 255, 255, 38)
                                                : rgba(235, 235, 240, 240));
    options.has_pressed_background = true;
    options.pressed_background = selected ? with_alpha(t.accent_strong, 250)
                                          : (dark ? rgba(255, 255, 255, 52)
                                                  : rgba(220, 220, 226, 245));
    options.has_text_color = true;
    options.text_color = selected ? rgba(255, 255, 255)
                                  : t.foreground;
    options.has_border_color = true;
    options.border_color = selected ? with_alpha(t.accent, 0)
                                    : with_alpha(t.border, 190);
    options.border_width = selected ? 0.0f : 1.0f;
    options.border_radius = 10.0f;
    options.max_width = 132.0f;
    options.fixed_height = 34.0f;
    options.min_hit_width = 132.0f;
    options.min_hit_height = 34.0f;
    options.focus_ring = false;
    phenotype::keyed(static_cast<std::uint32_t>(token), [&] {
        widget::button<Msg>(str{std::string(label)}, std::move(msg), options);
    });
}


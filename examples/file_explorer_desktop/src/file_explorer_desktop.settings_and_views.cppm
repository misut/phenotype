module;
#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module file_explorer_desktop:settings_and_views;

import file_explorer_shared;
import phenotype;
import phenotype.native;
import :messages_and_startup;
import :input_messages;
import :runtime_preferences;
import :state_and_debug;
import :style;
import :painting_icons;
import :text_and_status;
import :update;
import :buttons_and_sidebar;
import :toolbar;
import :overflow_and_grid;

export namespace file_explorer_desktop {
void finder_settings_window(State const& state) {
    using namespace phenotype;
    layout::dialog([&] {
        layout::column([&] {
            widget::text("Settings", TextSize::Heading);
            widget::text("Appearance", TextSize::Small, TextColor::Muted);
            layout::row([&] {
                settings_choice_button(
                    state.labels.preferences_system_appearance,
                    color_scheme_message("system"),
                    state.explorer.theme_preferences.prefer_system_color_scheme,
                    0x7101u);
                settings_choice_button(
                    state.labels.preferences_light_appearance,
                    color_scheme_message("light"),
                    !state.explorer.theme_preferences.prefer_system_color_scheme
                        && state.explorer.theme_preferences.color_scheme == "light",
                    0x7102u);
                settings_choice_button(
                    state.labels.preferences_dark_appearance,
                    color_scheme_message("dark"),
                    !state.explorer.theme_preferences.prefer_system_color_scheme
                        && state.explorer.theme_preferences.color_scheme == "dark",
                    0x7103u);
            }, SpaceToken::Xs);
            widget::text("Sidebar", TextSize::Small, TextColor::Muted);
            layout::row([&] {
                settings_choice_button(
                    "Left",
                    SetSidebarPosition{false},
                    !state.sidebar_right,
                    0x7111u);
                settings_choice_button(
                    "Right",
                    SetSidebarPosition{true},
                    state.sidebar_right,
                    0x7112u);
            }, SpaceToken::Xs);
            widget::text("Scroll Bars", TextSize::Small, TextColor::Muted);
            layout::row([&] {
                settings_choice_button(
                    state.labels.preferences_scrollbar_auto,
                    scroll_bar_visibility_message("auto"),
                    state.explorer.theme_preferences.scroll_bar_visibility == "auto",
                    0x7121u);
                settings_choice_button(
                    state.labels.preferences_scrollbar_always,
                    scroll_bar_visibility_message("always"),
                    state.explorer.theme_preferences.scroll_bar_visibility == "always",
                    0x7122u);
                settings_choice_button(
                    state.labels.preferences_scrollbar_hidden,
                    scroll_bar_visibility_message("hidden"),
                    state.explorer.theme_preferences.scroll_bar_visibility == "hidden",
                    0x7123u);
            }, SpaceToken::Xs);
            layout::row([&] {
                layout::weighted(1.0f, [] {});
                settings_choice_button("Done", CloseSettings{}, true, 0x7131u);
            }, SpaceToken::Xs);
        }, SpaceToken::Md);
    }, 460.0f, 72);
}

void finder_list(State const& state,
                 file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    float const scroll_height = file_explorer_demo::desktop_scroll_height(
        state.explorer, 164.0f, 552.0f, 676.0f);
    layout::material_surface(
        content_section_options(SpaceToken::Sm),
        [&] {
            layout::row([&] {
                layout::sized_box(420.0f, [&] {
                    widget::button<Msg>(
                        phenotype::str{state.labels.name},
                        SortBy{file_explorer_demo::SortMode::Name},
                        widget::glass_table_header_button_style(
                            GlassTableHeaderStyleOptions{
                                .sorted = snap.sort_mode
                                    == file_explorer_demo::SortMode::Name,
                                .width = 410.0f,
                                .height = 28.0f,
                                .border_radius = 8.0f,
                                .font_size = 12.0f,
                            }));
                });
                layout::sized_box(160.0f, [&] {
                    widget::button<Msg>(
                        phenotype::str{state.labels.kind},
                        SortBy{file_explorer_demo::SortMode::Kind},
                        widget::glass_table_header_button_style(
                            GlassTableHeaderStyleOptions{
                                .sorted = snap.sort_mode
                                    == file_explorer_demo::SortMode::Kind,
                                .width = 150.0f,
                                .height = 28.0f,
                                .border_radius = 8.0f,
                                .font_size = 12.0f,
                            }));
                });
                layout::sized_box(120.0f, [&] {
                    widget::button<Msg>(
                        phenotype::str{state.labels.size},
                        SortBy{file_explorer_demo::SortMode::Size},
                        widget::glass_table_header_button_style(
                            GlassTableHeaderStyleOptions{
                                .sorted = snap.sort_mode
                                    == file_explorer_demo::SortMode::Size,
                                .width = 110.0f,
                                .height = 28.0f,
                                .border_radius = 8.0f,
                                .font_size = 12.0f,
                            }));
                });
            }, SpaceToken::Sm, CrossAxisAlignment::Center, MainAxisAlignment::Start);
            if (entries.empty()) {
                widget::text(state.labels.no_matching_files);
                return;
            }
            layout::scroll_view(scroll_height, [&] {
                layout::column([&] {
                    for (auto const& entry : entries) {
                        bool const selected = snap.has_selection
                            && snap.selected.name == entry.name;
                        layout::row([&] {
                            layout::sized_box(420.0f, [&] {
                                finder_entry_row_button(
                                    entry,
                                    selected,
                                    410.0f,
                                    15.0f,
                                    32.0f,
                                    state.icon_cache);
                            });
                            layout::sized_box(160.0f, [&] {
                                widget::text(file_explorer_demo::entry_kind_label(entry),
                                             TextSize::Small,
                                             TextColor::Muted);
                            });
                            layout::sized_box(120.0f, [&] {
                                widget::text(file_explorer_demo::entry_size_label(entry),
                                             TextSize::Small,
                                             TextColor::Muted);
                            });
                        }, SpaceToken::Sm,
                           CrossAxisAlignment::Center,
                           MainAxisAlignment::Start);
                    }
                }, SpaceToken::Xs);
            }, SpaceToken::Sm);
        });
}

void finder_column_view(State const& state,
                        file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    float const column_height = file_explorer_demo::desktop_scroll_height(
        state.explorer, 224.0f, 500.0f, 620.0f);
    auto const& icon_cache = state.icon_cache;
    layout::material_surface(
        content_section_options(),
        [&] {
            layout::row([&] {
                layout::sized_box(210.0f, [&] {
                    layout::column([&] {
                        widget::text(state.labels.locations, TextSize::Small, TextColor::Muted);
                        finder_column_location_button(
                            "Demo Root",
                            "home",
                            SelectLocation{"root"},
                            snap.relative_location == "Demo Root",
                            190.0f,
                            14.0f,
                            state.icon_cache);
                        finder_column_location_button(
                            state.labels.documents,
                            "doc",
                            SelectLocation{"documents"},
                            snap.relative_location == "Demo Root/Documents",
                            190.0f,
                            14.0f,
                            state.icon_cache);
                        finder_column_location_button(
                            state.labels.pictures,
                            "image",
                            SelectLocation{"pictures"},
                            snap.relative_location == "Demo Root/Pictures",
                            190.0f,
                            14.0f,
                            state.icon_cache);
                        finder_column_location_button(
                            state.labels.sidebar_shared,
                            "shared",
                            SelectLocation{"shared"},
                            snap.relative_location == "Demo Root/Shared",
                            190.0f,
                            14.0f,
                            state.icon_cache);
                    }, SpaceToken::Xs);
                });
                layout::sized_box(360.0f, [&] {
                    layout::column([&] {
                        widget::text(snap.relative_location,
                                     TextSize::Small,
                                     TextColor::Muted);
                        if (entries.empty()) {
                            widget::text(state.labels.no_matching_files);
                            return;
                        }
                        layout::scroll_view(column_height, [&] {
                            layout::column([&] {
                                for (auto const& entry : entries) {
                                    bool const selected = snap.has_selection
                                        && snap.selected.name == entry.name;
                                    finder_entry_row_button(
                                        entry,
                                        selected,
                                        330.0f,
                                        14.0f,
                                        30.0f,
                                        state.icon_cache);
                                }
                            }, SpaceToken::Xs);
                        });
                    }, SpaceToken::Xs);
                });
                layout::weighted(1.0f, [&] {
                    layout::column([&] {
                        widget::text(state.labels.preview, TextSize::Small, TextColor::Muted);
                        if (snap.has_selection) {
                            widget::canvas(160.0f, 110.0f,
                                [entry = snap.selected,
                                 preview = snap.preview,
                                 &icon_cache](Painter& painter) {
                                    paint_item_thumbnail(
                                        painter,
                                        icon_cache,
                                        entry,
                                        true,
                                        preview);
                                },
                                {},
                                thumbnail_paint_token(
                                    snap.selected,
                                    true,
                                    snap.preview,
                                    0x440000u));
                            widget::text(snap.selected.name, TextSize::Heading);
                            widget::text(file_explorer_demo::entry_kind_label(snap.selected),
                                         TextSize::Small,
                                         TextColor::Muted);
                            widget::text(compact_preview(snap.preview),
                                         TextSize::Small,
                                         TextColor::Muted);
                        } else {
                            widget::text(state.labels.select_file_to_preview);
                        }
                    }, SpaceToken::Sm);
                });
            }, SpaceToken::Md, CrossAxisAlignment::Start, MainAxisAlignment::Start);
        });
}

void finder_gallery_view(State const& state,
                         file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    float const gallery_height = file_explorer_demo::desktop_scroll_height(
        state.explorer, 352.0f, 366.0f, 520.0f);
    auto const& icon_cache = state.icon_cache;
    file_explorer_demo::Entry hero = snap.has_selection && !snap.selected.name.empty()
        ? snap.selected
        : (entries.empty() ? file_explorer_demo::Entry{} : entries.front());
    auto const hero_preview = snap.has_selection && hero.name == snap.selected.name
        ? snap.preview
        : std::string{};
    bool const has_hero = !hero.name.empty();
    layout::material_surface(
        content_section_options(),
        [&] {
            if (!has_hero) {
                widget::text(state.labels.no_matching_files);
                return;
            }
            layout::row([&] {
                widget::canvas(220.0f, 150.0f,
                    [hero, hero_preview, &icon_cache](Painter& painter) {
                        paint_item_thumbnail(
                            painter,
                            icon_cache,
                            hero,
                            true,
                            hero_preview);
                    },
                    {},
                    thumbnail_paint_token(hero, true, hero_preview, 0x550000u));
                layout::weighted(1.0f, [&] {
                    layout::column([&] {
                        widget::text(hero.name, TextSize::Heading);
                        widget::text(file_explorer_demo::entry_kind_label(hero),
                                     TextSize::Small,
                                     TextColor::Muted);
                        widget::text(snap.has_selection
                            ? compact_preview(snap.preview)
                            : state.labels.select_file_to_preview,
                            TextSize::Small,
                            TextColor::Muted);
                    }, SpaceToken::Sm);
                });
            }, SpaceToken::Lg, CrossAxisAlignment::Center, MainAxisAlignment::Start);
            layout::scroll_view(gallery_height, [&] {
                std::vector<float> columns{160.0f, 160.0f, 160.0f, 160.0f};
                layout::grid(std::move(columns), 142.0f, [&] {
                    for (auto const& entry : entries) {
                        bool const selected = entry.name == hero.name;
                        auto const preview_source =
                            snap.has_selection && entry.name == snap.selected.name
                            ? snap.preview
                            : std::string{};
                        layout::column([&] {
                            widget::canvas(128.0f, 76.0f,
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
                                    preview_source,
                                    0x560000u));
                            finder_button(entry.name,
                                          SelectEntry{entry.name},
                                          selected,
                                          150.0f,
                                          14.0f,
                                          true);
                        }, SpaceToken::Xs,
                           CrossAxisAlignment::Center,
                           MainAxisAlignment::Start);
                    }
                }, 18.0f);
            });
        });
}

void finder_content(State const& state,
                    file_explorer_demo::Snapshot const& snap) {
    switch (state.explorer.view_mode) {
        case file_explorer_demo::ExplorerViewMode::List:
            finder_list(state, snap);
            return;
        case file_explorer_demo::ExplorerViewMode::Column:
            finder_column_view(state, snap);
            return;
        case file_explorer_demo::ExplorerViewMode::Gallery:
            finder_gallery_view(state, snap);
            return;
        case file_explorer_demo::ExplorerViewMode::Icon:
        default:
            finder_grid(state, snap);
            return;
    }
}

void finder_status_bar(State const& state,
                       file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    auto const chrome = file_explorer_demo::explorer_chrome_metrics(
        explorer,
        "desktop");
    auto options = status_section_options();
    layout::status_bar(
        options,
        [&] {
            layout::row([&] {
                std::string status = finder_status(snap);
                if (!explorer.status.empty())
                    status += " - " + explorer.status;
                if (snap.has_selection
                    && explorer.status != "Ready"
                    && !snap.preview.empty()) {
                    status += " - " + compact_preview(snap.preview);
                }
                if (!snap.operation_label.empty())
                    status += " - " + snap.operation_label;
                float const side_width = snap.has_selection ? 264.0f : 0.0f;
                float const status_width = std::max(
                    180.0f,
                    chrome.content_surface_width - side_width - 36.0f);
                layout::sized_box(status_width, [&] {
                    widget::semantic_canvas(
                        status_width,
                        20.0f,
                        str{status},
                        [status, status_width](Painter& painter) {
                            paint_elided_finder_text(
                                painter,
                                0.0f,
                                1.0f,
                                status,
                                status_width,
                                current_theme().small_font_size,
                                current_theme().muted);
                        },
                        {},
                        stable_token(status) ^ 0x5a5a5101u);
                });
                if (snap.has_selection) {
                    layout::sized_box(144.0f, [&] {
                        widget::text(snap.selected_kind_label,
                                     TextSize::Small,
                                     TextColor::Muted);
                    });
                    layout::sized_box(96.0f, [&] {
                        widget::text(snap.selected_size_label,
                                     TextSize::Small,
                                     TextColor::Muted);
                    });
                }
            },
            SpaceToken::Sm,
            CrossAxisAlignment::Center,
            MainAxisAlignment::Start);
        });
}

} // namespace file_explorer_desktop

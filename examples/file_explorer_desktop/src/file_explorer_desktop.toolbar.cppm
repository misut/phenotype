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

export module file_explorer_desktop:toolbar;

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

export namespace file_explorer_desktop {
void toolbar_separator() {
    phenotype::widget::canvas(1.0f, 28.0f,
        [](phenotype::Painter& painter) {
            fill_round(painter, 0.0f, 2.0f, 1.0f, 24.0f, 0.5f,
                       with_alpha(phenotype::current_theme().border, 130));
        },
        {},
        0x6901u);
}

phenotype::ButtonStyleOptions toolbar_icon_button_style(
        bool selected = false,
        bool enabled = true,
        phenotype::MaterialSurfaceRole role =
            phenotype::MaterialSurfaceRole::Toolbar) {
    using namespace phenotype;
    auto const& t = current_theme();
    ButtonStyleOptions options;
    options.disabled = !enabled;
    options.has_background = true;
    options.background = selected
        ? (finder_dark_palette() ? rgba(255, 255, 255, 48)
                                 : rgba(255, 255, 255, 176))
        : rgba(0, 0, 0, 0);
    options.has_hover_background = true;
    options.hover_background = selected
        ? (finder_dark_palette() ? rgba(255, 255, 255, 70)
                                 : rgba(255, 255, 255, 214))
        : (finder_dark_palette() ? rgba(255, 255, 255, 38)
                                 : rgba(255, 255, 255, 128));
    options.has_pressed_background = true;
    options.pressed_background = selected
        ? (finder_dark_palette() ? rgba(255, 255, 255, 92)
                                 : rgba(255, 255, 255, 236))
        : (finder_dark_palette() ? rgba(255, 255, 255, 56)
                                 : rgba(255, 255, 255, 166));
    options.has_border_color = true;
    options.border_color = rgba(0, 0, 0, 0);
    options.has_text_color = true;
    options.text_color = selected ? t.foreground : t.muted;
    options.border_width = 0.0f;
    options.border_radius = k_toolbar_group_radius;
    options.max_width = k_toolbar_icon_button_width;
    options.fixed_height = k_toolbar_icon_button_height;
    options.min_hit_width = k_toolbar_icon_button_width;
    options.min_hit_height = k_toolbar_icon_button_height;
    options.focus_ring = false;
    return widget::interaction_glass_button_style(
        options,
        role,
        MaterialKind::Clear,
        MaterialKind::Regular);
}

void view_mode_button(char const* label,
                      file_explorer_demo::ExplorerViewMode mode,
                      file_explorer_demo::ExplorerViewMode current,
                      phenotype::icons::Symbol symbol,
                      std::uint64_t token) {
    bool const selected = mode == current;
    std::string semantic_label(label);
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        symbol,
        SetViewMode{mode},
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Toolbar,
            .selected = selected,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = token,
        },
        toolbar_icon_button_style(selected));
}

void toolbar_action_button(char const* label,
                           phenotype::icons::Symbol symbol,
                           std::uint64_t token) {
    std::string semantic_label(label);
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        symbol,
        ToolbarAction{semantic_label},
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Toolbar,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = token,
        },
        toolbar_icon_button_style());
}

void toolbar_message_button(char const* label,
                            phenotype::icons::Symbol symbol,
                            Msg msg,
                            std::uint64_t token,
                            bool selected = false) {
    std::string semantic_label(label);
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        symbol,
        std::move(msg),
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Toolbar,
            .selected = selected,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = token,
        },
        toolbar_icon_button_style(selected));
}

Msg on_search_changed(std::string text) {
    return SearchChanged{std::move(text)};
}

void search_toggle_button(bool selected) {
    std::string semantic_label = "Search Control";
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        phenotype::icons::Symbol::Search,
        ToggleSearch{},
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Toolbar,
            .selected = selected,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = 0x6401u,
        },
        toolbar_icon_button_style(selected));
}

void sort_action_button(file_explorer_demo::Snapshot const& snap) {
    std::string semantic_label = "Group Sort";
    if (!snap.sort_label.empty())
        semantic_label += " (" + snap.sort_label + ")";
    auto style = phenotype::widget::interaction_glass_button_style(
        phenotype::widget::glass_split_button_style(
            phenotype::GlassSplitButtonStyleOptions{
                .kind = phenotype::MaterialKind::Clear,
                .role = phenotype::MaterialSurfaceRole::Toolbar,
                .segment = phenotype::GlassSplitButtonSegment::Single,
                .selected = false,
                .disabled = false,
                .container_id = 2100u,
                .spacing = 16.0f,
                .width = k_toolbar_icon_button_width,
                .height = k_toolbar_icon_button_height,
                .border_radius = 18.0f,
            }),
        phenotype::MaterialSurfaceRole::Toolbar,
        phenotype::MaterialKind::Clear,
        phenotype::MaterialKind::Regular);
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        phenotype::icons::Symbol::SortGroup,
        CycleSort{},
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Toolbar,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = 0x6501u,
        },
        style);
}

void file_action_button(char const* label,
                        Msg msg,
                        bool enabled,
                        phenotype::icons::Symbol symbol,
                        std::uint64_t token) {
    std::string semantic_label(label);
    auto const symbol_options = phenotype::icons::SymbolButtonOptions{
        .role = phenotype::icons::SymbolPresentationRole::Navigation,
        .disabled = !enabled,
        .width = k_toolbar_icon_button_width,
        .height = k_toolbar_icon_button_height,
        .token_salt = token,
    };
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        symbol,
        std::move(msg),
        symbol_options,
        phenotype::widget::interaction_glass_button_style(
            phenotype::widget::glass_menu_item_button_style(
                phenotype::GlassMenuItemStyleOptions{
                    .disabled = !enabled,
                    .width = k_toolbar_icon_button_width,
                    .height = k_toolbar_icon_button_height,
                    .border_radius = k_toolbar_group_radius,
                }),
            phenotype::MaterialSurfaceRole::Control,
            phenotype::MaterialKind::Clear,
            phenotype::MaterialKind::Regular));
}

void navigation_button(char const* label,
                       Msg msg,
                       bool enabled,
                       phenotype::icons::Symbol symbol,
                       std::uint64_t token) {
    std::string semantic_label(label);
    auto style = phenotype::widget::interaction_glass_button_style(
        phenotype::widget::glass_menu_item_button_style(
            phenotype::GlassMenuItemStyleOptions{
                .role = phenotype::MaterialSurfaceRole::Navigation,
                .disabled = !enabled,
                .width = k_toolbar_icon_button_width,
                .height = k_toolbar_icon_button_height,
                .border_radius = k_toolbar_group_radius,
            }),
        phenotype::MaterialSurfaceRole::Navigation,
        phenotype::MaterialKind::Clear,
        phenotype::MaterialKind::Regular);
    style.min_hit_width = k_toolbar_icon_button_width;
    style.min_hit_height = k_toolbar_icon_button_height;
    style.focus_ring = false;
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        symbol,
        std::move(msg),
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Navigation,
            .disabled = !enabled,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = token,
        },
        style);
}

void finder_toolbar(State const& state,
                    file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    layout::row([&] {
        layout::toolbar(
            toolbar_group_options(
                "Navigation Controls",
                file_explorer_demo::k_desktop_toolbar_navigation_group_width),
            [&] {
                navigation_button(state.labels.back.c_str(), GoBack{}, snap.can_go_back,
                                  icons::Symbol::Back, 0x6201u);
                toolbar_separator();
                navigation_button(state.labels.forward.c_str(), GoForward{}, snap.can_go_forward,
                                  icons::Symbol::Forward, 0x6202u);
            });
        widget::text(snap.root_is_demo && snap.relative_location == snap.root_label
            ? state.labels.title
            : snap.relative_location,
            TextSize::Heading);
        layout::weighted(1.0f, [] {});
        layout::segmented_control_surface(
            segmented_toolbar_options(
                "View Controls",
                file_explorer_demo::k_desktop_toolbar_view_group_width),
            [&] {
                view_mode_button("Icon View", file_explorer_demo::ExplorerViewMode::Icon,
                                 state.explorer.view_mode, icons::Symbol::Grid, 0x6301u);
                view_mode_button("List View", file_explorer_demo::ExplorerViewMode::List,
                                 state.explorer.view_mode, icons::Symbol::List, 0x6302u);
                toolbar_separator();
                view_mode_button("Column View", file_explorer_demo::ExplorerViewMode::Column,
                                 state.explorer.view_mode, icons::Symbol::Columns, 0x6303u);
                toolbar_separator();
                view_mode_button("Gallery View", file_explorer_demo::ExplorerViewMode::Gallery,
                                 state.explorer.view_mode, icons::Symbol::Gallery, 0x6304u);
            });
        layout::toolbar(
            toolbar_group_options(
                "Group Sort",
                file_explorer_demo::k_desktop_toolbar_sort_group_width),
            [&] {
                sort_action_button(snap);
            });
        layout::toolbar(
            toolbar_group_options(
                "Share Tag More",
                file_explorer_demo::k_desktop_toolbar_action_group_width),
            [&] {
                toolbar_action_button("Share", icons::Symbol::Share, 0x6601u);
                toolbar_action_button("Tag", icons::Symbol::Tag, 0x6602u);
                toolbar_message_button("More", icons::Symbol::More,
                                       ToggleMoreActions{},
                                       0x6603u,
                                       state.more_actions_open);
            });
        layout::toolbar(
            toolbar_group_options(
                "Search Control",
                state.search_visible
                    ? 220.0f
                    : file_explorer_demo::k_desktop_toolbar_search_collapsed_width),
            [&] {
                search_toggle_button(state.search_visible);
                if (state.search_visible) {
                    widget::text_field<Msg>(
                        state.labels.search_placeholder,
                        state.explorer.search,
                        on_search_changed,
                        widget::glass_text_field_style(
                            GlassTextFieldStyleOptions{
                                .role = MaterialSurfaceRole::Toolbar,
                                .width = 152.0f,
                                .height = k_toolbar_icon_button_height,
                                .border_radius = k_toolbar_group_radius,
                                .semantic_label = "Search Field",
                            }));
                }
            });
    },
    SpaceToken::Xs,
    CrossAxisAlignment::Center,
    MainAxisAlignment::Start);
}

template<typename Paint>
void more_action_item(char const* label,
                      Msg msg,
                      bool enabled,
                      Paint paint,
                      std::uint64_t token) {
    using namespace phenotype;
    layout::sized_box(84.0f, [&] {
        layout::column([&] {
            file_action_button(label,
                               std::move(msg),
                               enabled,
                               paint,
                               token);
            auto label_text = std::string{label};
            widget::text(label_text,
                         TextSize::Small,
                         enabled ? TextColor::Default : TextColor::Muted);
        },
        SpaceToken::Xs,
        CrossAxisAlignment::Center,
        MainAxisAlignment::Start);
    });
}

void finder_more_actions(State const& state,
                         file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    layout::row([&] {
        layout::weighted(1.0f, [] {});
        auto options = layout::glass_surface_options(
            layout::GlassSurfacePreset::ToolbarGroup,
            "More Actions Menu");
        options.kind = MaterialKind::Regular;
        options.max_width = 376.0f;
        options.fixed_height = 438.0f;
        options.border_radius = k_toolbar_group_radius;
        layout::popover(
            options,
            [&] {
                layout::column([&] {
                    layout::row([&] {
                        more_action_item(state.labels.create_file.c_str(),
                                         CreateFile{},
                                         snap.can_create_file,
                                         icons::Symbol::NewDocument,
                                         0x6701u);
                        more_action_item(state.labels.create_folder.c_str(),
                                         CreateFolder{},
                                         snap.can_create_folder,
                                         icons::Symbol::NewFolder,
                                         0x6702u);
                        more_action_item(state.labels.duplicate.c_str(),
                                         DuplicateSelected{},
                                         snap.can_duplicate_selected,
                                         icons::Symbol::Duplicate,
                                         0x6703u);
                        more_action_item(state.labels.delete_action.c_str(),
                                         DeleteSelected{},
                                         snap.can_delete_selected,
                                         icons::Symbol::Trash,
                                         0x6704u);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_system_font.c_str(),
                            font_family_message("system"),
                            true,
                            icons::Symbol::Document,
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
} // namespace file_explorer_desktop

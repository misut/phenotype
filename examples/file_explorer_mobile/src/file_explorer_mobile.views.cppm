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

export module file_explorer_mobile:views;

import file_explorer_shared;
import phenotype;
import phenotype.native;
import :messages_and_runtime;
import :state_and_debug;
import :input_messages;
import :painting;
import :update;

export namespace file_explorer_mobile {
void top_bar(State const& state, file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::toolbar(layout::MaterialSurfaceOptions{
        .kind = MaterialKind::Clear,
        .padding = SpaceToken::Md,
        .gap = SpaceToken::Sm,
        .semantic_label = "Toolbar",
    }, [&] {
        widget::text(state.labels.mobile_title);
        widget::text(snap.relative_location, TextSize::Small, TextColor::Muted);
        layout::spacer(8);
        widget::text_field<Msg>(
            state.labels.search_placeholder,
            explorer.search,
            on_search_changed,
            widget::glass_text_field_style(
                GlassTextFieldStyleOptions{
                    .role = MaterialSurfaceRole::Navigation,
                    .semantic_label = "Mobile Search Field",
                }));
        layout::spacer(8);
        std::vector<phenotype::str> tabs;
        tabs.emplace_back(
            state.labels.tab_browse.c_str(),
            static_cast<unsigned int>(state.labels.tab_browse.size()));
        tabs.emplace_back(
            state.labels.tab_preview.c_str(),
            static_cast<unsigned int>(state.labels.tab_preview.size()));
        tabs.emplace_back(
            state.labels.tab_create.c_str(),
            static_cast<unsigned int>(state.labels.tab_create.size()));
        widget::tabs<Msg>(tabs, explorer.mobile_tab, [](std::size_t index) -> Msg {
            return SelectTab{index};
        },
        TabsStyleOptions{
            .kind = MaterialKind::Regular,
            .role = MaterialSurfaceRole::Navigation,
            .semantic_label = "Mobile Mode Segmented Control",
        });
    });
}

void location_strip(file_explorer_demo::ExplorerLabels const& labels) {
    using namespace phenotype;
    layout::navigation(layout::MaterialSurfaceOptions{
        .kind = MaterialKind::Thin,
        .direction = FlexDirection::Row,
        .padding = SpaceToken::Sm,
        .gap = SpaceToken::Xs,
        .cross_align = CrossAxisAlignment::Center,
        .semantic_label = "Locations",
    }, [&] {
        layout::row([&] {
            widget::button<Msg>(labels.root, SelectLocation{"root"});
            widget::button<Msg>(labels.docs, SelectLocation{"documents"});
            widget::button<Msg>(labels.pics, SelectLocation{"pictures"});
            widget::button<Msg>(labels.sidebar_shared, SelectLocation{"shared"});
            widget::button<Msg>(labels.trash, SelectLocation{"trash"});
        }, SpaceToken::Xs, CrossAxisAlignment::Center, MainAxisAlignment::Start);
    });
}

void browse_tab(
        State const& state,
        file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& labels = state.labels;
    float const row_width = std::max(
        280.0f,
        static_cast<float>(state.explorer.viewport_width) - 48.0f);
    layout::material_surface(MaterialKind::Regular, [&] {
        layout::row([&] {
            layout::weighted(1.0f, [&] {
                std::string summary = std::to_string(snap.entries.size()) + " items";
                if (!snap.sort_label.empty())
                    summary += " - " + snap.sort_label;
                widget::text(summary, TextSize::Small, TextColor::Muted);
            });
            mobile_symbol_button(
                labels.sort,
                icons::Symbol::SortGroup,
                CycleSort{},
                icons::SymbolPresentationRole::Toolbar,
                true,
                false,
                0x7101u);
            mobile_symbol_button(
                labels.back,
                icons::Symbol::Back,
                GoBack{},
                icons::SymbolPresentationRole::Navigation,
                snap.can_go_back,
                false,
                0x7102u);
            mobile_symbol_button(
                labels.forward,
                icons::Symbol::Forward,
                GoForward{},
                icons::SymbolPresentationRole::Navigation,
                snap.can_go_forward,
                false,
                0x7103u);
            mobile_symbol_button(
                labels.up,
                icons::Symbol::ChevronUp,
                GoUp{},
                icons::SymbolPresentationRole::Navigation,
                true,
                false,
                0x7104u);
        });
        layout::spacer(8);
        layout::scroll_view(430.0f, [&] {
            if (snap.entries.empty()) {
                widget::text(labels.no_matching_files);
                return;
            }
            for (auto const& entry : snap.entries) {
                layout::material_surface(
                    entry.folder ? MaterialKind::Clear : MaterialKind::Thin,
                    [&] {
                        mobile_entry_row(
                            entry,
                            entry.name == state.explorer.selected_name,
                            row_width,
                            state.icon_cache);
                    },
                    SpaceToken::Sm,
                    SpaceToken::Xs);
            }
        }, SpaceToken::Sm);
    }, SpaceToken::Md, SpaceToken::Sm);
}

void preview_tab(
        State const& state,
        file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::material_surface(MaterialKind::Thick, [&] {
        widget::text(state.labels.preview);
        layout::spacer(4);
        if (snap.has_selection) {
            widget::text(snap.selected.name);
            widget::text(snap.selected_kind_label + " - "
                             + snap.selected_size_label,
                         TextSize::Small,
                         TextColor::Muted);
        } else {
            widget::text(state.labels.select_file_from_browse);
        }
        layout::spacer(8);
        widget::code(snap.preview);
        layout::spacer(12);
        widget::button<Msg>(state.labels.duplicate_file,
                            DuplicateSelected{},
                            ButtonVariant::Default,
                            !snap.can_duplicate_selected);
        layout::spacer(8);
        widget::button<Msg>(state.labels.delete_selected,
                            DeleteSelected{},
                            ButtonVariant::Default,
                            !snap.can_delete_selected);
        layout::spacer(8);
        widget::text(explorer.status, TextSize::Small, TextColor::Muted);
    }, SpaceToken::Md, SpaceToken::Sm);
}

void create_tab(State const& state) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::material_surface(MaterialKind::Regular, [&] {
        widget::text(state.labels.tab_create);
        widget::text(state.labels.create_hint,
                     TextSize::Small,
                     TextColor::Muted);
        layout::spacer(8);
        widget::text_field<Msg>(
            state.labels.file_name,
            explorer.draft_name,
            on_draft_name_changed);
        layout::spacer(8);
        widget::text_field<Msg>(
            state.labels.contents,
            explorer.draft_body,
            on_draft_body_changed);
        layout::spacer(10);
        widget::button<Msg>(
            state.labels.create_file,
            CreateFile{},
            ButtonVariant::Primary);
        layout::spacer(10);
        widget::text_field<Msg>(
            state.labels.folder_name,
            explorer.draft_folder_name,
            on_draft_folder_name_changed);
        layout::spacer(8);
        widget::button<Msg>(
            state.labels.create_folder,
            CreateFolder{},
            ButtonVariant::Default);
        layout::spacer(6);
        widget::button<Msg>(state.labels.reset_demo_files, ResetDemo{});
        layout::spacer(12);
        widget::text(state.labels.preferences);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_system_font,
                font_family_message("system"));
            widget::button<Msg>(
                state.labels.preferences_package_font,
                font_family_message("pretendard"));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_system_text_size,
                system_font_metrics_message("system"));
            widget::button<Msg>(
                state.labels.preferences_package_text_size,
                system_font_metrics_message("package"));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_text_larger,
                font_scale_step_message(explorer, 0.1f));
            widget::button<Msg>(
                state.labels.preferences_text_smaller,
                font_scale_step_message(explorer, -0.1f));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_body_larger,
                body_font_size_step_message(explorer, 1.0f));
            widget::button<Msg>(
                state.labels.preferences_body_smaller,
                body_font_size_step_message(explorer, -1.0f));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_heading_larger,
                heading_font_size_step_message(explorer, 1.0f));
            widget::button<Msg>(
                state.labels.preferences_heading_smaller,
                heading_font_size_step_message(explorer, -1.0f));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_small_larger,
                small_font_size_step_message(explorer, 1.0f));
            widget::button<Msg>(
                state.labels.preferences_small_smaller,
                small_font_size_step_message(explorer, -1.0f));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_line_height_taller,
                line_height_step_message(explorer, 0.1f));
            widget::button<Msg>(
                state.labels.preferences_line_height_tighter,
                line_height_step_message(explorer, -0.1f));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_scroll_faster,
                scroll_speed_step_message(explorer, 0.25f));
            widget::button<Msg>(
                state.labels.preferences_scroll_slower,
                scroll_speed_step_message(explorer, -0.25f));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_horizontal_scroll_faster,
                horizontal_scroll_speed_step_message(explorer, 0.25f));
            widget::button<Msg>(
                state.labels.preferences_horizontal_scroll_slower,
                horizontal_scroll_speed_step_message(explorer, -0.25f));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_system_scroll,
                system_scroll_metrics_message("system"));
            widget::button<Msg>(
                state.labels.preferences_app_scroll,
                system_scroll_metrics_message("app"));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_scrollbar_auto,
                scroll_bar_visibility_message("auto"));
            widget::button<Msg>(
                state.labels.preferences_scrollbar_always,
                scroll_bar_visibility_message("always"));
            widget::button<Msg>(
                state.labels.preferences_scrollbar_hidden,
                scroll_bar_visibility_message("hidden"));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_system_appearance,
                color_scheme_message("system"));
            widget::button<Msg>(
                state.labels.preferences_dark_appearance,
                color_scheme_message("dark"));
        }, SpaceToken::Xs);
    }, SpaceToken::Md, SpaceToken::Sm);
}

void view(State const& state) {
    using namespace phenotype;
    g_debug_state = &state;
    auto snap = file_explorer_demo::snapshot(state.explorer);
    layout::padded(SpaceToken::Md, [&] {
        layout::material_container(
            layout::MaterialContainerOptions{
                .container_id = 3100u,
                .spacing = 12.0f,
                .morph_transitions = true,
            },
            [&] {
                layout::column([&] {
                    top_bar(state, snap);
                    location_strip(state.labels);
                    if (state.explorer.mobile_tab == 0) {
                        browse_tab(state, snap);
                    } else if (state.explorer.mobile_tab == 1) {
                        preview_tab(state, snap);
                    } else {
                        create_tab(state);
                    }
                    layout::status_bar([&] {
                        widget::text(state.labels.status);
                        std::string detail = "Status: " + state.explorer.status;
                        if (!snap.operation_label.empty()) {
                            detail += "\n";
                            detail += snap.operation_label;
                        }
                        detail += "\nViewport: ";
                        detail += std::to_string(state.explorer.viewport_width);
                        detail += " x ";
                        detail += std::to_string(state.explorer.viewport_height);
                        widget::code(detail);
                    }, MaterialKind::Thick);
                }, SpaceToken::Sm);
            });
    });
}
} // namespace file_explorer_mobile

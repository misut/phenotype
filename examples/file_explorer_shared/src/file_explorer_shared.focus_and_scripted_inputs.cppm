module;
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

export module file_explorer_shared:focus_and_scripted_inputs;

import json;
import phenotype.icon_catalog;
import phenotype.resources;
import phenotype.theme_contract;
import :model_types;
import :desktop_metrics_and_symbols;
import :viewport_and_focus_helpers;
import :chrome_and_geometry;
import :filesystem_model;
import :icon_and_interaction_debug_json;
import :chrome_debug_json;
import :resources_debug_json;
import :debug_payload_json;
import :input_parsing;
import :filesystem_snapshot_helpers;
import :state_factories;
import :navigation_and_file_actions;

export namespace file_explorer_demo {
inline ExplorerInputModality default_input_modality(
        ExplorerInputKind kind) {
    switch (kind) {
        case ExplorerInputKind::FocusSearch:
        case ExplorerInputKind::MoveSelection:
        case ExplorerInputKind::ActivateSelected:
        case ExplorerInputKind::DeleteSelected:
        case ExplorerInputKind::DuplicateSelected:
        case ExplorerInputKind::DismissTransient:
        case ExplorerInputKind::TabFocus:
        case ExplorerInputKind::ShiftTabFocus:
        case ExplorerInputKind::FocusTarget:
            return ExplorerInputModality::Keyboard;
        case ExplorerInputKind::SelectLocation:
        case ExplorerInputKind::SelectEntry:
        case ExplorerInputKind::OpenEntry:
        case ExplorerInputKind::ActivateEntry:
        case ExplorerInputKind::PointerFocus:
            return ExplorerInputModality::Pointer;
        case ExplorerInputKind::Search:
            return ExplorerInputModality::Keyboard;
        case ExplorerInputKind::Noop:
        case ExplorerInputKind::ViewMode:
        case ExplorerInputKind::Viewport:
        case ExplorerInputKind::DraftName:
        case ExplorerInputKind::DraftFolderName:
        case ExplorerInputKind::DraftBody:
        case ExplorerInputKind::CreateFile:
        case ExplorerInputKind::CreateFolder:
        case ExplorerInputKind::GoBack:
        case ExplorerInputKind::GoForward:
        case ExplorerInputKind::GoUp:
        case ExplorerInputKind::Sort:
        case ExplorerInputKind::CycleSort:
        case ExplorerInputKind::Reset:
        case ExplorerInputKind::Scenario:
        case ExplorerInputKind::SetFontFamily:
        case ExplorerInputKind::SetFontScale:
        case ExplorerInputKind::SetBodyFontSize:
        case ExplorerInputKind::SetHeadingFontSize:
        case ExplorerInputKind::SetSmallFontSize:
        case ExplorerInputKind::SetLineHeightRatio:
        case ExplorerInputKind::SetSystemFontMetrics:
        case ExplorerInputKind::SetSystemScrollMetrics:
        case ExplorerInputKind::SetScrollSpeed:
        case ExplorerInputKind::SetHorizontalScrollSpeed:
        case ExplorerInputKind::SetScrollBarVisibility:
        case ExplorerInputKind::SetColorScheme:
        default:
            return ExplorerInputModality::Programmatic;
    }
}

inline ExplorerInputModality effective_input_modality(
        ExplorerInput const& input) {
    if (input.modality != ExplorerInputModality::Unspecified)
        return input.modality;
    return default_input_modality(input.kind);
}

inline void assign_focus(
        ExplorerState& state,
        ExplorerFocusTarget target,
        ExplorerInputModality modality) {
    state.last_input_modality = modality;
    state.focus_target = target;
    state.focus_visible = target != ExplorerFocusTarget::None
        && modality == ExplorerInputModality::Keyboard;
}

inline void apply_modality_without_focus_change(
        ExplorerState& state,
        ExplorerInputModality modality) {
    state.last_input_modality = modality;
    if (modality != ExplorerInputModality::Keyboard)
        state.focus_visible = false;
}

inline std::optional<std::size_t> focus_order_index(
        std::span<ExplorerFocusTarget const> order,
        ExplorerFocusTarget target) {
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == target)
            return i;
    }
    return std::nullopt;
}

inline void move_focus_by_tab(
        ExplorerState& state,
        std::string_view profile,
        bool reverse) {
    auto order = explorer_focus_order(profile);
    if (order.empty()) {
        assign_focus(
            state,
            ExplorerFocusTarget::None,
            ExplorerInputModality::Keyboard);
        state.status = "No focusable targets.";
        return;
    }

    auto index = focus_order_index(order, state.focus_target);
    std::size_t next = 0;
    if (index) {
        next = reverse
            ? (*index == 0 ? order.size() - 1 : *index - 1)
            : ((*index + 1) % order.size());
    } else if (reverse) {
        next = order.size() - 1;
    }
    assign_focus(state, order[next], ExplorerInputModality::Keyboard);
    state.status = std::string{"Keyboard focus moved to "}
        + focus_target_label(state.focus_target) + ".";
}

inline void apply_focus_policy_for_input(
        ExplorerState& state,
        ExplorerInput const& input,
        std::string_view profile) {
    auto modality = effective_input_modality(input);
    switch (input.kind) {
        case ExplorerInputKind::TabFocus:
            move_focus_by_tab(state, profile, false);
            return;
        case ExplorerInputKind::ShiftTabFocus:
            move_focus_by_tab(state, profile, true);
            return;
        case ExplorerInputKind::FocusTarget:
        case ExplorerInputKind::PointerFocus:
            if (auto target = focus_target_from_name(input.value)) {
                assign_focus(state, *target, modality);
            }
            return;
        case ExplorerInputKind::SelectLocation:
            assign_focus(state, ExplorerFocusTarget::Sidebar, modality);
            return;
        case ExplorerInputKind::FocusSearch:
        case ExplorerInputKind::Search:
            assign_focus(state, ExplorerFocusTarget::Search, modality);
            return;
        case ExplorerInputKind::SelectEntry:
        case ExplorerInputKind::OpenEntry:
        case ExplorerInputKind::ActivateEntry:
        case ExplorerInputKind::ActivateSelected:
        case ExplorerInputKind::MoveSelection:
            assign_focus(state, ExplorerFocusTarget::ContentGrid, modality);
            return;
        case ExplorerInputKind::DeleteSelected:
        case ExplorerInputKind::DuplicateSelected:
            assign_focus(state, ExplorerFocusTarget::ContentGrid, modality);
            return;
        case ExplorerInputKind::CreateFile:
        case ExplorerInputKind::CreateFolder:
            assign_focus(state, ExplorerFocusTarget::CreatePanel, modality);
            return;
        case ExplorerInputKind::DismissTransient:
            state.last_input_modality = modality;
            state.focus_visible = state.focus_target != ExplorerFocusTarget::None
                && modality == ExplorerInputModality::Keyboard;
            return;
        case ExplorerInputKind::Reset:
            assign_focus(
                state,
                ExplorerFocusTarget::None,
                ExplorerInputModality::Programmatic);
            return;
        case ExplorerInputKind::Noop:
        case ExplorerInputKind::ViewMode:
        case ExplorerInputKind::Viewport:
        case ExplorerInputKind::DraftName:
        case ExplorerInputKind::DraftFolderName:
        case ExplorerInputKind::DraftBody:
        case ExplorerInputKind::GoBack:
        case ExplorerInputKind::GoForward:
        case ExplorerInputKind::GoUp:
        case ExplorerInputKind::Sort:
        case ExplorerInputKind::CycleSort:
        case ExplorerInputKind::Scenario:
        case ExplorerInputKind::SetFontFamily:
        case ExplorerInputKind::SetFontScale:
        case ExplorerInputKind::SetBodyFontSize:
        case ExplorerInputKind::SetHeadingFontSize:
        case ExplorerInputKind::SetSmallFontSize:
        case ExplorerInputKind::SetLineHeightRatio:
        case ExplorerInputKind::SetSystemFontMetrics:
        case ExplorerInputKind::SetSystemScrollMetrics:
        case ExplorerInputKind::SetScrollSpeed:
        case ExplorerInputKind::SetHorizontalScrollSpeed:
        case ExplorerInputKind::SetScrollBarVisibility:
        case ExplorerInputKind::SetColorScheme:
        default:
            apply_modality_without_focus_change(state, modality);
            return;
    }
}

inline void remove_regular_file_if_present(fs::path const& path) {
    std::error_code ec;
    if (fs::is_regular_file(path, ec)) {
        ec.clear();
        fs::remove(path, ec);
    }
}

inline void remove_directory_if_present(fs::path const& path) {
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        ec.clear();
        fs::remove_all(path, ec);
    }
}

inline void apply_startup_scenario(
        ExplorerState& state,
        std::string_view scenario) {
    auto name = lower_copy(trim(scenario));
    if (name.empty() || name == "default")
        return;

    if (name == "created-preview" || name == "create") {
        select_location(state, "root");
        remove_regular_file_if_present(state.current / "Action Note.txt");
        state.draft_name = "Action Note";
        state.draft_body =
            "Created from artifact scenario.\n"
            "This note proves the file explorer can create and read a file.";
        create_file(state);
        state.mobile_tab = 1;
        return;
    }

    if (name == "deleted-file" || name == "delete") {
        select_location(state, "root");
        remove_regular_file_if_present(state.current / "Delete Me.txt");
        remove_regular_file_if_present(trash_path(state.root) / "Delete Me.txt");
        state.draft_name = "Delete Me";
        state.draft_body =
            "This temporary file should be moved to Trash before the artifact frame.";
        create_file(state);
        delete_selected(state);
        state.mobile_tab = 0;
        return;
    }

    if (name == "trash-view" || name == "trash") {
        select_location(state, "root");
        remove_regular_file_if_present(state.current / "Trash Note.txt");
        remove_regular_file_if_present(trash_path(state.root) / "Trash Note.txt");
        state.draft_name = "Trash Note";
        state.draft_body =
            "This note proves delete moves sandbox files to Trash.";
        create_file(state);
        delete_selected(state);
        select_location(state, "trash");
        state.mobile_tab = 0;
        return;
    }

    if (name == "created-folder" || name == "folder-create") {
        select_location(state, "root");
        remove_directory_if_present(state.current / "Review Folder");
        state.draft_folder_name = "Review Folder";
        create_folder(state);
        state.mobile_tab = 1;
        return;
    }

    if (name == "deleted-folder" || name == "folder-delete") {
        select_location(state, "root");
        remove_directory_if_present(state.current / "Trash Folder");
        remove_directory_if_present(trash_path(state.root) / "Trash Folder");
        state.draft_folder_name = "Trash Folder";
        create_folder(state);
        write_file_if_missing(
            state.current / state.selected_name / "Nested Note.txt",
            "Nested file proves folders move to Trash recursively.\n");
        delete_selected(state);
        state.mobile_tab = 0;
        return;
    }

    if (name == "duplicated-file" || name == "duplicate") {
        select_location(state, "root");
        remove_regular_file_if_present(state.current / "README copy.txt");
        remove_regular_file_if_present(state.current / "README copy 2.txt");
        select_entry(state, "README.txt");
        duplicate_selected(state);
        state.mobile_tab = 1;
        return;
    }

    if (name == "documents-preview" || name == "read") {
        select_location(state, "documents");
        select_entry(state, "Project Notes.txt");
        state.mobile_tab = 1;
        return;
    }

    if (name == "history-forward" || name == "history") {
        select_location(state, "documents");
        go_back(state);
        go_forward(state);
        state.mobile_tab = 0;
        return;
    }

    if (name == "sorted-kind" || name == "sort-kind") {
        set_sort_mode(state, SortMode::Kind);
        state.mobile_tab = 0;
        return;
    }

    if (name == "search-active" || name == "search") {
        select_location(state, "root");
        set_search_filter(state, "Screen");
        state.mobile_tab = 0;
        return;
    }

    if (name == "preferences-panel" || name == "preferences") {
        select_location(state, "root");
        state.mobile_tab = 2;
        state.status = "Display preferences ready.";
        return;
    }

    state.status = "Unknown startup scenario: " + std::string(scenario);
}

inline void apply_explorer_input(
        ExplorerState& state,
        ExplorerInput const& input,
        std::string_view profile) {
    apply_focus_policy_for_input(state, input, profile);
    switch (input.kind) {
        case ExplorerInputKind::Noop:
            state.status = "No input applied.";
            return;
        case ExplorerInputKind::SelectLocation:
            select_location(state, input.value);
            return;
        case ExplorerInputKind::SelectEntry:
            select_entry(state, input.value);
            return;
        case ExplorerInputKind::Search:
            set_search_filter(state, input.value);
            return;
        case ExplorerInputKind::FocusSearch:
            state.status = "Search ready.";
            return;
        case ExplorerInputKind::MoveSelection:
            move_selection(state, input.selection_move, profile);
            return;
        case ExplorerInputKind::OpenEntry:
            open_entry(state, input.value);
            return;
        case ExplorerInputKind::ActivateEntry:
            activate_entry(state, input.value);
            return;
        case ExplorerInputKind::ActivateSelected:
            activate_selected_entry(state);
            return;
        case ExplorerInputKind::ViewMode:
            state.view_mode = input.view_mode;
            state.status = "Switched to " + view_mode_label(input.view_mode) + ".";
            return;
        case ExplorerInputKind::Viewport:
            state.viewport_width = input.viewport_width;
            state.viewport_height = input.viewport_height;
            state.viewport_scale = input.viewport_scale;
            state.status = "Viewport set to " + input.value;
            return;
        case ExplorerInputKind::DraftName:
            state.draft_name = input.value;
            state.status = "Draft file name set.";
            return;
        case ExplorerInputKind::DraftFolderName:
            state.draft_folder_name = input.value;
            state.status = "Draft folder name set.";
            return;
        case ExplorerInputKind::DraftBody:
            state.draft_body = input.value;
            state.status = "Draft body set.";
            return;
        case ExplorerInputKind::CreateFile:
            create_file(state);
            return;
        case ExplorerInputKind::CreateFolder:
            create_folder(state);
            return;
        case ExplorerInputKind::DeleteSelected:
            delete_selected(state);
            return;
        case ExplorerInputKind::DuplicateSelected:
            duplicate_selected(state);
            return;
        case ExplorerInputKind::GoBack:
            go_back(state);
            return;
        case ExplorerInputKind::GoForward:
            go_forward(state);
            return;
        case ExplorerInputKind::GoUp:
            go_up(state);
            return;
        case ExplorerInputKind::Sort:
            set_sort_mode(state, input.sort_mode);
            return;
        case ExplorerInputKind::CycleSort:
            cycle_sort_mode(state);
            return;
        case ExplorerInputKind::SetFontFamily: {
            auto family = trim(input.value);
            auto lowered = lower_copy(family);
            state.preferences_source = "application-input";
            if (lowered == "system" || lowered == "os") {
                state.theme_preferences.prefer_system_font_family = true;
                state.theme_preferences.font_family.clear();
                state.status = "Using the OS font family.";
            } else if (lowered == "default" || lowered == "pretendard"
                       || lowered == "package") {
                state.theme_preferences.prefer_system_font_family = false;
                state.theme_preferences.font_family = "Pretendard";
                state.status = "Using the package font family.";
            } else if (!family.empty()) {
                state.theme_preferences.prefer_system_font_family = false;
                state.theme_preferences.font_family = std::string{family};
                state.status = "Font family set to " + std::string{family} + ".";
            } else {
                state.status = "Font family input was empty.";
            }
            return;
        }
        case ExplorerInputKind::SetFontScale: {
            auto scale = parse_preference_number(input.value, 0.75f, 1.8f);
            if (!scale) {
                state.status = "Font scale input was invalid.";
                return;
            }
            state.preferences_source = "application-input";
            state.theme_preferences.font_scale = *scale;
            state.status = "Text size set to "
                + compact_preference_number(*scale) + "x.";
            return;
        }
        case ExplorerInputKind::SetBodyFontSize: {
            auto size = parse_preference_number(input.value, 8.0f, 40.0f);
            if (!size) {
                state.status = "Body font size input was invalid.";
                return;
            }
            state.preferences_source = "application-input";
            state.theme_preferences.body_font_size = *size;
            state.status = "Body font size set to "
                + compact_preference_number(*size) + "pt.";
            return;
        }
        case ExplorerInputKind::SetHeadingFontSize: {
            auto size = parse_preference_number(input.value, 10.0f, 56.0f);
            if (!size) {
                state.status = "Heading font size input was invalid.";
                return;
            }
            state.preferences_source = "application-input";
            state.theme_preferences.heading_font_size = *size;
            state.status = "Heading font size set to "
                + compact_preference_number(*size) + "pt.";
            return;
        }
        case ExplorerInputKind::SetSmallFontSize: {
            auto size = parse_preference_number(input.value, 8.0f, 32.0f);
            if (!size) {
                state.status = "Small font size input was invalid.";
                return;
            }
            state.preferences_source = "application-input";
            state.theme_preferences.small_font_size = *size;
            state.status = "Small font size set to "
                + compact_preference_number(*size) + "pt.";
            return;
        }
        case ExplorerInputKind::SetLineHeightRatio: {
            auto ratio = parse_preference_number(input.value, 1.0f, 2.2f);
            if (!ratio) {
                state.status = "Line height input was invalid.";
                return;
            }
            state.preferences_source = "application-input";
            state.theme_preferences.line_height_ratio = *ratio;
            state.status = "Line height set to "
                + compact_preference_number(*ratio) + "x.";
            return;
        }
        case ExplorerInputKind::SetSystemFontMetrics: {
            auto value = lower_copy(trim(input.value));
            bool const enabled = value.empty()
                || value == "true" || value == "1" || value == "yes"
                || value == "on" || value == "system";
            bool const disabled = value == "false" || value == "0"
                || value == "no" || value == "off" || value == "package";
            if (!enabled && !disabled) {
                state.status = "System font metrics input was invalid.";
                return;
            }
            state.preferences_source = "application-input";
            state.theme_preferences.apply_system_font_metrics = enabled;
            state.status = enabled
                ? "Using OS font size metrics."
                : "Using package font size metrics.";
            return;
        }
        case ExplorerInputKind::SetSystemScrollMetrics: {
            auto value = lower_copy(trim(input.value));
            bool const enabled = value.empty()
                || value == "true" || value == "1" || value == "yes"
                || value == "on" || value == "system";
            bool const disabled = value == "false" || value == "0"
                || value == "no" || value == "off" || value == "app";
            if (!enabled && !disabled) {
                state.status = "System scroll metrics input was invalid.";
                return;
            }
            state.preferences_source = "application-input";
            state.theme_preferences.apply_system_scroll_metrics = enabled;
            state.status = enabled
                ? "Using OS scroll metrics."
                : "Using app-only scroll metrics.";
            return;
        }
        case ExplorerInputKind::SetScrollSpeed: {
            auto speed = parse_preference_number(input.value, 0.25f, 4.0f);
            if (!speed) {
                state.status = "Scroll speed input was invalid.";
                return;
            }
            state.preferences_source = "application-input";
            state.theme_preferences.scroll_delta_multiplier = *speed;
            state.status = "Scroll speed set to "
                + compact_preference_number(*speed) + "x.";
            return;
        }
        case ExplorerInputKind::SetHorizontalScrollSpeed: {
            auto speed = parse_preference_number(input.value, 0.25f, 4.0f);
            if (!speed) {
                state.status = "Horizontal scroll speed input was invalid.";
                return;
            }
            state.preferences_source = "application-input";
            state.theme_preferences.scroll_horizontal_delta_multiplier = *speed;
            state.status = "Horizontal scroll speed set to "
                + compact_preference_number(*speed) + "x.";
            return;
        }
        case ExplorerInputKind::SetScrollBarVisibility: {
            auto const mode = normalized_scroll_bar_visibility(input.value);
            state.preferences_source = "application-input";
            state.theme_preferences.scroll_bar_visibility = mode;
            if (mode == "always") {
                state.status = "Scroll bars are always visible.";
            } else if (mode == "hidden") {
                state.status = "Scroll bars are hidden.";
            } else {
                state.status = "Scroll bars appear while scrolling.";
            }
            return;
        }
        case ExplorerInputKind::SetMotionScale: {
            auto scale = parse_motion_preference_number(input.value);
            if (!scale) {
                state.status = "Motion scale input was invalid.";
                return;
            }
            state.preferences_source = "application-input";
            state.theme_preferences.motion_duration_multiplier = *scale;
            state.status = *scale == 0.0f
                ? "Animation motion disabled."
                : "Animation motion set to "
                    + compact_preference_number(*scale) + "x.";
            return;
        }
        case ExplorerInputKind::SetColorScheme: {
            auto scheme = lower_copy(trim(input.value));
            state.preferences_source = "application-input";
            if (scheme == "system") {
                state.theme_preferences.prefer_system_color_scheme = true;
                state.theme_preferences.color_scheme.clear();
                state.status = "Using the OS appearance.";
            } else if (scheme == "light" || scheme == "dark"
                       || scheme == "high-contrast-light"
                       || scheme == "high-contrast-dark") {
                state.theme_preferences.prefer_system_color_scheme = false;
                state.theme_preferences.color_scheme = std::string{scheme};
                state.status = "Appearance set to " + std::string{scheme} + ".";
            } else {
                state.status = "Appearance input was invalid.";
            }
            return;
        }
        case ExplorerInputKind::Reset:
            reset_demo_tree(state, profile);
            return;
        case ExplorerInputKind::Scenario:
            apply_startup_scenario(state, input.value);
            return;
        case ExplorerInputKind::DismissTransient:
            state.status = "Transient UI dismissed.";
            return;
        case ExplorerInputKind::FocusTarget:
            state.status = "Keyboard focus set to "
                + focus_target_label(state.focus_target) + ".";
            return;
        case ExplorerInputKind::PointerFocus:
            state.status = "Pointer focus set to "
                + focus_target_label(state.focus_target) + ".";
            return;
        case ExplorerInputKind::TabFocus:
        case ExplorerInputKind::ShiftTabFocus:
            return;
    }
}
} // namespace file_explorer_demo

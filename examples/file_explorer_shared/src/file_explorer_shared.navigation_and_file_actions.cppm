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

export module file_explorer_shared:navigation_and_file_actions;

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

export namespace file_explorer_demo {
inline fs::path location_path(ExplorerState const& state, std::string_view id) {
    if (id == "documents")
        return state.root / "Documents";
    if (id == "pictures")
        return state.root / "Pictures";
    if (id == "shared")
        return state.root / "Shared";
    if (id == "trash")
        return trash_path(state.root);
    return state.root;
}

inline void select_location(ExplorerState& state, std::string_view id) {
    std::error_code ec;
    auto next = location_path(state, id);
    if (!fs::is_directory(next, ec)) {
        state.status = "Location is not available.";
        return;
    }
    open_directory(
        state,
        next,
        "Opened " + relative_location(state.root, next, state.root_label));
}

inline void go_up(ExplorerState& state) {
    if (state.current == state.root) {
        state.status = "Already at " + state.root_label + ".";
        return;
    }
    auto next = state.current.parent_path();
    open_directory(
        state,
        next,
        "Moved up to " + relative_location(state.root, next, state.root_label));
}

inline void go_back(ExplorerState& state) {
    if (state.back_stack.empty()) {
        state.status = "No previous location.";
        return;
    }
    if (!state.current.empty()) {
        state.forward_stack.push_back(state.current);
        trim_history(state.forward_stack);
    }
    auto next = state.back_stack.back();
    state.back_stack.pop_back();
    open_directory(
        state,
        next,
        "Went back to " + relative_location(state.root, next, state.root_label),
        false);
}

inline void go_forward(ExplorerState& state) {
    if (state.forward_stack.empty()) {
        state.status = "No forward location.";
        return;
    }
    if (!state.current.empty()) {
        state.back_stack.push_back(state.current);
        trim_history(state.back_stack);
    }
    auto next = state.forward_stack.back();
    state.forward_stack.pop_back();
    open_directory(
        state,
        next,
        "Went forward to " + relative_location(state.root, next, state.root_label),
        false);
}

inline void set_sort_mode(ExplorerState& state, SortMode mode) {
    state.sort_mode = mode;
    state.status = "Sorted by " + sort_mode_label(mode);
}

inline void cycle_sort_mode(ExplorerState& state) {
    set_sort_mode(state, next_sort_mode(state.sort_mode));
}

inline void set_search_filter(ExplorerState& state, std::string text) {
    state.search = std::move(text);
    auto query = trim(state.search);
    if (!state.selected_name.empty()
        && !matches_filter(state.selected_name, state.search)) {
        state.selected_name.clear();
    }
    state.status = query.empty()
        ? "Search cleared."
        : "Searching for " + query;
}

inline void select_entry(ExplorerState& state, std::string const& name) {
    auto resolved = direct_child_entry_path(state, name);
    if (!resolved) {
        state.status = "Entry name must stay inside the current folder.";
        record_operation(
            state,
            "file_read",
            name,
            false,
            state.status);
        return;
    }
    std::error_code ec;
    if (fs::is_directory(*resolved, ec)) {
        auto plan = planned_entry_read(state, name, *resolved, true);
        state.selected_name = name;
        state.status = "Selected folder " + name;
        record_operation(
            state,
            std::move(plan),
            true,
            "Selected " + name + " for folder actions");
        return;
    }
    ec.clear();
    if (fs::is_regular_file(*resolved, ec)) {
        auto plan = planned_entry_read(state, name, *resolved, false);
        state.selected_name = name;
        state.status = "Selected " + name;
        record_operation(
            state,
            std::move(plan),
            true,
            "Selected " + name + " for preview");
        return;
    }
    state.status = "Entry is no longer available.";
    record_operation(
        state,
        "file_read",
        name,
        false,
        state.status);
}

inline std::optional<std::size_t> selected_entry_index(
        std::vector<Entry> const& entries,
        std::string const& selected_name) {
    if (selected_name.empty())
        return std::nullopt;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].name == selected_name)
            return i;
    }
    return std::nullopt;
}

inline int selection_navigation_stride(
        ExplorerState const& state,
        ExplorerSelectionMove move,
        ExplorerChromeMetrics const& chrome) {
    bool const icon_grid = state.view_mode == ExplorerViewMode::Icon;
    int const columns = std::max(1, chrome.icon_grid_columns);
    int const page_span = icon_grid
        ? std::max(columns, chrome.icon_grid_visible_capacity)
        : std::max(6, chrome.icon_grid_visible_rows);
    switch (move) {
        case ExplorerSelectionMove::Left: return -1;
        case ExplorerSelectionMove::Right: return 1;
        case ExplorerSelectionMove::Up: return icon_grid ? -columns : -1;
        case ExplorerSelectionMove::Down: return icon_grid ? columns : 1;
        case ExplorerSelectionMove::PageUp: return -page_span;
        case ExplorerSelectionMove::PageDown: return page_span;
        case ExplorerSelectionMove::Home:
        case ExplorerSelectionMove::End:
            return 0;
    }
    return 0;
}

inline void move_selection(
        ExplorerState& state,
        ExplorerSelectionMove move,
        std::string_view profile) {
    auto entries = list_entries(state.current, state.search, state.sort_mode);
    if (entries.empty()) {
        state.selected_name.clear();
        state.status = "No visible entries to navigate.";
        return;
    }

    auto const current = selected_entry_index(entries, state.selected_name);
    std::ptrdiff_t target = 0;
    if (!current) {
        target = move == ExplorerSelectionMove::End
            ? static_cast<std::ptrdiff_t>(entries.size() - 1)
            : 0;
    } else if (move == ExplorerSelectionMove::Home) {
        target = 0;
    } else if (move == ExplorerSelectionMove::End) {
        target = static_cast<std::ptrdiff_t>(entries.size() - 1);
    } else {
        auto const chrome = explorer_chrome_metrics(state, profile);
        target = static_cast<std::ptrdiff_t>(*current)
            + selection_navigation_stride(state, move, chrome);
    }

    auto const last = static_cast<std::ptrdiff_t>(entries.size() - 1);
    target = std::clamp<std::ptrdiff_t>(target, 0, last);
    auto selected = entries[static_cast<std::size_t>(target)].name;
    select_entry(state, selected);
    state.status = "Moved selection with "
        + selection_move_label(move)
        + " to " + selected + ".";
}

inline void open_entry(ExplorerState& state, std::string const& name) {
    auto resolved = direct_child_entry_path(state, name);
    if (!resolved) {
        select_entry(state, name);
        return;
    }
    std::error_code ec;
    if (fs::is_directory(*resolved, ec)) {
        auto plan = planned_folder_open(state, name, *resolved);
        open_directory(state, *resolved, "Opened " + name);
        record_operation(
            state,
            std::move(plan),
            true,
            state.status);
        return;
    }
    select_entry(state, name);
}

inline void activate_entry(ExplorerState& state, std::string const& name) {
    auto resolved = direct_child_entry_path(state, name);
    if (!resolved) {
        select_entry(state, name);
        return;
    }
    std::error_code ec;
    bool const selected = state.selected_name == name;
    if (selected && fs::is_directory(*resolved, ec)) {
        auto plan = planned_folder_open(state, name, *resolved);
        open_directory(state, *resolved, "Opened " + name);
        record_operation(
            state,
            std::move(plan),
            true,
            state.status);
        return;
    }
    select_entry(state, name);
}

inline void activate_selected_entry(ExplorerState& state) {
    if (state.selected_name.empty()) {
        state.status = "Select a file or folder before opening.";
        record_operation(
            state,
            "activate_selected",
            {},
            false,
            state.status);
        return;
    }
    activate_entry(state, state.selected_name);
}

inline fs::path unique_child_path(fs::path const& parent, std::string name) {
    std::error_code ec;
    auto candidate = parent / name;
    if (!fs::exists(candidate, ec))
        return candidate;
    auto stem = candidate.stem().string();
    auto ext = candidate.extension().string();
    for (int i = 2; i < 100; ++i) {
        auto next = parent / (stem + " " + std::to_string(i) + ext);
        if (!fs::exists(next, ec))
            return next;
    }
    return parent / (stem + " copy" + ext);
}

inline void create_file(ExplorerState& state) {
    auto name = sanitize_file_name(state.draft_name);
    auto path = unique_child_path(state.current, name);
    auto plan = planned_file_create(state, path.filename().string(), path);
    if (!state.filesystem_mutations_allowed) {
        plan.executable = false;
        plan.fallback_reason = "Filesystem mutations are disabled for this root.";
        state.status = "File changes are disabled for this location.";
        record_operation(state, std::move(plan), false, state.status);
        return;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        state.status = "Could not create " + name;
        record_operation(state, std::move(plan), false, state.status);
        return;
    }
    out << state.draft_body << "\n";
    if (!out) {
        state.status = "Could not write " + name;
        record_operation(state, std::move(plan), false, state.status);
        return;
    }
    state.selected_name = path.filename().string();
    state.status = "Created " + state.selected_name;
    record_operation(
        state,
        std::move(plan),
        true,
        state.status);
}

inline void create_folder(ExplorerState& state) {
    auto name = sanitize_folder_name(state.draft_folder_name);
    auto path = unique_child_path(state.current, name);
    auto plan = planned_folder_create(state, path.filename().string(), path);
    if (!state.filesystem_mutations_allowed) {
        plan.executable = false;
        plan.fallback_reason = "Filesystem mutations are disabled for this root.";
        state.status = "File changes are disabled for this location.";
        record_operation(state, std::move(plan), false, state.status);
        return;
    }
    std::error_code ec;
    if (!fs::create_directory(path, ec) || ec) {
        state.status = "Could not create folder " + name;
        record_operation(state, std::move(plan), false, state.status);
        return;
    }
    state.selected_name = path.filename().string();
    state.status = "Created folder " + state.selected_name;
    record_operation(
        state,
        std::move(plan),
        true,
        state.status);
}

inline void delete_selected(ExplorerState& state) {
    if (state.selected_name.empty()) {
        state.status = "Select a file or folder before deleting.";
        record_operation(state, "file_delete", {}, false, state.status);
        return;
    }
    if (!state.filesystem_mutations_allowed) {
        auto rejected = state.selected_name;
        auto resolved = direct_child_entry_path(state, rejected);
        auto plan = resolved
            ? planned_delete(
                state,
                rejected,
                *resolved,
                false,
                false)
            : OperationPlan{
                .kind = "file_delete",
                .display_name = rejected,
                .executable = false,
            };
        plan.executable = false;
        plan.fallback_reason = "Filesystem mutations are disabled for this root.";
        state.status = "File changes are disabled for this location.";
        record_operation(state, std::move(plan), false, state.status);
        return;
    }
    auto resolved = direct_child_entry_path(state, state.selected_name);
    if (!resolved) {
        auto rejected = state.selected_name;
        state.selected_name.clear();
        state.status = "Selected entry is outside the current folder.";
        record_operation(
            state,
            "file_delete",
            rejected,
            false,
            state.status);
        return;
    }
    auto path = *resolved;
    auto trash = trash_path(state.root);
    bool const deleting_from_trash = same_path(state.current, trash);
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        if (!deletable_directory(state.root, path)) {
            auto plan = planned_delete(
                state,
                state.selected_name,
                path,
                true,
                deleting_from_trash);
            plan.executable = false;
            plan.fallback_reason = "Only sandbox folders can be deleted.";
            state.status = "Only sandbox folders can be deleted.";
            record_operation(
                state,
                std::move(plan),
                false,
                state.status);
            return;
        }
        ec.clear();
        if (deleting_from_trash) {
            auto plan = planned_delete(
                state,
                state.selected_name,
                path,
                true,
                true);
            auto const removed = fs::remove_all(path, ec);
            if (ec || removed == 0) {
                state.status = "Could not delete folder " + state.selected_name;
                record_operation(
                    state,
                    std::move(plan),
                    false,
                    state.status);
                return;
            }
            state.status = "Deleted folder " + state.selected_name + " from Trash";
            state.selected_name.clear();
            record_operation(
                state,
                std::move(plan),
                true,
                state.status);
            return;
        }
        fs::create_directories(trash, ec);
        ec.clear();
        auto target = unique_child_path(trash, state.selected_name);
        auto plan = planned_delete(
            state,
            state.selected_name,
            path,
            true,
            false,
            target);
        fs::rename(path, target, ec);
        if (ec) {
            state.status = "Could not move folder " + state.selected_name + " to Trash";
            record_operation(
                state,
                std::move(plan),
                false,
                state.status);
            return;
        }
        state.status = "Moved folder " + state.selected_name + " to Trash";
        state.selected_name.clear();
        record_operation(
            state,
            std::move(plan),
            true,
            state.status);
        return;
    }
    ec.clear();
    if (!fs::is_regular_file(path, ec)) {
        state.status = "Only files or sandbox folders can be deleted.";
        record_operation(
            state,
            "file_delete",
            state.selected_name,
            false,
            state.status);
        return;
    }
    if (deleting_from_trash) {
        auto plan = planned_delete(
            state,
            state.selected_name,
            path,
            false,
            true);
        if (!fs::remove(path, ec) || ec) {
            state.status = "Could not delete " + state.selected_name;
            record_operation(
                state,
                std::move(plan),
                false,
                state.status);
            return;
        }
        state.status = "Deleted " + state.selected_name + " from Trash";
        state.selected_name.clear();
        record_operation(
            state,
            std::move(plan),
            true,
            state.status);
        return;
    }
    fs::create_directories(trash, ec);
    ec.clear();
    auto target = unique_child_path(trash, state.selected_name);
    auto plan = planned_delete(
        state,
        state.selected_name,
        path,
        false,
        false,
        target);
    fs::rename(path, target, ec);
    if (ec) {
        state.status = "Could not delete " + state.selected_name;
        record_operation(
            state,
            std::move(plan),
            false,
            state.status);
        return;
    }
    state.status = "Moved " + state.selected_name + " to Trash";
    state.selected_name.clear();
    record_operation(
        state,
        std::move(plan),
        true,
        state.status);
}

inline void duplicate_selected(ExplorerState& state) {
    if (state.selected_name.empty()) {
        state.status = "Select a file before duplicating.";
        record_operation(state, "file_duplicate", {}, false, state.status);
        return;
    }
    if (!state.filesystem_mutations_allowed) {
        auto rejected = state.selected_name;
        auto resolved = direct_child_entry_path(state, rejected);
        auto plan = resolved
            ? planned_duplicate(state, rejected, *resolved, state.current / rejected)
            : OperationPlan{
                .kind = "file_duplicate",
                .display_name = rejected,
                .executable = false,
            };
        plan.executable = false;
        plan.fallback_reason = "Filesystem mutations are disabled for this root.";
        state.status = "File changes are disabled for this location.";
        record_operation(state, std::move(plan), false, state.status);
        return;
    }
    auto resolved = direct_child_entry_path(state, state.selected_name);
    if (!resolved) {
        auto rejected = state.selected_name;
        state.selected_name.clear();
        state.status = "Selected entry is outside the current folder.";
        record_operation(
            state,
            "file_duplicate",
            rejected,
            false,
            state.status);
        return;
    }
    auto source = *resolved;
    std::error_code ec;
    if (!fs::is_regular_file(source, ec)) {
        state.status = "Only files can be duplicated in this demo.";
        record_operation(
            state,
            "file_duplicate",
            state.selected_name,
            false,
            state.status);
        return;
    }
    auto candidate_name =
        source.stem().string() + " copy" + source.extension().string();
    auto target = unique_child_path(state.current, candidate_name);
    auto plan = planned_duplicate(
        state,
        target.filename().string(),
        source,
        target);
    ec.clear();
    if (!fs::copy_file(source, target, fs::copy_options::none, ec) || ec) {
        state.status = "Could not duplicate " + state.selected_name;
        record_operation(
            state,
            std::move(plan),
            false,
            state.status);
        return;
    }
    auto previous = state.selected_name;
    state.selected_name = target.filename().string();
    state.status = "Duplicated " + previous + " to " + state.selected_name;
    record_operation(
        state,
        std::move(plan),
        true,
        state.status);
}

inline void reset_demo_tree(ExplorerState& state, std::string_view profile) {
    if (!state.root_is_demo) {
        state.current = state.root;
        state.selected_name.clear();
        state.search.clear();
        state.back_stack.clear();
        state.forward_stack.clear();
        state.last_operation = {};
        state.sort_mode = SortMode::Name;
        state.view_mode = ExplorerViewMode::Icon;
        state.last_input_modality = ExplorerInputModality::Programmatic;
        state.focus_target = ExplorerFocusTarget::None;
        state.focus_visible = false;
        state.status = "Real filesystem location refreshed.";
        return;
    }
    std::error_code ec;
    if (!state.root.empty())
        fs::remove_all(state.root, ec);
    state.root = ensure_demo_tree(profile);
    state.current = state.root;
    state.selected_name.clear();
    state.search.clear();
    state.draft_name = "New Note.txt";
    state.draft_folder_name = "New Folder";
    state.draft_body = "Created from the phenotype file explorer demo.";
    state.back_stack.clear();
    state.forward_stack.clear();
    state.last_operation = {};
    state.sort_mode = default_sort_mode(profile);
    state.view_mode = ExplorerViewMode::Icon;
    state.last_input_modality = ExplorerInputModality::Programmatic;
    state.focus_target = ExplorerFocusTarget::None;
    state.focus_visible = false;
    state.status = "Demo files reset.";
}
} // namespace file_explorer_demo

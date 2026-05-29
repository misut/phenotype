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

export module file_explorer_shared:debug_payload_json;

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

export namespace file_explorer_demo {
inline json::Value file_explorer_debug_json(
        ExplorerState const& state,
        Snapshot const& snap,
        ExplorerChromeMetrics const& chrome,
        std::string_view profile) {
    json::Object selection;
    selection.emplace("present", json::Value{snap.has_selection});
    selection.emplace(
        "name",
        json::Value{snap.has_selection ? snap.selected.name : std::string{}});
    selection.emplace(
        "kind",
        json::Value{snap.has_selection ? snap.selected_kind_label : std::string{}});
    selection.emplace(
        "index",
        json::Value{static_cast<std::int64_t>(
            snap.has_selection ? snap.selected_index : -1)});
    selection.emplace("size_label", json::Value{snap.selected_size_label});
    selection.emplace("path_label", json::Value{snap.selected_path_label});
    selection.emplace("can_preview", json::Value{snap.can_preview_selected});

    json::Object counts;
    counts.emplace("visible_entries", json::Value{static_cast<std::int64_t>(snap.entries.size())});
    counts.emplace("files", json::Value{static_cast<std::int64_t>(snap.file_count)});
    counts.emplace("folders", json::Value{static_cast<std::int64_t>(snap.folder_count)});

    json::Object capabilities;
    capabilities.emplace("can_go_back", json::Value{snap.can_go_back});
    capabilities.emplace("can_go_forward", json::Value{snap.can_go_forward});
    capabilities.emplace("can_create_file", json::Value{snap.can_create_file});
    capabilities.emplace("can_create_folder", json::Value{snap.can_create_folder});
    capabilities.emplace("can_delete_selected", json::Value{snap.can_delete_selected});
    capabilities.emplace("can_duplicate_selected", json::Value{snap.can_duplicate_selected});
    capabilities.emplace("can_preview_selected", json::Value{snap.can_preview_selected});

    json::Object sort;
    sort.emplace("value", json::Value{sort_mode_value_name(snap.sort_mode)});
    sort.emplace("label", json::Value{snap.sort_label});

    json::Object view_mode;
    view_mode.emplace("value", json::Value{view_mode_value_name(snap.view_mode)});
    view_mode.emplace("label", json::Value{view_mode_label(snap.view_mode)});

    json::Array entries;
    std::size_t const limit = std::min<std::size_t>(snap.entries.size(), 24);
    for (std::size_t i = 0; i < limit; ++i)
        entries.push_back(entry_debug_json(snap.entries[i]));

    json::Object out;
    out.emplace("schema_version", json::Value{1});
    out.emplace("kind", json::Value{"file_explorer"});
    out.emplace("profile", json::Value{std::string{profile}});
    out.emplace("root", json::Value{snap.root.string()});
    out.emplace("current", json::Value{snap.current.string()});
    out.emplace("root_label", json::Value{snap.root_label});
    out.emplace("root_source", json::Value{snap.root_source});
    out.emplace("root_is_demo", json::Value{snap.root_is_demo});
    out.emplace(
        "filesystem_mutations_allowed",
        json::Value{snap.filesystem_mutations_allowed});
    out.emplace("relative_location", json::Value{snap.relative_location});
    out.emplace("status", json::Value{state.status});
    out.emplace("search", json::Value{state.search});
    out.emplace("item_summary", json::Value{snap.item_summary});
    out.emplace("action_summary", json::Value{snap.action_summary});
    out.emplace("sort", json::Value{std::move(sort)});
    out.emplace("view_mode", json::Value{std::move(view_mode)});
    out.emplace("counts", json::Value{std::move(counts)});
    out.emplace("selection", json::Value{std::move(selection)});
    out.emplace("capabilities", json::Value{std::move(capabilities)});
    out.emplace(
        "operation",
        operation_receipt_debug_json(state.last_operation, snap.operation_label));
    out.emplace("chrome", explorer_chrome_debug_json(chrome));
    out.emplace(
        "finder_visual_contract",
        finder_visual_contract_debug_json(chrome, profile));
    out.emplace("theme_system", explorer_theme_system_debug_json(chrome));
    out.emplace("preferences", preferences_debug_json(state));
    out.emplace("resource_system", file_explorer_resource_system_debug_json(profile));
    out.emplace("input_model", input_model_debug_json(state, profile));
    out.emplace("keyboard_commands", keyboard_commands_debug_json(profile));
    out.emplace("entries_sample", json::Value{std::move(entries)});
    out.emplace("entry_symbol_summary", entry_symbol_summary_debug_json(snap));
    out.emplace("mobile_tab", json::Value{static_cast<std::int64_t>(state.mobile_tab)});
    return json::Value{std::move(out)};
}

inline json::Value file_explorer_application_debug_json(
        ExplorerState const& state,
        Snapshot const& snap,
        ExplorerChromeMetrics const& chrome,
        std::string_view profile) {
    json::Object out;
    out.emplace(
        "file_explorer",
        file_explorer_debug_json(state, snap, chrome, profile));
    return json::Value{std::move(out)};
}

} // namespace file_explorer_demo

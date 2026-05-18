export module phenotype_cli.file_explorer;

import file_explorer_shared;
import phenotype_cli.common;
import std;

namespace phenotype_cli::file_explorer {

using phenotype_cli::common::json_string;

export auto operation_receipt_json(
        file_explorer_demo::OperationReceipt const& receipt) -> std::string {
    auto const& plan = receipt.plan;
    auto plan_json = std::format(
        "{{\"kind\":{},\"display_name\":{},\"source\":{},"
        "\"destination\":{},\"executable\":{},\"sandboxed\":{},"
        "\"mutates_filesystem\":{},\"reads_file\":{},\"reads_directory\":{},"
        "\"writes_file\":{},"
        "\"creates_directory\":{},\"deletes_file\":{},"
        "\"deletes_directory\":{},\"moves_to_trash\":{},"
        "\"permanent_delete\":{},\"fallback_reason\":{}}}",
        json_string(plan.kind),
        json_string(plan.display_name),
        json_string(plan.source),
        json_string(plan.destination),
        plan.executable ? "true" : "false",
        plan.sandboxed ? "true" : "false",
        plan.mutates_filesystem ? "true" : "false",
        plan.reads_file ? "true" : "false",
        plan.reads_directory ? "true" : "false",
        plan.writes_file ? "true" : "false",
        plan.creates_directory ? "true" : "false",
        plan.deletes_file ? "true" : "false",
        plan.deletes_directory ? "true" : "false",
        plan.moves_to_trash ? "true" : "false",
        plan.permanent_delete ? "true" : "false",
        json_string(plan.fallback_reason));
    return std::format(
        "{{\"kind\":{},\"target\":{},\"ok\":{},\"detail\":{},"
        "\"plan\":{}}}",
        json_string(receipt.kind),
        json_string(receipt.target),
        receipt.ok ? "true" : "false",
        json_string(receipt.detail),
        plan_json);
}

export auto explorer_input_json(file_explorer_demo::ExplorerInput const& input)
    -> std::string {
    return std::format(
        "{{\"kind\":{},\"value\":{},\"modality\":{},"
        "\"sort_mode\":{},\"view_mode\":{},"
        "\"selection_move\":{},"
        "\"viewport\":{{\"w\":{},\"h\":{},\"scale\":{}}},\"label\":{}}}",
        json_string(file_explorer_demo::explorer_input_kind_name(input.kind)),
        json_string(input.value),
        json_string(file_explorer_demo::input_modality_value_name(
            input.modality)),
        json_string(file_explorer_demo::sort_mode_label(input.sort_mode)),
        json_string(file_explorer_demo::view_mode_value_name(input.view_mode)),
        json_string(file_explorer_demo::selection_move_value_name(
            input.selection_move)),
        input.viewport_width,
        input.viewport_height,
        input.viewport_scale,
        json_string(file_explorer_demo::explorer_input_label(input)));
}

export auto explorer_entry_json(file_explorer_demo::Entry const& entry)
    -> std::string {
    return std::format(
        "{{\"name\":{},\"folder\":{},\"size\":{},\"kind\":{},"
        "\"symbol\":{},\"symbol_semantic_reference_name\":{}}}",
        json_string(entry.name),
        entry.folder ? "true" : "false",
        entry.size,
        json_string(file_explorer_demo::entry_kind_label(entry)),
        json_string(file_explorer_demo::entry_symbol_name(entry)),
        json_string(
            file_explorer_demo::entry_symbol_semantic_reference_name(entry)));
}

export auto explorer_entries_json(
        std::span<file_explorer_demo::Entry const> entries) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (i > 0)
            out += ",";
        out += explorer_entry_json(entries[i]);
    }
    out += "]";
    return out;
}

export auto explorer_labels_json(
        file_explorer_demo::ExplorerLabels const& labels) -> std::string {
    return std::format(
        "{{\"title\":{},\"mobile_title\":{},\"sidebar_recents\":{},"
        "\"sidebar_shared\":{},\"favorites\":{},\"locations\":{},"
        "\"applications\":{},\"desktop\":{},\"documents\":{},"
        "\"pictures\":{},\"downloads\":{},\"icloud_drive\":{},"
        "\"home\":{},\"airdrop\":{},\"trash\":{},"
        "\"search_placeholder\":{},\"status_ready\":{},"
        "\"tab_browse\":{},\"tab_preview\":{},\"tab_create\":{},"
        "\"create_file\":{},\"create_folder\":{},\"duplicate\":{},"
        "\"duplicate_file\":{},\"delete\":{},\"delete_selected\":{},"
        "\"preview\":{},\"root\":{},\"docs\":{},\"pics\":{},"
        "\"back\":{},\"forward\":{},\"up\":{},\"sort\":{},"
        "\"name\":{},\"kind\":{},\"size\":{},"
        "\"no_matching_files\":{},\"select_file_to_preview\":{},"
        "\"select_file_from_browse\":{},\"create_hint\":{},"
        "\"file_name\":{},\"contents\":{},\"folder_name\":{},"
        "\"reset_demo_files\":{},\"status\":{}}}",
        json_string(labels.title),
        json_string(labels.mobile_title),
        json_string(labels.sidebar_recents),
        json_string(labels.sidebar_shared),
        json_string(labels.favorites),
        json_string(labels.locations),
        json_string(labels.applications),
        json_string(labels.desktop),
        json_string(labels.documents),
        json_string(labels.pictures),
        json_string(labels.downloads),
        json_string(labels.icloud_drive),
        json_string(labels.home),
        json_string(labels.airdrop),
        json_string(labels.trash),
        json_string(labels.search_placeholder),
        json_string(labels.status_ready),
        json_string(labels.tab_browse),
        json_string(labels.tab_preview),
        json_string(labels.tab_create),
        json_string(labels.create_file),
        json_string(labels.create_folder),
        json_string(labels.duplicate),
        json_string(labels.duplicate_file),
        json_string(labels.delete_action),
        json_string(labels.delete_selected),
        json_string(labels.preview),
        json_string(labels.root),
        json_string(labels.docs),
        json_string(labels.pics),
        json_string(labels.back),
        json_string(labels.forward),
        json_string(labels.up),
        json_string(labels.sort),
        json_string(labels.name),
        json_string(labels.kind),
        json_string(labels.size),
        json_string(labels.no_matching_files),
        json_string(labels.select_file_to_preview),
        json_string(labels.select_file_from_browse),
        json_string(labels.create_hint),
        json_string(labels.file_name),
        json_string(labels.contents),
        json_string(labels.folder_name),
        json_string(labels.reset_demo_files),
        json_string(labels.status));
}

export auto explorer_theme_system_json(
        file_explorer_demo::ExplorerChromeMetrics const& chrome) -> std::string {
    return std::format(
        "{{\"contract_version\":{},\"profile_name\":{},"
        "\"reference\":{},\"font_policy\":{},\"material_policy\":{},"
        "\"iconography_policy\":{},\"icon_asset_policy\":{},"
        "\"usage_policy\":{},\"container_policy\":{},"
        "\"performance_policy\":{},\"accessibility_policy\":{},"
        "\"fallback_policy\":{}}}",
        chrome.theme_contract_version,
        json_string(chrome.theme_profile_name),
        json_string(chrome.theme_reference),
        json_string(chrome.theme_font_policy),
        json_string(chrome.theme_material_policy),
        json_string(chrome.theme_iconography_policy),
        json_string(chrome.theme_icon_asset_policy),
        json_string(chrome.theme_usage_policy),
        json_string(chrome.theme_container_policy),
        json_string(chrome.theme_performance_policy),
        json_string(chrome.theme_accessibility_policy),
        json_string(chrome.theme_fallback_policy));
}

export auto explorer_keyboard_commands_json(std::string_view profile)
    -> std::string {
    auto commands = file_explorer_demo::file_explorer_keyboard_commands(profile);
    auto out = std::string{"["};
    for (std::size_t i = 0; i < commands.size(); ++i) {
        if (i > 0)
            out += ",";
        auto const& command = commands[i];
        out += std::format(
            "{{\"action\":{},\"shortcut\":{},\"input_alias\":{},"
            "\"allow_when_input_focused\":{}}}",
            json_string(command.action),
            json_string(command.shortcut),
            json_string(command.input_alias),
            command.allow_when_input_focused ? "true" : "false");
    }
    out += "]";
    return out;
}

export auto explorer_focus_order_json(std::string_view profile) -> std::string {
    auto out = std::string{"["};
    auto order = file_explorer_demo::explorer_focus_order(profile);
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (i > 0)
            out += ",";
        out += json_string(file_explorer_demo::focus_target_value_name(order[i]));
    }
    out += "]";
    return out;
}

export auto explorer_input_model_json(
        file_explorer_demo::ExplorerInputModality modality,
        file_explorer_demo::ExplorerFocusTarget focus_target,
        bool focus_visible,
        std::string_view profile) -> std::string {
    auto state = file_explorer_demo::ExplorerState{};
    state.last_input_modality = modality;
    state.focus_target = focus_target;
    state.focus_visible = focus_visible;
    return std::format(
        "{{\"abstraction_policy\":{},\"focus_visibility_policy\":{},"
        "\"focus_ring_style\":{},\"last_input_modality\":{},"
        "\"focus_target\":{},\"focus_target_label\":{},"
        "\"focus_visible\":{},\"focus_visibility_reason\":{},"
        "\"focus_order\":{}}}",
        json_string("external_input_to_pure_explorer_input_to_renderer_output"),
        json_string("keyboard_tab_navigation_shows_ring_pointer_click_hides_ring"),
        json_string(file_explorer_demo::k_focus_ring_style),
        json_string(file_explorer_demo::input_modality_value_name(modality)),
        json_string(file_explorer_demo::focus_target_value_name(focus_target)),
        json_string(file_explorer_demo::focus_target_label(focus_target)),
        focus_visible ? "true" : "false",
        json_string(file_explorer_demo::focus_ring_visibility_reason(state)),
        explorer_focus_order_json(profile));
}

export auto explorer_input_model_json(
        file_explorer_demo::ExplorerState const& state,
        std::string_view profile) -> std::string {
    return explorer_input_model_json(
        state.last_input_modality,
        state.focus_target,
        state.focus_visible,
        profile);
}

export auto explorer_input_model_json(
        file_explorer_demo::ExplorerInputTrace const& trace,
        std::string_view profile) -> std::string {
    return explorer_input_model_json(
        trace.last_input_modality,
        trace.focus_target,
        trace.focus_visible,
        profile);
}

export auto explorer_expectation_json(
        file_explorer_demo::ExplorerExpectationResult const& expectation)
    -> std::string {
    return std::format(
        "{{\"kind\":{},\"value\":{},\"expects_operation_ok\":{},"
        "\"operation_ok\":{},\"label\":{},\"ok\":{},\"actual\":{},"
        "\"detail\":{}}}",
        json_string(file_explorer_demo::explorer_expectation_kind_name(
            expectation.expectation.kind)),
        json_string(expectation.expectation.value),
        expectation.expectation.expects_operation_ok ? "true" : "false",
        expectation.expectation.operation_ok ? "true" : "false",
        json_string(file_explorer_demo::explorer_expectation_label(
            expectation.expectation)),
        expectation.ok ? "true" : "false",
        json_string(expectation.actual),
        json_string(expectation.detail));
}

export auto explorer_expectations_json(
        std::span<file_explorer_demo::ExplorerExpectationResult const>
            expectations) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < expectations.size(); ++i) {
        if (i > 0)
            out += ",";
        out += explorer_expectation_json(expectations[i]);
    }
    out += "]";
    return out;
}

}

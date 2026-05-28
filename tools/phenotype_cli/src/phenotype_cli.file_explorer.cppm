export module phenotype_cli.file_explorer;

import cppx.cli;
import cppx.terminal;
import file_explorer_shared;
import json;
import phenotype.icon_catalog;
import phenotype_cli.common;
import phenotype_cli.file_explorer_json;
import phenotype_cli.icons;
import phenotype_cli.icons_common;
import phenotype_cli.package;
import phenotype.resources;
import std;

namespace phenotype_cli::file_explorer {

namespace fs = std::filesystem;
namespace cli_package = phenotype_cli::package;
namespace icon_catalog = phenotype::icon_catalog;
using phenotype_cli::common::Check;
using phenotype_cli::common::all_ok;
using phenotype_cli::common::checks_json;
using phenotype_cli::common::json_string;
using phenotype_cli::common::path_string;
using phenotype_cli::common::print_error;
using phenotype_cli::common::read_text_file;
using phenotype_cli::common::resource_diagnostics_json;
using namespace phenotype_cli::icons;

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

export struct ExplorerDriveResources {
    std::string source = "builtin";
    fs::path package_root;
    std::string locale = "en";
    phenotype::ResourceCatalog catalog;
    std::vector<phenotype::ResourceDiagnostic> diagnostics;
    std::vector<Check> checks;
};

export auto explorer_chrome_json(
        file_explorer_demo::ExplorerChromeMetrics const& chrome) -> std::string {
    auto const thumbnail_system = std::format(
        "{{\"visual_policy\":{},\"asset_policy\":{},"
        "\"pdf_policy\":{},\"image_policy\":{},\"video_policy\":{},"
        "\"svg_policy\":{},\"svg_render_policy\":{},"
        "\"svg_preview_source_policy\":{},"
        "\"svg_external_resource_policy\":{},"
        "\"svg_document_cache_policy\":{},"
        "\"svg_document_cache_limit\":{},\"shadow_policy\":{},"
        "\"uses_external_previews\":{},"
        "\"pdf_page_width\":{},\"pdf_page_height\":{},"
        "\"pdf_page_radius\":{},\"pdf_fold_size\":{},"
        "\"media_preview_width\":{},\"media_preview_height\":{},"
        "\"media_preview_radius\":{},\"pdf_detail_line_count\":{},"
        "\"image_sample_block_count\":{},\"video_strip_count\":{}}}",
        json_string(chrome.thumbnail_visual_policy),
        json_string(chrome.thumbnail_asset_policy),
        json_string(chrome.thumbnail_pdf_policy),
        json_string(chrome.thumbnail_image_policy),
        json_string(chrome.thumbnail_video_policy),
        json_string(chrome.thumbnail_svg_policy),
        json_string(chrome.thumbnail_svg_render_policy),
        json_string(chrome.thumbnail_svg_preview_source_policy),
        json_string(chrome.thumbnail_svg_external_resource_policy),
        json_string(chrome.thumbnail_svg_document_cache_policy),
        chrome.thumbnail_svg_document_cache_limit,
        json_string(chrome.thumbnail_shadow_policy),
        chrome.thumbnail_uses_external_previews ? "true" : "false",
        chrome.thumbnail_pdf_page_width,
        chrome.thumbnail_pdf_page_height,
        chrome.thumbnail_pdf_page_radius,
        chrome.thumbnail_pdf_fold_size,
        chrome.thumbnail_media_preview_width,
        chrome.thumbnail_media_preview_height,
        chrome.thumbnail_media_preview_radius,
        chrome.thumbnail_pdf_detail_line_count,
        chrome.thumbnail_image_sample_block_count,
        chrome.thumbnail_video_strip_count);
    return std::format(
        "{{\"viewport\":{{\"w\":{},\"h\":{},\"scale\":{}}},"
        "\"integrated_titlebar_height\":{},\"sidebar_width\":{},"
        "\"sidebar_row_width\":{},\"sidebar_row_height\":{},"
        "\"sidebar_heading_height\":{},"
        "\"sidebar_selected_row_radius\":{},"
        "\"sidebar_selected_row_background_alpha\":{},"
        "\"sidebar_selected_row_hover_background_alpha\":{},"
        "\"sidebar_selection_policy\":{},"
        "\"toolbar_group_height\":{},"
        "\"toolbar_group_radius\":{},\"toolbar_icon_button_width\":{},"
        "\"toolbar_icon_button_height\":{},\"window_radius\":{},"
        "\"finder_density_policy\":{},"
        "\"icon_grid\":{{\"columns\":{},\"visible_rows\":{},"
        "\"visible_capacity\":{},\"column_width\":{},\"row_height\":{},"
        "\"top_inset\":{},\"column_pitch\":{},\"scroll_height\":{}}},"
        "\"thumbnail_system\":{},"
        "\"toolbar\":{{\"group_count\":{},\"separator_count\":{},"
        "\"icon_button_count\":{},\"overflow_action_button_count\":{},"
        "\"finder_segmented\":{},\"more_actions_open\":{},"
        "\"status_bar_visible\":{},\"finder_density_policy\":{}}},"
        "\"column_locations\":{{\"row_count\":{},\"row_height\":{},"
        "\"icon_size\":{}}},"
        "\"native_window\":{{\"integrated_titlebar\":{},"
        "\"native_window_controls\":{},\"duplicate_window_controls\":{},"
        "\"window_control_single_owner\":{},"
        "\"content_window_control_markers\":{},"
        "\"artifact_window_control_markers\":{},"
        "\"window_control_marker_mode\":{},"
        "\"native_window_control_owner\":{},"
        "\"window_control_duplication_guard\":{},"
        "\"native_window_control_count\":{},"
        "\"content_window_control_marker_count\":{},"
        "\"artifact_window_control_marker_count\":{},"
        "\"content_drawn_window_control_count\":{},"
        "\"artifact_drawn_window_control_count\":{},"
        "\"window_control_render_policy\":{},"
        "\"titlebar_control_reserve_policy\":{},"
        "\"native_window_control_integration_policy\":{},"
        "\"native_window_control_geometry_role\":{},"
        "\"native_window_control_palette_policy\":{},"
        "\"titlebar_drag_region_height\":{},"
        "\"leading_control_reserved_width\":{},"
        "\"trailing_control_reserved_width\":{}}},"
        "\"geometry\":{{\"policy\":{},\"window_content_inset\":{},"
        "\"window_gap\":{},\"toolbar_shell_x\":{},"
        "\"toolbar_shell_y\":{},\"toolbar_shell_width\":{},"
        "\"toolbar_shell_height\":{},\"toolbar_group_y\":{},"
        "\"toolbar_navigation_group_x\":{},\"toolbar_title_x\":{},"
        "\"toolbar_view_group_x\":{},\"toolbar_sort_group_x\":{},"
        "\"toolbar_action_group_x\":{},\"toolbar_search_group_x\":{},"
        "\"content_surface_x\":{},\"content_surface_y\":{},"
        "\"content_surface_width\":{},\"sidebar_surface_x\":{},"
        "\"sidebar_surface_y\":{},\"sidebar_first_row_y\":{},"
        "\"finder_density_policy\":{},\"icon_grid_top_inset\":{}}},"
        "\"icon_system\":{{\"module\":{},\"style\":{},"
        "\"family\":\"Material Symbols (new)\","
        "\"default_material_symbols_style\":{},"
        "\"available_material_symbols_styles\":{},"
        "\"source_format\":{},"
        "\"svg_subset_policy\":{},\"svg_supported_elements\":{},"
        "\"svg_supported_path_commands\":{},"
        "\"svg_supported_style_attributes\":{},"
        "\"svg_arc_policy\":{},"
        "\"stroke_geometry_policy\":{},\"stroke_cap_policy\":{},"
        "\"stroke_join_policy\":{},"
        "\"owned_assets\":{},"
        "\"audited_permissive_assets\":{},"
        "\"uses_apple_icon_assets\":{},"
        "\"uses_sf_symbols_assets\":{},\"round_stroke_contract\":{},"
        "\"total_symbol_count\":{},"
        "\"phenotype_owned_symbol_count\":{},"
        "\"permissive_source_symbol_count\":{},"
        "\"material_symbols_source_symbol_count\":{},"
        "\"material_symbols_unique_source_icon_count\":{},"
        "\"apple_asset_symbol_count\":{},"
        "\"platform_extracted_symbol_count\":{},"
        "\"runtime_fetched_symbol_count\":{},"
        "\"audited_symbol_source_count\":{},"
        "\"sidebar_symbol_count\":{},"
        "\"toolbar_symbol_count\":{},\"file_type_symbol_count\":{},"
        "\"filled_symbol_count\":{},"
        "\"outline_symbol_count\":{},\"hierarchical_symbol_count\":{},"
        "\"monochrome_symbol_count\":{},"
        "\"regular_weight_symbol_count\":{},"
        "\"palette_symbol_count\":{},\"multicolor_symbol_count\":{},"
        "\"reference_symbol_count\":{},\"reference_source_count\":{},"
        "\"reference_sources\":{},"
        "\"svg_path_arc_symbol_count\":{},"
        "\"round_stroke_symbol_count\":{},"
        "\"interaction_phase_count\":{},"
        "\"grid_size\":{},"
        "\"default_stroke_width\":{},\"secondary_opacity\":{},"
        "\"toolbar_point_size\":{},\"sidebar_point_size\":{},"
        "\"sidebar_optical_y_offset\":{},"
        "\"toolbar_hit_target_size\":{},"
        "\"sidebar_hit_target_size\":{},"
        "\"action_hit_target_size\":{},"
        "\"toolbar_activation_hit_target_size\":{},"
        "\"sidebar_activation_hit_target_size\":{},"
        "\"action_activation_hit_target_size\":{},"
        "\"toolbar_button_radius\":{},"
        "\"toolbar_button_background_alpha\":{},"
        "\"toolbar_button_hover_background_alpha\":{},"
        "\"toolbar_selected_button_background_alpha\":{},"
        "\"toolbar_selected_button_hover_background_alpha\":{},"
        "\"toolbar_pressed_button_background_alpha\":{},"
        "\"sidebar_selected_pressed_background_alpha\":{},"
        "\"pressed_symbol_opacity\":{},\"pressed_scale\":{},"
        "\"column_location_icon_size\":{},"
        "\"text_weight_aligned\":{},"
        "\"monochrome_rendering\":{},"
        "\"hierarchical_opacity\":{},"
        "\"palette_rendering\":{},\"multicolor_rendering\":{},"
        "\"design_reference\":{},"
        "\"reference_family\":{},\"reference_policy\":{},"
        "\"asset_policy\":{},"
        "\"source_license_policy\":{},"
        "\"preferred_external_source_policy\":{},"
        "\"source_acquisition_policy\":{},"
        "\"source_attribution_policy\":{},"
        "\"apple_asset_boundary\":{},"
        "\"interface_metaphor_policy\":{},"
        "\"visual_consistency_policy\":{},"
        "\"alignment\":{},\"rendering_mode\":{},"
        "\"default_weight\":{},"
        "\"rendering_capability_policy\":{},"
        "\"variant_policy\":{},\"presentation_policy\":{},"
        "\"tone_policy\":{},\"interaction_tone_policy\":{},"
        "\"symbol_control_chrome_policy\":{},"
        "\"symbol_interaction_phase_policy\":{},"
        "\"toolbar_symbol_chrome_policy\":{},"
        "\"sidebar_symbol_color_policy\":{},"
        "\"interaction_tones\":{},"
        "\"file_type_color_policy\":{},"
        "\"metrics_policy\":{},\"hit_target_policy\":{},"
        "\"scale\":{},"
        "\"sidebar_reference_symbols\":{},"
        "\"sidebar_symbol_tokens\":{},"
        "\"sidebar_symbol_presentations\":{},"
        "\"toolbar_reference_symbols\":{},"
        "\"toolbar_symbol_presentations\":{},"
        "\"file_type_symbol_tokens\":{},"
        "\"file_type_reference_symbols\":{},"
        "\"file_type_symbol_presentations\":{},"
        "\"presentation_samples\":{}}}}}",
        chrome.viewport.width,
        chrome.viewport.height,
        chrome.viewport.scale,
        chrome.integrated_titlebar_height,
        chrome.sidebar_width,
        chrome.sidebar_row_width,
        chrome.sidebar_row_height,
        chrome.sidebar_heading_height,
        chrome.sidebar_selected_row_radius,
        chrome.sidebar_selected_row_background_alpha,
        chrome.sidebar_selected_row_hover_background_alpha,
        json_string(chrome.sidebar_selection_policy),
        chrome.toolbar_group_height,
        chrome.toolbar_group_radius,
        chrome.toolbar_icon_button_width,
        chrome.toolbar_icon_button_height,
        chrome.window_radius,
        json_string(chrome.finder_density_policy),
        chrome.icon_grid_columns,
        chrome.icon_grid_visible_rows,
        chrome.icon_grid_visible_capacity,
        chrome.icon_grid_column_width,
        chrome.icon_grid_row_height,
        chrome.icon_grid_top_inset,
        chrome.icon_grid_column_pitch,
        chrome.icon_grid_scroll_height,
        thumbnail_system,
        chrome.toolbar_group_count,
        chrome.toolbar_separator_count,
        chrome.toolbar_icon_button_count,
        chrome.overflow_action_button_count,
        chrome.finder_segmented_toolbar ? "true" : "false",
        chrome.more_actions_open ? "true" : "false",
        chrome.status_bar_visible ? "true" : "false",
        json_string(chrome.finder_density_policy),
        chrome.column_location_row_count,
        chrome.column_location_row_height,
        chrome.column_location_icon_size,
        chrome.integrated_titlebar ? "true" : "false",
        chrome.native_window_controls ? "true" : "false",
        chrome.duplicate_window_controls ? "true" : "false",
        chrome.window_control_single_owner ? "true" : "false",
        chrome.content_window_control_markers ? "true" : "false",
        chrome.artifact_window_control_markers ? "true" : "false",
        json_string(chrome.window_control_marker_mode),
        json_string(chrome.native_window_control_owner),
        json_string(chrome.window_control_duplication_guard),
        chrome.native_window_control_count,
        chrome.content_window_control_marker_count,
        chrome.artifact_window_control_marker_count,
        chrome.content_drawn_window_control_count,
        chrome.artifact_drawn_window_control_count,
        json_string(chrome.window_control_render_policy),
        json_string(chrome.titlebar_control_reserve_policy),
        json_string(chrome.native_window_control_integration_policy),
        json_string(chrome.native_window_control_geometry_role),
        json_string(chrome.native_window_control_palette_policy),
        chrome.titlebar_drag_region_height,
        chrome.leading_control_reserved_width,
        chrome.trailing_control_reserved_width,
        json_string(chrome.chrome_geometry_policy),
        chrome.window_content_inset,
        chrome.window_gap,
        chrome.toolbar_shell_x,
        chrome.toolbar_shell_y,
        chrome.toolbar_shell_width,
        chrome.toolbar_shell_height,
        chrome.toolbar_group_y,
        chrome.toolbar_navigation_group_x,
        chrome.toolbar_title_x,
        chrome.toolbar_view_group_x,
        chrome.toolbar_sort_group_x,
        chrome.toolbar_action_group_x,
        chrome.toolbar_search_group_x,
        chrome.content_surface_x,
        chrome.content_surface_y,
        chrome.content_surface_width,
        chrome.sidebar_surface_x,
        chrome.sidebar_surface_y,
        chrome.sidebar_first_row_y,
        json_string(chrome.finder_density_policy),
        chrome.icon_grid_top_inset,
        json_string(chrome.icon_module),
        json_string(chrome.icon_style),
        json_string(icon_catalog::material_symbols_style_name(
            icon_catalog::default_material_symbols_style())),
        icon_material_symbols_styles_json(),
        json_string(chrome.icon_source_format),
        json_string(chrome.icon_svg_subset_policy),
        json_string(chrome.icon_svg_supported_elements),
        json_string(chrome.icon_svg_supported_path_commands),
        json_string(chrome.icon_svg_supported_style_attributes),
        json_string(chrome.icon_svg_arc_policy),
        json_string(chrome.icon_stroke_geometry_policy),
        json_string(chrome.icon_stroke_cap_policy),
        json_string(chrome.icon_stroke_join_policy),
        chrome.owned_icon_assets ? "true" : "false",
        chrome.audited_permissive_icon_assets ? "true" : "false",
        chrome.uses_apple_icon_assets ? "true" : "false",
        chrome.uses_sf_symbols_assets ? "true" : "false",
        chrome.icon_round_stroke_contract ? "true" : "false",
        chrome.icon_total_symbol_count,
        chrome.icon_phenotype_owned_symbol_count,
        chrome.icon_permissive_source_symbol_count,
        chrome.icon_material_symbols_source_symbol_count,
        chrome.icon_material_symbols_unique_source_icon_count,
        chrome.icon_apple_asset_symbol_count,
        chrome.icon_platform_extracted_symbol_count,
        chrome.icon_runtime_fetched_symbol_count,
        chrome.icon_audited_symbol_source_count,
        chrome.sidebar_symbol_count,
        chrome.toolbar_symbol_count,
        chrome.file_type_symbol_count,
        chrome.icon_filled_symbol_count,
        chrome.icon_outline_symbol_count,
        chrome.icon_hierarchical_symbol_count,
        chrome.icon_monochrome_symbol_count,
        chrome.icon_regular_weight_symbol_count,
        chrome.icon_palette_symbol_count,
        chrome.icon_multicolor_symbol_count,
        chrome.icon_reference_symbol_count,
        icon_catalog::reference_source_count,
        icon_reference_sources_json(),
        chrome.icon_svg_path_arc_symbol_count,
        chrome.icon_round_stroke_symbol_count,
        chrome.icon_interaction_phase_count,
        chrome.icon_grid_size,
        chrome.icon_default_stroke_width,
        chrome.icon_secondary_opacity,
        chrome.icon_toolbar_point_size,
        chrome.icon_sidebar_point_size,
        chrome.icon_sidebar_optical_y_offset,
        chrome.icon_toolbar_hit_target_size,
        chrome.icon_sidebar_hit_target_size,
        chrome.icon_action_hit_target_size,
        chrome.icon_toolbar_activation_hit_target_size,
        chrome.icon_sidebar_activation_hit_target_size,
        chrome.icon_action_activation_hit_target_size,
        chrome.icon_toolbar_button_radius,
        chrome.icon_toolbar_button_background_alpha,
        chrome.icon_toolbar_button_hover_background_alpha,
        chrome.icon_toolbar_selected_button_background_alpha,
        chrome.icon_toolbar_selected_button_hover_background_alpha,
        chrome.icon_toolbar_pressed_button_background_alpha,
        chrome.icon_sidebar_selected_pressed_background_alpha,
        chrome.icon_pressed_symbol_opacity,
        chrome.icon_pressed_scale,
        chrome.column_location_icon_size,
        chrome.icon_text_weight_aligned ? "true" : "false",
        chrome.icon_monochrome_rendering ? "true" : "false",
        chrome.icon_hierarchical_opacity ? "true" : "false",
        chrome.icon_palette_rendering ? "true" : "false",
        chrome.icon_multicolor_rendering ? "true" : "false",
        json_string(chrome.icon_design_reference),
        json_string(chrome.icon_reference_family),
        json_string(chrome.icon_reference_policy),
        json_string(chrome.icon_asset_policy),
        json_string(chrome.icon_source_license_policy),
        json_string(chrome.icon_preferred_external_source_policy),
        json_string(chrome.icon_source_acquisition_policy),
        json_string(chrome.icon_source_attribution_policy),
        json_string(chrome.icon_apple_asset_boundary),
        json_string(chrome.icon_interface_metaphor_policy),
        json_string(chrome.icon_visual_consistency_policy),
        json_string(chrome.icon_alignment),
        json_string(chrome.icon_rendering_mode),
        json_string(chrome.icon_default_weight),
        json_string(chrome.icon_rendering_capability_policy),
        json_string(chrome.icon_variant_policy),
        json_string(chrome.icon_presentation_policy),
        json_string(chrome.icon_tone_policy),
        json_string(chrome.icon_interaction_tone_policy),
        json_string(chrome.icon_symbol_control_chrome_policy),
        json_string(chrome.icon_symbol_interaction_phase_policy),
        json_string(chrome.icon_toolbar_symbol_chrome_policy),
        json_string(chrome.icon_sidebar_symbol_color_policy),
        icon_interaction_tones_json(
            chrome.icon_interaction_tone_policy != "n/a"),
        json_string(chrome.icon_file_type_color_policy),
        json_string(chrome.icon_metrics_policy),
        json_string(chrome.icon_hit_target_policy),
        json_string(chrome.icon_scale),
        icon_reference_names_json(
            IconCatalogSet::Sidebar,
            chrome.icon_reference_symbol_count > 0),
        sidebar_symbol_tokens_json(chrome.icon_reference_symbol_count > 0),
        json::emit(file_explorer_demo::sidebar_symbol_presentations_debug_json(
            chrome)),
        icon_reference_names_json(
            IconCatalogSet::Toolbar,
            chrome.icon_reference_symbol_count > 0),
        json::emit(file_explorer_demo::toolbar_symbol_presentations_debug_json(
            chrome)),
        json::emit(file_explorer_demo::file_type_symbol_tokens_debug_json(
            chrome)),
        json::emit(
            file_explorer_demo::file_type_icon_reference_symbols_debug_json(
                chrome)),
        json::emit(file_explorer_demo::file_type_symbol_presentations_debug_json(
            chrome)),
        json::emit(file_explorer_demo::icon_presentation_samples_debug_json(
            chrome)));
}

export auto explorer_trace_json(
        file_explorer_demo::ExplorerInputTrace const& trace,
        std::string_view profile,
        std::size_t index) -> std::string {
    return std::format(
        "{{\"index\":{},\"input\":{},\"chrome\":{},\"status\":{},"
        "\"relative_location\":{},\"selected_name\":{},"
        "\"visible_entries\":{},\"has_selection\":{},"
        "\"input_model\":{},\"operation_label\":{},\"operation\":{}}}",
        index,
        explorer_input_json(trace.input),
        explorer_chrome_json(trace.chrome),
        json_string(trace.status),
        json_string(trace.relative_location),
        json_string(trace.selected_name),
        trace.visible_entries,
        trace.has_selection ? "true" : "false",
        explorer_input_model_json(trace, profile),
        json_string(trace.operation_label),
        operation_receipt_json(trace.operation));
}

export auto explorer_trace_array_json(
        std::span<file_explorer_demo::ExplorerInputTrace const> trace,
        std::string_view profile)
    -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < trace.size(); ++i) {
        if (i > 0)
            out += ",";
        out += explorer_trace_json(trace[i], profile, i);
    }
    out += "]";
    return out;
}

export auto explorer_drive_ok(file_explorer_demo::ExplorerDriveResult const& result)
    -> bool {
    return std::ranges::all_of(result.trace, [](auto const& trace) {
        return trace.operation.kind.empty() || trace.operation.ok;
    });
}

export auto explorer_resources_json(ExplorerDriveResources const& resources)
    -> std::string {
    return std::format(
        "{{\"source\":{},\"package_root\":{},\"locale\":{},"
        "\"application_id\":{},\"display_name\":{},\"entry\":{},"
        "\"default_locale\":{},\"default_font_family\":{},"
        "\"diagnostics\":{},\"checks\":{}}}",
        json_string(resources.source),
        json_string(path_string(resources.package_root)),
        json_string(resources.locale),
        json_string(resources.catalog.application.id),
        json_string(resources.catalog.application.display_name),
        json_string(resources.catalog.application.entry),
        json_string(resources.catalog.default_locale),
        json_string(resources.catalog.default_font_family),
        resource_diagnostics_json(resources.diagnostics),
        checks_json(resources.checks));
}

export auto explorer_contract_ok(
        file_explorer_demo::ExplorerDriveResult const& result,
        std::span<file_explorer_demo::ExplorerExpectationResult const>
            expectations) -> bool {
    auto const expectations_ok =
        file_explorer_demo::explorer_expectations_ok(expectations);
    if (!expectations.empty())
        return expectations_ok;
    return explorer_drive_ok(result);
}

export auto explorer_drive_json(
        file_explorer_demo::ExplorerDriveResult const& result,
        ExplorerDriveResources const& resources,
        file_explorer_demo::ExplorerLabels const& labels,
        std::span<file_explorer_demo::ExplorerExpectationResult const>
            expectations)
    -> std::string {
    auto const& snap = result.snapshot;
    return std::format(
        "{{\"schema_version\":1,\"command\":\"drive file-explorer\","
        "\"ok\":{},\"profile\":{},\"input_count\":{},"
        "\"resources\":{},\"labels\":{},"
        "\"root\":{},\"current\":{},\"relative_location\":{},"
        "\"status\":{},\"sort_label\":{},"
        "\"view_mode\":{{\"value\":{},\"label\":{}}},"
        "\"viewport\":{{\"w\":{},\"h\":{},\"scale\":{}}},"
        "\"chrome\":{},"
        "\"finder_visual_contract\":{},"
        "\"theme_system\":{},"
        "\"input_model\":{},"
        "\"keyboard_commands\":{},"
        "\"selected\":{{\"present\":{},\"name\":{},\"kind\":{},"
        "\"index\":{},\"size\":{},\"path_label\":{},"
        "\"preview_excerpt\":{}}},"
        "\"counts\":{{\"visible_entries\":{},\"files\":{},\"folders\":{}}},"
        "\"capabilities\":{{\"can_go_back\":{},\"can_go_forward\":{},"
        "\"can_create_file\":{},\"can_create_folder\":{},"
        "\"can_delete_selected\":{},\"can_duplicate_selected\":{},"
        "\"can_preview_selected\":{}}},"
        "\"operation\":{},\"entry_symbol_summary\":{},\"entries\":{},\"trace\":{},"
        "\"expectations\":{}}}",
        explorer_contract_ok(result, expectations) ? "true" : "false",
        json_string(result.profile),
        result.trace.size(),
        explorer_resources_json(resources),
        explorer_labels_json(labels),
        json_string(path_string(result.state.root)),
        json_string(path_string(result.state.current)),
        json_string(snap.relative_location),
        json_string(result.state.status),
        json_string(snap.sort_label),
        json_string(file_explorer_demo::view_mode_value_name(snap.view_mode)),
        json_string(file_explorer_demo::view_mode_label(snap.view_mode)),
        result.state.viewport_width,
        result.state.viewport_height,
        result.state.viewport_scale,
        explorer_chrome_json(result.chrome),
        json::emit(file_explorer_demo::finder_visual_contract_debug_json(
            result.chrome,
            result.profile)),
        explorer_theme_system_json(result.chrome),
        explorer_input_model_json(result.state, result.profile),
        explorer_keyboard_commands_json(result.profile),
        snap.has_selection ? "true" : "false",
        json_string(snap.has_selection ? snap.selected.name : ""),
        json_string(snap.selected_kind_label),
        snap.has_selection
            ? static_cast<std::int64_t>(snap.selected_index)
            : -1,
        json_string(snap.selected_size_label),
        json_string(snap.selected_path_label),
        json_string(output_tail(snap.preview, 512)),
        snap.entries.size(),
        snap.file_count,
        snap.folder_count,
        snap.can_go_back ? "true" : "false",
        snap.can_go_forward ? "true" : "false",
        snap.can_create_file ? "true" : "false",
        snap.can_create_folder ? "true" : "false",
        snap.can_delete_selected ? "true" : "false",
        snap.can_duplicate_selected ? "true" : "false",
        snap.can_preview_selected ? "true" : "false",
        operation_receipt_json(result.state.last_operation),
        json::emit(file_explorer_demo::entry_symbol_summary_debug_json(snap)),
        explorer_entries_json(snap.entries),
        explorer_trace_array_json(result.trace, result.profile),
        explorer_expectations_json(expectations));
}

export auto parse_explorer_input_script(fs::path const& path)
    -> std::expected<std::vector<file_explorer_demo::ExplorerInput>, std::string> {
    auto ec = std::error_code{};
    if (!fs::exists(path, ec)) {
        return std::unexpected{
            std::format("input script does not exist: {}", path_string(path))};
    }
    auto text = read_text_file(path);
    auto parsed = file_explorer_demo::parse_explorer_input_lines(
        text,
        path_string(path));
    if (!parsed.ok)
        return std::unexpected{parsed.error};
    return std::move(parsed.inputs);
}

export auto join_explorer_input_lines(std::span<std::string const> inputs)
    -> std::string {
    auto out = std::string{};
    for (auto const& input : inputs) {
        if (!out.empty())
            out.push_back('\n');
        out += input;
    }
    return out;
}

export auto explorer_drive_resources(
        std::string_view profile,
        cppx::cli::Invocation const& invocation)
    -> std::expected<ExplorerDriveResources, std::string> {
    auto resources = ExplorerDriveResources{};
    resources.catalog =
        file_explorer_demo::file_explorer_resource_catalog(profile);

    if (auto package_root = invocation.value("package")) {
        auto package =
            cli_package::package_summary(fs::path{std::string{*package_root}});
        auto checks = cli_package::package_checks(package);
        if (!all_ok(checks)) {
            return std::unexpected{std::format(
                "package resources failed checks: {}",
                path_string(package.root))};
        }
        resources.source = "package";
        resources.package_root = package.root;
        resources.catalog = std::move(package.catalog);
        resources.diagnostics = std::move(package.catalog_diagnostics);
        resources.checks = std::move(checks);
    }

    resources.locale = resources.catalog.default_locale.empty()
        ? "en"
        : resources.catalog.default_locale;
    if (auto locale = invocation.value("locale"))
        resources.locale = std::string{*locale};
    return resources;
}

export auto parse_explorer_expectations(
        cppx::cli::Invocation const& invocation)
    -> std::expected<std::vector<file_explorer_demo::ExplorerExpectation>,
                     std::string> {
    auto expectations = std::vector<file_explorer_demo::ExplorerExpectation>{};
    for (auto const& raw : invocation.values("expect")) {
        auto parsed = file_explorer_demo::parse_explorer_expectation(raw);
        if (!parsed.ok)
            return std::unexpected{parsed.error};
        expectations.push_back(std::move(parsed.expectation));
    }
    return expectations;
}

export int run_drive_file_explorer(cppx::cli::Invocation const& invocation) {
    auto profile = std::string{"desktop"};
    if (auto value = invocation.value("profile"))
        profile = std::string{*value};
    auto normalized_profile = file_explorer_demo::lower_copy(profile);
    if (normalized_profile != "desktop" && normalized_profile != "mobile") {
        return print_error(
            "drive file-explorer",
            "profile must be 'desktop' or 'mobile'",
            invocation.has("json"));
    }

    auto inputs = std::vector<file_explorer_demo::ExplorerInput>{};
    if (auto scenario = invocation.value("scenario")) {
        inputs.push_back({
            .kind = file_explorer_demo::ExplorerInputKind::Scenario,
            .value = std::string{*scenario},
        });
    }
    for (auto const& script : invocation.values("script")) {
        auto parsed = parse_explorer_input_script(fs::path{script});
        if (!parsed) {
            return print_error(
                "drive file-explorer",
                parsed.error(),
                invocation.has("json"));
        }
        inputs.insert(inputs.end(),
                      std::make_move_iterator(parsed->begin()),
                      std::make_move_iterator(parsed->end()));
    }
    for (auto const& raw : invocation.values("input")) {
        auto parsed = file_explorer_demo::parse_explorer_input(raw);
        if (!parsed.ok) {
            return print_error(
                "drive file-explorer",
                parsed.error,
                invocation.has("json"));
        }
        inputs.push_back(std::move(parsed.input));
    }

    auto resources = explorer_drive_resources(normalized_profile, invocation);
    if (!resources) {
        return print_error(
            "drive file-explorer",
            resources.error(),
            invocation.has("json"));
    }
    auto labels = file_explorer_demo::file_explorer_labels(
        resources->locale,
        resources->catalog);
    auto expectations = parse_explorer_expectations(invocation);
    if (!expectations) {
        return print_error(
            "drive file-explorer",
            expectations.error(),
            invocation.has("json"));
    }

    auto result = file_explorer_demo::drive_explorer(
        normalized_profile,
        inputs);
    auto checked_expectations =
        file_explorer_demo::check_explorer_expectations(
            result,
            *expectations);
    if (invocation.has("json")) {
        std::println("{}",
                     explorer_drive_json(
                         result,
                         *resources,
                         labels,
                         checked_expectations));
    } else {
        auto const& snap = result.snapshot;
        auto lines = std::vector<cppx::terminal::StatusLine>{
            {.label = "profile",
             .value = result.profile,
             .status = cppx::terminal::StatusKind::ok},
            {.label = "locale",
             .value = resources->locale,
             .status = cppx::terminal::StatusKind::ok},
            {.label = "resources",
             .value = resources->source == "package"
                ? path_string(resources->package_root)
                : "builtin",
             .status = cppx::terminal::StatusKind::ok},
            {.label = "location",
             .value = snap.relative_location,
             .status = cppx::terminal::StatusKind::ok},
            {.label = "selected",
             .value = snap.has_selection ? snap.selected.name : "<none>",
             .status = snap.has_selection ? cppx::terminal::StatusKind::ok
                                          : cppx::terminal::StatusKind::skip},
            {.label = "entries",
             .value = std::format("{} files, {} folders",
                                  snap.file_count,
                                  snap.folder_count),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "view",
             .value = file_explorer_demo::view_mode_label(snap.view_mode),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "focus",
             .value = std::format(
                 "{} visible={} modality={}",
                 file_explorer_demo::focus_target_value_name(
                     result.state.focus_target),
                 result.state.focus_visible ? "true" : "false",
                 file_explorer_demo::input_modality_value_name(
                     result.state.last_input_modality)),
             .status = result.state.focus_visible
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::skip},
            {.label = "chrome",
             .value = std::format("{} cols x {} rows, viewport {}x{}@{}",
                                  result.chrome.icon_grid_columns,
                                  result.chrome.icon_grid_visible_rows,
                                  result.chrome.viewport.width,
                                  result.chrome.viewport.height,
                                  result.chrome.viewport.scale),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "status",
             .value = result.state.status,
             .status = explorer_drive_ok(result)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
        };
        if (!result.state.last_operation.kind.empty()) {
            lines.push_back({
                .label = "operation",
                .value = file_explorer_demo::operation_label(
                    result.state.last_operation),
                .status = result.state.last_operation.ok
                    ? cppx::terminal::StatusKind::ok
                    : cppx::terminal::StatusKind::fail,
            });
        }
        std::println("phenotype drive file-explorer");
        std::println("{}", cppx::terminal::format_status_frame(lines, false));
        if (!checked_expectations.empty()) {
            std::println("expectations:");
            for (auto const& expectation : checked_expectations) {
                std::println("  - {} -> {} ({})",
                             file_explorer_demo::explorer_expectation_label(
                                 expectation.expectation),
                             expectation.ok ? "ok" : "failed",
                             expectation.actual);
            }
        }
        if (!result.trace.empty()) {
            std::println("inputs:");
            for (auto const& trace : result.trace) {
                std::println("  - {} -> {}",
                             file_explorer_demo::explorer_input_label(
                                 trace.input),
                             trace.status);
            }
        }
    }
    return explorer_contract_ok(result, checked_expectations) ? 0 : 1;
}



}

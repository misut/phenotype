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

export module file_explorer_shared:chrome_and_geometry;

import json;
import phenotype.icon_catalog;
import phenotype.resources;
import phenotype.theme_contract;
import :model_types;
import :desktop_metrics_and_symbols;
import :viewport_and_focus_helpers;

export namespace file_explorer_demo {
phenotype::ResourceCatalog file_explorer_resource_catalog(
        std::string_view profile);

inline ExplorerChromeMetrics explorer_chrome_metrics(
        ExplorerState const& state,
        std::string_view profile) {
    auto viewport = effective_viewport(state, profile);
    if (mobile_profile(profile)) {
        auto const toolbar_chrome = icon_catalog::macos_control_chrome(
            icon_catalog::SymbolPresentationRole::Toolbar,
            icon_catalog::SymbolInteractionState{false, true});
        auto const toolbar_selected_chrome = icon_catalog::macos_control_chrome(
            icon_catalog::SymbolPresentationRole::Toolbar,
            icon_catalog::SymbolInteractionState{true, true});
        auto const toolbar_pressed_recipe = icon_catalog::macos_state_recipe(
            icon_catalog::SymbolPresentationRole::Toolbar,
            icon_catalog::SymbolInteractionState{false, true},
            icon_catalog::SymbolInteractionPhase::Pressed);
        auto const sidebar_selected_pressed_recipe =
            icon_catalog::macos_state_recipe(
                icon_catalog::SymbolPresentationRole::Sidebar,
                icon_catalog::SymbolInteractionState{true, true},
                icon_catalog::SymbolInteractionPhase::Pressed);
        return ExplorerChromeMetrics{
            .viewport = viewport,
            .integrated_titlebar_height = 0.0f,
            .window_content_inset = 0.0f,
            .window_gap = 0.0f,
            .toolbar_shell_x = 0.0f,
            .toolbar_shell_y = 0.0f,
            .toolbar_shell_width = 0.0f,
            .toolbar_shell_height = 0.0f,
            .toolbar_group_y = 0.0f,
            .toolbar_navigation_group_x = 0.0f,
            .toolbar_title_x = 0.0f,
            .toolbar_view_group_x = 0.0f,
            .toolbar_sort_group_x = 0.0f,
            .toolbar_action_group_x = 0.0f,
            .toolbar_search_group_x = 0.0f,
            .content_surface_x = 0.0f,
            .content_surface_y = 0.0f,
            .content_surface_width = 0.0f,
            .sidebar_surface_x = 0.0f,
            .sidebar_surface_y = 0.0f,
            .sidebar_first_row_y = 0.0f,
            .sidebar_width = 0.0f,
            .sidebar_row_width = 0.0f,
            .sidebar_row_height = 0.0f,
            .sidebar_heading_height = 0.0f,
            .sidebar_icon_size = 0.0f,
            .sidebar_icon_leading = 0.0f,
            .sidebar_label_leading = 0.0f,
            .sidebar_label_top = 0.0f,
            .sidebar_heading_label_leading = 0.0f,
            .sidebar_heading_label_top = 0.0f,
            .sidebar_section_gap = 0.0f,
            .sidebar_selected_row_radius = 0.0f,
            .sidebar_selected_row_background_alpha = 0,
            .sidebar_selected_row_hover_background_alpha = 0,
            .sidebar_selection_policy = "n/a",
            .toolbar_group_height = 0.0f,
            .toolbar_group_radius = 0.0f,
            .toolbar_icon_button_width = 0.0f,
            .toolbar_icon_button_height = 0.0f,
            .titlebar_control_cluster_height = 0.0f,
            .titlebar_control_diameter = 0.0f,
            .titlebar_control_spacing = 0.0f,
            .titlebar_control_start_x = 0.0f,
            .titlebar_control_top = 0.0f,
            .titlebar_drag_region_height = 0.0f,
            .leading_control_reserved_width = 0.0f,
            .trailing_control_reserved_width = 0.0f,
            .window_radius = 0.0f,
            .icon_grid_column_width = 0.0f,
            .icon_grid_row_height = 0.0f,
            .icon_grid_column_pitch = 0.0f,
            .icon_grid_thumbnail_width = 0.0f,
            .icon_grid_thumbnail_height = 0.0f,
            .thumbnail_pdf_page_width = 0.0f,
            .thumbnail_pdf_page_height = 0.0f,
            .thumbnail_pdf_page_radius = 0.0f,
            .thumbnail_pdf_fold_size = 0.0f,
            .thumbnail_media_preview_width = 0.0f,
            .thumbnail_media_preview_height = 0.0f,
            .thumbnail_media_preview_radius = 0.0f,
            .thumbnail_pdf_detail_line_count = 0,
            .thumbnail_image_sample_block_count = 0,
            .thumbnail_video_strip_count = 0,
            .icon_grid_label_height = 0.0f,
            .icon_grid_label_font_size = 0.0f,
            .icon_grid_label_policy = "not_applicable_mobile_row_list",
            .icon_grid_top_inset = 0.0f,
            .icon_grid_gap = 0.0f,
            .icon_grid_scroll_height = 0.0f,
            .icon_grid_columns = 0,
            .icon_grid_visible_rows = 0,
            .icon_grid_visible_capacity = 0,
            .toolbar_group_count = 0,
            .toolbar_separator_count = 0,
            .toolbar_icon_button_count = 4,
            .column_location_row_count = 0,
            .titlebar_control_count = 0,
            .native_window_control_count = 0,
            .content_window_control_marker_count = 0,
            .artifact_window_control_marker_count = 0,
            .content_drawn_window_control_count = 0,
            .artifact_drawn_window_control_count = 0,
            .icon_total_symbol_count =
                static_cast<int>(icon_catalog::all_symbol_count),
            .icon_phenotype_owned_symbol_count =
                static_cast<int>(icon_catalog::phenotype_owned_symbol_count),
            .icon_permissive_source_symbol_count =
                static_cast<int>(icon_catalog::permissive_source_symbol_count),
            .icon_material_symbols_source_symbol_count =
                static_cast<int>(icon_catalog::material_symbols_source_symbol_count),
            .icon_material_symbols_unique_source_icon_count =
                static_cast<int>(
                    icon_catalog::material_symbols_unique_source_icon_count),
            .icon_apple_asset_symbol_count =
                static_cast<int>(icon_catalog::apple_asset_symbol_count),
            .icon_platform_extracted_symbol_count =
                static_cast<int>(
                    icon_catalog::platform_extracted_symbol_count),
            .icon_runtime_fetched_symbol_count =
                static_cast<int>(icon_catalog::runtime_fetched_symbol_count),
            .icon_audited_symbol_source_count =
                static_cast<int>(icon_catalog::audited_symbol_source_count),
            .sidebar_symbol_count =
                static_cast<int>(icon_catalog::sidebar_symbol_count),
            .toolbar_symbol_count =
                static_cast<int>(icon_catalog::toolbar_symbol_count),
            .file_type_symbol_count =
                static_cast<int>(icon_catalog::file_type_symbol_count),
            .icon_filled_symbol_count =
                static_cast<int>(icon_catalog::filled_symbol_count),
            .icon_outline_symbol_count =
                static_cast<int>(icon_catalog::outline_symbol_count),
            .icon_hierarchical_symbol_count =
                static_cast<int>(icon_catalog::hierarchical_symbol_count),
            .icon_monochrome_symbol_count =
                static_cast<int>(icon_catalog::monochrome_symbol_count),
            .icon_regular_weight_symbol_count =
                static_cast<int>(icon_catalog::regular_weight_symbol_count),
            .icon_palette_symbol_count =
                static_cast<int>(icon_catalog::palette_symbol_count),
            .icon_multicolor_symbol_count =
                static_cast<int>(icon_catalog::multicolor_symbol_count),
            .icon_reference_symbol_count =
                static_cast<int>(icon_catalog::reference_symbol_count),
            .icon_svg_path_arc_symbol_count =
                static_cast<int>(icon_catalog::svg_path_arc_symbol_count),
            .icon_round_stroke_symbol_count =
                static_cast<int>(icon_catalog::round_stroke_symbol_count),
            .icon_interaction_phase_count =
                static_cast<int>(icon_catalog::symbol_interaction_phase_count),
            .icon_grid_size =
                icon_catalog::descriptor(
                    icon_catalog::Symbol::Document).grid_size,
            .icon_default_stroke_width =
                icon_catalog::descriptor(
                    icon_catalog::Symbol::Document).default_stroke_width,
            .icon_secondary_opacity =
                icon_catalog::descriptor(
                    icon_catalog::Symbol::Document).secondary_opacity,
            .icon_toolbar_point_size =
                icon_catalog::metrics(
                    icon_catalog::SymbolPresentationRole::Toolbar).point_size,
            .icon_sidebar_point_size =
                icon_catalog::metrics(
                    icon_catalog::SymbolPresentationRole::Sidebar).point_size,
            .icon_sidebar_optical_y_offset =
                icon_catalog::metrics(
                    icon_catalog::SymbolPresentationRole::Sidebar).optical_y_offset,
            .icon_toolbar_hit_target_size =
                icon_catalog::hit_target_size(
                    icon_catalog::SymbolPresentationRole::Toolbar),
            .icon_sidebar_hit_target_size =
                icon_catalog::hit_target_size(
                    icon_catalog::SymbolPresentationRole::Sidebar),
            .icon_action_hit_target_size =
                icon_catalog::hit_target_size(
                    icon_catalog::SymbolPresentationRole::Action),
            .icon_toolbar_activation_hit_target_size =
                icon_catalog::activation_hit_target_size(
                    icon_catalog::SymbolPresentationRole::Toolbar),
            .icon_sidebar_activation_hit_target_size =
                icon_catalog::activation_hit_target_size(
                    icon_catalog::SymbolPresentationRole::Sidebar),
            .icon_action_activation_hit_target_size =
                icon_catalog::activation_hit_target_size(
                    icon_catalog::SymbolPresentationRole::Action),
            .icon_toolbar_button_radius = toolbar_chrome.corner_radius,
            .icon_toolbar_button_background_alpha =
                toolbar_chrome.background_color.a,
            .icon_toolbar_button_hover_background_alpha =
                toolbar_chrome.hover_background_color.a,
            .icon_toolbar_selected_button_background_alpha =
                toolbar_selected_chrome.background_color.a,
            .icon_toolbar_selected_button_hover_background_alpha =
                toolbar_selected_chrome.hover_background_color.a,
            .icon_toolbar_pressed_button_background_alpha =
                toolbar_pressed_recipe.background_color.a,
            .icon_sidebar_selected_pressed_background_alpha =
                sidebar_selected_pressed_recipe.background_color.a,
            .icon_pressed_symbol_opacity =
                toolbar_pressed_recipe.symbol_opacity,
            .icon_pressed_scale = toolbar_pressed_recipe.scale,
            .column_location_row_height = 0.0f,
            .column_location_icon_size = 0.0f,
            .integrated_titlebar = false,
            .native_window_controls = false,
            .duplicate_window_controls = false,
            .window_control_single_owner = true,
            .content_window_control_markers = false,
            .finder_segmented_toolbar = false,
            .owned_icon_assets =
                icon_catalog::phenotype_owned_symbol_count
                == icon_catalog::all_symbol_count,
            .audited_permissive_icon_assets =
                icon_catalog::audited_symbol_source_count
                == icon_catalog::all_symbol_count,
            .uses_apple_icon_assets =
                icon_catalog::apple_asset_symbol_count > 0,
            .uses_sf_symbols_assets = false,
            .icon_round_stroke_contract = true,
            .icon_text_weight_aligned = true,
            .icon_monochrome_rendering = true,
            .icon_hierarchical_opacity = false,
            .icon_palette_rendering = false,
            .icon_multicolor_rendering = false,
            .thumbnail_uses_external_previews = false,
            .artifact_window_control_markers = false,
            .status_bar_visible = false,
            .theme_contract_version =
                static_cast<int>(theme_contract::theme_contract_version),
            .icon_module = "phenotype.icons",
            .icon_style = std::string{icon_catalog::style_name()},
            .icon_source_format = std::string{icon_catalog::source_format()},
            .icon_svg_subset_policy =
                std::string{icon_catalog::svg_subset_policy()},
            .icon_svg_supported_elements =
                std::string{icon_catalog::svg_supported_elements()},
            .icon_svg_supported_path_commands =
                std::string{icon_catalog::svg_supported_path_commands()},
            .icon_svg_supported_style_attributes =
                std::string{icon_catalog::svg_supported_style_attributes()},
            .icon_svg_arc_policy = std::string{icon_catalog::svg_arc_policy()},
            .icon_stroke_geometry_policy =
                std::string{icon_catalog::stroke_geometry_policy()},
            .icon_stroke_cap_policy =
                std::string{icon_catalog::stroke_cap_policy()},
            .icon_stroke_join_policy =
                std::string{icon_catalog::stroke_join_policy()},
            .icon_design_reference =
                std::string{icon_catalog::style_reference()},
            .icon_reference_family =
                std::string{icon_catalog::reference_family()},
            .icon_reference_policy =
                std::string{icon_catalog::reference_policy()},
            .icon_asset_policy = std::string{icon_catalog::asset_policy()},
            .icon_source_license_policy =
                std::string{icon_catalog::source_license_policy()},
            .icon_preferred_external_source_policy =
                std::string{icon_catalog::preferred_external_source_policy()},
            .icon_source_acquisition_policy =
                std::string{icon_catalog::source_acquisition_policy()},
            .icon_document_cache_policy =
                std::string{icon_catalog::document_cache_policy()},
            .icon_source_attribution_policy =
                std::string{icon_catalog::source_attribution_policy()},
            .icon_apple_asset_boundary =
                std::string{icon_catalog::apple_asset_boundary()},
            .icon_interface_metaphor_policy =
                std::string{icon_catalog::interface_metaphor_policy()},
            .icon_visual_consistency_policy =
                std::string{icon_catalog::visual_consistency_policy()},
            .icon_alignment = std::string{icon_catalog::alignment_policy()},
            .icon_rendering_mode = "monochrome",
            .icon_default_weight =
                std::string{icon_catalog::default_weight_policy()},
            .icon_rendering_capability_policy =
                std::string{icon_catalog::rendering_capability_policy()},
            .icon_variant_policy = std::string{icon_catalog::variant_policy()},
            .icon_presentation_policy =
                std::string{icon_catalog::presentation_policy()},
            .icon_tone_policy = std::string{icon_catalog::tone_policy()},
            .icon_interaction_tone_policy =
                std::string{icon_catalog::interaction_tone_policy()},
            .icon_symbol_control_chrome_policy =
                std::string{icon_catalog::symbol_control_chrome_policy()},
            .icon_symbol_interaction_phase_policy =
                std::string{icon_catalog::symbol_interaction_phase_policy()},
            .icon_toolbar_symbol_chrome_policy =
                std::string{icon_catalog::toolbar_symbol_chrome_policy()},
            .icon_sidebar_symbol_color_policy =
                std::string{icon_catalog::sidebar_symbol_color_policy()},
            .icon_file_type_color_policy =
                std::string{icon_catalog::file_type_color_policy()},
            .icon_metrics_policy = std::string{icon_catalog::metrics_policy()},
            .icon_hit_target_policy =
                std::string{icon_catalog::hit_target_policy()},
            .icon_scale = std::string{icon_catalog::default_scale_policy()},
            .thumbnail_visual_policy = "n/a",
            .thumbnail_asset_policy = "n/a",
            .thumbnail_pdf_policy = "n/a",
            .thumbnail_image_policy = "n/a",
            .thumbnail_svg_policy = "n/a",
            .thumbnail_svg_render_policy = "n/a",
            .thumbnail_svg_preview_source_policy = "n/a",
            .thumbnail_svg_external_resource_policy = "n/a",
            .thumbnail_svg_document_cache_policy = "n/a",
            .thumbnail_svg_document_cache_limit = 0,
            .thumbnail_video_policy = "n/a",
            .thumbnail_shadow_policy = "n/a",
            .theme_profile_name =
                std::string{theme_contract::default_theme_profile_name()},
            .theme_reference =
                std::string{theme_contract::default_theme_reference()},
            .theme_font_policy =
                std::string{theme_contract::default_theme_font_policy()},
            .theme_material_policy =
                std::string{theme_contract::default_theme_material_policy()},
            .theme_iconography_policy =
                std::string{theme_contract::default_theme_iconography_policy()},
            .theme_icon_asset_policy =
                std::string{theme_contract::default_theme_icon_asset_policy()},
            .theme_usage_policy =
                std::string{theme_contract::default_theme_usage_policy()},
            .theme_container_policy =
                std::string{theme_contract::default_theme_container_policy()},
            .theme_performance_policy =
                std::string{theme_contract::default_theme_performance_policy()},
            .theme_accessibility_policy =
                std::string{theme_contract::default_theme_accessibility_policy()},
            .theme_fallback_policy =
                std::string{theme_contract::default_theme_fallback_policy()},
            .chrome_geometry_policy = "n/a",
            .finder_density_policy = "not_applicable_mobile_row_list",
            .window_control_marker_mode = "none",
            .native_window_control_owner = "none",
            .window_control_duplication_guard =
                "not_applicable_mobile_shell",
            .window_control_render_policy = "not_applicable_mobile_shell",
            .titlebar_control_reserve_policy =
                "not_applicable_mobile_shell",
            .native_window_control_integration_policy =
                "not_applicable_mobile_shell",
            .native_window_control_geometry_role =
                "not_applicable_mobile_shell",
            .native_window_control_palette_policy =
                "not_applicable_mobile_shell",
        };
    }
    auto columns = desktop_icon_grid_column_count(viewport);
    auto scroll_height = desktop_scroll_height_for_viewport(
        viewport,
        176.0f,
        528.0f,
        660.0f);
    int visible_rows = static_cast<int>(
        scroll_height / k_desktop_icon_grid_row_height);
    if (visible_rows < 1)
        visible_rows = 1;
    auto const toolbar_chrome = icon_catalog::macos_control_chrome(
        icon_catalog::SymbolPresentationRole::Toolbar,
        icon_catalog::SymbolInteractionState{false, true});
    auto const toolbar_selected_chrome = icon_catalog::macos_control_chrome(
        icon_catalog::SymbolPresentationRole::Toolbar,
        icon_catalog::SymbolInteractionState{true, true});
    auto const toolbar_pressed_recipe = icon_catalog::macos_state_recipe(
        icon_catalog::SymbolPresentationRole::Toolbar,
        icon_catalog::SymbolInteractionState{false, true},
        icon_catalog::SymbolInteractionPhase::Pressed);
    auto const sidebar_selected_pressed_recipe =
        icon_catalog::macos_state_recipe(
            icon_catalog::SymbolPresentationRole::Sidebar,
            icon_catalog::SymbolInteractionState{true, true},
            icon_catalog::SymbolInteractionPhase::Pressed);
    return ExplorerChromeMetrics{
        .viewport = viewport,
        .integrated_titlebar_height = k_desktop_integrated_titlebar_height,
        .window_content_inset = k_desktop_window_content_inset,
        .window_gap = k_desktop_window_gap,
        .toolbar_shell_x = desktop_toolbar_shell_x(),
        .toolbar_shell_y = k_desktop_window_content_inset,
        .toolbar_shell_width = desktop_toolbar_shell_width(viewport),
        .toolbar_shell_height = k_desktop_toolbar_shell_height,
        .toolbar_group_y = desktop_toolbar_group_y(),
        .toolbar_navigation_group_x = desktop_toolbar_navigation_group_x(),
        .toolbar_title_x = desktop_toolbar_title_x(),
        .toolbar_view_group_x = desktop_toolbar_view_group_x(viewport),
        .toolbar_sort_group_x = desktop_toolbar_sort_group_x(viewport),
        .toolbar_action_group_x = desktop_toolbar_action_group_x(viewport),
        .toolbar_search_group_x = desktop_toolbar_search_group_x(viewport),
        .content_surface_x = desktop_toolbar_shell_x(),
        .content_surface_y = desktop_content_surface_y(),
        .content_surface_width = desktop_toolbar_shell_width(viewport),
        .sidebar_surface_x = k_desktop_window_content_inset,
        .sidebar_surface_y = k_desktop_window_content_inset,
        .sidebar_first_row_y = desktop_sidebar_first_row_y(),
        .sidebar_width = k_desktop_sidebar_width,
        .sidebar_row_width = k_desktop_sidebar_row_width,
        .sidebar_row_height = k_desktop_sidebar_row_height,
        .sidebar_heading_height = k_desktop_sidebar_heading_height,
        .sidebar_icon_size = k_desktop_sidebar_icon_size,
        .sidebar_icon_leading = k_desktop_sidebar_icon_leading,
        .sidebar_label_leading = k_desktop_sidebar_label_leading,
        .sidebar_label_top = k_desktop_sidebar_label_top,
        .sidebar_heading_label_leading = k_desktop_sidebar_heading_label_leading,
        .sidebar_heading_label_top = k_desktop_sidebar_heading_label_top,
        .sidebar_section_gap = k_desktop_sidebar_section_gap,
        .sidebar_selected_row_radius = k_desktop_sidebar_selected_row_radius,
        .sidebar_selected_row_background_alpha =
            k_desktop_sidebar_selected_row_background_alpha,
        .sidebar_selected_row_hover_background_alpha =
            k_desktop_sidebar_selected_row_hover_background_alpha,
        .sidebar_selection_policy = k_desktop_sidebar_selection_policy,
        .toolbar_group_height = k_desktop_toolbar_group_height,
        .toolbar_group_radius = k_desktop_toolbar_group_radius,
        .toolbar_icon_button_width = k_desktop_toolbar_icon_button_width,
        .toolbar_icon_button_height = k_desktop_toolbar_icon_button_height,
        .titlebar_control_cluster_height = k_desktop_titlebar_control_cluster_height,
        .titlebar_control_diameter = k_desktop_titlebar_control_diameter,
        .titlebar_control_spacing = k_desktop_titlebar_control_spacing,
        .titlebar_control_start_x = k_desktop_titlebar_control_start_x,
        .titlebar_control_top = k_desktop_titlebar_control_top,
        .titlebar_drag_region_height = k_desktop_titlebar_drag_region_height,
        .leading_control_reserved_width =
            k_desktop_leading_control_reserved_width,
        .trailing_control_reserved_width =
            k_desktop_trailing_control_reserved_width,
        .window_radius = k_desktop_window_radius,
        .icon_grid_column_width = k_desktop_icon_grid_column_width,
        .icon_grid_row_height = k_desktop_icon_grid_row_height,
        .icon_grid_column_pitch = k_desktop_icon_grid_column_pitch,
        .icon_grid_thumbnail_width = k_desktop_icon_grid_thumbnail_width,
        .icon_grid_thumbnail_height = k_desktop_icon_grid_thumbnail_height,
        .thumbnail_pdf_page_width = k_desktop_thumbnail_pdf_page_width,
        .thumbnail_pdf_page_height = k_desktop_thumbnail_pdf_page_height,
        .thumbnail_pdf_page_radius = k_desktop_thumbnail_pdf_page_radius,
        .thumbnail_pdf_fold_size = k_desktop_thumbnail_pdf_fold_size,
        .thumbnail_media_preview_width = k_desktop_thumbnail_media_preview_width,
        .thumbnail_media_preview_height = k_desktop_thumbnail_media_preview_height,
        .thumbnail_media_preview_radius = k_desktop_thumbnail_media_preview_radius,
        .thumbnail_pdf_detail_line_count =
            k_desktop_thumbnail_pdf_detail_line_count,
        .thumbnail_image_sample_block_count =
            k_desktop_thumbnail_image_sample_block_count,
        .thumbnail_video_strip_count = k_desktop_thumbnail_video_strip_count,
        .icon_grid_label_height = k_desktop_icon_grid_label_height,
        .icon_grid_label_font_size = k_desktop_icon_grid_label_font_size,
        .icon_grid_label_policy = k_desktop_icon_grid_label_policy,
        .icon_grid_top_inset = k_desktop_icon_grid_top_inset,
        .icon_grid_gap = k_desktop_icon_grid_gap,
        .icon_grid_scroll_height = scroll_height,
        .icon_grid_columns = columns,
        .icon_grid_visible_rows = visible_rows,
        .icon_grid_visible_capacity = columns * visible_rows,
        .toolbar_group_count = 5,
        .toolbar_separator_count = 3,
        .toolbar_icon_button_count = 11,
        .column_location_row_count = k_desktop_column_location_row_count,
        .titlebar_control_count = k_desktop_titlebar_control_count,
        .native_window_control_count = k_desktop_titlebar_control_count,
        .content_window_control_marker_count = 0,
        .artifact_window_control_marker_count = 0,
        .content_drawn_window_control_count = 0,
        .artifact_drawn_window_control_count = 0,
        .icon_total_symbol_count =
            static_cast<int>(icon_catalog::all_symbol_count),
        .icon_phenotype_owned_symbol_count =
            static_cast<int>(icon_catalog::phenotype_owned_symbol_count),
        .icon_permissive_source_symbol_count =
            static_cast<int>(icon_catalog::permissive_source_symbol_count),
        .icon_material_symbols_source_symbol_count =
            static_cast<int>(icon_catalog::material_symbols_source_symbol_count),
        .icon_material_symbols_unique_source_icon_count =
            static_cast<int>(icon_catalog::material_symbols_unique_source_icon_count),
        .icon_apple_asset_symbol_count =
            static_cast<int>(icon_catalog::apple_asset_symbol_count),
        .icon_platform_extracted_symbol_count =
            static_cast<int>(icon_catalog::platform_extracted_symbol_count),
        .icon_runtime_fetched_symbol_count =
            static_cast<int>(icon_catalog::runtime_fetched_symbol_count),
        .icon_audited_symbol_source_count =
            static_cast<int>(icon_catalog::audited_symbol_source_count),
        .sidebar_symbol_count =
            static_cast<int>(icon_catalog::sidebar_symbol_count),
        .toolbar_symbol_count =
            static_cast<int>(icon_catalog::toolbar_symbol_count),
        .file_type_symbol_count =
            static_cast<int>(icon_catalog::file_type_symbol_count),
        .icon_filled_symbol_count =
            static_cast<int>(icon_catalog::filled_symbol_count),
        .icon_outline_symbol_count =
            static_cast<int>(icon_catalog::outline_symbol_count),
        .icon_hierarchical_symbol_count =
            static_cast<int>(icon_catalog::hierarchical_symbol_count),
        .icon_monochrome_symbol_count =
            static_cast<int>(icon_catalog::monochrome_symbol_count),
        .icon_regular_weight_symbol_count =
            static_cast<int>(icon_catalog::regular_weight_symbol_count),
        .icon_palette_symbol_count =
            static_cast<int>(icon_catalog::palette_symbol_count),
        .icon_multicolor_symbol_count =
            static_cast<int>(icon_catalog::multicolor_symbol_count),
        .icon_reference_symbol_count =
            static_cast<int>(icon_catalog::reference_symbol_count),
        .icon_svg_path_arc_symbol_count =
            static_cast<int>(icon_catalog::svg_path_arc_symbol_count),
        .icon_round_stroke_symbol_count =
            static_cast<int>(icon_catalog::round_stroke_symbol_count),
        .icon_interaction_phase_count =
            static_cast<int>(icon_catalog::symbol_interaction_phase_count),
        .icon_grid_size =
            icon_catalog::descriptor(icon_catalog::Symbol::Document).grid_size,
        .icon_default_stroke_width =
            icon_catalog::descriptor(
                icon_catalog::Symbol::Document).default_stroke_width,
        .icon_secondary_opacity =
            icon_catalog::descriptor(
                icon_catalog::Symbol::Document).secondary_opacity,
        .icon_toolbar_point_size =
            icon_catalog::metrics(
                icon_catalog::SymbolPresentationRole::Toolbar).point_size,
        .icon_sidebar_point_size =
            icon_catalog::metrics(
                icon_catalog::SymbolPresentationRole::Sidebar).point_size,
        .icon_sidebar_optical_y_offset =
            icon_catalog::metrics(
                icon_catalog::SymbolPresentationRole::Sidebar).optical_y_offset,
        .icon_toolbar_hit_target_size =
            icon_catalog::hit_target_size(
                icon_catalog::SymbolPresentationRole::Toolbar),
        .icon_sidebar_hit_target_size =
            icon_catalog::hit_target_size(
                icon_catalog::SymbolPresentationRole::Sidebar),
        .icon_action_hit_target_size =
            icon_catalog::hit_target_size(
                icon_catalog::SymbolPresentationRole::Action),
        .icon_toolbar_activation_hit_target_size =
            icon_catalog::activation_hit_target_size(
                icon_catalog::SymbolPresentationRole::Toolbar),
        .icon_sidebar_activation_hit_target_size =
            icon_catalog::activation_hit_target_size(
                icon_catalog::SymbolPresentationRole::Sidebar),
        .icon_action_activation_hit_target_size =
            icon_catalog::activation_hit_target_size(
                icon_catalog::SymbolPresentationRole::Action),
        .icon_toolbar_button_radius = toolbar_chrome.corner_radius,
        .icon_toolbar_button_background_alpha =
            toolbar_chrome.background_color.a,
        .icon_toolbar_button_hover_background_alpha =
            toolbar_chrome.hover_background_color.a,
        .icon_toolbar_selected_button_background_alpha =
            toolbar_selected_chrome.background_color.a,
        .icon_toolbar_selected_button_hover_background_alpha =
            toolbar_selected_chrome.hover_background_color.a,
        .icon_toolbar_pressed_button_background_alpha =
            toolbar_pressed_recipe.background_color.a,
        .icon_sidebar_selected_pressed_background_alpha =
            sidebar_selected_pressed_recipe.background_color.a,
        .icon_pressed_symbol_opacity =
            toolbar_pressed_recipe.symbol_opacity,
        .icon_pressed_scale = toolbar_pressed_recipe.scale,
        .column_location_row_height = k_desktop_column_location_row_height,
        .column_location_icon_size = k_desktop_column_location_icon_size,
        .integrated_titlebar = true,
        .native_window_controls = true,
        .duplicate_window_controls = false,
        .window_control_single_owner = true,
        .content_window_control_markers = false,
        .finder_segmented_toolbar = true,
        .owned_icon_assets =
            icon_catalog::phenotype_owned_symbol_count
            == icon_catalog::all_symbol_count,
        .audited_permissive_icon_assets =
            icon_catalog::audited_symbol_source_count
            == icon_catalog::all_symbol_count,
        .uses_apple_icon_assets = icon_catalog::apple_asset_symbol_count > 0,
        .uses_sf_symbols_assets = false,
        .icon_round_stroke_contract = true,
        .icon_text_weight_aligned = true,
        .icon_monochrome_rendering = true,
        .icon_hierarchical_opacity = false,
        .icon_palette_rendering = false,
        .icon_multicolor_rendering = false,
        .thumbnail_uses_external_previews = false,
        .artifact_window_control_markers = false,
        .status_bar_visible = desktop_status_bar_visible(state),
        .theme_contract_version =
            static_cast<int>(theme_contract::theme_contract_version),
        .icon_module = "phenotype.icons",
        .icon_style = std::string{icon_catalog::style_name()},
        .icon_source_format = std::string{icon_catalog::source_format()},
        .icon_svg_subset_policy =
            std::string{icon_catalog::svg_subset_policy()},
        .icon_svg_supported_elements =
            std::string{icon_catalog::svg_supported_elements()},
        .icon_svg_supported_path_commands =
            std::string{icon_catalog::svg_supported_path_commands()},
        .icon_svg_supported_style_attributes =
            std::string{icon_catalog::svg_supported_style_attributes()},
        .icon_svg_arc_policy = std::string{icon_catalog::svg_arc_policy()},
        .icon_stroke_geometry_policy =
            std::string{icon_catalog::stroke_geometry_policy()},
        .icon_stroke_cap_policy =
            std::string{icon_catalog::stroke_cap_policy()},
        .icon_stroke_join_policy =
            std::string{icon_catalog::stroke_join_policy()},
        .icon_design_reference =
            std::string{icon_catalog::style_reference()},
        .icon_reference_family =
            std::string{icon_catalog::reference_family()},
        .icon_reference_policy =
            std::string{icon_catalog::reference_policy()},
        .icon_asset_policy = std::string{icon_catalog::asset_policy()},
        .icon_source_license_policy =
            std::string{icon_catalog::source_license_policy()},
        .icon_preferred_external_source_policy =
            std::string{icon_catalog::preferred_external_source_policy()},
        .icon_source_acquisition_policy =
            std::string{icon_catalog::source_acquisition_policy()},
        .icon_document_cache_policy =
            std::string{icon_catalog::document_cache_policy()},
        .icon_source_attribution_policy =
            std::string{icon_catalog::source_attribution_policy()},
        .icon_apple_asset_boundary =
            std::string{icon_catalog::apple_asset_boundary()},
        .icon_interface_metaphor_policy =
            std::string{icon_catalog::interface_metaphor_policy()},
        .icon_visual_consistency_policy =
            std::string{icon_catalog::visual_consistency_policy()},
        .icon_alignment = std::string{icon_catalog::alignment_policy()},
        .icon_rendering_mode = "monochrome",
        .icon_default_weight =
            std::string{icon_catalog::default_weight_policy()},
        .icon_rendering_capability_policy =
            std::string{icon_catalog::rendering_capability_policy()},
        .icon_variant_policy = std::string{icon_catalog::variant_policy()},
        .icon_presentation_policy =
            std::string{icon_catalog::presentation_policy()},
        .icon_tone_policy = std::string{icon_catalog::tone_policy()},
        .icon_interaction_tone_policy =
            std::string{icon_catalog::interaction_tone_policy()},
        .icon_symbol_control_chrome_policy =
            std::string{icon_catalog::symbol_control_chrome_policy()},
        .icon_symbol_interaction_phase_policy =
            std::string{icon_catalog::symbol_interaction_phase_policy()},
        .icon_toolbar_symbol_chrome_policy =
            std::string{icon_catalog::toolbar_symbol_chrome_policy()},
        .icon_sidebar_symbol_color_policy =
            std::string{icon_catalog::sidebar_symbol_color_policy()},
        .icon_file_type_color_policy =
            std::string{icon_catalog::file_type_color_policy()},
        .icon_metrics_policy = std::string{icon_catalog::metrics_policy()},
        .icon_hit_target_policy =
            std::string{icon_catalog::hit_target_policy()},
        .icon_scale = std::string{icon_catalog::default_scale_policy()},
        .thumbnail_visual_policy = k_desktop_thumbnail_visual_policy,
        .thumbnail_asset_policy = k_desktop_thumbnail_asset_policy,
        .thumbnail_pdf_policy = k_desktop_thumbnail_pdf_policy,
        .thumbnail_image_policy = k_desktop_thumbnail_image_policy,
        .thumbnail_svg_policy = k_desktop_thumbnail_svg_policy,
        .thumbnail_svg_render_policy = k_desktop_thumbnail_svg_render_policy,
        .thumbnail_svg_preview_source_policy =
            k_desktop_thumbnail_svg_preview_source_policy,
        .thumbnail_svg_external_resource_policy =
            k_desktop_thumbnail_svg_external_resource_policy,
        .thumbnail_svg_document_cache_policy =
            k_desktop_thumbnail_svg_document_cache_policy,
        .thumbnail_svg_document_cache_limit =
            k_desktop_thumbnail_svg_document_cache_limit,
        .thumbnail_video_policy = k_desktop_thumbnail_video_policy,
        .thumbnail_shadow_policy = k_desktop_thumbnail_shadow_policy,
        .theme_profile_name =
            std::string{theme_contract::default_theme_profile_name()},
        .theme_reference =
            std::string{theme_contract::default_theme_reference()},
        .theme_font_policy =
            std::string{theme_contract::default_theme_font_policy()},
        .theme_material_policy =
            std::string{theme_contract::default_theme_material_policy()},
        .theme_iconography_policy =
            std::string{theme_contract::default_theme_iconography_policy()},
        .theme_icon_asset_policy =
            std::string{theme_contract::default_theme_icon_asset_policy()},
        .theme_usage_policy =
            std::string{theme_contract::default_theme_usage_policy()},
        .theme_container_policy =
            std::string{theme_contract::default_theme_container_policy()},
        .theme_performance_policy =
            std::string{theme_contract::default_theme_performance_policy()},
        .theme_accessibility_policy =
            std::string{theme_contract::default_theme_accessibility_policy()},
        .theme_fallback_policy =
            std::string{theme_contract::default_theme_fallback_policy()},
        .chrome_geometry_policy = k_desktop_chrome_geometry_policy,
        .finder_density_policy = k_desktop_finder_density_policy,
        .window_control_marker_mode = "runtime-native-controls",
        .native_window_control_owner = "platform-edge",
        .window_control_duplication_guard =
            "native_window_controls_single_owner",
        .window_control_render_policy =
            "native_controls_runtime_only_no_content_or_artifact_markers",
        .titlebar_control_reserve_policy =
            "blank_reserve_under_os_window_controls",
        .native_window_control_integration_policy =
            "platform_standard_controls_inside_leading_content_reserve",
        .native_window_control_geometry_role =
            "reserve_metrics_only_not_paint_instructions",
        .native_window_control_palette_policy =
            "traffic_light_palette_forbidden_in_content_and_artifacts",
    };
}

inline ExplorerChromeMetrics explorer_chrome_with_native_window_control_ownership(
        ExplorerChromeMetrics chrome) {
    chrome.duplicate_window_controls = false;
    if (chrome.native_window_controls)
        chrome.window_control_marker_mode = "runtime-native-controls";
    chrome.content_window_control_markers = false;
    chrome.artifact_window_control_markers = false;
    chrome.content_window_control_marker_count = 0;
    chrome.artifact_window_control_marker_count = 0;
    chrome.content_drawn_window_control_count = 0;
    chrome.artifact_drawn_window_control_count = 0;
    if (chrome.native_window_controls) {
        if (chrome.native_window_control_count <= 0)
            chrome.native_window_control_count = chrome.titlebar_control_count;
        chrome.native_window_control_owner = "platform-edge";
        chrome.window_control_render_policy =
            "native_controls_runtime_only_no_content_or_artifact_markers";
        chrome.titlebar_control_reserve_policy =
            "blank_reserve_under_os_window_controls";
        chrome.native_window_control_integration_policy =
            "platform_standard_controls_inside_leading_content_reserve";
        chrome.native_window_control_geometry_role =
            "reserve_metrics_only_not_paint_instructions";
        chrome.native_window_control_palette_policy =
            "traffic_light_palette_forbidden_in_content_and_artifacts";
    } else {
        chrome.native_window_control_count = 0;
        chrome.native_window_control_owner = "none";
        chrome.window_control_marker_mode = "none";
        chrome.native_window_control_integration_policy = "none";
        chrome.native_window_control_geometry_role = "none";
        chrome.native_window_control_palette_policy = "none";
    }
    chrome.window_control_single_owner =
        !chrome.duplicate_window_controls
        && !chrome.content_window_control_markers
        && !chrome.artifact_window_control_markers
        && chrome.content_window_control_marker_count == 0
        && chrome.artifact_window_control_marker_count == 0
        && chrome.content_drawn_window_control_count == 0
        && chrome.artifact_drawn_window_control_count == 0
        && ((chrome.native_window_controls
                && chrome.native_window_control_owner == "platform-edge"
                && chrome.native_window_control_count > 0)
            || (!chrome.native_window_controls
                && chrome.native_window_control_owner == "none"
                && chrome.native_window_control_count == 0));
    chrome.window_control_duplication_guard =
        chrome.window_control_single_owner
            ? (chrome.native_window_controls
                  ? "native_window_controls_single_owner"
                  : "no_window_controls_in_shell")
            : "duplicate_window_control_risk";
    return chrome;
}

inline std::vector<float> explorer_icon_grid_columns(
        ExplorerChromeMetrics const& chrome) {
    int count = chrome.icon_grid_columns > 0 ? chrome.icon_grid_columns : 2;
    return std::vector<float>(
        static_cast<std::size_t>(count),
        chrome.icon_grid_column_width > 0.0f
            ? chrome.icon_grid_column_width
            : k_desktop_icon_grid_column_width);
}

} // namespace file_explorer_demo

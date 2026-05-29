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

export module file_explorer_shared:chrome_debug_json;

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

export namespace file_explorer_demo {
inline json::Value explorer_chrome_debug_json(
        ExplorerChromeMetrics const& chrome) {
    json::Object viewport;
    viewport.emplace("w", json::Value{static_cast<std::int64_t>(chrome.viewport.width)});
    viewport.emplace("h", json::Value{static_cast<std::int64_t>(chrome.viewport.height)});
    viewport.emplace("scale", json::Value{chrome.viewport.scale});

    json::Object native_window;
    native_window.emplace("integrated_titlebar", json::Value{chrome.integrated_titlebar});
    native_window.emplace("native_window_controls", json::Value{chrome.native_window_controls});
    native_window.emplace("duplicate_window_controls", json::Value{chrome.duplicate_window_controls});
    native_window.emplace(
        "window_control_single_owner",
        json::Value{chrome.window_control_single_owner});
    native_window.emplace("content_window_control_markers", json::Value{chrome.content_window_control_markers});
    native_window.emplace("artifact_window_control_markers", json::Value{chrome.artifact_window_control_markers});
    native_window.emplace("window_control_marker_mode", json::Value{chrome.window_control_marker_mode});
    native_window.emplace(
        "native_window_control_owner",
        json::Value{chrome.native_window_control_owner});
    native_window.emplace(
        "window_control_duplication_guard",
        json::Value{chrome.window_control_duplication_guard});
    native_window.emplace(
        "native_window_control_count",
        json::Value{static_cast<std::int64_t>(chrome.native_window_control_count)});
    native_window.emplace(
        "content_window_control_marker_count",
        json::Value{static_cast<std::int64_t>(
            chrome.content_window_control_marker_count)});
    native_window.emplace(
        "artifact_window_control_marker_count",
        json::Value{static_cast<std::int64_t>(
            chrome.artifact_window_control_marker_count)});
    native_window.emplace(
        "content_drawn_window_control_count",
        json::Value{static_cast<std::int64_t>(
            chrome.content_drawn_window_control_count)});
    native_window.emplace(
        "artifact_drawn_window_control_count",
        json::Value{static_cast<std::int64_t>(
            chrome.artifact_drawn_window_control_count)});
    native_window.emplace(
        "window_control_render_policy",
        json::Value{chrome.window_control_render_policy});
    native_window.emplace(
        "titlebar_control_reserve_policy",
        json::Value{chrome.titlebar_control_reserve_policy});
    native_window.emplace(
        "native_window_control_integration_policy",
        json::Value{chrome.native_window_control_integration_policy});
    native_window.emplace(
        "native_window_control_geometry_role",
        json::Value{chrome.native_window_control_geometry_role});
    native_window.emplace(
        "native_window_control_palette_policy",
        json::Value{chrome.native_window_control_palette_policy});
    native_window.emplace(
        "titlebar_drag_region_height",
        json::Value{chrome.titlebar_drag_region_height});
    native_window.emplace(
        "leading_control_reserved_width",
        json::Value{chrome.leading_control_reserved_width});
    native_window.emplace(
        "trailing_control_reserved_width",
        json::Value{chrome.trailing_control_reserved_width});

    json::Object geometry;
    geometry.emplace("policy", json::Value{chrome.chrome_geometry_policy});
    geometry.emplace("window_content_inset", json::Value{chrome.window_content_inset});
    geometry.emplace("window_gap", json::Value{chrome.window_gap});
    geometry.emplace("toolbar_shell_x", json::Value{chrome.toolbar_shell_x});
    geometry.emplace("toolbar_shell_y", json::Value{chrome.toolbar_shell_y});
    geometry.emplace("toolbar_shell_width", json::Value{chrome.toolbar_shell_width});
    geometry.emplace("toolbar_shell_height", json::Value{chrome.toolbar_shell_height});
    geometry.emplace("toolbar_group_y", json::Value{chrome.toolbar_group_y});
    geometry.emplace("toolbar_navigation_group_x", json::Value{chrome.toolbar_navigation_group_x});
    geometry.emplace("toolbar_title_x", json::Value{chrome.toolbar_title_x});
    geometry.emplace("toolbar_view_group_x", json::Value{chrome.toolbar_view_group_x});
    geometry.emplace("toolbar_sort_group_x", json::Value{chrome.toolbar_sort_group_x});
    geometry.emplace("toolbar_action_group_x", json::Value{chrome.toolbar_action_group_x});
    geometry.emplace("toolbar_search_group_x", json::Value{chrome.toolbar_search_group_x});
    geometry.emplace("content_surface_x", json::Value{chrome.content_surface_x});
    geometry.emplace("content_surface_y", json::Value{chrome.content_surface_y});
    geometry.emplace("content_surface_width", json::Value{chrome.content_surface_width});
    geometry.emplace("sidebar_surface_x", json::Value{chrome.sidebar_surface_x});
    geometry.emplace("sidebar_surface_y", json::Value{chrome.sidebar_surface_y});
    geometry.emplace("sidebar_first_row_y", json::Value{chrome.sidebar_first_row_y});
    geometry.emplace("finder_density_policy", json::Value{chrome.finder_density_policy});
    geometry.emplace("icon_grid_top_inset", json::Value{chrome.icon_grid_top_inset});

    json::Object icon_system;
    icon_system.emplace("module", json::Value{chrome.icon_module});
    icon_system.emplace("style", json::Value{chrome.icon_style});
    icon_system.emplace("family", json::Value{"Material Symbols (new)"});
    icon_system.emplace(
        "default_material_symbols_style",
        json::Value{std::string{icon_catalog::material_symbols_style_name(
            icon_catalog::default_material_symbols_style())}});
    icon_system.emplace(
        "available_material_symbols_styles",
        material_symbols_styles_debug_json());
    icon_system.emplace("source_format", json::Value{chrome.icon_source_format});
    icon_system.emplace("svg_subset_policy", json::Value{chrome.icon_svg_subset_policy});
    icon_system.emplace(
        "svg_supported_elements",
        json::Value{chrome.icon_svg_supported_elements});
    icon_system.emplace(
        "svg_supported_path_commands",
        json::Value{chrome.icon_svg_supported_path_commands});
    icon_system.emplace(
        "svg_supported_style_attributes",
        json::Value{chrome.icon_svg_supported_style_attributes});
    icon_system.emplace("svg_arc_policy", json::Value{chrome.icon_svg_arc_policy});
    icon_system.emplace(
        "stroke_geometry_policy",
        json::Value{chrome.icon_stroke_geometry_policy});
    icon_system.emplace(
        "stroke_cap_policy",
        json::Value{chrome.icon_stroke_cap_policy});
    icon_system.emplace(
        "stroke_join_policy",
        json::Value{chrome.icon_stroke_join_policy});
    icon_system.emplace("owned_assets", json::Value{chrome.owned_icon_assets});
    icon_system.emplace(
        "audited_permissive_assets",
        json::Value{chrome.audited_permissive_icon_assets});
    icon_system.emplace(
        "uses_apple_icon_assets",
        json::Value{chrome.uses_apple_icon_assets});
    icon_system.emplace("uses_sf_symbols_assets", json::Value{chrome.uses_sf_symbols_assets});
    icon_system.emplace("round_stroke_contract", json::Value{chrome.icon_round_stroke_contract});
    icon_system.emplace(
        "total_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_total_symbol_count)});
    icon_system.emplace(
        "phenotype_owned_symbol_count",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_phenotype_owned_symbol_count)});
    icon_system.emplace(
        "permissive_source_symbol_count",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_permissive_source_symbol_count)});
    icon_system.emplace(
        "material_symbols_source_symbol_count",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_material_symbols_source_symbol_count)});
    icon_system.emplace(
        "material_symbols_unique_source_icon_count",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_material_symbols_unique_source_icon_count)});
    icon_system.emplace(
        "apple_asset_symbol_count",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_apple_asset_symbol_count)});
    icon_system.emplace(
        "platform_extracted_symbol_count",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_platform_extracted_symbol_count)});
    icon_system.emplace(
        "runtime_fetched_symbol_count",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_runtime_fetched_symbol_count)});
    icon_system.emplace(
        "audited_symbol_source_count",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_audited_symbol_source_count)});
    icon_system.emplace(
        "sidebar_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.sidebar_symbol_count)});
    icon_system.emplace(
        "toolbar_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.toolbar_symbol_count)});
    icon_system.emplace(
        "file_type_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.file_type_symbol_count)});
    icon_system.emplace(
        "filled_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_filled_symbol_count)});
    icon_system.emplace(
        "outline_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_outline_symbol_count)});
    icon_system.emplace(
        "hierarchical_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_hierarchical_symbol_count)});
    icon_system.emplace(
        "monochrome_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_monochrome_symbol_count)});
    icon_system.emplace(
        "regular_weight_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_regular_weight_symbol_count)});
    icon_system.emplace(
        "palette_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_palette_symbol_count)});
    icon_system.emplace(
        "multicolor_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_multicolor_symbol_count)});
    icon_system.emplace(
        "reference_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_reference_symbol_count)});
    icon_system.emplace(
        "reference_source_count",
        json::Value{static_cast<std::int64_t>(
            icon_catalog::reference_source_count)});
    icon_system.emplace(
        "reference_sources",
        icon_reference_sources_debug_json());
    icon_system.emplace(
        "svg_path_arc_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_svg_path_arc_symbol_count)});
    icon_system.emplace(
        "round_stroke_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_round_stroke_symbol_count)});
    icon_system.emplace(
        "interaction_phase_count",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_interaction_phase_count)});
    icon_system.emplace("grid_size", json::Value{chrome.icon_grid_size});
    icon_system.emplace(
        "default_stroke_width",
        json::Value{chrome.icon_default_stroke_width});
    icon_system.emplace("secondary_opacity", json::Value{chrome.icon_secondary_opacity});
    icon_system.emplace(
        "toolbar_point_size",
        json::Value{chrome.icon_toolbar_point_size});
    icon_system.emplace(
        "sidebar_point_size",
        json::Value{chrome.icon_sidebar_point_size});
    icon_system.emplace(
        "sidebar_optical_y_offset",
        json::Value{chrome.icon_sidebar_optical_y_offset});
    icon_system.emplace(
        "toolbar_hit_target_size",
        json::Value{chrome.icon_toolbar_hit_target_size});
    icon_system.emplace(
        "sidebar_hit_target_size",
        json::Value{chrome.icon_sidebar_hit_target_size});
    icon_system.emplace(
        "action_hit_target_size",
        json::Value{chrome.icon_action_hit_target_size});
    icon_system.emplace(
        "toolbar_activation_hit_target_size",
        json::Value{chrome.icon_toolbar_activation_hit_target_size});
    icon_system.emplace(
        "sidebar_activation_hit_target_size",
        json::Value{chrome.icon_sidebar_activation_hit_target_size});
    icon_system.emplace(
        "action_activation_hit_target_size",
        json::Value{chrome.icon_action_activation_hit_target_size});
    icon_system.emplace(
        "toolbar_button_radius",
        json::Value{chrome.icon_toolbar_button_radius});
    icon_system.emplace(
        "toolbar_button_background_alpha",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_toolbar_button_background_alpha)});
    icon_system.emplace(
        "toolbar_button_hover_background_alpha",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_toolbar_button_hover_background_alpha)});
    icon_system.emplace(
        "toolbar_selected_button_background_alpha",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_toolbar_selected_button_background_alpha)});
    icon_system.emplace(
        "toolbar_selected_button_hover_background_alpha",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_toolbar_selected_button_hover_background_alpha)});
    icon_system.emplace(
        "toolbar_pressed_button_background_alpha",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_toolbar_pressed_button_background_alpha)});
    icon_system.emplace(
        "sidebar_selected_pressed_background_alpha",
        json::Value{static_cast<std::int64_t>(
            chrome.icon_sidebar_selected_pressed_background_alpha)});
    icon_system.emplace(
        "pressed_symbol_opacity",
        json::Value{chrome.icon_pressed_symbol_opacity});
    icon_system.emplace("pressed_scale", json::Value{chrome.icon_pressed_scale});
    icon_system.emplace(
        "column_location_icon_size",
        json::Value{chrome.column_location_icon_size});
    icon_system.emplace("text_weight_aligned", json::Value{chrome.icon_text_weight_aligned});
    icon_system.emplace("monochrome_rendering", json::Value{chrome.icon_monochrome_rendering});
    icon_system.emplace("hierarchical_opacity", json::Value{chrome.icon_hierarchical_opacity});
    icon_system.emplace("palette_rendering", json::Value{chrome.icon_palette_rendering});
    icon_system.emplace("multicolor_rendering", json::Value{chrome.icon_multicolor_rendering});
    icon_system.emplace("design_reference", json::Value{chrome.icon_design_reference});
    icon_system.emplace("reference_family", json::Value{chrome.icon_reference_family});
    icon_system.emplace("reference_policy", json::Value{chrome.icon_reference_policy});
    icon_system.emplace("asset_policy", json::Value{chrome.icon_asset_policy});
    icon_system.emplace(
        "source_license_policy",
        json::Value{chrome.icon_source_license_policy});
    icon_system.emplace(
        "preferred_external_source_policy",
        json::Value{chrome.icon_preferred_external_source_policy});
    icon_system.emplace(
        "source_acquisition_policy",
        json::Value{chrome.icon_source_acquisition_policy});
    icon_system.emplace(
        "document_cache_policy",
        json::Value{chrome.icon_document_cache_policy});
    icon_system.emplace(
        "source_attribution_policy",
        json::Value{chrome.icon_source_attribution_policy});
    icon_system.emplace(
        "apple_asset_boundary",
        json::Value{chrome.icon_apple_asset_boundary});
    icon_system.emplace(
        "interface_metaphor_policy",
        json::Value{chrome.icon_interface_metaphor_policy});
    icon_system.emplace(
        "visual_consistency_policy",
        json::Value{chrome.icon_visual_consistency_policy});
    icon_system.emplace("alignment", json::Value{chrome.icon_alignment});
    icon_system.emplace("rendering_mode", json::Value{chrome.icon_rendering_mode});
    icon_system.emplace("default_weight", json::Value{chrome.icon_default_weight});
    icon_system.emplace(
        "rendering_capability_policy",
        json::Value{chrome.icon_rendering_capability_policy});
    icon_system.emplace("variant_policy", json::Value{chrome.icon_variant_policy});
    icon_system.emplace("presentation_policy", json::Value{chrome.icon_presentation_policy});
    icon_system.emplace("tone_policy", json::Value{chrome.icon_tone_policy});
    icon_system.emplace(
        "interaction_tone_policy",
        json::Value{chrome.icon_interaction_tone_policy});
    icon_system.emplace(
        "symbol_control_chrome_policy",
        json::Value{chrome.icon_symbol_control_chrome_policy});
    icon_system.emplace(
        "symbol_interaction_phase_policy",
        json::Value{chrome.icon_symbol_interaction_phase_policy});
    icon_system.emplace(
        "toolbar_symbol_chrome_policy",
        json::Value{chrome.icon_toolbar_symbol_chrome_policy});
    icon_system.emplace(
        "sidebar_symbol_color_policy",
        json::Value{chrome.icon_sidebar_symbol_color_policy});
    icon_system.emplace(
        "interaction_tones",
        icon_interaction_tones_debug_json(chrome));
    icon_system.emplace(
        "file_type_color_policy",
        json::Value{chrome.icon_file_type_color_policy});
    icon_system.emplace(
        "metrics_policy",
        json::Value{chrome.icon_metrics_policy});
    icon_system.emplace(
        "hit_target_policy",
        json::Value{chrome.icon_hit_target_policy});
    icon_system.emplace("scale", json::Value{chrome.icon_scale});
    icon_system.emplace(
        "sidebar_reference_symbols",
        sidebar_icon_reference_symbols_debug_json(chrome));
    icon_system.emplace(
        "sidebar_symbol_tokens",
        sidebar_symbol_tokens_debug_json(chrome));
    icon_system.emplace(
        "sidebar_symbol_presentations",
        sidebar_symbol_presentations_debug_json(chrome));
    icon_system.emplace(
        "toolbar_reference_symbols",
        toolbar_icon_reference_symbols_debug_json(chrome));
    icon_system.emplace(
        "toolbar_symbol_presentations",
        toolbar_symbol_presentations_debug_json(chrome));
    icon_system.emplace(
        "file_type_symbol_tokens",
        file_type_symbol_tokens_debug_json(chrome));
    icon_system.emplace(
        "file_type_reference_symbols",
        file_type_icon_reference_symbols_debug_json(chrome));
    icon_system.emplace(
        "file_type_symbol_presentations",
        file_type_symbol_presentations_debug_json(chrome));
    icon_system.emplace(
        "presentation_samples",
        icon_presentation_samples_debug_json(chrome));

    json::Object thumbnail_system;
    thumbnail_system.emplace(
        "visual_policy",
        json::Value{chrome.thumbnail_visual_policy});
    thumbnail_system.emplace(
        "asset_policy",
        json::Value{chrome.thumbnail_asset_policy});
    thumbnail_system.emplace("pdf_policy", json::Value{chrome.thumbnail_pdf_policy});
    thumbnail_system.emplace(
        "image_policy",
        json::Value{chrome.thumbnail_image_policy});
    thumbnail_system.emplace(
        "svg_policy",
        json::Value{chrome.thumbnail_svg_policy});
    thumbnail_system.emplace(
        "svg_render_policy",
        json::Value{chrome.thumbnail_svg_render_policy});
    thumbnail_system.emplace(
        "svg_preview_source_policy",
        json::Value{chrome.thumbnail_svg_preview_source_policy});
    thumbnail_system.emplace(
        "svg_external_resource_policy",
        json::Value{chrome.thumbnail_svg_external_resource_policy});
    thumbnail_system.emplace(
        "svg_document_cache_policy",
        json::Value{chrome.thumbnail_svg_document_cache_policy});
    thumbnail_system.emplace(
        "svg_document_cache_limit",
        json::Value{static_cast<std::int64_t>(
            chrome.thumbnail_svg_document_cache_limit)});
    thumbnail_system.emplace(
        "video_policy",
        json::Value{chrome.thumbnail_video_policy});
    thumbnail_system.emplace(
        "shadow_policy",
        json::Value{chrome.thumbnail_shadow_policy});
    thumbnail_system.emplace(
        "uses_external_previews",
        json::Value{chrome.thumbnail_uses_external_previews});
    thumbnail_system.emplace(
        "pdf_page_width",
        json::Value{chrome.thumbnail_pdf_page_width});
    thumbnail_system.emplace(
        "pdf_page_height",
        json::Value{chrome.thumbnail_pdf_page_height});
    thumbnail_system.emplace(
        "pdf_page_radius",
        json::Value{chrome.thumbnail_pdf_page_radius});
    thumbnail_system.emplace(
        "pdf_fold_size",
        json::Value{chrome.thumbnail_pdf_fold_size});
    thumbnail_system.emplace(
        "media_preview_width",
        json::Value{chrome.thumbnail_media_preview_width});
    thumbnail_system.emplace(
        "media_preview_height",
        json::Value{chrome.thumbnail_media_preview_height});
    thumbnail_system.emplace(
        "media_preview_radius",
        json::Value{chrome.thumbnail_media_preview_radius});
    thumbnail_system.emplace(
        "pdf_detail_line_count",
        json::Value{static_cast<std::int64_t>(
            chrome.thumbnail_pdf_detail_line_count)});
    thumbnail_system.emplace(
        "image_sample_block_count",
        json::Value{static_cast<std::int64_t>(
            chrome.thumbnail_image_sample_block_count)});
    thumbnail_system.emplace(
        "video_strip_count",
        json::Value{static_cast<std::int64_t>(
            chrome.thumbnail_video_strip_count)});

    json::Object out;
    out.emplace("viewport", json::Value{std::move(viewport)});
    out.emplace("integrated_titlebar_height", json::Value{chrome.integrated_titlebar_height});
    out.emplace("sidebar_width", json::Value{chrome.sidebar_width});
    out.emplace("sidebar_row_width", json::Value{chrome.sidebar_row_width});
    out.emplace("sidebar_row_height", json::Value{chrome.sidebar_row_height});
    out.emplace("sidebar_heading_height", json::Value{chrome.sidebar_heading_height});
    out.emplace("sidebar_icon_size", json::Value{chrome.sidebar_icon_size});
    out.emplace("sidebar_icon_leading", json::Value{chrome.sidebar_icon_leading});
    out.emplace("sidebar_label_leading", json::Value{chrome.sidebar_label_leading});
    out.emplace("sidebar_label_top", json::Value{chrome.sidebar_label_top});
    out.emplace("sidebar_heading_label_leading", json::Value{chrome.sidebar_heading_label_leading});
    out.emplace("sidebar_heading_label_top", json::Value{chrome.sidebar_heading_label_top});
    out.emplace("sidebar_section_gap", json::Value{chrome.sidebar_section_gap});
    out.emplace("sidebar_selected_row_radius", json::Value{chrome.sidebar_selected_row_radius});
    out.emplace(
        "sidebar_selected_row_background_alpha",
        json::Value{static_cast<std::int64_t>(
            chrome.sidebar_selected_row_background_alpha)});
    out.emplace(
        "sidebar_selected_row_hover_background_alpha",
        json::Value{static_cast<std::int64_t>(
            chrome.sidebar_selected_row_hover_background_alpha)});
    out.emplace(
        "sidebar_selection_policy",
        json::Value{chrome.sidebar_selection_policy});
    out.emplace("toolbar_group_height", json::Value{chrome.toolbar_group_height});
    out.emplace("toolbar_group_radius", json::Value{chrome.toolbar_group_radius});
    out.emplace("toolbar_icon_button_width", json::Value{chrome.toolbar_icon_button_width});
    out.emplace("toolbar_icon_button_height", json::Value{chrome.toolbar_icon_button_height});
    out.emplace("titlebar_control_cluster_height", json::Value{chrome.titlebar_control_cluster_height});
    out.emplace("titlebar_control_diameter", json::Value{chrome.titlebar_control_diameter});
    out.emplace("titlebar_control_spacing", json::Value{chrome.titlebar_control_spacing});
    out.emplace("titlebar_control_start_x", json::Value{chrome.titlebar_control_start_x});
    out.emplace("titlebar_control_top", json::Value{chrome.titlebar_control_top});
    out.emplace("window_radius", json::Value{chrome.window_radius});
    out.emplace("icon_grid_column_width", json::Value{chrome.icon_grid_column_width});
    out.emplace("icon_grid_row_height", json::Value{chrome.icon_grid_row_height});
    out.emplace("icon_grid_column_pitch", json::Value{chrome.icon_grid_column_pitch});
    out.emplace("icon_grid_thumbnail_width", json::Value{chrome.icon_grid_thumbnail_width});
    out.emplace("icon_grid_thumbnail_height", json::Value{chrome.icon_grid_thumbnail_height});
    out.emplace("thumbnail_system", json::Value{std::move(thumbnail_system)});
    out.emplace("icon_grid_label_height", json::Value{chrome.icon_grid_label_height});
    out.emplace("icon_grid_label_font_size", json::Value{chrome.icon_grid_label_font_size});
    out.emplace("icon_grid_label_policy", json::Value{chrome.icon_grid_label_policy});
    out.emplace("icon_grid_top_inset", json::Value{chrome.icon_grid_top_inset});
    out.emplace("icon_grid_gap", json::Value{chrome.icon_grid_gap});
    out.emplace("icon_grid_scroll_height", json::Value{chrome.icon_grid_scroll_height});
    out.emplace("icon_grid_columns", json::Value{static_cast<std::int64_t>(chrome.icon_grid_columns)});
    out.emplace("icon_grid_visible_rows", json::Value{static_cast<std::int64_t>(chrome.icon_grid_visible_rows)});
    out.emplace("icon_grid_visible_capacity", json::Value{static_cast<std::int64_t>(chrome.icon_grid_visible_capacity)});
    out.emplace("toolbar_group_count", json::Value{static_cast<std::int64_t>(chrome.toolbar_group_count)});
    out.emplace("toolbar_separator_count", json::Value{static_cast<std::int64_t>(chrome.toolbar_separator_count)});
    out.emplace("toolbar_icon_button_count", json::Value{static_cast<std::int64_t>(chrome.toolbar_icon_button_count)});
    out.emplace("column_location_row_count", json::Value{static_cast<std::int64_t>(chrome.column_location_row_count)});
    out.emplace("column_location_row_height", json::Value{chrome.column_location_row_height});
    out.emplace("titlebar_control_count", json::Value{static_cast<std::int64_t>(chrome.titlebar_control_count)});
    out.emplace("native_window_control_count", json::Value{static_cast<std::int64_t>(chrome.native_window_control_count)});
    out.emplace("content_window_control_marker_count", json::Value{static_cast<std::int64_t>(chrome.content_window_control_marker_count)});
    out.emplace("artifact_window_control_marker_count", json::Value{static_cast<std::int64_t>(chrome.artifact_window_control_marker_count)});
    out.emplace("content_drawn_window_control_count", json::Value{static_cast<std::int64_t>(chrome.content_drawn_window_control_count)});
    out.emplace("artifact_drawn_window_control_count", json::Value{static_cast<std::int64_t>(chrome.artifact_drawn_window_control_count)});
    out.emplace("overflow_action_button_count", json::Value{static_cast<std::int64_t>(chrome.overflow_action_button_count)});
    out.emplace("sidebar_symbol_count", json::Value{static_cast<std::int64_t>(chrome.sidebar_symbol_count)});
    out.emplace("toolbar_symbol_count", json::Value{static_cast<std::int64_t>(chrome.toolbar_symbol_count)});
    out.emplace("file_type_symbol_count", json::Value{static_cast<std::int64_t>(chrome.file_type_symbol_count)});
    out.emplace("content_window_control_markers", json::Value{chrome.content_window_control_markers});
    out.emplace("artifact_window_control_markers", json::Value{chrome.artifact_window_control_markers});
    out.emplace("window_control_single_owner", json::Value{chrome.window_control_single_owner});
    out.emplace("window_control_marker_mode", json::Value{chrome.window_control_marker_mode});
    out.emplace("native_window_control_owner", json::Value{chrome.native_window_control_owner});
    out.emplace("window_control_duplication_guard", json::Value{chrome.window_control_duplication_guard});
    out.emplace("window_control_render_policy", json::Value{chrome.window_control_render_policy});
    out.emplace("titlebar_control_reserve_policy", json::Value{chrome.titlebar_control_reserve_policy});
    out.emplace("native_window_control_integration_policy", json::Value{chrome.native_window_control_integration_policy});
    out.emplace("native_window_control_geometry_role", json::Value{chrome.native_window_control_geometry_role});
    out.emplace("native_window_control_palette_policy", json::Value{chrome.native_window_control_palette_policy});
    out.emplace("finder_segmented_toolbar", json::Value{chrome.finder_segmented_toolbar});
    out.emplace("more_actions_open", json::Value{chrome.more_actions_open});
    out.emplace("status_bar_visible", json::Value{chrome.status_bar_visible});
    out.emplace("finder_density_policy", json::Value{chrome.finder_density_policy});
    out.emplace("native_window", json::Value{std::move(native_window)});
    out.emplace("geometry", json::Value{std::move(geometry)});
    out.emplace("icon_system", json::Value{std::move(icon_system)});
    return json::Value{std::move(out)};
}

} // namespace file_explorer_demo

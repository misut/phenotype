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

export module file_explorer_shared:desktop_metrics_and_symbols;

import json;
import phenotype.icon_catalog;
import phenotype.resources;
import phenotype.theme_contract;
import :model_types;

export namespace file_explorer_demo {
inline constexpr int k_desktop_default_viewport_width = 1300;
inline constexpr int k_desktop_default_viewport_height = 760;
inline constexpr int k_mobile_default_viewport_width = 390;
inline constexpr int k_mobile_default_viewport_height = 844;
inline constexpr float k_desktop_integrated_titlebar_height = 64.0f;
inline constexpr float k_desktop_window_content_inset = 4.0f;
inline constexpr float k_desktop_window_gap = 8.0f;
inline constexpr float k_desktop_sidebar_width = 224.0f;
inline constexpr float k_desktop_sidebar_row_width = 188.0f;
inline constexpr float k_desktop_sidebar_row_height = 38.0f;
inline constexpr float k_desktop_sidebar_heading_height = 30.0f;
inline constexpr float k_desktop_sidebar_icon_size = 26.0f;
inline constexpr float k_desktop_sidebar_icon_leading = 12.0f;
inline constexpr float k_desktop_sidebar_label_leading = 48.0f;
inline constexpr float k_desktop_sidebar_label_top = 8.0f;
inline constexpr float k_desktop_sidebar_heading_label_leading = 10.0f;
inline constexpr float k_desktop_sidebar_heading_label_top = 7.0f;
inline constexpr float k_desktop_sidebar_material_padding = 16.0f;
inline constexpr float k_desktop_sidebar_item_gap = 4.0f;
inline constexpr float k_desktop_sidebar_section_gap = 14.0f;
inline constexpr float k_desktop_sidebar_selected_row_radius = 10.0f;
inline constexpr float k_desktop_sidebar_selected_row_border_width = 0.0f;
inline constexpr int k_desktop_sidebar_selected_row_background_alpha = 150;
inline constexpr int k_desktop_sidebar_selected_row_hover_background_alpha = 176;
inline constexpr char k_desktop_sidebar_selection_policy[] =
    "finder_translucent_selected_row_no_outline_accent_symbol";
inline constexpr char k_finder_visual_contract_name[] =
    "finder_visual_parity_contract";
inline constexpr char k_finder_visual_contract_source[] =
    "file_explorer_shared::finder_visual_contract";
inline constexpr char k_finder_visual_contract_reference[] =
    "Apple HIG Materials, Icons, SF Symbols semantics, and macOS Finder visual reference without embedded Apple artwork";
inline constexpr char k_finder_visual_titlebar_strategy[] =
    "integrated_titlebar_content_reserve_with_platform_window_controls";
inline constexpr char k_finder_visual_marker_policy[] =
    "no_content_or_artifact_window_control_markers";
inline constexpr char k_finder_visual_focus_ring_policy[] =
    "keyboard_tab_navigation_only_pointer_click_hides_focus_ring";
inline constexpr char k_finder_visual_verifier_gate[] =
    "local_file_explorer_artifact_verify_not_default_pr_ci";
inline constexpr int k_desktop_column_location_row_count = 4;
inline constexpr float k_desktop_column_location_row_height = 30.0f;
inline constexpr float k_desktop_column_location_icon_size = 18.0f;
inline constexpr float k_desktop_window_radius = 18.0f;
inline constexpr float k_desktop_toolbar_group_radius = 22.0f;
inline constexpr float k_desktop_toolbar_group_height = 46.0f;
inline constexpr float k_desktop_toolbar_shell_height = 52.0f;
inline constexpr float k_desktop_toolbar_shell_padding = 4.0f;
inline constexpr float k_desktop_main_shell_padding = 4.0f;
inline constexpr float k_desktop_main_shell_gap = 4.0f;
inline constexpr float k_desktop_toolbar_navigation_group_width = 96.0f;
inline constexpr float k_desktop_toolbar_view_group_width = 216.0f;
inline constexpr float k_desktop_toolbar_sort_group_width = 48.0f;
inline constexpr float k_desktop_toolbar_action_group_width = 128.0f;
inline constexpr float k_desktop_toolbar_search_collapsed_width = 48.0f;
inline constexpr float k_desktop_toolbar_icon_button_width = 38.0f;
inline constexpr float k_desktop_toolbar_icon_button_height = 36.0f;
inline constexpr float k_desktop_titlebar_control_cluster_height = 40.0f;
inline constexpr float k_desktop_titlebar_control_diameter = 13.0f;
inline constexpr float k_desktop_titlebar_control_spacing = 23.0f;
inline constexpr float k_desktop_titlebar_control_start_x = 20.0f;
inline constexpr float k_desktop_titlebar_control_top = 16.0f;
inline constexpr float k_desktop_titlebar_drag_region_height =
    k_desktop_integrated_titlebar_height;
inline constexpr float k_desktop_leading_control_reserved_width = 176.0f;
inline constexpr float k_desktop_trailing_control_reserved_width = 168.0f;
inline constexpr int k_desktop_titlebar_control_count = 3;
inline constexpr float k_desktop_icon_grid_column_width = 126.0f;
inline constexpr float k_desktop_icon_grid_row_height = 148.0f;
inline constexpr float k_desktop_icon_grid_column_pitch = 150.0f;
inline constexpr float k_desktop_icon_grid_thumbnail_width = 118.0f;
inline constexpr float k_desktop_icon_grid_thumbnail_height = 72.0f;
inline constexpr float k_desktop_thumbnail_pdf_page_width = 50.0f;
inline constexpr float k_desktop_thumbnail_pdf_page_height = 64.0f;
inline constexpr float k_desktop_thumbnail_pdf_page_radius = 4.0f;
inline constexpr float k_desktop_thumbnail_pdf_fold_size = 12.0f;
inline constexpr float k_desktop_thumbnail_media_preview_width = 88.0f;
inline constexpr float k_desktop_thumbnail_media_preview_height = 38.0f;
inline constexpr float k_desktop_thumbnail_media_preview_radius = 7.0f;
inline constexpr int k_desktop_thumbnail_pdf_detail_line_count = 12;
inline constexpr int k_desktop_thumbnail_image_sample_block_count = 7;
inline constexpr int k_desktop_thumbnail_video_strip_count = 6;
inline constexpr char k_desktop_thumbnail_visual_policy[] =
    "finder_rich_preview_thumbnails_v1";
inline constexpr char k_desktop_thumbnail_asset_policy[] =
    "deterministic_vector_preview_painters_no_external_assets";
inline constexpr char k_desktop_thumbnail_pdf_policy[] =
    "page_preview_with_fold_header_table_rows_and_bottom_highlight";
inline constexpr char k_desktop_thumbnail_image_policy[] =
    "rounded_screenshot_strip_with_layered_blurred_content";
inline constexpr char k_desktop_thumbnail_svg_policy[] =
    "rounded_svg_vector_preview_with_source_safe_svg_badge";
inline constexpr char k_desktop_thumbnail_svg_render_policy[] =
    "phenotype_svg_subset_renders_file_body_inside_finder_preview";
inline constexpr char k_desktop_thumbnail_svg_preview_source_policy[] =
    "sandbox_file_body_only_no_system_icon_extraction";
inline constexpr char k_desktop_thumbnail_svg_external_resource_policy[] =
    "no_external_svg_resources_or_network_fetches";
inline constexpr char k_desktop_thumbnail_svg_document_cache_policy[] =
    "edge_svg_document_cache_keyed_by_file_preview_body_no_frame_parse_churn";
inline constexpr int k_desktop_thumbnail_svg_document_cache_limit = 32;
inline constexpr char k_desktop_thumbnail_video_policy[] =
    "wide_video_preview_with_filmstrip_and_content_bands";
inline constexpr char k_desktop_thumbnail_shadow_policy[] =
    "subtle_windowserver_like_drop_shadow";
inline constexpr float k_desktop_icon_grid_label_height = 46.0f;
inline constexpr float k_desktop_icon_grid_label_font_size = 14.0f;
inline constexpr char k_desktop_icon_grid_label_policy[] =
    "finder_two_line_middle_ellipsis_preserve_suffix";
inline constexpr float k_desktop_content_section_padding = 24.0f;
inline constexpr float k_desktop_content_section_bottom_padding = 0.0f;
inline constexpr float k_desktop_content_section_gap = 12.0f;
inline constexpr float k_desktop_status_bar_height = 28.0f;
inline constexpr float k_desktop_icon_grid_top_inset = 8.0f;
inline constexpr float k_desktop_icon_grid_gap = 24.0f;

inline constexpr std::array<ExplorerSidebarSymbol, 10>
    k_desktop_sidebar_symbols{{
        {"recents", icon_catalog::Symbol::Recents},
        {"shared", icon_catalog::Symbol::Shared},
        {"app", icon_catalog::Symbol::Applications},
        {"desktop", icon_catalog::Symbol::Desktop},
        {"doc", icon_catalog::Symbol::Document},
        {"download", icon_catalog::Symbol::Download},
        {"cloud", icon_catalog::Symbol::Cloud},
        {"home", icon_catalog::Symbol::Home},
        {"airdrop", icon_catalog::Symbol::AirDrop},
        {"trash", icon_catalog::Symbol::Trash},
    }};

inline constexpr std::array<ExplorerSidebarSymbol, 11>
    k_file_type_symbols{{
        {"folder", icon_catalog::Symbol::Folder},
        {"document", icon_catalog::Symbol::Document},
        {"pdf", icon_catalog::Symbol::PdfDocument},
        {"text", icon_catalog::Symbol::TextDocument},
        {"image", icon_catalog::Symbol::Image},
        {"movie", icon_catalog::Symbol::Movie},
        {"archive", icon_catalog::Symbol::Archive},
        {"audio", icon_catalog::Symbol::AudioDocument},
        {"code", icon_catalog::Symbol::CodeDocument},
        {"spreadsheet", icon_catalog::Symbol::SpreadsheetDocument},
        {"presentation", icon_catalog::Symbol::PresentationDocument},
    }};

inline auto desktop_sidebar_symbol_contract() noexcept
        -> std::span<ExplorerSidebarSymbol const> {
    return k_desktop_sidebar_symbols;
}

inline auto file_type_symbol_contract() noexcept
        -> std::span<ExplorerSidebarSymbol const> {
    return k_file_type_symbols;
}

inline auto file_type_icon_source_family() noexcept -> std::string_view {
    return "Google Material Symbols";
}

inline auto file_type_icon_source_revision() noexcept -> std::string_view {
    return icon_catalog::material_symbols_source_revision();
}

inline auto file_type_icon_license_asset_source() noexcept
        -> std::string_view {
    return "assets/icons/file-types/MATERIAL_SYMBOLS_LICENSE.txt";
}

inline auto file_type_icon_asset_policy() noexcept -> std::string_view {
    return "package_runtime_visible_file_type_svg_asset";
}

inline auto file_type_icon_token_for_symbol(
        icon_catalog::Symbol symbol) noexcept -> std::string_view {
    return icon_catalog::file_type_token(symbol);
}

inline auto file_type_icon_asset_name_for_symbol(
        icon_catalog::Symbol symbol) -> std::string {
    return icon_catalog::file_type_package_asset_name(symbol);
}

inline auto file_type_icon_asset_source_for_symbol(
        icon_catalog::Symbol symbol) -> std::string {
    return icon_catalog::file_type_package_asset_source(symbol);
}

inline auto sidebar_symbol_for_token(std::string_view token) noexcept
        -> icon_catalog::Symbol {
    for (auto const& item : desktop_sidebar_symbol_contract()) {
        if (item.token == token)
            return item.symbol;
    }
    return icon_catalog::Symbol::Folder;
}

inline auto sidebar_symbol_name_for_token(std::string_view token) noexcept
        -> std::string_view {
    return icon_catalog::name(sidebar_symbol_for_token(token));
}
inline constexpr char k_desktop_chrome_geometry_policy[] =
    "finder_integrated_glass_chrome_geometry_v1";
inline constexpr char k_desktop_finder_density_policy[] =
    "finder_reference_density_icon_grid_top_inset_v1";

} // namespace file_explorer_demo

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

export module file_explorer_shared;

import json;
import phenotype.icon_catalog;
import phenotype.resources;
import phenotype.theme_contract;

export namespace file_explorer_demo {

namespace fs = std::filesystem;
namespace icon_catalog = phenotype::icon_catalog;
namespace theme_contract = phenotype::theme_contract;

struct Entry {
    std::string name;
    bool folder = false;
    std::uintmax_t size = 0;
};

struct OperationReceipt {
    std::string kind;
    std::string target;
    bool ok = false;
    std::string detail;
};

enum class SortMode {
    Recent,
    Name,
    Kind,
    Size,
};

enum class ExplorerViewMode {
    Icon,
    List,
    Column,
    Gallery,
};

enum class ExplorerSelectionMove {
    Left,
    Right,
    Up,
    Down,
    PageUp,
    PageDown,
    Home,
    End,
};

struct Snapshot {
    fs::path root;
    fs::path current;
    std::string relative_location;
    std::vector<Entry> entries;
    Entry selected{};
    bool has_selection = false;
    std::size_t selected_index = 0;
    bool can_go_back = false;
    bool can_go_forward = false;
    bool can_create_file = false;
    bool can_create_folder = false;
    bool can_delete_selected = false;
    bool can_duplicate_selected = false;
    bool can_preview_selected = false;
    std::string selected_kind_label;
    std::string selected_size_label;
    std::string selected_path_label;
    std::string item_summary;
    std::string action_summary;
    std::string operation_label;
    std::string sort_label;
    std::string preview;
    SortMode sort_mode = SortMode::Name;
    ExplorerViewMode view_mode = ExplorerViewMode::Icon;
    std::size_t file_count = 0;
    std::size_t folder_count = 0;
};

struct ExplorerState {
    fs::path root;
    fs::path current;
    std::string selected_name;
    std::string search;
    std::string draft_name = "New Note.txt";
    std::string draft_folder_name = "New Folder";
    std::string draft_body = "Created from the phenotype file explorer demo.";
    std::string status = "Ready";
    std::vector<fs::path> back_stack;
    std::vector<fs::path> forward_stack;
    OperationReceipt last_operation;
    SortMode sort_mode = SortMode::Name;
    ExplorerViewMode view_mode = ExplorerViewMode::Icon;
    int viewport_width = 0;
    int viewport_height = 0;
    float viewport_scale = 1.0f;
    std::size_t mobile_tab = 0;
};

enum class ExplorerInputKind {
    Noop,
    SelectLocation,
    SelectEntry,
    Search,
    FocusSearch,
    MoveSelection,
    OpenEntry,
    ActivateEntry,
    ActivateSelected,
    ViewMode,
    Viewport,
    DraftName,
    DraftFolderName,
    DraftBody,
    CreateFile,
    CreateFolder,
    DeleteSelected,
    DuplicateSelected,
    GoBack,
    GoForward,
    GoUp,
    Sort,
    CycleSort,
    Reset,
    Scenario,
    DismissTransient,
};

struct ExplorerInput {
    ExplorerInputKind kind = ExplorerInputKind::Noop;
    std::string value;
    SortMode sort_mode = SortMode::Name;
    ExplorerViewMode view_mode = ExplorerViewMode::Icon;
    ExplorerSelectionMove selection_move = ExplorerSelectionMove::Down;
    int viewport_width = 0;
    int viewport_height = 0;
    float viewport_scale = 1.0f;
};

struct ExplorerInputParseResult {
    bool ok = false;
    ExplorerInput input{};
    std::string error;
};

struct ExplorerInputSequenceParseResult {
    bool ok = false;
    std::vector<ExplorerInput> inputs;
    std::string error;
    std::size_t line = 0;
};

struct ExplorerViewport {
    int width = 0;
    int height = 0;
    float scale = 1.0f;
};

struct ExplorerKeyboardCommand {
    std::string action;
    std::string shortcut;
    std::string input_alias;
    bool allow_when_input_focused = false;
};

struct ExplorerChromeMetrics {
    ExplorerViewport viewport{};
    float integrated_titlebar_height = 0.0f;
    float window_content_inset = 0.0f;
    float window_gap = 0.0f;
    float toolbar_shell_x = 0.0f;
    float toolbar_shell_y = 0.0f;
    float toolbar_shell_width = 0.0f;
    float toolbar_shell_height = 0.0f;
    float toolbar_group_y = 0.0f;
    float toolbar_navigation_group_x = 0.0f;
    float toolbar_title_x = 0.0f;
    float toolbar_view_group_x = 0.0f;
    float toolbar_sort_group_x = 0.0f;
    float toolbar_action_group_x = 0.0f;
    float toolbar_search_group_x = 0.0f;
    float content_surface_x = 0.0f;
    float content_surface_y = 0.0f;
    float content_surface_width = 0.0f;
    float sidebar_surface_x = 0.0f;
    float sidebar_surface_y = 0.0f;
    float sidebar_first_row_y = 0.0f;
    float sidebar_width = 0.0f;
    float sidebar_row_width = 0.0f;
    float sidebar_row_height = 0.0f;
    float sidebar_heading_height = 0.0f;
    float sidebar_icon_size = 0.0f;
    float sidebar_icon_leading = 0.0f;
    float sidebar_label_leading = 0.0f;
    float sidebar_label_top = 0.0f;
    float sidebar_heading_label_leading = 0.0f;
    float sidebar_heading_label_top = 0.0f;
    float sidebar_section_gap = 0.0f;
    float sidebar_selected_row_radius = 0.0f;
    int sidebar_selected_row_background_alpha = 0;
    int sidebar_selected_row_hover_background_alpha = 0;
    std::string sidebar_selection_policy;
    float toolbar_group_height = 0.0f;
    float toolbar_group_radius = 0.0f;
    float toolbar_icon_button_width = 0.0f;
    float toolbar_icon_button_height = 0.0f;
    float titlebar_control_cluster_height = 0.0f;
    float titlebar_control_diameter = 0.0f;
    float titlebar_control_spacing = 0.0f;
    float titlebar_control_start_x = 0.0f;
    float titlebar_control_top = 0.0f;
    float titlebar_drag_region_height = 0.0f;
    float leading_control_reserved_width = 0.0f;
    float trailing_control_reserved_width = 0.0f;
    float window_radius = 0.0f;
    float icon_grid_column_width = 0.0f;
    float icon_grid_row_height = 0.0f;
    float icon_grid_column_pitch = 0.0f;
    float icon_grid_thumbnail_width = 0.0f;
    float icon_grid_thumbnail_height = 0.0f;
    float thumbnail_pdf_page_width = 0.0f;
    float thumbnail_pdf_page_height = 0.0f;
    float thumbnail_pdf_page_radius = 0.0f;
    float thumbnail_pdf_fold_size = 0.0f;
    float thumbnail_media_preview_width = 0.0f;
    float thumbnail_media_preview_height = 0.0f;
    float thumbnail_media_preview_radius = 0.0f;
    int thumbnail_pdf_detail_line_count = 0;
    int thumbnail_image_sample_block_count = 0;
    int thumbnail_video_strip_count = 0;
    float icon_grid_label_height = 0.0f;
    float icon_grid_label_font_size = 0.0f;
    float icon_grid_gap = 0.0f;
    float icon_grid_scroll_height = 0.0f;
    int icon_grid_columns = 0;
    int icon_grid_visible_rows = 0;
    int icon_grid_visible_capacity = 0;
    int toolbar_group_count = 0;
    int toolbar_separator_count = 0;
    int toolbar_icon_button_count = 0;
    int column_location_row_count = 0;
    int titlebar_control_count = 0;
    int overflow_action_button_count = 0;
    int icon_total_symbol_count = 0;
    int sidebar_symbol_count = 0;
    int toolbar_symbol_count = 0;
    int icon_filled_symbol_count = 0;
    int icon_outline_symbol_count = 0;
    int icon_hierarchical_symbol_count = 0;
    int icon_monochrome_symbol_count = 0;
    int icon_regular_weight_symbol_count = 0;
    int icon_palette_symbol_count = 0;
    int icon_multicolor_symbol_count = 0;
    int icon_reference_symbol_count = 0;
    int icon_svg_path_arc_symbol_count = 0;
    int icon_round_stroke_symbol_count = 0;
    float icon_grid_size = 0.0f;
    float icon_default_stroke_width = 0.0f;
    float icon_secondary_opacity = 0.0f;
    float icon_toolbar_point_size = 0.0f;
    float icon_sidebar_point_size = 0.0f;
    float icon_sidebar_optical_y_offset = 0.0f;
    float icon_toolbar_hit_target_size = 0.0f;
    float icon_sidebar_hit_target_size = 0.0f;
    float icon_action_hit_target_size = 0.0f;
    float icon_toolbar_button_radius = 0.0f;
    int icon_toolbar_button_background_alpha = 0;
    int icon_toolbar_button_hover_background_alpha = 0;
    int icon_toolbar_selected_button_background_alpha = 0;
    int icon_toolbar_selected_button_hover_background_alpha = 0;
    float column_location_row_height = 0.0f;
    float column_location_icon_size = 0.0f;
    bool integrated_titlebar = true;
    bool native_window_controls = true;
    bool duplicate_window_controls = false;
    bool content_window_control_markers = false;
    bool finder_segmented_toolbar = false;
    bool owned_icon_assets = false;
    bool uses_sf_symbols_assets = false;
    bool icon_round_stroke_contract = false;
    bool icon_text_weight_aligned = false;
    bool icon_monochrome_rendering = false;
    bool icon_hierarchical_opacity = false;
    bool icon_palette_rendering = false;
    bool icon_multicolor_rendering = false;
    bool thumbnail_uses_external_previews = false;
    bool artifact_window_control_markers = false;
    bool more_actions_open = false;
    bool status_bar_visible = false;
    int theme_contract_version = 0;
    std::string icon_module;
    std::string icon_style;
    std::string icon_source_format;
    std::string icon_svg_subset_policy;
    std::string icon_svg_supported_path_commands;
    std::string icon_svg_supported_style_attributes;
    std::string icon_svg_arc_policy;
    std::string icon_stroke_geometry_policy;
    std::string icon_stroke_cap_policy;
    std::string icon_stroke_join_policy;
    std::string icon_design_reference;
    std::string icon_reference_family;
    std::string icon_reference_policy;
    std::string icon_asset_policy;
    std::string icon_interface_metaphor_policy;
    std::string icon_visual_consistency_policy;
    std::string icon_alignment;
    std::string icon_rendering_mode;
    std::string icon_default_weight;
    std::string icon_rendering_capability_policy;
    std::string icon_variant_policy;
    std::string icon_presentation_policy;
    std::string icon_tone_policy;
    std::string icon_interaction_tone_policy;
    std::string icon_symbol_control_chrome_policy;
    std::string icon_toolbar_symbol_chrome_policy;
    std::string icon_sidebar_symbol_color_policy;
    std::string icon_file_type_color_policy;
    std::string icon_metrics_policy;
    std::string icon_hit_target_policy;
    std::string icon_scale;
    std::string thumbnail_visual_policy;
    std::string thumbnail_asset_policy;
    std::string thumbnail_pdf_policy;
    std::string thumbnail_image_policy;
    std::string thumbnail_video_policy;
    std::string thumbnail_shadow_policy;
    std::string theme_profile_name;
    std::string theme_reference;
    std::string theme_font_policy;
    std::string theme_material_policy;
    std::string theme_iconography_policy;
    std::string theme_icon_asset_policy;
    std::string theme_usage_policy;
    std::string theme_container_policy;
    std::string theme_performance_policy;
    std::string theme_accessibility_policy;
    std::string theme_fallback_policy;
    std::string chrome_geometry_policy;
    std::string window_control_marker_mode;
};

struct ExplorerInputTrace {
    ExplorerInput input{};
    ExplorerChromeMetrics chrome{};
    std::string status;
    std::string relative_location;
    std::string selected_name;
    std::string operation_label;
    OperationReceipt operation;
    std::size_t visible_entries = 0;
    bool has_selection = false;
};

struct ExplorerDriveResult {
    std::string profile;
    ExplorerState state;
    Snapshot snapshot;
    ExplorerChromeMetrics chrome{};
    std::vector<ExplorerInputTrace> trace;
};

enum class ExplorerExpectationKind {
    Selected,
    Location,
    Entry,
    MissingEntry,
    Operation,
    StatusContains,
};

struct ExplorerExpectation {
    ExplorerExpectationKind kind = ExplorerExpectationKind::Selected;
    std::string value;
    bool expects_operation_ok = false;
    bool operation_ok = false;
};

struct ExplorerExpectationParseResult {
    bool ok = false;
    ExplorerExpectation expectation{};
    std::string error;
};

struct ExplorerExpectationResult {
    ExplorerExpectation expectation{};
    bool ok = false;
    std::string actual;
    std::string detail;
};

struct PackageResourceText {
    std::string source;
    std::string text;
};

struct ExplorerSidebarSymbol {
    std::string_view token;
    icon_catalog::Symbol symbol = icon_catalog::Symbol::Folder;
};

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
inline constexpr int k_desktop_sidebar_selected_row_background_alpha = 238;
inline constexpr int k_desktop_sidebar_selected_row_hover_background_alpha = 248;
inline constexpr char k_desktop_sidebar_selection_policy[] =
    "finder_soft_selected_row_no_outline_accent_symbol";
inline constexpr int k_desktop_column_location_row_count = 4;
inline constexpr float k_desktop_column_location_row_height = 30.0f;
inline constexpr float k_desktop_column_location_icon_size = 18.0f;
inline constexpr float k_desktop_window_radius = 18.0f;
inline constexpr float k_desktop_toolbar_group_radius = 22.0f;
inline constexpr float k_desktop_toolbar_group_height = 46.0f;
inline constexpr float k_desktop_toolbar_shell_height = 52.0f;
inline constexpr float k_desktop_toolbar_shell_padding = 4.0f;
inline constexpr float k_desktop_toolbar_navigation_group_width = 92.0f;
inline constexpr float k_desktop_toolbar_view_group_width = 216.0f;
inline constexpr float k_desktop_toolbar_sort_group_width = 48.0f;
inline constexpr float k_desktop_toolbar_action_group_width = 128.0f;
inline constexpr float k_desktop_toolbar_search_collapsed_width = 48.0f;
inline constexpr float k_desktop_toolbar_icon_button_width = 38.0f;
inline constexpr float k_desktop_toolbar_icon_button_height = 36.0f;
inline constexpr float k_desktop_titlebar_control_cluster_height = 52.0f;
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
inline constexpr char k_desktop_thumbnail_video_policy[] =
    "wide_video_preview_with_filmstrip_and_content_bands";
inline constexpr char k_desktop_thumbnail_shadow_policy[] =
    "subtle_windowserver_like_drop_shadow";
inline constexpr float k_desktop_icon_grid_label_height = 46.0f;
inline constexpr float k_desktop_icon_grid_label_font_size = 14.0f;
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

inline auto desktop_sidebar_symbol_contract() noexcept
        -> std::span<ExplorerSidebarSymbol const> {
    return k_desktop_sidebar_symbols;
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

inline bool mobile_profile(std::string_view profile) {
    return profile == "mobile";
}

inline SortMode default_sort_mode(std::string_view profile) {
    return mobile_profile(profile) ? SortMode::Name : SortMode::Recent;
}

inline ExplorerViewport default_viewport(std::string_view profile) {
    return mobile_profile(profile)
        ? ExplorerViewport{
            .width = k_mobile_default_viewport_width,
            .height = k_mobile_default_viewport_height,
            .scale = 2.0f,
        }
        : ExplorerViewport{
            .width = k_desktop_default_viewport_width,
            .height = k_desktop_default_viewport_height,
            .scale = 1.0f,
        };
}

inline ExplorerViewport effective_viewport(
        ExplorerState const& state,
        std::string_view profile) {
    auto viewport = default_viewport(profile);
    if (state.viewport_width > 0)
        viewport.width = state.viewport_width;
    if (state.viewport_height > 0)
        viewport.height = state.viewport_height;
    if (state.viewport_scale > 0.0f)
        viewport.scale = state.viewport_scale;
    return viewport;
}

inline void apply_default_viewport(
        ExplorerState& state,
        std::string_view profile) {
    auto viewport = default_viewport(profile);
    state.viewport_width = viewport.width;
    state.viewport_height = viewport.height;
    state.viewport_scale = viewport.scale;
}

inline float desktop_scroll_height_for_viewport(
        ExplorerViewport const& viewport,
        float chrome_budget,
        float minimum,
        float maximum) {
    float height = static_cast<float>(viewport.height) - chrome_budget;
    return std::clamp(height, minimum, maximum);
}

inline float desktop_scroll_height(
        ExplorerState const& state,
        float chrome_budget,
        float minimum,
        float maximum) {
    return desktop_scroll_height_for_viewport(
        effective_viewport(state, "desktop"),
        chrome_budget,
        minimum,
        maximum);
}

inline int desktop_icon_grid_column_count(ExplorerViewport const& viewport) {
    float const window_width = viewport.width > 0
        ? static_cast<float>(viewport.width)
        : static_cast<float>(k_desktop_default_viewport_width);
    float const available = std::max(
        k_desktop_icon_grid_column_pitch * 2.0f,
        window_width - k_desktop_sidebar_width - 80.0f);
    int columns = static_cast<int>(
        available / k_desktop_icon_grid_column_pitch);
    return std::clamp(columns, 2, 7);
}

inline float desktop_toolbar_shell_x() noexcept {
    return k_desktop_window_content_inset + k_desktop_sidebar_width
        + k_desktop_window_gap;
}

inline float desktop_toolbar_shell_width(
        ExplorerViewport const& viewport) noexcept {
    float const window_width = viewport.width > 0
        ? static_cast<float>(viewport.width)
        : static_cast<float>(k_desktop_default_viewport_width);
    return std::max(
        0.0f,
        window_width - desktop_toolbar_shell_x()
            - k_desktop_window_content_inset);
}

inline float desktop_toolbar_group_y() noexcept {
    return k_desktop_window_content_inset + k_desktop_toolbar_shell_padding;
}

inline float desktop_toolbar_navigation_group_x() noexcept {
    return desktop_toolbar_shell_x() + k_desktop_toolbar_shell_padding;
}

inline float desktop_toolbar_title_x() noexcept {
    return desktop_toolbar_navigation_group_x()
        + k_desktop_toolbar_navigation_group_width
        + k_desktop_toolbar_shell_padding;
}

inline float desktop_toolbar_search_group_x(
        ExplorerViewport const& viewport) noexcept {
    float const window_width = viewport.width > 0
        ? static_cast<float>(viewport.width)
        : static_cast<float>(k_desktop_default_viewport_width);
    return window_width - k_desktop_window_content_inset
        - k_desktop_toolbar_shell_padding
        - k_desktop_toolbar_search_collapsed_width;
}

inline float desktop_toolbar_action_group_x(
        ExplorerViewport const& viewport) noexcept {
    return desktop_toolbar_search_group_x(viewport)
        - k_desktop_toolbar_shell_padding
        - k_desktop_toolbar_action_group_width;
}

inline float desktop_toolbar_sort_group_x(
        ExplorerViewport const& viewport) noexcept {
    return desktop_toolbar_action_group_x(viewport)
        - k_desktop_toolbar_shell_padding
        - k_desktop_toolbar_sort_group_width;
}

inline float desktop_toolbar_view_group_x(
        ExplorerViewport const& viewport) noexcept {
    return desktop_toolbar_sort_group_x(viewport)
        - k_desktop_toolbar_shell_padding
        - k_desktop_toolbar_view_group_width;
}

inline float desktop_content_surface_y() noexcept {
    return k_desktop_window_content_inset + k_desktop_toolbar_shell_height
        + k_desktop_window_gap;
}

inline float desktop_sidebar_first_row_y() noexcept {
    return k_desktop_window_content_inset + k_desktop_sidebar_material_padding
        + k_desktop_titlebar_control_cluster_height
        + k_desktop_sidebar_item_gap;
}

inline bool desktop_status_bar_visible(ExplorerState const& state) {
    if (!state.selected_name.empty()
        || !state.last_operation.kind.empty()
        || !state.search.empty())
        return true;
    if (state.status == "Ready")
        return false;
    if (state.status.starts_with("Viewport set to ")
        || state.status.starts_with("Switched to ")
        || state.status.starts_with("Sorted by ")
        || state.status.starts_with("Draft "))
        return false;
    return true;
}

inline phenotype::ResourceCatalog file_explorer_resource_catalog(
        std::string_view profile);

inline ExplorerChromeMetrics explorer_chrome_metrics(
        ExplorerState const& state,
        std::string_view profile) {
    auto viewport = effective_viewport(state, profile);
    if (mobile_profile(profile)) {
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
            .icon_grid_gap = 0.0f,
            .icon_grid_scroll_height = 0.0f,
            .icon_grid_columns = 0,
            .icon_grid_visible_rows = 0,
            .icon_grid_visible_capacity = 0,
            .toolbar_group_count = 0,
            .toolbar_separator_count = 0,
            .toolbar_icon_button_count = 0,
            .column_location_row_count = 0,
            .titlebar_control_count = 0,
            .icon_total_symbol_count = 0,
            .sidebar_symbol_count = 0,
            .toolbar_symbol_count = 0,
            .icon_filled_symbol_count = 0,
            .icon_outline_symbol_count = 0,
            .icon_hierarchical_symbol_count = 0,
            .icon_monochrome_symbol_count = 0,
            .icon_regular_weight_symbol_count = 0,
            .icon_palette_symbol_count = 0,
            .icon_multicolor_symbol_count = 0,
            .icon_reference_symbol_count = 0,
            .icon_svg_path_arc_symbol_count = 0,
            .icon_round_stroke_symbol_count = 0,
            .icon_grid_size = 0.0f,
            .icon_default_stroke_width = 0.0f,
            .icon_secondary_opacity = 0.0f,
            .icon_toolbar_point_size = 0.0f,
            .icon_sidebar_point_size = 0.0f,
            .icon_sidebar_optical_y_offset = 0.0f,
            .icon_toolbar_hit_target_size = 0.0f,
            .icon_sidebar_hit_target_size = 0.0f,
            .icon_action_hit_target_size = 0.0f,
            .icon_toolbar_button_radius = 0.0f,
            .icon_toolbar_button_background_alpha = 0,
            .icon_toolbar_button_hover_background_alpha = 0,
            .icon_toolbar_selected_button_background_alpha = 0,
            .icon_toolbar_selected_button_hover_background_alpha = 0,
            .column_location_row_height = 0.0f,
            .column_location_icon_size = 0.0f,
            .integrated_titlebar = false,
            .native_window_controls = false,
            .duplicate_window_controls = false,
            .content_window_control_markers = false,
            .finder_segmented_toolbar = false,
            .owned_icon_assets = false,
            .uses_sf_symbols_assets = false,
            .icon_round_stroke_contract = false,
            .icon_text_weight_aligned = false,
            .icon_monochrome_rendering = false,
            .icon_hierarchical_opacity = false,
            .icon_palette_rendering = false,
            .icon_multicolor_rendering = false,
            .thumbnail_uses_external_previews = false,
            .artifact_window_control_markers = false,
            .status_bar_visible = false,
            .theme_contract_version = 0,
            .icon_module = "text_controls",
            .icon_style = "mobile_text_buttons",
            .icon_source_format = "none",
            .icon_svg_subset_policy = "n/a",
            .icon_svg_supported_path_commands = "n/a",
            .icon_svg_supported_style_attributes = "n/a",
            .icon_svg_arc_policy = "n/a",
            .icon_stroke_geometry_policy = "n/a",
            .icon_stroke_cap_policy = "n/a",
            .icon_stroke_join_policy = "n/a",
            .icon_design_reference = "mobile text controls",
            .icon_reference_family = "n/a",
            .icon_reference_policy = "n/a",
            .icon_asset_policy = "no vector icon assets in mobile profile",
            .icon_interface_metaphor_policy = "n/a",
            .icon_visual_consistency_policy = "n/a",
            .icon_alignment = "n/a",
            .icon_rendering_mode = "n/a",
            .icon_default_weight = "n/a",
            .icon_rendering_capability_policy = "n/a",
            .icon_variant_policy = "n/a",
            .icon_presentation_policy = "n/a",
            .icon_tone_policy = "n/a",
            .icon_interaction_tone_policy = "n/a",
            .icon_symbol_control_chrome_policy = "n/a",
            .icon_toolbar_symbol_chrome_policy = "n/a",
            .icon_sidebar_symbol_color_policy = "n/a",
            .icon_file_type_color_policy = "n/a",
            .icon_metrics_policy = "n/a",
            .icon_hit_target_policy = "n/a",
            .icon_scale = "n/a",
            .thumbnail_visual_policy = "n/a",
            .thumbnail_asset_policy = "n/a",
            .thumbnail_pdf_policy = "n/a",
            .thumbnail_image_policy = "n/a",
            .thumbnail_video_policy = "n/a",
            .thumbnail_shadow_policy = "n/a",
            .theme_profile_name = "n/a",
            .theme_reference = "n/a",
            .theme_font_policy = "n/a",
            .theme_material_policy = "n/a",
            .theme_iconography_policy = "n/a",
            .theme_icon_asset_policy = "n/a",
            .theme_usage_policy = "n/a",
            .theme_container_policy = "n/a",
            .theme_performance_policy = "n/a",
            .theme_accessibility_policy = "n/a",
            .theme_fallback_policy = "n/a",
            .chrome_geometry_policy = "n/a",
            .window_control_marker_mode = "none",
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
        .icon_total_symbol_count =
            static_cast<int>(icon_catalog::all_symbol_count),
        .sidebar_symbol_count =
            static_cast<int>(icon_catalog::sidebar_symbol_count),
        .toolbar_symbol_count =
            static_cast<int>(icon_catalog::toolbar_symbol_count),
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
        .icon_toolbar_button_radius = toolbar_chrome.corner_radius,
        .icon_toolbar_button_background_alpha =
            toolbar_chrome.background_color.a,
        .icon_toolbar_button_hover_background_alpha =
            toolbar_chrome.hover_background_color.a,
        .icon_toolbar_selected_button_background_alpha =
            toolbar_selected_chrome.background_color.a,
        .icon_toolbar_selected_button_hover_background_alpha =
            toolbar_selected_chrome.hover_background_color.a,
        .column_location_row_height = k_desktop_column_location_row_height,
        .column_location_icon_size = k_desktop_column_location_icon_size,
        .integrated_titlebar = true,
        .native_window_controls = true,
        .duplicate_window_controls = false,
        .content_window_control_markers = false,
        .finder_segmented_toolbar = true,
        .owned_icon_assets = true,
        .uses_sf_symbols_assets = false,
        .icon_round_stroke_contract = true,
        .icon_text_weight_aligned = true,
        .icon_monochrome_rendering = true,
        .icon_hierarchical_opacity = true,
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
        .icon_interface_metaphor_policy =
            std::string{icon_catalog::interface_metaphor_policy()},
        .icon_visual_consistency_policy =
            std::string{icon_catalog::visual_consistency_policy()},
        .icon_alignment = std::string{icon_catalog::alignment_policy()},
        .icon_rendering_mode = "hierarchical",
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
        .window_control_marker_mode = "runtime-native-controls",
    };
}

inline ExplorerChromeMetrics explorer_chrome_with_artifact_window_markers(
        ExplorerChromeMetrics chrome) {
    if (!chrome.native_window_controls || chrome.duplicate_window_controls)
        return chrome;
    chrome.content_window_control_markers = true;
    chrome.artifact_window_control_markers = true;
    chrome.window_control_marker_mode = "artifact-probe-marker";
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

inline std::string lower_copy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

inline std::string trim(std::string_view text) {
    auto begin = text.begin();
    auto end = text.end();
    while (begin != end
           && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (begin != end
           && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

inline std::string sanitize_file_name(std::string_view input) {
    std::string cleaned;
    cleaned.reserve(input.size());
    bool previous_space = false;
    for (char ch : input) {
        auto uch = static_cast<unsigned char>(ch);
        bool const ok = std::isalnum(uch) || ch == '-' || ch == '_'
            || ch == '.' || ch == ' ';
        if (!ok)
            continue;
        if (ch == ' ') {
            if (previous_space)
                continue;
            previous_space = true;
        } else {
            previous_space = false;
        }
        cleaned.push_back(ch);
        if (cleaned.size() >= 48)
            break;
    }
    cleaned = trim(cleaned);
    while (!cleaned.empty() && cleaned.front() == '.')
        cleaned.erase(cleaned.begin());
    cleaned = trim(cleaned);
    if (cleaned.empty())
        cleaned = "New Note.txt";
    if (cleaned.find('.') == std::string::npos)
        cleaned += ".txt";
    return cleaned;
}

inline std::string sanitize_folder_name(std::string_view input) {
    std::string cleaned;
    cleaned.reserve(input.size());
    bool previous_space = false;
    for (char ch : input) {
        auto uch = static_cast<unsigned char>(ch);
        bool const ok = std::isalnum(uch) || ch == '-' || ch == '_'
            || ch == ' ';
        if (!ok)
            continue;
        if (ch == ' ') {
            if (previous_space)
                continue;
            previous_space = true;
        } else {
            previous_space = false;
        }
        cleaned.push_back(ch);
        if (cleaned.size() >= 48)
            break;
    }
    cleaned = trim(cleaned);
    while (!cleaned.empty() && cleaned.front() == '.')
        cleaned.erase(cleaned.begin());
    cleaned = trim(cleaned);
    if (cleaned.empty())
        cleaned = "New Folder";
    return cleaned;
}

inline std::string extension_lower(std::string const& name) {
    auto pos = name.rfind('.');
    if (pos == std::string::npos)
        return {};
    auto ext = name.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
}

inline fs::path demo_root(std::string_view profile) {
    std::string name = "phenotype-file-explorer-";
    name += profile;
    std::error_code ec;
    auto base = fs::temp_directory_path(ec);
    if (ec || base.empty()) {
        ec.clear();
        base = fs::current_path(ec);
    }
    if (ec || base.empty())
        base = ".";
    return base / name;
}

inline fs::path trash_path(fs::path const& root) {
    return root / ".Trash";
}

inline void write_file_if_missing(fs::path const& path, std::string_view body) {
    std::error_code ec;
    if (fs::exists(path, ec))
        return;
    ec.clear();
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    out << body;
}

inline void remove_file_if_body_matches(fs::path const& path,
                                        std::string_view expected_body) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || ec)
        return;
    ec.clear();
    auto const size = fs::file_size(path, ec);
    if (ec || size != expected_body.size())
        return;

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return;
    std::string body(expected_body.size(), '\0');
    in.read(body.data(), static_cast<std::streamsize>(body.size()));
    if (in.gcount() != static_cast<std::streamsize>(body.size()))
        return;
    if (body != expected_body)
        return;

    fs::remove(path, ec);
}

inline fs::path ensure_demo_tree(std::string_view profile) {
    std::error_code ec;
    auto root = demo_root(profile);
    fs::create_directories(root / "Documents", ec);
    fs::create_directories(root / "Pictures", ec);
    fs::create_directories(root / "Shared", ec);
    fs::create_directories(trash_path(root), ec);

    write_file_if_missing(
        root / "README.txt",
        "Phenotype File Explorer\n\n"
        "This demo root is isolated under the system temp directory. "
        "Create and delete files here without touching the real home folder.\n");
    write_file_if_missing(
        root / "Application Form 3.pdf",
        "PDF placeholder\n\n"
        "The desktop example renders this sandboxed text file as a Finder-like "
        "document thumbnail while keeping file reads deterministic.\n");
    write_file_if_missing(
        root / "Application Form 2.pdf",
        "PDF placeholder for the Recents probe scene.\n");
    if (profile == "desktop" || profile == "test-model-contract") {
        write_file_if_missing(
            root / "1_필수_중도인출 신청서.pdf",
            "PDF placeholder with Korean filename for font fallback verification.\n");
        write_file_if_missing(
            root / "2_필수_운용지시서.pdf",
            "PDF placeholder with Korean filename for font fallback verification.\n");
        write_file_if_missing(
            root / "작성예시3_선택_DC 탈퇴신청서.pdf",
            "PDF placeholder with Korean filename for font fallback verification.\n");
        write_file_if_missing(
            root / "작성예시2_필수_운용지시서.pdf",
            "PDF placeholder with Korean filename for Finder recents ordering.\n");
        write_file_if_missing(
            root / "작성예시1_필수_중도인출 신청서.pdf",
            "PDF placeholder with Korean filename for Finder recents ordering.\n");
        write_file_if_missing(
            root / "契約書_サンプル.pdf",
            "PDF placeholder with Japanese filename for font fallback verification.\n");
        write_file_if_missing(
            root / "资产证明.pdf",
            "PDF placeholder with Chinese filename for font fallback verification.\n");
    }
    write_file_if_missing(
        root / "※해당 시 필독_①무주택자인 경우.pdf",
        "PDF placeholder matching the Finder-style Korean Recents probe scene.\n");
    write_file_if_missing(
        root / "[카카오] 퇴직금 지급 기준.pdf",
        "PDF placeholder matching the Finder-style Korean Recents probe scene.\n");
    remove_file_if_body_matches(
        root / "Withdrawal Notice.pdf",
        "PDF placeholder for the Recents probe scene.\n");
    remove_file_if_body_matches(
        root / "Operating Guide.pdf",
        "PDF placeholder for the Recents probe scene.\n");
    write_file_if_missing(
        root / "line_2.png",
        "PNG placeholder rendered as a light rounded Finder thumbnail.\n");
    write_file_if_missing(
        root / "line_23.png",
        "PNG placeholder rendered as a light rounded Finder thumbnail.\n");
    write_file_if_missing(
        root / "line_8.png",
        "PNG placeholder rendered as a light rounded Finder thumbnail.\n");
    write_file_if_missing(
        root / "Screen Recording 2025-11-04.mov",
        "MOV placeholder rendered as a wide video thumbnail.\n");
    write_file_if_missing(
        root / "Screen Recording 2025-11-05.mov",
        "MOV placeholder rendered as a wide video thumbnail.\n");
    write_file_if_missing(
        root / "Screen Recording 2025-11-06.mov",
        "MOV placeholder rendered as a wide video thumbnail.\n");
    write_file_if_missing(
        root / "Screen Recording 2025-11-07.mov",
        "MOV placeholder rendered as a wide video thumbnail.\n");
    write_file_if_missing(
        root / "Screenshot 2025-11-05.png",
        "PNG screenshot placeholder for the Finder-like icon grid.\n");
    write_file_if_missing(
        root / "Screenshot 2025-11-06.png",
        "PNG screenshot placeholder for the Finder-like icon grid.\n");
    write_file_if_missing(
        root / "Archive.zip",
        "ZIP placeholder rendered as a text-style fallback thumbnail.\n");
    write_file_if_missing(
        root / "Documents" / "Project Notes.txt",
        "Finder-like desktop layout\n"
        "- Sidebar locations\n"
        "- File list\n"
        "- Preview pane\n"
        "- Create and delete actions\n");
    write_file_if_missing(
        root / "Documents" / "Budget.txt",
        "Q2 prototype budget\nDesign: 12\nEngineering: 24\nQA: 8\n");
    write_file_if_missing(
        root / "Pictures" / "Glass Backdrop.txt",
        "Image placeholder for the mobile and desktop file explorer demos.\n");
    write_file_if_missing(
        root / "Shared" / "Team Checklist.txt",
        "Review artifact bundle\nVerify mobile layout\nKeep file operations sandboxed\n");
    return root;
}

inline std::string format_size(std::uintmax_t size) {
    if (size < 1024)
        return std::to_string(size) + " B";
    std::uintmax_t const kib = (size + 1023) / 1024;
    if (kib < 1024)
        return std::to_string(kib) + " KB";
    std::uintmax_t const mib = (kib + 1023) / 1024;
    return std::to_string(mib) + " MB";
}

inline std::string entry_kind_label(Entry const& entry) {
    if (entry.folder)
        return "Folder";
    auto ext = extension_lower(entry.name);
    if (ext == "pdf")
        return "PDF Document";
    if (ext == "png" || ext == "jpg" || ext == "jpeg")
        return "Image";
    if (ext == "mov" || ext == "mp4")
        return "Movie";
    if (ext == "zip")
        return "Archive";
    if (ext.empty())
        return "Document";
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return ext + " File";
}

inline std::string entry_size_label(Entry const& entry) {
    return entry.folder ? "--" : format_size(entry.size);
}

inline std::string sort_mode_label(SortMode mode) {
    switch (mode) {
        case SortMode::Recent: return "Recent";
        case SortMode::Kind: return "Kind";
        case SortMode::Size: return "Size";
        case SortMode::Name:
        default: return "Name";
    }
}

inline std::string sort_mode_value_name(SortMode mode) {
    switch (mode) {
        case SortMode::Recent: return "recent";
        case SortMode::Kind: return "kind";
        case SortMode::Size: return "size";
        case SortMode::Name:
        default: return "name";
    }
}

inline std::string view_mode_value_name(ExplorerViewMode mode) {
    switch (mode) {
        case ExplorerViewMode::List: return "list";
        case ExplorerViewMode::Column: return "column";
        case ExplorerViewMode::Gallery: return "gallery";
        case ExplorerViewMode::Icon:
        default: return "icon";
    }
}

inline std::string view_mode_label(ExplorerViewMode mode) {
    switch (mode) {
        case ExplorerViewMode::List: return "List View";
        case ExplorerViewMode::Column: return "Column View";
        case ExplorerViewMode::Gallery: return "Gallery View";
        case ExplorerViewMode::Icon:
        default: return "Icon View";
    }
}

inline std::string selection_move_value_name(ExplorerSelectionMove move) {
    switch (move) {
        case ExplorerSelectionMove::Left: return "left";
        case ExplorerSelectionMove::Right: return "right";
        case ExplorerSelectionMove::Up: return "up";
        case ExplorerSelectionMove::Down: return "down";
        case ExplorerSelectionMove::PageUp: return "page-up";
        case ExplorerSelectionMove::PageDown: return "page-down";
        case ExplorerSelectionMove::Home: return "home";
        case ExplorerSelectionMove::End: return "end";
    }
    return "down";
}

inline std::string selection_move_label(ExplorerSelectionMove move) {
    switch (move) {
        case ExplorerSelectionMove::Left: return "Arrow Left";
        case ExplorerSelectionMove::Right: return "Arrow Right";
        case ExplorerSelectionMove::Up: return "Arrow Up";
        case ExplorerSelectionMove::Down: return "Arrow Down";
        case ExplorerSelectionMove::PageUp: return "Page Up";
        case ExplorerSelectionMove::PageDown: return "Page Down";
        case ExplorerSelectionMove::Home: return "Home";
        case ExplorerSelectionMove::End: return "End";
    }
    return "Arrow Down";
}

inline ExplorerViewMode view_mode_from_name(std::string_view value) {
    auto mode = lower_copy(trim(value));
    if (mode == "list")
        return ExplorerViewMode::List;
    if (mode == "column")
        return ExplorerViewMode::Column;
    if (mode == "gallery")
        return ExplorerViewMode::Gallery;
    return ExplorerViewMode::Icon;
}

inline bool known_view_mode_name(std::string_view value) {
    auto mode = lower_copy(trim(value));
    return mode == "icon" || mode == "list" || mode == "column"
        || mode == "gallery";
}

inline std::optional<ExplorerSelectionMove> selection_move_from_name(
        std::string_view value) {
    auto move = lower_copy(trim(value));
    if (move == "left" || move == "arrow-left" || move == "arrow_left")
        return ExplorerSelectionMove::Left;
    if (move == "right" || move == "arrow-right" || move == "arrow_right")
        return ExplorerSelectionMove::Right;
    if (move == "up" || move == "arrow-up" || move == "arrow_up")
        return ExplorerSelectionMove::Up;
    if (move == "down" || move == "arrow-down" || move == "arrow_down")
        return ExplorerSelectionMove::Down;
    if (move == "page-up" || move == "page_up" || move == "pageup")
        return ExplorerSelectionMove::PageUp;
    if (move == "page-down" || move == "page_down" || move == "pagedown")
        return ExplorerSelectionMove::PageDown;
    if (move == "home")
        return ExplorerSelectionMove::Home;
    if (move == "end")
        return ExplorerSelectionMove::End;
    return std::nullopt;
}

inline json::Value entry_debug_json(Entry const& entry) {
    json::Object out;
    out.emplace("name", json::Value{entry.name});
    out.emplace("kind", json::Value{entry_kind_label(entry)});
    out.emplace("folder", json::Value{entry.folder});
    out.emplace("size", json::Value{static_cast<std::int64_t>(entry.size)});
    return json::Value{std::move(out)};
}

inline json::Value operation_receipt_debug_json(
        OperationReceipt const& receipt,
        std::string const& label) {
    json::Object out;
    out.emplace("kind", json::Value{receipt.kind});
    out.emplace("target", json::Value{receipt.target});
    out.emplace("ok", json::Value{receipt.ok});
    out.emplace("detail", json::Value{receipt.detail});
    out.emplace("label", json::Value{label});
    return json::Value{std::move(out)};
}

inline std::vector<ExplorerKeyboardCommand> file_explorer_keyboard_commands(
        std::string_view profile) {
    if (profile == "mobile")
        return {};
    return {
        {"show_search", "CommandOrControl+F", "shortcut:find", true},
        {"activate_selected", "Enter", "key:enter", false},
        {"move_selection_up", "ArrowUp", "key:arrow-up", false},
        {"move_selection_down", "ArrowDown", "key:arrow-down", false},
        {"move_selection_left", "ArrowLeft", "key:arrow-left", false},
        {"move_selection_right", "ArrowRight", "key:arrow-right", false},
        {"move_selection_home", "Home", "key:home", false},
        {"move_selection_end", "End", "key:end", false},
        {"move_selection_page_up", "PageUp", "key:page-up", false},
        {"move_selection_page_down", "PageDown", "key:page-down", false},
        {"delete_selected", "DeleteOrBackspace", "key:delete", false},
        {"duplicate_selected", "CommandOrControl+D", "shortcut:duplicate", false},
        {"create_folder", "Shift+CommandOrControl+N", "shortcut:new-folder", false},
        {"dismiss_transient", "Escape", "key:escape", true},
    };
}

inline json::Value keyboard_commands_debug_json(std::string_view profile) {
    json::Array commands;
    for (auto const& command : file_explorer_keyboard_commands(profile)) {
        json::Object item;
        item.emplace("action", json::Value{command.action});
        item.emplace("shortcut", json::Value{command.shortcut});
        item.emplace("input_alias", json::Value{command.input_alias});
        item.emplace(
            "allow_when_input_focused",
            json::Value{command.allow_when_input_focused});
        commands.push_back(json::Value{std::move(item)});
    }
    return json::Value{std::move(commands)};
}

inline json::Value string_array_debug_json(
        std::initializer_list<std::string_view> values) {
    json::Array out;
    for (auto value : values)
        out.push_back(json::Value{std::string{value}});
    return json::Value{std::move(out)};
}

inline json::Value sidebar_icon_reference_symbols_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Array{}};
    json::Array out;
    for (unsigned int i = 0; i < icon_catalog::sidebar_symbol_count; ++i) {
        out.push_back(json::Value{std::string{
            icon_catalog::semantic_reference_name(
                icon_catalog::sidebar_symbol_at(i))}});
    }
    return json::Value{std::move(out)};
}

inline json::Value sidebar_symbol_tokens_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Array{}};
    json::Array out;
    for (auto const& item : desktop_sidebar_symbol_contract()) {
        json::Object token;
        token.emplace("token", json::Value{std::string{item.token}});
        token.emplace(
            "symbol",
            json::Value{std::string{icon_catalog::name(item.symbol)}});
        token.emplace(
            "semantic_reference_name",
            json::Value{std::string{
                icon_catalog::semantic_reference_name(item.symbol)}});
        out.push_back(json::Value{std::move(token)});
    }
    return json::Value{std::move(out)};
}

inline json::Value toolbar_icon_reference_symbols_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Array{}};
    json::Array out;
    for (unsigned int i = 0; i < icon_catalog::toolbar_symbol_count; ++i) {
        out.push_back(json::Value{std::string{
            icon_catalog::semantic_reference_name(
                icon_catalog::toolbar_symbol_at(i))}});
    }
    return json::Value{std::move(out)};
}

inline auto interaction_tone_name(icon_catalog::SymbolPresentationRole role,
                                  bool selected,
                                  bool enabled) -> std::string {
    return std::string{icon_catalog::symbol_tone_name(
        icon_catalog::macos_interaction_tone(
            role,
            icon_catalog::SymbolInteractionState{selected, enabled}))};
}

inline json::Value icon_interaction_tones_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_interaction_tone_policy == "n/a")
        return json::Value{json::Object{}};
    json::Object out;
    out.emplace(
        "sidebar_selected",
        json::Value{interaction_tone_name(
            icon_catalog::SymbolPresentationRole::Sidebar, true, true)});
    out.emplace(
        "sidebar_unselected",
        json::Value{interaction_tone_name(
            icon_catalog::SymbolPresentationRole::Sidebar, false, true)});
    out.emplace(
        "toolbar_selected",
        json::Value{interaction_tone_name(
            icon_catalog::SymbolPresentationRole::Toolbar, true, true)});
    out.emplace(
        "toolbar_unselected",
        json::Value{interaction_tone_name(
            icon_catalog::SymbolPresentationRole::Toolbar, false, true)});
    out.emplace(
        "disabled",
        json::Value{interaction_tone_name(
            icon_catalog::SymbolPresentationRole::Toolbar, false, false)});
    return json::Value{std::move(out)};
}

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
    native_window.emplace("content_window_control_markers", json::Value{chrome.content_window_control_markers});
    native_window.emplace("artifact_window_control_markers", json::Value{chrome.artifact_window_control_markers});
    native_window.emplace("window_control_marker_mode", json::Value{chrome.window_control_marker_mode});
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

    json::Object icon_system;
    icon_system.emplace("module", json::Value{chrome.icon_module});
    icon_system.emplace("style", json::Value{chrome.icon_style});
    icon_system.emplace("source_format", json::Value{chrome.icon_source_format});
    icon_system.emplace("svg_subset_policy", json::Value{chrome.icon_svg_subset_policy});
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
    icon_system.emplace("uses_sf_symbols_assets", json::Value{chrome.uses_sf_symbols_assets});
    icon_system.emplace("round_stroke_contract", json::Value{chrome.icon_round_stroke_contract});
    icon_system.emplace(
        "total_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_total_symbol_count)});
    icon_system.emplace(
        "sidebar_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.sidebar_symbol_count)});
    icon_system.emplace(
        "toolbar_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.toolbar_symbol_count)});
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
        "svg_path_arc_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_svg_path_arc_symbol_count)});
    icon_system.emplace(
        "round_stroke_symbol_count",
        json::Value{static_cast<std::int64_t>(chrome.icon_round_stroke_symbol_count)});
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
        "toolbar_reference_symbols",
        toolbar_icon_reference_symbols_debug_json(chrome));

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
    out.emplace("overflow_action_button_count", json::Value{static_cast<std::int64_t>(chrome.overflow_action_button_count)});
    out.emplace("sidebar_symbol_count", json::Value{static_cast<std::int64_t>(chrome.sidebar_symbol_count)});
    out.emplace("toolbar_symbol_count", json::Value{static_cast<std::int64_t>(chrome.toolbar_symbol_count)});
    out.emplace("content_window_control_markers", json::Value{chrome.content_window_control_markers});
    out.emplace("artifact_window_control_markers", json::Value{chrome.artifact_window_control_markers});
    out.emplace("window_control_marker_mode", json::Value{chrome.window_control_marker_mode});
    out.emplace("finder_segmented_toolbar", json::Value{chrome.finder_segmented_toolbar});
    out.emplace("more_actions_open", json::Value{chrome.more_actions_open});
    out.emplace("status_bar_visible", json::Value{chrome.status_bar_visible});
    out.emplace("native_window", json::Value{std::move(native_window)});
    out.emplace("geometry", json::Value{std::move(geometry)});
    out.emplace("icon_system", json::Value{std::move(icon_system)});
    return json::Value{std::move(out)};
}

inline json::Value explorer_theme_system_debug_json(
        ExplorerChromeMetrics const& chrome) {
    json::Object out;
    out.emplace(
        "contract_version",
        json::Value{static_cast<std::int64_t>(
            chrome.theme_contract_version)});
    out.emplace("profile_name", json::Value{chrome.theme_profile_name});
    out.emplace("reference", json::Value{chrome.theme_reference});
    out.emplace("font_policy", json::Value{chrome.theme_font_policy});
    out.emplace("material_policy", json::Value{chrome.theme_material_policy});
    out.emplace(
        "iconography_policy",
        json::Value{chrome.theme_iconography_policy});
    out.emplace(
        "icon_asset_policy",
        json::Value{chrome.theme_icon_asset_policy});
    out.emplace("usage_policy", json::Value{chrome.theme_usage_policy});
    out.emplace("container_policy", json::Value{chrome.theme_container_policy});
    out.emplace(
        "performance_policy",
        json::Value{chrome.theme_performance_policy});
    out.emplace(
        "accessibility_policy",
        json::Value{chrome.theme_accessibility_policy});
    out.emplace("fallback_policy", json::Value{chrome.theme_fallback_policy});
    return json::Value{std::move(out)};
}

inline auto resource_contract_locale_keys(
        phenotype::ResourceCatalog const& catalog)
        -> std::vector<std::string_view> {
    auto out = std::vector<std::string_view>{};
    if (auto locale = phenotype::find_locale(catalog, catalog.default_locale)) {
        out.reserve(locale->get().strings.size());
        for (auto const& text : locale->get().strings)
            out.push_back(text.key);
    }
    return out;
}

inline json::Value string_vector_debug_json(
        std::span<std::string const> values) {
    json::Array out;
    for (auto const& value : values)
        out.push_back(json::Value{value});
    return json::Value{std::move(out)};
}

inline json::Value locale_coverage_debug_json(
        std::span<phenotype::LocaleCoverage const> coverage) {
    json::Array out;
    for (auto const& locale : coverage) {
        json::Object item;
        item.emplace("tag", json::Value{locale.tag});
        item.emplace("default_locale", json::Value{locale.default_locale});
        item.emplace(
            "fallback_chain",
            string_vector_debug_json(locale.fallback_chain));
        item.emplace(
            "declared_string_count",
            json::Value{static_cast<std::int64_t>(
                locale.declared_string_count)});
        item.emplace(
            "required_key_count",
            json::Value{static_cast<std::int64_t>(
                locale.required_key_count)});
        item.emplace(
            "resolved_key_count",
            json::Value{static_cast<std::int64_t>(
                locale.resolved_key_count)});
        item.emplace(
            "missing_keys",
            string_vector_debug_json(locale.missing_keys));
        out.push_back(json::Value{std::move(item)});
    }
    return json::Value{std::move(out)};
}

inline json::Value file_explorer_resource_system_debug_json(
        std::string_view profile) {
    auto catalog = file_explorer_resource_catalog(profile);
    auto required_keys = resource_contract_locale_keys(catalog);
    auto contract = phenotype::resource_catalog_contract(
        catalog,
        std::span<std::string_view const>{required_keys});

    json::Array platforms;
    for (auto const& platform : catalog.application.platforms)
        platforms.push_back(json::Value{platform});

    json::Object application;
    application.emplace("id", json::Value{catalog.application.id});
    application.emplace(
        "display_name",
        json::Value{catalog.application.display_name});
    application.emplace("version", json::Value{catalog.application.version});
    application.emplace("entry", json::Value{catalog.application.entry});
    application.emplace("platforms", json::Value{std::move(platforms)});

    json::Object app_icon;
    app_icon.emplace("declared", json::Value{contract.app_icon_declared});
    app_icon.emplace("svg", json::Value{contract.app_icon_svg});
    app_icon.emplace("preload", json::Value{contract.app_icon_preload});
    if (auto asset = phenotype::find_asset(catalog, "app.icon")) {
        app_icon.emplace("source", json::Value{asset->get().source});
        app_icon.emplace(
            "content_type",
            json::Value{asset->get().content_type});
    }

    json::Object defaults;
    defaults.emplace("locale", json::Value{catalog.default_locale});
    defaults.emplace(
        "default_locale_declared",
        json::Value{contract.default_locale_declared});
    defaults.emplace("font_family", json::Value{catalog.default_font_family});
    defaults.emplace(
        "default_font_declared",
        json::Value{contract.default_font_declared});
    defaults.emplace(
        "default_font_has_cjk_fallback",
        json::Value{contract.default_font_has_cjk_fallback});

    json::Object locales;
    locales.emplace(
        "count",
        json::Value{static_cast<std::int64_t>(contract.locale_count)});
    locales.emplace(
        "string_count",
        json::Value{static_cast<std::int64_t>(contract.locale_string_count)});
    locales.emplace(
        "required_key_count",
        json::Value{static_cast<std::int64_t>(required_keys.size())});
    locales.emplace(
        "coverage",
        locale_coverage_debug_json(contract.locale_coverage));

    json::Object fonts;
    fonts.emplace(
        "count",
        json::Value{static_cast<std::int64_t>(contract.font_count)});
    fonts.emplace(
        "registered_count",
        json::Value{static_cast<std::int64_t>(
            contract.registered_font_count)});

    json::Object debug;
    debug.emplace(
        "artifact_manifest",
        json::Value{catalog.debug.artifact_manifest});
    debug.emplace("probe_scene", json::Value{catalog.debug.probe_scene});
    debug.emplace("verifier", json::Value{catalog.debug.verifier});
    debug.emplace(
        "artifact_manifest_declared",
        json::Value{contract.debug_artifact_manifest_declared});
    debug.emplace(
        "probe_scene_declared",
        json::Value{contract.debug_probe_scene_declared});
    debug.emplace(
        "verifier_declared",
        json::Value{contract.debug_verifier_declared});

    json::Object out;
    out.emplace("schema_version", json::Value{1});
    out.emplace("application", json::Value{std::move(application)});
    out.emplace(
        "asset_count",
        json::Value{static_cast<std::int64_t>(contract.asset_count)});
    out.emplace(
        "preload_asset_count",
        json::Value{static_cast<std::int64_t>(
            contract.preload_asset_count)});
    out.emplace(
        "runtime_visible_asset_count",
        json::Value{static_cast<std::int64_t>(
            contract.runtime_visible_asset_count)});
    out.emplace(
        "svg_asset_count",
        json::Value{static_cast<std::int64_t>(contract.svg_asset_count)});
    out.emplace(
        "preload_svg_asset_count",
        json::Value{static_cast<std::int64_t>(
            contract.preload_svg_asset_count)});
    out.emplace(
        "runtime_visible_svg_asset_count",
        json::Value{static_cast<std::int64_t>(
            contract.runtime_visible_svg_asset_count)});
    out.emplace(
        "svg_asset_policy",
        json::Value{std::string{phenotype::svg_asset_contract_policy()}});
    out.emplace("app_icon", json::Value{std::move(app_icon)});
    out.emplace("defaults", json::Value{std::move(defaults)});
    out.emplace("locales", json::Value{std::move(locales)});
    out.emplace("fonts", json::Value{std::move(fonts)});
    out.emplace("debug", json::Value{std::move(debug)});
    return json::Value{std::move(out)};
}

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
    out.emplace("theme_system", explorer_theme_system_debug_json(chrome));
    out.emplace("resource_system", file_explorer_resource_system_debug_json(profile));
    out.emplace("keyboard_commands", keyboard_commands_debug_json(profile));
    out.emplace("entries_sample", json::Value{std::move(entries)});
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

inline SortMode next_sort_mode(SortMode mode) {
    switch (mode) {
        case SortMode::Recent: return SortMode::Name;
        case SortMode::Name: return SortMode::Kind;
        case SortMode::Kind: return SortMode::Size;
        case SortMode::Size:
        default: return SortMode::Name;
    }
}

inline std::string explorer_input_kind_name(ExplorerInputKind kind) {
    switch (kind) {
        case ExplorerInputKind::Noop: return "noop";
        case ExplorerInputKind::SelectLocation: return "select_location";
        case ExplorerInputKind::SelectEntry: return "select_entry";
        case ExplorerInputKind::Search: return "search";
        case ExplorerInputKind::FocusSearch: return "focus_search";
        case ExplorerInputKind::MoveSelection: return "move_selection";
        case ExplorerInputKind::OpenEntry: return "open_entry";
        case ExplorerInputKind::ActivateEntry: return "activate_entry";
        case ExplorerInputKind::ActivateSelected: return "activate_selected";
        case ExplorerInputKind::ViewMode: return "view_mode";
        case ExplorerInputKind::Viewport: return "viewport";
        case ExplorerInputKind::DraftName: return "draft_name";
        case ExplorerInputKind::DraftFolderName: return "draft_folder_name";
        case ExplorerInputKind::DraftBody: return "draft_body";
        case ExplorerInputKind::CreateFile: return "create_file";
        case ExplorerInputKind::CreateFolder: return "create_folder";
        case ExplorerInputKind::DeleteSelected: return "delete_selected";
        case ExplorerInputKind::DuplicateSelected: return "duplicate_selected";
        case ExplorerInputKind::GoBack: return "go_back";
        case ExplorerInputKind::GoForward: return "go_forward";
        case ExplorerInputKind::GoUp: return "go_up";
        case ExplorerInputKind::Sort: return "sort";
        case ExplorerInputKind::CycleSort: return "cycle_sort";
        case ExplorerInputKind::Reset: return "reset";
        case ExplorerInputKind::Scenario: return "scenario";
        case ExplorerInputKind::DismissTransient: return "dismiss_transient";
    }
    return "noop";
}

inline std::string explorer_input_label(ExplorerInput const& input) {
    auto label = explorer_input_kind_name(input.kind);
    if (input.kind == ExplorerInputKind::Sort)
        return label + ":" + sort_mode_label(input.sort_mode);
    if (input.kind == ExplorerInputKind::ViewMode)
        return label + ":" + view_mode_value_name(input.view_mode);
    if (input.kind == ExplorerInputKind::MoveSelection)
        return label + ":" + selection_move_value_name(input.selection_move);
    if (!input.value.empty())
        return label + ":" + input.value;
    return label;
}

inline std::string explorer_expectation_kind_name(ExplorerExpectationKind kind) {
    switch (kind) {
        case ExplorerExpectationKind::Selected:       return "selected";
        case ExplorerExpectationKind::Location:       return "location";
        case ExplorerExpectationKind::Entry:          return "entry";
        case ExplorerExpectationKind::MissingEntry:   return "missing-entry";
        case ExplorerExpectationKind::Operation:      return "operation";
        case ExplorerExpectationKind::StatusContains: return "status-contains";
    }
    return "selected";
}

inline std::string explorer_expectation_label(
        ExplorerExpectation const& expectation) {
    auto label = explorer_expectation_kind_name(expectation.kind) + ":"
        + expectation.value;
    if (expectation.kind == ExplorerExpectationKind::Operation
        && expectation.expects_operation_ok) {
        label += expectation.operation_ok ? ":ok" : ":fail";
    }
    return label;
}

inline ExplorerExpectationParseResult parsed_expectation(
        ExplorerExpectation expectation) {
    return ExplorerExpectationParseResult{
        .ok = true,
        .expectation = std::move(expectation),
    };
}

inline ExplorerExpectationParseResult expectation_parse_error(
        std::string message) {
    return ExplorerExpectationParseResult{
        .ok = false,
        .error = std::move(message),
    };
}

inline ExplorerExpectationParseResult parse_explorer_expectation(
        std::string_view raw) {
    auto text = trim(raw);
    auto separator = text.find(':');
    if (separator == std::string::npos)
        separator = text.find('=');
    if (text.empty() || separator == std::string::npos) {
        return expectation_parse_error(
            "expectation requires kind:value, such as selected:README.txt");
    }

    auto name = lower_copy(trim(std::string_view{text}.substr(0, separator)));
    auto value = trim(std::string_view{text}.substr(separator + 1));
    if (value.empty()) {
        return expectation_parse_error(
            "expectation '" + name + "' requires a value");
    }

    if (name == "selected" || name == "selection") {
        return parsed_expectation({
            .kind = ExplorerExpectationKind::Selected,
            .value = value,
        });
    }
    if (name == "location" || name == "loc") {
        return parsed_expectation({
            .kind = ExplorerExpectationKind::Location,
            .value = value,
        });
    }
    if (name == "entry" || name == "has-entry" || name == "has_entry") {
        return parsed_expectation({
            .kind = ExplorerExpectationKind::Entry,
            .value = value,
        });
    }
    if (name == "missing-entry" || name == "missing_entry"
        || name == "no-entry" || name == "no_entry") {
        return parsed_expectation({
            .kind = ExplorerExpectationKind::MissingEntry,
            .value = value,
        });
    }
    if (name == "status" || name == "status-contains"
        || name == "status_contains") {
        return parsed_expectation({
            .kind = ExplorerExpectationKind::StatusContains,
            .value = value,
        });
    }
    if (name == "operation" || name == "op") {
        auto op_kind = value;
        auto expected_ok = std::optional<bool>{};
        auto op_separator = op_kind.rfind(':');
        if (op_separator != std::string::npos) {
            auto suffix = lower_copy(trim(
                std::string_view{op_kind}.substr(op_separator + 1)));
            if (suffix == "ok" || suffix == "success" || suffix == "true") {
                expected_ok = true;
                op_kind = trim(std::string_view{op_kind}.substr(0, op_separator));
            } else if (suffix == "fail" || suffix == "failure"
                       || suffix == "false") {
                expected_ok = false;
                op_kind = trim(std::string_view{op_kind}.substr(0, op_separator));
            }
        }
        if (op_kind.empty()) {
            return expectation_parse_error(
                "expectation 'operation' requires an operation kind");
        }
        return parsed_expectation({
            .kind = ExplorerExpectationKind::Operation,
            .value = op_kind,
            .expects_operation_ok = expected_ok.has_value(),
            .operation_ok = expected_ok.value_or(false),
        });
    }

    return expectation_parse_error("unknown file explorer expectation: " + name);
}

inline std::optional<int> parse_positive_int(std::string_view text) {
    auto trimmed = trim(text);
    if (trimmed.empty())
        return std::nullopt;
    char* end = nullptr;
    long value = std::strtol(trimmed.c_str(), &end, 10);
    if (end != trimmed.c_str() + trimmed.size() || value <= 0 || value > 100000)
        return std::nullopt;
    return static_cast<int>(value);
}

inline std::optional<float> parse_positive_float(std::string_view text) {
    auto trimmed = trim(text);
    if (trimmed.empty())
        return std::nullopt;
    char* end = nullptr;
    float value = std::strtof(trimmed.c_str(), &end);
    if (end != trimmed.c_str() + trimmed.size() || value <= 0.0f || value > 16.0f)
        return std::nullopt;
    return value;
}

inline std::string viewport_value_label(int width, int height, float scale) {
    std::ostringstream out;
    out << width << "x" << height;
    if (scale != 1.0f)
        out << "@" << scale;
    return out.str();
}

inline SortMode sort_mode_from_name(std::string_view value) {
    auto name = lower_copy(trim(value));
    if (name == "recent" || name == "recents")
        return SortMode::Recent;
    if (name == "kind")
        return SortMode::Kind;
    if (name == "size")
        return SortMode::Size;
    return SortMode::Name;
}

inline ExplorerInputParseResult parsed_input(ExplorerInput input) {
    return ExplorerInputParseResult{
        .ok = true,
        .input = std::move(input),
    };
}

inline ExplorerInputParseResult input_parse_error(std::string message) {
    return ExplorerInputParseResult{
        .ok = false,
        .error = std::move(message),
    };
}

inline ExplorerInputParseResult parse_viewport_input(std::string_view value) {
    auto text = trim(value);
    auto scale = 1.0f;
    auto scale_separator = text.find('@');
    if (scale_separator != std::string::npos) {
        auto parsed_scale = parse_positive_float(
            std::string_view{text}.substr(scale_separator + 1));
        if (!parsed_scale) {
            return input_parse_error(
                "input 'viewport' scale must be a positive number");
        }
        scale = *parsed_scale;
        text = trim(std::string_view{text}.substr(0, scale_separator));
    }

    auto separator = text.find('x');
    if (separator == std::string::npos)
        separator = text.find('X');
    if (separator == std::string::npos)
        separator = text.find(',');
    if (separator == std::string::npos) {
        return input_parse_error(
            "input 'viewport' requires WIDTHxHEIGHT or WIDTHxHEIGHT@SCALE");
    }

    auto width = parse_positive_int(std::string_view{text}.substr(0, separator));
    auto height = parse_positive_int(std::string_view{text}.substr(separator + 1));
    if (!width || !height) {
        return input_parse_error(
            "input 'viewport' width and height must be positive integers");
    }

    return parsed_input({
        .kind = ExplorerInputKind::Viewport,
        .value = viewport_value_label(*width, *height, scale),
        .viewport_width = *width,
        .viewport_height = *height,
        .viewport_scale = scale,
    });
}

inline ExplorerInputParseResult parse_explorer_input(std::string_view raw) {
    auto text = trim(raw);
    if (text.empty())
        return parsed_input({});

    auto separator = text.find(':');
    if (separator == std::string::npos)
        separator = text.find('=');
    auto name = lower_copy(trim(separator == std::string::npos
        ? std::string_view{text}
        : std::string_view{text}.substr(0, separator)));
    auto value = separator == std::string::npos
        ? std::string{}
        : trim(std::string_view{text}.substr(separator + 1));

    auto require_value = [&](ExplorerInputKind kind) -> ExplorerInputParseResult {
        if (value.empty()) {
            return input_parse_error(
                "input '" + name + "' requires a value");
        }
        return parsed_input(ExplorerInput{
            .kind = kind,
            .value = value,
        });
    };

    auto parse_move = [&](std::string_view move_name) -> ExplorerInputParseResult {
        auto move = selection_move_from_name(move_name);
        if (!move) {
            return input_parse_error(
                "unknown file explorer selection move: "
                + std::string{move_name});
        }
        return parsed_input(ExplorerInput{
            .kind = ExplorerInputKind::MoveSelection,
            .value = selection_move_value_name(*move),
            .selection_move = *move,
        });
    };

    if (name == "noop")
        return parsed_input({});
    if (name == "location" || name == "loc"
        || name == "select-location") {
        return require_value(ExplorerInputKind::SelectLocation);
    }
    if (name == "select" || name == "entry" || name == "select-entry"
        || name == "read") {
        return require_value(ExplorerInputKind::SelectEntry);
    }
    if (name == "open" || name == "open-entry")
        return require_value(ExplorerInputKind::OpenEntry);
    if (name == "activate" || name == "click" || name == "activate-entry")
        return require_value(ExplorerInputKind::ActivateEntry);
    if (name == "open-selected" || name == "open_selection"
        || name == "activate-selected" || name == "activate_selection"
        || name == "enter") {
        return parsed_input({.kind = ExplorerInputKind::ActivateSelected});
    }
    if (name == "move" || name == "move-selection"
        || name == "move_selection" || name == "navigate") {
        if (value.empty()) {
            return input_parse_error(
                "input '" + name + "' requires a selection move");
        }
        return parse_move(value);
    }
    if (name == "key" || name == "shortcut") {
        auto command = lower_copy(trim(value));
        if (command.empty()) {
            return input_parse_error(
                "input '" + name + "' requires a command name");
        }
        if (command == "enter" || command == "return"
            || command == "open" || command == "activate") {
            return parsed_input({.kind = ExplorerInputKind::ActivateSelected});
        }
        if (command == "delete" || command == "backspace"
            || command == "trash") {
            return parsed_input({.kind = ExplorerInputKind::DeleteSelected});
        }
        if (command == "find" || command == "search") {
            return parsed_input({.kind = ExplorerInputKind::FocusSearch});
        }
        if (auto move = selection_move_from_name(command)) {
            return parsed_input(ExplorerInput{
                .kind = ExplorerInputKind::MoveSelection,
                .value = selection_move_value_name(*move),
                .selection_move = *move,
            });
        }
        if (command == "escape" || command == "dismiss") {
            return parsed_input({.kind = ExplorerInputKind::DismissTransient});
        }
        if (command == "duplicate" || command == "copy") {
            return parsed_input({.kind = ExplorerInputKind::DuplicateSelected});
        }
        if (command == "new-folder" || command == "new_folder"
            || command == "mkdir") {
            return parsed_input({.kind = ExplorerInputKind::CreateFolder});
        }
        if (command == "new-file" || command == "new_file"
            || command == "touch") {
            return parsed_input({.kind = ExplorerInputKind::CreateFile});
        }
        return input_parse_error(
            "unknown file explorer " + name + " command: " + command);
    }
    if (name == "search")
        return require_value(ExplorerInputKind::Search);
    if (name == "view" || name == "view-mode" || name == "view_mode"
        || name == "mode") {
        if (value.empty())
            return input_parse_error(
                "input 'view' requires icon, list, column, or gallery");
        if (!known_view_mode_name(value)) {
            return input_parse_error(
                "input 'view' accepts only icon, list, column, or gallery");
        }
        auto mode = view_mode_from_name(value);
        return parsed_input({
            .kind = ExplorerInputKind::ViewMode,
            .value = view_mode_value_name(mode),
            .view_mode = mode,
        });
    }
    if (name == "viewport" || name == "resize" || name == "size") {
        if (value.empty())
            return input_parse_error("input 'viewport' requires a value");
        return parse_viewport_input(value);
    }
    if (name == "draft-name" || name == "file-name" || name == "name")
        return require_value(ExplorerInputKind::DraftName);
    if (name == "draft-folder" || name == "folder-name")
        return require_value(ExplorerInputKind::DraftFolderName);
    if (name == "draft-body" || name == "body" || name == "contents")
        return require_value(ExplorerInputKind::DraftBody);
    if (name == "create-file" || name == "touch")
        return parsed_input({.kind = ExplorerInputKind::CreateFile});
    if (name == "create-folder" || name == "mkdir")
        return parsed_input({.kind = ExplorerInputKind::CreateFolder});
    if (name == "delete" || name == "trash" || name == "delete-selected")
        return parsed_input({.kind = ExplorerInputKind::DeleteSelected});
    if (name == "duplicate" || name == "copy")
        return parsed_input({.kind = ExplorerInputKind::DuplicateSelected});
    if (name == "back")
        return parsed_input({.kind = ExplorerInputKind::GoBack});
    if (name == "forward")
        return parsed_input({.kind = ExplorerInputKind::GoForward});
    if (name == "up")
        return parsed_input({.kind = ExplorerInputKind::GoUp});
    if (name == "sort") {
        if (value.empty())
            return input_parse_error(
                "input 'sort' requires recent, name, kind, or size");
        auto lowered = lower_copy(value);
        if (lowered != "recent" && lowered != "recents"
            && lowered != "name" && lowered != "kind" && lowered != "size") {
            return input_parse_error(
                "input 'sort' accepts only recent, name, kind, or size");
        }
        return parsed_input({
            .kind = ExplorerInputKind::Sort,
            .value = value,
            .sort_mode = sort_mode_from_name(value),
        });
    }
    if (name == "cycle-sort")
        return parsed_input({.kind = ExplorerInputKind::CycleSort});
    if (name == "reset")
        return parsed_input({.kind = ExplorerInputKind::Reset});
    if (name == "scenario")
        return require_value(ExplorerInputKind::Scenario);

    return input_parse_error("unknown file explorer input: " + name);
}

inline ExplorerInputSequenceParseResult parse_explorer_input_lines(
        std::string_view text,
        std::string_view source_label = "file explorer input script") {
    auto result = ExplorerInputSequenceParseResult{};
    auto lines = std::istringstream{std::string{text}};
    auto line = std::string{};
    auto line_number = std::size_t{0};
    while (std::getline(lines, line)) {
        ++line_number;
        auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.starts_with("#"))
            continue;
        if (trimmed.starts_with("input "))
            trimmed = trim(std::string_view{trimmed}.substr(6));
        auto parsed = parse_explorer_input(trimmed);
        if (!parsed.ok) {
            result.ok = false;
            result.line = line_number;
            result.error = std::string{source_label}
                + ":" + std::to_string(line_number)
                + ": " + parsed.error;
            return result;
        }
        result.inputs.push_back(std::move(parsed.input));
    }
    result.ok = true;
    return result;
}

inline ExplorerInputSequenceParseResult parse_explorer_input_sequence(
        std::string_view text,
        std::string_view source_label = "file explorer input sequence") {
    auto normalized = std::string{text};
    std::replace(normalized.begin(), normalized.end(), ';', '\n');
    return parse_explorer_input_lines(normalized, source_label);
}

inline void record_operation(
        ExplorerState& state,
        std::string kind,
        std::string target,
        bool ok,
        std::string detail) {
    state.last_operation = OperationReceipt{
        .kind = std::move(kind),
        .target = std::move(target),
        .ok = ok,
        .detail = std::move(detail),
    };
}

inline std::string operation_label(OperationReceipt const& receipt) {
    if (receipt.kind.empty())
        return {};
    std::string label = "Operation: " + receipt.kind;
    label += receipt.ok ? " ok" : " failed";
    if (!receipt.target.empty())
        label += " - " + receipt.target;
    if (!receipt.detail.empty())
        label += " - " + receipt.detail;
    return label;
}

inline std::string relative_location(fs::path const& root, fs::path const& current) {
    std::error_code ec;
    auto const trash = trash_path(root);
    if (fs::equivalent(current, trash, ec))
        return "Trash";
    ec.clear();
    auto trash_rel = fs::relative(current, trash, ec);
    if (!ec && !trash_rel.empty() && trash_rel != ".") {
        auto text = trash_rel.generic_string();
        if (text != ".." && !text.starts_with("../"))
            return std::string("Trash/") + text;
    }
    ec.clear();
    auto rel = fs::relative(current, root, ec);
    if (ec || rel.empty() || rel == ".")
        return "Demo Root";
    return std::string("Demo Root/") + rel.generic_string();
}

inline bool same_path(fs::path const& a, fs::path const& b) {
    if (a.empty() || b.empty())
        return a.empty() == b.empty();

    std::error_code ec;
    if (fs::equivalent(a, b, ec))
        return true;

    auto lhs = a.lexically_normal().generic_string();
    auto rhs = b.lexically_normal().generic_string();
#if defined(_WIN32)
    lhs = lower_copy(std::move(lhs));
    rhs = lower_copy(std::move(rhs));
#endif
    return lhs == rhs;
}

inline void trim_history(std::vector<fs::path>& history) {
    constexpr std::size_t max_history = 32;
    if (history.size() > max_history) {
        history.erase(history.begin(),
                      history.begin()
                          + static_cast<std::ptrdiff_t>(
                              history.size() - max_history));
    }
}

inline void remember_current_for_back(ExplorerState& state) {
    if (state.current.empty())
        return;
    if (state.back_stack.empty()
        || !same_path(state.back_stack.back(), state.current)) {
        state.back_stack.push_back(state.current);
        trim_history(state.back_stack);
    }
    state.forward_stack.clear();
}

inline void open_directory(
        ExplorerState& state,
        fs::path next,
        std::string status,
        bool record_history = true) {
    if (!same_path(state.current, next)) {
        if (record_history)
            remember_current_for_back(state);
        state.current = std::move(next);
    }
    state.selected_name.clear();
    state.status = std::move(status);
}

inline bool matches_filter(std::string const& name, std::string const& filter) {
    auto needle = trim(filter);
    if (needle.empty())
        return true;
    return lower_copy(name).find(lower_copy(needle)) != std::string::npos;
}

inline bool protected_root_folder(std::string_view name) {
    return name == "Documents" || name == "Pictures" || name == "Shared"
        || name == ".Trash";
}

inline int finder_recents_rank(std::string_view name) {
    if (name == "작성예시3_선택_DC 탈퇴신청서.pdf")
        return 0;
    if (name == "작성예시2_필수_운용지시서.pdf")
        return 1;
    if (name == "작성예시1_필수_중도인출 신청서.pdf")
        return 2;
    if (name == "2_필수_운용지시서.pdf")
        return 3;
    if (name == "1_필수_중도인출 신청서.pdf")
        return 4;
    if (name == "※해당 시 필독_①무주택자인 경우.pdf")
        return 5;
    if (name == "[카카오] 퇴직금 지급 기준.pdf")
        return 6;
    if (name == "line_2.png")
        return 7;
    if (name == "line_23.png")
        return 8;
    if (name == "line_8.png")
        return 9;
    if (name == "Screen Recording 2025-11-04.mov")
        return 10;
    if (name == "Screen Recording 2025-11-05.mov")
        return 11;
    if (name == "Screen Recording 2025-11-06.mov")
        return 12;
    if (name == "Screen Recording 2025-11-07.mov")
        return 13;
    if (name == "Screenshot 2025-11-05.png")
        return 14;
    if (name == "Screenshot 2025-11-06.png")
        return 15;
    if (name == "README.txt")
        return 16;
    if (name == "Application Form 3.pdf")
        return 17;
    if (name == "Application Form 2.pdf")
        return 18;
    if (name == "Archive.zip")
        return 19;
    if (name == "契約書_サンプル.pdf")
        return 20;
    if (name == "资产证明.pdf")
        return 21;
    return 1000;
}

inline bool entry_name_less(Entry const& a, Entry const& b) {
    return lower_copy(a.name) < lower_copy(b.name);
}

inline bool entry_less(Entry const& a, Entry const& b, SortMode sort_mode) {
    if (sort_mode == SortMode::Recent) {
        auto lhs_rank = finder_recents_rank(a.name);
        auto rhs_rank = finder_recents_rank(b.name);
        if (lhs_rank != rhs_rank)
            return lhs_rank < rhs_rank;
        if (a.folder != b.folder)
            return !a.folder && b.folder;
        return entry_name_less(a, b);
    }
    if (a.folder != b.folder)
        return a.folder && !b.folder;
    if (sort_mode == SortMode::Kind) {
        auto lhs_kind = lower_copy(entry_kind_label(a));
        auto rhs_kind = lower_copy(entry_kind_label(b));
        if (lhs_kind != rhs_kind)
            return lhs_kind < rhs_kind;
    } else if (sort_mode == SortMode::Size && !a.folder && !b.folder) {
        if (a.size != b.size)
            return a.size > b.size;
    }
    return entry_name_less(a, b);
}

inline void sort_entries(std::vector<Entry>& entries, SortMode sort_mode) {
    for (std::size_t i = 1; i < entries.size(); ++i) {
        auto value = std::move(entries[i]);
        auto j = i;
        while (j > 0 && entry_less(value, entries[j - 1], sort_mode)) {
            entries[j] = std::move(entries[j - 1]);
            --j;
        }
        entries[j] = std::move(value);
    }
}

inline bool path_inside_root(fs::path const& root, fs::path const& path) {
    auto rel = path.lexically_relative(root).generic_string();
    if (rel.empty() || rel == ".")
        return false;
    return rel != ".." && !rel.starts_with("../");
}

inline bool direct_child_entry_name(std::string_view name) {
    auto text = trim(name);
    if (text.empty() || text != name || text == "." || text == "..")
        return false;
    if (text.front() == '.')
        return false;
    return text.find('/') == std::string::npos
        && text.find('\\') == std::string::npos;
}

inline std::optional<fs::path> direct_child_entry_path(
        ExplorerState const& state,
        std::string_view name) {
    if (!direct_child_entry_name(name))
        return std::nullopt;
    auto path = (state.current / std::string{name}).lexically_normal();
    if (!path_inside_root(state.root, path))
        return std::nullopt;
    return path;
}

inline bool deletable_directory(
        fs::path const& root,
        fs::path const& path) {
    std::error_code ec;
    if (!fs::is_directory(path, ec) || ec)
        return false;
    if (!path_inside_root(root, path) || same_path(root, path))
        return false;
    if (same_path(path.parent_path(), root)
        && protected_root_folder(path.filename().string())) {
        return false;
    }
    return true;
}

inline std::vector<Entry> list_entries(
        fs::path const& current,
        std::string const& filter,
        SortMode sort_mode = SortMode::Name) {
    std::vector<Entry> entries;
    std::error_code ec;
    fs::directory_iterator it(current, ec);
    if (ec)
        return entries;
    for (auto const& item : it) {
        auto name = item.path().filename().string();
        if (!name.empty() && name.front() == '.')
            continue;
        if (name.empty() || !matches_filter(name, filter))
            continue;
        Entry entry;
        entry.name = name;
        ec.clear();
        entry.folder = item.is_directory(ec);
        if (ec)
            continue;
        if (!entry.folder) {
            ec.clear();
            if (!item.is_regular_file(ec) || ec)
                continue;
            ec.clear();
            entry.size = item.file_size(ec);
            if (ec)
                entry.size = 0;
        }
        entries.push_back(entry);
    }
    sort_entries(entries, sort_mode);
    return entries;
}

inline std::string read_preview(fs::path const& path) {
    std::error_code ec;
    if (!fs::exists(path, ec))
        return "No file selected.";
    if (fs::is_directory(path, ec))
        return "Open this folder to view its files.";
    auto size = fs::file_size(path, ec);
    if (ec)
        return "Unable to read file size.";
    if (size > 64 * 1024)
        return "Preview skipped because the file is larger than 64 KB.";

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return "Unable to open file for preview.";
    std::ostringstream buffer;
    buffer << in.rdbuf();
    auto text = buffer.str();
    for (char& ch : text) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (ch == '\n' || ch == '\t')
            continue;
        if (uch < 32 || uch == 127)
            ch = ' ';
    }
    if (text.empty())
        return "(empty file)";
    if (text.size() > 1800)
        text = text.substr(0, 1800) + "\n...";
    return text;
}

inline Snapshot snapshot(ExplorerState const& state) {
    Snapshot out;
    out.root = state.root;
    out.current = state.current;
    out.relative_location = relative_location(state.root, state.current);
    out.can_go_back = !state.back_stack.empty();
    out.can_go_forward = !state.forward_stack.empty();
    std::error_code ec;
    out.can_create_file = fs::is_directory(state.current, ec) && !ec;
    out.can_create_folder = out.can_create_file;
    out.entries = list_entries(state.current, state.search, state.sort_mode);
    out.operation_label = operation_label(state.last_operation);
    out.sort_mode = state.sort_mode;
    out.sort_label = "Sort: " + sort_mode_label(state.sort_mode);
    out.view_mode = state.view_mode;
    for (std::size_t i = 0; i < out.entries.size(); ++i) {
        auto const& entry = out.entries[i];
        if (entry.folder)
            ++out.folder_count;
        else
            ++out.file_count;
        if (entry.name == state.selected_name) {
            out.selected = entry;
            out.has_selection = true;
            out.selected_index = i;
        }
    }
    out.item_summary = std::to_string(out.file_count) + " files";
    if (out.folder_count > 0)
        out.item_summary += ", " + std::to_string(out.folder_count) + " folders";
    if (out.has_selection) {
        auto const selected_path = state.current / out.selected.name;
        out.can_delete_selected = out.selected.folder
            ? deletable_directory(state.root, selected_path)
            : true;
        out.can_duplicate_selected = !out.selected.folder;
        out.can_preview_selected = true;
        out.selected_kind_label = entry_kind_label(out.selected);
        out.selected_size_label = entry_size_label(out.selected);
        out.selected_path_label =
            out.relative_location + "/" + out.selected.name;
        out.action_summary = "Selected " + out.selected.name + " - "
            + out.selected_kind_label + " - " + out.selected_size_label;
        out.preview = read_preview(state.current / out.selected.name);
    } else {
        out.action_summary = "Select a file to read, duplicate, or delete it.";
        out.preview = "Select a file to read its contents.";
    }
    return out;
}

inline ExplorerState make_state(std::string_view profile) {
    ExplorerState state;
    state.root = ensure_demo_tree(profile);
    state.current = state.root;
    state.selected_name.clear();
    state.sort_mode = default_sort_mode(profile);
    apply_default_viewport(state, profile);
    return state;
}

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
        "Opened " + relative_location(state.root, next));
}

inline void go_up(ExplorerState& state) {
    if (state.current == state.root) {
        state.status = "Already at the demo root.";
        return;
    }
    auto next = state.current.parent_path();
    open_directory(
        state,
        next,
        "Moved up to " + relative_location(state.root, next));
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
        "Went back to " + relative_location(state.root, next),
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
        "Went forward to " + relative_location(state.root, next),
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
        state.selected_name = name;
        state.status = "Selected folder " + name;
        record_operation(
            state,
            "folder_select",
            name,
            true,
            "Selected " + name + " for folder actions");
        return;
    }
    ec.clear();
    if (fs::is_regular_file(*resolved, ec)) {
        state.selected_name = name;
        state.status = "Selected " + name;
        record_operation(
            state,
            "file_read",
            name,
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
        open_directory(state, *resolved, "Opened " + name);
        record_operation(
            state,
            "folder_open",
            name,
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
        open_directory(state, *resolved, "Opened " + name);
        record_operation(
            state,
            "folder_open",
            name,
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
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        state.status = "Could not create " + name;
        record_operation(state, "file_create", name, false, state.status);
        return;
    }
    out << state.draft_body << "\n";
    if (!out) {
        state.status = "Could not write " + name;
        record_operation(state, "file_create", name, false, state.status);
        return;
    }
    state.selected_name = path.filename().string();
    state.status = "Created " + state.selected_name;
    record_operation(
        state,
        "file_create",
        state.selected_name,
        true,
        state.status);
}

inline void create_folder(ExplorerState& state) {
    auto name = sanitize_folder_name(state.draft_folder_name);
    auto path = unique_child_path(state.current, name);
    std::error_code ec;
    if (!fs::create_directory(path, ec) || ec) {
        state.status = "Could not create folder " + name;
        record_operation(state, "folder_create", name, false, state.status);
        return;
    }
    state.selected_name = path.filename().string();
    state.status = "Created folder " + state.selected_name;
    record_operation(
        state,
        "folder_create",
        state.selected_name,
        true,
        state.status);
}

inline void delete_selected(ExplorerState& state) {
    if (state.selected_name.empty()) {
        state.status = "Select a file or folder before deleting.";
        record_operation(state, "file_delete", {}, false, state.status);
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
            state.status = "Only sandbox folders can be deleted.";
            record_operation(
                state,
                "folder_delete",
                state.selected_name,
                false,
                state.status);
            return;
        }
        ec.clear();
        if (deleting_from_trash) {
            auto const removed = fs::remove_all(path, ec);
            if (ec || removed == 0) {
                state.status = "Could not delete folder " + state.selected_name;
                record_operation(
                    state,
                    "folder_delete",
                    state.selected_name,
                    false,
                    state.status);
                return;
            }
            auto deleted = state.selected_name;
            state.status = "Deleted folder " + state.selected_name + " from Trash";
            state.selected_name.clear();
            record_operation(
                state,
                "folder_delete",
                deleted,
                true,
                state.status);
            return;
        }
        fs::create_directories(trash, ec);
        ec.clear();
        auto target = unique_child_path(trash, state.selected_name);
        fs::rename(path, target, ec);
        if (ec) {
            state.status = "Could not move folder " + state.selected_name + " to Trash";
            record_operation(
                state,
                "folder_delete",
                state.selected_name,
                false,
                state.status);
            return;
        }
        auto moved = state.selected_name;
        state.status = "Moved folder " + state.selected_name + " to Trash";
        state.selected_name.clear();
        record_operation(
            state,
            "folder_delete",
            moved,
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
        if (!fs::remove(path, ec) || ec) {
            state.status = "Could not delete " + state.selected_name;
            record_operation(
                state,
                "file_delete",
                state.selected_name,
                false,
                state.status);
            return;
        }
        auto deleted = state.selected_name;
        state.status = "Deleted " + state.selected_name + " from Trash";
        state.selected_name.clear();
        record_operation(
            state,
            "file_delete",
            deleted,
            true,
            state.status);
        return;
    }
    fs::create_directories(trash, ec);
    ec.clear();
    auto target = unique_child_path(trash, state.selected_name);
    fs::rename(path, target, ec);
    if (ec) {
        state.status = "Could not delete " + state.selected_name;
        record_operation(
            state,
            "file_delete",
            state.selected_name,
            false,
            state.status);
        return;
    }
    auto deleted = state.selected_name;
    state.status = "Moved " + state.selected_name + " to Trash";
    state.selected_name.clear();
    record_operation(
        state,
        "file_delete",
        deleted,
        true,
        state.status);
}

inline void duplicate_selected(ExplorerState& state) {
    if (state.selected_name.empty()) {
        state.status = "Select a file before duplicating.";
        record_operation(state, "file_duplicate", {}, false, state.status);
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
    ec.clear();
    if (!fs::copy_file(source, target, fs::copy_options::none, ec) || ec) {
        state.status = "Could not duplicate " + state.selected_name;
        record_operation(
            state,
            "file_duplicate",
            state.selected_name,
            false,
            state.status);
        return;
    }
    auto previous = state.selected_name;
    state.selected_name = target.filename().string();
    state.status = "Duplicated " + previous + " to " + state.selected_name;
    record_operation(
        state,
        "file_duplicate",
        state.selected_name,
        true,
        state.status);
}

inline void reset_demo_tree(ExplorerState& state, std::string_view profile) {
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
    state.status = "Demo files reset.";
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

    state.status = "Unknown startup scenario: " + std::string(scenario);
}

inline void apply_explorer_input(
        ExplorerState& state,
        ExplorerInput const& input,
        std::string_view profile) {
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
        case ExplorerInputKind::Reset:
            reset_demo_tree(state, profile);
            return;
        case ExplorerInputKind::Scenario:
            apply_startup_scenario(state, input.value);
            return;
        case ExplorerInputKind::DismissTransient:
            state.status = "Transient UI dismissed.";
            return;
    }
}

inline void apply_explorer_inputs(
        ExplorerState& state,
        std::span<ExplorerInput const> inputs,
        std::string_view profile) {
    for (auto const& input : inputs)
        apply_explorer_input(state, input, profile);
}

inline ExplorerInputTrace explorer_input_trace(
        ExplorerState const& state,
        ExplorerInput const& input,
        std::string_view profile) {
    auto snap = snapshot(state);
    return ExplorerInputTrace{
        .input = input,
        .chrome = explorer_chrome_metrics(state, profile),
        .status = state.status,
        .relative_location = snap.relative_location,
        .selected_name = state.selected_name,
        .operation_label = snap.operation_label,
        .operation = state.last_operation,
        .visible_entries = snap.entries.size(),
        .has_selection = snap.has_selection,
    };
}

inline ExplorerDriveResult drive_explorer(
        std::string_view profile,
        std::span<ExplorerInput const> inputs) {
    ExplorerDriveResult result;
    result.profile = profile.empty() ? "desktop" : std::string{profile};
    result.state = make_state(result.profile);
    result.trace.reserve(inputs.size());
    for (auto const& input : inputs) {
        apply_explorer_input(result.state, input, result.profile);
        result.trace.push_back(explorer_input_trace(
            result.state,
            input,
            result.profile));
    }
    result.snapshot = snapshot(result.state);
    result.chrome = explorer_chrome_metrics(result.state, result.profile);
    return result;
}

inline bool snapshot_has_entry(Snapshot const& snapshot, std::string_view name) {
    return std::ranges::any_of(snapshot.entries, [&](Entry const& entry) {
        return entry.name == name;
    });
}

inline ExplorerExpectationResult check_explorer_expectation(
        ExplorerDriveResult const& result,
        ExplorerExpectation const& expectation) {
    auto const& snap = result.snapshot;
    auto checked = ExplorerExpectationResult{
        .expectation = expectation,
    };
    switch (expectation.kind) {
        case ExplorerExpectationKind::Selected:
            checked.actual = snap.has_selection ? snap.selected.name : "<none>";
            checked.ok = snap.has_selection && snap.selected.name == expectation.value;
            checked.detail = checked.ok
                ? "selected entry matched"
                : "selected entry did not match";
            return checked;
        case ExplorerExpectationKind::Location:
            checked.actual = snap.relative_location;
            checked.ok = snap.relative_location == expectation.value;
            checked.detail = checked.ok
                ? "location matched"
                : "relative location did not match";
            return checked;
        case ExplorerExpectationKind::Entry:
            checked.actual = snapshot_has_entry(snap, expectation.value)
                ? expectation.value
                : "<missing>";
            checked.ok = checked.actual == expectation.value;
            checked.detail = checked.ok
                ? "entry was visible"
                : "entry was not visible in the final snapshot";
            return checked;
        case ExplorerExpectationKind::MissingEntry:
            checked.actual = snapshot_has_entry(snap, expectation.value)
                ? expectation.value
                : "<missing>";
            checked.ok = checked.actual == "<missing>";
            checked.detail = checked.ok
                ? "entry was absent"
                : "entry was unexpectedly visible in the final snapshot";
            return checked;
        case ExplorerExpectationKind::Operation:
            checked.actual = result.state.last_operation.kind.empty()
                ? "<none>"
                : result.state.last_operation.kind
                    + (result.state.last_operation.ok ? ":ok" : ":fail");
            checked.ok = result.state.last_operation.kind == expectation.value;
            if (expectation.expects_operation_ok) {
                checked.ok = checked.ok
                    && result.state.last_operation.ok == expectation.operation_ok;
            }
            checked.detail = checked.ok
                ? "last operation matched"
                : "last operation did not match";
            return checked;
        case ExplorerExpectationKind::StatusContains:
            checked.actual = result.state.status;
            checked.ok = result.state.status.find(expectation.value)
                != std::string::npos;
            checked.detail = checked.ok
                ? "status contained expected text"
                : "status did not contain expected text";
            return checked;
    }
    checked.actual = "<unknown>";
    checked.detail = "unknown expectation";
    return checked;
}

inline std::vector<ExplorerExpectationResult> check_explorer_expectations(
        ExplorerDriveResult const& result,
        std::span<ExplorerExpectation const> expectations) {
    auto checked = std::vector<ExplorerExpectationResult>{};
    checked.reserve(expectations.size());
    for (auto const& expectation : expectations)
        checked.push_back(check_explorer_expectation(result, expectation));
    return checked;
}

inline bool explorer_expectations_ok(
        std::span<ExplorerExpectationResult const> expectations) {
    return std::ranges::all_of(expectations, [](auto const& expectation) {
        return expectation.ok;
    });
}

inline std::string entry_label(Entry const& entry) {
    std::string label = entry.folder ? "[Folder] " : "[File] ";
    label += entry.name;
    return label;
}

struct ExplorerLabels {
    std::string title = "Recents";
    std::string mobile_title = "Files";
    std::string sidebar_recents = "Recents";
    std::string sidebar_shared = "Shared";
    std::string favorites = "Favorites";
    std::string locations = "Locations";
    std::string applications = "Applications";
    std::string desktop = "Desktop";
    std::string documents = "Documents";
    std::string pictures = "Pictures";
    std::string downloads = "Downloads";
    std::string icloud_drive = "iCloud Drive";
    std::string home = "kakao";
    std::string airdrop = "AirDrop";
    std::string trash = "Trash";
    std::string search_placeholder = "Search";
    std::string status_ready = "Ready";
    std::string tab_browse = "Browse";
    std::string tab_preview = "Preview";
    std::string tab_create = "Create";
    std::string create_file = "New File";
    std::string create_folder = "New Folder";
    std::string duplicate = "Duplicate";
    std::string duplicate_file = "Duplicate File";
    std::string delete_action = "Delete";
    std::string delete_selected = "Delete Selected";
    std::string preview = "Preview";
    std::string root = "Root";
    std::string docs = "Docs";
    std::string pics = "Pics";
    std::string back = "Back";
    std::string forward = "Forward";
    std::string up = "Up";
    std::string sort = "Sort";
    std::string name = "Name";
    std::string kind = "Kind";
    std::string size = "Size";
    std::string no_matching_files = "No matching files.";
    std::string select_file_to_preview = "Select a file to preview.";
    std::string select_file_from_browse = "Select a file from Browse.";
    std::string create_hint =
        "Files and folders are written only inside the demo root.";
    std::string file_name = "File name";
    std::string contents = "Contents";
    std::string folder_name = "Folder name";
    std::string reset_demo_files = "Reset Demo Files";
    std::string status = "Status";
};

inline void add_locale_strings(
        phenotype::LocaleDescriptor& locale,
        std::initializer_list<std::pair<std::string_view, std::string_view>> values) {
    for (auto const& [key, value] : values) {
        locale.strings.push_back({
            .key = std::string{key},
            .value = std::string{value},
        });
    }
}

inline phenotype::ResourceCatalog file_explorer_resource_catalog(
        std::string_view profile) {
    phenotype::ResourceCatalog catalog;
    bool const mobile = profile == "mobile";
    catalog.application.id = mobile
        ? "com.misut.phenotype.examples.file-explorer-mobile"
        : "com.misut.phenotype.examples.file-explorer-desktop";
    catalog.application.display_name = mobile
        ? "Phenotype Mobile File Explorer"
        : "Phenotype File Explorer";
    catalog.application.version = "0.1.0";
    catalog.application.entry = mobile
        ? "file_explorer_mobile"
        : "file_explorer_desktop";
    catalog.application.platforms = {"macos", "windows"};
    catalog.default_locale = "en";
    catalog.default_font_family = "Pretendard";
    catalog.assets.push_back({
        .name = "app.icon",
        .source = "assets/file-explorer-icon.svg",
        .content_type = "image/svg+xml",
        .preload = true,
    });

    phenotype::LocaleDescriptor en{
        .tag = "en",
        .source = "locales/en.toml",
    };
    add_locale_strings(en, {
        {"app.title", "Recents"},
        {"app.mobile_title", "Files"},
        {"app.sidebar_recents", "Recents"},
        {"app.sidebar_shared", "Shared"},
        {"app.favorites", "Favorites"},
        {"app.locations", "Locations"},
        {"app.applications", "Applications"},
        {"app.desktop", "Desktop"},
        {"app.documents", "Documents"},
        {"app.pictures", "Pictures"},
        {"app.downloads", "Downloads"},
        {"app.icloud_drive", "iCloud Drive"},
        {"app.home", "kakao"},
        {"app.airdrop", "AirDrop"},
        {"app.trash", "Trash"},
        {"app.search_placeholder", "Search"},
        {"app.status_ready", "Ready"},
        {"app.tab_browse", "Browse"},
        {"app.tab_preview", "Preview"},
        {"app.tab_create", "Create"},
        {"actions.create_file", "New File"},
        {"actions.create_folder", "New Folder"},
        {"actions.duplicate", "Duplicate"},
        {"actions.duplicate_file", "Duplicate File"},
        {"actions.delete", "Delete"},
        {"actions.delete_selected", "Delete Selected"},
        {"actions.preview", "Preview"},
        {"nav.root", "Root"},
        {"nav.docs", "Docs"},
        {"nav.pics", "Pics"},
        {"nav.back", "Back"},
        {"nav.forward", "Forward"},
        {"nav.up", "Up"},
        {"nav.sort", "Sort"},
        {"table.name", "Name"},
        {"table.kind", "Kind"},
        {"table.size", "Size"},
        {"state.no_matching_files", "No matching files."},
        {"state.select_file_to_preview", "Select a file to preview."},
        {"state.select_file_from_browse", "Select a file from Browse."},
        {"create.hint", "Files and folders are written only inside the demo root."},
        {"create.file_name", "File name"},
        {"create.contents", "Contents"},
        {"create.folder_name", "Folder name"},
        {"create.reset_demo_files", "Reset Demo Files"},
        {"status.title", "Status"},
    });
    phenotype::LocaleDescriptor ko{
        .tag = "ko",
        .source = "locales/ko.toml",
        .fallback = {"en"},
    };
    add_locale_strings(ko, {
        {"app.title", "최근 항목"},
        {"app.mobile_title", "파일"},
        {"app.sidebar_recents", "최근 항목"},
        {"app.sidebar_shared", "공유"},
        {"app.favorites", "즐겨찾기"},
        {"app.locations", "위치"},
        {"app.applications", "응용 프로그램"},
        {"app.desktop", "데스크탑"},
        {"app.documents", "문서"},
        {"app.pictures", "사진"},
        {"app.downloads", "다운로드"},
        {"app.icloud_drive", "iCloud Drive"},
        {"app.home", "kakao"},
        {"app.airdrop", "AirDrop"},
        {"app.trash", "휴지통"},
        {"app.search_placeholder", "검색"},
        {"app.status_ready", "준비됨"},
        {"app.tab_browse", "둘러보기"},
        {"app.tab_preview", "미리보기"},
        {"app.tab_create", "만들기"},
        {"actions.create_file", "새 파일"},
        {"actions.create_folder", "새 폴더"},
        {"actions.duplicate", "복제"},
        {"actions.duplicate_file", "파일 복제"},
        {"actions.delete", "삭제"},
        {"actions.delete_selected", "선택 항목 삭제"},
        {"actions.preview", "미리보기"},
        {"nav.root", "루트"},
        {"nav.docs", "문서"},
        {"nav.pics", "사진"},
        {"nav.back", "뒤로"},
        {"nav.forward", "앞으로"},
        {"nav.up", "위로"},
        {"nav.sort", "정렬"},
        {"table.name", "이름"},
        {"table.kind", "종류"},
        {"table.size", "크기"},
        {"state.no_matching_files", "일치하는 파일이 없습니다."},
        {"state.select_file_to_preview", "미리 볼 파일을 선택하세요."},
        {"state.select_file_from_browse", "둘러보기에서 파일을 선택하세요."},
        {"create.hint", "파일과 폴더는 데모 루트 안에만 기록됩니다."},
        {"create.file_name", "파일 이름"},
        {"create.contents", "내용"},
        {"create.folder_name", "폴더 이름"},
        {"create.reset_demo_files", "데모 파일 초기화"},
        {"status.title", "상태"},
    });
    catalog.locales = {std::move(en), std::move(ko)};
    catalog.fonts.push_back({
        .family = "Pretendard",
        .source = "fonts/pretendard.alias.toml",
        .fallback = {"system-ui", "Apple SD Gothic Neo", "Segoe UI", "Noto Sans CJK"},
    });
    catalog.debug.artifact_manifest = "artifact_manifest.json";
    catalog.debug.probe_scene = mobile
        ? "mobile-file-workflow"
        : "finder-style-startup";
    catalog.debug.verifier = "phenotype artifact verify-file-explorer";
    return catalog;
}

inline phenotype::ResourceCatalog file_explorer_resource_catalog_from_package_texts(
        std::string_view profile,
        std::string_view manifest_text,
        std::span<PackageResourceText const> locale_texts) {
    auto fallback = file_explorer_resource_catalog(profile);
    if (trim(manifest_text).empty())
        return fallback;

    auto parsed = phenotype::parse_resource_manifest(manifest_text);
    if (!parsed.application_section || !parsed.resources_section)
        return fallback;

    auto catalog = std::move(parsed.catalog);
    if (catalog.default_locale.empty())
        catalog.default_locale = fallback.default_locale;
    if (catalog.default_font_family.empty())
        catalog.default_font_family = fallback.default_font_family;
    for (auto& locale : catalog.locales) {
        auto found = std::ranges::find_if(
            locale_texts,
            [&](PackageResourceText const& text) {
                return text.source == locale.source;
            });
        if (found != locale_texts.end()) {
            locale.strings =
                phenotype::parse_resource_locale_strings(found->text);
        }
    }
    return catalog;
}

inline std::string localized_or(
        phenotype::ResourceCatalog const& catalog,
        std::string_view locale,
        std::string_view key,
        std::string_view fallback) {
    if (auto value = phenotype::localized_string(catalog, key, locale))
        return value->value;
    return std::string{fallback};
}

inline ExplorerLabels file_explorer_labels(
        std::string_view locale,
        phenotype::ResourceCatalog const& catalog) {
    ExplorerLabels labels;
    auto get = [&](std::string_view key, std::string_view fallback) {
        return localized_or(catalog, locale, key, fallback);
    };
    labels.title = get("app.title", labels.title);
    labels.mobile_title = get("app.mobile_title", labels.mobile_title);
    labels.sidebar_recents = get("app.sidebar_recents", labels.sidebar_recents);
    labels.sidebar_shared = get("app.sidebar_shared", labels.sidebar_shared);
    labels.favorites = get("app.favorites", labels.favorites);
    labels.locations = get("app.locations", labels.locations);
    labels.applications = get("app.applications", labels.applications);
    labels.desktop = get("app.desktop", labels.desktop);
    labels.documents = get("app.documents", labels.documents);
    labels.pictures = get("app.pictures", labels.pictures);
    labels.downloads = get("app.downloads", labels.downloads);
    labels.icloud_drive = get("app.icloud_drive", labels.icloud_drive);
    labels.home = get("app.home", labels.home);
    labels.airdrop = get("app.airdrop", labels.airdrop);
    labels.trash = get("app.trash", labels.trash);
    labels.search_placeholder = get("app.search_placeholder", labels.search_placeholder);
    labels.status_ready = get("app.status_ready", labels.status_ready);
    labels.tab_browse = get("app.tab_browse", labels.tab_browse);
    labels.tab_preview = get("app.tab_preview", labels.tab_preview);
    labels.tab_create = get("app.tab_create", labels.tab_create);
    labels.create_file = get("actions.create_file", labels.create_file);
    labels.create_folder = get("actions.create_folder", labels.create_folder);
    labels.duplicate = get("actions.duplicate", labels.duplicate);
    labels.duplicate_file = get("actions.duplicate_file", labels.duplicate_file);
    labels.delete_action = get("actions.delete", labels.delete_action);
    labels.delete_selected = get("actions.delete_selected", labels.delete_selected);
    labels.preview = get("actions.preview", labels.preview);
    labels.root = get("nav.root", labels.root);
    labels.docs = get("nav.docs", labels.docs);
    labels.pics = get("nav.pics", labels.pics);
    labels.back = get("nav.back", labels.back);
    labels.forward = get("nav.forward", labels.forward);
    labels.up = get("nav.up", labels.up);
    labels.sort = get("nav.sort", labels.sort);
    labels.name = get("table.name", labels.name);
    labels.kind = get("table.kind", labels.kind);
    labels.size = get("table.size", labels.size);
    labels.no_matching_files = get("state.no_matching_files", labels.no_matching_files);
    labels.select_file_to_preview = get(
        "state.select_file_to_preview",
        labels.select_file_to_preview);
    labels.select_file_from_browse = get(
        "state.select_file_from_browse",
        labels.select_file_from_browse);
    labels.create_hint = get("create.hint", labels.create_hint);
    labels.file_name = get("create.file_name", labels.file_name);
    labels.contents = get("create.contents", labels.contents);
    labels.folder_name = get("create.folder_name", labels.folder_name);
    labels.reset_demo_files = get("create.reset_demo_files", labels.reset_demo_files);
    labels.status = get("status.title", labels.status);
    return labels;
}

inline ExplorerLabels file_explorer_labels(
        std::string_view locale,
        std::string_view profile) {
    return file_explorer_labels(locale, file_explorer_resource_catalog(profile));
}

} // namespace file_explorer_demo

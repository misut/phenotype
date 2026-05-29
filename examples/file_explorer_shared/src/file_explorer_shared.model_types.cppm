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

export module file_explorer_shared:model_types;

import json;
import phenotype.icon_catalog;
import phenotype.resources;
import phenotype.theme_contract;

export namespace file_explorer_demo {
namespace fs = std::filesystem;
namespace icon_catalog = phenotype::icon_catalog;
namespace theme_contract = phenotype::theme_contract;

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

struct Entry {
    std::string name;
    bool folder = false;
    std::uintmax_t size = 0;
};

struct OperationPlan {
    std::string kind;
    std::string display_name;
    std::string source;
    std::string destination;
    bool executable = false;
    bool sandboxed = true;
    bool mutates_filesystem = false;
    bool reads_file = false;
    bool reads_directory = false;
    bool writes_file = false;
    bool creates_directory = false;
    bool deletes_file = false;
    bool deletes_directory = false;
    bool moves_to_trash = false;
    bool permanent_delete = false;
    std::string fallback_reason;
};

struct OperationReceipt {
    std::string kind;
    std::string target;
    bool ok = false;
    std::string detail;
    OperationPlan plan{};
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

enum class ExplorerInputModality {
    Unspecified,
    Keyboard,
    Pointer,
    Programmatic,
};

enum class ExplorerFocusTarget {
    None,
    Sidebar,
    ToolbarNavigation,
    ToolbarView,
    ToolbarActions,
    Search,
    ContentGrid,
    PreviewPanel,
    CreatePanel,
    MoreActions,
};

struct RgbaColorSnapshot {
    int r = 0;
    int g = 122;
    int b = 255;
    int a = 255;
};

struct SystemPreferenceSnapshot {
    std::string source = "fallback";
    std::string font_family;
    std::string font_family_source = "fallback";
    float body_font_size = 0.0f;
    float heading_font_size = 0.0f;
    float small_font_size = 0.0f;
    float line_height_ratio = 1.6f;
    float font_scale = 1.0f;
    std::string text_size_source = "fallback";
    int font_weight_adjustment = 0;
    std::string font_weight_source = "fallback";
    std::string preferred_scroller_style = "unknown";
    bool overlay_scrollbars = false;
    float scroll_line_height = 0.0f;
    float scroll_wheel_lines = 3.0f;
    bool scroll_page_mode = false;
    float scroll_vertical_factor = 0.0f;
    float scroll_horizontal_factor = 0.0f;
    float scroll_bar_size = 0.0f;
    float touch_slop = 0.0f;
    float scroll_friction = 0.0f;
    float scroll_delta_multiplier = 1.0f;
    float scroll_horizontal_delta_multiplier = 1.0f;
    std::string scroll_source = "fallback";
    bool font_family_available = false;
    bool font_metrics_available = false;
    bool font_scale_available = false;
    bool line_height_available = false;
    bool scroll_metrics_available = false;
    bool color_scheme_available = false;
    bool reduce_motion_available = false;
    float double_click_interval_ms = 500.0f;
    float key_repeat_delay_ms = 500.0f;
    float key_repeat_interval_ms = 50.0f;
    float caret_blink_interval_ms = 530.0f;
    std::string input_timing_source = "fallback";
    std::string preferred_locale = "en";
    std::string preferred_locale_source = "fallback";
    std::string color_scheme = "light";
    std::string color_scheme_source = "fallback";
    std::string appearance_name;
    bool reduce_transparency = false;
    bool increase_contrast = false;
    bool reduce_motion = false;
    std::string accessibility_source = "fallback";
    bool accent_color_available = false;
    RgbaColorSnapshot accent_color{};
    std::string accent_color_source = "fallback";
    bool accent_color_opaque = true;
};

struct ThemePreferenceSnapshot {
    std::string font_family;
    std::string color_scheme;
    float font_scale = 1.0f;
    float body_font_size = 0.0f;
    float heading_font_size = 0.0f;
    float small_font_size = 0.0f;
    float line_height_ratio = 0.0f;
    float scroll_delta_multiplier = 1.0f;
    float scroll_horizontal_delta_multiplier = 1.0f;
    std::string scroll_bar_visibility;
    bool prefer_system_font_family = false;
    bool prefer_system_color_scheme = false;
    bool apply_system_font_metrics = true;
    bool apply_system_font_scale = true;
    bool apply_system_scroll_metrics = true;
    bool apply_system_accent_color = false;
    bool apply_system_reduce_motion = true;
    float motion_duration_multiplier = 1.0f;
};

struct Snapshot {
    fs::path root;
    fs::path current;
    std::string root_label = "Demo Root";
    std::string root_source = "demo-generated";
    bool root_is_demo = true;
    bool filesystem_mutations_allowed = true;
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
    std::string root_label = "Demo Root";
    std::string root_source = "demo-generated";
    bool root_is_demo = true;
    bool filesystem_mutations_allowed = true;
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
    ExplorerInputModality last_input_modality =
        ExplorerInputModality::Unspecified;
    ExplorerFocusTarget focus_target = ExplorerFocusTarget::None;
    bool focus_visible = false;
    SystemPreferenceSnapshot system_settings{};
    ThemePreferenceSnapshot theme_preferences{};
    std::string preferences_source = "default";
    std::string effective_font_family = "Pretendard";
    std::string effective_color_scheme = "light";
    float effective_body_font_size = 16.0f;
    float effective_heading_font_size = 20.0f;
    float effective_small_font_size = 14.4f;
    float effective_line_height_ratio = 1.6f;
    float effective_scroll_delta_multiplier = 1.0f;
    float effective_scroll_horizontal_delta_multiplier = 1.0f;
    std::string effective_scroll_bar_visibility = "auto";
    float effective_motion_duration_multiplier = 1.0f;
    bool used_system_font_family = false;
    bool used_system_color_scheme = false;
    bool used_system_font_metrics = false;
    bool used_system_font_scale = false;
    bool used_user_font_scale = false;
    bool used_user_font_size = false;
    bool used_system_line_height = false;
    bool used_user_line_height = false;
    bool used_system_scroll_metrics = false;
    bool used_user_scroll_scale = false;
    bool used_user_scroll_bar_visibility = false;
    bool used_system_accent_color = false;
    bool used_system_reduce_motion = false;
    bool used_user_motion_scale = false;
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
    FocusTarget,
    PointerFocus,
    TabFocus,
    ShiftTabFocus,
    SetFontFamily,
    SetFontScale,
    SetBodyFontSize,
    SetHeadingFontSize,
    SetSmallFontSize,
    SetLineHeightRatio,
    SetSystemFontMetrics,
    SetSystemScrollMetrics,
    SetScrollSpeed,
    SetHorizontalScrollSpeed,
    SetScrollBarVisibility,
    SetMotionScale,
    SetColorScheme,
};

struct ExplorerInput {
    ExplorerInputKind kind = ExplorerInputKind::Noop;
    std::string value;
    ExplorerInputModality modality = ExplorerInputModality::Unspecified;
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
    std::string icon_grid_label_policy;
    float icon_grid_top_inset = 0.0f;
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
    int native_window_control_count = 0;
    int content_window_control_marker_count = 0;
    int artifact_window_control_marker_count = 0;
    int content_drawn_window_control_count = 0;
    int artifact_drawn_window_control_count = 0;
    int overflow_action_button_count = 0;
    int icon_total_symbol_count = 0;
    int icon_phenotype_owned_symbol_count = 0;
    int icon_permissive_source_symbol_count = 0;
    int icon_material_symbols_source_symbol_count = 0;
    int icon_material_symbols_unique_source_icon_count = 0;
    int icon_apple_asset_symbol_count = 0;
    int icon_platform_extracted_symbol_count = 0;
    int icon_runtime_fetched_symbol_count = 0;
    int icon_audited_symbol_source_count = 0;
    int sidebar_symbol_count = 0;
    int toolbar_symbol_count = 0;
    int file_type_symbol_count = 0;
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
    int icon_interaction_phase_count = 0;
    float icon_grid_size = 0.0f;
    float icon_default_stroke_width = 0.0f;
    float icon_secondary_opacity = 0.0f;
    float icon_toolbar_point_size = 0.0f;
    float icon_sidebar_point_size = 0.0f;
    float icon_sidebar_optical_y_offset = 0.0f;
    float icon_toolbar_hit_target_size = 0.0f;
    float icon_sidebar_hit_target_size = 0.0f;
    float icon_action_hit_target_size = 0.0f;
    float icon_toolbar_activation_hit_target_size = 0.0f;
    float icon_sidebar_activation_hit_target_size = 0.0f;
    float icon_action_activation_hit_target_size = 0.0f;
    float icon_toolbar_button_radius = 0.0f;
    int icon_toolbar_button_background_alpha = 0;
    int icon_toolbar_button_hover_background_alpha = 0;
    int icon_toolbar_selected_button_background_alpha = 0;
    int icon_toolbar_selected_button_hover_background_alpha = 0;
    int icon_toolbar_pressed_button_background_alpha = 0;
    int icon_sidebar_selected_pressed_background_alpha = 0;
    float icon_pressed_symbol_opacity = 0.0f;
    float icon_pressed_scale = 0.0f;
    float column_location_row_height = 0.0f;
    float column_location_icon_size = 0.0f;
    bool integrated_titlebar = true;
    bool native_window_controls = true;
    bool duplicate_window_controls = false;
    bool window_control_single_owner = true;
    bool content_window_control_markers = false;
    bool finder_segmented_toolbar = false;
    bool owned_icon_assets = false;
    bool audited_permissive_icon_assets = false;
    bool uses_apple_icon_assets = false;
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
    std::string icon_svg_supported_elements;
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
    std::string icon_source_license_policy;
    std::string icon_preferred_external_source_policy;
    std::string icon_source_acquisition_policy;
    std::string icon_document_cache_policy;
    std::string icon_source_attribution_policy;
    std::string icon_apple_asset_boundary;
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
    std::string icon_symbol_interaction_phase_policy;
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
    std::string thumbnail_svg_policy;
    std::string thumbnail_svg_render_policy;
    std::string thumbnail_svg_preview_source_policy;
    std::string thumbnail_svg_external_resource_policy;
    std::string thumbnail_svg_document_cache_policy;
    int thumbnail_svg_document_cache_limit = 0;
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
    std::string finder_density_policy;
    std::string window_control_marker_mode;
    std::string native_window_control_owner;
    std::string window_control_duplication_guard;
    std::string window_control_render_policy;
    std::string titlebar_control_reserve_policy;
    std::string native_window_control_integration_policy;
    std::string native_window_control_geometry_role;
    std::string native_window_control_palette_policy;
};

struct FinderVisualContract {
    int schema_version = 1;
    std::string name;
    std::string profile;
    std::string source;
    std::string reference;
    std::string titlebar_strategy;
    std::string native_control_owner;
    std::string native_control_marker_policy;
    std::string native_control_geometry_role;
    std::string native_control_palette_policy;
    std::string sidebar_selection_style;
    float sidebar_selected_row_border_width = 0.0f;
    int sidebar_selected_row_background_alpha = 0;
    std::string focus_ring_policy;
    std::string icon_source_policy;
    std::string embedded_svg_policy;
    int apple_asset_symbol_count = 0;
    int platform_extracted_symbol_count = 0;
    int runtime_fetched_symbol_count = 0;
    std::string thumbnail_preview_policy;
    float leading_control_reserved_width = 0.0f;
    float titlebar_drag_region_height = 0.0f;
    std::string verifier_gate;
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
    ExplorerInputModality last_input_modality =
        ExplorerInputModality::Unspecified;
    ExplorerFocusTarget focus_target = ExplorerFocusTarget::None;
    bool focus_visible = false;
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
    FocusTarget,
    FocusVisible,
    InputModality,
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

struct RuntimePreferenceState {
    std::string source = "default";
    SystemPreferenceSnapshot system_settings{};
    ThemePreferenceSnapshot theme_preferences{};
    std::string effective_font_family = "Pretendard";
    std::string effective_color_scheme = "light";
    float effective_body_font_size = 16.0f;
    float effective_heading_font_size = 20.0f;
    float effective_small_font_size = 14.4f;
    float effective_line_height_ratio = 1.6f;
    float effective_scroll_delta_multiplier = 1.0f;
    float effective_scroll_horizontal_delta_multiplier = 1.0f;
    std::string effective_scroll_bar_visibility = "auto";
    float effective_motion_duration_multiplier = 1.0f;
    bool used_system_font_family = false;
    bool used_system_color_scheme = false;
    bool used_system_font_metrics = false;
    bool used_system_font_scale = false;
    bool used_user_font_scale = false;
    bool used_user_font_size = false;
    bool used_system_line_height = false;
    bool used_user_line_height = false;
    bool used_system_scroll_metrics = false;
    bool used_user_scroll_scale = false;
    bool used_user_scroll_bar_visibility = false;
    bool used_system_accent_color = false;
    bool used_system_reduce_motion = false;
    bool used_user_motion_scale = false;
};

struct ExplorerSidebarSymbol {
    std::string_view token;
    icon_catalog::Symbol symbol = icon_catalog::Symbol::Folder;
};
} // namespace file_explorer_demo

#include <cassert>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

import file_explorer_shared;
import json;

namespace {

[[noreturn]] void fail_assert(char const* expression, char const* file, int line) {
    std::cerr << "FAIL: " << file << ":" << line << ": " << expression << "\n";
    std::exit(1);
}

} // namespace

#undef assert
#define assert(expression) \
    ((expression) ? static_cast<void>(0) : fail_assert(#expression, __FILE__, __LINE__))

int main() {
#if defined(__wasi__) || defined(__ANDROID__)
    std::puts("SKIP: file explorer model filesystem test");
#else
    namespace demo = file_explorer_demo;
    namespace fs = std::filesystem;

    assert(demo::sanitize_file_name("../ bad:name") == "badname.txt");
    assert(demo::sanitize_file_name("  Report  Draft  ") == "Report Draft.txt");
    assert(demo::sanitize_file_name(".hidden") == "hidden.txt");
    assert(demo::sanitize_folder_name("../ Drafts:v1") == "Draftsv1");
    assert(demo::sanitize_folder_name("  Review   Folder  ") == "Review Folder");

    auto catalog = demo::file_explorer_resource_catalog("desktop");
    assert(catalog.default_font_family == "Pretendard");
    assert(catalog.debug.verifier == "phenotype artifact verify-file-explorer");
    assert(demo::file_explorer_labels("ko", "desktop").sidebar_recents
        == "최근 항목");
    assert(demo::file_explorer_labels("ko", catalog).sidebar_recents
        == "최근 항목");
    assert(demo::file_explorer_labels("ja", "desktop").sidebar_recents
        == "Recents");
    assert(demo::file_explorer_labels("ko", "mobile").tab_create == "만들기");
    auto packaged_texts = std::vector<demo::PackageResourceText>{
        {.source = "locales/ko.toml",
         .text = R"(
[app]
sidebar_recents = "패키지 최근 항목"

[actions]
create_file = "패키지 새 파일"
)"},
        {.source = "locales/en.toml",
         .text = R"(
[app]
sidebar_recents = "Packaged Recents"

[actions]
create_file = "Packaged New File"
)"},
    };
    auto packaged_catalog =
        demo::file_explorer_resource_catalog_from_package_texts(
            "desktop",
            R"(
[application]
id = "com.example.file-explorer"
display_name = "Packaged Explorer"
version = "0.1.0"
entry = "file_explorer_desktop"
platforms = ["macos"]

[resources]
default_locale = "ko"
default_font_family = "Pretendard"

[[locales]]
tag = "ko"
source = "locales/ko.toml"
fallback = ["en"]

[[locales]]
tag = "en"
source = "locales/en.toml"
fallback = []
)",
            packaged_texts);
    assert(packaged_catalog.application.display_name == "Packaged Explorer");
    assert(packaged_catalog.default_locale == "ko");
    assert(demo::file_explorer_labels("ko", packaged_catalog).sidebar_recents
        == "패키지 최근 항목");
    assert(demo::file_explorer_labels("ko", packaged_catalog).create_file
        == "패키지 새 파일");
    assert(demo::file_explorer_labels("ja", packaged_catalog).sidebar_recents
        == "패키지 최근 항목");

    auto parsed_location = demo::parse_explorer_input("location:documents");
    assert(parsed_location.ok);
    assert(parsed_location.input.kind == demo::ExplorerInputKind::SelectLocation);
    assert(parsed_location.input.value == "documents");
    auto parsed_open = demo::parse_explorer_input("open:Documents");
    assert(parsed_open.ok);
    assert(parsed_open.input.kind == demo::ExplorerInputKind::OpenEntry);
    assert(parsed_open.input.value == "Documents");
    auto parsed_activate = demo::parse_explorer_input("activate:Documents");
    assert(parsed_activate.ok);
    assert(parsed_activate.input.kind == demo::ExplorerInputKind::ActivateEntry);
    assert(parsed_activate.input.value == "Documents");
    auto parsed_enter = demo::parse_explorer_input("key:enter");
    assert(parsed_enter.ok);
    assert(parsed_enter.input.kind
           == demo::ExplorerInputKind::ActivateSelected);
    auto parsed_delete_key = demo::parse_explorer_input("key:delete");
    assert(parsed_delete_key.ok);
    assert(parsed_delete_key.input.kind
           == demo::ExplorerInputKind::DeleteSelected);
    auto parsed_duplicate_shortcut =
        demo::parse_explorer_input("shortcut:duplicate");
    assert(parsed_duplicate_shortcut.ok);
    assert(parsed_duplicate_shortcut.input.kind
           == demo::ExplorerInputKind::DuplicateSelected);
    auto parsed_find_shortcut =
        demo::parse_explorer_input("shortcut:find");
    assert(parsed_find_shortcut.ok);
    assert(parsed_find_shortcut.input.kind
           == demo::ExplorerInputKind::FocusSearch);
    auto parsed_arrow_down =
        demo::parse_explorer_input("key:arrow-down");
    assert(parsed_arrow_down.ok);
    assert(parsed_arrow_down.input.kind
           == demo::ExplorerInputKind::MoveSelection);
    assert(parsed_arrow_down.input.selection_move
           == demo::ExplorerSelectionMove::Down);
    assert(demo::explorer_input_label(parsed_arrow_down.input)
           == "move_selection:down");
    auto parsed_arrow_right =
        demo::parse_explorer_input("key:right");
    assert(parsed_arrow_right.ok);
    assert(parsed_arrow_right.input.kind
           == demo::ExplorerInputKind::MoveSelection);
    assert(parsed_arrow_right.input.selection_move
           == demo::ExplorerSelectionMove::Right);
    auto parsed_home_key =
        demo::parse_explorer_input("key:home");
    assert(parsed_home_key.ok);
    assert(parsed_home_key.input.kind
           == demo::ExplorerInputKind::MoveSelection);
    assert(parsed_home_key.input.selection_move
           == demo::ExplorerSelectionMove::Home);
    auto parsed_page_down =
        demo::parse_explorer_input("move:page-down");
    assert(parsed_page_down.ok);
    assert(parsed_page_down.input.kind
           == demo::ExplorerInputKind::MoveSelection);
    assert(parsed_page_down.input.selection_move
           == demo::ExplorerSelectionMove::PageDown);
    auto parsed_escape_key =
        demo::parse_explorer_input("key:escape");
    assert(parsed_escape_key.ok);
    assert(parsed_escape_key.input.kind
           == demo::ExplorerInputKind::DismissTransient);
    auto parsed_view = demo::parse_explorer_input("view:gallery");
    assert(parsed_view.ok);
    assert(parsed_view.input.kind == demo::ExplorerInputKind::ViewMode);
    assert(parsed_view.input.view_mode == demo::ExplorerViewMode::Gallery);
    assert(parsed_view.input.value == "gallery");
    assert(demo::explorer_input_label(parsed_view.input) == "view_mode:gallery");
    auto parsed_bad_view = demo::parse_explorer_input("view:cover-flow");
    assert(!parsed_bad_view.ok);
    assert(parsed_bad_view.error.find("view") != std::string::npos);
    auto parsed_sort = demo::parse_explorer_input("sort:kind");
    assert(parsed_sort.ok);
    assert(parsed_sort.input.kind == demo::ExplorerInputKind::Sort);
    assert(parsed_sort.input.sort_mode == demo::SortMode::Kind);
    auto parsed_recent_sort = demo::parse_explorer_input("sort:recent");
    assert(parsed_recent_sort.ok);
    assert(parsed_recent_sort.input.kind == demo::ExplorerInputKind::Sort);
    assert(parsed_recent_sort.input.sort_mode == demo::SortMode::Recent);
    auto parsed_viewport = demo::parse_explorer_input("viewport:900x620@2");
    assert(parsed_viewport.ok);
    assert(parsed_viewport.input.kind == demo::ExplorerInputKind::Viewport);
    assert(parsed_viewport.input.viewport_width == 900);
    assert(parsed_viewport.input.viewport_height == 620);
    assert(parsed_viewport.input.viewport_scale == 2.0f);
    assert(parsed_viewport.input.value == "900x620@2");
    auto parsed_bad_viewport = demo::parse_explorer_input("viewport:900");
    assert(!parsed_bad_viewport.ok);
    assert(parsed_bad_viewport.error.find("viewport") != std::string::npos);
    auto parsed_bad = demo::parse_explorer_input("sort:date");
    assert(!parsed_bad.ok);
    assert(parsed_bad.error.find("sort") != std::string::npos);
    auto parsed_lines = demo::parse_explorer_input_lines(R"(
# Native startup script.
input viewport:760x540@2
input select:README.txt
duplicate
)");
    assert(parsed_lines.ok);
    assert(parsed_lines.inputs.size() == 3);
    assert(parsed_lines.inputs[0].kind == demo::ExplorerInputKind::Viewport);
    assert(parsed_lines.inputs[1].kind == demo::ExplorerInputKind::SelectEntry);
    assert(parsed_lines.inputs[2].kind
           == demo::ExplorerInputKind::DuplicateSelected);
    auto parsed_sequence =
        demo::parse_explorer_input_sequence("select:README.txt;duplicate");
    assert(parsed_sequence.ok);
    assert(parsed_sequence.inputs.size() == 2);
    auto parsed_line_error =
        demo::parse_explorer_input_lines("select:README.txt\nsort:date\n",
                                         "startup.drive");
    assert(!parsed_line_error.ok);
    assert(parsed_line_error.line == 2);
    assert(parsed_line_error.error.find("startup.drive:2") != std::string::npos);
    auto expected_selected =
        demo::parse_explorer_expectation("selected:README.txt");
    assert(expected_selected.ok);
    assert(expected_selected.expectation.kind
        == demo::ExplorerExpectationKind::Selected);
    auto expected_operation =
        demo::parse_explorer_expectation("operation:file_delete:ok");
    assert(expected_operation.ok);
    assert(expected_operation.expectation.expects_operation_ok);
    assert(expected_operation.expectation.operation_ok);
    auto expected_missing =
        demo::parse_explorer_expectation("missing-entry:Ghost.txt");
    assert(expected_missing.ok);
    auto bad_expectation = demo::parse_explorer_expectation("selected");
    assert(!bad_expectation.ok);

    std::string const profile = "test-model-contract";
    auto root = demo::demo_root(profile);
    std::error_code ec;
    fs::remove_all(root, ec);

    std::string const startup_profile = "test-startup-inputs";
    fs::remove_all(demo::demo_root(startup_profile), ec);
    auto startup_state = demo::make_state(startup_profile);
    demo::apply_explorer_inputs(
        startup_state,
        parsed_sequence.inputs,
        startup_profile);
    assert(startup_state.selected_name == "README copy.txt");
    assert(startup_state.last_operation.kind == "file_duplicate");
    fs::remove_all(startup_state.root, ec);

    auto state = demo::make_state(profile);
    assert(state.viewport_width == demo::k_desktop_default_viewport_width);
    assert(state.viewport_height == demo::k_desktop_default_viewport_height);
    assert(state.viewport_scale == 1.0f);
    auto chrome = demo::explorer_chrome_metrics(state, profile);
    assert(chrome.viewport.width == demo::k_desktop_default_viewport_width);
    assert(chrome.viewport.height == demo::k_desktop_default_viewport_height);
    assert(chrome.icon_grid_columns == 6);
    assert(chrome.icon_grid_visible_rows == 3);
    assert(chrome.icon_grid_visible_capacity == 18);
    assert(chrome.window_content_inset
           == demo::k_desktop_window_content_inset);
    assert(chrome.window_gap == demo::k_desktop_window_gap);
    assert(chrome.toolbar_shell_x == 236.0f);
    assert(chrome.toolbar_shell_y == demo::k_desktop_window_content_inset);
    assert(chrome.toolbar_shell_width == 1060.0f);
    assert(chrome.toolbar_shell_height
           == demo::k_desktop_toolbar_shell_height);
    assert(chrome.toolbar_group_y == 8.0f);
    assert(chrome.toolbar_navigation_group_x == 240.0f);
    assert(chrome.toolbar_title_x == 336.0f);
    assert(chrome.toolbar_view_group_x == 840.0f);
    assert(chrome.toolbar_sort_group_x == 1060.0f);
    assert(chrome.toolbar_action_group_x == 1112.0f);
    assert(chrome.toolbar_search_group_x == 1244.0f);
    assert(chrome.content_surface_x == chrome.toolbar_shell_x);
    assert(chrome.content_surface_y == 64.0f);
    assert(chrome.content_surface_width == chrome.toolbar_shell_width);
    assert(chrome.sidebar_surface_x == demo::k_desktop_window_content_inset);
    assert(chrome.sidebar_surface_y == demo::k_desktop_window_content_inset);
    assert(chrome.sidebar_first_row_y == 76.0f);
    assert(chrome.sidebar_row_height == demo::k_desktop_sidebar_row_height);
    assert(chrome.sidebar_heading_height
           == demo::k_desktop_sidebar_heading_height);
    assert(chrome.sidebar_icon_size == demo::k_desktop_sidebar_icon_size);
    assert(chrome.sidebar_icon_leading == demo::k_desktop_sidebar_icon_leading);
    assert(chrome.sidebar_label_leading == demo::k_desktop_sidebar_label_leading);
    assert(chrome.sidebar_label_top == demo::k_desktop_sidebar_label_top);
    assert(chrome.sidebar_heading_label_leading
           == demo::k_desktop_sidebar_heading_label_leading);
    assert(chrome.sidebar_heading_label_top
           == demo::k_desktop_sidebar_heading_label_top);
    assert(chrome.sidebar_section_gap == demo::k_desktop_sidebar_section_gap);
    assert(chrome.sidebar_selected_row_radius
           == demo::k_desktop_sidebar_selected_row_radius);
    assert(chrome.sidebar_selected_row_background_alpha == 238);
    assert(chrome.sidebar_selected_row_hover_background_alpha == 248);
    assert(chrome.sidebar_selection_policy
           == demo::k_desktop_sidebar_selection_policy);
    assert(chrome.toolbar_group_height
           == demo::k_desktop_toolbar_group_height);
    assert(chrome.toolbar_group_radius
           == demo::k_desktop_toolbar_group_radius);
    assert(chrome.toolbar_icon_button_width
           == demo::k_desktop_toolbar_icon_button_width);
    assert(chrome.toolbar_icon_button_height
           == demo::k_desktop_toolbar_icon_button_height);
    assert(chrome.titlebar_control_cluster_height
           == demo::k_desktop_titlebar_control_cluster_height);
    assert(chrome.titlebar_control_diameter
           == demo::k_desktop_titlebar_control_diameter);
    assert(chrome.titlebar_control_spacing
           == demo::k_desktop_titlebar_control_spacing);
    assert(chrome.titlebar_control_start_x
           == demo::k_desktop_titlebar_control_start_x);
    assert(chrome.titlebar_control_top
           == demo::k_desktop_titlebar_control_top);
    assert(chrome.titlebar_drag_region_height
           == demo::k_desktop_titlebar_drag_region_height);
    assert(chrome.leading_control_reserved_width
           == demo::k_desktop_leading_control_reserved_width);
    assert(chrome.trailing_control_reserved_width
           == demo::k_desktop_trailing_control_reserved_width);
    assert(chrome.icon_grid_column_width
           == demo::k_desktop_icon_grid_column_width);
    assert(chrome.icon_grid_row_height
           == demo::k_desktop_icon_grid_row_height);
    assert(chrome.icon_grid_column_pitch
           == demo::k_desktop_icon_grid_column_pitch);
    assert(chrome.icon_grid_thumbnail_width
           == demo::k_desktop_icon_grid_thumbnail_width);
    assert(chrome.icon_grid_thumbnail_height
           == demo::k_desktop_icon_grid_thumbnail_height);
    assert(chrome.icon_grid_label_height
           == demo::k_desktop_icon_grid_label_height);
    assert(chrome.icon_grid_label_font_size
           == demo::k_desktop_icon_grid_label_font_size);
    assert(chrome.icon_grid_gap == demo::k_desktop_icon_grid_gap);
    assert(chrome.toolbar_group_count == 5);
    assert(chrome.toolbar_separator_count == 3);
    assert(chrome.toolbar_icon_button_count == 11);
    assert(chrome.column_location_row_count
           == demo::k_desktop_column_location_row_count);
    assert(chrome.column_location_row_height
           == demo::k_desktop_column_location_row_height);
    assert(chrome.column_location_icon_size
           == demo::k_desktop_column_location_icon_size);
    assert(chrome.titlebar_control_count
           == demo::k_desktop_titlebar_control_count);
    assert(chrome.icon_total_symbol_count == 31);
    assert(chrome.sidebar_symbol_count == 11);
    assert(chrome.toolbar_symbol_count == 15);
    assert(chrome.icon_filled_symbol_count == 1);
    assert(chrome.icon_outline_symbol_count == 30);
    assert(chrome.icon_hierarchical_symbol_count == 20);
    assert(chrome.icon_reference_symbol_count == 31);
    assert(chrome.icon_svg_path_arc_symbol_count == 1);
    assert(chrome.icon_grid_size == 24.0f);
    assert(demo::desktop_sidebar_symbol_contract().size() == 10);
    assert(demo::sidebar_symbol_name_for_token("recents") == "recents");
    assert(demo::sidebar_symbol_name_for_token("shared") == "shared");
    assert(demo::sidebar_symbol_name_for_token("desktop") == "desktop");
    assert(demo::sidebar_symbol_name_for_token("download") == "download");
    assert(demo::sidebar_symbol_name_for_token("unknown-token") == "folder");
    assert(chrome.icon_default_stroke_width == 1.8f);
    assert(chrome.icon_secondary_opacity == 0.66f);
    assert(chrome.icon_toolbar_button_radius == 15.0f);
    assert(chrome.icon_toolbar_button_background_alpha == 0);
    assert(chrome.icon_toolbar_button_hover_background_alpha == 120);
    assert(chrome.icon_toolbar_selected_button_background_alpha == 150);
    assert(chrome.icon_toolbar_selected_button_hover_background_alpha == 190);
    assert(chrome.icon_module == "phenotype.icons");
    assert(chrome.icon_style == "macos_rounded_outline_svg");
    assert(chrome.icon_source_format == "svg");
    assert(chrome.icon_svg_subset_policy == "bounded_svg_icon_subset");
    assert(chrome.icon_svg_supported_path_commands.find("A Z")
           != std::string::npos);
    assert(chrome.icon_svg_arc_policy.find("bounded cubic Bezier")
           != std::string::npos);
    assert(chrome.icon_design_reference.find("macOS Finder")
           != std::string::npos);
    assert(chrome.icon_reference_family == "SF Symbols semantic reference");
    assert(chrome.icon_reference_policy.find("phenotype-owned")
           != std::string::npos);
    assert(chrome.icon_asset_policy.find("no Apple") != std::string::npos);
    assert(chrome.icon_alignment == "24x24 text-aligned symbol grid");
    assert(chrome.icon_rendering_mode == "hierarchical");
    assert(chrome.icon_variant_policy
           == "outline primary with filled action variants");
    assert(chrome.icon_interaction_tone_policy
           == "macos_finder_interaction_tones");
    assert(chrome.icon_symbol_control_chrome_policy
           == "macos_finder_symbol_state_chrome");
    assert(chrome.icon_file_type_color_policy
           == "macos_finder_file_type_tints");
    assert(chrome.icon_scale == "medium");
    assert(chrome.theme_contract_version == 1);
    assert(chrome.theme_profile_name == "apple-glass-light");
    assert(chrome.theme_reference.find("Liquid Glass") != std::string::npos);
    assert(chrome.theme_font_policy.find("Pretendard") != std::string::npos);
    assert(chrome.theme_material_policy.find("pure material planner")
           != std::string::npos);
    assert(chrome.theme_iconography_policy.find("macos_finder")
           != std::string::npos);
    assert(chrome.theme_icon_asset_policy.find("without_embedded_apple_artwork")
           != std::string::npos);
    assert(chrome.theme_usage_policy.find("not_content_fill")
           != std::string::npos);
    assert(chrome.theme_container_policy.find("explicit_container_spacing")
           != std::string::npos);
    assert(chrome.theme_performance_policy.find("bounded_glass_surfaces")
           != std::string::npos);
    assert(chrome.theme_accessibility_policy.find("reduced_transparency")
           != std::string::npos);
    assert(chrome.theme_fallback_policy.find("unsupported_backends")
           != std::string::npos);
    assert(chrome.chrome_geometry_policy
           == demo::k_desktop_chrome_geometry_policy);
    assert(chrome.owned_icon_assets);
    assert(!chrome.uses_sf_symbols_assets);
    assert(chrome.icon_round_stroke_contract);
    assert(chrome.icon_text_weight_aligned);
    assert(chrome.icon_hierarchical_opacity);
    assert(chrome.finder_segmented_toolbar);
    assert(chrome.integrated_titlebar);
    assert(chrome.native_window_controls);
    assert(!chrome.duplicate_window_controls);
    assert(!chrome.content_window_control_markers);
    assert(!chrome.artifact_window_control_markers);
    assert(chrome.window_control_marker_mode == "runtime-native-controls");
    auto artifact_chrome =
        demo::explorer_chrome_with_artifact_window_markers(chrome);
    assert(artifact_chrome.content_window_control_markers);
    assert(artifact_chrome.artifact_window_control_markers);
    assert(!artifact_chrome.duplicate_window_controls);
    assert(artifact_chrome.window_control_marker_mode
           == "artifact-probe-marker");
    assert(!chrome.status_bar_visible);
    assert(demo::explorer_icon_grid_columns(chrome).size() == 6);
    std::string const viewport_profile = "test-status-bar-viewport";
    auto viewport_state = demo::make_state(viewport_profile);
    demo::apply_explorer_input(
        viewport_state,
        parsed_viewport.input,
        viewport_profile);
    assert(viewport_state.status == "Viewport set to 900x620@2");
    assert(!demo::explorer_chrome_metrics(viewport_state, profile)
        .status_bar_visible);
    fs::remove_all(viewport_state.root, ec);
    fs::remove_all(demo::demo_root("mobile"), ec);
    auto mobile_state = demo::make_state("mobile");
    auto mobile_chrome = demo::explorer_chrome_metrics(mobile_state, "mobile");
    assert(mobile_state.viewport_width == demo::k_mobile_default_viewport_width);
    assert(mobile_state.viewport_height == demo::k_mobile_default_viewport_height);
    assert(mobile_state.viewport_scale == 2.0f);
    assert(!mobile_chrome.integrated_titlebar);
    assert(!mobile_chrome.native_window_controls);
    assert(mobile_chrome.icon_grid_columns == 0);
    assert(mobile_chrome.toolbar_group_count == 0);
    assert(mobile_chrome.toolbar_separator_count == 0);
    assert(mobile_chrome.toolbar_icon_button_count == 0);
    assert(mobile_chrome.titlebar_control_count == 0);
    assert(!mobile_chrome.content_window_control_markers);
    assert(!mobile_chrome.finder_segmented_toolbar);
    assert(!mobile_chrome.status_bar_visible);
    fs::remove_all(mobile_state.root, ec);
    auto snap = demo::snapshot(state);
    assert(!snap.has_selection);
    assert(snap.file_count >= 15);
    assert(snap.folder_count == 3);
    assert(!demo::deletable_directory(state.root, demo::trash_path(state.root)));
    assert(snap.can_create_file);
    assert(snap.can_create_folder);
    assert(!snap.can_delete_selected);
    assert(!snap.can_duplicate_selected);
    assert(!snap.can_preview_selected);
    assert(snap.action_summary.find("Select a file") != std::string::npos);
    assert(snap.sort_label == "Sort: Recent");
    assert(snap.view_mode == demo::ExplorerViewMode::Icon);
    assert(demo::view_mode_label(snap.view_mode) == "Icon View");
    assert(snap.entries.size() >= 5);
    assert(snap.entries[0].name == "작성예시3_선택_DC 탈퇴신청서.pdf");
    assert(snap.entries[1].name == "작성예시2_필수_운용지시서.pdf");
    assert(snap.entries[2].name == "작성예시1_필수_중도인출 신청서.pdf");
    assert(snap.entries[3].name == "2_필수_운용지시서.pdf");
    assert(snap.entries[4].name == "1_필수_중도인출 신청서.pdf");
    assert(snap.entries[5].name == "※해당 시 필독_①무주택자인 경우.pdf");
    assert(snap.entries[6].name == "[카카오] 퇴직금 지급 기준.pdf");

    std::string const keyboard_profile = "test-keyboard-navigation";
    auto keyboard_state = demo::make_state(keyboard_profile);
    auto keyboard_entries = demo::snapshot(keyboard_state).entries;
    auto keyboard_chrome =
        demo::explorer_chrome_metrics(keyboard_state, keyboard_profile);
    assert(keyboard_entries.size()
           > static_cast<std::size_t>(keyboard_chrome.icon_grid_columns + 1));
    demo::apply_explorer_input(
        keyboard_state,
        parsed_arrow_down.input,
        keyboard_profile);
    auto keyboard_snap = demo::snapshot(keyboard_state);
    assert(keyboard_snap.has_selection);
    assert(keyboard_snap.selected_index == 0);
    assert(keyboard_snap.selected.name == keyboard_entries[0].name);
    assert(keyboard_state.status.find("Arrow Down") != std::string::npos);
    demo::apply_explorer_input(
        keyboard_state,
        parsed_arrow_right.input,
        keyboard_profile);
    keyboard_snap = demo::snapshot(keyboard_state);
    assert(keyboard_snap.selected_index == 1);
    assert(keyboard_snap.selected.name == keyboard_entries[1].name);
    demo::apply_explorer_input(
        keyboard_state,
        parsed_arrow_down.input,
        keyboard_profile);
    keyboard_snap = demo::snapshot(keyboard_state);
    assert(keyboard_snap.selected_index
           == static_cast<std::size_t>(keyboard_chrome.icon_grid_columns + 1));
    assert(keyboard_snap.selected.name
           == keyboard_entries[static_cast<std::size_t>(
               keyboard_chrome.icon_grid_columns + 1)].name);
    demo::apply_explorer_input(
        keyboard_state,
        parsed_home_key.input,
        keyboard_profile);
    keyboard_snap = demo::snapshot(keyboard_state);
    assert(keyboard_snap.selected_index == 0);
    demo::apply_explorer_input(
        keyboard_state,
        parsed_page_down.input,
        keyboard_profile);
    keyboard_snap = demo::snapshot(keyboard_state);
    assert(keyboard_snap.selected_index
           == std::min<std::size_t>(
               static_cast<std::size_t>(
                   keyboard_chrome.icon_grid_visible_capacity),
               keyboard_entries.size() - 1));
    fs::remove_all(keyboard_state.root, ec);

    auto outside = state.root.parent_path()
        / "phenotype-file-explorer-outside.txt";
    {
        std::ofstream out(outside, std::ios::binary);
        out << "This file must stay outside the demo sandbox.\n";
    }
    assert(fs::exists(outside));
    demo::select_entry(state, "../phenotype-file-explorer-outside.txt");
    assert(state.selected_name.empty());
    assert(state.last_operation.kind == "file_read");
    assert(!state.last_operation.ok);
    assert(fs::exists(outside));
    demo::select_entry(state, R"(..\phenotype-file-explorer-outside.txt)");
    assert(state.selected_name.empty());
    assert(!state.last_operation.ok);
    assert(fs::exists(outside));
    demo::select_entry(state, ".Trash");
    assert(state.selected_name.empty());
    assert(!state.last_operation.ok);
    assert(fs::exists(outside));
    state.selected_name = "../phenotype-file-explorer-outside.txt";
    demo::delete_selected(state);
    assert(state.selected_name.empty());
    assert(state.last_operation.kind == "file_delete");
    assert(!state.last_operation.ok);
    assert(fs::exists(outside));
    state.selected_name = "../phenotype-file-explorer-outside.txt";
    demo::duplicate_selected(state);
    assert(state.selected_name.empty());
    assert(state.last_operation.kind == "file_duplicate");
    assert(!state.last_operation.ok);
    assert(fs::exists(outside));
    fs::remove(outside, ec);

    demo::select_entry(state, "README.txt");
    assert(demo::explorer_chrome_metrics(state, profile).status_bar_visible);
    snap = demo::snapshot(state);
    assert(snap.has_selection);
    assert(snap.selected.name == "README.txt");
    assert(snap.can_delete_selected);
    assert(snap.can_duplicate_selected);
    assert(snap.can_preview_selected);
    assert(snap.selected_kind_label == "TXT File");
    assert(snap.selected_size_label != "--");
    assert(snap.action_summary.find("Selected README.txt") != std::string::npos);
    demo::set_sort_mode(state, demo::SortMode::Name);
    snap = demo::snapshot(state);
    bool saw_file_after_folder = false;
    for (auto const& entry : snap.entries) {
        if (!entry.folder)
            saw_file_after_folder = true;
        assert(!entry.folder || !saw_file_after_folder);
    }
    bool saw_korean_pdf = false;
    bool saw_japanese_pdf = false;
    bool saw_chinese_pdf = false;
    for (auto const& entry : snap.entries) {
        if (entry.name == "1_필수_중도인출 신청서.pdf")
            saw_korean_pdf = true;
        if (entry.name == "작성예시1_필수_중도인출 신청서.pdf")
            saw_korean_pdf = true;
        if (entry.name == "契約書_サンプル.pdf")
            saw_japanese_pdf = true;
        if (entry.name == "资产证明.pdf")
            saw_chinese_pdf = true;
    }
    assert(saw_korean_pdf);
    assert(saw_japanese_pdf);
    assert(saw_chinese_pdf);

    demo::apply_explorer_input(state, parsed_view.input, profile);
    snap = demo::snapshot(state);
    assert(snap.view_mode == demo::ExplorerViewMode::Gallery);
    assert(state.status == "Switched to Gallery View.");
    auto debug_payload = demo::file_explorer_application_debug_json(
        state,
        snap,
        demo::explorer_chrome_metrics(state, profile),
        profile);
    auto debug_text = json::emit(debug_payload);
    assert(debug_text.find("\"file_explorer\"") != std::string::npos);
    assert(debug_text.find("\"view_mode\"") != std::string::npos);
    assert(debug_text.find("\"Gallery View\"") != std::string::npos);
    assert(debug_text.find("\"icon_grid_column_pitch\"") != std::string::npos);
    assert(debug_text.find("\"icon_grid_thumbnail_width\"") != std::string::npos);
    assert(debug_text.find("\"icon_grid_label_font_size\"") != std::string::npos);
    assert(debug_text.find("\"sidebar_icon_size\"") != std::string::npos);
    assert(debug_text.find("\"sidebar_label_leading\"") != std::string::npos);
    assert(debug_text.find("\"icon_system\"") != std::string::npos);
    assert(debug_text.find("\"phenotype.icons\"") != std::string::npos);
    assert(debug_text.find("\"macos_rounded_outline_svg\"") != std::string::npos);
    assert(debug_text.find("\"svg_subset_policy\":\"bounded_svg_icon_subset\"")
           != std::string::npos);
    assert(debug_text.find("\"svg_supported_path_commands\":\"M L H V Q T C S A Z")
           != std::string::npos);
    assert(debug_text.find("\"svg_arc_policy\":\"circle elements and isolated circular path A/a")
           != std::string::npos);
    assert(debug_text.find("\"svg_path_arc_symbol_count\":1")
           != std::string::npos);
    assert(debug_text.find("\"hierarchical_opacity\":true")
           != std::string::npos);
    assert(debug_text.find("\"rendering_mode\":\"hierarchical\"")
           != std::string::npos);
    assert(debug_text.find("\"presentation_policy\":\"macos_role_aware_symbol_presentation\"")
           != std::string::npos);
    assert(debug_text.find("\"tone_policy\":\"primary, secondary, selected, accent, disabled, destructive\"")
           != std::string::npos);
    assert(debug_text.find("\"interaction_tone_policy\":\"macos_finder_interaction_tones\"")
           != std::string::npos);
    assert(debug_text.find("\"symbol_control_chrome_policy\":\"macos_finder_symbol_state_chrome\"")
           != std::string::npos);
    assert(debug_text.find("\"toolbar_button_hover_background_alpha\":120")
           != std::string::npos);
    assert(debug_text.find("\"toolbar_selected_button_background_alpha\":150")
           != std::string::npos);
    assert(debug_text.find("\"sidebar_selected\":\"accent\"")
           != std::string::npos);
    assert(debug_text.find("\"toolbar_unselected\":\"secondary\"")
           != std::string::npos);
    assert(debug_text.find("\"file_type_color_policy\":\"macos_finder_file_type_tints\"")
           != std::string::npos);
    assert(debug_text.find("\"toolbar_point_size\":24") != std::string::npos);
    assert(debug_text.find("\"sidebar_point_size\":26") != std::string::npos);
    assert(debug_text.find("\"column_location_row_count\":4")
           != std::string::npos);
    assert(debug_text.find("\"column_location_icon_size\":18")
           != std::string::npos);
    assert(debug_text.find("\"uses_sf_symbols_assets\":false")
           != std::string::npos);
    assert(debug_text.find(
        "\"window_control_marker_mode\":\"runtime-native-controls\"")
           != std::string::npos);
    assert(debug_text.find("\"content_window_control_markers\":false")
           != std::string::npos);
    assert(debug_text.find("\"titlebar_control_start_x\"") != std::string::npos);
    assert(debug_text.find("\"profile\"") != std::string::npos);
    assert(debug_text.find(profile) != std::string::npos);
    assert(debug_text.find("\"keyboard_commands\"") != std::string::npos);
    assert(debug_text.find("CommandOrControl+F") != std::string::npos);
    assert(debug_text.find("ArrowDown") != std::string::npos);
    assert(debug_text.find("\"index\"") != std::string::npos);

    state.draft_name = "../ Launch Plan";
    state.draft_body = "Created from test_file_explorer_model.";
    demo::create_file(state);
    assert(state.selected_name == "Launch Plan.txt");
    assert(fs::exists(state.current / "Launch Plan.txt"));
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_create ok - Launch Plan.txt") != std::string::npos);
    assert(demo::read_preview(state.current / "Launch Plan.txt")
        .find("Created from test_file_explorer_model") != std::string::npos);

    demo::delete_selected(state);
    assert(!fs::exists(state.current / "Launch Plan.txt"));
    assert(fs::exists(demo::trash_path(state.root) / "Launch Plan.txt"));
    assert(state.selected_name.empty());
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_delete ok - Launch Plan.txt") != std::string::npos);

    state.draft_folder_name = "../ Review Folder";
    demo::create_folder(state);
    assert(state.selected_name == "Review Folder");
    assert(fs::is_directory(state.current / "Review Folder"));
    snap = demo::snapshot(state);
    assert(snap.selected.folder);
    assert(snap.can_delete_selected);
    assert(snap.operation_label
        .find("Operation: folder_create ok - Review Folder") != std::string::npos);
    assert(snap.preview.find("Open this folder") != std::string::npos);

    demo::select_entry(state, "Review Folder");
    assert(demo::same_path(state.current, state.root));
    snap = demo::snapshot(state);
    assert(snap.has_selection);
    assert(snap.selected.folder);
    assert(snap.operation_label
        .find("Operation: folder_select ok - Review Folder") != std::string::npos);

    demo::activate_entry(state, "Review Folder");
    assert(demo::relative_location(state.root, state.current)
        == "Demo Root/Review Folder");
    assert(state.selected_name.empty());
    demo::go_back(state);
    assert(demo::same_path(state.current, state.root));
    demo::select_entry(state, "Review Folder");

    demo::write_file_if_missing(
        state.current / state.selected_name / "Nested Note.txt",
        "Nested file proves folder deletion moves contents to Trash.\n");
    snap = demo::snapshot(state);
    assert(snap.can_delete_selected);

    demo::delete_selected(state);
    assert(!fs::exists(state.current / "Review Folder"));
    assert(fs::is_directory(demo::trash_path(state.root) / "Review Folder"));
    assert(fs::exists(demo::trash_path(state.root)
        / "Review Folder" / "Nested Note.txt"));
    assert(state.selected_name.empty());
    assert(demo::snapshot(state).operation_label
        .find("Operation: folder_delete ok - Review Folder") != std::string::npos);

    demo::apply_startup_scenario(state, "created-preview");
    assert(state.selected_name == "Action Note.txt");
    assert(fs::exists(state.current / "Action Note.txt"));
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_create ok - Action Note.txt") != std::string::npos);
    assert(demo::read_preview(state.current / "Action Note.txt")
        .find("Created from artifact scenario") != std::string::npos);

    demo::apply_startup_scenario(state, "created-preview");
    assert(state.selected_name == "Action Note.txt");
    assert(fs::exists(state.current / "Action Note.txt"));
    assert(!fs::exists(state.current / "Action Note 2.txt"));

    demo::apply_startup_scenario(state, "deleted-file");
    assert(!fs::exists(state.current / "Delete Me.txt"));
    assert(fs::exists(demo::trash_path(state.root) / "Delete Me.txt"));
    assert(state.selected_name.empty());
    assert(state.status == "Moved Delete Me.txt to Trash");
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_delete ok - Delete Me.txt") != std::string::npos);

    demo::apply_startup_scenario(state, "trash-view");
    assert(demo::relative_location(state.root, state.current) == "Trash");
    assert(fs::exists(demo::trash_path(state.root) / "Trash Note.txt"));
    snap = demo::snapshot(state);
    bool saw_trash_note = false;
    for (auto const& entry : snap.entries) {
        if (entry.name == "Trash Note.txt")
            saw_trash_note = true;
    }
    assert(saw_trash_note);
    assert(snap.operation_label
        .find("Operation: file_delete ok - Trash Note.txt") != std::string::npos);
    demo::select_entry(state, "Trash Note.txt");
    demo::delete_selected(state);
    assert(!fs::exists(demo::trash_path(state.root) / "Trash Note.txt"));
    assert(state.status == "Deleted Trash Note.txt from Trash");

    demo::apply_startup_scenario(state, "created-folder");
    assert(state.selected_name == "Review Folder");
    assert(fs::is_directory(state.current / "Review Folder"));
    assert(demo::snapshot(state).operation_label
        .find("Operation: folder_create ok - Review Folder") != std::string::npos);

    demo::apply_startup_scenario(state, "deleted-folder");
    assert(!fs::exists(state.current / "Trash Folder"));
    assert(fs::is_directory(demo::trash_path(state.root) / "Trash Folder"));
    assert(fs::exists(demo::trash_path(state.root)
        / "Trash Folder" / "Nested Note.txt"));
    assert(state.selected_name.empty());
    assert(demo::snapshot(state).operation_label
        .find("Operation: folder_delete ok - Trash Folder") != std::string::npos);

    demo::select_entry(state, "README.txt");
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_read ok - README.txt") != std::string::npos);
    demo::duplicate_selected(state);
    assert(state.selected_name == "README copy.txt");
    assert(fs::exists(state.current / "README copy.txt"));
    assert(state.status == "Duplicated README.txt to README copy.txt");
    snap = demo::snapshot(state);
    assert(snap.selected.name == "README copy.txt");
    assert(snap.operation_label
        .find("Operation: file_duplicate ok - README copy.txt") != std::string::npos);
    assert(snap.can_delete_selected);
    assert(snap.can_duplicate_selected);
    assert(snap.selected_path_label.find("README copy.txt") != std::string::npos);

    demo::set_sort_mode(state, demo::SortMode::Kind);
    snap = demo::snapshot(state);
    assert(snap.sort_label == "Sort: Kind");
    assert(state.status == "Sorted by Kind");
    std::string previous_kind;
    for (auto const& entry : snap.entries) {
        if (entry.folder)
            continue;
        auto current_kind = demo::lower_copy(demo::entry_kind_label(entry));
        assert(previous_kind.empty() || previous_kind <= current_kind);
        previous_kind = std::move(current_kind);
    }
    demo::cycle_sort_mode(state);
    snap = demo::snapshot(state);
    assert(snap.sort_label == "Sort: Size");
    std::uintmax_t previous_size = 0;
    bool saw_sized_file = false;
    for (auto const& entry : snap.entries) {
        if (entry.folder)
            continue;
        if (saw_sized_file)
            assert(previous_size >= entry.size);
        previous_size = entry.size;
        saw_sized_file = true;
    }
    demo::cycle_sort_mode(state);
    assert(demo::snapshot(state).sort_label == "Sort: Name");

    demo::apply_startup_scenario(state, "documents-preview");
    assert(demo::relative_location(state.root, state.current)
        == "Demo Root/Documents");
    assert(state.selected_name == "Project Notes.txt");
    assert(demo::snapshot(state).preview.find("Finder-like desktop layout")
        != std::string::npos);
    assert(demo::snapshot(state).can_go_back);

    demo::go_back(state);
    assert(demo::same_path(state.current, state.root));
    assert(state.status == "Went back to Demo Root");
    assert(demo::snapshot(state).can_go_forward);

    demo::go_forward(state);
    assert(demo::relative_location(state.root, state.current)
        == "Demo Root/Documents");
    assert(state.status == "Went forward to Demo Root/Documents");
    assert(demo::snapshot(state).can_go_back);

    demo::select_location(state, "documents");
    assert(demo::relative_location(state.root, state.current)
        == "Demo Root/Documents");
    demo::go_up(state);
    assert(demo::same_path(state.current, state.root));
    assert(state.forward_stack.empty());

    demo::apply_startup_scenario(state, "history-forward");
    assert(demo::relative_location(state.root, state.current)
        == "Demo Root/Documents");
    assert(state.status == "Went forward to Demo Root/Documents");

    demo::apply_startup_scenario(state, "sorted-kind");
    assert(demo::snapshot(state).sort_label == "Sort: Kind");
    assert(state.status == "Sorted by Kind");

    demo::apply_startup_scenario(state, "search-active");
    snap = demo::snapshot(state);
    assert(state.search == "Screen");
    assert(state.status == "Searching for Screen");
    assert(demo::explorer_chrome_metrics(state, profile).status_bar_visible);
    assert(!snap.has_selection);
    assert(!snap.entries.empty());
    for (auto const& entry : snap.entries) {
        assert(demo::lower_copy(entry.name).find("screen") != std::string::npos);
    }

    demo::apply_startup_scenario(state, "duplicated-file");
    assert(state.selected_name == "README copy.txt");
    assert(fs::exists(state.current / "README copy.txt"));
    assert(state.status == "Duplicated README.txt to README copy.txt");

    std::string const drive_profile = "test-cli-drive";
    auto drive_root = demo::demo_root(drive_profile);
    fs::remove_all(drive_root, ec);
    std::vector<demo::ExplorerInput> inputs{
        {.kind = demo::ExplorerInputKind::ViewMode,
         .value = "list",
         .view_mode = demo::ExplorerViewMode::List},
        {.kind = demo::ExplorerInputKind::Viewport,
         .value = "900x620@2",
         .viewport_width = 900,
         .viewport_height = 620,
         .viewport_scale = 2.0f},
        {.kind = demo::ExplorerInputKind::DraftName, .value = "CLI Note"},
        {.kind = demo::ExplorerInputKind::DraftBody,
         .value = "Created from typed CLI input."},
        {.kind = demo::ExplorerInputKind::CreateFile},
        {.kind = demo::ExplorerInputKind::DuplicateSelected},
        {.kind = demo::ExplorerInputKind::DeleteSelected},
        {.kind = demo::ExplorerInputKind::SelectLocation, .value = "trash"},
        {.kind = demo::ExplorerInputKind::Sort,
         .value = "kind",
         .sort_mode = demo::SortMode::Kind},
    };
    auto driven = demo::drive_explorer(drive_profile, inputs);
    assert(driven.profile == drive_profile);
    assert(driven.trace.size() == inputs.size());
    assert(driven.snapshot.view_mode == demo::ExplorerViewMode::List);
    assert(driven.state.viewport_width == 900);
    assert(driven.state.viewport_height == 620);
    assert(driven.state.viewport_scale == 2.0f);
    assert(driven.chrome.viewport.width == 900);
    assert(driven.chrome.viewport.height == 620);
    assert(driven.chrome.viewport.scale == 2.0f);
    assert(driven.chrome.icon_grid_columns == 3);
    assert(driven.trace[0].status == "Switched to List View.");
    assert(driven.trace[1].chrome.viewport.width == 900);
    assert(driven.trace[1].chrome.icon_grid_columns == 3);
    assert(driven.trace[1].status == "Viewport set to 900x620@2");
    assert(!driven.trace[1].chrome.status_bar_visible);
    assert(driven.trace[4].operation.kind == "file_create");
    assert(driven.trace[4].operation.ok);
    assert(driven.trace[5].operation.kind == "file_duplicate");
    assert(driven.trace[5].operation.ok);
    assert(driven.trace[6].operation.kind == "file_delete");
    assert(driven.trace[6].operation.ok);
    assert(driven.snapshot.relative_location == "Trash");
    assert(driven.snapshot.sort_label == "Sort: Kind");
    bool saw_cli_note_copy = false;
    for (auto const& entry : driven.snapshot.entries) {
        if (entry.name == "CLI Note copy.txt")
            saw_cli_note_copy = true;
    }
    assert(saw_cli_note_copy);
    std::vector<demo::ExplorerExpectation> expectations{
        {.kind = demo::ExplorerExpectationKind::Location, .value = "Trash"},
        {.kind = demo::ExplorerExpectationKind::Entry,
         .value = "CLI Note copy.txt"},
        {.kind = demo::ExplorerExpectationKind::MissingEntry,
         .value = "CLI Note.txt"},
        {.kind = demo::ExplorerExpectationKind::Operation,
         .value = "file_delete",
         .expects_operation_ok = true,
         .operation_ok = true},
        {.kind = demo::ExplorerExpectationKind::StatusContains,
         .value = "Sorted"},
    };
    auto checked = demo::check_explorer_expectations(driven, expectations);
    assert(checked.size() == expectations.size());
    assert(demo::explorer_expectations_ok(checked));
    assert(checked[0].actual == "Trash");
    assert(checked[1].actual == "CLI Note copy.txt");
    assert(checked[2].actual == "<missing>");
    auto failed_expectation = demo::check_explorer_expectation(
        driven,
        {.kind = demo::ExplorerExpectationKind::Selected,
         .value = "CLI Note.txt"});
    assert(!failed_expectation.ok);
    assert(failed_expectation.actual == "<none>");
    fs::remove_all(drive_root, ec);

    std::string const navigation_profile = "test-cli-navigation";
    auto navigation_root = demo::demo_root(navigation_profile);
    fs::remove_all(navigation_root, ec);
    std::vector<demo::ExplorerInput> open_inputs{
        {.kind = demo::ExplorerInputKind::OpenEntry, .value = "Documents"},
        {.kind = demo::ExplorerInputKind::SelectEntry,
         .value = "Project Notes.txt"},
    };
    auto opened = demo::drive_explorer(
        navigation_profile,
        open_inputs);
    assert(opened.snapshot.relative_location == "Demo Root/Documents");
    assert(opened.snapshot.has_selection);
    assert(opened.snapshot.selected.name == "Project Notes.txt");
    assert(opened.state.last_operation.kind == "file_read");

    fs::remove_all(navigation_root, ec);
    std::vector<demo::ExplorerInput> activate_inputs{
        {.kind = demo::ExplorerInputKind::ActivateEntry, .value = "Documents"},
        {.kind = demo::ExplorerInputKind::ActivateSelected},
    };
    auto activated = demo::drive_explorer(
        navigation_profile,
        activate_inputs);
    assert(activated.trace.size() == 2);
    assert(activated.trace[0].selected_name == "Documents");
    assert(activated.trace[0].operation.kind == "folder_select");
    assert(activated.trace[1].operation.kind == "folder_open");
    assert(activated.trace[1].operation.ok);
    assert(activated.snapshot.relative_location == "Demo Root/Documents");
    fs::remove_all(navigation_root, ec);

    fs::remove_all(navigation_root, ec);
    std::vector<demo::ExplorerInput> keyboard_inputs{
        {.kind = demo::ExplorerInputKind::MoveSelection,
         .value = "down",
         .selection_move = demo::ExplorerSelectionMove::Down},
        {.kind = demo::ExplorerInputKind::MoveSelection,
         .value = "right",
         .selection_move = demo::ExplorerSelectionMove::Right},
    };
    auto keyboard_driven = demo::drive_explorer(
        navigation_profile,
        keyboard_inputs);
    assert(keyboard_driven.trace.size() == 2);
    assert(keyboard_driven.trace[0].selected_name
           == keyboard_driven.snapshot.entries[0].name);
    assert(keyboard_driven.trace[0].operation.kind == "file_read");
    assert(keyboard_driven.trace[0].operation.ok);
    assert(keyboard_driven.snapshot.has_selection);
    assert(keyboard_driven.snapshot.selected_index == 1);
    assert(keyboard_driven.snapshot.selected.name
           == keyboard_driven.snapshot.entries[1].name);
    fs::remove_all(navigation_root, ec);

    demo::reset_demo_tree(state, profile);
    assert(!demo::snapshot(state).can_go_back);
    assert(!demo::snapshot(state).can_go_forward);
    assert(demo::snapshot(state).operation_label.empty());

    fs::remove_all(root, ec);
    std::puts("PASS: file explorer model contract");
#endif
}

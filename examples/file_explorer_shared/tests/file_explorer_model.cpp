#include <cassert>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

import file_explorer_shared;
import json;
import phenotype.icon_catalog;
import phenotype.resources;

namespace {

[[noreturn]] void fail_assert(char const* expression, char const* file, int line) {
    std::cerr << "FAIL: " << file << ":" << line << ": " << expression << "\n";
    std::exit(1);
}

std::string unique_test_profile(std::string_view stem) {
    auto stamp = std::chrono::steady_clock::now()
        .time_since_epoch()
        .count();
    auto marker = reinterpret_cast<std::uintptr_t>(&stamp);
    return std::string{stem} + "-" + std::to_string(stamp) + "-"
        + std::to_string(marker);
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
    auto required_locale_keys =
        demo::resource_contract_locale_keys(catalog);
    assert(required_locale_keys.size() == 66);
    auto contract = phenotype::resource_catalog_contract(
        catalog,
        std::span<std::string_view const>{required_locale_keys});
    assert(contract.asset_count == 13);
    assert(contract.svg_asset_count == 12);
    assert(contract.preload_svg_asset_count == 12);
    assert(contract.runtime_visible_svg_asset_count == 11);
    assert(contract.app_icon_declared);
    assert(contract.app_icon_svg);
    assert(contract.app_icon_preload);
    assert(contract.default_font_has_cjk_fallback);
    assert(contract.debug_artifact_manifest_declared);
    assert(contract.debug_probe_scene_declared);
    assert(contract.debug_verifier_declared);
    assert(demo::file_explorer_labels("ko", "desktop").sidebar_recents
        == "최근 항목");
    assert(demo::file_explorer_labels("ko", "desktop").preferences_scroll_faster
        == "스크롤 +");
    assert(demo::file_explorer_labels("ko", "desktop").preferences_body_larger
        == "본문 +");
    assert(demo::file_explorer_labels("ko", "desktop")
               .preferences_line_height_tighter
        == "행간 -");
    assert(demo::file_explorer_labels("ko", "desktop").preferences_system_text_size
        == "OS 크기");
    assert(demo::file_explorer_labels("ko", "desktop").preferences_dark_appearance
        == "다크");
    assert(demo::file_explorer_labels("ko", catalog).sidebar_recents
        == "최근 항목");
    assert(demo::file_explorer_labels("en", catalog).preferences_system_font
        == "System");
    assert(demo::file_explorer_labels("ja", "desktop").sidebar_recents
        == "Recents");
    assert(demo::resolve_supported_locale(catalog, "ko-KR") == "ko");
    assert(demo::resolve_supported_locale(catalog, "ko_KR") == "ko");
    assert(demo::resolve_supported_locale(catalog, "ja-JP") == "en");
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
    assert(parsed_enter.input.modality == demo::ExplorerInputModality::Keyboard);
    auto parsed_delete_key = demo::parse_explorer_input("key:delete");
    assert(parsed_delete_key.ok);
    assert(parsed_delete_key.input.kind
           == demo::ExplorerInputKind::DeleteSelected);
    assert(parsed_delete_key.input.modality
           == demo::ExplorerInputModality::Keyboard);
    auto parsed_duplicate_shortcut =
        demo::parse_explorer_input("shortcut:duplicate");
    assert(parsed_duplicate_shortcut.ok);
    assert(parsed_duplicate_shortcut.input.kind
           == demo::ExplorerInputKind::DuplicateSelected);
    assert(parsed_duplicate_shortcut.input.modality
           == demo::ExplorerInputModality::Keyboard);
    auto parsed_find_shortcut =
        demo::parse_explorer_input("shortcut:find");
    assert(parsed_find_shortcut.ok);
    assert(parsed_find_shortcut.input.kind
           == demo::ExplorerInputKind::FocusSearch);
    assert(parsed_find_shortcut.input.modality
           == demo::ExplorerInputModality::Keyboard);
    auto parsed_tab = demo::parse_explorer_input("key:tab");
    assert(parsed_tab.ok);
    assert(parsed_tab.input.kind == demo::ExplorerInputKind::TabFocus);
    assert(parsed_tab.input.modality == demo::ExplorerInputModality::Keyboard);
    auto parsed_shift_tab = demo::parse_explorer_input("shift-tab");
    assert(parsed_shift_tab.ok);
    assert(parsed_shift_tab.input.kind
           == demo::ExplorerInputKind::ShiftTabFocus);
    auto parsed_focus_search = demo::parse_explorer_input("focus:search");
    assert(parsed_focus_search.ok);
    assert(parsed_focus_search.input.kind
           == demo::ExplorerInputKind::FocusTarget);
    assert(parsed_focus_search.input.value == "search");
    auto parsed_pointer_content =
        demo::parse_explorer_input("pointer:content-grid");
    assert(parsed_pointer_content.ok);
    assert(parsed_pointer_content.input.kind
           == demo::ExplorerInputKind::PointerFocus);
    assert(parsed_pointer_content.input.modality
           == demo::ExplorerInputModality::Pointer);
    assert(parsed_pointer_content.input.value == "content_grid");
    auto parsed_click = demo::parse_explorer_input("click:README.txt");
    assert(parsed_click.ok);
    assert(parsed_click.input.kind == demo::ExplorerInputKind::ActivateEntry);
    assert(parsed_click.input.modality == demo::ExplorerInputModality::Pointer);
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
    auto parsed_font_system = demo::parse_explorer_input("font-family:system");
    assert(parsed_font_system.ok);
    assert(parsed_font_system.input.kind
           == demo::ExplorerInputKind::SetFontFamily);
    assert(parsed_font_system.input.value == "system");
    auto parsed_font_package = demo::parse_explorer_input("font:pretendard");
    assert(parsed_font_package.ok);
    assert(parsed_font_package.input.kind
           == demo::ExplorerInputKind::SetFontFamily);
    assert(parsed_font_package.input.value == "pretendard");
    auto parsed_font_scale = demo::parse_explorer_input("font-scale:1.25");
    assert(parsed_font_scale.ok);
    assert(parsed_font_scale.input.kind
           == demo::ExplorerInputKind::SetFontScale);
    assert(parsed_font_scale.input.value == "1.25");
    assert(demo::explorer_input_label(parsed_font_scale.input)
           == "set_font_scale:1.25");
    auto parsed_large_font_scale = demo::parse_explorer_input("font-scale:5");
    assert(parsed_large_font_scale.ok);
    assert(parsed_large_font_scale.input.value == "1.8");
    auto parsed_font_size = demo::parse_explorer_input("font-size:17");
    assert(parsed_font_size.ok);
    assert(parsed_font_size.input.kind
           == demo::ExplorerInputKind::SetBodyFontSize);
    assert(parsed_font_size.input.value == "17");
    auto parsed_heading_font_size =
        demo::parse_explorer_input("heading-font-size:22");
    assert(parsed_heading_font_size.ok);
    assert(parsed_heading_font_size.input.kind
           == demo::ExplorerInputKind::SetHeadingFontSize);
    auto parsed_small_font_size =
        demo::parse_explorer_input("small-font-size:13");
    assert(parsed_small_font_size.ok);
    assert(parsed_small_font_size.input.kind
           == demo::ExplorerInputKind::SetSmallFontSize);
    auto parsed_line_height = demo::parse_explorer_input("line-height:1.45");
    assert(parsed_line_height.ok);
    assert(parsed_line_height.input.kind
           == demo::ExplorerInputKind::SetLineHeightRatio);
    auto parsed_font_metrics =
        demo::parse_explorer_input("system-font-metrics:false");
    assert(parsed_font_metrics.ok);
    assert(parsed_font_metrics.input.kind
           == demo::ExplorerInputKind::SetSystemFontMetrics);
    assert(parsed_font_metrics.input.value == "false");
    auto parsed_scroll_metrics =
        demo::parse_explorer_input("system-scroll-metrics:app");
    assert(parsed_scroll_metrics.ok);
    assert(parsed_scroll_metrics.input.kind
           == demo::ExplorerInputKind::SetSystemScrollMetrics);
    assert(parsed_scroll_metrics.input.value == "app");
    auto parsed_scroll_speed = demo::parse_explorer_input("scroll-speed:1.5");
    assert(parsed_scroll_speed.ok);
    assert(parsed_scroll_speed.input.kind
           == demo::ExplorerInputKind::SetScrollSpeed);
    assert(parsed_scroll_speed.input.value == "1.5");
    auto parsed_slow_scroll = demo::parse_explorer_input("scroll-speed:0.1");
    assert(parsed_slow_scroll.ok);
    assert(parsed_slow_scroll.input.value == "0.25");
    auto parsed_horizontal_scroll =
        demo::parse_explorer_input("scroll-x-speed:2.25");
    assert(parsed_horizontal_scroll.ok);
    assert(parsed_horizontal_scroll.input.kind
           == demo::ExplorerInputKind::SetHorizontalScrollSpeed);
    assert(parsed_horizontal_scroll.input.value == "2.25");
    auto parsed_bad_font_scale = demo::parse_explorer_input("font-scale:big");
    assert(!parsed_bad_font_scale.ok);
    assert(parsed_bad_font_scale.error.find("font-scale") != std::string::npos);
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
    auto expected_focus =
        demo::parse_explorer_expectation("focus-target:search");
    assert(expected_focus.ok);
    assert(expected_focus.expectation.kind
           == demo::ExplorerExpectationKind::FocusTarget);
    assert(expected_focus.expectation.value == "search");
    auto expected_focus_visible =
        demo::parse_explorer_expectation("focus-visible:true");
    assert(expected_focus_visible.ok);
    assert(expected_focus_visible.expectation.kind
           == demo::ExplorerExpectationKind::FocusVisible);
    auto expected_input_modality =
        demo::parse_explorer_expectation("input-modality:keyboard");
    assert(expected_input_modality.ok);
    assert(expected_input_modality.expectation.kind
           == demo::ExplorerExpectationKind::InputModality);
    auto bad_expectation = demo::parse_explorer_expectation("selected");
    assert(!bad_expectation.ok);

    std::string const profile = unique_test_profile("test-model-contract");
    auto root = demo::demo_root(profile);
    std::error_code ec;
    fs::remove_all(root, ec);

    std::string const startup_profile = unique_test_profile("test-startup-inputs");
    fs::remove_all(demo::demo_root(startup_profile), ec);
    auto startup_state = demo::make_state(startup_profile);
    demo::apply_explorer_inputs(
        startup_state,
        parsed_sequence.inputs,
        startup_profile);
    assert(startup_state.selected_name == "README copy.txt");
    assert(startup_state.last_operation.kind == "file_duplicate");
    fs::remove_all(startup_state.root, ec);

    std::string const preference_profile =
        unique_test_profile("test-preference-inputs");
    fs::remove_all(demo::demo_root(preference_profile), ec);
    auto preference_state = demo::make_state(preference_profile);
    auto preference_sequence = demo::parse_explorer_input_sequence(
        "font-family:system;font-scale:1.25;font-size:17;"
        "heading-font-size:22;small-font-size:13;line-height:1.45;"
        "system-font-metrics:false;system-scroll-metrics:app;scroll-speed:1.5;"
        "horizontal-scroll-speed:2;motion-scale:0;color-scheme:system");
    assert(preference_sequence.ok);
    demo::apply_explorer_inputs(
        preference_state,
        preference_sequence.inputs,
        preference_profile);
    assert(preference_state.preferences_source == "application-input");
    assert(preference_state.theme_preferences.prefer_system_font_family);
    assert(preference_state.theme_preferences.font_family.empty());
    assert(preference_state.theme_preferences.font_scale == 1.25f);
    assert(preference_state.theme_preferences.body_font_size == 17.0f);
    assert(preference_state.theme_preferences.heading_font_size == 22.0f);
    assert(preference_state.theme_preferences.small_font_size == 13.0f);
    assert(std::fabs(preference_state.theme_preferences.line_height_ratio - 1.45f)
           < 0.001f);
    assert(!preference_state.theme_preferences.apply_system_font_metrics);
    assert(!preference_state.theme_preferences.apply_system_scroll_metrics);
    assert(preference_state.theme_preferences.scroll_delta_multiplier == 1.5f);
    assert(preference_state.theme_preferences.scroll_horizontal_delta_multiplier
           == 2.0f);
    assert(preference_state.theme_preferences.motion_duration_multiplier
           == 0.0f);
    assert(preference_state.theme_preferences.prefer_system_color_scheme);
    assert(preference_state.status == "Using the OS appearance.");
    auto preference_debug_text = json::emit(
        demo::file_explorer_application_debug_json(
            preference_state,
            demo::snapshot(preference_state),
            demo::explorer_chrome_metrics(
                preference_state,
                preference_profile),
            preference_profile));
    assert(preference_debug_text.find("\"source\":\"application-input\"")
           != std::string::npos);
    assert(preference_debug_text.find("\"prefer_system_font_family\":true")
           != std::string::npos);
    assert(preference_debug_text.find("\"font_scale\":1.25")
           != std::string::npos);
    assert(preference_debug_text.find("\"effective_theme\"")
           != std::string::npos);
    assert(preference_debug_text.find("\"heading_font_size\"")
           != std::string::npos);
    assert(preference_debug_text.find("\"line_height_ratio\"")
           != std::string::npos);
    assert(preference_debug_text.find("\"resolution\"")
           != std::string::npos);
    assert(preference_debug_text.find("\"used_system_font_metrics\"")
           != std::string::npos);
    assert(preference_debug_text.find("\"used_user_scroll_scale\"")
           != std::string::npos);
    assert(preference_debug_text.find("\"used_user_motion_scale\"")
           != std::string::npos);
    assert(preference_debug_text.find("\"scroll_delta_multiplier\":1.5")
           != std::string::npos);
    assert(preference_debug_text.find("\"motion_duration_multiplier\":0")
           != std::string::npos);
    assert(preference_debug_text.find("\"apply_system_scroll_metrics\":false")
           != std::string::npos);
    assert(preference_debug_text.find("\"system_refresh_policy\"")
           != std::string::npos);
    assert(preference_debug_text.find(
               "\"scroll_horizontal_delta_multiplier\":2")
           != std::string::npos);
    assert(preference_debug_text.find("\"prefer_system_color_scheme\":true")
           != std::string::npos);
    assert(preference_debug_text.find(
               "\"scroll_horizontal_delta_multiplier\"")
           != std::string::npos);
    fs::remove_all(preference_state.root, ec);

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
    assert(chrome.sidebar_first_row_y == 64.0f);
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
    assert(chrome.thumbnail_pdf_page_width
           == demo::k_desktop_thumbnail_pdf_page_width);
    assert(chrome.thumbnail_pdf_page_height
           == demo::k_desktop_thumbnail_pdf_page_height);
    assert(chrome.thumbnail_pdf_page_radius
           == demo::k_desktop_thumbnail_pdf_page_radius);
    assert(chrome.thumbnail_pdf_fold_size
           == demo::k_desktop_thumbnail_pdf_fold_size);
    assert(chrome.thumbnail_media_preview_width
           == demo::k_desktop_thumbnail_media_preview_width);
    assert(chrome.thumbnail_media_preview_height
           == demo::k_desktop_thumbnail_media_preview_height);
    assert(chrome.thumbnail_media_preview_radius
           == demo::k_desktop_thumbnail_media_preview_radius);
    assert(chrome.thumbnail_pdf_detail_line_count
           == demo::k_desktop_thumbnail_pdf_detail_line_count);
    assert(chrome.thumbnail_image_sample_block_count
           == demo::k_desktop_thumbnail_image_sample_block_count);
    assert(chrome.thumbnail_video_strip_count
           == demo::k_desktop_thumbnail_video_strip_count);
    assert(!chrome.thumbnail_uses_external_previews);
    assert(chrome.thumbnail_visual_policy
           == demo::k_desktop_thumbnail_visual_policy);
    assert(chrome.thumbnail_asset_policy
           == demo::k_desktop_thumbnail_asset_policy);
    assert(chrome.thumbnail_pdf_policy == demo::k_desktop_thumbnail_pdf_policy);
    assert(chrome.thumbnail_image_policy
           == demo::k_desktop_thumbnail_image_policy);
    assert(chrome.thumbnail_svg_policy
           == demo::k_desktop_thumbnail_svg_policy);
    assert(chrome.thumbnail_svg_render_policy
           == demo::k_desktop_thumbnail_svg_render_policy);
    assert(chrome.thumbnail_svg_preview_source_policy
           == demo::k_desktop_thumbnail_svg_preview_source_policy);
    assert(chrome.thumbnail_svg_external_resource_policy
           == demo::k_desktop_thumbnail_svg_external_resource_policy);
    assert(chrome.thumbnail_svg_document_cache_policy
           == demo::k_desktop_thumbnail_svg_document_cache_policy);
    assert(chrome.thumbnail_svg_document_cache_limit
           == demo::k_desktop_thumbnail_svg_document_cache_limit);
    assert(chrome.thumbnail_video_policy
           == demo::k_desktop_thumbnail_video_policy);
    assert(chrome.thumbnail_shadow_policy
           == demo::k_desktop_thumbnail_shadow_policy);
    assert(chrome.icon_grid_label_height
           == demo::k_desktop_icon_grid_label_height);
    assert(chrome.icon_grid_label_font_size
           == demo::k_desktop_icon_grid_label_font_size);
    assert(chrome.icon_grid_label_policy
           == demo::k_desktop_icon_grid_label_policy);
    assert(chrome.icon_grid_top_inset
           == demo::k_desktop_icon_grid_top_inset);
    assert(chrome.icon_grid_gap == demo::k_desktop_icon_grid_gap);
    assert(chrome.finder_density_policy
           == demo::k_desktop_finder_density_policy);
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
    assert(chrome.icon_total_symbol_count == 39);
    assert(chrome.sidebar_symbol_count == 11);
    assert(chrome.toolbar_symbol_count == 15);
    assert(chrome.file_type_symbol_count == 11);
    assert(chrome.icon_filled_symbol_count == 1);
    assert(chrome.icon_outline_symbol_count == 38);
    assert(chrome.icon_hierarchical_symbol_count == 17);
    assert(chrome.icon_reference_symbol_count == 39);
    assert(chrome.icon_svg_path_arc_symbol_count == 16);
    assert(chrome.icon_phenotype_owned_symbol_count == 4);
    assert(chrome.icon_permissive_source_symbol_count == 35);
    assert(chrome.icon_lucide_source_symbol_count == 35);
    assert(chrome.icon_lucide_unique_source_icon_count == 34);
    assert(chrome.icon_apple_asset_symbol_count == 0);
    assert(chrome.icon_platform_extracted_symbol_count == 0);
    assert(chrome.icon_runtime_fetched_symbol_count == 0);
    assert(chrome.icon_audited_symbol_source_count == chrome.icon_total_symbol_count);
    assert(chrome.icon_interaction_phase_count == 3);
    assert(chrome.icon_grid_size == 24.0f);
    assert(demo::desktop_sidebar_symbol_contract().size() == 10);
    assert(demo::file_type_symbol_contract().size() == 11);
    for (std::size_t i = 0; i < demo::file_type_symbol_contract().size(); ++i) {
        assert(demo::file_type_symbol_contract()[i].symbol
               == phenotype::icon_catalog::file_type_symbol_at(
                   static_cast<unsigned int>(i)));
    }
    assert(demo::sidebar_symbol_name_for_token("recents") == "recents");
    assert(demo::sidebar_symbol_name_for_token("shared") == "shared");
    assert(demo::sidebar_symbol_name_for_token("desktop") == "desktop");
    assert(demo::sidebar_symbol_name_for_token("download") == "download");
    assert(demo::sidebar_symbol_name_for_token("unknown-token") == "folder");
    demo::Entry pdf_entry{.name = "Application Form 3.pdf"};
    demo::Entry text_entry{.name = "README.txt"};
    demo::Entry archive_entry{.name = "Archive.zip"};
    demo::Entry audio_entry{.name = "Voice Memo.m4a"};
    demo::Entry code_entry{.name = "Glass Theme Notes.cpp"};
    demo::Entry sheet_entry{.name = "Expense Matrix.csv"};
    demo::Entry presentation_entry{.name = "Design Review.key"};
    demo::Entry svg_entry{.name = "Glass Symbol.svg"};
    assert(demo::entry_symbol_name(pdf_entry) == "pdf_document");
    assert(demo::entry_symbol_semantic_reference_name(pdf_entry)
           == "doc.richtext");
    assert(demo::entry_symbol_name(text_entry) == "text_document");
    assert(demo::entry_symbol_semantic_reference_name(text_entry)
           == "doc.plaintext");
    assert(demo::entry_symbol_name(archive_entry) == "archive");
    assert(demo::entry_symbol_semantic_reference_name(archive_entry)
           == "archivebox");
    assert(demo::entry_symbol_name(audio_entry) == "audio_document");
    assert(demo::entry_symbol_semantic_reference_name(audio_entry)
           == "music.note");
    assert(demo::entry_symbol_name(code_entry) == "code_document");
    assert(demo::entry_symbol_semantic_reference_name(code_entry)
           == "chevron.left.forwardslash.chevron.right");
    assert(demo::entry_symbol_name(sheet_entry) == "spreadsheet_document");
    assert(demo::entry_symbol_semantic_reference_name(sheet_entry)
           == "tablecells");
    assert(demo::entry_symbol_name(presentation_entry)
           == "presentation_document");
    assert(demo::entry_symbol_semantic_reference_name(presentation_entry)
           == "rectangle.on.rectangle.angled");
    assert(demo::entry_kind_label(svg_entry) == "SVG Image");
    assert(demo::entry_symbol_name(svg_entry) == "image");
    assert(demo::entry_symbol_semantic_reference_name(svg_entry)
           == "photo");
    assert(chrome.icon_default_stroke_width == 2.0f);
    assert(chrome.icon_secondary_opacity == 1.0f);
    assert(chrome.icon_toolbar_activation_hit_target_size == 44.0f);
    assert(chrome.icon_sidebar_activation_hit_target_size == 44.0f);
    assert(chrome.icon_action_activation_hit_target_size == 44.0f);
    assert(chrome.icon_toolbar_button_radius == 15.0f);
    assert(chrome.icon_toolbar_button_background_alpha == 0);
    assert(chrome.icon_toolbar_button_hover_background_alpha == 120);
    assert(chrome.icon_toolbar_selected_button_background_alpha == 150);
    assert(chrome.icon_toolbar_selected_button_hover_background_alpha == 190);
    assert(chrome.icon_toolbar_pressed_button_background_alpha == 150);
    assert(chrome.icon_sidebar_selected_pressed_background_alpha == 255);
    assert(chrome.icon_pressed_symbol_opacity == 0.82f);
    assert(chrome.icon_pressed_scale == 0.985f);
    assert(chrome.icon_module == "phenotype.icons");
    assert(chrome.icon_style == "macos_rounded_outline_svg");
    assert(chrome.icon_source_format == "svg");
    assert(chrome.icon_svg_subset_policy == "bounded_svg_icon_subset");
    assert(chrome.icon_svg_supported_elements.find("circle")
           != std::string::npos);
    assert(chrome.icon_svg_supported_path_commands.find("A Z")
           != std::string::npos);
    assert(chrome.icon_svg_arc_policy.find("bounded cubic Bezier")
           != std::string::npos);
    assert(chrome.icon_design_reference.find("macOS Finder")
           != std::string::npos);
    assert(chrome.icon_reference_family == "SF Symbols semantic reference");
    assert(chrome.icon_reference_policy.find("audited permissive")
           != std::string::npos);
    assert(chrome.icon_asset_policy.find("no Apple") != std::string::npos);
    assert(chrome.icon_source_attribution_policy.find("direct raw SVG URL")
           != std::string::npos);
    assert(chrome.icon_source_attribution_policy.find("source revision")
           != std::string::npos);
    assert(chrome.icon_source_attribution_policy.find("platform extraction flag")
           != std::string::npos);
    assert(chrome.icon_source_acquisition_policy.find(
               "runtime uses embedded SVG strings")
           != std::string::npos);
    assert(chrome.icon_source_acquisition_policy.find(
               "platform icon extraction disabled")
           != std::string::npos);
    assert(chrome.icon_document_cache_policy.find(
               "keyed_by_symbol_descriptor")
           != std::string::npos);
    assert(chrome.icon_document_cache_policy.find("no_frame_parse_churn")
           != std::string::npos);
    assert(chrome.icon_alignment == "24x24 text-aligned symbol grid");
    assert(chrome.icon_rendering_mode == "hierarchical");
    assert(chrome.icon_variant_policy
           == "outline primary with filled action variants");
    assert(chrome.icon_interaction_tone_policy
           == "macos_finder_interaction_tones");
    assert(chrome.icon_symbol_control_chrome_policy
           == "macos_finder_symbol_state_chrome");
    assert(chrome.icon_symbol_interaction_phase_policy
           == "macos_finder_normal_hover_pressed_symbol_chrome");
    assert(chrome.icon_file_type_color_policy
           == "macos_finder_file_type_tints");
    assert(chrome.icon_scale == "medium");
    assert(chrome.theme_contract_version == 2);
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
    assert(!chrome.owned_icon_assets);
    assert(chrome.audited_permissive_icon_assets);
    assert(!chrome.uses_apple_icon_assets);
    assert(!chrome.uses_sf_symbols_assets);
    assert(chrome.icon_round_stroke_contract);
    assert(chrome.icon_text_weight_aligned);
    assert(chrome.icon_hierarchical_opacity);
    assert(chrome.finder_segmented_toolbar);
    assert(chrome.integrated_titlebar);
    assert(chrome.native_window_controls);
    assert(!chrome.duplicate_window_controls);
    assert(chrome.window_control_single_owner);
    assert(!chrome.content_window_control_markers);
    assert(!chrome.artifact_window_control_markers);
    assert(chrome.window_control_marker_mode == "runtime-native-controls");
    assert(chrome.native_window_control_owner == "platform-edge");
    assert(chrome.window_control_duplication_guard
           == "native_window_controls_single_owner");
    assert(chrome.native_window_control_count
           == demo::k_desktop_titlebar_control_count);
    assert(chrome.content_window_control_marker_count == 0);
    assert(chrome.artifact_window_control_marker_count == 0);
    assert(chrome.content_drawn_window_control_count == 0);
    assert(chrome.artifact_drawn_window_control_count == 0);
    assert(chrome.window_control_render_policy
           == "native_controls_runtime_only_no_content_or_artifact_markers");
    assert(chrome.titlebar_control_reserve_policy
           == "blank_reserve_under_os_window_controls");
    assert(chrome.native_window_control_integration_policy
           == "platform_standard_controls_inside_leading_content_reserve");
    auto finder_contract = demo::finder_visual_contract(chrome, profile);
    assert(finder_contract.schema_version == 1);
    assert(finder_contract.name == "finder_visual_parity_contract");
    assert(finder_contract.source
           == "file_explorer_shared::finder_visual_contract");
    assert(finder_contract.titlebar_strategy
           == "integrated_titlebar_content_reserve_with_platform_window_controls");
    assert(finder_contract.native_control_owner == "platform-edge");
    assert(finder_contract.native_control_marker_policy
           == "no_content_or_artifact_window_control_markers");
    assert(finder_contract.native_control_geometry_role
           == "reserve_metrics_only_not_paint_instructions");
    assert(finder_contract.native_control_palette_policy
           == "traffic_light_palette_forbidden_in_content_and_artifacts");
    assert(finder_contract.sidebar_selection_style
           == demo::k_desktop_sidebar_selection_policy);
    assert(finder_contract.sidebar_selected_row_border_width == 0.0f);
    assert(finder_contract.sidebar_selected_row_background_alpha
           == demo::k_desktop_sidebar_selected_row_background_alpha);
    assert(finder_contract.focus_ring_policy
           == "keyboard_tab_navigation_only_pointer_click_hides_focus_ring");
    assert(finder_contract.icon_source_policy.find("permissive SVG")
           != std::string::npos);
    assert(finder_contract.embedded_svg_policy.find("pinned direct raw SVG URL")
           != std::string::npos);
    assert(finder_contract.apple_asset_symbol_count == 0);
    assert(finder_contract.platform_extracted_symbol_count == 0);
    assert(finder_contract.runtime_fetched_symbol_count == 0);
    assert(finder_contract.thumbnail_preview_policy
           == demo::k_desktop_thumbnail_visual_policy);
    assert(finder_contract.leading_control_reserved_width
           == demo::k_desktop_leading_control_reserved_width);
    assert(finder_contract.titlebar_drag_region_height
           == demo::k_desktop_titlebar_drag_region_height);
    assert(finder_contract.verifier_gate
           == "local_file_explorer_artifact_verify_not_default_pr_ci");
    auto artifact_chrome =
        demo::explorer_chrome_with_native_window_control_ownership(chrome);
    assert(!artifact_chrome.content_window_control_markers);
    assert(!artifact_chrome.artifact_window_control_markers);
    assert(!artifact_chrome.duplicate_window_controls);
    assert(artifact_chrome.window_control_single_owner);
    assert(artifact_chrome.window_control_marker_mode
           == "runtime-native-controls");
    assert(artifact_chrome.window_control_duplication_guard
           == "native_window_controls_single_owner");
    auto poisoned_chrome = chrome;
    poisoned_chrome.duplicate_window_controls = true;
    poisoned_chrome.content_window_control_markers = true;
    poisoned_chrome.artifact_window_control_markers = true;
    poisoned_chrome.content_window_control_marker_count = 3;
    poisoned_chrome.artifact_window_control_marker_count = 3;
    poisoned_chrome.native_window_control_count = 0;
    poisoned_chrome.native_window_control_owner = "content";
    poisoned_chrome.window_control_marker_mode = "artifact-markers";
    auto repaired_chrome =
        demo::explorer_chrome_with_native_window_control_ownership(
            poisoned_chrome);
    assert(!repaired_chrome.duplicate_window_controls);
    assert(!repaired_chrome.content_window_control_markers);
    assert(!repaired_chrome.artifact_window_control_markers);
    assert(repaired_chrome.content_window_control_marker_count == 0);
    assert(repaired_chrome.artifact_window_control_marker_count == 0);
    assert(repaired_chrome.content_drawn_window_control_count == 0);
    assert(repaired_chrome.artifact_drawn_window_control_count == 0);
    assert(repaired_chrome.window_control_single_owner);
    assert(repaired_chrome.native_window_control_owner == "platform-edge");
    assert(repaired_chrome.window_control_marker_mode
           == "runtime-native-controls");
    assert(repaired_chrome.window_control_duplication_guard
           == "native_window_controls_single_owner");
    assert(repaired_chrome.native_window_control_count
           == demo::k_desktop_titlebar_control_count);
    assert(repaired_chrome.native_window_control_integration_policy
           == "platform_standard_controls_inside_leading_content_reserve");
    assert(!chrome.status_bar_visible);
    assert(demo::explorer_icon_grid_columns(chrome).size() == 6);
    std::string const viewport_profile =
        unique_test_profile("test-status-bar-viewport");
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
    assert(mobile_chrome.toolbar_icon_button_count == 4);
    assert(mobile_chrome.titlebar_control_count == 0);
    assert(mobile_chrome.native_window_control_count == 0);
    assert(mobile_chrome.content_window_control_marker_count == 0);
    assert(mobile_chrome.artifact_window_control_marker_count == 0);
    assert(mobile_chrome.content_drawn_window_control_count == 0);
    assert(mobile_chrome.artifact_drawn_window_control_count == 0);
    assert(mobile_chrome.window_control_single_owner);
    assert(mobile_chrome.native_window_control_owner == "none");
    auto mobile_finder_contract =
        demo::finder_visual_contract(mobile_chrome, "mobile");
    assert(mobile_finder_contract.titlebar_strategy
           == "not_applicable_mobile_shell");
    assert(mobile_finder_contract.native_control_owner == "none");
    assert(mobile_finder_contract.sidebar_selection_style == "n/a");
    assert(mobile_finder_contract.apple_asset_symbol_count == 0);
    assert(mobile_chrome.window_control_duplication_guard
           == "not_applicable_mobile_shell");
    assert(mobile_chrome.icon_module == "phenotype.icons");
    assert(mobile_chrome.icon_style == "macos_rounded_outline_svg");
    assert(mobile_chrome.file_type_symbol_count == 11);
    assert(mobile_chrome.icon_reference_symbol_count == 39);
    assert(!mobile_chrome.owned_icon_assets);
    assert(mobile_chrome.audited_permissive_icon_assets);
    assert(!mobile_chrome.uses_apple_icon_assets);
    assert(!mobile_chrome.uses_sf_symbols_assets);
    assert(mobile_chrome.icon_file_type_color_policy
           == "macos_finder_file_type_tints");
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

    std::string const keyboard_profile =
        unique_test_profile("test-keyboard-navigation");
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
    assert(keyboard_state.last_input_modality
           == demo::ExplorerInputModality::Keyboard);
    assert(keyboard_state.focus_target
           == demo::ExplorerFocusTarget::ContentGrid);
    assert(keyboard_state.focus_visible);
    assert(demo::focus_ring_visibility_reason(keyboard_state)
           == "keyboard_focus_navigation");
    fs::remove_all(keyboard_state.root, ec);

    std::string const focus_profile =
        unique_test_profile("test-focus-modality-contract");
    auto focus_state = demo::make_state(focus_profile);
    assert(focus_state.focus_target == demo::ExplorerFocusTarget::None);
    assert(!focus_state.focus_visible);
    demo::apply_explorer_input(focus_state, parsed_click.input, focus_profile);
    assert(focus_state.selected_name == "README.txt");
    assert(focus_state.last_input_modality
           == demo::ExplorerInputModality::Pointer);
    assert(focus_state.focus_target == demo::ExplorerFocusTarget::ContentGrid);
    assert(!focus_state.focus_visible);
    assert(demo::focus_ring_visibility_reason(focus_state)
           == "pointer_input_hides_focus_ring");
    demo::apply_explorer_input(focus_state, parsed_tab.input, focus_profile);
    assert(focus_state.last_input_modality
           == demo::ExplorerInputModality::Keyboard);
    assert(focus_state.focus_target == demo::ExplorerFocusTarget::PreviewPanel);
    assert(focus_state.focus_visible);
    demo::apply_explorer_input(
        focus_state,
        parsed_focus_search.input,
        focus_profile);
    assert(focus_state.focus_target == demo::ExplorerFocusTarget::Search);
    assert(focus_state.focus_visible);
    demo::apply_explorer_input(
        focus_state,
        parsed_pointer_content.input,
        focus_profile);
    assert(focus_state.focus_target == demo::ExplorerFocusTarget::ContentGrid);
    assert(!focus_state.focus_visible);
    demo::apply_explorer_input(
        focus_state,
        parsed_shift_tab.input,
        focus_profile);
    assert(focus_state.focus_target == demo::ExplorerFocusTarget::Search);
    assert(focus_state.focus_visible);
    demo::apply_explorer_input(
        focus_state,
        demo::ExplorerInput{
            .kind = demo::ExplorerInputKind::ViewMode,
            .value = "list",
            .modality = demo::ExplorerInputModality::Pointer,
            .view_mode = demo::ExplorerViewMode::List,
        },
        focus_profile);
    assert(focus_state.last_input_modality
           == demo::ExplorerInputModality::Pointer);
    assert(focus_state.focus_target == demo::ExplorerFocusTarget::Search);
    assert(!focus_state.focus_visible);
    assert(demo::focus_ring_visibility_reason(focus_state)
           == "pointer_input_hides_focus_ring");
    demo::apply_explorer_input(
        focus_state,
        parsed_focus_search.input,
        focus_profile);
    assert(focus_state.focus_visible);
    demo::apply_explorer_input(
        focus_state,
        demo::ExplorerInput{
            .kind = demo::ExplorerInputKind::Viewport,
            .value = "900x620@2",
            .viewport_width = 900,
            .viewport_height = 620,
            .viewport_scale = 2.0f,
        },
        focus_profile);
    assert(focus_state.last_input_modality
           == demo::ExplorerInputModality::Programmatic);
    assert(!focus_state.focus_visible);
    assert(demo::focus_ring_visibility_reason(focus_state)
           == "programmatic_input_does_not_show_focus_ring");
    fs::remove_all(focus_state.root, ec);

    auto const outside_name = std::string{"phenotype-file-explorer-outside-"}
        + profile
        + "-"
        + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count())
        + ".txt";
    auto const outside_relative = std::string{"../"} + outside_name;
    auto const outside_windows_relative = std::string{"..\\"} + outside_name;
    auto outside = state.root.parent_path() / outside_name;
    fs::remove(outside, ec);
    {
        std::ofstream out(outside, std::ios::binary);
        out << "This file must stay outside the demo sandbox.\n";
    }
    assert(fs::exists(outside));
    demo::select_entry(state, outside_relative);
    assert(state.selected_name.empty());
    assert(state.last_operation.kind == "file_read");
    assert(!state.last_operation.ok);
    assert(fs::exists(outside));
    demo::select_entry(state, outside_windows_relative);
    assert(state.selected_name.empty());
    assert(!state.last_operation.ok);
    assert(fs::exists(outside));
    demo::select_entry(state, ".Trash");
    assert(state.selected_name.empty());
    assert(!state.last_operation.ok);
    assert(fs::exists(outside));
    state.selected_name = outside_relative;
    demo::delete_selected(state);
    assert(state.selected_name.empty());
    assert(state.last_operation.kind == "file_delete");
    assert(!state.last_operation.ok);
    assert(fs::exists(outside));
    state.selected_name = outside_relative;
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
    assert(debug_text.find("\"entry_symbol_summary\"") != std::string::npos);
    assert(debug_text.find("\"preferences\"") != std::string::npos);
    assert(debug_text.find("\"system_settings\"") != std::string::npos);
    assert(debug_text.find("\"font_family_source\"") != std::string::npos);
    assert(debug_text.find("\"font_weight_adjustment\"") != std::string::npos);
    assert(debug_text.find("\"scroll_vertical_factor\"") != std::string::npos);
    assert(debug_text.find("\"scroll_horizontal_delta_multiplier\"")
           != std::string::npos);
    assert(debug_text.find("\"double_click_interval_ms\"") != std::string::npos);
    assert(debug_text.find("\"key_repeat_delay_ms\"") != std::string::npos);
    assert(debug_text.find("\"caret_blink_interval_ms\"") != std::string::npos);
    assert(debug_text.find("\"input_timing_source\"") != std::string::npos);
    assert(debug_text.find("\"preferred_locale\"") != std::string::npos);
    assert(debug_text.find("\"preferred_locale_source\"") != std::string::npos);
    assert(debug_text.find("\"color_scheme\"") != std::string::npos);
    assert(debug_text.find("\"accessibility_source\"") != std::string::npos);
    assert(debug_text.find("\"Pretendard package default")
           != std::string::npos);
    assert(debug_text.find("\"source\":\"file_explorer_shared::entry_symbol\"")
           != std::string::npos);
    assert(debug_text.find("\"presentation_role\":\"file_type\"")
           != std::string::npos);
    assert(debug_text.find("\"all_visible_entries_have_symbols\":true")
           != std::string::npos);
    assert(debug_text.find("\"text_document\"") != std::string::npos);
    assert(debug_text.find("\"icon_grid_column_pitch\"") != std::string::npos);
    assert(debug_text.find("\"icon_grid_thumbnail_width\"") != std::string::npos);
    assert(debug_text.find("\"thumbnail_system\"") != std::string::npos);
    assert(debug_text.find("\"finder_rich_preview_thumbnails_v1\"")
           != std::string::npos);
    assert(debug_text.find("\"uses_external_previews\":false")
           != std::string::npos);
    assert(debug_text.find(
        "\"svg_policy\":\"rounded_svg_vector_preview_with_source_safe_svg_badge\"")
        != std::string::npos);
    assert(debug_text.find(
        "\"svg_render_policy\":\"phenotype_svg_subset_renders_file_body_inside_finder_preview\"")
        != std::string::npos);
    assert(debug_text.find(
        "\"svg_external_resource_policy\":\"no_external_svg_resources_or_network_fetches\"")
        != std::string::npos);
    assert(debug_text.find("\"platform_extracted\":false")
        != std::string::npos);
    assert(debug_text.find("\"runtime_fetch_required\":false")
        != std::string::npos);
    assert(debug_text.find("\"platform_extracted_symbol_count\":0")
        != std::string::npos);
    assert(debug_text.find("\"runtime_fetched_symbol_count\":0")
        != std::string::npos);
    assert(debug_text.find("\"source_acquisition_policy\"")
        != std::string::npos);
    assert(debug_text.find("\"document_cache_policy\"")
        != std::string::npos);
    assert(debug_text.find("\"icon_grid_label_font_size\"") != std::string::npos);
    assert(debug_text.find(
        "\"icon_grid_label_policy\":\"finder_two_line_middle_ellipsis_preserve_suffix\"")
        != std::string::npos);
    assert(debug_text.find("\"icon_grid_top_inset\":8") != std::string::npos);
    assert(debug_text.find(
        "\"finder_density_policy\":\"finder_reference_density_icon_grid_top_inset_v1\"")
        != std::string::npos);
    assert(debug_text.find("\"finder_visual_contract\"") != std::string::npos);
    assert(debug_text.find("\"name\":\"finder_visual_parity_contract\"")
           != std::string::npos);
    assert(debug_text.find(
        "\"native_control_marker_policy\":\"no_content_or_artifact_window_control_markers\"")
        != std::string::npos);
    assert(debug_text.find(
        "\"focus_ring_policy\":\"keyboard_tab_navigation_only_pointer_click_hides_focus_ring\"")
        != std::string::npos);
    assert(debug_text.find("\"apple_asset_symbol_count\":0")
           != std::string::npos);
    assert(debug_text.find("\"sidebar_icon_size\"") != std::string::npos);
    assert(debug_text.find("\"sidebar_label_leading\"") != std::string::npos);
    assert(debug_text.find("\"icon_system\"") != std::string::npos);
    assert(debug_text.find("\"phenotype.icons\"") != std::string::npos);
    assert(debug_text.find("\"macos_rounded_outline_svg\"") != std::string::npos);
    assert(debug_text.find("\"svg_subset_policy\":\"bounded_svg_icon_subset\"")
           != std::string::npos);
    assert(debug_text.find("\"svg_supported_elements\":\"svg, g, path, rect, circle")
           != std::string::npos);
    assert(debug_text.find("\"svg_supported_path_commands\":\"M L H V Q T C S A Z")
           != std::string::npos);
    assert(debug_text.find("\"svg_arc_policy\":\"circle elements and isolated circular path A/a")
           != std::string::npos);
    assert(debug_text.find("\"svg_path_arc_symbol_count\":16")
           != std::string::npos);
    assert(debug_text.find("\"lucide_source_symbol_count\":35")
           != std::string::npos);
    assert(debug_text.find("\"lucide_unique_source_icon_count\":34")
           != std::string::npos);
    assert(debug_text.find("\"source_attribution_policy\"")
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
    assert(debug_text.find("\"symbol_interaction_phase_policy\":\"macos_finder_normal_hover_pressed_symbol_chrome\"")
           != std::string::npos);
    assert(debug_text.find("\"toolbar_button_hover_background_alpha\":120")
           != std::string::npos);
    assert(debug_text.find("\"toolbar_selected_button_background_alpha\":150")
           != std::string::npos);
    assert(debug_text.find("\"toolbar_pressed_button_background_alpha\":150")
           != std::string::npos);
    assert(debug_text.find("\"pressed_scale\":0.985")
           != std::string::npos);
    assert(debug_text.find("\"sidebar_selected\":\"accent\"")
           != std::string::npos);
    assert(debug_text.find("\"toolbar_unselected\":\"secondary\"")
           != std::string::npos);
    assert(debug_text.find("\"file_type_color_policy\":\"macos_finder_file_type_tints\"")
           != std::string::npos);
    assert(debug_text.find("\"file_type_symbol_count\":11")
           != std::string::npos);
    assert(debug_text.find("\"file_type_symbol_tokens\"")
           != std::string::npos);
    assert(debug_text.find(
               "\"package_asset_policy\":"
               "\"package_runtime_visible_file_type_svg_asset\"")
        != std::string::npos);
    assert(debug_text.find("\"package_asset_name\":\"file_type.pdf.icon\"")
           != std::string::npos);
    assert(debug_text.find(
               "\"package_asset_source\":"
               "\"assets/icons/file-types/pdf.svg\"")
        != std::string::npos);
    assert(debug_text.find(
               "\"symbol_package_asset_name\":\"file_type.pdf.icon\"")
        != std::string::npos);
    assert(debug_text.find("\"file_type_reference_symbols\"")
           != std::string::npos);
    assert(debug_text.find("\"archivebox\"")
           != std::string::npos);
    assert(debug_text.find("\"music.note\"")
           != std::string::npos);
    assert(debug_text.find("\"spreadsheet_document\"")
           != std::string::npos);
    assert(debug_text.find("\"toolbar_symbol_presentations\"")
           != std::string::npos);
    assert(debug_text.find("\"sidebar_symbol_presentations\"")
           != std::string::npos);
    assert(debug_text.find("\"file_type_symbol_presentations\"")
           != std::string::npos);
    assert(debug_text.find("\"presentation_samples\"")
           != std::string::npos);
    assert(debug_text.find("\"toolbar_pressed_search\"")
           != std::string::npos);
    assert(debug_text.find("\"toolbar_disabled_forward\"")
           != std::string::npos);
    assert(debug_text.find("\"sidebar_selected_recents\"")
           != std::string::npos);
    assert(debug_text.find("\"selected_presentation\"")
           != std::string::npos);
    assert(debug_text.find("\"unselected_presentation\"")
           != std::string::npos);
    assert(debug_text.find("\"symbol_presentation\"")
           != std::string::npos);
    assert(debug_text.find("\"visible_symbol_color\"")
           != std::string::npos);
    assert(debug_text.find("\"likely_pass\":\"svg_path_or_line_paint\"")
           != std::string::npos);
    assert(debug_text.find("\"effective_point_size\":23.64")
           != std::string::npos);
    assert(debug_text.find("\"symbol\":\"pdf_document\"")
           != std::string::npos);
    assert(debug_text.find("\"semantic_reference_name\":\"doc.richtext\"")
           != std::string::npos);
    assert(debug_text.find("\"symbol_semantic_reference_name\":\"doc.plaintext\"")
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
    assert(debug_text.find("\"native_window_control_owner\":\"platform-edge\"")
           != std::string::npos);
    assert(debug_text.find("\"window_control_single_owner\":true")
           != std::string::npos);
    assert(debug_text.find(
        "\"window_control_duplication_guard\":\"native_window_controls_single_owner\"")
           != std::string::npos);
    assert(debug_text.find("\"content_window_control_marker_count\":0")
           != std::string::npos);
    assert(debug_text.find("\"content_drawn_window_control_count\":0")
           != std::string::npos);
    assert(debug_text.find("\"content_window_control_markers\":false")
           != std::string::npos);
    assert(debug_text.find(
        "\"native_window_control_integration_policy\":\"platform_standard_controls_inside_leading_content_reserve\"")
           != std::string::npos);
    assert(debug_text.find("\"titlebar_control_start_x\"") != std::string::npos);
    assert(debug_text.find("\"profile\"") != std::string::npos);
    assert(debug_text.find(profile) != std::string::npos);
    assert(debug_text.find("\"resource_system\"") != std::string::npos);
    assert(debug_text.find("\"input_model\"") != std::string::npos);
    assert(debug_text.find(
        "\"focus_visibility_policy\":\"keyboard_tab_navigation_shows_ring_pointer_click_hides_ring\"")
        != std::string::npos);
    assert(debug_text.find(
        "\"focus_ring_style\":\"macos_blue_keyboard_focus_ring_outset_4px_2px_stroke\"")
        != std::string::npos);
    assert(debug_text.find("\"focus_order\"") != std::string::npos);
    assert(debug_text.find("\"svg_asset_count\":12") != std::string::npos);
    assert(debug_text.find("\"preload_svg_asset_count\":12")
        != std::string::npos);
    assert(debug_text.find("\"runtime_visible_svg_asset_count\":11")
        != std::string::npos);
    assert(debug_text.find("\"file_type_icon_asset_count\":11")
        != std::string::npos);
    assert(debug_text.find("\"file_type_icon_source_family\":\"Lucide\"")
        != std::string::npos);
    assert(debug_text.find(
               "\"file_type_icon_source_revision\":"
               "\"5b40f2c5a76a27eeb81c8f1b1c311121dee45495\"")
        != std::string::npos);
    assert(debug_text.find(
               "\"file_type_icon_license_asset\":"
               "\"assets/icons/file-types/LUCIDE_LICENSE.txt\"")
        != std::string::npos);
    assert(debug_text.find("\"file_type.pdf.icon\"")
        != std::string::npos);
    assert(debug_text.find("\"file_type_icon_source_map\"")
        != std::string::npos);
    assert(debug_text.find("\"package_declared\":true")
        != std::string::npos);
    assert(debug_text.find(
               "\"source_url\":"
               "\"https://raw.githubusercontent.com/lucide-icons/lucide/"
               "5b40f2c5a76a27eeb81c8f1b1c311121dee45495/"
               "icons/file-text.svg\"")
        != std::string::npos);
    assert(debug_text.find("\"runtime_fetch_required\":false")
        != std::string::npos);
    assert(debug_text.find("\"platform_extracted\":false")
        != std::string::npos);
    assert(debug_text.find("\"app_icon\"") != std::string::npos);
    assert(debug_text.find("\"declared\":true") != std::string::npos);
    assert(debug_text.find("\"svg\":true") != std::string::npos);
    assert(debug_text.find("\"preload\":true") != std::string::npos);
    assert(debug_text.find("\"font_family\":\"Pretendard\"")
           != std::string::npos);
    assert(debug_text.find("\"default_font_has_cjk_fallback\":true")
           != std::string::npos);
    assert(debug_text.find(
        "\"svg_asset_policy\":\"package_svg_assets_must_declare_image_svg_xml_and_svg_source_suffix\"")
           != std::string::npos);
    assert(debug_text.find("\"verifier_declared\":true")
           != std::string::npos);
    assert(debug_text.find("\"keyboard_commands\"") != std::string::npos);
    assert(debug_text.find("CommandOrControl+F") != std::string::npos);
    assert(debug_text.find("ArrowDown") != std::string::npos);
    assert(debug_text.find("\"index\"") != std::string::npos);

    state.draft_name = "../ Launch Plan";
    state.draft_body = "Created from test_file_explorer_model.";
    demo::create_file(state);
    assert(state.selected_name == "Launch Plan.txt");
    assert(fs::exists(state.current / "Launch Plan.txt"));
    assert(state.last_operation.plan.writes_file);
    assert(state.last_operation.plan.mutates_filesystem);
    assert(state.last_operation.plan.destination
           == "Demo Root/Launch Plan.txt");
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_create ok - Launch Plan.txt") != std::string::npos);
    assert(demo::read_preview(state.current / "Launch Plan.txt")
        .find("Created from test_file_explorer_model") != std::string::npos);

    demo::delete_selected(state);
    assert(!fs::exists(state.current / "Launch Plan.txt"));
    assert(fs::exists(demo::trash_path(state.root) / "Launch Plan.txt"));
    assert(state.selected_name.empty());
    assert(state.last_operation.plan.moves_to_trash);
    assert(!state.last_operation.plan.permanent_delete);
    assert(state.last_operation.plan.destination == "Trash/Launch Plan.txt");
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_delete ok - Launch Plan.txt") != std::string::npos);

    state.draft_folder_name = "../ Review Folder";
    demo::create_folder(state);
    assert(state.selected_name == "Review Folder");
    assert(fs::is_directory(state.current / "Review Folder"));
    assert(state.last_operation.plan.creates_directory);
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
    assert(state.last_operation.plan.kind == "folder_select");
    assert(state.last_operation.plan.source == "Demo Root/Review Folder");
    assert(state.last_operation.plan.reads_directory);
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
    assert(state.last_operation.plan.deletes_directory);
    assert(state.last_operation.plan.moves_to_trash);
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
    assert(state.last_operation.plan.permanent_delete);
    assert(!state.last_operation.plan.moves_to_trash);

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
    assert(state.last_operation.plan.reads_file);
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_read ok - README.txt") != std::string::npos);
    demo::duplicate_selected(state);
    assert(state.selected_name == "README copy.txt");
    assert(fs::exists(state.current / "README copy.txt"));
    assert(state.status == "Duplicated README.txt to README copy.txt");
    assert(state.last_operation.plan.reads_file);
    assert(state.last_operation.plan.writes_file);
    assert(state.last_operation.plan.destination == "Demo Root/README copy.txt");
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

    demo::apply_startup_scenario(state, "preferences-panel");
    assert(state.mobile_tab == 2);
    assert(state.status == "Display preferences ready.");

    demo::apply_startup_scenario(state, "duplicated-file");
    assert(state.selected_name == "README copy.txt");
    assert(fs::exists(state.current / "README copy.txt"));
    assert(state.status == "Duplicated README.txt to README copy.txt");

    std::string const drive_profile = unique_test_profile("test-cli-drive");
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
    assert(driven.trace[4].operation.plan.writes_file);
    assert(driven.trace[4].operation.plan.destination == "Demo Root/CLI Note.txt");
    assert(driven.trace[5].operation.kind == "file_duplicate");
    assert(driven.trace[5].operation.ok);
    assert(driven.trace[5].operation.plan.source == "Demo Root/CLI Note.txt");
    assert(driven.trace[5].operation.plan.destination
           == "Demo Root/CLI Note copy.txt");
    assert(driven.trace[6].operation.kind == "file_delete");
    assert(driven.trace[6].operation.ok);
    assert(driven.trace[6].operation.plan.moves_to_trash);
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

    std::string const navigation_profile =
        unique_test_profile("test-cli-navigation");
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

    fs::remove_all(navigation_root, ec);
    std::vector<demo::ExplorerInput> focus_inputs{
        parsed_click.input,
        parsed_tab.input,
        parsed_pointer_content.input,
    };
    auto focus_driven = demo::drive_explorer(
        navigation_profile,
        focus_inputs);
    assert(focus_driven.trace.size() == 3);
    assert(focus_driven.trace[0].focus_target
           == demo::ExplorerFocusTarget::ContentGrid);
    assert(!focus_driven.trace[0].focus_visible);
    assert(focus_driven.trace[0].last_input_modality
           == demo::ExplorerInputModality::Pointer);
    assert(focus_driven.trace[1].focus_target
           == demo::ExplorerFocusTarget::PreviewPanel);
    assert(focus_driven.trace[1].focus_visible);
    assert(focus_driven.trace[2].focus_target
           == demo::ExplorerFocusTarget::ContentGrid);
    assert(!focus_driven.trace[2].focus_visible);
    std::vector<demo::ExplorerExpectation> focus_expectations{
        {.kind = demo::ExplorerExpectationKind::FocusTarget,
         .value = "content_grid"},
        {.kind = demo::ExplorerExpectationKind::FocusVisible,
         .value = "false"},
        {.kind = demo::ExplorerExpectationKind::InputModality,
         .value = "pointer"},
    };
    auto checked_focus =
        demo::check_explorer_expectations(focus_driven, focus_expectations);
    assert(demo::explorer_expectations_ok(checked_focus));
    auto failed_focus_visible = demo::check_explorer_expectation(
        focus_driven,
        {.kind = demo::ExplorerExpectationKind::FocusVisible,
         .value = "true"});
    assert(!failed_focus_visible.ok);
    assert(failed_focus_visible.actual == "false");
    fs::remove_all(navigation_root, ec);

    demo::reset_demo_tree(state, profile);
    assert(!demo::snapshot(state).can_go_back);
    assert(!demo::snapshot(state).can_go_forward);
    assert(demo::snapshot(state).operation_label.empty());

    fs::remove_all(root, ec);
    std::puts("PASS: file explorer model contract");
#endif
}

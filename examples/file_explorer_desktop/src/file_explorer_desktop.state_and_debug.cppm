module;
#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module file_explorer_desktop:state_and_debug;

import file_explorer_shared;
import phenotype;
import phenotype.native;
import :messages_and_startup;
import :input_messages;
import :runtime_preferences;

export namespace file_explorer_desktop {
struct State {
    phenotype::icons::SymbolDocumentCache icon_cache =
        phenotype::icons::make_symbol_document_cache();
    file_explorer_demo::ExplorerState explorer;
    file_explorer_demo::ExplorerLabels labels;
    bool search_visible = false;
    bool more_actions_open = false;
    bool settings_open = false;
    bool sidebar_right = false;
    bool favorites_expanded = true;
    bool locations_expanded = true;

    State()
        : explorer(initial_explorer_state()),
          labels(file_explorer_demo::file_explorer_labels(
              resolved_initial_locale(),
              runtime_resource_catalog())),
          search_visible(!explorer.search.empty()),
          more_actions_open(initial_more_actions_open()) {
        file_explorer_demo::apply_runtime_preferences(
            explorer,
            g_runtime_preferences);
        apply_startup_inputs(explorer, "desktop");
        search_visible = !explorer.search.empty();
        sync_runtime_theme(explorer);
        if (initial_settings_open())
            phenotype::detail::post<Msg>(OpenSettings{});
    }
};

State const* g_debug_state = nullptr;
constexpr int k_more_actions_button_count = 29;

auto file_explorer_application_debug_payload() {
    if (!g_debug_state) {
        file_explorer_demo::ExplorerState empty{};
        auto snap = file_explorer_demo::snapshot(empty);
        auto chrome = file_explorer_demo::explorer_chrome_metrics(empty, "desktop");
        return file_explorer_demo::file_explorer_application_debug_json(
            empty,
            snap,
            chrome,
            "desktop");
    }
    auto snap = file_explorer_demo::snapshot(g_debug_state->explorer);
    auto chrome = file_explorer_demo::explorer_chrome_metrics(
        g_debug_state->explorer,
        "desktop");
    chrome =
        file_explorer_demo::explorer_chrome_with_native_window_control_ownership(
            std::move(chrome));
    chrome.more_actions_open = g_debug_state->more_actions_open;
    chrome.overflow_action_button_count = g_debug_state->more_actions_open
        ? k_more_actions_button_count
        : 0;
    return file_explorer_demo::file_explorer_application_debug_json(
        g_debug_state->explorer,
        snap,
        chrome,
        "desktop");
}

constexpr float k_pi = 3.14159265358979323846f;
constexpr float k_integrated_titlebar_height =
    file_explorer_demo::k_desktop_integrated_titlebar_height;
constexpr float k_sidebar_width = file_explorer_demo::k_desktop_sidebar_width;
constexpr float k_sidebar_row_width = file_explorer_demo::k_desktop_sidebar_row_width;
constexpr float k_sidebar_row_height =
    file_explorer_demo::k_desktop_sidebar_row_height;
constexpr float k_sidebar_heading_height =
    file_explorer_demo::k_desktop_sidebar_heading_height;
constexpr float k_sidebar_icon_size =
    file_explorer_demo::k_desktop_sidebar_icon_size;
constexpr float k_sidebar_icon_leading =
    file_explorer_demo::k_desktop_sidebar_icon_leading;
constexpr float k_sidebar_label_leading =
    file_explorer_demo::k_desktop_sidebar_label_leading;
constexpr float k_sidebar_label_top =
    file_explorer_demo::k_desktop_sidebar_label_top;
constexpr float k_sidebar_heading_label_leading =
    file_explorer_demo::k_desktop_sidebar_heading_label_leading;
constexpr float k_sidebar_heading_label_top =
    file_explorer_demo::k_desktop_sidebar_heading_label_top;
constexpr float k_sidebar_section_gap =
    file_explorer_demo::k_desktop_sidebar_section_gap;
constexpr float k_sidebar_selected_row_radius =
    file_explorer_demo::k_desktop_sidebar_selected_row_radius;
constexpr float k_column_location_row_height =
    file_explorer_demo::k_desktop_column_location_row_height;
constexpr float k_column_location_icon_size =
    file_explorer_demo::k_desktop_column_location_icon_size;
constexpr float k_window_radius = file_explorer_demo::k_desktop_window_radius;
constexpr float k_toolbar_group_radius =
    file_explorer_demo::k_desktop_toolbar_group_radius;
constexpr float k_toolbar_group_height =
    file_explorer_demo::k_desktop_toolbar_group_height;
constexpr float k_toolbar_icon_button_width =
    file_explorer_demo::k_desktop_toolbar_icon_button_width;
constexpr float k_toolbar_icon_button_height =
    file_explorer_demo::k_desktop_toolbar_icon_button_height;
constexpr float k_file_thumbnail_symbol_stroke_scale = 0.68f;
constexpr float k_native_window_control_reserve_height =
    file_explorer_demo::k_desktop_titlebar_control_cluster_height;
constexpr float k_titlebar_drag_region_height =
    file_explorer_demo::k_desktop_titlebar_drag_region_height;
constexpr float k_leading_control_reserved_width =
    file_explorer_demo::k_desktop_leading_control_reserved_width;
constexpr float k_trailing_control_reserved_width =
    file_explorer_demo::k_desktop_trailing_control_reserved_width;

} // namespace file_explorer_desktop

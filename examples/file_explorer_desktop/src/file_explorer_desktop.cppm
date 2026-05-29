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

export module file_explorer_desktop;

import file_explorer_shared;
import phenotype;
import phenotype.native;


export namespace file_explorer_desktop {
int run(int argc, char** argv);
}

namespace file_explorer_desktop {
#include "file_explorer_desktop/messages_and_startup.inc"
#include "file_explorer_desktop/input_messages.inc"
#include "file_explorer_desktop/runtime_preferences.inc"
#include "file_explorer_desktop/state_and_debug.inc"
#include "file_explorer_desktop/style.inc"
#include "file_explorer_desktop/painting_icons.inc"
#include "file_explorer_desktop/text_and_status.inc"
#include "file_explorer_desktop/update.inc"
#include "file_explorer_desktop/buttons_and_sidebar.inc"
#include "file_explorer_desktop/toolbar.inc"
#include "file_explorer_desktop/overflow_and_grid.inc"
#include "file_explorer_desktop/settings_and_views.inc"
#include "file_explorer_desktop/keyboard.inc"
#include "file_explorer_desktop/view.inc"
int run(int argc, char** argv) {
    configure_initial_filesystem_root(argc, argv);
    phenotype::Theme theme = phenotype::current_theme();
    theme = phenotype::theme_with_resource_defaults(
        theme,
        runtime_resource_catalog());
    theme.background = rgba(246, 246, 246, 0);
    theme.foreground = rgba(29, 29, 31);
    theme.accent = rgba(0, 122, 255);
    theme.accent_strong = rgba(0, 99, 204);
    theme.muted = rgba(112, 112, 117);
    theme.border = rgba(226, 226, 230);
    theme.surface = rgba(255, 255, 255);
    theme.code_bg = rgba(242, 242, 247);
    theme.body_font_size = 14.0f;
    theme.heading_font_size = 18.0f;
    theme.small_font_size = 12.0f;
    theme.radius_sm = 10.0f;
    theme.radius_md = 14.0f;
    theme.radius_lg = 22.0f;
    auto const system_settings = capture_system_settings();
    auto const theme_preferences = initial_theme_preference_overrides();
    g_base_theme = theme;
    g_system_settings = system_settings;
    auto const resolved = phenotype::resolve_system_theme_preferences(
        theme,
        system_settings,
        theme_preferences,
        "native-system-settings+environment-overrides");
    g_runtime_preferences = runtime_preference_state(
        resolved,
        system_settings,
        theme_preferences);
    phenotype::set_theme(resolved.theme);
    phenotype::diag::set_application_debug_provider(
        file_explorer_application_debug_payload);

    phenotype::native::WindowOptions window_options{
        .chrome = phenotype::native::WindowChromeStyle::IntegratedTitlebar,
        .integrated_titlebar = {
            .height = k_integrated_titlebar_height,
            .drag_region_height = k_titlebar_drag_region_height,
            .leading_control_reserved_width = k_leading_control_reserved_width,
            .trailing_control_reserved_width = k_trailing_control_reserved_width,
        },
        .native_backdrop_material =
            phenotype::native::NativeBackdropMaterial::Sidebar,
        .native_backdrop_opacity = 1.0f,
        .keep_running_after_last_window_closed = true,
        .install_standard_app_menu = true,
        .application_name = "phenotype file explorer desktop",
        .settings_menu_title = "Settings...",
        .settings_menu_key_equivalent = ",",
        .on_settings_menu = open_settings_from_app_menu,
    };

    return phenotype::native::run_app<State, Msg>(
        1300,
        760,
        "phenotype file explorer desktop",
        window_options,
        view,
        update,
        [](int width, int height, float scale) -> Msg {
            return Resized{width, height, scale};
        });
}

} // namespace file_explorer_desktop

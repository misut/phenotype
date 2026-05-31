module;
#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdio>
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

import :messages_and_startup;
import :input_messages;
import :runtime_preferences;
import :state_and_debug;
import :style;
import :painting_icons;
import :text_and_status;
import :update;
import :buttons_and_sidebar;
import :toolbar;
import :overflow_and_grid;
import :settings_and_views;
import :keyboard;
import :view;

export namespace file_explorer_desktop {
int run(int argc, char** argv);
}

namespace file_explorer_desktop {
#if defined(__APPLE__)
constexpr char const* k_settings_window_identifier =
    k_settings_scene_id;

char const* settings_scene_artifact_dir() {
    if (char const* dir = std::getenv(
            "PHENOTYPE_FILE_EXPLORER_SETTINGS_ARTIFACT_DIR")) {
        if (*dir)
            return dir;
    }
    if (char const* dir = std::getenv("PHENOTYPE_SETTINGS_ARTIFACT_DIR")) {
        if (*dir)
            return dir;
    }
    return nullptr;
}

void write_settings_scene_artifact_if_requested() {
    char const* dir = settings_scene_artifact_dir();
    if (!dir)
        return;
    auto result = phenotype::native::scene_window::write_artifact_bundle(
        k_settings_window_identifier,
        dir,
        "file-explorer-settings-scene");
    if (!result.ok && !result.error.empty()) {
        std::fprintf(
            stderr,
            "[file-explorer] settings artifact failed: %s\n",
            result.error.c_str());
    }
}

void settings_scene_closed(void*) {
    auto main_scene = phenotype::runtime::main_scene();
    phenotype::runtime::post_to_scene<Msg>(main_scene, CloseSettings{});
    phenotype::runtime::trigger_scene_rebuild(main_scene);
}

void sync_settings_scene_theme(State& state) {
    auto settings_scene = phenotype::runtime::ensure_scene(
        phenotype::SceneDescriptor{
            .id = k_settings_scene_id,
            .title = "File Explorer Settings",
            .role = phenotype::SceneRole::Settings,
            .visible = state.settings_open,
        });
    phenotype::runtime::SceneActivation activate{settings_scene};
    sync_runtime_theme(state.explorer);
    auto theme = phenotype::current_theme();
    theme.background = with_alpha(theme.surface, 255);
    phenotype::set_theme(theme);
}

void settings_scene_update(State& state, Msg msg) {
    update(state, std::move(msg));
    sync_settings_scene_theme(state);
    if (!state.settings_open) {
        phenotype::native::scene_window::close_window(
            k_settings_window_identifier);
    }
    phenotype::runtime::trigger_scene_rebuild(phenotype::runtime::main_scene());
}

bool show_settings_scene_window(State& state, bool order_front) {
    sync_settings_scene_theme(state);
    phenotype::native::NativeSceneWindowOptions options{
        .identifier = k_settings_window_identifier,
        .title = "File Explorer Settings",
        .width = 500,
        .height = 360,
        .scene_id = k_settings_scene_id,
        .surface_id = k_settings_render_surface_id,
        .scene_role = phenotype::SceneRole::Settings,
        .surface_role = phenotype::RenderSurfaceRole::Settings,
        .window_options = {
            .chrome = phenotype::native::WindowChromeStyle::System,
            .native_backdrop_material =
                phenotype::native::NativeBackdropMaterial::UnderWindowBackground,
            .native_backdrop_opacity = 1.0f,
        },
        .order_front = order_front,
        .on_close = settings_scene_closed,
    };
    bool const shown = phenotype::native::scene_window::show_with_state<State, Msg>(
        options,
        state,
        finder_settings_scene,
        settings_scene_update);
    if (shown)
        write_settings_scene_artifact_if_requested();
    return shown;
}

void sync_settings_scene_window(State& state, bool order_front) {
    if (state.settings_open) {
        (void)show_settings_scene_window(state, order_front);
    } else {
        phenotype::native::scene_window::close_window(
            k_settings_window_identifier);
    }
}
#endif

void desktop_update(State& state, Msg msg) {
    bool const order_settings_front = std::holds_alternative<OpenSettings>(msg);
    update(state, std::move(msg));
#if defined(__APPLE__)
    sync_settings_scene_window(state, order_settings_front);
#endif
}

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
        desktop_update,
        [](int width, int height, float scale) -> Msg {
            return Resized{width, height, scale};
        });
}
} // namespace file_explorer_desktop

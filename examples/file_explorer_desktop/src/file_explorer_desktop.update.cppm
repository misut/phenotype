module;
#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module file_explorer_desktop:update;

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

export namespace file_explorer_desktop {
std::vector<file_explorer_demo::Entry> finder_entries(
        file_explorer_demo::Snapshot const& snap) {
    auto entries = snap.entries;
    if (snap.sort_mode == file_explorer_demo::SortMode::Recent)
        return entries;
    std::stable_sort(entries.begin(), entries.end(),
                     [](auto const& a, auto const& b) {
        if (a.folder != b.folder)
            return !a.folder && b.folder;
        return file_explorer_demo::lower_copy(a.name)
            < file_explorer_demo::lower_copy(b.name);
    });
    return entries;
}

constexpr char const* k_settings_scene_id = "file-explorer-desktop-settings";
constexpr char const* k_settings_render_surface_id =
    "file-explorer-desktop-settings-window";

void sync_settings_runtime_scene(State const& state) {
    phenotype::runtime::ensure_scene(phenotype::SceneDescriptor{
        .id = k_settings_scene_id,
        .title = "File Explorer Settings",
        .role = phenotype::SceneRole::Settings,
        .visible = state.settings_open,
    });
    phenotype::runtime::ensure_render_surface(phenotype::RenderSurfaceDescriptor{
        .id = k_settings_render_surface_id,
        .title = "File Explorer Settings",
        .scene_id = k_settings_scene_id,
        .role = phenotype::RenderSurfaceRole::Settings,
        .visible = state.settings_open,
        .logical_width = 500,
        .logical_height = 284,
        .framebuffer_width = 500,
        .framebuffer_height = 284,
        .content_scale = 1.0f,
    });
}

#if defined(__APPLE__)
constexpr char const* k_settings_window_identifier =
    k_settings_scene_id;

void native_settings_color_scheme_selected(char const* value, void*) {
    phenotype::runtime::post<Msg>(
        color_scheme_message(value && *value ? value : "system"));
}

void native_settings_sidebar_position_selected(char const* value, void*) {
    phenotype::runtime::post<Msg>(
        SetSidebarPosition{std::string_view{value ? value : ""} == "right"});
}

void native_settings_scrollbar_selected(char const* value, void*) {
    phenotype::runtime::post<Msg>(
        scroll_bar_visibility_message(value && *value ? value : "auto"));
}

void native_settings_window_closed(void*) {
    phenotype::runtime::post<Msg>(CloseSettings{});
}

std::string native_settings_appearance(State const& state) {
    if (state.explorer.theme_preferences.prefer_system_color_scheme)
        return "system";
    return state.explorer.theme_preferences.color_scheme == "dark"
        ? "dark"
        : "light";
}

bool publish_native_settings_window(State const& state, bool order_front) {
    auto const appearance = native_settings_appearance(state);
    phenotype::native::NativePreferencesChoice appearance_choices[] = {
        {
            state.labels.preferences_system_appearance.c_str(),
            "system",
            state.explorer.theme_preferences.prefer_system_color_scheme,
        },
        {
            state.labels.preferences_light_appearance.c_str(),
            "light",
            !state.explorer.theme_preferences.prefer_system_color_scheme
                && state.explorer.theme_preferences.color_scheme == "light",
        },
        {
            state.labels.preferences_dark_appearance.c_str(),
            "dark",
            !state.explorer.theme_preferences.prefer_system_color_scheme
                && state.explorer.theme_preferences.color_scheme == "dark",
        },
    };
    phenotype::native::NativePreferencesChoice sidebar_choices[] = {
        {"Left", "left", !state.sidebar_right},
        {"Right", "right", state.sidebar_right},
    };
    phenotype::native::NativePreferencesChoice scrollbar_choices[] = {
        {
            state.labels.preferences_scrollbar_auto.c_str(),
            "auto",
            state.explorer.theme_preferences.scroll_bar_visibility == "auto",
        },
        {
            state.labels.preferences_scrollbar_always.c_str(),
            "always",
            state.explorer.theme_preferences.scroll_bar_visibility == "always",
        },
        {
            state.labels.preferences_scrollbar_hidden.c_str(),
            "hidden",
            state.explorer.theme_preferences.scroll_bar_visibility == "hidden",
        },
    };
    phenotype::native::NativePreferencesSection sections[] = {
        {
            "Appearance",
            "",
            appearance_choices,
            std::size(appearance_choices),
            native_settings_color_scheme_selected,
        },
        {
            "Sidebar",
            "",
            sidebar_choices,
            std::size(sidebar_choices),
            native_settings_sidebar_position_selected,
        },
        {
            "Scroll Bars",
            "",
            scrollbar_choices,
            std::size(scrollbar_choices),
            native_settings_scrollbar_selected,
        },
    };
    phenotype::native::NativePreferencesWindowOptions const options{
        .identifier = k_settings_window_identifier,
        .title = "File Explorer Settings",
        .width = 500,
        .height = 284,
        .appearance = appearance.c_str(),
        .sections = sections,
        .section_count = std::size(sections),
        .on_close = native_settings_window_closed,
    };
    return order_front
        ? phenotype::native::preferences::show_window(options)
        : phenotype::native::preferences::sync_window(options);
}

bool show_native_settings_window(State const& state) {
    return publish_native_settings_window(state, true);
}

void refresh_native_settings_window_if_visible(State const& state) {
    if (phenotype::native::preferences::is_window_visible(
            k_settings_window_identifier)) {
        (void)publish_native_settings_window(state, false);
    }
}

void close_native_settings_window() {
    phenotype::native::preferences::close_window(k_settings_window_identifier);
}
#endif

void update(State& state, Msg msg) {
    auto& explorer = state.explorer;
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, SelectLocation>) {
            state.more_actions_open = false;
            auto input = pointer_input(
                file_explorer_demo::ExplorerInputKind::SelectLocation);
            input.value = m.id;
            apply_desktop_input(explorer, std::move(input));
        } else if constexpr (std::same_as<T, SelectEntry>) {
            state.more_actions_open = false;
            auto input = pointer_input(
                file_explorer_demo::ExplorerInputKind::ActivateEntry);
            input.value = m.name;
            apply_desktop_input(explorer, std::move(input));
        } else if constexpr (std::same_as<T, ExplorerInputMessage>) {
            state.more_actions_open = false;
            apply_desktop_input(explorer, m.input);
        } else if constexpr (std::same_as<T, SetViewMode>) {
            state.more_actions_open = false;
            auto input = pointer_input(
                file_explorer_demo::ExplorerInputKind::ViewMode);
            input.value = file_explorer_demo::view_mode_value_name(m.mode);
            input.view_mode = m.mode;
            apply_desktop_input(explorer, std::move(input));
        } else if constexpr (std::same_as<T, ToolbarAction>) {
            state.more_actions_open = false;
            apply_desktop_pointer_focus(explorer, "toolbar_actions");
            explorer.status = m.label + " action is available in the native toolbar contract.";
        } else if constexpr (std::same_as<T, SearchChanged>) {
            auto input = keyboard_input(
                file_explorer_demo::ExplorerInputKind::Search);
            input.value = m.text;
            apply_desktop_input(explorer, std::move(input));
            state.search_visible = true;
        } else if constexpr (std::same_as<T, ToggleSearch>) {
            state.more_actions_open = false;
            state.search_visible = !state.search_visible;
            if (!state.search_visible) {
                auto input = pointer_input(
                    file_explorer_demo::ExplorerInputKind::Search);
                apply_desktop_input(explorer, std::move(input));
            } else {
                apply_desktop_input(
                    explorer,
                    pointer_input(file_explorer_demo::ExplorerInputKind::FocusSearch));
            }
        } else if constexpr (std::same_as<T, ShowSearch>) {
            state.more_actions_open = false;
            state.search_visible = true;
            apply_desktop_input(
                explorer,
                keyboard_input(file_explorer_demo::ExplorerInputKind::FocusSearch));
        } else if constexpr (std::same_as<T, DismissTransient>) {
            if (state.more_actions_open) {
                state.more_actions_open = false;
                explorer.status = "More actions closed.";
            } else if (state.settings_open) {
                state.settings_open = false;
#if defined(__APPLE__)
                close_native_settings_window();
#endif
                explorer.status = "Settings closed.";
            } else if (state.search_visible) {
                state.search_visible = false;
                auto input = keyboard_input(
                    file_explorer_demo::ExplorerInputKind::Search);
                apply_desktop_input(explorer, std::move(input));
            } else {
                explorer.status = "Ready";
            }
        } else if constexpr (std::same_as<T, MoveSelection>) {
            state.more_actions_open = false;
            apply_desktop_input(explorer, keyboard_move_input(m.move));
        } else if constexpr (std::same_as<T, CreateFile>) {
            state.more_actions_open = false;
            apply_desktop_input(
                explorer,
                pointer_input(file_explorer_demo::ExplorerInputKind::CreateFile));
        } else if constexpr (std::same_as<T, CreateFolder>) {
            state.more_actions_open = false;
            apply_desktop_input(
                explorer,
                pointer_input(file_explorer_demo::ExplorerInputKind::CreateFolder));
        } else if constexpr (std::same_as<T, DeleteSelected>) {
            state.more_actions_open = false;
            apply_desktop_input(
                explorer,
                pointer_input(file_explorer_demo::ExplorerInputKind::DeleteSelected));
        } else if constexpr (std::same_as<T, DuplicateSelected>) {
            state.more_actions_open = false;
            apply_desktop_input(
                explorer,
                pointer_input(file_explorer_demo::ExplorerInputKind::DuplicateSelected));
        } else if constexpr (std::same_as<T, ActivateSelected>) {
            state.more_actions_open = false;
            apply_desktop_input(
                explorer,
                keyboard_input(file_explorer_demo::ExplorerInputKind::ActivateSelected));
        } else if constexpr (std::same_as<T, ToggleMoreActions>) {
            apply_desktop_pointer_focus(explorer, "more_actions");
            state.more_actions_open = !state.more_actions_open;
            state.settings_open = false;
            explorer.status = state.more_actions_open
                ? "More actions ready."
                : "Ready";
        } else if constexpr (std::same_as<T, OpenSettings>) {
            state.more_actions_open = false;
            state.settings_open = true;
#if defined(__APPLE__)
            if (!show_native_settings_window(state))
                state.settings_open = true;
#endif
            explorer.status = "Settings ready.";
        } else if constexpr (std::same_as<T, CloseSettings>) {
            state.settings_open = false;
#if defined(__APPLE__)
            close_native_settings_window();
#endif
            explorer.status = "Ready";
        } else if constexpr (std::same_as<T, SetSidebarPosition>) {
            state.sidebar_right = m.right;
            explorer.status = state.sidebar_right
                ? "Sidebar moved to the right."
                : "Sidebar moved to the left.";
        } else if constexpr (std::same_as<T, ToggleSidebarSection>) {
            if (m.id == "favorites") {
                state.favorites_expanded = !state.favorites_expanded;
                explorer.status = state.favorites_expanded
                    ? "Favorites expanded."
                    : "Favorites collapsed.";
            } else if (m.id == "locations") {
                state.locations_expanded = !state.locations_expanded;
                explorer.status = state.locations_expanded
                    ? "Locations expanded."
                    : "Locations collapsed.";
            }
        } else if constexpr (std::same_as<T, GoBack>) {
            state.more_actions_open = false;
            apply_desktop_input(
                explorer,
                pointer_input(file_explorer_demo::ExplorerInputKind::GoBack));
        } else if constexpr (std::same_as<T, GoForward>) {
            state.more_actions_open = false;
            apply_desktop_input(
                explorer,
                pointer_input(file_explorer_demo::ExplorerInputKind::GoForward));
        } else if constexpr (std::same_as<T, GoUp>) {
            state.more_actions_open = false;
            apply_desktop_input(
                explorer,
                pointer_input(file_explorer_demo::ExplorerInputKind::GoUp));
        } else if constexpr (std::same_as<T, CycleSort>) {
            state.more_actions_open = false;
            apply_desktop_input(
                explorer,
                pointer_input(file_explorer_demo::ExplorerInputKind::CycleSort));
        } else if constexpr (std::same_as<T, SortBy>) {
            state.more_actions_open = false;
            auto input = pointer_input(file_explorer_demo::ExplorerInputKind::Sort);
            input.sort_mode = m.mode;
            apply_desktop_input(explorer, std::move(input));
        } else if constexpr (std::same_as<T, Refresh>) {
            state.more_actions_open = false;
            apply_desktop_pointer_focus(explorer, "toolbar_actions");
            explorer.status = "Refreshed " + file_explorer_demo::relative_location(
                explorer.root,
                explorer.current);
        } else if constexpr (std::same_as<T, ResetDemo>) {
            state.more_actions_open = false;
            file_explorer_demo::reset_demo_tree(explorer, "desktop");
        } else if constexpr (std::same_as<T, Resized>) {
            explorer.viewport_width = m.width;
            explorer.viewport_height = m.height;
            explorer.viewport_scale = m.scale;
        } else if constexpr (std::same_as<T, Noop>) {
        }
    }, msg);
    sync_runtime_theme(explorer);
    sync_settings_runtime_scene(state);
#if defined(__APPLE__)
    refresh_native_settings_window_if_visible(state);
#endif
}

} // namespace file_explorer_desktop

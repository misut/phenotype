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

export module file_explorer_desktop:view;

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

export namespace file_explorer_desktop {
void view(State const& state) {
    using namespace phenotype;
    g_debug_state = &state;
    auto snap = file_explorer_demo::snapshot(state.explorer);
    register_finder_key_commands();
    layout::material_container(
        layout::MaterialContainerOptions{
            .container_id = 2100u,
            .spacing = 16.0f,
            .morph_transitions = true,
        },
        [&] {
            auto window_options = layout::glass_surface_options(
                layout::GlassSurfacePreset::Window,
                "Finder Window");
            window_options.max_width = 0.0f;
            window_options.fixed_height = -1.0f;
            window_options.border_radius = k_window_radius;
            window_options.kind = MaterialKind::None;
            window_options.has_material_override = true;
            window_options.material_override = MaterialStyle{};
            layout::material_surface(
                window_options,
                [&] {
                    auto main_content = [&] {
                        layout::material_surface(
                            main_content_shell_options(state.explorer),
                            [&] {
                                finder_toolbar(state, snap);
                                layout::spacer(static_cast<unsigned int>(
                                    file_explorer_demo::k_desktop_main_shell_gap));
                                layout::divider();
                                layout::spacer(static_cast<unsigned int>(
                                    file_explorer_demo::k_desktop_main_shell_gap));
                                if (state.more_actions_open)
                                    finder_more_actions(state, snap);
                                layout::weighted(1.0f, [&] {
                                    finder_content(state, snap);
                                });
                                if (file_explorer_demo::desktop_status_bar_visible(
                                        state.explorer)) {
                                    layout::divider();
                                    finder_status_bar(state, snap);
                                }
                            });
                    };
                    if (!state.sidebar_right)
                        finder_sidebar(state);
                    layout::weighted(1.0f, main_content);
                    if (state.sidebar_right)
                        finder_sidebar(state);
                });
        });
    if (state.settings_open)
        finder_settings_window(state);
}
} // namespace file_explorer_desktop

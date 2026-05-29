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

export module file_explorer_mobile;

import file_explorer_shared;
import phenotype;
import phenotype.native;

import :messages_and_runtime;
import :state_and_debug;
import :input_messages;
import :painting;
import :update;
import :views;

export namespace file_explorer_mobile {
int run();
}

namespace file_explorer_mobile {
int run() {
    phenotype::Theme theme = phenotype::current_theme();
    theme = phenotype::theme_with_resource_defaults(
        theme,
        runtime_resource_catalog());
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

    return phenotype::native::run_app<State, Msg>(
        390,
        780,
        "phenotype file explorer mobile",
        view,
        update,
        [](int width, int height, float scale) -> Msg {
            return Resized{width, height, scale};
        });
}
} // namespace file_explorer_mobile

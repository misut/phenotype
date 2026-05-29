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

export module file_explorer_mobile:input_messages;

import file_explorer_shared;
import phenotype;
import phenotype.native;
import :messages_and_runtime;
import :state_and_debug;

export namespace file_explorer_mobile {
Msg on_search_changed(std::string text) {
    return SearchChanged{std::move(text)};
}

Msg on_draft_name_changed(std::string text) {
    return DraftNameChanged{std::move(text)};
}

Msg on_draft_folder_name_changed(std::string text) {
    return DraftFolderNameChanged{std::move(text)};
}

Msg on_draft_body_changed(std::string text) {
    return DraftBodyChanged{std::move(text)};
}

file_explorer_demo::ExplorerInput preference_input(
        file_explorer_demo::ExplorerInputKind kind,
        std::string value) {
    return file_explorer_demo::ExplorerInput{
        .kind = kind,
        .value = std::move(value),
        .modality = file_explorer_demo::ExplorerInputModality::Pointer,
    };
}

ExplorerInputMessage preference_message(
        file_explorer_demo::ExplorerInputKind kind,
        std::string value) {
    return ExplorerInputMessage{preference_input(kind, std::move(value))};
}

ExplorerInputMessage font_family_message(std::string value) {
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetFontFamily,
        std::move(value));
}

ExplorerInputMessage color_scheme_message(std::string value) {
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetColorScheme,
        std::move(value));
}

ExplorerInputMessage system_scroll_metrics_message(std::string value) {
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetSystemScrollMetrics,
        std::move(value));
}

ExplorerInputMessage scroll_bar_visibility_message(std::string value) {
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetScrollBarVisibility,
        std::move(value));
}

ExplorerInputMessage system_font_metrics_message(std::string value) {
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetSystemFontMetrics,
        std::move(value));
}

ExplorerInputMessage font_scale_step_message(
        file_explorer_demo::ExplorerState const& explorer,
        float delta) {
    auto value = std::clamp(
        explorer.theme_preferences.font_scale + delta,
        0.75f,
        1.8f);
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetFontScale,
        std::to_string(value));
}

ExplorerInputMessage body_font_size_step_message(
        file_explorer_demo::ExplorerState const& explorer,
        float delta) {
    auto const base = explorer.theme_preferences.body_font_size > 0.0f
        ? explorer.theme_preferences.body_font_size
        : explorer.effective_body_font_size;
    auto value = std::clamp(base + delta, 8.0f, 40.0f);
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetBodyFontSize,
        std::to_string(value));
}

ExplorerInputMessage heading_font_size_step_message(
        file_explorer_demo::ExplorerState const& explorer,
        float delta) {
    auto const base = explorer.theme_preferences.heading_font_size > 0.0f
        ? explorer.theme_preferences.heading_font_size
        : explorer.effective_heading_font_size;
    auto value = std::clamp(base + delta, 10.0f, 56.0f);
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetHeadingFontSize,
        std::to_string(value));
}

ExplorerInputMessage small_font_size_step_message(
        file_explorer_demo::ExplorerState const& explorer,
        float delta) {
    auto const base = explorer.theme_preferences.small_font_size > 0.0f
        ? explorer.theme_preferences.small_font_size
        : explorer.effective_small_font_size;
    auto value = std::clamp(base + delta, 8.0f, 32.0f);
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetSmallFontSize,
        std::to_string(value));
}

ExplorerInputMessage line_height_step_message(
        file_explorer_demo::ExplorerState const& explorer,
        float delta) {
    auto const base = explorer.theme_preferences.line_height_ratio > 0.0f
        ? explorer.theme_preferences.line_height_ratio
        : explorer.effective_line_height_ratio;
    auto value = std::clamp(base + delta, 1.0f, 2.2f);
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetLineHeightRatio,
        std::to_string(value));
}

ExplorerInputMessage scroll_speed_step_message(
        file_explorer_demo::ExplorerState const& explorer,
        float delta) {
    auto value = std::clamp(
        explorer.theme_preferences.scroll_delta_multiplier + delta,
        0.25f,
        4.0f);
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetScrollSpeed,
        std::to_string(value));
}

ExplorerInputMessage horizontal_scroll_speed_step_message(
        file_explorer_demo::ExplorerState const& explorer,
        float delta) {
    auto value = std::clamp(
        explorer.theme_preferences.scroll_horizontal_delta_multiplier + delta,
        0.25f,
        4.0f);
    return preference_message(
        file_explorer_demo::ExplorerInputKind::SetHorizontalScrollSpeed,
        std::to_string(value));
}

} // namespace file_explorer_mobile

file_explorer_demo::ExplorerInput explorer_input(
        file_explorer_demo::ExplorerInputKind kind,
        file_explorer_demo::ExplorerInputModality modality) {
    return file_explorer_demo::ExplorerInput{
        .kind = kind,
        .modality = modality,
    };
}

file_explorer_demo::ExplorerInput pointer_input(
        file_explorer_demo::ExplorerInputKind kind) {
    return explorer_input(
        kind,
        file_explorer_demo::ExplorerInputModality::Pointer);
}

file_explorer_demo::ExplorerInput preference_input(
        file_explorer_demo::ExplorerInputKind kind,
        std::string value) {
    auto input = pointer_input(kind);
    input.value = std::move(value);
    return input;
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

file_explorer_demo::ExplorerInput keyboard_input(
        file_explorer_demo::ExplorerInputKind kind) {
    return explorer_input(
        kind,
        file_explorer_demo::ExplorerInputModality::Keyboard);
}

file_explorer_demo::ExplorerInput keyboard_move_input(
        file_explorer_demo::ExplorerSelectionMove move) {
    auto input = keyboard_input(
        file_explorer_demo::ExplorerInputKind::MoveSelection);
    input.value = file_explorer_demo::selection_move_value_name(move);
    input.selection_move = move;
    return input;
}

void apply_desktop_input(
        file_explorer_demo::ExplorerState& explorer,
        file_explorer_demo::ExplorerInput input) {
    file_explorer_demo::apply_explorer_input(
        explorer,
        input,
        "desktop");
}

void apply_desktop_pointer_focus(
        file_explorer_demo::ExplorerState& explorer,
        std::string_view target) {
    auto input = pointer_input(
        file_explorer_demo::ExplorerInputKind::PointerFocus);
    input.value = std::string{target};
    apply_desktop_input(explorer, std::move(input));
}


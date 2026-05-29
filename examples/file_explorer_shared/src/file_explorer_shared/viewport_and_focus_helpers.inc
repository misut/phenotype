inline bool mobile_profile(std::string_view profile) {
    return profile == "mobile";
}

inline std::string lower_copy(std::string text);
inline std::string trim(std::string_view text);

inline std::string normalized_scroll_bar_visibility(std::string_view value) {
    auto normalized = lower_copy(trim(value));
    if (normalized == "hidden" || normalized == "none"
        || normalized == "off" || normalized == "false"
        || normalized == "0") {
        return "hidden";
    }
    if (normalized == "always" || normalized == "visible"
        || normalized == "on" || normalized == "true"
        || normalized == "1") {
        return "always";
    }
    return "auto";
}

inline std::string input_modality_value_name(ExplorerInputModality modality) {
    switch (modality) {
        case ExplorerInputModality::Keyboard: return "keyboard";
        case ExplorerInputModality::Pointer: return "pointer";
        case ExplorerInputModality::Programmatic: return "programmatic";
        case ExplorerInputModality::Unspecified:
        default:
            return "unspecified";
    }
}

inline std::string focus_target_value_name(ExplorerFocusTarget target) {
    switch (target) {
        case ExplorerFocusTarget::Sidebar: return "sidebar";
        case ExplorerFocusTarget::ToolbarNavigation:
            return "toolbar_navigation";
        case ExplorerFocusTarget::ToolbarView: return "toolbar_view";
        case ExplorerFocusTarget::ToolbarActions: return "toolbar_actions";
        case ExplorerFocusTarget::Search: return "search";
        case ExplorerFocusTarget::ContentGrid: return "content_grid";
        case ExplorerFocusTarget::PreviewPanel: return "preview_panel";
        case ExplorerFocusTarget::CreatePanel: return "create_panel";
        case ExplorerFocusTarget::MoreActions: return "more_actions";
        case ExplorerFocusTarget::None:
        default:
            return "none";
    }
}

inline std::string focus_target_label(ExplorerFocusTarget target) {
    switch (target) {
        case ExplorerFocusTarget::Sidebar: return "Sidebar";
        case ExplorerFocusTarget::ToolbarNavigation:
            return "Toolbar Navigation";
        case ExplorerFocusTarget::ToolbarView: return "View Controls";
        case ExplorerFocusTarget::ToolbarActions: return "Toolbar Actions";
        case ExplorerFocusTarget::Search: return "Search";
        case ExplorerFocusTarget::ContentGrid: return "File Grid";
        case ExplorerFocusTarget::PreviewPanel: return "Preview";
        case ExplorerFocusTarget::CreatePanel: return "Create";
        case ExplorerFocusTarget::MoreActions: return "More Actions";
        case ExplorerFocusTarget::None:
        default:
            return "None";
    }
}

inline std::optional<ExplorerFocusTarget> focus_target_from_name(
        std::string_view value) {
    auto name = lower_copy(trim(value));
    if (name == "none" || name == "clear")
        return ExplorerFocusTarget::None;
    if (name == "sidebar" || name == "side-bar")
        return ExplorerFocusTarget::Sidebar;
    if (name == "toolbar-navigation" || name == "toolbar_navigation"
        || name == "navigation" || name == "nav")
        return ExplorerFocusTarget::ToolbarNavigation;
    if (name == "toolbar-view" || name == "toolbar_view"
        || name == "view-controls" || name == "view_controls")
        return ExplorerFocusTarget::ToolbarView;
    if (name == "toolbar-actions" || name == "toolbar_actions"
        || name == "actions")
        return ExplorerFocusTarget::ToolbarActions;
    if (name == "search" || name == "find")
        return ExplorerFocusTarget::Search;
    if (name == "content" || name == "grid" || name == "content-grid"
        || name == "content_grid" || name == "files")
        return ExplorerFocusTarget::ContentGrid;
    if (name == "preview" || name == "preview-panel"
        || name == "preview_panel")
        return ExplorerFocusTarget::PreviewPanel;
    if (name == "create" || name == "create-panel"
        || name == "create_panel")
        return ExplorerFocusTarget::CreatePanel;
    if (name == "more" || name == "more-actions" || name == "more_actions")
        return ExplorerFocusTarget::MoreActions;
    return std::nullopt;
}

inline std::span<ExplorerFocusTarget const> explorer_focus_order(
        std::string_view profile) {
    static constexpr std::array desktop_order{
        ExplorerFocusTarget::Sidebar,
        ExplorerFocusTarget::ToolbarNavigation,
        ExplorerFocusTarget::ToolbarView,
        ExplorerFocusTarget::ToolbarActions,
        ExplorerFocusTarget::Search,
        ExplorerFocusTarget::ContentGrid,
        ExplorerFocusTarget::PreviewPanel,
        ExplorerFocusTarget::CreatePanel,
    };
    static constexpr std::array mobile_order{
        ExplorerFocusTarget::Search,
        ExplorerFocusTarget::Sidebar,
        ExplorerFocusTarget::ContentGrid,
        ExplorerFocusTarget::PreviewPanel,
        ExplorerFocusTarget::CreatePanel,
    };
    return mobile_profile(profile)
        ? std::span<ExplorerFocusTarget const>{mobile_order}
        : std::span<ExplorerFocusTarget const>{desktop_order};
}

inline std::string focus_ring_visibility_reason(
        ExplorerState const& state) {
    if (state.focus_target == ExplorerFocusTarget::None)
        return "no_focused_target";
    if (state.last_input_modality == ExplorerInputModality::Keyboard
        && state.focus_visible)
        return "keyboard_focus_navigation";
    if (state.last_input_modality == ExplorerInputModality::Pointer)
        return "pointer_input_hides_focus_ring";
    if (state.last_input_modality == ExplorerInputModality::Programmatic)
        return "programmatic_input_does_not_show_focus_ring";
    return "unspecified_input_modality";
}

inline constexpr char k_focus_ring_style[] =
    "macos_blue_keyboard_focus_ring_outset_4px_2px_stroke";

inline SortMode default_sort_mode(std::string_view profile) {
    return mobile_profile(profile) ? SortMode::Name : SortMode::Recent;
}

inline ExplorerViewport default_viewport(std::string_view profile) {
    return mobile_profile(profile)
        ? ExplorerViewport{
            .width = k_mobile_default_viewport_width,
            .height = k_mobile_default_viewport_height,
            .scale = 2.0f,
        }
        : ExplorerViewport{
            .width = k_desktop_default_viewport_width,
            .height = k_desktop_default_viewport_height,
            .scale = 1.0f,
        };
}

inline ExplorerViewport effective_viewport(
        ExplorerState const& state,
        std::string_view profile) {
    auto viewport = default_viewport(profile);
    if (state.viewport_width > 0)
        viewport.width = state.viewport_width;
    if (state.viewport_height > 0)
        viewport.height = state.viewport_height;
    if (state.viewport_scale > 0.0f)
        viewport.scale = state.viewport_scale;
    return viewport;
}

inline void apply_default_viewport(
        ExplorerState& state,
        std::string_view profile) {
    auto viewport = default_viewport(profile);
    state.viewport_width = viewport.width;
    state.viewport_height = viewport.height;
    state.viewport_scale = viewport.scale;
}

inline float desktop_scroll_height_for_viewport(
        ExplorerViewport const& viewport,
        float chrome_budget,
        float minimum,
        float maximum) {
    float height = static_cast<float>(viewport.height) - chrome_budget;
    return std::clamp(height, minimum, maximum);
}

inline float desktop_scroll_height(
        ExplorerState const& state,
        float chrome_budget,
        float minimum,
        float maximum) {
    return desktop_scroll_height_for_viewport(
        effective_viewport(state, "desktop"),
        chrome_budget,
        minimum,
        maximum);
}

inline int desktop_icon_grid_column_count(ExplorerViewport const& viewport) {
    float const window_width = viewport.width > 0
        ? static_cast<float>(viewport.width)
        : static_cast<float>(k_desktop_default_viewport_width);
    float const available = std::max(
        k_desktop_icon_grid_column_pitch * 2.0f,
        window_width - k_desktop_sidebar_width - 80.0f);
    int columns = static_cast<int>(
        available / k_desktop_icon_grid_column_pitch);
    return std::clamp(columns, 2, 7);
}

inline float desktop_toolbar_shell_x() noexcept {
    return k_desktop_window_content_inset + k_desktop_sidebar_width
        + k_desktop_window_gap;
}

inline float desktop_toolbar_shell_width(
        ExplorerViewport const& viewport) noexcept {
    float const window_width = viewport.width > 0
        ? static_cast<float>(viewport.width)
        : static_cast<float>(k_desktop_default_viewport_width);
    return std::max(
        0.0f,
        window_width - desktop_toolbar_shell_x()
            - k_desktop_window_content_inset);
}

inline float desktop_toolbar_group_y() noexcept {
    return k_desktop_window_content_inset + k_desktop_toolbar_shell_padding;
}

inline float desktop_toolbar_navigation_group_x() noexcept {
    return desktop_toolbar_shell_x() + k_desktop_toolbar_shell_padding;
}

inline float desktop_toolbar_title_x() noexcept {
    return desktop_toolbar_navigation_group_x()
        + k_desktop_toolbar_navigation_group_width
        + k_desktop_toolbar_shell_padding;
}

inline float desktop_toolbar_search_group_x(
        ExplorerViewport const& viewport) noexcept {
    float const window_width = viewport.width > 0
        ? static_cast<float>(viewport.width)
        : static_cast<float>(k_desktop_default_viewport_width);
    return window_width - k_desktop_window_content_inset
        - k_desktop_toolbar_shell_padding
        - k_desktop_toolbar_search_collapsed_width;
}

inline float desktop_toolbar_action_group_x(
        ExplorerViewport const& viewport) noexcept {
    return desktop_toolbar_search_group_x(viewport)
        - k_desktop_toolbar_shell_padding
        - k_desktop_toolbar_action_group_width;
}

inline float desktop_toolbar_sort_group_x(
        ExplorerViewport const& viewport) noexcept {
    return desktop_toolbar_action_group_x(viewport)
        - k_desktop_toolbar_shell_padding
        - k_desktop_toolbar_sort_group_width;
}

inline float desktop_toolbar_view_group_x(
        ExplorerViewport const& viewport) noexcept {
    return desktop_toolbar_sort_group_x(viewport)
        - k_desktop_toolbar_shell_padding
        - k_desktop_toolbar_view_group_width;
}

inline float desktop_content_surface_y() noexcept {
    return k_desktop_window_content_inset + k_desktop_toolbar_shell_height
        + k_desktop_window_gap;
}

inline float desktop_sidebar_first_row_y() noexcept {
    return k_desktop_window_content_inset + k_desktop_sidebar_material_padding
        + k_desktop_titlebar_control_cluster_height
        + k_desktop_sidebar_item_gap;
}

inline bool desktop_status_bar_visible(ExplorerState const& state) {
    if (!state.selected_name.empty()
        || !state.last_operation.kind.empty()
        || !state.search.empty())
        return true;
    if (state.status == "Ready")
        return false;
    if (state.status.starts_with("Viewport set to ")
        || state.status.starts_with("Switched to ")
        || state.status.starts_with("Sorted by ")
        || state.status.starts_with("Draft "))
        return false;
    return true;
}


inline Snapshot snapshot(ExplorerState const& state) {
    Snapshot out;
    out.root = state.root;
    out.current = state.current;
    out.root_label = state.root_label;
    out.root_source = state.root_source;
    out.root_is_demo = state.root_is_demo;
    out.filesystem_mutations_allowed = state.filesystem_mutations_allowed;
    out.relative_location = relative_location(state);
    out.can_go_back = !state.back_stack.empty();
    out.can_go_forward = !state.forward_stack.empty();
    std::error_code ec;
    out.can_create_file =
        state.filesystem_mutations_allowed && fs::is_directory(state.current, ec)
        && !ec;
    out.can_create_folder = out.can_create_file;
    out.entries = list_entries(state.current, state.search, state.sort_mode);
    out.operation_label = operation_label(state.last_operation);
    out.sort_mode = state.sort_mode;
    out.sort_label = "Sort: " + sort_mode_label(state.sort_mode);
    out.view_mode = state.view_mode;
    for (std::size_t i = 0; i < out.entries.size(); ++i) {
        auto const& entry = out.entries[i];
        if (entry.folder)
            ++out.folder_count;
        else
            ++out.file_count;
        if (entry.name == state.selected_name) {
            out.selected = entry;
            out.has_selection = true;
            out.selected_index = i;
        }
    }
    out.item_summary = std::to_string(out.file_count) + " files";
    if (out.folder_count > 0)
        out.item_summary += ", " + std::to_string(out.folder_count) + " folders";
    if (out.has_selection) {
        auto const selected_path = state.current / out.selected.name;
        out.can_delete_selected = state.filesystem_mutations_allowed && (out.selected.folder
            ? deletable_directory(state.root, selected_path)
            : true);
        out.can_duplicate_selected =
            state.filesystem_mutations_allowed && !out.selected.folder;
        out.can_preview_selected = true;
        out.selected_kind_label = entry_kind_label(out.selected);
        out.selected_size_label = entry_size_label(out.selected);
        out.selected_path_label =
            out.relative_location + "/" + out.selected.name;
        out.action_summary = "Selected " + out.selected.name + " - "
            + out.selected_kind_label + " - " + out.selected_size_label;
        out.preview = read_preview(state.current / out.selected.name);
    } else {
        out.action_summary = state.filesystem_mutations_allowed
            ? "Select a file to read, duplicate, or delete it."
            : "Select a file to read it. Enable mutations explicitly before creating or deleting files.";
        out.preview = "Select a file to read its contents.";
    }
    return out;
}

inline ExplorerState make_state(std::string_view profile) {
    ExplorerState state;
    state.root = ensure_demo_tree(profile);
    state.current = state.root;
    state.root_label = "Demo Root";
    state.root_source = "demo-generated";
    state.root_is_demo = true;
    state.filesystem_mutations_allowed = true;
    state.selected_name.clear();
    state.sort_mode = default_sort_mode(profile);
    apply_default_viewport(state, profile);
    return state;
}

inline std::string default_root_label(fs::path const& root) {
    auto name = root.filename().string();
    if (!name.empty())
        return name;
    auto text = root.lexically_normal().generic_string();
    return text.empty() ? std::string{"Files"} : text;
}

inline ExplorerState make_state_for_root(
        std::string_view profile,
        fs::path root,
        std::string root_label,
        std::string root_source,
        bool filesystem_mutations_allowed) {
    ExplorerState state;
    std::error_code ec;
    auto normalized = fs::weakly_canonical(root, ec);
    if (ec || normalized.empty())
        normalized = root.lexically_normal();
    ec.clear();
    if (!fs::is_directory(normalized, ec) || ec) {
        state = make_state(profile);
        state.status = "Requested file explorer root is unavailable.";
        return state;
    }

    state.root = normalized;
    state.current = state.root;
    state.root_label = root_label.empty()
        ? default_root_label(state.root)
        : std::move(root_label);
    state.root_source = root_source.empty()
        ? std::string{"external-filesystem"}
        : std::move(root_source);
    state.root_is_demo = false;
    state.filesystem_mutations_allowed = filesystem_mutations_allowed;
    state.selected_name.clear();
    state.sort_mode = SortMode::Name;
    apply_default_viewport(state, profile);
    state.status = filesystem_mutations_allowed
        ? "Opened real filesystem location with file changes enabled."
        : "Opened real filesystem location in read-only mode.";
    return state;
}

inline void apply_runtime_preferences(
        ExplorerState& state,
        RuntimePreferenceState const& preferences) {
    state.preferences_source = preferences.source;
    state.system_settings = preferences.system_settings;
    state.theme_preferences = preferences.theme_preferences;
    state.effective_font_family = preferences.effective_font_family;
    state.effective_color_scheme = preferences.effective_color_scheme;
    state.effective_body_font_size = preferences.effective_body_font_size;
    state.effective_heading_font_size =
        preferences.effective_heading_font_size;
    state.effective_small_font_size = preferences.effective_small_font_size;
    state.effective_line_height_ratio =
        preferences.effective_line_height_ratio;
    state.effective_scroll_delta_multiplier =
        preferences.effective_scroll_delta_multiplier;
    state.effective_scroll_horizontal_delta_multiplier =
        preferences.effective_scroll_horizontal_delta_multiplier;
    state.effective_scroll_bar_visibility =
        preferences.effective_scroll_bar_visibility;
    state.effective_motion_duration_multiplier =
        preferences.effective_motion_duration_multiplier;
    state.used_system_font_family = preferences.used_system_font_family;
    state.used_system_color_scheme = preferences.used_system_color_scheme;
    state.used_system_font_metrics = preferences.used_system_font_metrics;
    state.used_system_font_scale = preferences.used_system_font_scale;
    state.used_user_font_scale = preferences.used_user_font_scale;
    state.used_user_font_size = preferences.used_user_font_size;
    state.used_system_line_height = preferences.used_system_line_height;
    state.used_user_line_height = preferences.used_user_line_height;
    state.used_system_scroll_metrics = preferences.used_system_scroll_metrics;
    state.used_user_scroll_scale = preferences.used_user_scroll_scale;
    state.used_user_scroll_bar_visibility =
        preferences.used_user_scroll_bar_visibility;
    state.used_system_accent_color = preferences.used_system_accent_color;
    state.used_system_reduce_motion = preferences.used_system_reduce_motion;
    state.used_user_motion_scale = preferences.used_user_motion_scale;

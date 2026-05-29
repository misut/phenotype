inline json::Value icon_color_debug_json(icon_catalog::SymbolColor color) {
    json::Object out;
    out.emplace("r", json::Value{static_cast<std::int64_t>(color.r)});
    out.emplace("g", json::Value{static_cast<std::int64_t>(color.g)});
    out.emplace("b", json::Value{static_cast<std::int64_t>(color.b)});
    out.emplace("a", json::Value{static_cast<std::int64_t>(color.a)});
    return json::Value{std::move(out)};
}

inline json::Value icon_source_attribution_debug_json(
        icon_catalog::Symbol symbol) {
    auto const source = icon_catalog::source_attribution(symbol);
    json::Object out;
    out.emplace("family", json::Value{std::string{source.family}});
    out.emplace("icon_name", json::Value{std::string{source.icon_name}});
    out.emplace("style", json::Value{std::string{source.style}});
    out.emplace("license", json::Value{std::string{source.license}});
    out.emplace("license_url", json::Value{std::string{source.license_url}});
    out.emplace("source_url", json::Value{std::string{source.source_url}});
    out.emplace(
        "source_revision",
        json::Value{std::string{source.source_revision}});
    out.emplace("copyright", json::Value{std::string{source.copyright}});
    out.emplace("embedded_source", json::Value{source.embedded_source});
    out.emplace(
        "modified_for_phenotype",
        json::Value{source.modified_for_phenotype});
    out.emplace("apple_asset", json::Value{source.apple_asset});
    out.emplace("platform_extracted", json::Value{source.platform_extracted});
    out.emplace(
        "runtime_fetch_required",
        json::Value{source.runtime_fetch_required});
    return json::Value{std::move(out)};
}

inline json::Value material_symbols_styles_debug_json() {
    json::Array out;
    for (unsigned int i = 0; i < icon_catalog::material_symbols_style_count; ++i) {
        auto const style = icon_catalog::material_symbols_style_at(i);
        json::Object item;
        item.emplace(
            "name",
            json::Value{std::string{icon_catalog::material_symbols_style_name(style)}});
        item.emplace(
            "label",
            json::Value{std::string{icon_catalog::material_symbols_style_label(style)}});
        item.emplace(
            "font_family",
            json::Value{std::string{icon_catalog::material_symbols_font_family(style)}});
        item.emplace(
            "css_class",
            json::Value{std::string{icon_catalog::material_symbols_css_class(style)}});
        item.emplace(
            "source_directory",
            json::Value{std::string{icon_catalog::material_symbols_source_directory(style)}});
        item.emplace(
            "default",
            json::Value{icon_catalog::is_default_material_symbols_style(style)});
        out.push_back(json::Value{std::move(item)});
    }
    return json::Value{std::move(out)};
}

inline json::Value icon_reference_sources_debug_json() {
    json::Array out;
    for (unsigned int i = 0; i < icon_catalog::reference_source_count; ++i) {
        auto const source = icon_catalog::reference_source_at(i);
        json::Object item;
        item.emplace("name", json::Value{std::string{source.name}});
        item.emplace("url", json::Value{std::string{source.url}});
        item.emplace("role", json::Value{std::string{source.role}});
        item.emplace(
            "license_policy",
            json::Value{std::string{source.license_policy}});
        item.emplace(
            "used_as_embedded_asset_source",
            json::Value{source.used_as_embedded_asset_source});
        item.emplace(
            "apple_owned_artwork",
            json::Value{source.apple_owned_artwork});
        out.push_back(json::Value{std::move(item)});
    }
    return json::Value{std::move(out)};
}

inline icon_catalog::SymbolColor icon_color_with_opacity(
        icon_catalog::SymbolColor color,
        float opacity) {
    auto alpha = static_cast<float>(color.a)
        * std::clamp(opacity, 0.0f, 1.0f);
    auto rounded = static_cast<int>(alpha + 0.5f);
    color.a = static_cast<unsigned char>(std::clamp(rounded, 0, 255));
    return color;
}

inline double icon_contract_round_hundredth(float value) {
    auto scaled = static_cast<int>(value * 100.0f + 0.5f);
    return static_cast<double>(scaled) / 100.0;
}

inline json::Value icon_rendering_capabilities_debug_json(
        icon_catalog::Symbol symbol) {
    auto const capabilities = icon_catalog::rendering_capabilities(symbol);
    json::Object out;
    out.emplace("policy", json::Value{std::string{capabilities.policy}});
    out.emplace("monochrome", json::Value{capabilities.monochrome});
    out.emplace("hierarchical", json::Value{capabilities.hierarchical});
    out.emplace("palette", json::Value{capabilities.palette});
    out.emplace("multicolor", json::Value{capabilities.multicolor});
    return json::Value{std::move(out)};
}

inline json::Value icon_presentation_debug_json(
        icon_catalog::Symbol symbol,
        icon_catalog::SymbolPresentationRole role,
        icon_catalog::SymbolInteractionState state,
        icon_catalog::SymbolInteractionPhase phase) {
    auto const desc = icon_catalog::descriptor(symbol);
    auto const metrics = icon_catalog::metrics(role);
    auto const recipe = icon_catalog::macos_state_recipe(role, state, phase);
    auto const visible_color =
        icon_color_with_opacity(recipe.symbol_color, recipe.symbol_opacity);
    auto const effective_point_size =
        icon_contract_round_hundredth(metrics.point_size * recipe.scale);
    auto const content_inset = icon_contract_round_hundredth(
        (recipe.hit_target_size - static_cast<float>(effective_point_size))
        / 2.0f);
    json::Object out;
    out.emplace("schema_version", json::Value{1});
    out.emplace("symbol", json::Value{std::string{desc.name}});
    out.emplace(
        "semantic_reference_name",
        json::Value{std::string{desc.semantic_reference_name}});
    out.emplace(
        "source_attribution",
        icon_source_attribution_debug_json(symbol));
    out.emplace(
        "role",
        json::Value{std::string{
            icon_catalog::symbol_presentation_role_name(role)}});
    out.emplace(
        "phase",
        json::Value{std::string{
            icon_catalog::symbol_interaction_phase_name(phase)}});
    out.emplace("selected", json::Value{state.selected});
    out.emplace("enabled", json::Value{state.enabled});
    out.emplace("policy", json::Value{std::string{recipe.policy}});
    out.emplace(
        "rendering",
        json::Value{std::string{
            icon_catalog::symbol_rendering_mode_name(
                desc.preferred_rendering)}});
    out.emplace(
        "rendering_capabilities",
        icon_rendering_capabilities_debug_json(symbol));
    out.emplace(
        "symbol_tone",
        json::Value{std::string{
            icon_catalog::symbol_tone_name(recipe.symbol_tone)}});
    out.emplace(
        "base_symbol_color",
        icon_color_debug_json(recipe.symbol_color));
    out.emplace(
        "visible_symbol_color",
        icon_color_debug_json(visible_color));
    out.emplace(
        "background_color",
        icon_color_debug_json(recipe.background_color));
    out.emplace(
        "symbol_opacity",
        json::Value{icon_contract_round_hundredth(recipe.symbol_opacity)});
    out.emplace("phase_scale", json::Value{recipe.scale});
    out.emplace("point_size", json::Value{metrics.point_size});
    out.emplace("effective_point_size", json::Value{effective_point_size});
    out.emplace("hit_target_size", json::Value{recipe.hit_target_size});
    out.emplace("content_inset", json::Value{content_inset});
    out.emplace("optical_y_offset", json::Value{metrics.optical_y_offset});
    out.emplace("corner_radius", json::Value{recipe.corner_radius});
    out.emplace("borderless", json::Value{recipe.borderless});
    out.emplace("grouped_control", json::Value{recipe.grouped_control});
    out.emplace("likely_layer", json::Value{"icon_glyph"});
    out.emplace("likely_pass", json::Value{"svg_path_or_line_paint"});
    return json::Value{std::move(out)};
}

inline json::Value entry_debug_json(Entry const& entry) {
    json::Object out;
    auto const symbol = entry_symbol(entry);
    out.emplace("name", json::Value{entry.name});
    out.emplace("kind", json::Value{entry_kind_label(entry)});
    out.emplace("symbol", json::Value{std::string{icon_catalog::name(symbol)}});
    out.emplace(
        "symbol_semantic_reference_name",
        json::Value{std::string{
            icon_catalog::semantic_reference_name(symbol)}});
    out.emplace(
        "symbol_package_asset_name",
        json::Value{file_type_icon_asset_name_for_symbol(symbol)});
    out.emplace(
        "symbol_package_asset_source",
        json::Value{file_type_icon_asset_source_for_symbol(symbol)});
    out.emplace(
        "symbol_package_asset_policy",
        json::Value{std::string{file_type_icon_asset_policy()}});
    out.emplace(
        "symbol_presentation",
        icon_presentation_debug_json(
            symbol,
            icon_catalog::SymbolPresentationRole::FileType,
            icon_catalog::SymbolInteractionState{false, true},
            icon_catalog::SymbolInteractionPhase::Normal));
    out.emplace("folder", json::Value{entry.folder});
    out.emplace("size", json::Value{static_cast<std::int64_t>(entry.size)});
    return json::Value{std::move(out)};
}

inline json::Value entry_symbol_summary_debug_json(Snapshot const& snap) {
    json::Object by_symbol;
    std::int64_t symbolized = 0;
    for (unsigned int i = 0; i < icon_catalog::file_type_symbol_count; ++i) {
        auto const symbol = icon_catalog::file_type_symbol_at(i);
        std::int64_t count = 0;
        for (auto const& entry : snap.entries) {
            if (entry_symbol(entry) == symbol)
                ++count;
        }
        if (count > 0)
            symbolized += count;
        by_symbol.emplace(
            std::string{icon_catalog::name(symbol)},
            json::Value{count});
    }

    json::Object out;
    out.emplace(
        "source",
        json::Value{"file_explorer_shared::entry_symbol"});
    out.emplace(
        "presentation_role",
        json::Value{std::string{
            icon_catalog::symbol_presentation_role_name(
                icon_catalog::SymbolPresentationRole::FileType)}});
    out.emplace(
        "file_type_symbol_count",
        json::Value{static_cast<std::int64_t>(
            icon_catalog::file_type_symbol_count)});
    out.emplace(
        "package_asset_policy",
        json::Value{std::string{file_type_icon_asset_policy()}});
    out.emplace(
        "package_asset_source_family",
        json::Value{std::string{file_type_icon_source_family()}});
    out.emplace(
        "package_asset_source_revision",
        json::Value{std::string{file_type_icon_source_revision()}});
    out.emplace(
        "package_asset_count",
        json::Value{static_cast<std::int64_t>(
            icon_catalog::file_type_symbol_count)});
    out.emplace(
        "visible_entry_count",
        json::Value{static_cast<std::int64_t>(snap.entries.size())});
    out.emplace(
        "visible_entry_symbol_count",
        json::Value{symbolized});
    out.emplace(
        "all_visible_entries_have_symbols",
        json::Value{symbolized == static_cast<std::int64_t>(
            snap.entries.size())});
    out.emplace("by_symbol", json::Value{std::move(by_symbol)});
    return json::Value{std::move(out)};
}

inline FinderVisualContract finder_visual_contract(
        ExplorerChromeMetrics const& chrome,
        std::string_view profile) {
    return FinderVisualContract{
        .schema_version = 1,
        .name = std::string{k_finder_visual_contract_name},
        .profile = std::string{profile},
        .source = std::string{k_finder_visual_contract_source},
        .reference = std::string{k_finder_visual_contract_reference},
        .titlebar_strategy = mobile_profile(profile)
            ? "not_applicable_mobile_shell"
            : std::string{k_finder_visual_titlebar_strategy},
        .native_control_owner = chrome.native_window_control_owner,
        .native_control_marker_policy =
            std::string{k_finder_visual_marker_policy},
        .native_control_geometry_role =
            chrome.native_window_control_geometry_role,
        .native_control_palette_policy =
            chrome.native_window_control_palette_policy,
        .sidebar_selection_style = chrome.sidebar_selection_policy,
        .sidebar_selected_row_border_width =
            k_desktop_sidebar_selected_row_border_width,
        .sidebar_selected_row_background_alpha =
            chrome.sidebar_selected_row_background_alpha,
        .focus_ring_policy = std::string{k_finder_visual_focus_ring_policy},
        .icon_source_policy = chrome.icon_source_license_policy,
        .embedded_svg_policy = chrome.icon_source_attribution_policy,
        .apple_asset_symbol_count = chrome.icon_apple_asset_symbol_count,
        .platform_extracted_symbol_count =
            chrome.icon_platform_extracted_symbol_count,
        .runtime_fetched_symbol_count =
            chrome.icon_runtime_fetched_symbol_count,
        .thumbnail_preview_policy = chrome.thumbnail_visual_policy,
        .leading_control_reserved_width =
            chrome.leading_control_reserved_width,
        .titlebar_drag_region_height = chrome.titlebar_drag_region_height,
        .verifier_gate = std::string{k_finder_visual_verifier_gate},
    };
}

inline json::Value finder_visual_contract_debug_json(
        ExplorerChromeMetrics const& chrome,
        std::string_view profile) {
    auto contract = finder_visual_contract(chrome, profile);
    json::Object out;
    out.emplace(
        "schema_version",
        json::Value{static_cast<std::int64_t>(contract.schema_version)});
    out.emplace("name", json::Value{contract.name});
    out.emplace("profile", json::Value{contract.profile});
    out.emplace("source", json::Value{contract.source});
    out.emplace("reference", json::Value{contract.reference});
    out.emplace("titlebar_strategy", json::Value{contract.titlebar_strategy});
    out.emplace(
        "native_control_owner",
        json::Value{contract.native_control_owner});
    out.emplace(
        "native_control_marker_policy",
        json::Value{contract.native_control_marker_policy});
    out.emplace(
        "native_control_geometry_role",
        json::Value{contract.native_control_geometry_role});
    out.emplace(
        "native_control_palette_policy",
        json::Value{contract.native_control_palette_policy});
    out.emplace(
        "sidebar_selection_style",
        json::Value{contract.sidebar_selection_style});
    out.emplace(
        "sidebar_selected_row_border_width",
        json::Value{contract.sidebar_selected_row_border_width});
    out.emplace(
        "sidebar_selected_row_background_alpha",
        json::Value{static_cast<std::int64_t>(
            contract.sidebar_selected_row_background_alpha)});
    out.emplace("focus_ring_policy", json::Value{contract.focus_ring_policy});
    out.emplace("icon_source_policy", json::Value{contract.icon_source_policy});
    out.emplace("embedded_svg_policy", json::Value{contract.embedded_svg_policy});
    out.emplace(
        "apple_asset_symbol_count",
        json::Value{static_cast<std::int64_t>(
            contract.apple_asset_symbol_count)});
    out.emplace(
        "platform_extracted_symbol_count",
        json::Value{static_cast<std::int64_t>(
            contract.platform_extracted_symbol_count)});
    out.emplace(
        "runtime_fetched_symbol_count",
        json::Value{static_cast<std::int64_t>(
            contract.runtime_fetched_symbol_count)});
    out.emplace(
        "thumbnail_preview_policy",
        json::Value{contract.thumbnail_preview_policy});
    out.emplace(
        "leading_control_reserved_width",
        json::Value{contract.leading_control_reserved_width});
    out.emplace(
        "titlebar_drag_region_height",
        json::Value{contract.titlebar_drag_region_height});
    out.emplace("verifier_gate", json::Value{contract.verifier_gate});
    return json::Value{std::move(out)};
}

inline json::Value operation_plan_debug_json(OperationPlan const& plan) {
    json::Object out;
    out.emplace("kind", json::Value{plan.kind});
    out.emplace("display_name", json::Value{plan.display_name});
    out.emplace("source", json::Value{plan.source});
    out.emplace("destination", json::Value{plan.destination});
    out.emplace("executable", json::Value{plan.executable});
    out.emplace("sandboxed", json::Value{plan.sandboxed});
    out.emplace("mutates_filesystem", json::Value{plan.mutates_filesystem});
    out.emplace("reads_file", json::Value{plan.reads_file});
    out.emplace("reads_directory", json::Value{plan.reads_directory});
    out.emplace("writes_file", json::Value{plan.writes_file});
    out.emplace("creates_directory", json::Value{plan.creates_directory});
    out.emplace("deletes_file", json::Value{plan.deletes_file});
    out.emplace("deletes_directory", json::Value{plan.deletes_directory});
    out.emplace("moves_to_trash", json::Value{plan.moves_to_trash});
    out.emplace("permanent_delete", json::Value{plan.permanent_delete});
    out.emplace("fallback_reason", json::Value{plan.fallback_reason});
    return json::Value{std::move(out)};
}

inline json::Value operation_receipt_debug_json(
        OperationReceipt const& receipt,
        std::string const& label) {
    json::Object out;
    out.emplace("kind", json::Value{receipt.kind});
    out.emplace("target", json::Value{receipt.target});
    out.emplace("ok", json::Value{receipt.ok});
    out.emplace("detail", json::Value{receipt.detail});
    out.emplace("label", json::Value{label});
    out.emplace("plan", operation_plan_debug_json(receipt.plan));
    return json::Value{std::move(out)};
}

inline std::vector<ExplorerKeyboardCommand> file_explorer_keyboard_commands(
        std::string_view profile) {
    if (profile == "mobile")
        return {};
    return {
        {"show_search", "CommandOrControl+F", "shortcut:find", true},
        {"activate_selected", "Enter", "key:enter", false},
        {"move_selection_up", "ArrowUp", "key:arrow-up", false},
        {"move_selection_down", "ArrowDown", "key:arrow-down", false},
        {"move_selection_left", "ArrowLeft", "key:arrow-left", false},
        {"move_selection_right", "ArrowRight", "key:arrow-right", false},
        {"move_selection_home", "Home", "key:home", false},
        {"move_selection_end", "End", "key:end", false},
        {"move_selection_page_up", "PageUp", "key:page-up", false},
        {"move_selection_page_down", "PageDown", "key:page-down", false},
        {"delete_selected", "DeleteOrBackspace", "key:delete", false},
        {"duplicate_selected", "CommandOrControl+D", "shortcut:duplicate", false},
        {"create_folder", "Shift+CommandOrControl+N", "shortcut:new-folder", false},
        {"dismiss_transient", "Escape", "key:escape", true},
    };
}

inline json::Value keyboard_commands_debug_json(std::string_view profile) {
    json::Array commands;
    for (auto const& command : file_explorer_keyboard_commands(profile)) {
        json::Object item;
        item.emplace("action", json::Value{command.action});
        item.emplace("shortcut", json::Value{command.shortcut});
        item.emplace("input_alias", json::Value{command.input_alias});
        item.emplace(
            "allow_when_input_focused",
            json::Value{command.allow_when_input_focused});
        commands.push_back(json::Value{std::move(item)});
    }
    return json::Value{std::move(commands)};
}

inline json::Value focus_order_debug_json(std::string_view profile) {
    json::Array out;
    for (auto target : explorer_focus_order(profile))
        out.push_back(json::Value{focus_target_value_name(target)});
    return json::Value{std::move(out)};
}

inline json::Value input_model_debug_json(
        ExplorerState const& state,
        std::string_view profile) {
    json::Object out;
    out.emplace(
        "abstraction_policy",
        json::Value{
            "external_input_to_pure_explorer_input_to_renderer_output"});
    out.emplace(
        "focus_visibility_policy",
        json::Value{
            "keyboard_tab_navigation_shows_ring_pointer_click_hides_ring"});
    out.emplace(
        "focus_ring_style",
        json::Value{std::string{k_focus_ring_style}});
    out.emplace(
        "last_input_modality",
        json::Value{input_modality_value_name(state.last_input_modality)});
    out.emplace(
        "focus_target",
        json::Value{focus_target_value_name(state.focus_target)});
    out.emplace(
        "focus_target_label",
        json::Value{focus_target_label(state.focus_target)});
    out.emplace("focus_visible", json::Value{state.focus_visible});
    out.emplace(
        "focus_visibility_reason",
        json::Value{focus_ring_visibility_reason(state)});
    out.emplace("focus_order", focus_order_debug_json(profile));
    return json::Value{std::move(out)};
}

inline json::Value system_settings_debug_json(
        SystemPreferenceSnapshot const& settings) {
    json::Object accent_color;
    accent_color.emplace("r", json::Value{static_cast<std::int64_t>(
        settings.accent_color.r)});
    accent_color.emplace("g", json::Value{static_cast<std::int64_t>(
        settings.accent_color.g)});
    accent_color.emplace("b", json::Value{static_cast<std::int64_t>(
        settings.accent_color.b)});
    accent_color.emplace("a", json::Value{static_cast<std::int64_t>(
        settings.accent_color.a)});

    json::Object out;
    out.emplace("source", json::Value{settings.source});
    out.emplace("font_family", json::Value{settings.font_family});
    out.emplace("font_family_source", json::Value{settings.font_family_source});
    out.emplace("body_font_size", json::Value{settings.body_font_size});
    out.emplace("heading_font_size", json::Value{settings.heading_font_size});
    out.emplace("small_font_size", json::Value{settings.small_font_size});
    out.emplace("line_height_ratio", json::Value{settings.line_height_ratio});
    out.emplace("font_scale", json::Value{settings.font_scale});
    out.emplace("text_size_source", json::Value{settings.text_size_source});
    out.emplace(
        "font_weight_adjustment",
        json::Value{static_cast<std::int64_t>(
            settings.font_weight_adjustment)});
    out.emplace("font_weight_source", json::Value{settings.font_weight_source});
    out.emplace(
        "preferred_scroller_style",
        json::Value{settings.preferred_scroller_style});
    out.emplace("overlay_scrollbars", json::Value{settings.overlay_scrollbars});
    out.emplace("scroll_line_height", json::Value{settings.scroll_line_height});
    out.emplace("scroll_wheel_lines", json::Value{settings.scroll_wheel_lines});
    out.emplace("scroll_page_mode", json::Value{settings.scroll_page_mode});
    out.emplace(
        "scroll_vertical_factor",
        json::Value{settings.scroll_vertical_factor});
    out.emplace(
        "scroll_horizontal_factor",
        json::Value{settings.scroll_horizontal_factor});
    out.emplace("scroll_bar_size", json::Value{settings.scroll_bar_size});
    out.emplace("touch_slop", json::Value{settings.touch_slop});
    out.emplace("scroll_friction", json::Value{settings.scroll_friction});
    out.emplace(
        "scroll_delta_multiplier",
        json::Value{settings.scroll_delta_multiplier});
    out.emplace(
        "scroll_horizontal_delta_multiplier",
        json::Value{settings.scroll_horizontal_delta_multiplier});
    out.emplace("scroll_source", json::Value{settings.scroll_source});
    out.emplace(
        "font_family_available",
        json::Value{settings.font_family_available});
    out.emplace(
        "font_metrics_available",
        json::Value{settings.font_metrics_available});
    out.emplace(
        "font_scale_available",
        json::Value{settings.font_scale_available});
    out.emplace(
        "line_height_available",
        json::Value{settings.line_height_available});
    out.emplace(
        "scroll_metrics_available",
        json::Value{settings.scroll_metrics_available});
    out.emplace(
        "color_scheme_available",
        json::Value{settings.color_scheme_available});
    out.emplace(
        "reduce_motion_available",
        json::Value{settings.reduce_motion_available});
    out.emplace(
        "double_click_interval_ms",
        json::Value{settings.double_click_interval_ms});
    out.emplace(
        "key_repeat_delay_ms",
        json::Value{settings.key_repeat_delay_ms});
    out.emplace(
        "key_repeat_interval_ms",
        json::Value{settings.key_repeat_interval_ms});
    out.emplace(
        "caret_blink_interval_ms",
        json::Value{settings.caret_blink_interval_ms});
    out.emplace(
        "input_timing_source",
        json::Value{settings.input_timing_source});
    out.emplace("preferred_locale", json::Value{settings.preferred_locale});
    out.emplace(
        "preferred_locale_source",
        json::Value{settings.preferred_locale_source});
    out.emplace("color_scheme", json::Value{settings.color_scheme});
    out.emplace(
        "color_scheme_source",
        json::Value{settings.color_scheme_source});
    out.emplace("appearance_name", json::Value{settings.appearance_name});
    out.emplace(
        "reduce_transparency",
        json::Value{settings.reduce_transparency});
    out.emplace(
        "increase_contrast",
        json::Value{settings.increase_contrast});
    out.emplace("reduce_motion", json::Value{settings.reduce_motion});
    out.emplace(
        "accessibility_source",
        json::Value{settings.accessibility_source});
    out.emplace(
        "accent_color_available",
        json::Value{settings.accent_color_available});
    out.emplace("accent_color", json::Value{std::move(accent_color)});
    out.emplace(
        "accent_color_source",
        json::Value{settings.accent_color_source});
    out.emplace(
        "accent_color_opaque",
        json::Value{settings.accent_color_opaque});
    return json::Value{std::move(out)};
}

inline json::Value preferences_debug_json(ExplorerState const& state) {
    json::Object overrides;
    overrides.emplace(
        "font_family",
        json::Value{state.theme_preferences.font_family});
    overrides.emplace(
        "color_scheme",
        json::Value{state.theme_preferences.color_scheme});
    overrides.emplace(
        "font_scale",
        json::Value{state.theme_preferences.font_scale});
    overrides.emplace(
        "body_font_size",
        json::Value{state.theme_preferences.body_font_size});
    overrides.emplace(
        "heading_font_size",
        json::Value{state.theme_preferences.heading_font_size});
    overrides.emplace(
        "small_font_size",
        json::Value{state.theme_preferences.small_font_size});
    overrides.emplace(
        "line_height_ratio",
        json::Value{state.theme_preferences.line_height_ratio});
    overrides.emplace(
        "scroll_delta_multiplier",
        json::Value{state.theme_preferences.scroll_delta_multiplier});
    overrides.emplace(
        "scroll_horizontal_delta_multiplier",
        json::Value{
            state.theme_preferences.scroll_horizontal_delta_multiplier});
    overrides.emplace(
        "scroll_bar_visibility",
        json::Value{state.theme_preferences.scroll_bar_visibility});
    overrides.emplace(
        "prefer_system_font_family",
        json::Value{state.theme_preferences.prefer_system_font_family});
    overrides.emplace(
        "prefer_system_color_scheme",
        json::Value{state.theme_preferences.prefer_system_color_scheme});
    overrides.emplace(
        "apply_system_font_metrics",
        json::Value{state.theme_preferences.apply_system_font_metrics});
    overrides.emplace(
        "apply_system_font_scale",
        json::Value{state.theme_preferences.apply_system_font_scale});
    overrides.emplace(
        "apply_system_scroll_metrics",
        json::Value{state.theme_preferences.apply_system_scroll_metrics});
    overrides.emplace(
        "apply_system_accent_color",
        json::Value{state.theme_preferences.apply_system_accent_color});
    overrides.emplace(
        "apply_system_reduce_motion",
        json::Value{state.theme_preferences.apply_system_reduce_motion});
    overrides.emplace(
        "motion_duration_multiplier",
        json::Value{state.theme_preferences.motion_duration_multiplier});

    json::Object effective;
    effective.emplace(
        "font_family",
        json::Value{state.effective_font_family});
    effective.emplace(
        "color_scheme",
        json::Value{state.effective_color_scheme});
    effective.emplace(
        "body_font_size",
        json::Value{state.effective_body_font_size});
    effective.emplace(
        "heading_font_size",
        json::Value{state.effective_heading_font_size});
    effective.emplace(
        "small_font_size",
        json::Value{state.effective_small_font_size});
    effective.emplace(
        "line_height_ratio",
        json::Value{state.effective_line_height_ratio});
    effective.emplace(
        "scroll_delta_multiplier",
        json::Value{state.effective_scroll_delta_multiplier});
    effective.emplace(
        "scroll_horizontal_delta_multiplier",
        json::Value{state.effective_scroll_horizontal_delta_multiplier});
    effective.emplace(
        "scroll_bar_visibility",
        json::Value{state.effective_scroll_bar_visibility});
    effective.emplace(
        "motion_duration_multiplier",
        json::Value{state.effective_motion_duration_multiplier});

    json::Object resolution;
    resolution.emplace(
        "used_system_font_family",
        json::Value{state.used_system_font_family});
    resolution.emplace(
        "used_system_color_scheme",
        json::Value{state.used_system_color_scheme});
    resolution.emplace(
        "used_system_font_metrics",
        json::Value{state.used_system_font_metrics});
    resolution.emplace(
        "used_system_font_scale",
        json::Value{state.used_system_font_scale});
    resolution.emplace(
        "used_user_font_scale",
        json::Value{state.used_user_font_scale});
    resolution.emplace(
        "used_user_font_size",
        json::Value{state.used_user_font_size});
    resolution.emplace(
        "used_system_line_height",
        json::Value{state.used_system_line_height});
    resolution.emplace(
        "used_user_line_height",
        json::Value{state.used_user_line_height});
    resolution.emplace(
        "used_system_scroll_metrics",
        json::Value{state.used_system_scroll_metrics});
    resolution.emplace(
        "used_user_scroll_scale",
        json::Value{state.used_user_scroll_scale});
    resolution.emplace(
        "used_user_scroll_bar_visibility",
        json::Value{state.used_user_scroll_bar_visibility});
    resolution.emplace(
        "used_system_accent_color",
        json::Value{state.used_system_accent_color});
    resolution.emplace(
        "used_system_reduce_motion",
        json::Value{state.used_system_reduce_motion});
    resolution.emplace(
        "used_user_motion_scale",
        json::Value{state.used_user_motion_scale});

    json::Object out;
    out.emplace("source", json::Value{state.preferences_source});
    out.emplace(
        "system_settings",
        system_settings_debug_json(state.system_settings));
    out.emplace("app_overrides", json::Value{std::move(overrides)});
    out.emplace("effective_theme", json::Value{std::move(effective)});
    out.emplace("resolution", json::Value{std::move(resolution)});
    out.emplace(
        "font_policy",
        json::Value{
            "Pretendard package default, OS font size metrics applied, user override wins"});
    out.emplace(
        "appearance_policy",
        json::Value{
            "OS appearance and accent applied by default, user override wins"});
    out.emplace(
        "scroll_policy",
        json::Value{
            "OS scroll deltas/line settings at edge, app can disable static "
            "OS scroll multipliers, axis theme multipliers are pure inputs"});
    out.emplace(
        "system_refresh_policy",
        json::Value{
            "native edge captures OS settings before first frame and refreshes "
            "them at update-boundary theme sync"});
    return json::Value{std::move(out)};
}

inline json::Value string_array_debug_json(
        std::initializer_list<std::string_view> values) {
    json::Array out;
    for (auto value : values)
        out.push_back(json::Value{std::string{value}});
    return json::Value{std::move(out)};
}

inline json::Value sidebar_icon_reference_symbols_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Array{}};
    json::Array out;
    for (unsigned int i = 0; i < icon_catalog::sidebar_symbol_count; ++i) {
        out.push_back(json::Value{std::string{
            icon_catalog::semantic_reference_name(
                icon_catalog::sidebar_symbol_at(i))}});
    }
    return json::Value{std::move(out)};
}

inline json::Value sidebar_symbol_tokens_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Array{}};
    json::Array out;
    for (auto const& item : desktop_sidebar_symbol_contract()) {
        json::Object token;
        token.emplace("token", json::Value{std::string{item.token}});
        token.emplace(
            "symbol",
            json::Value{std::string{icon_catalog::name(item.symbol)}});
        token.emplace(
            "semantic_reference_name",
            json::Value{std::string{
                icon_catalog::semantic_reference_name(item.symbol)}});
        out.push_back(json::Value{std::move(token)});
    }
    return json::Value{std::move(out)};
}

inline json::Value sidebar_symbol_presentations_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Array{}};
    json::Array out;
    for (auto const& item : desktop_sidebar_symbol_contract()) {
        json::Object token;
        token.emplace("token", json::Value{std::string{item.token}});
        token.emplace(
            "symbol",
            json::Value{std::string{icon_catalog::name(item.symbol)}});
        token.emplace(
            "semantic_reference_name",
            json::Value{std::string{
                icon_catalog::semantic_reference_name(item.symbol)}});
        token.emplace(
            "unselected_presentation",
            icon_presentation_debug_json(
                item.symbol,
                icon_catalog::SymbolPresentationRole::Sidebar,
                icon_catalog::SymbolInteractionState{false, true},
                icon_catalog::SymbolInteractionPhase::Normal));
        token.emplace(
            "selected_presentation",
            icon_presentation_debug_json(
                item.symbol,
                icon_catalog::SymbolPresentationRole::Sidebar,
                icon_catalog::SymbolInteractionState{true, true},
                icon_catalog::SymbolInteractionPhase::Normal));
        out.push_back(json::Value{std::move(token)});
    }
    return json::Value{std::move(out)};
}

inline json::Value toolbar_icon_reference_symbols_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Array{}};
    json::Array out;
    for (unsigned int i = 0; i < icon_catalog::toolbar_symbol_count; ++i) {
        out.push_back(json::Value{std::string{
            icon_catalog::semantic_reference_name(
                icon_catalog::toolbar_symbol_at(i))}});
    }
    return json::Value{std::move(out)};
}

inline json::Value file_type_symbol_tokens_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Array{}};
    json::Array out;
    for (auto const& item : file_type_symbol_contract()) {
        json::Object token;
        token.emplace("token", json::Value{std::string{item.token}});
        token.emplace(
            "symbol",
            json::Value{std::string{icon_catalog::name(item.symbol)}});
        token.emplace(
            "semantic_reference_name",
            json::Value{std::string{
                icon_catalog::semantic_reference_name(item.symbol)}});
        token.emplace(
            "package_asset_name",
            json::Value{file_type_icon_asset_name_for_symbol(item.symbol)});
        token.emplace(
            "package_asset_source",
            json::Value{file_type_icon_asset_source_for_symbol(item.symbol)});
        token.emplace(
            "package_asset_policy",
            json::Value{std::string{file_type_icon_asset_policy()}});
        out.push_back(json::Value{std::move(token)});
    }
    return json::Value{std::move(out)};
}

inline json::Value file_type_icon_reference_symbols_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Array{}};
    json::Array out;
    for (unsigned int i = 0; i < icon_catalog::file_type_symbol_count; ++i) {
        out.push_back(json::Value{std::string{
            icon_catalog::semantic_reference_name(
                icon_catalog::file_type_symbol_at(i))}});
    }
    return json::Value{std::move(out)};
}

inline json::Value file_type_symbol_presentations_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Array{}};
    json::Array out;
    for (auto const& item : file_type_symbol_contract()) {
        json::Object token;
        token.emplace("token", json::Value{std::string{item.token}});
        token.emplace(
            "symbol",
            json::Value{std::string{icon_catalog::name(item.symbol)}});
        token.emplace(
            "semantic_reference_name",
            json::Value{std::string{
                icon_catalog::semantic_reference_name(item.symbol)}});
        token.emplace(
            "package_asset_name",
            json::Value{file_type_icon_asset_name_for_symbol(item.symbol)});
        token.emplace(
            "package_asset_source",
            json::Value{file_type_icon_asset_source_for_symbol(item.symbol)});
        token.emplace(
            "source_attribution",
            icon_source_attribution_debug_json(item.symbol));
        token.emplace(
            "presentation",
            icon_presentation_debug_json(
                item.symbol,
                icon_catalog::SymbolPresentationRole::FileType,
                icon_catalog::SymbolInteractionState{false, true},
                icon_catalog::SymbolInteractionPhase::Normal));
        out.push_back(json::Value{std::move(token)});
    }
    return json::Value{std::move(out)};
}

inline json::Value toolbar_symbol_presentations_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Array{}};
    json::Array out;
    for (unsigned int i = 0; i < icon_catalog::toolbar_symbol_count; ++i) {
        auto const symbol = icon_catalog::toolbar_symbol_at(i);
        json::Object item;
        item.emplace("symbol", json::Value{std::string{
            icon_catalog::name(symbol)}});
        item.emplace("semantic_reference_name", json::Value{std::string{
            icon_catalog::semantic_reference_name(symbol)}});
        item.emplace(
            "normal_presentation",
            icon_presentation_debug_json(
                symbol,
                icon_catalog::SymbolPresentationRole::Toolbar,
                icon_catalog::SymbolInteractionState{false, true},
                icon_catalog::SymbolInteractionPhase::Normal));
        item.emplace(
            "pressed_presentation",
            icon_presentation_debug_json(
                symbol,
                icon_catalog::SymbolPresentationRole::Toolbar,
                icon_catalog::SymbolInteractionState{false, true},
                icon_catalog::SymbolInteractionPhase::Pressed));
        out.push_back(json::Value{std::move(item)});
    }
    return json::Value{std::move(out)};
}

inline json::Value icon_presentation_samples_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_reference_symbol_count <= 0)
        return json::Value{json::Object{}};
    json::Object out;
    out.emplace(
        "sidebar_selected_recents",
        icon_presentation_debug_json(
            icon_catalog::Symbol::Recents,
            icon_catalog::SymbolPresentationRole::Sidebar,
            icon_catalog::SymbolInteractionState{true, true},
            icon_catalog::SymbolInteractionPhase::Normal));
    out.emplace(
        "sidebar_unselected_trash",
        icon_presentation_debug_json(
            icon_catalog::Symbol::Trash,
            icon_catalog::SymbolPresentationRole::Sidebar,
            icon_catalog::SymbolInteractionState{false, true},
            icon_catalog::SymbolInteractionPhase::Normal));
    out.emplace(
        "toolbar_pressed_search",
        icon_presentation_debug_json(
            icon_catalog::Symbol::Search,
            icon_catalog::SymbolPresentationRole::Toolbar,
            icon_catalog::SymbolInteractionState{false, true},
            icon_catalog::SymbolInteractionPhase::Pressed));
    out.emplace(
        "toolbar_disabled_forward",
        icon_presentation_debug_json(
            icon_catalog::Symbol::Forward,
            icon_catalog::SymbolPresentationRole::Toolbar,
            icon_catalog::SymbolInteractionState{false, false},
            icon_catalog::SymbolInteractionPhase::Normal));
    out.emplace(
        "file_type_pdf",
        icon_presentation_debug_json(
            icon_catalog::Symbol::PdfDocument,
            icon_catalog::SymbolPresentationRole::FileType,
            icon_catalog::SymbolInteractionState{false, true},
            icon_catalog::SymbolInteractionPhase::Normal));
    return json::Value{std::move(out)};
}

inline auto interaction_tone_name(icon_catalog::SymbolPresentationRole role,
                                  bool selected,
                                  bool enabled) -> std::string {
    return std::string{icon_catalog::symbol_tone_name(
        icon_catalog::macos_interaction_tone(
            role,
            icon_catalog::SymbolInteractionState{selected, enabled}))};
}

inline json::Value icon_interaction_tones_debug_json(
        ExplorerChromeMetrics const& chrome) {
    if (chrome.icon_interaction_tone_policy == "n/a")
        return json::Value{json::Object{}};
    json::Object out;
    out.emplace(
        "sidebar_selected",
        json::Value{interaction_tone_name(
            icon_catalog::SymbolPresentationRole::Sidebar, true, true)});
    out.emplace(
        "sidebar_unselected",
        json::Value{interaction_tone_name(
            icon_catalog::SymbolPresentationRole::Sidebar, false, true)});
    out.emplace(
        "toolbar_selected",
        json::Value{interaction_tone_name(
            icon_catalog::SymbolPresentationRole::Toolbar, true, true)});
    out.emplace(
        "toolbar_unselected",
        json::Value{interaction_tone_name(
            icon_catalog::SymbolPresentationRole::Toolbar, false, true)});
    out.emplace(
        "disabled",
        json::Value{interaction_tone_name(
            icon_catalog::SymbolPresentationRole::Toolbar, false, false)});
    return json::Value{std::move(out)};
}


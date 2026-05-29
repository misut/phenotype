namespace fs = std::filesystem;

struct SelectLocation { std::string id; };
struct SelectEntry { std::string name; };
struct ExplorerInputMessage { file_explorer_demo::ExplorerInput input; };
struct SearchChanged { std::string text; };
struct DraftNameChanged { std::string text; };
struct DraftFolderNameChanged { std::string text; };
struct DraftBodyChanged { std::string text; };
struct SelectTab { std::size_t value; };
struct CreateFile {};
struct CreateFolder {};
struct DeleteSelected {};
struct DuplicateSelected {};
struct GoBack {};
struct GoForward {};
struct GoUp {};
struct CycleSort {};
struct ResetDemo {};
struct Resized { int width; int height; float scale; };

using Msg = std::variant<
    SelectLocation,
    SelectEntry,
    ExplorerInputMessage,
    SearchChanged,
    DraftNameChanged,
    DraftFolderNameChanged,
    DraftBodyChanged,
    SelectTab,
    CreateFile,
    CreateFolder,
    DeleteSelected,
    DuplicateSelected,
    GoBack,
    GoForward,
    GoUp,
    CycleSort,
    ResetDemo,
    Resized>;

std::string read_text_file(fs::path const& path) {
    auto input = std::ifstream{path, std::ios::binary};
    if (!input)
        return {};
    auto out = std::ostringstream{};
    out << input.rdbuf();
    return out.str();
}

void apply_startup_inputs(file_explorer_demo::ExplorerState& state,
                          std::string_view profile) {
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_SCENARIO")) {
        file_explorer_demo::apply_startup_scenario(state, raw);
    }
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_INPUTS")) {
        if (*raw) {
            auto parsed = file_explorer_demo::parse_explorer_input_sequence(
                raw,
                "PHENOTYPE_FILE_EXPLORER_INPUTS");
            if (parsed.ok) {
                file_explorer_demo::apply_explorer_inputs(
                    state,
                    parsed.inputs,
                    profile);
            } else {
                state.status = parsed.error;
                return;
            }
        }
    }
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_SCRIPT")) {
        if (*raw) {
            auto path = fs::path{raw};
            auto text = read_text_file(path);
            if (text.empty()) {
                state.status = "Input script is empty or unreadable: "
                    + path.string();
                return;
            }
            auto parsed = file_explorer_demo::parse_explorer_input_lines(
                text,
                path.string());
            if (parsed.ok) {
                file_explorer_demo::apply_explorer_inputs(
                    state,
                    parsed.inputs,
                    profile);
            } else {
                state.status = parsed.error;
                return;
            }
        }
    }
}

file_explorer_demo::ExplorerState initial_explorer_state() {
    auto state = file_explorer_demo::make_state("mobile");
    return state;
}

std::string initial_locale() {
    char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_LOCALE");
    return raw && *raw ? std::string{raw} : std::string{"en"};
}

bool env_truthy(char const* raw) {
    if (!raw || !*raw)
        return false;
    auto value = std::string{raw};
    std::ranges::transform(value, value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value == "1" || value == "true" || value == "yes";
}

std::optional<float> env_float(
        char const* raw,
        float minimum,
        float maximum,
        bool allow_zero = false) {
    if (!raw || !*raw)
        return std::nullopt;
    char* end = nullptr;
    float value = std::strtof(raw, &end);
    if (end == raw || (end && *end != '\0'))
        return std::nullopt;
    if (allow_zero ? !(value >= 0.0f) : !(value > 0.0f))
        return std::nullopt;
    if (value < minimum)
        value = minimum;
    if (value > maximum)
        value = maximum;
    return value;
}

phenotype::ThemePreferenceOverrides initial_theme_preference_overrides() {
    phenotype::ThemePreferenceOverrides overrides{};
    overrides.prefer_system_color_scheme = true;
    overrides.apply_system_accent_color = true;
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_FONT_FAMILY")) {
        if (std::string_view{raw} == "system") {
            overrides.prefer_system_font_family = true;
        } else if (*raw) {
            overrides.font_family = raw;
        }
    }
    if (env_truthy(std::getenv("PHENOTYPE_FILE_EXPLORER_USE_SYSTEM_FONT")))
        overrides.prefer_system_font_family = true;
    if (env_truthy(std::getenv(
            "PHENOTYPE_FILE_EXPLORER_USE_SYSTEM_FONT_METRICS"))) {
        overrides.apply_system_font_metrics = true;
    }
    if (env_truthy(std::getenv(
            "PHENOTYPE_FILE_EXPLORER_DISABLE_SYSTEM_FONT_METRICS"))) {
        overrides.apply_system_font_metrics = false;
    }
    if (env_truthy(std::getenv(
            "PHENOTYPE_FILE_EXPLORER_USE_SYSTEM_SCROLL_METRICS"))) {
        overrides.apply_system_scroll_metrics = true;
    }
    if (env_truthy(std::getenv(
            "PHENOTYPE_FILE_EXPLORER_DISABLE_SYSTEM_SCROLL_METRICS"))) {
        overrides.apply_system_scroll_metrics = false;
    }
    if (auto scale = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_FONT_SCALE"),
            0.75f,
            1.8f)) {
        overrides.font_scale = *scale;
    }
    if (auto size = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_FONT_SIZE"),
            8.0f,
            40.0f)) {
        overrides.body_font_size = *size;
    }
    if (auto size = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_BODY_FONT_SIZE"),
            8.0f,
            40.0f)) {
        overrides.body_font_size = *size;
    }
    if (auto size = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_HEADING_FONT_SIZE"),
            10.0f,
            56.0f)) {
        overrides.heading_font_size = *size;
    }
    if (auto size = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_SMALL_FONT_SIZE"),
            8.0f,
            32.0f)) {
        overrides.small_font_size = *size;
    }
    if (auto ratio = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_LINE_HEIGHT"),
            1.0f,
            2.2f)) {
        overrides.line_height_ratio = *ratio;
    }
    if (auto ratio = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_LINE_HEIGHT_RATIO"),
            1.0f,
            2.2f)) {
        overrides.line_height_ratio = *ratio;
    }
    if (auto speed = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_SCROLL_SPEED"),
            0.25f,
            4.0f)) {
        overrides.scroll_delta_multiplier = *speed;
    }
    if (auto speed = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_HORIZONTAL_SCROLL_SPEED"),
            0.25f,
            4.0f)) {
        overrides.scroll_horizontal_delta_multiplier = *speed;
    }
    if (char const* raw =
            std::getenv("PHENOTYPE_FILE_EXPLORER_SCROLL_BAR_VISIBILITY")) {
        if (*raw)
            overrides.scroll_bar_visibility =
                phenotype::normalized_scroll_bar_visibility(raw);
    }
    if (char const* raw =
            std::getenv("PHENOTYPE_FILE_EXPLORER_SCROLLBAR_VISIBILITY")) {
        if (*raw)
            overrides.scroll_bar_visibility =
                phenotype::normalized_scroll_bar_visibility(raw);
    }
    if (auto scale = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_MOTION_SCALE"),
            0.0f,
            4.0f,
            true)) {
        overrides.motion_duration_multiplier = *scale;
    }
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_COLOR_SCHEME")) {
        if (std::string_view{raw} == "system") {
            overrides.prefer_system_color_scheme = true;
        } else if (std::string_view{raw} == "dark"
                   || std::string_view{raw} == "light"
                   || std::string_view{raw} == "high-contrast-dark"
                   || std::string_view{raw} == "high-contrast-light") {
            overrides.color_scheme = raw;
        }
    }
    return overrides;
}

file_explorer_demo::SystemPreferenceSnapshot system_preference_snapshot(
        phenotype::PlatformSystemSettingsSnapshot const& system) {
    auto const contract_system =
        phenotype::theme_contract_system_snapshot(system);
    return {
        .source = system.source,
        .font_family = system.font_family,
        .font_family_source = system.font_family_source,
        .body_font_size = system.body_font_size,
        .heading_font_size = system.heading_font_size,
        .small_font_size = system.small_font_size,
        .line_height_ratio = system.line_height_ratio,
        .font_scale = system.font_scale,
        .text_size_source = system.text_size_source,
        .font_weight_adjustment = system.font_weight_adjustment,
        .font_weight_source = system.font_weight_source,
        .preferred_scroller_style = system.preferred_scroller_style,
        .overlay_scrollbars = system.overlay_scrollbars,
        .scroll_line_height = system.scroll_line_height,
        .scroll_wheel_lines = system.scroll_wheel_lines,
        .scroll_page_mode = system.scroll_page_mode,
        .scroll_vertical_factor = system.scroll_vertical_factor,
        .scroll_horizontal_factor = system.scroll_horizontal_factor,
        .scroll_bar_size = system.scroll_bar_size,
        .touch_slop = system.touch_slop,
        .scroll_friction = system.scroll_friction,
        .scroll_delta_multiplier = system.scroll_delta_multiplier,
        .scroll_horizontal_delta_multiplier =
            system.scroll_horizontal_delta_multiplier,
        .scroll_source = system.scroll_source,
        .font_family_available = contract_system.font_family_available,
        .font_metrics_available = contract_system.font_metrics_available,
        .font_scale_available = contract_system.font_scale_available,
        .line_height_available = contract_system.line_height_available,
        .scroll_metrics_available = contract_system.scroll_metrics_available,
        .color_scheme_available = contract_system.color_scheme_available,
        .reduce_motion_available = contract_system.reduce_motion_available,
        .double_click_interval_ms = system.double_click_interval_ms,
        .key_repeat_delay_ms = system.key_repeat_delay_ms,
        .key_repeat_interval_ms = system.key_repeat_interval_ms,
        .caret_blink_interval_ms = system.caret_blink_interval_ms,
        .input_timing_source = system.input_timing_source,
        .preferred_locale = system.preferred_locale,
        .preferred_locale_source = system.preferred_locale_source,
        .color_scheme = system.color_scheme,
        .color_scheme_source = system.color_scheme_source,
        .appearance_name = system.appearance_name,
        .reduce_transparency = system.reduce_transparency,
        .increase_contrast = system.increase_contrast,
        .reduce_motion = system.reduce_motion,
        .accessibility_source = system.accessibility_source,
        .accent_color_available = system.accent_color_available,
        .accent_color = {
            static_cast<int>(system.accent_color.r),
            static_cast<int>(system.accent_color.g),
            static_cast<int>(system.accent_color.b),
            static_cast<int>(system.accent_color.a),
        },
        .accent_color_source = system.accent_color_source,
        .accent_color_opaque = system.accent_color_opaque,
    };
}

file_explorer_demo::ThemePreferenceSnapshot theme_preference_snapshot(
        phenotype::ThemePreferenceOverrides const& overrides) {
    return {
        .font_family = overrides.font_family,
        .color_scheme = overrides.color_scheme,
        .font_scale = overrides.font_scale,
        .body_font_size = overrides.body_font_size,
        .heading_font_size = overrides.heading_font_size,
        .small_font_size = overrides.small_font_size,
        .line_height_ratio = overrides.line_height_ratio,
        .scroll_delta_multiplier = overrides.scroll_delta_multiplier,
        .scroll_horizontal_delta_multiplier =
            overrides.scroll_horizontal_delta_multiplier,
        .scroll_bar_visibility = overrides.scroll_bar_visibility.empty()
            ? std::string{}
            : phenotype::normalized_scroll_bar_visibility(
                overrides.scroll_bar_visibility),
        .prefer_system_font_family = overrides.prefer_system_font_family,
        .prefer_system_color_scheme = overrides.prefer_system_color_scheme,
        .apply_system_font_metrics = overrides.apply_system_font_metrics,
        .apply_system_font_scale = overrides.apply_system_font_scale,
        .apply_system_scroll_metrics =
            overrides.apply_system_scroll_metrics,
        .apply_system_accent_color = overrides.apply_system_accent_color,
        .apply_system_reduce_motion = overrides.apply_system_reduce_motion,
        .motion_duration_multiplier = overrides.motion_duration_multiplier,
    };
}

file_explorer_demo::RuntimePreferenceState runtime_preference_state(
        phenotype::ResolvedThemePreferences const& resolved,
        phenotype::PlatformSystemSettingsSnapshot system_settings,
        phenotype::ThemePreferenceOverrides theme_preferences) {
    file_explorer_demo::RuntimePreferenceState state{};
    state.source = resolved.source;
    state.system_settings = system_preference_snapshot(system_settings);
    state.theme_preferences = theme_preference_snapshot(theme_preferences);
    state.effective_font_family = resolved.effective_font_family;
    state.effective_color_scheme = resolved.effective_color_scheme;
    state.effective_body_font_size = resolved.effective_body_font_size;
    state.effective_heading_font_size = resolved.effective_heading_font_size;
    state.effective_small_font_size = resolved.effective_small_font_size;
    state.effective_line_height_ratio = resolved.effective_line_height_ratio;
    state.effective_scroll_delta_multiplier =
        resolved.effective_scroll_delta_multiplier;
    state.effective_scroll_horizontal_delta_multiplier =
        resolved.effective_scroll_horizontal_delta_multiplier;
    state.effective_scroll_bar_visibility =
        resolved.effective_scroll_bar_visibility;
    state.effective_motion_duration_multiplier =
        resolved.effective_motion_duration_multiplier;
    state.used_system_font_family = resolved.used_system_font_family;
    state.used_system_color_scheme = resolved.used_system_color_scheme;
    state.used_system_font_metrics = resolved.used_system_font_metrics;
    state.used_system_font_scale = resolved.used_system_font_scale;
    state.used_user_font_scale = resolved.used_user_font_scale;
    state.used_user_font_size = resolved.used_user_font_size;
    state.used_system_line_height = resolved.used_system_line_height;
    state.used_user_line_height = resolved.used_user_line_height;
    state.used_system_scroll_metrics = resolved.used_system_scroll_metrics;
    state.used_user_scroll_scale = resolved.used_user_scroll_scale;
    state.used_user_scroll_bar_visibility =
        resolved.used_user_scroll_bar_visibility;
    state.used_system_accent_color = resolved.used_system_accent_color;
    state.used_system_reduce_motion = resolved.used_system_reduce_motion;
    state.used_user_motion_scale = resolved.used_user_motion_scale;
    return state;
}

fs::path initial_package_root() {
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_PACKAGE_ROOT")) {
        if (*raw)
            return fs::path{raw};
    }
    if (char const* raw = std::getenv("PHENOTYPE_PACKAGE_ROOT")) {
        if (*raw)
            return fs::path{raw};
    }
    std::error_code ec;
    auto current = fs::current_path(ec);
    return ec ? fs::path{} : current;
}

phenotype::ResourceCatalog runtime_resource_catalog() {
    auto fallback = file_explorer_demo::file_explorer_resource_catalog("mobile");
    auto root = initial_package_root();
    if (root.empty())
        return fallback;
    auto manifest_text = read_text_file(root / "phenotype.package.toml");
    if (manifest_text.empty())
        return fallback;

    auto parsed = phenotype::parse_resource_manifest(manifest_text);
    auto locale_texts = std::vector<file_explorer_demo::PackageResourceText>{};
    locale_texts.reserve(parsed.catalog.locales.size());
    for (auto const& locale : parsed.catalog.locales) {
        if (locale.source.empty())
            continue;
        locale_texts.push_back({
            .source = locale.source,
            .text = read_text_file(root / locale.source),
        });
    }
    return file_explorer_demo::file_explorer_resource_catalog_from_package_texts(
        "mobile",
        manifest_text,
        locale_texts);
}

file_explorer_demo::RuntimePreferenceState g_runtime_preferences;
phenotype::Theme g_base_theme;
phenotype::PlatformSystemSettingsSnapshot g_system_settings;

phenotype::PlatformSystemSettingsSnapshot capture_system_settings() {
    return phenotype::native::system_settings();
}

void refresh_system_settings_from_platform() {
    g_system_settings = capture_system_settings();
}

std::string resolved_initial_locale() {
    auto const& catalog = runtime_resource_catalog();
    char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_LOCALE");
    if (raw && *raw)
        return file_explorer_demo::resolve_supported_locale(catalog, raw);
    return file_explorer_demo::resolve_supported_locale(
        catalog,
        g_system_settings.preferred_locale);
}

phenotype::ThemePreferenceOverrides theme_preferences_from_state(
        file_explorer_demo::ThemePreferenceSnapshot const& preferences) {
    return phenotype::ThemePreferenceOverrides{
        .font_family = preferences.font_family,
        .color_scheme = preferences.color_scheme,
        .font_scale = preferences.font_scale,
        .body_font_size = preferences.body_font_size,
        .heading_font_size = preferences.heading_font_size,
        .small_font_size = preferences.small_font_size,
        .line_height_ratio = preferences.line_height_ratio,
        .scroll_delta_multiplier = preferences.scroll_delta_multiplier,
        .scroll_horizontal_delta_multiplier =
            preferences.scroll_horizontal_delta_multiplier,
        .scroll_bar_visibility = preferences.scroll_bar_visibility.empty()
            ? std::string{}
            : phenotype::normalized_scroll_bar_visibility(
                preferences.scroll_bar_visibility),
        .prefer_system_font_family = preferences.prefer_system_font_family,
        .prefer_system_color_scheme = preferences.prefer_system_color_scheme,
        .apply_system_font_metrics = preferences.apply_system_font_metrics,
        .apply_system_font_scale = preferences.apply_system_font_scale,
        .apply_system_scroll_metrics =
            preferences.apply_system_scroll_metrics,
        .apply_system_accent_color = preferences.apply_system_accent_color,
        .apply_system_reduce_motion = preferences.apply_system_reduce_motion,
        .motion_duration_multiplier = preferences.motion_duration_multiplier,
    };
}

void sync_runtime_theme(file_explorer_demo::ExplorerState& explorer) {
    refresh_system_settings_from_platform();
    auto overrides = theme_preferences_from_state(explorer.theme_preferences);
    auto resolved = phenotype::resolve_system_theme_preferences(
        g_base_theme,
        g_system_settings,
        overrides,
        "native-system-settings+environment-overrides");
    auto runtime = runtime_preference_state(
        resolved,
        g_system_settings,
        overrides);
    if (explorer.preferences_source == "application-input")
        runtime.source = explorer.preferences_source;
    g_runtime_preferences = runtime;
    file_explorer_demo::apply_runtime_preferences(explorer, runtime);
    phenotype::set_theme(resolved.theme);
}


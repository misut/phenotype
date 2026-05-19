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

import file_explorer_shared;
import phenotype;
import phenotype.native;
import phenotype.svg;

namespace {

namespace fs = std::filesystem;

struct SelectLocation { std::string id; };
struct SelectEntry { std::string name; };
struct ExplorerInputMessage { file_explorer_demo::ExplorerInput input; };
struct SetViewMode { file_explorer_demo::ExplorerViewMode mode; };
struct ToolbarAction { std::string label; };
struct SearchChanged { std::string text; };
struct ToggleSearch {};
struct ShowSearch {};
struct DismissTransient {};
struct MoveSelection { file_explorer_demo::ExplorerSelectionMove move; };
struct CreateFile {};
struct CreateFolder {};
struct DeleteSelected {};
struct DuplicateSelected {};
struct ActivateSelected {};
struct ToggleMoreActions {};
struct GoBack {};
struct GoForward {};
struct GoUp {};
struct CycleSort {};
struct Refresh {};
struct ResetDemo {};
struct Resized { int width; int height; float scale; };
struct Noop {};

using Msg = std::variant<
    SelectLocation,
    SelectEntry,
    ExplorerInputMessage,
    SetViewMode,
    ToolbarAction,
    SearchChanged,
    ToggleSearch,
    ShowSearch,
    DismissTransient,
    MoveSelection,
    CreateFile,
    CreateFolder,
    DeleteSelected,
    DuplicateSelected,
    ActivateSelected,
    ToggleMoreActions,
    GoBack,
    GoForward,
    GoUp,
    CycleSort,
    Refresh,
    ResetDemo,
    Resized,
    Noop>;

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
    auto state = file_explorer_demo::make_state("desktop");
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_VIEW")) {
        if (*raw) {
            state.view_mode = file_explorer_demo::known_view_mode_name(raw)
                ? file_explorer_demo::view_mode_from_name(raw)
                : file_explorer_demo::ExplorerViewMode::Icon;
        }
    }
    return state;
}

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
    return value == "1" || value == "true" || value == "yes"
        || value == "open";
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
    state.used_system_accent_color = resolved.used_system_accent_color;
    state.used_system_reduce_motion = resolved.used_system_reduce_motion;
    state.used_user_motion_scale = resolved.used_user_motion_scale;
    return state;
}

bool initial_more_actions_open() {
    return env_truthy(std::getenv("PHENOTYPE_FILE_EXPLORER_MORE_ACTIONS"));
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
    auto fallback = file_explorer_demo::file_explorer_resource_catalog("desktop");
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
        "desktop",
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

struct SvgPreviewDocumentCacheEntry {
    std::string source;
    phenotype::svg::Document document;
};

struct SvgPreviewDocumentCache {
    std::vector<SvgPreviewDocumentCacheEntry> entries;
};

phenotype::svg::Document const* cached_svg_preview_document(
        SvgPreviewDocumentCache& cache,
        std::string_view source) {
    if (source.empty())
        return nullptr;
    for (auto const& entry : cache.entries) {
        if (entry.source == source)
            return &entry.document;
    }
    if (cache.entries.size()
        >= static_cast<std::size_t>(
            file_explorer_demo::k_desktop_thumbnail_svg_document_cache_limit)) {
        return nullptr;
    }
    auto& entry = cache.entries.emplace_back();
    entry.source = std::string{source};
    entry.document = phenotype::svg::parse(source);
    return &entry.document;
}

struct State {
    phenotype::icons::SymbolDocumentCache icon_cache =
        phenotype::icons::make_symbol_document_cache();
    mutable SvgPreviewDocumentCache svg_preview_cache;
    file_explorer_demo::ExplorerState explorer;
    file_explorer_demo::ExplorerLabels labels;
    bool search_visible = false;
    bool more_actions_open = false;

    State()
        : explorer(initial_explorer_state()),
          labels(file_explorer_demo::file_explorer_labels(
              resolved_initial_locale(),
              runtime_resource_catalog())),
          search_visible(!explorer.search.empty()),
          more_actions_open(initial_more_actions_open()) {
        file_explorer_demo::apply_runtime_preferences(
            explorer,
            g_runtime_preferences);
        apply_startup_inputs(explorer, "desktop");
        search_visible = !explorer.search.empty();
        sync_runtime_theme(explorer);
        svg_preview_cache.entries.reserve(
            static_cast<std::size_t>(
                file_explorer_demo::k_desktop_thumbnail_svg_document_cache_limit));
    }
};

State const* g_debug_state = nullptr;

auto file_explorer_application_debug_payload() {
    if (!g_debug_state) {
        file_explorer_demo::ExplorerState empty{};
        auto snap = file_explorer_demo::snapshot(empty);
        auto chrome = file_explorer_demo::explorer_chrome_metrics(empty, "desktop");
        return file_explorer_demo::file_explorer_application_debug_json(
            empty,
            snap,
            chrome,
            "desktop");
    }
    auto snap = file_explorer_demo::snapshot(g_debug_state->explorer);
    auto chrome = file_explorer_demo::explorer_chrome_metrics(
        g_debug_state->explorer,
        "desktop");
    chrome =
        file_explorer_demo::explorer_chrome_with_native_window_control_ownership(
            std::move(chrome));
    chrome.more_actions_open = g_debug_state->more_actions_open;
    chrome.overflow_action_button_count = g_debug_state->more_actions_open
        ? 10
        : 0;
    return file_explorer_demo::file_explorer_application_debug_json(
        g_debug_state->explorer,
        snap,
        chrome,
        "desktop");
}

constexpr float k_pi = 3.14159265358979323846f;
constexpr float k_integrated_titlebar_height =
    file_explorer_demo::k_desktop_integrated_titlebar_height;
constexpr float k_sidebar_width = file_explorer_demo::k_desktop_sidebar_width;
constexpr float k_sidebar_row_width = file_explorer_demo::k_desktop_sidebar_row_width;
constexpr float k_sidebar_row_height =
    file_explorer_demo::k_desktop_sidebar_row_height;
constexpr float k_sidebar_heading_height =
    file_explorer_demo::k_desktop_sidebar_heading_height;
constexpr float k_sidebar_icon_size =
    file_explorer_demo::k_desktop_sidebar_icon_size;
constexpr float k_sidebar_icon_leading =
    file_explorer_demo::k_desktop_sidebar_icon_leading;
constexpr float k_sidebar_label_leading =
    file_explorer_demo::k_desktop_sidebar_label_leading;
constexpr float k_sidebar_label_top =
    file_explorer_demo::k_desktop_sidebar_label_top;
constexpr float k_sidebar_heading_label_leading =
    file_explorer_demo::k_desktop_sidebar_heading_label_leading;
constexpr float k_sidebar_heading_label_top =
    file_explorer_demo::k_desktop_sidebar_heading_label_top;
constexpr float k_sidebar_section_gap =
    file_explorer_demo::k_desktop_sidebar_section_gap;
constexpr float k_sidebar_selected_row_radius =
    file_explorer_demo::k_desktop_sidebar_selected_row_radius;
constexpr float k_column_location_row_height =
    file_explorer_demo::k_desktop_column_location_row_height;
constexpr float k_column_location_icon_size =
    file_explorer_demo::k_desktop_column_location_icon_size;
constexpr float k_content_radius = 0.0f;
constexpr float k_window_radius = file_explorer_demo::k_desktop_window_radius;
constexpr float k_toolbar_group_radius =
    file_explorer_demo::k_desktop_toolbar_group_radius;
constexpr float k_toolbar_group_height =
    file_explorer_demo::k_desktop_toolbar_group_height;
constexpr float k_toolbar_icon_button_width =
    file_explorer_demo::k_desktop_toolbar_icon_button_width;
constexpr float k_toolbar_icon_button_height =
    file_explorer_demo::k_desktop_toolbar_icon_button_height;
constexpr float k_native_window_control_reserve_height =
    file_explorer_demo::k_desktop_titlebar_control_cluster_height;
constexpr float k_titlebar_drag_region_height =
    file_explorer_demo::k_desktop_titlebar_drag_region_height;
constexpr float k_leading_control_reserved_width =
    file_explorer_demo::k_desktop_leading_control_reserved_width;
constexpr float k_trailing_control_reserved_width =
    file_explorer_demo::k_desktop_trailing_control_reserved_width;

phenotype::Color rgba(int r, int g, int b, int a = 255) {
    return phenotype::Color{
        static_cast<unsigned char>(r),
        static_cast<unsigned char>(g),
        static_cast<unsigned char>(b),
        static_cast<unsigned char>(a),
    };
}

phenotype::FontSpec finder_font(
        phenotype::FontWeight weight = phenotype::FontWeight::Regular) {
    auto const& theme = phenotype::current_theme();
    return phenotype::FontSpec{
        .family = theme.default_font_family,
        .weight = weight,
    };
}

phenotype::layout::MaterialSurfaceOptions toolbar_shell_options() {
    using namespace phenotype;
    return layout::glass_surface_options(
        layout::GlassSurfacePreset::Toolbar,
        "Toolbar");
}

phenotype::layout::MaterialSurfaceOptions toolbar_group_options(
        char const* label,
        float max_width) {
    using namespace phenotype;
    auto options = layout::glass_surface_options(
        layout::GlassSurfacePreset::ToolbarGroup,
        label);
    options.max_width = max_width;
    options.fixed_height = k_toolbar_group_height;
    options.border_radius = k_toolbar_group_radius;
    return options;
}

phenotype::layout::MaterialSurfaceOptions content_surface_options(
        phenotype::SpaceToken gap = phenotype::SpaceToken::Md) {
    using namespace phenotype;
    auto options = layout::glass_surface_options(
        layout::GlassSurfacePreset::Content,
        "Files");
    options.gap = gap;
    options.border_radius = k_content_radius;
    return options;
}

phenotype::PathBuilder rounded_rect_path(
        float x, float y, float w, float h, float r) {
    using phenotype::PathBuilder;
    if (r < 0.0f)
        r = 0.0f;
    float const max_r = (w < h ? w : h) * 0.5f;
    if (r > max_r)
        r = max_r;

    PathBuilder path;
    path.move_to(x + r, y);
    path.line_to(x + w - r, y);
    path.arc_to(x + w - r, y + r, r, -0.5f * k_pi, 0.0f);
    path.line_to(x + w, y + h - r);
    path.arc_to(x + w - r, y + h - r, r, 0.0f, 0.5f * k_pi);
    path.line_to(x + r, y + h);
    path.arc_to(x + r, y + h - r, r, 0.5f * k_pi, k_pi);
    path.line_to(x, y + r);
    path.arc_to(x + r, y + r, r, k_pi, 1.5f * k_pi);
    path.close();
    return path;
}

void fill_rect(phenotype::Painter& painter,
               float x, float y, float w, float h,
               phenotype::Color color) {
    phenotype::PaintRect rect{x, y, w, h, color};
    painter.fill_rects(&rect, 1);
}

void fill_round(phenotype::Painter& painter,
                float x, float y, float w, float h, float r,
                phenotype::Color color) {
    auto path = rounded_rect_path(x, y, w, h, r);
    painter.fill_path(path, color);
}

void stroke_round(phenotype::Painter& painter,
                  float x, float y, float w, float h, float r,
                  float thickness,
                  phenotype::Color color) {
    if (w <= 0.0f || h <= 0.0f || thickness <= 0.0f)
        return;
    float max_r = std::min(w, h) * 0.5f;
    if (r < 0.0f)
        r = 0.0f;
    if (r > max_r)
        r = max_r;
    if (r <= 0.0f) {
        painter.line(x, y, x + w, y, thickness, color);
        painter.line(x + w, y, x + w, y + h, thickness, color);
        painter.line(x + w, y + h, x, y + h, thickness, color);
        painter.line(x, y + h, x, y, thickness, color);
        return;
    }

    painter.line(x + r, y, x + w - r, y, thickness, color);
    painter.arc(x + w - r, y + r, r,
                -0.5f * k_pi, 0.0f, thickness, color);
    painter.line(x + w, y + r, x + w, y + h - r, thickness, color);
    painter.arc(x + w - r, y + h - r, r,
                0.0f, 0.5f * k_pi, thickness, color);
    painter.line(x + w - r, y + h, x + r, y + h, thickness, color);
    painter.arc(x + r, y + h - r, r,
                0.5f * k_pi, k_pi, thickness, color);
    painter.line(x, y + h - r, x, y + r, thickness, color);
    painter.arc(x + r, y + r, r,
                k_pi, 1.5f * k_pi, thickness, color);
}

phenotype::icons::Symbol sidebar_symbol(std::string_view id) {
    return phenotype::icons::from_catalog_symbol(
        file_explorer_demo::sidebar_symbol_for_token(id));
}

void paint_finder_symbol_centered(phenotype::Painter& painter,
                                  phenotype::icons::SymbolDocumentCache const& cache,
                                  phenotype::icons::Symbol symbol,
                                  float box_x,
                                  float box_y,
                                  float box_width,
                                  float box_height,
                                  float size,
                                  phenotype::Color color) {
    phenotype::icons::paint_symbol_centered(
        painter,
        cache,
        symbol,
        box_x,
        box_y,
        box_width,
        box_height,
        size,
        color);
}

void paint_finder_symbol_centered(
        phenotype::Painter& painter,
        phenotype::icons::SymbolDocumentCache const& cache,
        phenotype::icons::SymbolPresentation presentation,
        float box_x,
        float box_y,
        float box_width,
        float box_height) {
    phenotype::icons::paint_symbol_centered(
        painter,
        cache,
        presentation,
        box_x,
        box_y,
        box_width,
        box_height);
}

phenotype::icons::SymbolPresentation icon_presentation_for_state(
        phenotype::icons::Symbol symbol,
        phenotype::icons::SymbolPresentationRole role,
        bool selected,
        phenotype::ButtonVisualState state) {
    return phenotype::icons::macos_presentation(
        symbol,
        role,
        selected,
        state);
}

void paint_sidebar_icon(phenotype::Painter& painter,
                        phenotype::icons::SymbolDocumentCache const& cache,
                        std::string_view id,
                        bool selected,
                        phenotype::ButtonVisualState state,
                        float origin_x,
                        float origin_y) {
    paint_finder_symbol_centered(
        painter,
        cache,
        icon_presentation_for_state(
            sidebar_symbol(id),
            phenotype::icons::SymbolPresentationRole::Sidebar,
            selected,
            state),
        origin_x,
        origin_y,
        k_sidebar_icon_size,
        k_sidebar_icon_size);
}

std::string extension_lower(std::string const& name) {
    auto pos = name.rfind('.');
    if (pos == std::string::npos)
        return {};
    auto ext = name.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
}

bool svg_image_extension(std::string_view ext) {
    return ext == "svg" || ext == "svgz";
}

bool image_extension(std::string_view ext) {
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "heic"
        || ext == "heif" || ext == "webp" || svg_image_extension(ext);
}

bool movie_extension(std::string_view ext) {
    return ext == "mov" || ext == "mp4" || ext == "m4v";
}

std::uint64_t stable_token(std::string_view value) {
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char ch : value) {
        h ^= ch;
        h *= 1099511628211ull;
    }
    return h ? h : 1ull;
}

void paint_document_fold(phenotype::Painter& painter,
                         float x,
                         float y,
                         float w,
                         phenotype::Color border) {
    phenotype::PathBuilder fold;
    fold.move_to(x + w - 12.0f, y + 1.0f);
    fold.line_to(x + w - 1.0f, y + 12.0f);
    fold.line_to(x + w - 12.0f, y + 12.0f);
    fold.close();
    painter.fill_path(fold, rgba(238, 241, 245, 245));
    painter.line(x + w - 12.0f, y + 1.0f,
                 x + w - 1.0f, y + 12.0f,
                 1.0f, border);
    painter.line(x + w - 12.0f, y + 12.0f,
                 x + w - 1.0f, y + 12.0f,
                 1.0f, rgba(225, 229, 235));
}

void paint_pdf_thumbnail(phenotype::Painter& painter,
                         std::string const& name,
                         bool selected) {
    auto accent = selected ? rgba(0, 122, 255) : rgba(38, 132, 255);
    auto border = selected ? rgba(0, 122, 255) : rgba(210, 214, 220);
    float const page_w = file_explorer_demo::k_desktop_thumbnail_pdf_page_width;
    float const page_h = file_explorer_demo::k_desktop_thumbnail_pdf_page_height;
    float const page_x =
        (file_explorer_demo::k_desktop_icon_grid_thumbnail_width - page_w) * 0.5f;
    float const page_y = 3.0f;
    fill_round(painter,
               page_x + 2.0f,
               page_y + 3.0f,
               page_w,
               page_h,
               file_explorer_demo::k_desktop_thumbnail_pdf_page_radius,
               rgba(0, 0, 0, 24));
    fill_round(painter,
               page_x,
               page_y,
               page_w,
               page_h,
               file_explorer_demo::k_desktop_thumbnail_pdf_page_radius,
               rgba(255, 255, 255, 250));
    stroke_round(painter,
                 page_x,
                 page_y,
                 page_w,
                 page_h,
                 file_explorer_demo::k_desktop_thumbnail_pdf_page_radius,
                 1.0f,
                 border);
    paint_document_fold(painter, page_x, page_y, page_w, border);
    fill_rect(painter, page_x + 4.0f, page_y + 5.0f, 27.0f, 3.5f, accent);
    fill_rect(painter, page_x + 5.0f, page_y + 13.0f, 38.0f, 1.6f,
              rgba(196, 204, 215));
    fill_rect(painter, page_x + 5.0f, page_y + 17.0f, 18.0f, 1.4f,
              rgba(228, 118, 118, 170));
    fill_rect(painter, page_x + 26.0f, page_y + 17.0f, 15.0f, 1.4f,
              rgba(196, 204, 215));
    for (int row = 0;
         row < file_explorer_demo::k_desktop_thumbnail_pdf_detail_line_count / 2;
         ++row) {
        float y = page_y + 23.0f + static_cast<float>(row) * 5.0f;
        fill_rect(painter, page_x + 5.0f, y, 16.0f, 1.2f,
                  rgba(202, 210, 221));
        fill_rect(painter, page_x + 24.0f, y, 19.0f, 1.2f,
                  rgba(221, 226, 234));
        if (row % 2 == 0) {
            fill_rect(painter, page_x + 5.0f, y + 2.2f, 38.0f, 1.0f,
                      rgba(232, 235, 240));
        }
    }
    fill_rect(painter, page_x + 5.0f, page_y + 52.0f, 20.0f, 6.0f,
              rgba(255, 231, 125, 210));
    fill_rect(painter, page_x + 28.0f, page_y + 52.0f, 14.0f, 6.0f,
              rgba(238, 243, 251));
    auto tag = extension_lower(name);
    painter.text(page_x + 8.0f, page_y + 55.0f, tag.c_str(),
                 static_cast<unsigned int>(tag.size()),
                 9.0f, rgba(80, 87, 96), finder_font());
}

void paint_svg_preview_fallback(phenotype::Painter& painter,
                                float x,
                                float y) {
    phenotype::PathBuilder curve;
    curve.move_to(x + 14.0f, y + 25.0f);
    curve.cubic_to(
        x + 27.0f, y + 9.0f,
        x + 57.0f, y + 9.0f,
        x + 74.0f, y + 25.0f);
    painter.stroke_path(curve, 2.2f, rgba(0, 122, 255, 210));
    fill_round(painter, x + 10.0f, y + 12.0f, 6.0f, 6.0f, 3.0f,
               rgba(0, 122, 255, 160));
    fill_round(painter, x + 70.0f, y + 12.0f, 6.0f, 6.0f, 3.0f,
               rgba(0, 122, 255, 160));
}

void paint_svg_file_preview(phenotype::Painter& painter,
                            SvgPreviewDocumentCache& cache,
                            std::string_view source,
                            float x,
                            float y,
                            float w,
                            float h,
                            bool selected) {
    auto local_doc = phenotype::svg::Document{};
    auto const* doc = cached_svg_preview_document(cache, source);
    if (!doc) {
        local_doc = phenotype::svg::parse(source);
        doc = &local_doc;
    }
    if (!doc->empty()) {
        auto const current_color = selected
            ? rgba(0, 122, 255)
            : rgba(68, 76, 88);
        phenotype::svg::paint(
            painter,
            *doc,
            x + 10.0f,
            y + 7.0f,
            w - 20.0f,
            h - 14.0f,
            phenotype::svg::RenderOptions{current_color, true});
    } else {
        paint_svg_preview_fallback(painter, x, y);
    }
    fill_round(painter, x + w - 29.0f, y + h - 15.0f, 24.0f, 11.0f, 5.5f,
               selected ? rgba(0, 122, 255, 220) : rgba(238, 243, 251, 235));
    painter.text(x + w - 25.0f, y + h - 7.0f, "SVG", 3, 7.5f,
                 selected ? rgba(255, 255, 255) : rgba(75, 82, 92),
                 finder_font());
}

void paint_image_thumbnail(phenotype::Painter& painter,
                           SvgPreviewDocumentCache& cache,
                           bool selected,
                           bool svg_file,
                           std::string_view svg_source) {
    auto border = selected ? rgba(0, 122, 255) : rgba(224, 228, 234);
    float const w = file_explorer_demo::k_desktop_thumbnail_media_preview_width;
    float const h = file_explorer_demo::k_desktop_thumbnail_media_preview_height;
    float const x =
        (file_explorer_demo::k_desktop_icon_grid_thumbnail_width - w) * 0.5f;
    float const y = 19.0f;
    float const radius =
        file_explorer_demo::k_desktop_thumbnail_media_preview_radius;
    fill_round(painter, x + 2.0f, y + 2.0f, w, h, radius, rgba(0, 0, 0, 22));
    fill_round(painter, x, y, w, h, radius, rgba(255, 255, 255, 248));
    painter.linear_gradient_rect(
        x + 3.0f, y + 7.0f, w - 6.0f, h - 10.0f,
        rgba(247, 249, 252),
        rgba(225, 230, 238),
        phenotype::GradientAxis::Horizontal,
        12);
    stroke_round(painter, x, y, w, h, radius, 1.0f, border);
    fill_rect(painter, x + 3.0f, y + 4.0f, w - 6.0f, 2.0f,
              rgba(235, 239, 245));
    if (svg_file) {
        paint_svg_file_preview(painter, cache, svg_source, x, y, w, h, selected);
        return;
    }
    fill_round(painter, x + 10.0f, y + 13.0f, 26.0f, 10.0f, 4.0f,
               rgba(210, 215, 224));
    fill_round(painter, x + 40.0f, y + 13.0f, 18.0f, 10.0f, 4.0f,
               rgba(184, 191, 202));
    fill_round(painter, x + 61.0f, y + 13.0f, 16.0f, 10.0f, 4.0f,
               rgba(232, 235, 240));
    fill_rect(painter, x + 9.0f, y + 27.0f, 30.0f, 2.0f,
              rgba(204, 211, 221));
    fill_rect(painter, x + 44.0f, y + 27.0f, 33.0f, 2.0f,
              rgba(222, 227, 234));
}

void paint_video_thumbnail(phenotype::Painter& painter,
                           bool selected) {
    auto border = selected ? rgba(0, 122, 255) : rgba(204, 211, 221);
    float const w = file_explorer_demo::k_desktop_thumbnail_media_preview_width;
    float const h = file_explorer_demo::k_desktop_thumbnail_media_preview_height;
    float const x =
        (file_explorer_demo::k_desktop_icon_grid_thumbnail_width - w) * 0.5f;
    float const y = 18.0f;
    float const radius =
        file_explorer_demo::k_desktop_thumbnail_media_preview_radius;
    fill_round(painter, x + 2.0f, y + 2.0f, w, h, radius, rgba(0, 0, 0, 30));
    fill_round(painter, x, y, w, h, radius, rgba(246, 248, 251));
    stroke_round(painter, x, y, w, h, radius, 1.0f, border);
    painter.linear_gradient_rect(
        x + 2.0f, y + 3.0f, w - 4.0f, 8.0f,
        rgba(42, 123, 222, 185),
        rgba(82, 96, 210, 160),
        phenotype::GradientAxis::Horizontal,
        10);
    for (int i = 0;
         i < file_explorer_demo::k_desktop_thumbnail_video_strip_count;
         ++i) {
        float const strip_x = x + 8.0f + static_cast<float>(i) * 12.0f;
        fill_rect(painter, strip_x, y + 17.0f, 8.0f, 13.0f,
                  (i % 2 == 0) ? rgba(212, 219, 228) : rgba(235, 239, 244));
    }
    fill_rect(painter, x + 7.0f, y + 32.0f, w - 14.0f, 2.0f,
              rgba(198, 207, 219));
}

void paint_text_thumbnail(phenotype::Painter& painter,
                          bool selected) {
    auto border = selected ? rgba(0, 122, 255) : rgba(220, 224, 231);
    fill_round(painter, 41.0f, 5.0f, 44.0f, 61.0f, 4.0f, rgba(0, 0, 0, 22));
    fill_round(painter, 39.0f, 3.0f, 44.0f, 61.0f, 4.0f, rgba(255, 255, 255, 250));
    stroke_round(painter, 39.0f, 3.0f, 44.0f, 61.0f, 4.0f, 1.0f, border);
    paint_document_fold(painter, 39.0f, 3.0f, 44.0f, border);
    for (int i = 0; i < 8; ++i) {
        fill_rect(painter, 44.0f, 14.0f + static_cast<float>(i) * 5.6f,
                  (i % 3 == 0) ? 25.0f : 33.0f,
                  2.0f,
                  rgba(204, 211, 221));
    }
}

void paint_folder_thumbnail(phenotype::Painter& painter,
                            bool selected) {
    auto border = selected ? rgba(0, 122, 255) : rgba(223, 163, 41);
    fill_round(painter, 22.0f, 25.0f, 86.0f, 46.0f, 8.0f, rgba(0, 0, 0, 20));
    fill_round(painter, 20.0f, 23.0f, 86.0f, 46.0f, 8.0f, rgba(255, 202, 85));
    fill_round(painter, 24.0f, 17.0f, 36.0f, 16.0f, 5.0f, rgba(255, 214, 118));
    stroke_round(painter, 20.0f, 23.0f, 86.0f, 46.0f, 8.0f, 1.0f, border);
}

void paint_item_thumbnail(phenotype::Painter& painter,
                          SvgPreviewDocumentCache& cache,
                          file_explorer_demo::Entry const& entry,
                          bool selected,
                          std::string_view preview_source = {}) {
    if (entry.folder) {
        paint_folder_thumbnail(painter, selected);
        return;
    }
    auto ext = extension_lower(entry.name);
    if (ext == "pdf") {
        paint_pdf_thumbnail(painter, entry.name, selected);
    } else if (image_extension(ext)) {
        paint_image_thumbnail(
            painter,
            cache,
            selected,
            svg_image_extension(ext),
            preview_source);
    } else if (movie_extension(ext)) {
        paint_video_thumbnail(painter, selected);
    } else {
        paint_text_thumbnail(painter, selected);
    }
}

std::uint64_t thumbnail_paint_token(file_explorer_demo::Entry const& entry,
                                    bool selected,
                                    std::string_view preview_source = {},
                                    std::uint64_t salt = 0) {
    auto token = stable_token(entry.name) ^ salt;
    if (svg_image_extension(extension_lower(entry.name)))
        token ^= stable_token(preview_source) << 1u;
    if (selected)
        token ^= 0x100000u;
    return token;
}

phenotype::Color entry_symbol_color(
        file_explorer_demo::Entry const& entry,
        bool selected) {
    if (selected)
        return rgba(255, 255, 255);
    return phenotype::icons::macos_file_type_color(
        phenotype::icons::from_catalog_symbol(
            file_explorer_demo::entry_symbol(entry)));
}

void paint_entry_symbol(phenotype::Painter& painter,
                        phenotype::icons::SymbolDocumentCache const& cache,
                        file_explorer_demo::Entry const& entry,
                        bool selected,
                        float x,
                        float y,
                        float box_size,
                        float icon_size) {
    paint_finder_symbol_centered(
        painter,
        cache,
        phenotype::icons::from_catalog_symbol(
            file_explorer_demo::entry_symbol(entry)),
        x,
        y,
        box_size,
        box_size,
        icon_size,
        entry_symbol_color(entry, selected));
}

std::string finder_status(file_explorer_demo::Snapshot const& snap) {
    std::string text = snap.item_summary;
    if (snap.has_selection)
        text += " - selected " + snap.selected.name;
    if (!snap.sort_label.empty())
        text += " - " + snap.sort_label;
    return text;
}

std::string compact_preview(std::string text) {
    for (char& ch : text) {
        if (ch == '\n' || ch == '\t')
            ch = ' ';
    }
    text.erase(std::unique(text.begin(), text.end(), [](char a, char b) {
        return a == ' ' && b == ' ';
    }), text.end());
    if (text.size() > 96)
        text = text.substr(0, 93) + "...";
    return text;
}

void pop_utf8_codepoint(std::string& text);

void paint_elided_finder_text(phenotype::Painter& painter,
                              float x,
                              float y,
                              std::string_view text,
                              float max_width,
                              float font_size,
                              phenotype::Color color) {
    if (max_width <= 0.0f || text.empty())
        return;

    auto const font = finder_font();
    auto fits = [&](std::string_view value) {
        return painter.measure_text(
            value.data(),
            static_cast<unsigned int>(value.size()),
            font_size,
            font) <= max_width;
    };
    if (fits(text)) {
        painter.text(x,
                     y,
                     text.data(),
                     static_cast<unsigned int>(text.size()),
                     font_size,
                     color,
                     font);
        return;
    }

    std::string shortened{text};
    while (!shortened.empty()) {
        pop_utf8_codepoint(shortened);
        auto candidate = shortened + "...";
        if (fits(candidate)) {
            painter.text(x,
                         y,
                         candidate.c_str(),
                         static_cast<unsigned int>(candidate.size()),
                         font_size,
                         color,
                         font);
            return;
        }
    }
    painter.text(x, y, "...", 3, font_size, color, font);
}

bool icon_label_break_char(char ch) {
    return ch == ' ' || ch == '_' || ch == '-';
}

std::string trim_icon_label_line(std::string line) {
    while (!line.empty() && icon_label_break_char(line.back()))
        line.pop_back();
    while (!line.empty() && line.front() == ' ')
        line.erase(line.begin());
    return line;
}

std::vector<std::string> icon_label_tokens(std::string const& label) {
    std::vector<std::string> tokens;
    std::size_t start = 0;
    for (std::size_t i = 0; i < label.size(); ++i) {
        if (!icon_label_break_char(label[i]))
            continue;
        tokens.push_back(label.substr(start, i - start + 1));
        start = i + 1;
    }
    if (start < label.size())
        tokens.push_back(label.substr(start));
    return tokens;
}

void pop_utf8_codepoint(std::string& text) {
    if (text.empty())
        return;
    std::size_t start = text.size() - 1;
    while (start > 0
           && (static_cast<unsigned char>(text[start]) & 0xc0) == 0x80) {
        --start;
    }
    text.erase(start);
}

std::string middle_elide_icon_label_line(
        phenotype::Painter& painter,
        std::string text,
        float max_width,
        float font_size) {
    auto const font = finder_font();
    auto width_of = [&](std::string const& value) {
        return painter.measure_text(
            value.c_str(),
            static_cast<unsigned int>(value.size()),
            font_size,
            font);
    };
    if (width_of(text) <= max_width)
        return text;

    std::string suffix;
    std::string head = text;
    auto const last_break = text.find_last_of(" _-");
    if (last_break != std::string::npos
        && last_break + 1 < text.size()
        && text.size() - last_break - 1 <= 24) {
        head = trim_icon_label_line(text.substr(0, last_break));
        suffix = text.substr(last_break + 1);
    } else if (auto const dot = text.find_last_of('.');
               dot != std::string::npos
               && dot > 0
               && text.size() - dot <= 8) {
        head = text.substr(0, dot);
        suffix = text.substr(dot);
    }

    while (!head.empty() && width_of(head + "..." + suffix) > max_width)
        pop_utf8_codepoint(head);
    if (!head.empty())
        return head + "..." + suffix;

    head = text;
    while (!head.empty() && width_of(head + "...") > max_width)
        pop_utf8_codepoint(head);
    return head.empty() ? std::string{"..."} : head + "...";
}

std::vector<std::string> finder_icon_label_lines(
        phenotype::Painter& painter,
        std::string const& label,
        float max_width,
        float font_size) {
    auto const font = finder_font();
    auto width_of = [&](std::string const& text) {
        return painter.measure_text(
            text.c_str(),
            static_cast<unsigned int>(text.size()),
            font_size,
            font);
    };
    auto const tokens = icon_label_tokens(label);
    std::string first_line;
    std::size_t next_token = 0;
    for (; next_token < tokens.size(); ++next_token) {
        auto candidate = first_line + tokens[next_token];
        if (!first_line.empty() && width_of(candidate) > max_width)
            break;
        first_line = std::move(candidate);
    }

    auto first = trim_icon_label_line(std::move(first_line));
    if (first.empty())
        return {middle_elide_icon_label_line(
            painter,
            label,
            max_width,
            font_size)};

    if (next_token >= tokens.size())
        return {std::move(first)};

    std::string remainder;
    for (; next_token < tokens.size(); ++next_token)
        remainder += tokens[next_token];
    auto second = middle_elide_icon_label_line(
        painter,
        trim_icon_label_line(std::move(remainder)),
        max_width,
        font_size);
    return {std::move(first), std::move(second)};
}

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
            apply_desktop_pointer_focus(explorer, "toolbar_actions");
            state.more_actions_open = !state.more_actions_open;
            explorer.status = state.more_actions_open
                ? "More actions ready."
                : "Ready";
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
}

void finder_button(std::string label,
                   Msg msg,
                   bool selected = false,
                   float max_width = 0.0f,
                   float font_size = 15.0f,
                   bool centered = false,
                   bool subtle_chrome = false) {
    auto const& t = phenotype::current_theme();
    phenotype::ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected
        ? t.accent
        : (subtle_chrome ? rgba(255, 255, 255, 92) : t.transparent);
    options.has_hover_background = true;
    options.hover_background = selected
        ? t.accent_strong
        : rgba(255, 255, 255, 110);
    options.has_border_color = true;
    options.border_color = t.transparent;
    options.has_text_color = true;
    options.text_color = selected ? t.state_active_fg : t.foreground;
    options.border_width = 0.0f;
    options.border_radius = 10.0f;
    options.font_size = font_size;
    options.padding_top = 6.0f;
    options.padding_right = 10.0f;
    options.padding_bottom = 6.0f;
    options.padding_left = 10.0f;
    options.max_width = max_width;
    if (centered)
        options.text_align = phenotype::TextAlign::Center;

    phenotype::widget::button<Msg>(label, std::move(msg), options);
}

void finder_column_location_button(std::string label,
                                   std::string icon,
                                   Msg msg,
                                   bool selected,
                                   float max_width,
                                   float font_size,
                                   phenotype::icons::SymbolDocumentCache const& cache) {
    auto options = phenotype::icons::macos_control_button_style(
        phenotype::icons::ControlButtonStyleOptions{
            .role = phenotype::icons::SymbolPresentationRole::Sidebar,
            .selected = selected,
            .width = max_width,
            .height = k_column_location_row_height,
            .border_radius = 8.0f,
        });

    phenotype::widget::canvas_button<Msg>(
        phenotype::str{label},
        max_width,
        k_column_location_row_height,
        [label, icon, selected, max_width, font_size, &cache](
                phenotype::Painter& painter,
                phenotype::ButtonVisualState state) {
            float const icon_box = 22.0f;
            float const icon_x = 7.0f;
            float const icon_y =
                (k_column_location_row_height - icon_box) * 0.5f;
            paint_finder_symbol_centered(
                painter,
                cache,
                icon_presentation_for_state(
                    sidebar_symbol(icon),
                    phenotype::icons::SymbolPresentationRole::Sidebar,
                    selected,
                    state),
                icon_x,
                icon_y,
                icon_box,
                icon_box);

            auto const ink = selected ? rgba(0, 122, 255)
                                      : rgba(28, 28, 30);
            float const text_y =
                std::max(0.0f, (k_column_location_row_height - 18.0f) * 0.5f);
            painter.text(34.0f,
                         text_y,
                         label.c_str(),
                         static_cast<unsigned int>(label.size()),
                         font_size,
                         ink,
                         finder_font());
            paint_finder_symbol_centered(
                painter,
                cache,
                phenotype::icons::Symbol::Forward,
                max_width - 22.0f,
                (k_column_location_row_height - 18.0f) * 0.5f,
                18.0f,
                18.0f,
                14.0f,
                selected ? rgba(0, 122, 255, 210)
                         : rgba(130, 130, 136, 210));
        },
        std::move(msg),
        options,
        stable_token(label) ^ stable_token(icon) ^ 0x3f7000u
            ^ (selected ? 0x3f1f00u : 0u));
}

void finder_icon_label_button(std::string const& label,
                              bool selected,
                              float max_width,
                              float font_size,
                              float fixed_height) {
    auto const& t = phenotype::current_theme();
    phenotype::ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected ? t.accent : t.transparent;
    options.has_hover_background = true;
    options.hover_background = selected
        ? t.accent_strong
        : rgba(255, 255, 255, 110);
    options.has_border_color = true;
    options.border_color = t.transparent;
    options.has_text_color = true;
    options.text_color = selected ? t.state_active_fg : t.foreground;
    options.border_width = 0.0f;
    options.border_radius = 10.0f;
    options.fixed_height = fixed_height;
    options.max_width = max_width;
    phenotype::widget::canvas_button<Msg>(
        phenotype::str{label},
        max_width,
        fixed_height,
        [label, selected, max_width, font_size, fixed_height](
                phenotype::Painter& painter) {
            auto const ink = selected ? rgba(255, 255, 255) : rgba(28, 28, 30);
            auto const lines = finder_icon_label_lines(
                painter,
                label,
                max_width - 12.0f,
                font_size);
            float y = lines.size() == 1
                ? std::max(0.0f, (fixed_height - 20.0f) * 0.5f)
                : 4.0f;
            for (auto const& line : lines) {
                auto const width = painter.measure_text(
                    line.c_str(),
                    static_cast<unsigned int>(line.size()),
                    font_size,
                    finder_font());
                painter.text(
                    std::max(0.0f, (max_width - width) * 0.5f),
                    y,
                    line.c_str(),
                    static_cast<unsigned int>(line.size()),
                    font_size,
                    ink,
                    finder_font());
                y += 19.0f;
            }
        },
        SelectEntry{label},
        options,
        stable_token(label) ^ (selected ? 0x1f1f00u : 0x1f1000u));
}

void finder_entry_row_button(file_explorer_demo::Entry const& entry,
                             bool selected,
                             float max_width,
                             float font_size,
                             float fixed_height,
                             phenotype::icons::SymbolDocumentCache const& cache) {
    auto const& t = phenotype::current_theme();
    phenotype::ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected ? t.accent : t.transparent;
    options.has_hover_background = true;
    options.hover_background = selected
        ? t.accent_strong
        : rgba(255, 255, 255, 110);
    options.has_border_color = true;
    options.border_color = t.transparent;
    options.has_text_color = true;
    options.text_color = selected ? t.state_active_fg : t.foreground;
    options.border_width = 0.0f;
    options.border_radius = 8.0f;
    options.fixed_height = fixed_height;
    options.max_width = max_width;

    phenotype::widget::canvas_button<Msg>(
        phenotype::str{entry.name},
        max_width,
        fixed_height,
        [entry, selected, max_width, font_size, fixed_height, &cache](
                phenotype::Painter& painter) {
            float const icon_box = 24.0f;
            float const icon_size = 18.0f;
            float const icon_x = 8.0f;
            float const icon_y = (fixed_height - icon_box) * 0.5f;
            paint_entry_symbol(
                painter,
                cache,
                entry,
                selected,
                icon_x,
                icon_y,
                icon_box,
                icon_size);

            auto const ink = selected ? rgba(255, 255, 255)
                                      : rgba(28, 28, 30);
            float const text_x = 36.0f;
            float const text_y = std::max(0.0f, (fixed_height - 18.0f) * 0.5f);
            painter.text(text_x,
                         text_y,
                         entry.name.c_str(),
                         static_cast<unsigned int>(entry.name.size()),
                         font_size,
                         ink,
                         finder_font());
            if (entry.folder) {
                auto const chevron = ">";
                painter.text(max_width - 20.0f,
                             text_y,
                             chevron,
                             1,
                             font_size,
                             selected ? rgba(255, 255, 255, 220)
                                      : rgba(110, 110, 116),
                             finder_font());
            }
        },
        SelectEntry{entry.name},
        options,
        stable_token(entry.name) ^ (selected ? 0x2f1f00u : 0x2f1000u));
}

void sidebar_row(std::string_view label,
                 std::string_view icon,
                 std::string location_id,
                 bool selected,
                 phenotype::icons::SymbolDocumentCache const& cache) {
    using namespace phenotype;
    auto options = icons::macos_control_button_style(
        icons::ControlButtonStyleOptions{
            .role = icons::SymbolPresentationRole::Sidebar,
            .selected = selected,
            .width = k_sidebar_row_width,
            .height = k_sidebar_row_height,
            .border_radius = k_sidebar_selected_row_radius,
        });

    std::string label_text(label);
    std::string icon_name(icon);
    widget::canvas_button<Msg>(
        str{label_text},
        k_sidebar_row_width,
        k_sidebar_row_height,
        [label_text, icon_name, selected, &cache](
                Painter& painter,
                phenotype::ButtonVisualState state) {
            float const icon_top =
                (k_sidebar_row_height - k_sidebar_icon_size) * 0.5f;
            paint_sidebar_icon(
                painter,
                cache,
                icon_name,
                selected,
                state,
                k_sidebar_icon_leading,
                icon_top);
            auto ink = selected ? rgba(0, 122, 255) : rgba(30, 30, 30);
            painter.text(k_sidebar_label_leading,
                         k_sidebar_label_top,
                         label_text.c_str(),
                         static_cast<unsigned int>(label_text.size()),
                         15.0f,
                         ink,
                         finder_font());
        },
        SelectLocation{std::move(location_id)},
        options,
        stable_token(label_text) ^ stable_token(icon_name)
            ^ (selected ? 0x511f00u : 0x510000u));
}

void sidebar_heading(std::string_view label) {
    using namespace phenotype;
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = rgba(0, 0, 0, 0);
    options.has_hover_background = true;
    options.hover_background = rgba(0, 0, 0, 0);
    options.has_border_color = true;
    options.border_color = rgba(0, 0, 0, 0);
    options.border_width = 0.0f;
    options.border_radius = 0.0f;
    options.max_width = k_sidebar_row_width;
    options.fixed_height = k_sidebar_heading_height;

    std::string label_text(label);
    widget::canvas_button<Msg>(
        str{label_text},
        k_sidebar_row_width,
        k_sidebar_heading_height,
        [label_text](Painter& painter) {
            painter.text(k_sidebar_heading_label_leading,
                         k_sidebar_heading_label_top,
                         label_text.c_str(),
                         static_cast<unsigned int>(label_text.size()),
                         13.0f,
                         rgba(82, 82, 86),
                         finder_font(phenotype::FontWeight::Bold));
        },
        Noop{},
        options,
        stable_token(label_text) ^ 0x520000u);
}

void native_window_control_reserve_slot() {
    phenotype::widget::canvas(
        k_sidebar_row_width,
        k_native_window_control_reserve_height,
        [](phenotype::Painter&) {},
        {},
        0x530000u);
}

void finder_sidebar(State const& state) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    auto relative = file_explorer_demo::relative_location(
        explorer.root,
        explorer.current);
    bool const in_root = relative == "Demo Root";
    auto const& labels = state.labels;
    auto const& icon_cache = state.icon_cache;
    layout::sidebar(k_sidebar_width, [&] {
        native_window_control_reserve_slot();
        sidebar_row(labels.sidebar_recents, "recents", "root", in_root, icon_cache);
        sidebar_row(labels.sidebar_shared, "shared", "shared",
                    relative == "Demo Root/Shared", icon_cache);
        layout::spacer(k_sidebar_section_gap);
        sidebar_heading(labels.favorites);
        sidebar_row(labels.applications, "app", "root", false, icon_cache);
        sidebar_row(labels.desktop, "desktop", "root", false, icon_cache);
        sidebar_row(labels.documents, "doc", "documents",
                    relative == "Demo Root/Documents", icon_cache);
        sidebar_row(labels.downloads, "download", "root", false, icon_cache);
        layout::spacer(k_sidebar_section_gap);
        sidebar_heading(labels.locations);
        sidebar_row(labels.icloud_drive, "cloud", "root", false, icon_cache);
        sidebar_row(labels.home, "home", "root", false, icon_cache);
        sidebar_row(labels.airdrop, "airdrop", "shared", false, icon_cache);
        sidebar_row(labels.trash, "trash", "trash", relative == "Trash", icon_cache);
    }, MaterialKind::Thin, SpaceToken::Lg, SpaceToken::Xs);
}

void toolbar_separator() {
    phenotype::widget::canvas(1.0f, 28.0f,
        [](phenotype::Painter& painter) {
            fill_round(painter, 0.0f, 2.0f, 1.0f, 24.0f, 0.5f,
                       rgba(205, 205, 210, 130));
        },
        {},
        0x6901u);
}

void view_mode_button(char const* label,
                      file_explorer_demo::ExplorerViewMode mode,
                      file_explorer_demo::ExplorerViewMode current,
                      phenotype::icons::Symbol symbol,
                      std::uint64_t token) {
    bool const selected = mode == current;
    std::string semantic_label(label);
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        symbol,
        SetViewMode{mode},
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Toolbar,
            .selected = selected,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = token,
        });
}

void toolbar_action_button(char const* label,
                           phenotype::icons::Symbol symbol,
                           std::uint64_t token) {
    std::string semantic_label(label);
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        symbol,
        ToolbarAction{semantic_label},
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Toolbar,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = token,
        });
}

void toolbar_message_button(char const* label,
                            phenotype::icons::Symbol symbol,
                            Msg msg,
                            std::uint64_t token,
                            bool selected = false) {
    std::string semantic_label(label);
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        symbol,
        std::move(msg),
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Toolbar,
            .selected = selected,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = token,
        });
}

Msg on_search_changed(std::string text) {
    return SearchChanged{std::move(text)};
}

void search_toggle_button(bool selected) {
    std::string semantic_label = "Search Control";
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        phenotype::icons::Symbol::Search,
        ToggleSearch{},
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Toolbar,
            .selected = selected,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = 0x6401u,
        });
}

void sort_action_button(file_explorer_demo::Snapshot const& snap) {
    std::string semantic_label = "Group Sort";
    if (!snap.sort_label.empty())
        semantic_label += " (" + snap.sort_label + ")";
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        phenotype::icons::Symbol::SortGroup,
        CycleSort{},
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Toolbar,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = 0x6501u,
        });
}

void file_action_button(char const* label,
                        Msg msg,
                        bool enabled,
                        phenotype::icons::Symbol symbol,
                        std::uint64_t token) {
    std::string semantic_label(label);
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        symbol,
        std::move(msg),
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Navigation,
            .disabled = !enabled,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = token,
        });
}

void navigation_button(char const* label,
                       Msg msg,
                       bool enabled,
                       phenotype::icons::Symbol symbol,
                       std::uint64_t token) {
    std::string semantic_label(label);
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{semantic_label},
        symbol,
        std::move(msg),
        phenotype::icons::SymbolButtonOptions{
            .role = phenotype::icons::SymbolPresentationRole::Navigation,
            .disabled = !enabled,
            .width = k_toolbar_icon_button_width,
            .height = k_toolbar_icon_button_height,
            .token_salt = token,
        });
}

void finder_toolbar(State const& state,
                    file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    layout::toolbar(toolbar_shell_options(), [&] {
        layout::toolbar(
            toolbar_group_options(
                "Navigation Controls",
                file_explorer_demo::k_desktop_toolbar_navigation_group_width),
            [&] {
                navigation_button(state.labels.back.c_str(), GoBack{}, snap.can_go_back,
                                  icons::Symbol::Back, 0x6201u);
                toolbar_separator();
                navigation_button(state.labels.forward.c_str(), GoForward{}, snap.can_go_forward,
                                  icons::Symbol::Forward, 0x6202u);
            });
        widget::text(snap.relative_location == "Demo Root"
            ? state.labels.title
            : snap.relative_location,
            TextSize::Heading);
        layout::weighted(1.0f, [] {});
        layout::toolbar(
            toolbar_group_options(
                "View Controls",
                file_explorer_demo::k_desktop_toolbar_view_group_width),
            [&] {
                view_mode_button("Icon View", file_explorer_demo::ExplorerViewMode::Icon,
                                 state.explorer.view_mode, icons::Symbol::Grid, 0x6301u);
                view_mode_button("List View", file_explorer_demo::ExplorerViewMode::List,
                                 state.explorer.view_mode, icons::Symbol::List, 0x6302u);
                toolbar_separator();
                view_mode_button("Column View", file_explorer_demo::ExplorerViewMode::Column,
                                 state.explorer.view_mode, icons::Symbol::Columns, 0x6303u);
                toolbar_separator();
                view_mode_button("Gallery View", file_explorer_demo::ExplorerViewMode::Gallery,
                                 state.explorer.view_mode, icons::Symbol::Gallery, 0x6304u);
            });
        layout::toolbar(
            toolbar_group_options(
                "Group Sort",
                file_explorer_demo::k_desktop_toolbar_sort_group_width),
            [&] {
                sort_action_button(snap);
            });
        layout::toolbar(
            toolbar_group_options(
                "Share Tag More",
                file_explorer_demo::k_desktop_toolbar_action_group_width),
            [&] {
                toolbar_action_button("Share", icons::Symbol::Share, 0x6601u);
                toolbar_action_button("Tag", icons::Symbol::Tag, 0x6602u);
                toolbar_message_button("More", icons::Symbol::More,
                                       ToggleMoreActions{},
                                       0x6603u,
                                       state.more_actions_open);
            });
        layout::toolbar(
            toolbar_group_options(
                "Search Control",
                state.search_visible
                    ? 220.0f
                    : file_explorer_demo::k_desktop_toolbar_search_collapsed_width),
            [&] {
                search_toggle_button(state.search_visible);
                if (state.search_visible) {
                    widget::text_field<Msg>(
                        state.labels.search_placeholder,
                        state.explorer.search,
                        on_search_changed);
                }
            });
    });
}

template<typename Paint>
void more_action_item(char const* label,
                      Msg msg,
                      bool enabled,
                      Paint paint,
                      std::uint64_t token) {
    using namespace phenotype;
    layout::sized_box(84.0f, [&] {
        layout::column([&] {
            file_action_button(label,
                               std::move(msg),
                               enabled,
                               paint,
                               token);
            auto label_text = std::string{label};
            widget::text(label_text,
                         TextSize::Small,
                         enabled ? TextColor::Default : TextColor::Muted);
        },
        SpaceToken::Xs,
        CrossAxisAlignment::Center,
        MainAxisAlignment::Start);
    });
}

void finder_more_actions(State const& state,
                         file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    layout::row([&] {
        layout::weighted(1.0f, [] {});
        auto options = layout::glass_surface_options(
            layout::GlassSurfacePreset::ToolbarGroup,
            "More Actions Menu");
        options.kind = MaterialKind::Regular;
        options.max_width = 376.0f;
        options.fixed_height = 328.0f;
        options.border_radius = k_toolbar_group_radius;
        layout::material_surface(
            options,
            [&] {
                layout::column([&] {
                    layout::row([&] {
                        more_action_item(state.labels.create_file.c_str(),
                                         CreateFile{},
                                         snap.can_create_file,
                                         icons::Symbol::NewDocument,
                                         0x6701u);
                        more_action_item(state.labels.create_folder.c_str(),
                                         CreateFolder{},
                                         snap.can_create_folder,
                                         icons::Symbol::NewFolder,
                                         0x6702u);
                        more_action_item(state.labels.duplicate.c_str(),
                                         DuplicateSelected{},
                                         snap.can_duplicate_selected,
                                         icons::Symbol::Duplicate,
                                         0x6703u);
                        more_action_item(state.labels.delete_action.c_str(),
                                         DeleteSelected{},
                                         snap.can_delete_selected,
                                         icons::Symbol::Trash,
                                         0x6704u);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_system_font.c_str(),
                            font_family_message("system"),
                            true,
                            icons::Symbol::Document,
                            0x6711u);
                        more_action_item(
                            state.labels.preferences_package_font.c_str(),
                            font_family_message("pretendard"),
                            true,
                            icons::Symbol::TextDocument,
                            0x6712u);
                        more_action_item(
                            state.labels.preferences_system_text_size.c_str(),
                            system_font_metrics_message("system"),
                            true,
                            icons::Symbol::Search,
                            0x671du);
                        more_action_item(
                            state.labels.preferences_package_text_size.c_str(),
                            system_font_metrics_message("package"),
                            true,
                            icons::Symbol::Document,
                            0x671eu);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_text_larger.c_str(),
                            font_scale_step_message(state.explorer, 0.1f),
                            true,
                            icons::Symbol::Plus,
                            0x6713u);
                        more_action_item(
                            state.labels.preferences_text_smaller.c_str(),
                            font_scale_step_message(state.explorer, -0.1f),
                            true,
                            icons::Symbol::ChevronDown,
                            0x6714u);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_body_larger.c_str(),
                            body_font_size_step_message(state.explorer, 1.0f),
                            true,
                            icons::Symbol::Plus,
                            0x6721u);
                        more_action_item(
                            state.labels.preferences_body_smaller.c_str(),
                            body_font_size_step_message(state.explorer, -1.0f),
                            true,
                            icons::Symbol::ChevronDown,
                            0x6722u);
                        more_action_item(
                            state.labels.preferences_line_height_taller.c_str(),
                            line_height_step_message(state.explorer, 0.1f),
                            true,
                            icons::Symbol::List,
                            0x6723u);
                        more_action_item(
                            state.labels.preferences_line_height_tighter.c_str(),
                            line_height_step_message(state.explorer, -0.1f),
                            true,
                            icons::Symbol::Columns,
                            0x6724u);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        auto const* horizontal_faster =
                            state.labels.preferences_horizontal_scroll_faster.c_str();
                        auto const* horizontal_slower =
                            state.labels.preferences_horizontal_scroll_slower.c_str();
                        more_action_item(
                            state.labels.preferences_scroll_faster.c_str(),
                            scroll_speed_step_message(state.explorer, 0.25f),
                            true,
                            icons::Symbol::ChevronDown,
                            0x6715u);
                        more_action_item(
                            state.labels.preferences_scroll_slower.c_str(),
                            scroll_speed_step_message(state.explorer, -0.25f),
                            true,
                            icons::Symbol::ChevronUp,
                            0x6716u);
                        more_action_item(
                            horizontal_faster,
                            horizontal_scroll_speed_step_message(
                                state.explorer,
                                0.25f),
                            true,
                            icons::Symbol::Columns,
                            0x6717u);
                        more_action_item(
                            horizontal_slower,
                            horizontal_scroll_speed_step_message(
                                state.explorer,
                                -0.25f),
                            true,
                            icons::Symbol::List,
                            0x6718u);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_system_scroll.c_str(),
                            system_scroll_metrics_message("system"),
                            true,
                            icons::Symbol::Columns,
                            0x671bu);
                        more_action_item(
                            state.labels.preferences_app_scroll.c_str(),
                            system_scroll_metrics_message("app"),
                            true,
                            icons::Symbol::List,
                            0x671cu);
                    }, SpaceToken::Xs);
                    layout::row([&] {
                        more_action_item(
                            state.labels.preferences_system_appearance.c_str(),
                            color_scheme_message("system"),
                            true,
                            icons::Symbol::Gallery,
                            0x6719u);
                        more_action_item(
                            state.labels.preferences_dark_appearance.c_str(),
                            color_scheme_message("dark"),
                            true,
                            icons::Symbol::Movie,
                            0x6720u);
                    }, SpaceToken::Xs);
                }, SpaceToken::Xs);
            });
    },
    SpaceToken::Xs,
    CrossAxisAlignment::Center,
    MainAxisAlignment::Start);
}

void finder_grid(State const& state,
                 file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    auto const chrome = file_explorer_demo::explorer_chrome_metrics(
        state.explorer,
        "desktop");
    auto& svg_cache = state.svg_preview_cache;
    layout::material_surface(
        content_surface_options(),
        [&] {
            if (entries.empty()) {
                widget::text(state.labels.no_matching_files);
                return;
            }
            layout::spacer(chrome.icon_grid_top_inset);
            layout::scroll_view(chrome.icon_grid_scroll_height, [&] {
                auto columns = file_explorer_demo::explorer_icon_grid_columns(chrome);
                layout::grid(std::move(columns), chrome.icon_grid_row_height, [&] {
                    for (auto const& entry : entries) {
                        bool const selected = snap.has_selection
                            && snap.selected.name == entry.name;
                        auto const preview_source = selected
                            ? snap.preview
                            : std::string{};
                        layout::column([&] {
                            widget::canvas(chrome.icon_grid_thumbnail_width,
                                chrome.icon_grid_thumbnail_height,
                                [entry, selected, preview_source, &svg_cache](
                                        Painter& painter) {
                                    paint_item_thumbnail(
                                        painter,
                                        svg_cache,
                                        entry,
                                        selected,
                                        preview_source);
                                },
                                {},
                                thumbnail_paint_token(
                                    entry,
                                    selected,
                                    preview_source));
                            finder_icon_label_button(
                                entry.name,
                                selected,
                                chrome.icon_grid_column_width,
                                chrome.icon_grid_label_font_size,
                                chrome.icon_grid_label_height);
                        }, SpaceToken::Xs,
                           CrossAxisAlignment::Center,
                           MainAxisAlignment::Start);
                    }
                }, chrome.icon_grid_gap);
            }, SpaceToken::Sm);
        });
}

void finder_list(State const& state,
                 file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    float const scroll_height = file_explorer_demo::desktop_scroll_height(
        state.explorer, 164.0f, 552.0f, 676.0f);
    layout::material_surface(
        content_surface_options(SpaceToken::Sm),
        [&] {
            layout::row([&] {
                layout::sized_box(420.0f, [&] {
                    widget::text(state.labels.name, TextSize::Small, TextColor::Muted);
                });
                layout::sized_box(160.0f, [&] {
                    widget::text(state.labels.kind, TextSize::Small, TextColor::Muted);
                });
                layout::sized_box(120.0f, [&] {
                    widget::text(state.labels.size, TextSize::Small, TextColor::Muted);
                });
            }, SpaceToken::Sm, CrossAxisAlignment::Center, MainAxisAlignment::Start);
            if (entries.empty()) {
                widget::text(state.labels.no_matching_files);
                return;
            }
            layout::scroll_view(scroll_height, [&] {
                layout::column([&] {
                    for (auto const& entry : entries) {
                        bool const selected = snap.has_selection
                            && snap.selected.name == entry.name;
                        layout::row([&] {
                            layout::sized_box(420.0f, [&] {
                                finder_entry_row_button(
                                    entry,
                                    selected,
                                    410.0f,
                                    15.0f,
                                    32.0f,
                                    state.icon_cache);
                            });
                            layout::sized_box(160.0f, [&] {
                                widget::text(file_explorer_demo::entry_kind_label(entry),
                                             TextSize::Small,
                                             TextColor::Muted);
                            });
                            layout::sized_box(120.0f, [&] {
                                widget::text(file_explorer_demo::entry_size_label(entry),
                                             TextSize::Small,
                                             TextColor::Muted);
                            });
                        }, SpaceToken::Sm,
                           CrossAxisAlignment::Center,
                           MainAxisAlignment::Start);
                    }
                }, SpaceToken::Xs);
            }, SpaceToken::Sm);
        });
}

void finder_column_view(State const& state,
                        file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    float const column_height = file_explorer_demo::desktop_scroll_height(
        state.explorer, 224.0f, 500.0f, 620.0f);
    auto& svg_cache = state.svg_preview_cache;
    layout::material_surface(
        content_surface_options(),
        [&] {
            layout::row([&] {
                layout::sized_box(210.0f, [&] {
                    layout::column([&] {
                        widget::text(state.labels.locations, TextSize::Small, TextColor::Muted);
                        finder_column_location_button(
                            "Demo Root",
                            "home",
                            SelectLocation{"root"},
                            snap.relative_location == "Demo Root",
                            190.0f,
                            14.0f,
                            state.icon_cache);
                        finder_column_location_button(
                            state.labels.documents,
                            "doc",
                            SelectLocation{"documents"},
                            snap.relative_location == "Demo Root/Documents",
                            190.0f,
                            14.0f,
                            state.icon_cache);
                        finder_column_location_button(
                            state.labels.pictures,
                            "image",
                            SelectLocation{"pictures"},
                            snap.relative_location == "Demo Root/Pictures",
                            190.0f,
                            14.0f,
                            state.icon_cache);
                        finder_column_location_button(
                            state.labels.sidebar_shared,
                            "shared",
                            SelectLocation{"shared"},
                            snap.relative_location == "Demo Root/Shared",
                            190.0f,
                            14.0f,
                            state.icon_cache);
                    }, SpaceToken::Xs);
                });
                layout::sized_box(360.0f, [&] {
                    layout::column([&] {
                        widget::text(snap.relative_location,
                                     TextSize::Small,
                                     TextColor::Muted);
                        if (entries.empty()) {
                            widget::text(state.labels.no_matching_files);
                            return;
                        }
                        layout::scroll_view(column_height, [&] {
                            layout::column([&] {
                                for (auto const& entry : entries) {
                                    bool const selected = snap.has_selection
                                        && snap.selected.name == entry.name;
                                    finder_entry_row_button(
                                        entry,
                                        selected,
                                        330.0f,
                                        14.0f,
                                        30.0f,
                                        state.icon_cache);
                                }
                            }, SpaceToken::Xs);
                        });
                    }, SpaceToken::Xs);
                });
                layout::weighted(1.0f, [&] {
                    layout::column([&] {
                        widget::text(state.labels.preview, TextSize::Small, TextColor::Muted);
                        if (snap.has_selection) {
                            widget::canvas(160.0f, 110.0f,
                                [entry = snap.selected,
                                 preview = snap.preview,
                                 &svg_cache](Painter& painter) {
                                    paint_item_thumbnail(
                                        painter,
                                        svg_cache,
                                        entry,
                                        true,
                                        preview);
                                },
                                {},
                                thumbnail_paint_token(
                                    snap.selected,
                                    true,
                                    snap.preview,
                                    0x440000u));
                            widget::text(snap.selected.name, TextSize::Heading);
                            widget::text(file_explorer_demo::entry_kind_label(snap.selected),
                                         TextSize::Small,
                                         TextColor::Muted);
                            widget::text(compact_preview(snap.preview),
                                         TextSize::Small,
                                         TextColor::Muted);
                        } else {
                            widget::text(state.labels.select_file_to_preview);
                        }
                    }, SpaceToken::Sm);
                });
            }, SpaceToken::Md, CrossAxisAlignment::Start, MainAxisAlignment::Start);
        });
}

void finder_gallery_view(State const& state,
                         file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    float const gallery_height = file_explorer_demo::desktop_scroll_height(
        state.explorer, 352.0f, 366.0f, 520.0f);
    auto& svg_cache = state.svg_preview_cache;
    file_explorer_demo::Entry hero = snap.has_selection && !snap.selected.name.empty()
        ? snap.selected
        : (entries.empty() ? file_explorer_demo::Entry{} : entries.front());
    auto const hero_preview = snap.has_selection && hero.name == snap.selected.name
        ? snap.preview
        : std::string{};
    bool const has_hero = !hero.name.empty();
    layout::material_surface(
        content_surface_options(),
        [&] {
            if (!has_hero) {
                widget::text(state.labels.no_matching_files);
                return;
            }
            layout::row([&] {
                widget::canvas(220.0f, 150.0f,
                    [hero, hero_preview, &svg_cache](Painter& painter) {
                        paint_item_thumbnail(
                            painter,
                            svg_cache,
                            hero,
                            true,
                            hero_preview);
                    },
                    {},
                    thumbnail_paint_token(hero, true, hero_preview, 0x550000u));
                layout::weighted(1.0f, [&] {
                    layout::column([&] {
                        widget::text(hero.name, TextSize::Heading);
                        widget::text(file_explorer_demo::entry_kind_label(hero),
                                     TextSize::Small,
                                     TextColor::Muted);
                        widget::text(snap.has_selection
                            ? compact_preview(snap.preview)
                            : state.labels.select_file_to_preview,
                            TextSize::Small,
                            TextColor::Muted);
                    }, SpaceToken::Sm);
                });
            }, SpaceToken::Lg, CrossAxisAlignment::Center, MainAxisAlignment::Start);
            layout::scroll_view(gallery_height, [&] {
                std::vector<float> columns{160.0f, 160.0f, 160.0f, 160.0f};
                layout::grid(std::move(columns), 142.0f, [&] {
                    for (auto const& entry : entries) {
                        bool const selected = entry.name == hero.name;
                        auto const preview_source =
                            snap.has_selection && entry.name == snap.selected.name
                            ? snap.preview
                            : std::string{};
                        layout::column([&] {
                            widget::canvas(128.0f, 76.0f,
                                [entry, selected, preview_source, &svg_cache](
                                        Painter& painter) {
                                    paint_item_thumbnail(
                                        painter,
                                        svg_cache,
                                        entry,
                                        selected,
                                        preview_source);
                                },
                                {},
                                thumbnail_paint_token(
                                    entry,
                                    selected,
                                    preview_source,
                                    0x560000u));
                            finder_button(entry.name,
                                          SelectEntry{entry.name},
                                          selected,
                                          150.0f,
                                          14.0f,
                                          true);
                        }, SpaceToken::Xs,
                           CrossAxisAlignment::Center,
                           MainAxisAlignment::Start);
                    }
                }, 18.0f);
            });
        });
}

void finder_content(State const& state,
                    file_explorer_demo::Snapshot const& snap) {
    switch (state.explorer.view_mode) {
        case file_explorer_demo::ExplorerViewMode::List:
            finder_list(state, snap);
            return;
        case file_explorer_demo::ExplorerViewMode::Column:
            finder_column_view(state, snap);
            return;
        case file_explorer_demo::ExplorerViewMode::Gallery:
            finder_gallery_view(state, snap);
            return;
        case file_explorer_demo::ExplorerViewMode::Icon:
        default:
            finder_grid(state, snap);
            return;
    }
}

void finder_status_bar(State const& state,
                       file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    auto const chrome = file_explorer_demo::explorer_chrome_metrics(
        explorer,
        "desktop");
    auto options = layout::glass_surface_options(
        layout::GlassSurfacePreset::StatusBar,
        "Status Bar");
    options.padding = SpaceToken::Xs;
    options.border_radius = 12.0f;
    layout::status_bar(
        options,
        [&] {
            layout::row([&] {
                std::string status = finder_status(snap);
                if (!explorer.status.empty())
                    status += " - " + explorer.status;
                if (snap.has_selection
                    && explorer.status != "Ready"
                    && !snap.preview.empty()) {
                    status += " - " + compact_preview(snap.preview);
                }
                if (!snap.operation_label.empty())
                    status += " - " + snap.operation_label;
                float const side_width = snap.has_selection ? 264.0f : 0.0f;
                float const status_width = std::max(
                    180.0f,
                    chrome.content_surface_width - side_width - 36.0f);
                layout::sized_box(status_width, [&] {
                    widget::semantic_canvas(
                        status_width,
                        20.0f,
                        str{status},
                        [status, status_width](Painter& painter) {
                            paint_elided_finder_text(
                                painter,
                                0.0f,
                                1.0f,
                                status,
                                status_width,
                                current_theme().small_font_size,
                                current_theme().muted);
                        },
                        {},
                        stable_token(status) ^ 0x5a5a5101u);
                });
                if (snap.has_selection) {
                    layout::sized_box(144.0f, [&] {
                        widget::text(snap.selected_kind_label,
                                     TextSize::Small,
                                     TextColor::Muted);
                    });
                    layout::sized_box(96.0f, [&] {
                        widget::text(snap.selected_size_label,
                                     TextSize::Small,
                                     TextColor::Muted);
                    });
                }
            },
            SpaceToken::Sm,
            CrossAxisAlignment::Center,
            MainAxisAlignment::Start);
        });
}

template<typename CommandMsg>
void finder_key_command(phenotype::native::Key key,
                        int modifiers,
                        CommandMsg msg,
                        char const* label,
                        bool allow_when_input_focused = false) {
    phenotype::app::key_command<Msg>(
        static_cast<unsigned int>(key),
        modifiers,
        Msg{std::move(msg)},
        phenotype::KeyCommandOptions{
            .allow_when_input_focused = allow_when_input_focused,
            .debug_label = label ? std::string{label} : std::string{},
        });
}

ExplorerInputMessage explorer_key_message(
        file_explorer_demo::ExplorerInput input) {
    input.modality = file_explorer_demo::ExplorerInputModality::Keyboard;
    return ExplorerInputMessage{std::move(input)};
}

ExplorerInputMessage explorer_key_message(
        file_explorer_demo::ExplorerInputKind kind) {
    return explorer_key_message(keyboard_input(kind));
}

void finder_key_input(phenotype::native::Key key,
                      int modifiers,
                      file_explorer_demo::ExplorerInput input,
                      char const* label,
                      bool allow_when_input_focused = false) {
    finder_key_command(
        key,
        modifiers,
        explorer_key_message(std::move(input)),
        label,
        allow_when_input_focused);
}

void finder_key_input(phenotype::native::Key key,
                      int modifiers,
                      file_explorer_demo::ExplorerInputKind kind,
                      char const* label,
                      bool allow_when_input_focused = false) {
    finder_key_input(
        key,
        modifiers,
        keyboard_input(kind),
        label,
        allow_when_input_focused);
}

void finder_key_move(phenotype::native::Key key,
                     int modifiers,
                     file_explorer_demo::ExplorerSelectionMove move,
                     char const* label) {
    finder_key_input(key, modifiers, keyboard_move_input(move), label);
}

void register_finder_key_commands() {
    using phenotype::native::Key;
    using phenotype::native::Modifier;
    constexpr int none = 0;
    constexpr int shift = static_cast<int>(Modifier::Shift);
    constexpr int control = static_cast<int>(Modifier::Control);
    constexpr int super = static_cast<int>(Modifier::Super);

    finder_key_command(Key::F, super, ShowSearch{}, "show_search", true);
    finder_key_command(Key::F, control, ShowSearch{}, "show_search", true);
    finder_key_input(
        Key::Enter,
        none,
        file_explorer_demo::ExplorerInputKind::ActivateSelected,
        "activate_selected");
    finder_key_input(
        Key::KpEnter,
        none,
        file_explorer_demo::ExplorerInputKind::ActivateSelected,
        "activate_selected");
    finder_key_move(
        Key::Up,
        none,
        file_explorer_demo::ExplorerSelectionMove::Up,
        "move_selection_up");
    finder_key_move(
        Key::Down,
        none,
        file_explorer_demo::ExplorerSelectionMove::Down,
        "move_selection_down");
    finder_key_move(
        Key::Left,
        none,
        file_explorer_demo::ExplorerSelectionMove::Left,
        "move_selection_left");
    finder_key_move(
        Key::Right,
        none,
        file_explorer_demo::ExplorerSelectionMove::Right,
        "move_selection_right");
    finder_key_move(
        Key::Home,
        none,
        file_explorer_demo::ExplorerSelectionMove::Home,
        "move_selection_home");
    finder_key_move(
        Key::End,
        none,
        file_explorer_demo::ExplorerSelectionMove::End,
        "move_selection_end");
    finder_key_move(
        Key::PageUp,
        none,
        file_explorer_demo::ExplorerSelectionMove::PageUp,
        "move_selection_page_up");
    finder_key_move(
        Key::PageDown,
        none,
        file_explorer_demo::ExplorerSelectionMove::PageDown,
        "move_selection_page_down");
    finder_key_input(
        Key::Backspace,
        none,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::Delete,
        none,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::Backspace,
        super,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::Backspace,
        control,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::Delete,
        super,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::Delete,
        control,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::D,
        super,
        file_explorer_demo::ExplorerInputKind::DuplicateSelected,
        "duplicate_selected");
    finder_key_input(
        Key::D,
        control,
        file_explorer_demo::ExplorerInputKind::DuplicateSelected,
        "duplicate_selected");
    finder_key_input(
        Key::N,
        shift | super,
        file_explorer_demo::ExplorerInputKind::CreateFolder,
        "create_folder");
    finder_key_input(
        Key::N,
        shift | control,
        file_explorer_demo::ExplorerInputKind::CreateFolder,
        "create_folder");
    finder_key_input(
        Key::Escape,
        none,
        file_explorer_demo::ExplorerInputKind::DismissTransient,
        "dismiss_transient",
        true);
}

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
            layout::material_surface(
                window_options,
                [&] {
                    finder_sidebar(state);
                    layout::weighted(1.0f, [&] {
                        layout::column([&] {
                            finder_toolbar(state, snap);
                            if (state.more_actions_open)
                                finder_more_actions(state, snap);
                            finder_content(state, snap);
                            if (file_explorer_demo::desktop_status_bar_visible(
                                    state.explorer)) {
                                finder_status_bar(state, snap);
                            }
                        }, SpaceToken::Sm);
                    });
                });
        });
}

} // namespace

int main() {
    phenotype::Theme theme = phenotype::current_theme();
    theme = phenotype::theme_with_resource_defaults(
        theme,
        runtime_resource_catalog());
    theme.background = rgba(246, 246, 246);
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
    };

    return phenotype::native::run_app<State, Msg>(
        1300,
        760,
        "phenotype file explorer desktop",
        window_options,
        view,
        update,
        [](int width, int height, float scale) -> Msg {
            return Resized{width, height, scale};
        });
}

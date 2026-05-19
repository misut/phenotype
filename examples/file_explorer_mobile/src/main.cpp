#include <concepts>
#include <algorithm>
#include <cctype>
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

namespace {

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
        float maximum) {
    if (!raw || !*raw)
        return std::nullopt;
    char* end = nullptr;
    float value = std::strtof(raw, &end);
    if (end == raw || (end && *end != '\0'))
        return std::nullopt;
    if (!(value > 0.0f))
        return std::nullopt;
    if (value < minimum)
        value = minimum;
    if (value > maximum)
        value = maximum;
    return value;
}

phenotype::ThemePreferenceOverrides initial_theme_preference_overrides() {
    phenotype::ThemePreferenceOverrides overrides{};
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_FONT_FAMILY")) {
        if (std::string_view{raw} == "system") {
            overrides.prefer_system_font_family = true;
        } else if (*raw) {
            overrides.font_family = raw;
        }
    }
    if (env_truthy(std::getenv("PHENOTYPE_FILE_EXPLORER_USE_SYSTEM_FONT")))
        overrides.prefer_system_font_family = true;
    if (auto scale = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_FONT_SCALE"),
            0.75f,
            1.8f)) {
        overrides.font_scale = *scale;
    }
    if (auto speed = env_float(
            std::getenv("PHENOTYPE_FILE_EXPLORER_SCROLL_SPEED"),
            0.25f,
            4.0f)) {
        overrides.scroll_delta_multiplier = *speed;
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
        .prefer_system_font_family = overrides.prefer_system_font_family,
        .prefer_system_color_scheme = overrides.prefer_system_color_scheme,
        .apply_system_font_scale = overrides.apply_system_font_scale,
        .apply_system_accent_color = overrides.apply_system_accent_color,
    };
}

file_explorer_demo::RuntimePreferenceState runtime_preference_state(
        phenotype::Theme const& theme,
        phenotype::PlatformSystemSettingsSnapshot system_settings,
        phenotype::ThemePreferenceOverrides theme_preferences) {
    file_explorer_demo::RuntimePreferenceState state{};
    state.source = "native-debug-capabilities+environment-overrides";
    state.system_settings = system_preference_snapshot(system_settings);
    state.theme_preferences = theme_preference_snapshot(theme_preferences);
    state.effective_font_family = theme.default_font_family;
    state.effective_color_scheme = phenotype::theme_color_scheme_is_dark(
        theme_preferences.prefer_system_color_scheme
            ? system_settings.color_scheme
            : theme_preferences.color_scheme)
        ? "dark"
        : "light";
    state.effective_body_font_size = theme.body_font_size;
    state.effective_small_font_size = theme.small_font_size;
    state.effective_scroll_delta_multiplier = theme.scroll_delta_multiplier;
    state.effective_scroll_horizontal_delta_multiplier =
        theme.scroll_horizontal_delta_multiplier;
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
        .prefer_system_font_family = preferences.prefer_system_font_family,
        .prefer_system_color_scheme = preferences.prefer_system_color_scheme,
        .apply_system_font_scale = preferences.apply_system_font_scale,
        .apply_system_accent_color = preferences.apply_system_accent_color,
    };
}

void sync_runtime_theme(file_explorer_demo::ExplorerState& explorer) {
    auto overrides = theme_preferences_from_state(explorer.theme_preferences);
    auto theme = phenotype::apply_system_theme_preferences(
        g_base_theme,
        g_system_settings,
        overrides);
    auto runtime = runtime_preference_state(
        theme,
        g_system_settings,
        overrides);
    if (explorer.preferences_source == "application-input")
        runtime.source = explorer.preferences_source;
    g_runtime_preferences = runtime;
    file_explorer_demo::apply_runtime_preferences(explorer, runtime);
    phenotype::set_theme(theme);
}

struct State {
    phenotype::icons::SymbolDocumentCache icon_cache =
        phenotype::icons::make_symbol_document_cache();
    file_explorer_demo::ExplorerState explorer;
    file_explorer_demo::ExplorerLabels labels;

    State()
        : explorer(initial_explorer_state()),
          labels(file_explorer_demo::file_explorer_labels(
              initial_locale(),
              runtime_resource_catalog())) {
        file_explorer_demo::apply_runtime_preferences(
            explorer,
            g_runtime_preferences);
        apply_startup_inputs(explorer, "mobile");
        sync_runtime_theme(explorer);
    }
};

State const* g_debug_state = nullptr;

auto file_explorer_application_debug_payload() {
    if (!g_debug_state) {
        file_explorer_demo::ExplorerState empty{};
        auto snap = file_explorer_demo::snapshot(empty);
        auto chrome = file_explorer_demo::explorer_chrome_metrics(empty, "mobile");
        return file_explorer_demo::file_explorer_application_debug_json(
            empty,
            snap,
            chrome,
            "mobile");
    }
    auto snap = file_explorer_demo::snapshot(g_debug_state->explorer);
    auto chrome = file_explorer_demo::explorer_chrome_metrics(
        g_debug_state->explorer,
        "mobile");
    return file_explorer_demo::file_explorer_application_debug_json(
        g_debug_state->explorer,
        snap,
        chrome,
        "mobile");
}

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

void mobile_symbol_button(std::string const& label,
                          phenotype::icons::Symbol symbol,
                          Msg msg,
                          phenotype::icons::SymbolPresentationRole role =
                              phenotype::icons::SymbolPresentationRole::Navigation,
                          bool enabled = true,
                          bool selected = false,
                          std::uint64_t token = 0) {
    phenotype::widget::symbol_button<Msg>(
        phenotype::str{label},
        symbol,
        std::move(msg),
        phenotype::icons::SymbolButtonOptions{
            .role = role,
            .selected = selected,
            .disabled = !enabled,
            .width = 38.0f,
            .height = 38.0f,
            .token_salt = token,
        });
}

std::uint64_t mobile_stable_token(std::string_view value,
                                  std::uint64_t salt) noexcept {
    std::uint64_t hash = 1469598103934665603ull ^ salt;
    for (unsigned char ch : value) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    return hash;
}

phenotype::icons::Symbol mobile_entry_symbol(
        file_explorer_demo::Entry const& entry) noexcept {
    return phenotype::icons::from_catalog_symbol(
        file_explorer_demo::entry_symbol(entry));
}

phenotype::Color mobile_entry_symbol_color(
        file_explorer_demo::Entry const& entry,
        bool selected) noexcept {
    if (selected)
        return phenotype::Color{255, 255, 255, 255};
    return phenotype::icons::macos_file_type_color(mobile_entry_symbol(entry));
}

phenotype::Color with_alpha(phenotype::Color color,
                            unsigned char alpha) noexcept {
    color.a = alpha;
    return color;
}

std::size_t utf8_prefix_boundary(std::string_view text,
                                 std::size_t bytes) noexcept {
    bytes = std::min(bytes, text.size());
    while (bytes > 0
           && bytes < text.size()
           && (static_cast<unsigned char>(text[bytes]) & 0xC0u) == 0x80u) {
        --bytes;
    }
    return bytes;
}

void paint_elided_text(phenotype::Painter& painter,
                       float x,
                       float y,
                       std::string_view text,
                       float max_width,
                       float font_size,
                       phenotype::Color color) {
    if (max_width <= 0.0f || text.empty())
        return;
    if (painter.measure_text(text.data(),
                             static_cast<unsigned int>(text.size()),
                             font_size) <= max_width) {
        painter.text(x,
                     y,
                     text.data(),
                     static_cast<unsigned int>(text.size()),
                     font_size,
                     color);
        return;
    }

    std::string shortened;
    std::size_t bytes = text.size();
    while (bytes > 0) {
        bytes = utf8_prefix_boundary(text, bytes - 1);
        if (bytes == 0)
            break;
        shortened.assign(text.substr(0, bytes));
        shortened += "...";
        if (painter.measure_text(shortened.c_str(),
                                 static_cast<unsigned int>(shortened.size()),
                                 font_size) <= max_width) {
            painter.text(x,
                         y,
                         shortened.c_str(),
                         static_cast<unsigned int>(shortened.size()),
                         font_size,
                         color);
            return;
        }
    }

    char const* ellipsis = "...";
    painter.text(x, y, ellipsis, 3, font_size, color);
}

void mobile_entry_row(file_explorer_demo::Entry const& entry,
                      bool selected,
                      float row_width,
                      phenotype::icons::SymbolDocumentCache const& cache) {
    using namespace phenotype;
    auto const& theme = current_theme();
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected
        ? theme.accent
        : Color{255,
                255,
                255,
                static_cast<unsigned char>(entry.folder ? 54 : 84)};
    options.has_hover_background = true;
    options.hover_background = selected
        ? theme.accent_strong
        : Color{255, 255, 255, 132};
    options.has_pressed_background = true;
    options.pressed_background = selected
        ? theme.accent_strong
        : Color{229, 229, 234, 180};
    options.has_border_color = true;
    options.border_color = Color{0, 0, 0, 0};
    options.border_width = 0.0f;
    options.border_radius = 14.0f;
    options.max_width = row_width;
    options.fixed_height = 60.0f;

    auto const label = file_explorer_demo::entry_label(entry);
    auto const symbol = mobile_entry_symbol(entry);
    auto const symbol_color = mobile_entry_symbol_color(entry, selected);
    auto const title_color = selected
        ? Color{255, 255, 255, 255}
        : theme.foreground;
    auto const meta_color = selected
        ? Color{255, 255, 255, 210}
        : theme.muted;
    auto const meta = entry.folder
        ? std::string{"Folder"}
        : file_explorer_demo::format_size(entry.size);
    auto const token = mobile_stable_token(entry.name, 0x771000u)
        ^ icons::paint_token(symbol, 24.0f, symbol_color);

    widget::canvas_button<Msg>(
        str{label},
        row_width,
        60.0f,
        [entry, symbol, symbol_color, title_color, meta_color, meta, row_width, &cache](
                Painter& painter,
                ButtonVisualState state) {
            auto icon_color = symbol_color;
            if (state.pressed)
                icon_color = with_alpha(icon_color, 210);
            icons::paint_symbol_centered(
                painter,
                cache,
                symbol,
                10.0f,
                12.0f,
                36.0f,
                36.0f,
                24.0f,
                icon_color);

            float const text_x = 56.0f;
            float const text_max = std::max(80.0f, row_width - 86.0f);
            paint_elided_text(
                painter,
                text_x,
                12.0f,
                entry.name,
                text_max,
                16.0f,
                title_color);
            paint_elided_text(
                painter,
                text_x,
                35.0f,
                meta,
                text_max,
                12.0f,
                meta_color);
            if (entry.folder) {
                char const* chevron = ">";
                painter.text(row_width - 24.0f,
                             22.0f,
                             chevron,
                             1,
                             16.0f,
                             meta_color);
            }
        },
        SelectEntry{entry.name},
        options,
        token);
}

void update(State& state, Msg msg) {
    auto& explorer = state.explorer;
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, SelectLocation>) {
            file_explorer_demo::select_location(explorer, m.id);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, SelectEntry>) {
            file_explorer_demo::open_entry(explorer, m.name);
            if (!explorer.selected_name.empty())
                explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, ExplorerInputMessage>) {
            file_explorer_demo::apply_explorer_input(
                explorer,
                m.input,
                "mobile");
        } else if constexpr (std::same_as<T, SearchChanged>) {
            file_explorer_demo::set_search_filter(explorer, m.text);
        } else if constexpr (std::same_as<T, DraftNameChanged>) {
            explorer.draft_name = m.text;
        } else if constexpr (std::same_as<T, DraftFolderNameChanged>) {
            explorer.draft_folder_name = m.text;
        } else if constexpr (std::same_as<T, DraftBodyChanged>) {
            explorer.draft_body = m.text;
        } else if constexpr (std::same_as<T, SelectTab>) {
            explorer.mobile_tab = m.value;
        } else if constexpr (std::same_as<T, CreateFile>) {
            file_explorer_demo::create_file(explorer);
            explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, CreateFolder>) {
            file_explorer_demo::create_folder(explorer);
            explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, DeleteSelected>) {
            file_explorer_demo::delete_selected(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, DuplicateSelected>) {
            file_explorer_demo::duplicate_selected(explorer);
            explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, GoBack>) {
            file_explorer_demo::go_back(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, GoForward>) {
            file_explorer_demo::go_forward(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, GoUp>) {
            file_explorer_demo::go_up(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, CycleSort>) {
            file_explorer_demo::cycle_sort_mode(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, ResetDemo>) {
            file_explorer_demo::reset_demo_tree(explorer, "mobile");
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, Resized>) {
            explorer.viewport_width = m.width;
            explorer.viewport_height = m.height;
            explorer.viewport_scale = m.scale;
        }
    }, msg);
    sync_runtime_theme(explorer);
}

void top_bar(State const& state, file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::toolbar(layout::MaterialSurfaceOptions{
        .kind = MaterialKind::Clear,
        .padding = SpaceToken::Md,
        .gap = SpaceToken::Sm,
        .semantic_label = "Toolbar",
    }, [&] {
        widget::text(state.labels.mobile_title);
        widget::text(snap.relative_location, TextSize::Small, TextColor::Muted);
        layout::spacer(8);
        widget::text_field<Msg>(
            state.labels.search_placeholder,
            explorer.search,
            on_search_changed);
        layout::spacer(8);
        std::vector<phenotype::str> tabs;
        tabs.emplace_back(
            state.labels.tab_browse.c_str(),
            static_cast<unsigned int>(state.labels.tab_browse.size()));
        tabs.emplace_back(
            state.labels.tab_preview.c_str(),
            static_cast<unsigned int>(state.labels.tab_preview.size()));
        tabs.emplace_back(
            state.labels.tab_create.c_str(),
            static_cast<unsigned int>(state.labels.tab_create.size()));
        widget::tabs<Msg>(tabs, explorer.mobile_tab, [](std::size_t index) -> Msg {
            return SelectTab{index};
        });
    });
}

void location_strip(file_explorer_demo::ExplorerLabels const& labels) {
    using namespace phenotype;
    layout::navigation(layout::MaterialSurfaceOptions{
        .kind = MaterialKind::Thin,
        .direction = FlexDirection::Row,
        .padding = SpaceToken::Sm,
        .gap = SpaceToken::Xs,
        .cross_align = CrossAxisAlignment::Center,
        .semantic_label = "Locations",
    }, [&] {
        layout::row([&] {
            widget::button<Msg>(labels.root, SelectLocation{"root"});
            widget::button<Msg>(labels.docs, SelectLocation{"documents"});
            widget::button<Msg>(labels.pics, SelectLocation{"pictures"});
            widget::button<Msg>(labels.sidebar_shared, SelectLocation{"shared"});
            widget::button<Msg>(labels.trash, SelectLocation{"trash"});
        }, SpaceToken::Xs, CrossAxisAlignment::Center, MainAxisAlignment::Start);
    });
}

void browse_tab(
        State const& state,
        file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& labels = state.labels;
    float const row_width = std::max(
        280.0f,
        static_cast<float>(state.explorer.viewport_width) - 48.0f);
    layout::material_surface(MaterialKind::Regular, [&] {
        layout::row([&] {
            layout::weighted(1.0f, [&] {
                std::string summary = std::to_string(snap.entries.size()) + " items";
                if (!snap.sort_label.empty())
                    summary += " - " + snap.sort_label;
                widget::text(summary, TextSize::Small, TextColor::Muted);
            });
            mobile_symbol_button(
                labels.sort,
                icons::Symbol::SortGroup,
                CycleSort{},
                icons::SymbolPresentationRole::Toolbar,
                true,
                false,
                0x7101u);
            mobile_symbol_button(
                labels.back,
                icons::Symbol::Back,
                GoBack{},
                icons::SymbolPresentationRole::Navigation,
                snap.can_go_back,
                false,
                0x7102u);
            mobile_symbol_button(
                labels.forward,
                icons::Symbol::Forward,
                GoForward{},
                icons::SymbolPresentationRole::Navigation,
                snap.can_go_forward,
                false,
                0x7103u);
            mobile_symbol_button(
                labels.up,
                icons::Symbol::ChevronUp,
                GoUp{},
                icons::SymbolPresentationRole::Navigation,
                true,
                false,
                0x7104u);
        });
        layout::spacer(8);
        layout::scroll_view(430.0f, [&] {
            if (snap.entries.empty()) {
                widget::text(labels.no_matching_files);
                return;
            }
            for (auto const& entry : snap.entries) {
                layout::material_surface(
                    entry.folder ? MaterialKind::Clear : MaterialKind::Thin,
                    [&] {
                        mobile_entry_row(
                            entry,
                            entry.name == state.explorer.selected_name,
                            row_width,
                            state.icon_cache);
                    },
                    SpaceToken::Sm,
                    SpaceToken::Xs);
            }
        }, SpaceToken::Sm);
    }, SpaceToken::Md, SpaceToken::Sm);
}

void preview_tab(
        State const& state,
        file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::material_surface(MaterialKind::Thick, [&] {
        widget::text(state.labels.preview);
        layout::spacer(4);
        if (snap.has_selection) {
            widget::text(snap.selected.name);
            widget::text(snap.selected_kind_label + " - "
                             + snap.selected_size_label,
                         TextSize::Small,
                         TextColor::Muted);
        } else {
            widget::text(state.labels.select_file_from_browse);
        }
        layout::spacer(8);
        widget::code(snap.preview);
        layout::spacer(12);
        widget::button<Msg>(state.labels.duplicate_file,
                            DuplicateSelected{},
                            ButtonVariant::Default,
                            !snap.can_duplicate_selected);
        layout::spacer(8);
        widget::button<Msg>(state.labels.delete_selected,
                            DeleteSelected{},
                            ButtonVariant::Default,
                            !snap.can_delete_selected);
        layout::spacer(8);
        widget::text(explorer.status, TextSize::Small, TextColor::Muted);
    }, SpaceToken::Md, SpaceToken::Sm);
}

void create_tab(State const& state) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::material_surface(MaterialKind::Regular, [&] {
        widget::text(state.labels.tab_create);
        widget::text(state.labels.create_hint,
                     TextSize::Small,
                     TextColor::Muted);
        layout::spacer(8);
        widget::text_field<Msg>(
            state.labels.file_name,
            explorer.draft_name,
            on_draft_name_changed);
        layout::spacer(8);
        widget::text_field<Msg>(
            state.labels.contents,
            explorer.draft_body,
            on_draft_body_changed);
        layout::spacer(10);
        widget::button<Msg>(
            state.labels.create_file,
            CreateFile{},
            ButtonVariant::Primary);
        layout::spacer(10);
        widget::text_field<Msg>(
            state.labels.folder_name,
            explorer.draft_folder_name,
            on_draft_folder_name_changed);
        layout::spacer(8);
        widget::button<Msg>(
            state.labels.create_folder,
            CreateFolder{},
            ButtonVariant::Default);
        layout::spacer(6);
        widget::button<Msg>(state.labels.reset_demo_files, ResetDemo{});
        layout::spacer(12);
        widget::text(state.labels.preferences);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_system_font,
                font_family_message("system"));
            widget::button<Msg>(
                state.labels.preferences_package_font,
                font_family_message("pretendard"));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_text_larger,
                font_scale_step_message(explorer, 0.1f));
            widget::button<Msg>(
                state.labels.preferences_text_smaller,
                font_scale_step_message(explorer, -0.1f));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_scroll_faster,
                scroll_speed_step_message(explorer, 0.25f));
            widget::button<Msg>(
                state.labels.preferences_scroll_slower,
                scroll_speed_step_message(explorer, -0.25f));
        }, SpaceToken::Xs);
        layout::row([&] {
            widget::button<Msg>(
                state.labels.preferences_system_appearance,
                color_scheme_message("system"));
            widget::button<Msg>(
                state.labels.preferences_dark_appearance,
                color_scheme_message("dark"));
        }, SpaceToken::Xs);
    }, SpaceToken::Md, SpaceToken::Sm);
}

void view(State const& state) {
    using namespace phenotype;
    g_debug_state = &state;
    auto snap = file_explorer_demo::snapshot(state.explorer);
    layout::padded(SpaceToken::Md, [&] {
        layout::material_container(
            layout::MaterialContainerOptions{
                .container_id = 3100u,
                .spacing = 12.0f,
                .morph_transitions = true,
            },
            [&] {
                layout::column([&] {
                    top_bar(state, snap);
                    location_strip(state.labels);
                    if (state.explorer.mobile_tab == 0) {
                        browse_tab(state, snap);
                    } else if (state.explorer.mobile_tab == 1) {
                        preview_tab(state, snap);
                    } else {
                        create_tab(state);
                    }
                    layout::status_bar([&] {
                        widget::text(state.labels.status);
                        std::string detail = "Status: " + state.explorer.status;
                        if (!snap.operation_label.empty()) {
                            detail += "\n";
                            detail += snap.operation_label;
                        }
                        detail += "\nViewport: ";
                        detail += std::to_string(state.explorer.viewport_width);
                        detail += " x ";
                        detail += std::to_string(state.explorer.viewport_height);
                        widget::code(detail);
                    }, MaterialKind::Thick);
                }, SpaceToken::Sm);
            });
    });
}

} // namespace

int main() {
    phenotype::Theme theme = phenotype::current_theme();
    theme = phenotype::theme_with_resource_defaults(
        theme,
        runtime_resource_catalog());
    auto const system_settings =
        phenotype::native::debug::capabilities().system_settings;
    auto const theme_preferences = initial_theme_preference_overrides();
    g_base_theme = theme;
    g_system_settings = system_settings;
    theme = phenotype::apply_system_theme_preferences(
        theme,
        system_settings,
        theme_preferences);
    g_runtime_preferences = runtime_preference_state(
        theme,
        system_settings,
        theme_preferences);
    phenotype::set_theme(theme);
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

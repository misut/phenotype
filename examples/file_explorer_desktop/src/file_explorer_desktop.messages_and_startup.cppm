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

export module file_explorer_desktop:messages_and_startup;

import file_explorer_shared;
import phenotype;
import phenotype.native;

export namespace file_explorer_desktop {
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
struct OpenSettings {};
struct CloseSettings {};
struct SetSidebarPosition { bool right; };
struct ToggleSidebarSection { std::string id; };
struct GoBack {};
struct GoForward {};
struct GoUp {};
struct CycleSort {};
struct SortBy { file_explorer_demo::SortMode mode; };
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
    OpenSettings,
    CloseSettings,
    SetSidebarPosition,
    ToggleSidebarSection,
    GoBack,
    GoForward,
    GoUp,
    CycleSort,
    SortBy,
    Refresh,
    ResetDemo,
    Resized,
    Noop>;

void open_settings_from_app_menu() {
    phenotype::runtime::post<Msg>(OpenSettings{});
}

fs::path g_initial_filesystem_root;
std::string g_initial_filesystem_root_label;
std::string g_initial_filesystem_root_source = "default-home";
bool g_use_demo_root = false;
bool g_initial_filesystem_root_configured = false;
bool g_initial_filesystem_mutations_allowed = false;
bool g_initial_settings_open = false;

std::string read_text_file(fs::path const& path) {
    auto input = std::ifstream{path, std::ios::binary};
    if (!input)
        return {};
    auto out = std::ostringstream{};
    out << input.rdbuf();
    return out.str();
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

fs::path home_directory() {
    if (char const* raw = std::getenv("HOME")) {
        if (*raw)
            return fs::path{raw};
    }
    std::error_code ec;
    auto current = fs::current_path(ec);
    return ec ? fs::path{} : current;
}

void set_initial_filesystem_root(
        fs::path root,
        std::string source,
        std::string label = {}) {
    g_initial_filesystem_root = std::move(root);
    g_initial_filesystem_root_source = std::move(source);
    g_initial_filesystem_root_label = std::move(label);
    g_initial_filesystem_root_configured = true;
    g_use_demo_root = false;
}

bool apply_initial_root_arg(
        std::string_view arg,
        char const* next) {
    if (arg == "--demo-root") {
        g_use_demo_root = true;
        g_initial_filesystem_root_configured = false;
        return false;
    }
    if (arg == "--allow-file-mutations"
        || arg == "--allow-filesystem-mutations") {
        g_initial_filesystem_mutations_allowed = true;
        return false;
    }
    if (arg == "--settings" || arg == "--open-settings"
        || arg == "--preferences") {
        g_initial_settings_open = true;
        return false;
    }
    if (arg == "--path" || arg == "--root") {
        if (next && *next)
            set_initial_filesystem_root(fs::path{next}, "command-line");
        return true;
    }
    constexpr std::string_view path_prefix = "--path=";
    constexpr std::string_view root_prefix = "--root=";
    if (arg.starts_with(path_prefix)) {
        set_initial_filesystem_root(
            fs::path{std::string{arg.substr(path_prefix.size())}},
            "command-line");
    } else if (arg.starts_with(root_prefix)) {
        set_initial_filesystem_root(
            fs::path{std::string{arg.substr(root_prefix.size())}},
            "command-line");
    }
    return false;
}

void configure_initial_filesystem_root(int argc, char** argv) {
    if (char const* mode = std::getenv("PHENOTYPE_FILE_EXPLORER_ROOT_MODE")) {
        auto value = std::string_view{mode};
        if (value == "demo" || value == "sandbox") {
            g_use_demo_root = true;
            g_initial_filesystem_root_configured = false;
        }
    }
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_ROOT")) {
        if (*raw)
            set_initial_filesystem_root(fs::path{raw}, "environment");
    }
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_PATH")) {
        if (*raw)
            set_initial_filesystem_root(fs::path{raw}, "environment");
    }
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_ROOT_LABEL")) {
        if (*raw)
            g_initial_filesystem_root_label = raw;
    }
    if (env_truthy(std::getenv("PHENOTYPE_FILE_EXPLORER_ALLOW_MUTATION"))
        || env_truthy(std::getenv("PHENOTYPE_FILE_EXPLORER_ALLOW_MUTATIONS"))
        || env_truthy(std::getenv(
            "PHENOTYPE_FILE_EXPLORER_ALLOW_FILESYSTEM_MUTATIONS"))) {
        g_initial_filesystem_mutations_allowed = true;
    }
    if (env_truthy(std::getenv("PHENOTYPE_FILE_EXPLORER_OPEN_SETTINGS"))
        || env_truthy(std::getenv(
            "PHENOTYPE_FILE_EXPLORER_SETTINGS_OPEN"))) {
        g_initial_settings_open = true;
    }
    for (int i = 1; i < argc; ++i) {
        auto arg = std::string_view{argv[i]};
        char const* next = (i + 1) < argc ? argv[i + 1] : nullptr;
        if (apply_initial_root_arg(arg, next))
            ++i;
    }
    if (!g_initial_filesystem_root_configured && !g_use_demo_root
        && !std::getenv("PHENOTYPE_ARTIFACT_EXIT")) {
        set_initial_filesystem_root(home_directory(), "default-home", "Home");
    }
}

bool initial_settings_open() {
    return g_initial_settings_open;
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
    auto state = g_use_demo_root || !g_initial_filesystem_root_configured
        ? file_explorer_demo::make_state("desktop")
        : file_explorer_demo::make_state_for_root(
            "desktop",
            g_initial_filesystem_root,
            g_initial_filesystem_root_label,
            g_initial_filesystem_root_source,
            g_initial_filesystem_mutations_allowed);
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_VIEW")) {
        if (*raw) {
            state.view_mode = file_explorer_demo::known_view_mode_name(raw)
                ? file_explorer_demo::view_mode_from_name(raw)
                : file_explorer_demo::ExplorerViewMode::Icon;
        }
    }
    return state;
}

} // namespace file_explorer_desktop

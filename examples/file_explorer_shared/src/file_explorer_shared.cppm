module;
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

export module file_explorer_shared;

import phenotype.resources;

export namespace file_explorer_demo {

namespace fs = std::filesystem;

struct Entry {
    std::string name;
    bool folder = false;
    std::uintmax_t size = 0;
};

struct OperationReceipt {
    std::string kind;
    std::string target;
    bool ok = false;
    std::string detail;
};

enum class SortMode {
    Name,
    Kind,
    Size,
};

struct Snapshot {
    fs::path root;
    fs::path current;
    std::string relative_location;
    std::vector<Entry> entries;
    Entry selected{};
    bool has_selection = false;
    bool can_go_back = false;
    bool can_go_forward = false;
    bool can_create_file = false;
    bool can_create_folder = false;
    bool can_delete_selected = false;
    bool can_duplicate_selected = false;
    bool can_preview_selected = false;
    std::string selected_kind_label;
    std::string selected_size_label;
    std::string selected_path_label;
    std::string item_summary;
    std::string action_summary;
    std::string operation_label;
    std::string sort_label;
    std::string preview;
    std::size_t file_count = 0;
    std::size_t folder_count = 0;
};

struct ExplorerState {
    fs::path root;
    fs::path current;
    std::string selected_name = "README.txt";
    std::string search;
    std::string draft_name = "New Note.txt";
    std::string draft_folder_name = "New Folder";
    std::string draft_body = "Created from the phenotype file explorer demo.";
    std::string status = "Ready";
    std::vector<fs::path> back_stack;
    std::vector<fs::path> forward_stack;
    OperationReceipt last_operation;
    SortMode sort_mode = SortMode::Name;
    int viewport_width = 0;
    int viewport_height = 0;
    float viewport_scale = 1.0f;
    std::size_t mobile_tab = 0;
};

enum class ExplorerInputKind {
    Noop,
    SelectLocation,
    SelectEntry,
    Search,
    Viewport,
    DraftName,
    DraftFolderName,
    DraftBody,
    CreateFile,
    CreateFolder,
    DeleteSelected,
    DuplicateSelected,
    GoBack,
    GoForward,
    GoUp,
    Sort,
    CycleSort,
    Reset,
    Scenario,
};

struct ExplorerInput {
    ExplorerInputKind kind = ExplorerInputKind::Noop;
    std::string value;
    SortMode sort_mode = SortMode::Name;
    int viewport_width = 0;
    int viewport_height = 0;
    float viewport_scale = 1.0f;
};

struct ExplorerInputParseResult {
    bool ok = false;
    ExplorerInput input{};
    std::string error;
};

struct ExplorerViewport {
    int width = 0;
    int height = 0;
    float scale = 1.0f;
};

struct ExplorerChromeMetrics {
    ExplorerViewport viewport{};
    float integrated_titlebar_height = 0.0f;
    float sidebar_width = 0.0f;
    float sidebar_row_width = 0.0f;
    float sidebar_row_height = 0.0f;
    float sidebar_heading_height = 0.0f;
    float toolbar_group_height = 0.0f;
    float toolbar_group_radius = 0.0f;
    float toolbar_icon_button_width = 0.0f;
    float toolbar_icon_button_height = 0.0f;
    float window_radius = 0.0f;
    float icon_grid_column_width = 0.0f;
    float icon_grid_row_height = 0.0f;
    float icon_grid_column_pitch = 0.0f;
    float icon_grid_scroll_height = 0.0f;
    int icon_grid_columns = 0;
    int icon_grid_visible_rows = 0;
    int icon_grid_visible_capacity = 0;
    int toolbar_group_count = 0;
    int toolbar_separator_count = 0;
    int toolbar_icon_button_count = 0;
    bool integrated_titlebar = true;
    bool native_window_controls = true;
    bool duplicate_window_controls = false;
    bool finder_segmented_toolbar = false;
};

struct ExplorerInputTrace {
    ExplorerInput input{};
    ExplorerChromeMetrics chrome{};
    std::string status;
    std::string relative_location;
    std::string selected_name;
    std::string operation_label;
    OperationReceipt operation;
    std::size_t visible_entries = 0;
    bool has_selection = false;
};

struct ExplorerDriveResult {
    std::string profile;
    ExplorerState state;
    Snapshot snapshot;
    ExplorerChromeMetrics chrome{};
    std::vector<ExplorerInputTrace> trace;
};

enum class ExplorerExpectationKind {
    Selected,
    Location,
    Entry,
    MissingEntry,
    Operation,
    StatusContains,
};

struct ExplorerExpectation {
    ExplorerExpectationKind kind = ExplorerExpectationKind::Selected;
    std::string value;
    bool expects_operation_ok = false;
    bool operation_ok = false;
};

struct ExplorerExpectationParseResult {
    bool ok = false;
    ExplorerExpectation expectation{};
    std::string error;
};

struct ExplorerExpectationResult {
    ExplorerExpectation expectation{};
    bool ok = false;
    std::string actual;
    std::string detail;
};

struct PackageResourceText {
    std::string source;
    std::string text;
};

inline constexpr int k_desktop_default_viewport_width = 1300;
inline constexpr int k_desktop_default_viewport_height = 760;
inline constexpr int k_mobile_default_viewport_width = 390;
inline constexpr int k_mobile_default_viewport_height = 844;
inline constexpr float k_desktop_integrated_titlebar_height = 56.0f;
inline constexpr float k_desktop_sidebar_width = 224.0f;
inline constexpr float k_desktop_sidebar_row_width = 188.0f;
inline constexpr float k_desktop_sidebar_row_height = 32.0f;
inline constexpr float k_desktop_sidebar_heading_height = 26.0f;
inline constexpr float k_desktop_window_radius = 18.0f;
inline constexpr float k_desktop_toolbar_group_radius = 19.0f;
inline constexpr float k_desktop_toolbar_group_height = 38.0f;
inline constexpr float k_desktop_toolbar_icon_button_width = 34.0f;
inline constexpr float k_desktop_toolbar_icon_button_height = 30.0f;
inline constexpr float k_desktop_icon_grid_column_width = 142.0f;
inline constexpr float k_desktop_icon_grid_row_height = 166.0f;
inline constexpr float k_desktop_icon_grid_column_pitch = 166.0f;

inline bool mobile_profile(std::string_view profile) {
    return profile == "mobile";
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

inline ExplorerChromeMetrics explorer_chrome_metrics(
        ExplorerState const& state,
        std::string_view profile) {
    auto viewport = effective_viewport(state, profile);
    if (mobile_profile(profile)) {
        return ExplorerChromeMetrics{
            .viewport = viewport,
            .integrated_titlebar_height = 0.0f,
            .sidebar_width = 0.0f,
            .sidebar_row_width = 0.0f,
            .sidebar_row_height = 0.0f,
            .sidebar_heading_height = 0.0f,
            .toolbar_group_height = 0.0f,
            .toolbar_group_radius = 0.0f,
            .toolbar_icon_button_width = 0.0f,
            .toolbar_icon_button_height = 0.0f,
            .window_radius = 0.0f,
            .icon_grid_column_width = 0.0f,
            .icon_grid_row_height = 0.0f,
            .icon_grid_column_pitch = 0.0f,
            .icon_grid_scroll_height = 0.0f,
            .icon_grid_columns = 0,
            .icon_grid_visible_rows = 0,
            .icon_grid_visible_capacity = 0,
            .toolbar_group_count = 0,
            .toolbar_separator_count = 0,
            .toolbar_icon_button_count = 0,
            .integrated_titlebar = false,
            .native_window_controls = false,
            .duplicate_window_controls = false,
            .finder_segmented_toolbar = false,
        };
    }
    auto columns = desktop_icon_grid_column_count(viewport);
    auto scroll_height = desktop_scroll_height_for_viewport(
        viewport,
        176.0f,
        528.0f,
        660.0f);
    int visible_rows = static_cast<int>(
        scroll_height / k_desktop_icon_grid_row_height);
    if (visible_rows < 1)
        visible_rows = 1;
    return ExplorerChromeMetrics{
        .viewport = viewport,
        .integrated_titlebar_height = k_desktop_integrated_titlebar_height,
        .sidebar_width = k_desktop_sidebar_width,
        .sidebar_row_width = k_desktop_sidebar_row_width,
        .sidebar_row_height = k_desktop_sidebar_row_height,
        .sidebar_heading_height = k_desktop_sidebar_heading_height,
        .toolbar_group_height = k_desktop_toolbar_group_height,
        .toolbar_group_radius = k_desktop_toolbar_group_radius,
        .toolbar_icon_button_width = k_desktop_toolbar_icon_button_width,
        .toolbar_icon_button_height = k_desktop_toolbar_icon_button_height,
        .window_radius = k_desktop_window_radius,
        .icon_grid_column_width = k_desktop_icon_grid_column_width,
        .icon_grid_row_height = k_desktop_icon_grid_row_height,
        .icon_grid_column_pitch = k_desktop_icon_grid_column_pitch,
        .icon_grid_scroll_height = scroll_height,
        .icon_grid_columns = columns,
        .icon_grid_visible_rows = visible_rows,
        .icon_grid_visible_capacity = columns * visible_rows,
        .toolbar_group_count = 6,
        .toolbar_separator_count = 3,
        .toolbar_icon_button_count = 15,
        .integrated_titlebar = true,
        .native_window_controls = true,
        .duplicate_window_controls = false,
        .finder_segmented_toolbar = true,
    };
}

inline std::vector<float> explorer_icon_grid_columns(
        ExplorerChromeMetrics const& chrome) {
    int count = chrome.icon_grid_columns > 0 ? chrome.icon_grid_columns : 2;
    return std::vector<float>(
        static_cast<std::size_t>(count),
        chrome.icon_grid_column_width > 0.0f
            ? chrome.icon_grid_column_width
            : k_desktop_icon_grid_column_width);
}

inline std::string lower_copy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

inline std::string trim(std::string_view text) {
    auto begin = text.begin();
    auto end = text.end();
    while (begin != end
           && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (begin != end
           && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

inline std::string sanitize_file_name(std::string_view input) {
    std::string cleaned;
    cleaned.reserve(input.size());
    bool previous_space = false;
    for (char ch : input) {
        auto uch = static_cast<unsigned char>(ch);
        bool const ok = std::isalnum(uch) || ch == '-' || ch == '_'
            || ch == '.' || ch == ' ';
        if (!ok)
            continue;
        if (ch == ' ') {
            if (previous_space)
                continue;
            previous_space = true;
        } else {
            previous_space = false;
        }
        cleaned.push_back(ch);
        if (cleaned.size() >= 48)
            break;
    }
    cleaned = trim(cleaned);
    while (!cleaned.empty() && cleaned.front() == '.')
        cleaned.erase(cleaned.begin());
    cleaned = trim(cleaned);
    if (cleaned.empty())
        cleaned = "New Note.txt";
    if (cleaned.find('.') == std::string::npos)
        cleaned += ".txt";
    return cleaned;
}

inline std::string sanitize_folder_name(std::string_view input) {
    std::string cleaned;
    cleaned.reserve(input.size());
    bool previous_space = false;
    for (char ch : input) {
        auto uch = static_cast<unsigned char>(ch);
        bool const ok = std::isalnum(uch) || ch == '-' || ch == '_'
            || ch == ' ';
        if (!ok)
            continue;
        if (ch == ' ') {
            if (previous_space)
                continue;
            previous_space = true;
        } else {
            previous_space = false;
        }
        cleaned.push_back(ch);
        if (cleaned.size() >= 48)
            break;
    }
    cleaned = trim(cleaned);
    while (!cleaned.empty() && cleaned.front() == '.')
        cleaned.erase(cleaned.begin());
    cleaned = trim(cleaned);
    if (cleaned.empty())
        cleaned = "New Folder";
    return cleaned;
}

inline std::string extension_lower(std::string const& name) {
    auto pos = name.rfind('.');
    if (pos == std::string::npos)
        return {};
    auto ext = name.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
}

inline fs::path demo_root(std::string_view profile) {
    std::string name = "phenotype-file-explorer-";
    name += profile;
    std::error_code ec;
    auto base = fs::temp_directory_path(ec);
    if (ec || base.empty()) {
        ec.clear();
        base = fs::current_path(ec);
    }
    if (ec || base.empty())
        base = ".";
    return base / name;
}

inline fs::path trash_path(fs::path const& root) {
    return root / ".Trash";
}

inline void write_file_if_missing(fs::path const& path, std::string_view body) {
    std::error_code ec;
    if (fs::exists(path, ec))
        return;
    ec.clear();
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    out << body;
}

inline fs::path ensure_demo_tree(std::string_view profile) {
    std::error_code ec;
    auto root = demo_root(profile);
    fs::create_directories(root / "Documents", ec);
    fs::create_directories(root / "Pictures", ec);
    fs::create_directories(root / "Shared", ec);
    fs::create_directories(trash_path(root), ec);

    write_file_if_missing(
        root / "README.txt",
        "Phenotype File Explorer\n\n"
        "This demo root is isolated under the system temp directory. "
        "Create and delete files here without touching the real home folder.\n");
    write_file_if_missing(
        root / "Application Form 3.pdf",
        "PDF placeholder\n\n"
        "The desktop example renders this sandboxed text file as a Finder-like "
        "document thumbnail while keeping file reads deterministic.\n");
    write_file_if_missing(
        root / "Application Form 2.pdf",
        "PDF placeholder for the Recents probe scene.\n");
    if (profile == "desktop" || profile == "test-model-contract") {
        write_file_if_missing(
            root / "1_필수_중도인출 신청서.pdf",
            "PDF placeholder with Korean filename for font fallback verification.\n");
        write_file_if_missing(
            root / "2_필수_운용지시서.pdf",
            "PDF placeholder with Korean filename for font fallback verification.\n");
        write_file_if_missing(
            root / "작성예시3_선택_DC 탈퇴신청서.pdf",
            "PDF placeholder with Korean filename for font fallback verification.\n");
        write_file_if_missing(
            root / "契約書_サンプル.pdf",
            "PDF placeholder with Japanese filename for font fallback verification.\n");
        write_file_if_missing(
            root / "资产证明.pdf",
            "PDF placeholder with Chinese filename for font fallback verification.\n");
    }
    write_file_if_missing(
        root / "Withdrawal Notice.pdf",
        "PDF placeholder for the Recents probe scene.\n");
    write_file_if_missing(
        root / "Operating Guide.pdf",
        "PDF placeholder for the Recents probe scene.\n");
    write_file_if_missing(
        root / "line_2.png",
        "PNG placeholder rendered as a light rounded Finder thumbnail.\n");
    write_file_if_missing(
        root / "line_23.png",
        "PNG placeholder rendered as a light rounded Finder thumbnail.\n");
    write_file_if_missing(
        root / "line_8.png",
        "PNG placeholder rendered as a light rounded Finder thumbnail.\n");
    write_file_if_missing(
        root / "Screen Recording 2025-11-04.mov",
        "MOV placeholder rendered as a wide video thumbnail.\n");
    write_file_if_missing(
        root / "Screen Recording 2025-11-05.mov",
        "MOV placeholder rendered as a wide video thumbnail.\n");
    write_file_if_missing(
        root / "Screen Recording 2025-11-06.mov",
        "MOV placeholder rendered as a wide video thumbnail.\n");
    write_file_if_missing(
        root / "Screen Recording 2025-11-07.mov",
        "MOV placeholder rendered as a wide video thumbnail.\n");
    write_file_if_missing(
        root / "Screenshot 2025-11-05.png",
        "PNG screenshot placeholder for the Finder-like icon grid.\n");
    write_file_if_missing(
        root / "Screenshot 2025-11-06.png",
        "PNG screenshot placeholder for the Finder-like icon grid.\n");
    write_file_if_missing(
        root / "Archive.zip",
        "ZIP placeholder rendered as a text-style fallback thumbnail.\n");
    write_file_if_missing(
        root / "Documents" / "Project Notes.txt",
        "Finder-like desktop layout\n"
        "- Sidebar locations\n"
        "- File list\n"
        "- Preview pane\n"
        "- Create and delete actions\n");
    write_file_if_missing(
        root / "Documents" / "Budget.txt",
        "Q2 prototype budget\nDesign: 12\nEngineering: 24\nQA: 8\n");
    write_file_if_missing(
        root / "Pictures" / "Glass Backdrop.txt",
        "Image placeholder for the mobile and desktop file explorer demos.\n");
    write_file_if_missing(
        root / "Shared" / "Team Checklist.txt",
        "Review artifact bundle\nVerify mobile layout\nKeep file operations sandboxed\n");
    return root;
}

inline std::string format_size(std::uintmax_t size) {
    if (size < 1024)
        return std::to_string(size) + " B";
    std::uintmax_t const kib = (size + 1023) / 1024;
    if (kib < 1024)
        return std::to_string(kib) + " KB";
    std::uintmax_t const mib = (kib + 1023) / 1024;
    return std::to_string(mib) + " MB";
}

inline std::string entry_kind_label(Entry const& entry) {
    if (entry.folder)
        return "Folder";
    auto ext = extension_lower(entry.name);
    if (ext == "pdf")
        return "PDF Document";
    if (ext == "png" || ext == "jpg" || ext == "jpeg")
        return "Image";
    if (ext == "mov" || ext == "mp4")
        return "Movie";
    if (ext == "zip")
        return "Archive";
    if (ext.empty())
        return "Document";
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return ext + " File";
}

inline std::string entry_size_label(Entry const& entry) {
    return entry.folder ? "--" : format_size(entry.size);
}

inline std::string sort_mode_label(SortMode mode) {
    switch (mode) {
        case SortMode::Kind: return "Kind";
        case SortMode::Size: return "Size";
        case SortMode::Name:
        default: return "Name";
    }
}

inline SortMode next_sort_mode(SortMode mode) {
    switch (mode) {
        case SortMode::Name: return SortMode::Kind;
        case SortMode::Kind: return SortMode::Size;
        case SortMode::Size:
        default: return SortMode::Name;
    }
}

inline std::string explorer_input_kind_name(ExplorerInputKind kind) {
    switch (kind) {
        case ExplorerInputKind::Noop: return "noop";
        case ExplorerInputKind::SelectLocation: return "select_location";
        case ExplorerInputKind::SelectEntry: return "select_entry";
        case ExplorerInputKind::Search: return "search";
        case ExplorerInputKind::Viewport: return "viewport";
        case ExplorerInputKind::DraftName: return "draft_name";
        case ExplorerInputKind::DraftFolderName: return "draft_folder_name";
        case ExplorerInputKind::DraftBody: return "draft_body";
        case ExplorerInputKind::CreateFile: return "create_file";
        case ExplorerInputKind::CreateFolder: return "create_folder";
        case ExplorerInputKind::DeleteSelected: return "delete_selected";
        case ExplorerInputKind::DuplicateSelected: return "duplicate_selected";
        case ExplorerInputKind::GoBack: return "go_back";
        case ExplorerInputKind::GoForward: return "go_forward";
        case ExplorerInputKind::GoUp: return "go_up";
        case ExplorerInputKind::Sort: return "sort";
        case ExplorerInputKind::CycleSort: return "cycle_sort";
        case ExplorerInputKind::Reset: return "reset";
        case ExplorerInputKind::Scenario: return "scenario";
    }
    return "noop";
}

inline std::string explorer_input_label(ExplorerInput const& input) {
    auto label = explorer_input_kind_name(input.kind);
    if (input.kind == ExplorerInputKind::Sort)
        return label + ":" + sort_mode_label(input.sort_mode);
    if (!input.value.empty())
        return label + ":" + input.value;
    return label;
}

inline std::string explorer_expectation_kind_name(ExplorerExpectationKind kind) {
    switch (kind) {
        case ExplorerExpectationKind::Selected:       return "selected";
        case ExplorerExpectationKind::Location:       return "location";
        case ExplorerExpectationKind::Entry:          return "entry";
        case ExplorerExpectationKind::MissingEntry:   return "missing-entry";
        case ExplorerExpectationKind::Operation:      return "operation";
        case ExplorerExpectationKind::StatusContains: return "status-contains";
    }
    return "selected";
}

inline std::string explorer_expectation_label(
        ExplorerExpectation const& expectation) {
    auto label = explorer_expectation_kind_name(expectation.kind) + ":"
        + expectation.value;
    if (expectation.kind == ExplorerExpectationKind::Operation
        && expectation.expects_operation_ok) {
        label += expectation.operation_ok ? ":ok" : ":fail";
    }
    return label;
}

inline ExplorerExpectationParseResult parsed_expectation(
        ExplorerExpectation expectation) {
    return ExplorerExpectationParseResult{
        .ok = true,
        .expectation = std::move(expectation),
    };
}

inline ExplorerExpectationParseResult expectation_parse_error(
        std::string message) {
    return ExplorerExpectationParseResult{
        .ok = false,
        .error = std::move(message),
    };
}

inline ExplorerExpectationParseResult parse_explorer_expectation(
        std::string_view raw) {
    auto text = trim(raw);
    auto separator = text.find(':');
    if (separator == std::string::npos)
        separator = text.find('=');
    if (text.empty() || separator == std::string::npos) {
        return expectation_parse_error(
            "expectation requires kind:value, such as selected:README.txt");
    }

    auto name = lower_copy(trim(std::string_view{text}.substr(0, separator)));
    auto value = trim(std::string_view{text}.substr(separator + 1));
    if (value.empty()) {
        return expectation_parse_error(
            "expectation '" + name + "' requires a value");
    }

    if (name == "selected" || name == "selection") {
        return parsed_expectation({
            .kind = ExplorerExpectationKind::Selected,
            .value = value,
        });
    }
    if (name == "location" || name == "loc") {
        return parsed_expectation({
            .kind = ExplorerExpectationKind::Location,
            .value = value,
        });
    }
    if (name == "entry" || name == "has-entry" || name == "has_entry") {
        return parsed_expectation({
            .kind = ExplorerExpectationKind::Entry,
            .value = value,
        });
    }
    if (name == "missing-entry" || name == "missing_entry"
        || name == "no-entry" || name == "no_entry") {
        return parsed_expectation({
            .kind = ExplorerExpectationKind::MissingEntry,
            .value = value,
        });
    }
    if (name == "status" || name == "status-contains"
        || name == "status_contains") {
        return parsed_expectation({
            .kind = ExplorerExpectationKind::StatusContains,
            .value = value,
        });
    }
    if (name == "operation" || name == "op") {
        auto op_kind = value;
        auto expected_ok = std::optional<bool>{};
        auto op_separator = op_kind.rfind(':');
        if (op_separator != std::string::npos) {
            auto suffix = lower_copy(trim(
                std::string_view{op_kind}.substr(op_separator + 1)));
            if (suffix == "ok" || suffix == "success" || suffix == "true") {
                expected_ok = true;
                op_kind = trim(std::string_view{op_kind}.substr(0, op_separator));
            } else if (suffix == "fail" || suffix == "failure"
                       || suffix == "false") {
                expected_ok = false;
                op_kind = trim(std::string_view{op_kind}.substr(0, op_separator));
            }
        }
        if (op_kind.empty()) {
            return expectation_parse_error(
                "expectation 'operation' requires an operation kind");
        }
        return parsed_expectation({
            .kind = ExplorerExpectationKind::Operation,
            .value = op_kind,
            .expects_operation_ok = expected_ok.has_value(),
            .operation_ok = expected_ok.value_or(false),
        });
    }

    return expectation_parse_error("unknown file explorer expectation: " + name);
}

inline std::optional<int> parse_positive_int(std::string_view text) {
    auto trimmed = trim(text);
    if (trimmed.empty())
        return std::nullopt;
    char* end = nullptr;
    long value = std::strtol(trimmed.c_str(), &end, 10);
    if (end != trimmed.c_str() + trimmed.size() || value <= 0 || value > 100000)
        return std::nullopt;
    return static_cast<int>(value);
}

inline std::optional<float> parse_positive_float(std::string_view text) {
    auto trimmed = trim(text);
    if (trimmed.empty())
        return std::nullopt;
    char* end = nullptr;
    float value = std::strtof(trimmed.c_str(), &end);
    if (end != trimmed.c_str() + trimmed.size() || value <= 0.0f || value > 16.0f)
        return std::nullopt;
    return value;
}

inline std::string viewport_value_label(int width, int height, float scale) {
    std::ostringstream out;
    out << width << "x" << height;
    if (scale != 1.0f)
        out << "@" << scale;
    return out.str();
}

inline SortMode sort_mode_from_name(std::string_view value) {
    auto name = lower_copy(trim(value));
    if (name == "kind")
        return SortMode::Kind;
    if (name == "size")
        return SortMode::Size;
    return SortMode::Name;
}

inline ExplorerInputParseResult parsed_input(ExplorerInput input) {
    return ExplorerInputParseResult{
        .ok = true,
        .input = std::move(input),
    };
}

inline ExplorerInputParseResult input_parse_error(std::string message) {
    return ExplorerInputParseResult{
        .ok = false,
        .error = std::move(message),
    };
}

inline ExplorerInputParseResult parse_viewport_input(std::string_view value) {
    auto text = trim(value);
    auto scale = 1.0f;
    auto scale_separator = text.find('@');
    if (scale_separator != std::string::npos) {
        auto parsed_scale = parse_positive_float(
            std::string_view{text}.substr(scale_separator + 1));
        if (!parsed_scale) {
            return input_parse_error(
                "input 'viewport' scale must be a positive number");
        }
        scale = *parsed_scale;
        text = trim(std::string_view{text}.substr(0, scale_separator));
    }

    auto separator = text.find('x');
    if (separator == std::string::npos)
        separator = text.find('X');
    if (separator == std::string::npos)
        separator = text.find(',');
    if (separator == std::string::npos) {
        return input_parse_error(
            "input 'viewport' requires WIDTHxHEIGHT or WIDTHxHEIGHT@SCALE");
    }

    auto width = parse_positive_int(std::string_view{text}.substr(0, separator));
    auto height = parse_positive_int(std::string_view{text}.substr(separator + 1));
    if (!width || !height) {
        return input_parse_error(
            "input 'viewport' width and height must be positive integers");
    }

    return parsed_input({
        .kind = ExplorerInputKind::Viewport,
        .value = viewport_value_label(*width, *height, scale),
        .viewport_width = *width,
        .viewport_height = *height,
        .viewport_scale = scale,
    });
}

inline ExplorerInputParseResult parse_explorer_input(std::string_view raw) {
    auto text = trim(raw);
    if (text.empty())
        return parsed_input({});

    auto separator = text.find(':');
    if (separator == std::string::npos)
        separator = text.find('=');
    auto name = lower_copy(trim(separator == std::string::npos
        ? std::string_view{text}
        : std::string_view{text}.substr(0, separator)));
    auto value = separator == std::string::npos
        ? std::string{}
        : trim(std::string_view{text}.substr(separator + 1));

    auto require_value = [&](ExplorerInputKind kind) -> ExplorerInputParseResult {
        if (value.empty()) {
            return input_parse_error(
                "input '" + name + "' requires a value");
        }
        return parsed_input(ExplorerInput{
            .kind = kind,
            .value = value,
        });
    };

    if (name == "noop")
        return parsed_input({});
    if (name == "location" || name == "loc"
        || name == "select-location") {
        return require_value(ExplorerInputKind::SelectLocation);
    }
    if (name == "select" || name == "entry" || name == "open"
        || name == "select-entry" || name == "read") {
        return require_value(ExplorerInputKind::SelectEntry);
    }
    if (name == "search")
        return require_value(ExplorerInputKind::Search);
    if (name == "viewport" || name == "resize" || name == "size") {
        if (value.empty())
            return input_parse_error("input 'viewport' requires a value");
        return parse_viewport_input(value);
    }
    if (name == "draft-name" || name == "file-name" || name == "name")
        return require_value(ExplorerInputKind::DraftName);
    if (name == "draft-folder" || name == "folder-name")
        return require_value(ExplorerInputKind::DraftFolderName);
    if (name == "draft-body" || name == "body" || name == "contents")
        return require_value(ExplorerInputKind::DraftBody);
    if (name == "create-file" || name == "touch")
        return parsed_input({.kind = ExplorerInputKind::CreateFile});
    if (name == "create-folder" || name == "mkdir")
        return parsed_input({.kind = ExplorerInputKind::CreateFolder});
    if (name == "delete" || name == "trash" || name == "delete-selected")
        return parsed_input({.kind = ExplorerInputKind::DeleteSelected});
    if (name == "duplicate" || name == "copy")
        return parsed_input({.kind = ExplorerInputKind::DuplicateSelected});
    if (name == "back")
        return parsed_input({.kind = ExplorerInputKind::GoBack});
    if (name == "forward")
        return parsed_input({.kind = ExplorerInputKind::GoForward});
    if (name == "up")
        return parsed_input({.kind = ExplorerInputKind::GoUp});
    if (name == "sort") {
        if (value.empty())
            return input_parse_error("input 'sort' requires name, kind, or size");
        auto lowered = lower_copy(value);
        if (lowered != "name" && lowered != "kind" && lowered != "size") {
            return input_parse_error(
                "input 'sort' accepts only name, kind, or size");
        }
        return parsed_input({
            .kind = ExplorerInputKind::Sort,
            .value = value,
            .sort_mode = sort_mode_from_name(value),
        });
    }
    if (name == "cycle-sort")
        return parsed_input({.kind = ExplorerInputKind::CycleSort});
    if (name == "reset")
        return parsed_input({.kind = ExplorerInputKind::Reset});
    if (name == "scenario")
        return require_value(ExplorerInputKind::Scenario);

    return input_parse_error("unknown file explorer input: " + name);
}

inline void record_operation(
        ExplorerState& state,
        std::string kind,
        std::string target,
        bool ok,
        std::string detail) {
    state.last_operation = OperationReceipt{
        .kind = std::move(kind),
        .target = std::move(target),
        .ok = ok,
        .detail = std::move(detail),
    };
}

inline std::string operation_label(OperationReceipt const& receipt) {
    if (receipt.kind.empty())
        return {};
    std::string label = "Operation: " + receipt.kind;
    label += receipt.ok ? " ok" : " failed";
    if (!receipt.target.empty())
        label += " - " + receipt.target;
    if (!receipt.detail.empty())
        label += " - " + receipt.detail;
    return label;
}

inline std::string relative_location(fs::path const& root, fs::path const& current) {
    std::error_code ec;
    auto const trash = trash_path(root);
    if (fs::equivalent(current, trash, ec))
        return "Trash";
    ec.clear();
    auto trash_rel = fs::relative(current, trash, ec);
    if (!ec && !trash_rel.empty() && trash_rel != ".") {
        auto text = trash_rel.generic_string();
        if (text != ".." && !text.starts_with("../"))
            return std::string("Trash/") + text;
    }
    ec.clear();
    auto rel = fs::relative(current, root, ec);
    if (ec || rel.empty() || rel == ".")
        return "Demo Root";
    return std::string("Demo Root/") + rel.generic_string();
}

inline bool same_path(fs::path const& a, fs::path const& b) {
    if (a.empty() || b.empty())
        return a.empty() == b.empty();

    std::error_code ec;
    if (fs::equivalent(a, b, ec))
        return true;

    auto lhs = a.lexically_normal().generic_string();
    auto rhs = b.lexically_normal().generic_string();
#if defined(_WIN32)
    lhs = lower_copy(std::move(lhs));
    rhs = lower_copy(std::move(rhs));
#endif
    return lhs == rhs;
}

inline void trim_history(std::vector<fs::path>& history) {
    constexpr std::size_t max_history = 32;
    if (history.size() > max_history) {
        history.erase(history.begin(),
                      history.begin()
                          + static_cast<std::ptrdiff_t>(
                              history.size() - max_history));
    }
}

inline void remember_current_for_back(ExplorerState& state) {
    if (state.current.empty())
        return;
    if (state.back_stack.empty()
        || !same_path(state.back_stack.back(), state.current)) {
        state.back_stack.push_back(state.current);
        trim_history(state.back_stack);
    }
    state.forward_stack.clear();
}

inline void open_directory(
        ExplorerState& state,
        fs::path next,
        std::string status,
        bool record_history = true) {
    if (!same_path(state.current, next)) {
        if (record_history)
            remember_current_for_back(state);
        state.current = std::move(next);
    }
    state.selected_name.clear();
    state.status = std::move(status);
}

inline bool matches_filter(std::string const& name, std::string const& filter) {
    auto needle = trim(filter);
    if (needle.empty())
        return true;
    return lower_copy(name).find(lower_copy(needle)) != std::string::npos;
}

inline bool protected_root_folder(std::string_view name) {
    return name == "Documents" || name == "Pictures" || name == "Shared"
        || name == ".Trash";
}

inline bool path_inside_root(fs::path const& root, fs::path const& path) {
    auto rel = path.lexically_relative(root).generic_string();
    if (rel.empty() || rel == ".")
        return false;
    return rel != ".." && !rel.starts_with("../");
}

inline bool deletable_directory(
        fs::path const& root,
        fs::path const& path) {
    std::error_code ec;
    if (!fs::is_directory(path, ec) || ec)
        return false;
    if (!path_inside_root(root, path) || same_path(root, path))
        return false;
    if (same_path(path.parent_path(), root)
        && protected_root_folder(path.filename().string())) {
        return false;
    }
    return true;
}

inline std::vector<Entry> list_entries(
        fs::path const& current,
        std::string const& filter,
        SortMode sort_mode = SortMode::Name) {
    std::vector<Entry> entries;
    std::error_code ec;
    fs::directory_iterator it(current, ec);
    if (ec)
        return entries;
    for (auto const& item : it) {
        auto name = item.path().filename().string();
        if (!name.empty() && name.front() == '.')
            continue;
        if (name.empty() || !matches_filter(name, filter))
            continue;
        Entry entry;
        entry.name = name;
        ec.clear();
        entry.folder = item.is_directory(ec);
        if (ec)
            continue;
        if (!entry.folder) {
            ec.clear();
            if (!item.is_regular_file(ec) || ec)
                continue;
            ec.clear();
            entry.size = item.file_size(ec);
            if (ec)
                entry.size = 0;
        }
        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end(), [sort_mode](Entry const& a, Entry const& b) {
        if (a.folder != b.folder)
            return a.folder && !b.folder;
        if (sort_mode == SortMode::Kind) {
            auto lhs_kind = lower_copy(entry_kind_label(a));
            auto rhs_kind = lower_copy(entry_kind_label(b));
            if (lhs_kind != rhs_kind)
                return lhs_kind < rhs_kind;
        } else if (sort_mode == SortMode::Size && !a.folder && !b.folder) {
            if (a.size != b.size)
                return a.size > b.size;
        }
        return lower_copy(a.name) < lower_copy(b.name);
    });
    return entries;
}

inline std::string read_preview(fs::path const& path) {
    std::error_code ec;
    if (!fs::exists(path, ec))
        return "No file selected.";
    if (fs::is_directory(path, ec))
        return "Open this folder to view its files.";
    auto size = fs::file_size(path, ec);
    if (ec)
        return "Unable to read file size.";
    if (size > 64 * 1024)
        return "Preview skipped because the file is larger than 64 KB.";

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return "Unable to open file for preview.";
    std::ostringstream buffer;
    buffer << in.rdbuf();
    auto text = buffer.str();
    for (char& ch : text) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (ch == '\n' || ch == '\t')
            continue;
        if (uch < 32 || uch == 127)
            ch = ' ';
    }
    if (text.empty())
        return "(empty file)";
    if (text.size() > 1800)
        text = text.substr(0, 1800) + "\n...";
    return text;
}

inline Snapshot snapshot(ExplorerState const& state) {
    Snapshot out;
    out.root = state.root;
    out.current = state.current;
    out.relative_location = relative_location(state.root, state.current);
    out.can_go_back = !state.back_stack.empty();
    out.can_go_forward = !state.forward_stack.empty();
    std::error_code ec;
    out.can_create_file = fs::is_directory(state.current, ec) && !ec;
    out.can_create_folder = out.can_create_file;
    out.entries = list_entries(state.current, state.search, state.sort_mode);
    out.operation_label = operation_label(state.last_operation);
    out.sort_label = "Sort: " + sort_mode_label(state.sort_mode);
    for (auto const& entry : out.entries) {
        if (entry.folder)
            ++out.folder_count;
        else
            ++out.file_count;
        if (entry.name == state.selected_name) {
            out.selected = entry;
            out.has_selection = true;
        }
    }
    out.item_summary = std::to_string(out.file_count) + " files";
    if (out.folder_count > 0)
        out.item_summary += ", " + std::to_string(out.folder_count) + " folders";
    if (out.has_selection) {
        auto const selected_path = state.current / out.selected.name;
        out.can_delete_selected = out.selected.folder
            ? deletable_directory(state.root, selected_path)
            : true;
        out.can_duplicate_selected = !out.selected.folder;
        out.can_preview_selected = true;
        out.selected_kind_label = entry_kind_label(out.selected);
        out.selected_size_label = entry_size_label(out.selected);
        out.selected_path_label =
            out.relative_location + "/" + out.selected.name;
        out.action_summary = "Selected " + out.selected.name + " - "
            + out.selected_kind_label + " - " + out.selected_size_label;
        out.preview = read_preview(state.current / out.selected.name);
    } else {
        out.action_summary = "Select a file to read, duplicate, or delete it.";
        out.preview = "Select a file to read its contents.";
    }
    return out;
}

inline ExplorerState make_state(std::string_view profile) {
    ExplorerState state;
    state.root = ensure_demo_tree(profile);
    state.current = state.root;
    state.selected_name = "README.txt";
    apply_default_viewport(state, profile);
    return state;
}

inline fs::path location_path(ExplorerState const& state, std::string_view id) {
    if (id == "documents")
        return state.root / "Documents";
    if (id == "pictures")
        return state.root / "Pictures";
    if (id == "shared")
        return state.root / "Shared";
    if (id == "trash")
        return trash_path(state.root);
    return state.root;
}

inline void select_location(ExplorerState& state, std::string_view id) {
    std::error_code ec;
    auto next = location_path(state, id);
    if (!fs::is_directory(next, ec)) {
        state.status = "Location is not available.";
        return;
    }
    open_directory(
        state,
        next,
        "Opened " + relative_location(state.root, next));
}

inline void go_up(ExplorerState& state) {
    if (state.current == state.root) {
        state.status = "Already at the demo root.";
        return;
    }
    auto next = state.current.parent_path();
    open_directory(
        state,
        next,
        "Moved up to " + relative_location(state.root, next));
}

inline void go_back(ExplorerState& state) {
    if (state.back_stack.empty()) {
        state.status = "No previous location.";
        return;
    }
    if (!state.current.empty()) {
        state.forward_stack.push_back(state.current);
        trim_history(state.forward_stack);
    }
    auto next = state.back_stack.back();
    state.back_stack.pop_back();
    open_directory(
        state,
        next,
        "Went back to " + relative_location(state.root, next),
        false);
}

inline void go_forward(ExplorerState& state) {
    if (state.forward_stack.empty()) {
        state.status = "No forward location.";
        return;
    }
    if (!state.current.empty()) {
        state.back_stack.push_back(state.current);
        trim_history(state.back_stack);
    }
    auto next = state.forward_stack.back();
    state.forward_stack.pop_back();
    open_directory(
        state,
        next,
        "Went forward to " + relative_location(state.root, next),
        false);
}

inline void set_sort_mode(ExplorerState& state, SortMode mode) {
    state.sort_mode = mode;
    state.status = "Sorted by " + sort_mode_label(mode);
}

inline void cycle_sort_mode(ExplorerState& state) {
    set_sort_mode(state, next_sort_mode(state.sort_mode));
}

inline void set_search_filter(ExplorerState& state, std::string text) {
    state.search = std::move(text);
    auto query = trim(state.search);
    if (!state.selected_name.empty()
        && !matches_filter(state.selected_name, state.search)) {
        state.selected_name.clear();
    }
    state.status = query.empty()
        ? "Search cleared."
        : "Searching for " + query;
}

inline void select_entry(ExplorerState& state, std::string const& name) {
    std::error_code ec;
    auto path = state.current / name;
    if (fs::is_directory(path, ec)) {
        open_directory(state, path, "Opened " + name);
        return;
    }
    ec.clear();
    if (fs::is_regular_file(path, ec)) {
        state.selected_name = name;
        state.status = "Selected " + name;
        record_operation(
            state,
            "file_read",
            name,
            true,
            "Selected " + name + " for preview");
        return;
    }
    state.status = "Entry is no longer available.";
    record_operation(
        state,
        "file_read",
        name,
        false,
        state.status);
}

inline fs::path unique_child_path(fs::path const& parent, std::string name) {
    std::error_code ec;
    auto candidate = parent / name;
    if (!fs::exists(candidate, ec))
        return candidate;
    auto stem = candidate.stem().string();
    auto ext = candidate.extension().string();
    for (int i = 2; i < 100; ++i) {
        auto next = parent / (stem + " " + std::to_string(i) + ext);
        if (!fs::exists(next, ec))
            return next;
    }
    return parent / (stem + " copy" + ext);
}

inline void create_file(ExplorerState& state) {
    auto name = sanitize_file_name(state.draft_name);
    auto path = unique_child_path(state.current, name);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        state.status = "Could not create " + name;
        record_operation(state, "file_create", name, false, state.status);
        return;
    }
    out << state.draft_body << "\n";
    if (!out) {
        state.status = "Could not write " + name;
        record_operation(state, "file_create", name, false, state.status);
        return;
    }
    state.selected_name = path.filename().string();
    state.status = "Created " + state.selected_name;
    record_operation(
        state,
        "file_create",
        state.selected_name,
        true,
        state.status);
}

inline void create_folder(ExplorerState& state) {
    auto name = sanitize_folder_name(state.draft_folder_name);
    auto path = unique_child_path(state.current, name);
    std::error_code ec;
    if (!fs::create_directory(path, ec) || ec) {
        state.status = "Could not create folder " + name;
        record_operation(state, "folder_create", name, false, state.status);
        return;
    }
    state.selected_name = path.filename().string();
    state.status = "Created folder " + state.selected_name;
    record_operation(
        state,
        "folder_create",
        state.selected_name,
        true,
        state.status);
}

inline void delete_selected(ExplorerState& state) {
    if (state.selected_name.empty()) {
        state.status = "Select a file or folder before deleting.";
        record_operation(state, "file_delete", {}, false, state.status);
        return;
    }
    auto path = state.current / state.selected_name;
    auto trash = trash_path(state.root);
    bool const deleting_from_trash = same_path(state.current, trash);
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        if (!deletable_directory(state.root, path)) {
            state.status = "Only sandbox folders can be deleted.";
            record_operation(
                state,
                "folder_delete",
                state.selected_name,
                false,
                state.status);
            return;
        }
        ec.clear();
        if (deleting_from_trash) {
            auto const removed = fs::remove_all(path, ec);
            if (ec || removed == 0) {
                state.status = "Could not delete folder " + state.selected_name;
                record_operation(
                    state,
                    "folder_delete",
                    state.selected_name,
                    false,
                    state.status);
                return;
            }
            auto deleted = state.selected_name;
            state.status = "Deleted folder " + state.selected_name + " from Trash";
            state.selected_name.clear();
            record_operation(
                state,
                "folder_delete",
                deleted,
                true,
                state.status);
            return;
        }
        fs::create_directories(trash, ec);
        ec.clear();
        auto target = unique_child_path(trash, state.selected_name);
        fs::rename(path, target, ec);
        if (ec) {
            state.status = "Could not move folder " + state.selected_name + " to Trash";
            record_operation(
                state,
                "folder_delete",
                state.selected_name,
                false,
                state.status);
            return;
        }
        auto moved = state.selected_name;
        state.status = "Moved folder " + state.selected_name + " to Trash";
        state.selected_name.clear();
        record_operation(
            state,
            "folder_delete",
            moved,
            true,
            state.status);
        return;
    }
    ec.clear();
    if (!fs::is_regular_file(path, ec)) {
        state.status = "Only files or sandbox folders can be deleted.";
        record_operation(
            state,
            "file_delete",
            state.selected_name,
            false,
            state.status);
        return;
    }
    if (deleting_from_trash) {
        if (!fs::remove(path, ec) || ec) {
            state.status = "Could not delete " + state.selected_name;
            record_operation(
                state,
                "file_delete",
                state.selected_name,
                false,
                state.status);
            return;
        }
        auto deleted = state.selected_name;
        state.status = "Deleted " + state.selected_name + " from Trash";
        state.selected_name.clear();
        record_operation(
            state,
            "file_delete",
            deleted,
            true,
            state.status);
        return;
    }
    fs::create_directories(trash, ec);
    ec.clear();
    auto target = unique_child_path(trash, state.selected_name);
    fs::rename(path, target, ec);
    if (ec) {
        state.status = "Could not delete " + state.selected_name;
        record_operation(
            state,
            "file_delete",
            state.selected_name,
            false,
            state.status);
        return;
    }
    auto deleted = state.selected_name;
    state.status = "Moved " + state.selected_name + " to Trash";
    state.selected_name.clear();
    record_operation(
        state,
        "file_delete",
        deleted,
        true,
        state.status);
}

inline void duplicate_selected(ExplorerState& state) {
    if (state.selected_name.empty()) {
        state.status = "Select a file before duplicating.";
        record_operation(state, "file_duplicate", {}, false, state.status);
        return;
    }
    auto source = state.current / state.selected_name;
    std::error_code ec;
    if (!fs::is_regular_file(source, ec)) {
        state.status = "Only files can be duplicated in this demo.";
        record_operation(
            state,
            "file_duplicate",
            state.selected_name,
            false,
            state.status);
        return;
    }
    auto candidate_name =
        source.stem().string() + " copy" + source.extension().string();
    auto target = unique_child_path(state.current, candidate_name);
    ec.clear();
    if (!fs::copy_file(source, target, fs::copy_options::none, ec) || ec) {
        state.status = "Could not duplicate " + state.selected_name;
        record_operation(
            state,
            "file_duplicate",
            state.selected_name,
            false,
            state.status);
        return;
    }
    auto previous = state.selected_name;
    state.selected_name = target.filename().string();
    state.status = "Duplicated " + previous + " to " + state.selected_name;
    record_operation(
        state,
        "file_duplicate",
        state.selected_name,
        true,
        state.status);
}

inline void reset_demo_tree(ExplorerState& state, std::string_view profile) {
    std::error_code ec;
    if (!state.root.empty())
        fs::remove_all(state.root, ec);
    state.root = ensure_demo_tree(profile);
    state.current = state.root;
    state.selected_name = "README.txt";
    state.search.clear();
    state.draft_name = "New Note.txt";
    state.draft_folder_name = "New Folder";
    state.draft_body = "Created from the phenotype file explorer demo.";
    state.back_stack.clear();
    state.forward_stack.clear();
    state.last_operation = {};
    state.sort_mode = SortMode::Name;
    state.status = "Demo files reset.";
}

inline void remove_regular_file_if_present(fs::path const& path) {
    std::error_code ec;
    if (fs::is_regular_file(path, ec)) {
        ec.clear();
        fs::remove(path, ec);
    }
}

inline void remove_directory_if_present(fs::path const& path) {
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        ec.clear();
        fs::remove_all(path, ec);
    }
}

inline void apply_startup_scenario(
        ExplorerState& state,
        std::string_view scenario) {
    auto name = lower_copy(trim(scenario));
    if (name.empty() || name == "default")
        return;

    if (name == "created-preview" || name == "create") {
        select_location(state, "root");
        remove_regular_file_if_present(state.current / "Action Note.txt");
        state.draft_name = "Action Note";
        state.draft_body =
            "Created from artifact scenario.\n"
            "This note proves the file explorer can create and read a file.";
        create_file(state);
        state.mobile_tab = 1;
        return;
    }

    if (name == "deleted-file" || name == "delete") {
        select_location(state, "root");
        remove_regular_file_if_present(state.current / "Delete Me.txt");
        remove_regular_file_if_present(trash_path(state.root) / "Delete Me.txt");
        state.draft_name = "Delete Me";
        state.draft_body =
            "This temporary file should be moved to Trash before the artifact frame.";
        create_file(state);
        delete_selected(state);
        state.mobile_tab = 0;
        return;
    }

    if (name == "trash-view" || name == "trash") {
        select_location(state, "root");
        remove_regular_file_if_present(state.current / "Trash Note.txt");
        remove_regular_file_if_present(trash_path(state.root) / "Trash Note.txt");
        state.draft_name = "Trash Note";
        state.draft_body =
            "This note proves delete moves sandbox files to Trash.";
        create_file(state);
        delete_selected(state);
        select_location(state, "trash");
        state.mobile_tab = 0;
        return;
    }

    if (name == "created-folder" || name == "folder-create") {
        select_location(state, "root");
        remove_directory_if_present(state.current / "Review Folder");
        state.draft_folder_name = "Review Folder";
        create_folder(state);
        state.mobile_tab = 1;
        return;
    }

    if (name == "deleted-folder" || name == "folder-delete") {
        select_location(state, "root");
        remove_directory_if_present(state.current / "Trash Folder");
        remove_directory_if_present(trash_path(state.root) / "Trash Folder");
        state.draft_folder_name = "Trash Folder";
        create_folder(state);
        write_file_if_missing(
            state.current / state.selected_name / "Nested Note.txt",
            "Nested file proves folders move to Trash recursively.\n");
        delete_selected(state);
        state.mobile_tab = 0;
        return;
    }

    if (name == "duplicated-file" || name == "duplicate") {
        select_location(state, "root");
        remove_regular_file_if_present(state.current / "README copy.txt");
        remove_regular_file_if_present(state.current / "README copy 2.txt");
        select_entry(state, "README.txt");
        duplicate_selected(state);
        state.mobile_tab = 1;
        return;
    }

    if (name == "documents-preview" || name == "read") {
        select_location(state, "documents");
        select_entry(state, "Project Notes.txt");
        state.mobile_tab = 1;
        return;
    }

    if (name == "history-forward" || name == "history") {
        select_location(state, "documents");
        go_back(state);
        go_forward(state);
        state.mobile_tab = 0;
        return;
    }

    if (name == "sorted-kind" || name == "sort-kind") {
        set_sort_mode(state, SortMode::Kind);
        state.mobile_tab = 0;
        return;
    }

    if (name == "search-active" || name == "search") {
        select_location(state, "root");
        set_search_filter(state, "Screen");
        state.mobile_tab = 0;
        return;
    }

    state.status = "Unknown startup scenario: " + std::string(scenario);
}

inline void apply_explorer_input(
        ExplorerState& state,
        ExplorerInput const& input,
        std::string_view profile) {
    switch (input.kind) {
        case ExplorerInputKind::Noop:
            state.status = "No input applied.";
            return;
        case ExplorerInputKind::SelectLocation:
            select_location(state, input.value);
            return;
        case ExplorerInputKind::SelectEntry:
            select_entry(state, input.value);
            return;
        case ExplorerInputKind::Search:
            set_search_filter(state, input.value);
            return;
        case ExplorerInputKind::Viewport:
            state.viewport_width = input.viewport_width;
            state.viewport_height = input.viewport_height;
            state.viewport_scale = input.viewport_scale;
            state.status = "Viewport set to " + input.value;
            return;
        case ExplorerInputKind::DraftName:
            state.draft_name = input.value;
            state.status = "Draft file name set.";
            return;
        case ExplorerInputKind::DraftFolderName:
            state.draft_folder_name = input.value;
            state.status = "Draft folder name set.";
            return;
        case ExplorerInputKind::DraftBody:
            state.draft_body = input.value;
            state.status = "Draft body set.";
            return;
        case ExplorerInputKind::CreateFile:
            create_file(state);
            return;
        case ExplorerInputKind::CreateFolder:
            create_folder(state);
            return;
        case ExplorerInputKind::DeleteSelected:
            delete_selected(state);
            return;
        case ExplorerInputKind::DuplicateSelected:
            duplicate_selected(state);
            return;
        case ExplorerInputKind::GoBack:
            go_back(state);
            return;
        case ExplorerInputKind::GoForward:
            go_forward(state);
            return;
        case ExplorerInputKind::GoUp:
            go_up(state);
            return;
        case ExplorerInputKind::Sort:
            set_sort_mode(state, input.sort_mode);
            return;
        case ExplorerInputKind::CycleSort:
            cycle_sort_mode(state);
            return;
        case ExplorerInputKind::Reset:
            reset_demo_tree(state, profile);
            return;
        case ExplorerInputKind::Scenario:
            apply_startup_scenario(state, input.value);
            return;
    }
}

inline ExplorerInputTrace explorer_input_trace(
        ExplorerState const& state,
        ExplorerInput const& input,
        std::string_view profile) {
    auto snap = snapshot(state);
    return ExplorerInputTrace{
        .input = input,
        .chrome = explorer_chrome_metrics(state, profile),
        .status = state.status,
        .relative_location = snap.relative_location,
        .selected_name = state.selected_name,
        .operation_label = snap.operation_label,
        .operation = state.last_operation,
        .visible_entries = snap.entries.size(),
        .has_selection = snap.has_selection,
    };
}

inline ExplorerDriveResult drive_explorer(
        std::string_view profile,
        std::span<ExplorerInput const> inputs) {
    ExplorerDriveResult result;
    result.profile = profile.empty() ? "desktop" : std::string{profile};
    result.state = make_state(result.profile);
    result.trace.reserve(inputs.size());
    for (auto const& input : inputs) {
        apply_explorer_input(result.state, input, result.profile);
        result.trace.push_back(explorer_input_trace(
            result.state,
            input,
            result.profile));
    }
    result.snapshot = snapshot(result.state);
    result.chrome = explorer_chrome_metrics(result.state, result.profile);
    return result;
}

inline bool snapshot_has_entry(Snapshot const& snapshot, std::string_view name) {
    return std::ranges::any_of(snapshot.entries, [&](Entry const& entry) {
        return entry.name == name;
    });
}

inline ExplorerExpectationResult check_explorer_expectation(
        ExplorerDriveResult const& result,
        ExplorerExpectation const& expectation) {
    auto const& snap = result.snapshot;
    auto checked = ExplorerExpectationResult{
        .expectation = expectation,
    };
    switch (expectation.kind) {
        case ExplorerExpectationKind::Selected:
            checked.actual = snap.has_selection ? snap.selected.name : "<none>";
            checked.ok = snap.has_selection && snap.selected.name == expectation.value;
            checked.detail = checked.ok
                ? "selected entry matched"
                : "selected entry did not match";
            return checked;
        case ExplorerExpectationKind::Location:
            checked.actual = snap.relative_location;
            checked.ok = snap.relative_location == expectation.value;
            checked.detail = checked.ok
                ? "location matched"
                : "relative location did not match";
            return checked;
        case ExplorerExpectationKind::Entry:
            checked.actual = snapshot_has_entry(snap, expectation.value)
                ? expectation.value
                : "<missing>";
            checked.ok = checked.actual == expectation.value;
            checked.detail = checked.ok
                ? "entry was visible"
                : "entry was not visible in the final snapshot";
            return checked;
        case ExplorerExpectationKind::MissingEntry:
            checked.actual = snapshot_has_entry(snap, expectation.value)
                ? expectation.value
                : "<missing>";
            checked.ok = checked.actual == "<missing>";
            checked.detail = checked.ok
                ? "entry was absent"
                : "entry was unexpectedly visible in the final snapshot";
            return checked;
        case ExplorerExpectationKind::Operation:
            checked.actual = result.state.last_operation.kind.empty()
                ? "<none>"
                : result.state.last_operation.kind
                    + (result.state.last_operation.ok ? ":ok" : ":fail");
            checked.ok = result.state.last_operation.kind == expectation.value;
            if (expectation.expects_operation_ok) {
                checked.ok = checked.ok
                    && result.state.last_operation.ok == expectation.operation_ok;
            }
            checked.detail = checked.ok
                ? "last operation matched"
                : "last operation did not match";
            return checked;
        case ExplorerExpectationKind::StatusContains:
            checked.actual = result.state.status;
            checked.ok = result.state.status.find(expectation.value)
                != std::string::npos;
            checked.detail = checked.ok
                ? "status contained expected text"
                : "status did not contain expected text";
            return checked;
    }
    checked.actual = "<unknown>";
    checked.detail = "unknown expectation";
    return checked;
}

inline std::vector<ExplorerExpectationResult> check_explorer_expectations(
        ExplorerDriveResult const& result,
        std::span<ExplorerExpectation const> expectations) {
    auto checked = std::vector<ExplorerExpectationResult>{};
    checked.reserve(expectations.size());
    for (auto const& expectation : expectations)
        checked.push_back(check_explorer_expectation(result, expectation));
    return checked;
}

inline bool explorer_expectations_ok(
        std::span<ExplorerExpectationResult const> expectations) {
    return std::ranges::all_of(expectations, [](auto const& expectation) {
        return expectation.ok;
    });
}

inline std::string entry_label(Entry const& entry) {
    std::string label = entry.folder ? "[Folder] " : "[File] ";
    label += entry.name;
    return label;
}

struct ExplorerLabels {
    std::string title = "Recents";
    std::string mobile_title = "Files";
    std::string sidebar_recents = "Recents";
    std::string sidebar_shared = "Shared";
    std::string favorites = "Favorites";
    std::string locations = "Locations";
    std::string applications = "Applications";
    std::string desktop = "Desktop";
    std::string documents = "Documents";
    std::string pictures = "Pictures";
    std::string downloads = "Downloads";
    std::string icloud_drive = "iCloud Drive";
    std::string home = "kakao";
    std::string airdrop = "AirDrop";
    std::string trash = "Trash";
    std::string search_placeholder = "Search";
    std::string status_ready = "Ready";
    std::string tab_browse = "Browse";
    std::string tab_preview = "Preview";
    std::string tab_create = "Create";
    std::string create_file = "New File";
    std::string create_folder = "New Folder";
    std::string duplicate = "Duplicate";
    std::string duplicate_file = "Duplicate File";
    std::string delete_action = "Delete";
    std::string delete_selected = "Delete Selected";
    std::string preview = "Preview";
    std::string root = "Root";
    std::string docs = "Docs";
    std::string pics = "Pics";
    std::string back = "Back";
    std::string forward = "Forward";
    std::string up = "Up";
    std::string sort = "Sort";
    std::string name = "Name";
    std::string kind = "Kind";
    std::string size = "Size";
    std::string no_matching_files = "No matching files.";
    std::string select_file_to_preview = "Select a file to preview.";
    std::string select_file_from_browse = "Select a file from Browse.";
    std::string create_hint =
        "Files and folders are written only inside the demo root.";
    std::string file_name = "File name";
    std::string contents = "Contents";
    std::string folder_name = "Folder name";
    std::string reset_demo_files = "Reset Demo Files";
    std::string status = "Status";
};

inline void add_locale_strings(
        phenotype::LocaleDescriptor& locale,
        std::initializer_list<std::pair<std::string_view, std::string_view>> values) {
    for (auto const& [key, value] : values) {
        locale.strings.push_back({
            .key = std::string{key},
            .value = std::string{value},
        });
    }
}

inline phenotype::ResourceCatalog file_explorer_resource_catalog(
        std::string_view profile) {
    phenotype::ResourceCatalog catalog;
    bool const mobile = profile == "mobile";
    catalog.application.id = mobile
        ? "com.misut.phenotype.examples.file-explorer-mobile"
        : "com.misut.phenotype.examples.file-explorer-desktop";
    catalog.application.display_name = mobile
        ? "Phenotype Mobile File Explorer"
        : "Phenotype File Explorer";
    catalog.application.version = "0.1.0";
    catalog.application.entry = mobile
        ? "file_explorer_mobile"
        : "file_explorer_desktop";
    catalog.application.platforms = {"macos", "windows"};
    catalog.default_locale = "en";
    catalog.default_font_family = "Pretendard";
    catalog.assets.push_back({
        .name = "app.icon",
        .source = "assets/file-explorer-icon.txt",
        .content_type = "text/plain",
        .preload = true,
    });

    phenotype::LocaleDescriptor en{
        .tag = "en",
        .source = "locales/en.toml",
    };
    add_locale_strings(en, {
        {"app.title", "Recents"},
        {"app.mobile_title", "Files"},
        {"app.sidebar_recents", "Recents"},
        {"app.sidebar_shared", "Shared"},
        {"app.favorites", "Favorites"},
        {"app.locations", "Locations"},
        {"app.applications", "Applications"},
        {"app.desktop", "Desktop"},
        {"app.documents", "Documents"},
        {"app.pictures", "Pictures"},
        {"app.downloads", "Downloads"},
        {"app.icloud_drive", "iCloud Drive"},
        {"app.home", "kakao"},
        {"app.airdrop", "AirDrop"},
        {"app.trash", "Trash"},
        {"app.search_placeholder", "Search"},
        {"app.status_ready", "Ready"},
        {"app.tab_browse", "Browse"},
        {"app.tab_preview", "Preview"},
        {"app.tab_create", "Create"},
        {"actions.create_file", "New File"},
        {"actions.create_folder", "New Folder"},
        {"actions.duplicate", "Duplicate"},
        {"actions.duplicate_file", "Duplicate File"},
        {"actions.delete", "Delete"},
        {"actions.delete_selected", "Delete Selected"},
        {"actions.preview", "Preview"},
        {"nav.root", "Root"},
        {"nav.docs", "Docs"},
        {"nav.pics", "Pics"},
        {"nav.back", "Back"},
        {"nav.forward", "Forward"},
        {"nav.up", "Up"},
        {"nav.sort", "Sort"},
        {"table.name", "Name"},
        {"table.kind", "Kind"},
        {"table.size", "Size"},
        {"state.no_matching_files", "No matching files."},
        {"state.select_file_to_preview", "Select a file to preview."},
        {"state.select_file_from_browse", "Select a file from Browse."},
        {"create.hint", "Files and folders are written only inside the demo root."},
        {"create.file_name", "File name"},
        {"create.contents", "Contents"},
        {"create.folder_name", "Folder name"},
        {"create.reset_demo_files", "Reset Demo Files"},
        {"status.title", "Status"},
    });
    phenotype::LocaleDescriptor ko{
        .tag = "ko",
        .source = "locales/ko.toml",
        .fallback = {"en"},
    };
    add_locale_strings(ko, {
        {"app.title", "최근 항목"},
        {"app.mobile_title", "파일"},
        {"app.sidebar_recents", "최근 항목"},
        {"app.sidebar_shared", "공유"},
        {"app.favorites", "즐겨찾기"},
        {"app.locations", "위치"},
        {"app.applications", "응용 프로그램"},
        {"app.desktop", "데스크탑"},
        {"app.documents", "문서"},
        {"app.pictures", "사진"},
        {"app.downloads", "다운로드"},
        {"app.icloud_drive", "iCloud Drive"},
        {"app.home", "kakao"},
        {"app.airdrop", "AirDrop"},
        {"app.trash", "휴지통"},
        {"app.search_placeholder", "검색"},
        {"app.status_ready", "준비됨"},
        {"app.tab_browse", "둘러보기"},
        {"app.tab_preview", "미리보기"},
        {"app.tab_create", "만들기"},
        {"actions.create_file", "새 파일"},
        {"actions.create_folder", "새 폴더"},
        {"actions.duplicate", "복제"},
        {"actions.duplicate_file", "파일 복제"},
        {"actions.delete", "삭제"},
        {"actions.delete_selected", "선택 항목 삭제"},
        {"actions.preview", "미리보기"},
        {"nav.root", "루트"},
        {"nav.docs", "문서"},
        {"nav.pics", "사진"},
        {"nav.back", "뒤로"},
        {"nav.forward", "앞으로"},
        {"nav.up", "위로"},
        {"nav.sort", "정렬"},
        {"table.name", "이름"},
        {"table.kind", "종류"},
        {"table.size", "크기"},
        {"state.no_matching_files", "일치하는 파일이 없습니다."},
        {"state.select_file_to_preview", "미리 볼 파일을 선택하세요."},
        {"state.select_file_from_browse", "둘러보기에서 파일을 선택하세요."},
        {"create.hint", "파일과 폴더는 데모 루트 안에만 기록됩니다."},
        {"create.file_name", "파일 이름"},
        {"create.contents", "내용"},
        {"create.folder_name", "폴더 이름"},
        {"create.reset_demo_files", "데모 파일 초기화"},
        {"status.title", "상태"},
    });
    catalog.locales = {std::move(en), std::move(ko)};
    catalog.fonts.push_back({
        .family = "Pretendard",
        .source = "fonts/pretendard.alias.toml",
        .fallback = {"system-ui", "Apple SD Gothic Neo", "Segoe UI", "Noto Sans CJK"},
    });
    catalog.debug.artifact_manifest = "artifact_manifest.json";
    catalog.debug.probe_scene = mobile
        ? "mobile-file-workflow"
        : "finder-style-startup";
    catalog.debug.verifier = "tools/verify_file_explorer_artifacts.sh";
    return catalog;
}

inline phenotype::ResourceCatalog file_explorer_resource_catalog_from_package_texts(
        std::string_view profile,
        std::string_view manifest_text,
        std::span<PackageResourceText const> locale_texts) {
    auto fallback = file_explorer_resource_catalog(profile);
    if (trim(manifest_text).empty())
        return fallback;

    auto parsed = phenotype::parse_resource_manifest(manifest_text);
    if (!parsed.application_section || !parsed.resources_section)
        return fallback;

    auto catalog = std::move(parsed.catalog);
    if (catalog.default_locale.empty())
        catalog.default_locale = fallback.default_locale;
    if (catalog.default_font_family.empty())
        catalog.default_font_family = fallback.default_font_family;
    for (auto& locale : catalog.locales) {
        auto found = std::ranges::find_if(
            locale_texts,
            [&](PackageResourceText const& text) {
                return text.source == locale.source;
            });
        if (found != locale_texts.end()) {
            locale.strings =
                phenotype::parse_resource_locale_strings(found->text);
        }
    }
    return catalog;
}

inline std::string localized_or(
        phenotype::ResourceCatalog const& catalog,
        std::string_view locale,
        std::string_view key,
        std::string_view fallback) {
    if (auto value = phenotype::localized_string(catalog, key, locale))
        return value->value;
    return std::string{fallback};
}

inline ExplorerLabels file_explorer_labels(
        std::string_view locale,
        phenotype::ResourceCatalog const& catalog) {
    ExplorerLabels labels;
    auto get = [&](std::string_view key, std::string_view fallback) {
        return localized_or(catalog, locale, key, fallback);
    };
    labels.title = get("app.title", labels.title);
    labels.mobile_title = get("app.mobile_title", labels.mobile_title);
    labels.sidebar_recents = get("app.sidebar_recents", labels.sidebar_recents);
    labels.sidebar_shared = get("app.sidebar_shared", labels.sidebar_shared);
    labels.favorites = get("app.favorites", labels.favorites);
    labels.locations = get("app.locations", labels.locations);
    labels.applications = get("app.applications", labels.applications);
    labels.desktop = get("app.desktop", labels.desktop);
    labels.documents = get("app.documents", labels.documents);
    labels.pictures = get("app.pictures", labels.pictures);
    labels.downloads = get("app.downloads", labels.downloads);
    labels.icloud_drive = get("app.icloud_drive", labels.icloud_drive);
    labels.home = get("app.home", labels.home);
    labels.airdrop = get("app.airdrop", labels.airdrop);
    labels.trash = get("app.trash", labels.trash);
    labels.search_placeholder = get("app.search_placeholder", labels.search_placeholder);
    labels.status_ready = get("app.status_ready", labels.status_ready);
    labels.tab_browse = get("app.tab_browse", labels.tab_browse);
    labels.tab_preview = get("app.tab_preview", labels.tab_preview);
    labels.tab_create = get("app.tab_create", labels.tab_create);
    labels.create_file = get("actions.create_file", labels.create_file);
    labels.create_folder = get("actions.create_folder", labels.create_folder);
    labels.duplicate = get("actions.duplicate", labels.duplicate);
    labels.duplicate_file = get("actions.duplicate_file", labels.duplicate_file);
    labels.delete_action = get("actions.delete", labels.delete_action);
    labels.delete_selected = get("actions.delete_selected", labels.delete_selected);
    labels.preview = get("actions.preview", labels.preview);
    labels.root = get("nav.root", labels.root);
    labels.docs = get("nav.docs", labels.docs);
    labels.pics = get("nav.pics", labels.pics);
    labels.back = get("nav.back", labels.back);
    labels.forward = get("nav.forward", labels.forward);
    labels.up = get("nav.up", labels.up);
    labels.sort = get("nav.sort", labels.sort);
    labels.name = get("table.name", labels.name);
    labels.kind = get("table.kind", labels.kind);
    labels.size = get("table.size", labels.size);
    labels.no_matching_files = get("state.no_matching_files", labels.no_matching_files);
    labels.select_file_to_preview = get(
        "state.select_file_to_preview",
        labels.select_file_to_preview);
    labels.select_file_from_browse = get(
        "state.select_file_from_browse",
        labels.select_file_from_browse);
    labels.create_hint = get("create.hint", labels.create_hint);
    labels.file_name = get("create.file_name", labels.file_name);
    labels.contents = get("create.contents", labels.contents);
    labels.folder_name = get("create.folder_name", labels.folder_name);
    labels.reset_demo_files = get("create.reset_demo_files", labels.reset_demo_files);
    labels.status = get("status.title", labels.status);
    return labels;
}

inline ExplorerLabels file_explorer_labels(
        std::string_view locale,
        std::string_view profile) {
    return file_explorer_labels(locale, file_explorer_resource_catalog(profile));
}

} // namespace file_explorer_demo

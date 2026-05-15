module;
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

export module file_explorer_shared;

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
    return name == "Documents" || name == "Pictures" || name == "Shared";
}

inline bool path_inside_root(fs::path const& root, fs::path const& path) {
    auto rel = path.lexically_relative(root).generic_string();
    if (rel.empty() || rel == ".")
        return false;
    return rel != ".." && !rel.starts_with("../");
}

inline bool deletable_empty_directory(
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
    ec.clear();
    return fs::is_empty(path, ec) && !ec;
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
            ? deletable_empty_directory(state.root, selected_path)
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
    return state;
}

inline fs::path location_path(ExplorerState const& state, std::string_view id) {
    if (id == "documents")
        return state.root / "Documents";
    if (id == "pictures")
        return state.root / "Pictures";
    if (id == "shared")
        return state.root / "Shared";
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
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        if (!deletable_empty_directory(state.root, path)) {
            state.status = "Only empty demo folders can be deleted.";
            record_operation(
                state,
                "folder_delete",
                state.selected_name,
                false,
                state.status);
            return;
        }
        ec.clear();
        if (!fs::remove(path, ec) || ec) {
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
        state.status = "Deleted folder " + state.selected_name;
        state.selected_name.clear();
        record_operation(
            state,
            "folder_delete",
            deleted,
            true,
            state.status);
        return;
    }
    ec.clear();
    if (!fs::is_regular_file(path, ec)) {
        state.status = "Only files or empty demo folders can be deleted.";
        record_operation(
            state,
            "file_delete",
            state.selected_name,
            false,
            state.status);
        return;
    }
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
    state.status = "Deleted " + state.selected_name;
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
        remove_regular_file_if_present(state.current / "Delete Me.txt");
        state.draft_name = "Delete Me";
        state.draft_body =
            "This temporary file should be deleted before the artifact frame.";
        create_file(state);
        delete_selected(state);
        state.mobile_tab = 0;
        return;
    }

    if (name == "created-folder" || name == "folder-create") {
        remove_directory_if_present(state.current / "Review Folder");
        state.draft_folder_name = "Review Folder";
        create_folder(state);
        state.mobile_tab = 1;
        return;
    }

    if (name == "deleted-folder" || name == "folder-delete") {
        remove_directory_if_present(state.current / "Trash Folder");
        state.draft_folder_name = "Trash Folder";
        create_folder(state);
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

inline std::string entry_label(Entry const& entry) {
    std::string label = entry.folder ? "[Folder] " : "[File] ";
    label += entry.name;
    return label;
}

} // namespace file_explorer_demo

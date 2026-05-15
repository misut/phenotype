#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace file_explorer_demo {

namespace fs = std::filesystem;

struct Entry {
    std::string name;
    bool folder = false;
    std::uintmax_t size = 0;
};

struct Snapshot {
    fs::path root;
    fs::path current;
    std::string relative_location;
    std::vector<Entry> entries;
    Entry selected{};
    bool has_selection = false;
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
    std::string draft_body = "Created from the phenotype file explorer demo.";
    std::string status = "Ready";
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

inline std::string relative_location(fs::path const& root, fs::path const& current) {
    std::error_code ec;
    auto rel = fs::relative(current, root, ec);
    if (ec || rel.empty() || rel == ".")
        return "Demo Root";
    return std::string("Demo Root/") + rel.generic_string();
}

inline bool matches_filter(std::string const& name, std::string const& filter) {
    auto needle = trim(filter);
    if (needle.empty())
        return true;
    return lower_copy(name).find(lower_copy(needle)) != std::string::npos;
}

inline std::vector<Entry> list_entries(
        fs::path const& current,
        std::string const& filter) {
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
    std::sort(entries.begin(), entries.end(), [](Entry const& a, Entry const& b) {
        if (a.folder != b.folder)
            return a.folder && !b.folder;
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
    out.entries = list_entries(state.current, state.search);
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
    if (out.has_selection) {
        out.preview = read_preview(state.current / out.selected.name);
    } else {
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
    state.current = next;
    state.selected_name.clear();
    state.status = "Opened " + relative_location(state.root, state.current);
}

inline void go_up(ExplorerState& state) {
    if (state.current == state.root) {
        state.status = "Already at the demo root.";
        return;
    }
    state.current = state.current.parent_path();
    state.selected_name.clear();
    state.status = "Moved up to " + relative_location(state.root, state.current);
}

inline void select_entry(ExplorerState& state, std::string const& name) {
    std::error_code ec;
    auto path = state.current / name;
    if (fs::is_directory(path, ec)) {
        state.current = path;
        state.selected_name.clear();
        state.status = "Opened " + name;
        return;
    }
    ec.clear();
    if (fs::is_regular_file(path, ec)) {
        state.selected_name = name;
        state.status = "Selected " + name;
        return;
    }
    state.status = "Entry is no longer available.";
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
        return;
    }
    out << state.draft_body << "\n";
    state.selected_name = path.filename().string();
    state.status = "Created " + state.selected_name;
}

inline void delete_selected(ExplorerState& state) {
    if (state.selected_name.empty()) {
        state.status = "Select a file before deleting.";
        return;
    }
    auto path = state.current / state.selected_name;
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        state.status = "Only files can be deleted in this demo.";
        return;
    }
    if (!fs::remove(path, ec) || ec) {
        state.status = "Could not delete " + state.selected_name;
        return;
    }
    state.status = "Deleted " + state.selected_name;
    state.selected_name.clear();
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
    state.draft_body = "Created from the phenotype file explorer demo.";
    state.status = "Demo files reset.";
}

inline std::string entry_label(Entry const& entry) {
    std::string label = entry.folder ? "[Folder] " : "[File] ";
    label += entry.name;
    return label;
}

} // namespace file_explorer_demo

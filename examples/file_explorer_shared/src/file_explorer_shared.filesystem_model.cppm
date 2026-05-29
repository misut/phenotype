module;
#include <algorithm>
#include <array>
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

export module file_explorer_shared:filesystem_model;

import json;
import phenotype.icon_catalog;
import phenotype.resources;
import phenotype.theme_contract;
import :model_types;
import :desktop_metrics_and_symbols;
import :viewport_and_focus_helpers;
import :chrome_and_geometry;

export namespace file_explorer_demo {
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

inline bool svg_image_extension(std::string_view ext) noexcept {
    return icon_catalog::file_extension_is_svg_image(ext);
}

inline bool raster_image_extension(std::string_view ext) noexcept {
    return icon_catalog::file_extension_is_raster_image(ext);
}

inline bool image_extension(std::string_view ext) noexcept {
    return icon_catalog::file_extension_is_image(ext);
}

inline bool movie_extension(std::string_view ext) noexcept {
    return icon_catalog::file_extension_is_movie(ext);
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

inline void remove_file_if_body_matches(fs::path const& path,
                                        std::string_view expected_body) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || ec)
        return;
    ec.clear();
    auto const size = fs::file_size(path, ec);
    if (ec || size != expected_body.size())
        return;

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return;
    std::string body(expected_body.size(), '\0');
    in.read(body.data(), static_cast<std::streamsize>(body.size()));
    if (in.gcount() != static_cast<std::streamsize>(body.size()))
        return;
    if (body != expected_body)
        return;

    fs::remove(path, ec);
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
    if (profile == "desktop" || profile.starts_with("test-model-contract")) {
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
            root / "작성예시2_필수_운용지시서.pdf",
            "PDF placeholder with Korean filename for Finder recents ordering.\n");
        write_file_if_missing(
            root / "작성예시1_필수_중도인출 신청서.pdf",
            "PDF placeholder with Korean filename for Finder recents ordering.\n");
        write_file_if_missing(
            root / "契約書_サンプル.pdf",
            "PDF placeholder with Japanese filename for font fallback verification.\n");
        write_file_if_missing(
            root / "资产证明.pdf",
            "PDF placeholder with Chinese filename for font fallback verification.\n");
    }
    write_file_if_missing(
        root / "※해당 시 필독_①무주택자인 경우.pdf",
        "PDF placeholder matching the Finder-style Korean Recents probe scene.\n");
    write_file_if_missing(
        root / "[카카오] 퇴직금 지급 기준.pdf",
        "PDF placeholder matching the Finder-style Korean Recents probe scene.\n");
    remove_file_if_body_matches(
        root / "Withdrawal Notice.pdf",
        "PDF placeholder for the Recents probe scene.\n");
    remove_file_if_body_matches(
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
        root / "line_4.png",
        "PNG placeholder rendered as a light rounded Finder thumbnail.\n");
    write_file_if_missing(
        root / "Glass Symbol.svg",
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "viewBox=\"0 0 48 48\" fill=\"none\">\n"
        "  <rect x=\"6\" y=\"6\" width=\"36\" height=\"36\" "
        "rx=\"10\" fill=\"white\"/>\n"
        "  <path d=\"M15 30 C19 20 29 20 33 30\" "
        "stroke=\"#007AFF\" stroke-width=\"3\" "
        "stroke-linecap=\"round\"/>\n"
        "</svg>\n");
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
        root / "Expense Matrix.csv",
        "category,amount\nDesign,12\nEngineering,24\nQA,8\n");
    write_file_if_missing(
        root / "Glass Theme Notes.cpp",
        "import phenotype;\n\nint main() {\n    return 0;\n}\n");
    write_file_if_missing(
        root / "Design Review.key",
        "Presentation placeholder for Finder-like file-type glyph coverage.\n");
    write_file_if_missing(
        root / "Voice Memo.m4a",
        "Audio placeholder for Finder-like file-type glyph coverage.\n");
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
        return std::string{icon_catalog::file_type_kind_label(
            icon_catalog::Symbol::Folder)};
    auto ext = extension_lower(entry.name);
    if (auto symbol = icon_catalog::known_file_type_symbol_for_extension(ext);
        symbol.has_value() && *symbol != icon_catalog::Symbol::TextDocument) {
        if (auto label =
                icon_catalog::known_file_type_kind_label_for_extension(ext))
            return std::string{*label};
    }
    if (ext.empty())
        return std::string{icon_catalog::file_type_kind_label(
            icon_catalog::Symbol::Document)};
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return ext + " File";
}

inline icon_catalog::Symbol entry_symbol(Entry const& entry) {
    return icon_catalog::file_type_symbol_for_entry(
        extension_lower(entry.name),
        entry.folder);
}

inline std::string_view entry_symbol_name(Entry const& entry) {
    return icon_catalog::name(entry_symbol(entry));
}

inline std::string_view entry_symbol_semantic_reference_name(
        Entry const& entry) {
    return icon_catalog::semantic_reference_name(entry_symbol(entry));
}

inline std::string entry_size_label(Entry const& entry) {
    return entry.folder ? "--" : format_size(entry.size);
}

inline std::string sort_mode_label(SortMode mode) {
    switch (mode) {
        case SortMode::Recent: return "Recent";
        case SortMode::Kind: return "Kind";
        case SortMode::Size: return "Size";
        case SortMode::Name:
        default: return "Name";
    }
}

inline std::string sort_mode_value_name(SortMode mode) {
    switch (mode) {
        case SortMode::Recent: return "recent";
        case SortMode::Kind: return "kind";
        case SortMode::Size: return "size";
        case SortMode::Name:
        default: return "name";
    }
}

inline std::string view_mode_value_name(ExplorerViewMode mode) {
    switch (mode) {
        case ExplorerViewMode::List: return "list";
        case ExplorerViewMode::Column: return "column";
        case ExplorerViewMode::Gallery: return "gallery";
        case ExplorerViewMode::Icon:
        default: return "icon";
    }
}

inline std::string view_mode_label(ExplorerViewMode mode) {
    switch (mode) {
        case ExplorerViewMode::List: return "List View";
        case ExplorerViewMode::Column: return "Column View";
        case ExplorerViewMode::Gallery: return "Gallery View";
        case ExplorerViewMode::Icon:
        default: return "Icon View";
    }
}

inline std::string selection_move_value_name(ExplorerSelectionMove move) {
    switch (move) {
        case ExplorerSelectionMove::Left: return "left";
        case ExplorerSelectionMove::Right: return "right";
        case ExplorerSelectionMove::Up: return "up";
        case ExplorerSelectionMove::Down: return "down";
        case ExplorerSelectionMove::PageUp: return "page-up";
        case ExplorerSelectionMove::PageDown: return "page-down";
        case ExplorerSelectionMove::Home: return "home";
        case ExplorerSelectionMove::End: return "end";
    }
    return "down";
}

inline std::string selection_move_label(ExplorerSelectionMove move) {
    switch (move) {
        case ExplorerSelectionMove::Left: return "Arrow Left";
        case ExplorerSelectionMove::Right: return "Arrow Right";
        case ExplorerSelectionMove::Up: return "Arrow Up";
        case ExplorerSelectionMove::Down: return "Arrow Down";
        case ExplorerSelectionMove::PageUp: return "Page Up";
        case ExplorerSelectionMove::PageDown: return "Page Down";
        case ExplorerSelectionMove::Home: return "Home";
        case ExplorerSelectionMove::End: return "End";
    }
    return "Arrow Down";
}

inline ExplorerViewMode view_mode_from_name(std::string_view value) {
    auto mode = lower_copy(trim(value));
    if (mode == "list")
        return ExplorerViewMode::List;
    if (mode == "column")
        return ExplorerViewMode::Column;
    if (mode == "gallery")
        return ExplorerViewMode::Gallery;
    return ExplorerViewMode::Icon;
}

inline bool known_view_mode_name(std::string_view value) {
    auto mode = lower_copy(trim(value));
    return mode == "icon" || mode == "list" || mode == "column"
        || mode == "gallery";
}

inline std::optional<ExplorerSelectionMove> selection_move_from_name(
        std::string_view value) {
    auto move = lower_copy(trim(value));
    if (move == "left" || move == "arrow-left" || move == "arrow_left")
        return ExplorerSelectionMove::Left;
    if (move == "right" || move == "arrow-right" || move == "arrow_right")
        return ExplorerSelectionMove::Right;
    if (move == "up" || move == "arrow-up" || move == "arrow_up")
        return ExplorerSelectionMove::Up;
    if (move == "down" || move == "arrow-down" || move == "arrow_down")
        return ExplorerSelectionMove::Down;
    if (move == "page-up" || move == "page_up" || move == "pageup")
        return ExplorerSelectionMove::PageUp;
    if (move == "page-down" || move == "page_down" || move == "pagedown")
        return ExplorerSelectionMove::PageDown;
    if (move == "home")
        return ExplorerSelectionMove::Home;
    if (move == "end")
        return ExplorerSelectionMove::End;
    return std::nullopt;
}

} // namespace file_explorer_demo

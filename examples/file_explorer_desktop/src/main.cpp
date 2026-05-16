#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
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
enum class FinderViewMode { Icon, List, Column, Gallery };
struct SetViewMode { FinderViewMode mode; };
struct ToolbarAction { std::string label; };
struct SearchChanged { std::string text; };
struct ToggleSearch {};
struct CreateFile {};
struct CreateFolder {};
struct DeleteSelected {};
struct DuplicateSelected {};
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
    SetViewMode,
    ToolbarAction,
    SearchChanged,
    ToggleSearch,
    CreateFile,
    CreateFolder,
    DeleteSelected,
    DuplicateSelected,
    GoBack,
    GoForward,
    GoUp,
    CycleSort,
    Refresh,
    ResetDemo,
    Resized,
    Noop>;

FinderViewMode view_mode_from_name(std::string_view value) {
    std::string mode(value);
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (mode == "list")
        return FinderViewMode::List;
    if (mode == "column")
        return FinderViewMode::Column;
    if (mode == "gallery")
        return FinderViewMode::Gallery;
    return FinderViewMode::Icon;
}

FinderViewMode initial_view_mode() {
    char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_VIEW");
    return raw ? view_mode_from_name(raw) : FinderViewMode::Icon;
}

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
    apply_startup_inputs(state, "desktop");
    return state;
}

std::string initial_locale() {
    char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_LOCALE");
    return raw && *raw ? std::string{raw} : std::string{"en"};
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

struct State {
    file_explorer_demo::ExplorerState explorer;
    file_explorer_demo::ExplorerLabels labels;
    FinderViewMode view_mode;
    bool search_visible = false;

    State()
        : explorer(initial_explorer_state()),
          labels(file_explorer_demo::file_explorer_labels(
              initial_locale(),
              runtime_resource_catalog())),
          view_mode(initial_view_mode()),
          search_visible(!explorer.search.empty()) {
    }
};

constexpr float k_pi = 3.14159265358979323846f;
constexpr float k_tau = 6.28318530717958647692f;
constexpr float k_integrated_titlebar_height =
    file_explorer_demo::k_desktop_integrated_titlebar_height;
constexpr float k_sidebar_width = file_explorer_demo::k_desktop_sidebar_width;
constexpr float k_sidebar_row_width = file_explorer_demo::k_desktop_sidebar_row_width;
constexpr float k_sidebar_row_height =
    file_explorer_demo::k_desktop_sidebar_row_height;
constexpr float k_sidebar_heading_height =
    file_explorer_demo::k_desktop_sidebar_heading_height;
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
    return phenotype::FontSpec{
        .family = "Pretendard",
        .weight = weight,
    };
}

phenotype::layout::MaterialSurfaceOptions toolbar_shell_options() {
    using namespace phenotype;
    return layout::MaterialSurfaceOptions{
        .kind = MaterialKind::Clear,
        .role = MaterialSurfaceRole::Toolbar,
        .direction = FlexDirection::Row,
        .padding = SpaceToken::Xs,
        .gap = SpaceToken::Xs,
        .cross_align = CrossAxisAlignment::Center,
        .main_align = MainAxisAlignment::Start,
        .border_radius = 0.0f,
        .border_width = 0.0f,
        .semantic_label = "Toolbar",
    };
}

phenotype::layout::MaterialSurfaceOptions toolbar_group_options(
        char const* label,
        float max_width) {
    using namespace phenotype;
    return layout::MaterialSurfaceOptions{
        .kind = MaterialKind::Thick,
        .role = MaterialSurfaceRole::Toolbar,
        .direction = FlexDirection::Row,
        .padding = SpaceToken::Xs,
        .gap = SpaceToken::Xs,
        .cross_align = CrossAxisAlignment::Center,
        .main_align = MainAxisAlignment::Start,
        .max_width = max_width,
        .fixed_height = k_toolbar_group_height,
        .border_radius = k_toolbar_group_radius,
        .border_width = 0.0f,
        .semantic_label = label,
    };
}

phenotype::layout::MaterialSurfaceOptions content_surface_options(
        phenotype::SpaceToken gap = phenotype::SpaceToken::Md) {
    using namespace phenotype;
    return layout::MaterialSurfaceOptions{
        .kind = MaterialKind::Regular,
        .role = MaterialSurfaceRole::Content,
        .direction = FlexDirection::Column,
        .padding = SpaceToken::Xl,
        .gap = gap,
        .cross_align = CrossAxisAlignment::Start,
        .main_align = MainAxisAlignment::Start,
        .border_radius = k_content_radius,
        .border_width = 0.0f,
        .semantic_label = "Files",
    };
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

void paint_sidebar_icon(phenotype::Painter& painter, std::string_view id) {
    auto ink = rgba(30, 30, 30);
    if (id == "recents")
        ink = rgba(0, 122, 255);
    if (id == "folder") {
        fill_round(painter, 2.0f, 9.0f, 22.0f, 14.0f, 3.0f, rgba(255, 255, 255, 235));
        stroke_round(painter, 2.0f, 9.0f, 22.0f, 14.0f, 3.0f, 2.0f, ink);
        painter.line(4.0f, 8.0f, 12.0f, 8.0f, 2.0f, ink);
    } else if (id == "recents") {
        painter.arc(13.0f, 13.0f, 10.0f, 0.0f, k_tau, 2.2f, ink);
        painter.line(13.0f, 13.0f, 13.0f, 5.5f, 2.2f, ink);
        painter.line(13.0f, 13.0f, 6.8f, 13.0f, 2.2f, ink);
    } else if (id == "app") {
        painter.text(3.0f, 0.0f, "A", 1, 24.0f, ink,
                     finder_font(phenotype::FontWeight::Bold));
        painter.line(4.0f, 22.0f, 21.0f, 22.0f, 2.0f, ink);
    } else if (id == "desktop") {
        stroke_round(painter, 3.0f, 7.0f, 20.0f, 14.0f, 3.0f, 2.0f, ink);
        painter.line(7.0f, 17.0f, 19.0f, 17.0f, 2.0f, ink);
    } else if (id == "doc") {
        stroke_round(painter, 6.0f, 3.0f, 15.0f, 20.0f, 2.0f, 2.0f, ink);
        painter.line(14.0f, 3.0f, 21.0f, 10.0f, 2.0f, ink);
    } else if (id == "download") {
        painter.arc(13.0f, 13.0f, 10.0f, 0.0f, k_tau, 2.0f, ink);
        painter.line(13.0f, 5.0f, 13.0f, 17.0f, 2.0f, ink);
        painter.line(8.0f, 13.5f, 13.0f, 18.0f, 2.0f, ink);
        painter.line(18.0f, 13.5f, 13.0f, 18.0f, 2.0f, ink);
    } else if (id == "cloud") {
        painter.arc(9.0f, 16.0f, 6.0f, k_pi, k_tau, 2.0f, ink);
        painter.arc(16.0f, 13.0f, 8.0f, k_pi, k_tau, 2.0f, ink);
        painter.line(5.0f, 17.0f, 24.0f, 17.0f, 2.0f, ink);
    } else if (id == "home") {
        painter.line(4.0f, 14.0f, 13.0f, 5.0f, 2.0f, ink);
        painter.line(13.0f, 5.0f, 22.0f, 14.0f, 2.0f, ink);
        stroke_round(painter, 7.0f, 13.0f, 12.0f, 10.0f, 1.0f, 2.0f, ink);
    } else if (id == "airdrop") {
        painter.arc(13.0f, 14.0f, 9.0f, k_pi, k_tau, 1.8f, ink);
        painter.arc(13.0f, 14.0f, 5.5f, k_pi, k_tau, 1.8f, ink);
        painter.arc(13.0f, 14.0f, 2.5f, 0.0f, k_tau, 2.0f, ink);
    } else if (id == "trash") {
        stroke_round(painter, 7.0f, 8.0f, 12.0f, 16.0f, 1.5f, 2.0f, ink);
        painter.line(5.0f, 8.0f, 21.0f, 8.0f, 2.0f, ink);
        painter.line(9.0f, 5.0f, 17.0f, 5.0f, 2.0f, ink);
    }
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

std::uint64_t stable_token(std::string const& value) {
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char ch : value) {
        h ^= ch;
        h *= 1099511628211ull;
    }
    return h ? h : 1ull;
}

void paint_pdf_thumbnail(phenotype::Painter& painter,
                         std::string const& name,
                         bool selected) {
    auto accent = selected ? rgba(0, 122, 255) : rgba(38, 132, 255);
    fill_round(painter, 36.0f, 5.0f, 58.0f, 76.0f, 4.0f, rgba(0, 0, 0, 24));
    fill_round(painter, 34.0f, 3.0f, 58.0f, 76.0f, 4.0f, rgba(255, 255, 255, 250));
    stroke_round(painter, 34.0f, 3.0f, 58.0f, 76.0f, 4.0f, 1.0f, rgba(210, 214, 220));
    fill_rect(painter, 36.0f, 7.0f, 34.0f, 5.0f, accent);
    fill_rect(painter, 38.0f, 16.0f, 47.0f, 3.0f, rgba(207, 214, 225));
    fill_rect(painter, 38.0f, 23.0f, 42.0f, 3.0f, rgba(229, 124, 124, 170));
    fill_rect(painter, 38.0f, 31.0f, 48.0f, 2.0f, rgba(207, 214, 225));
    fill_rect(painter, 38.0f, 38.0f, 42.0f, 2.0f, rgba(207, 214, 225));
    fill_rect(painter, 38.0f, 45.0f, 48.0f, 2.0f, rgba(207, 214, 225));
    fill_rect(painter, 38.0f, 55.0f, 26.0f, 8.0f, rgba(255, 231, 125, 210));
    auto tag = extension_lower(name);
    painter.text(40.0f, 64.0f, tag.c_str(),
                 static_cast<unsigned int>(tag.size()),
                 9.0f, rgba(80, 87, 96), finder_font());
}

void paint_image_thumbnail(phenotype::Painter& painter,
                           bool selected) {
    auto border = selected ? rgba(0, 122, 255) : rgba(224, 228, 234);
    fill_round(painter, 18.0f, 30.0f, 92.0f, 22.0f, 8.0f, rgba(0, 0, 0, 20));
    fill_round(painter, 16.0f, 28.0f, 92.0f, 22.0f, 8.0f, rgba(255, 255, 255, 248));
    painter.linear_gradient_rect(
        18.0f, 30.0f, 88.0f, 18.0f,
        rgba(248, 250, 252),
        rgba(226, 232, 240),
        phenotype::GradientAxis::Horizontal,
        12);
    stroke_round(painter, 16.0f, 28.0f, 92.0f, 22.0f, 8.0f, 1.0f, border);
    fill_round(painter, 29.0f, 33.0f, 24.0f, 12.0f, 6.0f, rgba(214, 217, 222));
    fill_round(painter, 56.0f, 33.0f, 18.0f, 12.0f, 6.0f, rgba(188, 193, 201));
    fill_round(painter, 77.0f, 33.0f, 18.0f, 12.0f, 6.0f, rgba(231, 233, 237));
}

void paint_video_thumbnail(phenotype::Painter& painter,
                           bool selected) {
    auto border = selected ? rgba(0, 122, 255) : rgba(204, 211, 221);
    fill_round(painter, 15.0f, 22.0f, 96.0f, 40.0f, 7.0f, rgba(0, 0, 0, 30));
    fill_round(painter, 13.0f, 20.0f, 96.0f, 40.0f, 7.0f, rgba(245, 248, 251));
    stroke_round(painter, 13.0f, 20.0f, 96.0f, 40.0f, 7.0f, 1.0f, border);
    painter.linear_gradient_rect(
        15.0f, 22.0f, 92.0f, 9.0f,
        rgba(42, 123, 222, 210),
        rgba(99, 102, 241, 190),
        phenotype::GradientAxis::Horizontal,
        10);
    for (int i = 0; i < 6; ++i) {
        float x = 19.0f + static_cast<float>(i) * 14.0f;
        fill_rect(painter, x, 34.0f, 9.0f, 18.0f,
                  (i % 2 == 0) ? rgba(212, 219, 228) : rgba(235, 239, 244));
    }
}

void paint_text_thumbnail(phenotype::Painter& painter,
                          bool selected) {
    auto border = selected ? rgba(0, 122, 255) : rgba(220, 224, 231);
    fill_round(painter, 38.0f, 6.0f, 52.0f, 72.0f, 4.0f, rgba(0, 0, 0, 22));
    fill_round(painter, 36.0f, 4.0f, 52.0f, 72.0f, 4.0f, rgba(255, 255, 255, 250));
    stroke_round(painter, 36.0f, 4.0f, 52.0f, 72.0f, 4.0f, 1.0f, border);
    for (int i = 0; i < 7; ++i) {
        fill_rect(painter, 42.0f, 16.0f + static_cast<float>(i) * 7.0f,
                  (i % 3 == 0) ? 30.0f : 39.0f,
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
                          file_explorer_demo::Entry const& entry,
                          bool selected) {
    if (entry.folder) {
        paint_folder_thumbnail(painter, selected);
        return;
    }
    auto ext = extension_lower(entry.name);
    if (ext == "pdf") {
        paint_pdf_thumbnail(painter, entry.name, selected);
    } else if (ext == "png" || ext == "jpg" || ext == "jpeg") {
        paint_image_thumbnail(painter, selected);
    } else if (ext == "mov" || ext == "mp4") {
        paint_video_thumbnail(painter, selected);
    } else {
        paint_text_thumbnail(painter, selected);
    }
}

std::string finder_status(file_explorer_demo::Snapshot const& snap) {
    std::string text = snap.item_summary;
    if (snap.has_selection)
        text += " - selected " + snap.selected.name;
    if (!snap.sort_label.empty())
        text += " - " + snap.sort_label;
    return text;
}

std::string view_mode_name(FinderViewMode mode) {
    switch (mode) {
        case FinderViewMode::Icon: return "Icon View";
        case FinderViewMode::List: return "List View";
        case FinderViewMode::Column: return "Column View";
        case FinderViewMode::Gallery: return "Gallery View";
    }
    return "Icon View";
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
    std::vector<std::string> lines;
    std::string current;
    for (auto const& token : icon_label_tokens(label)) {
        auto candidate = current + token;
        if (!current.empty() && width_of(candidate) > max_width) {
            lines.push_back(trim_icon_label_line(std::move(current)));
            current = token;
            if (lines.size() == 2)
                break;
        } else {
            current = std::move(candidate);
        }
    }
    if (lines.size() < 2 && !current.empty())
        lines.push_back(trim_icon_label_line(std::move(current)));
    if (lines.empty())
        lines.push_back(label);
    if (lines.size() > 2)
        lines.resize(2);
    if (lines.size() == 2) {
        auto& tail = lines.back();
        bool truncated = false;
        while (width_of(tail) > max_width && tail.size() > 4) {
            pop_utf8_codepoint(tail);
            truncated = true;
        }
        if (truncated && width_of(tail + "...") <= max_width)
            tail += "...";
    }
    return lines;
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
            file_explorer_demo::select_location(explorer, m.id);
        } else if constexpr (std::same_as<T, SelectEntry>) {
            file_explorer_demo::activate_entry(explorer, m.name);
        } else if constexpr (std::same_as<T, SetViewMode>) {
            state.view_mode = m.mode;
            explorer.status = "Switched to " + view_mode_name(m.mode) + ".";
        } else if constexpr (std::same_as<T, ToolbarAction>) {
            explorer.status = m.label + " action is available in the native toolbar contract.";
        } else if constexpr (std::same_as<T, SearchChanged>) {
            file_explorer_demo::set_search_filter(explorer, m.text);
            state.search_visible = true;
        } else if constexpr (std::same_as<T, ToggleSearch>) {
            state.search_visible = !state.search_visible;
            if (!state.search_visible) {
                file_explorer_demo::set_search_filter(explorer, {});
            } else {
                explorer.status = "Search ready.";
            }
        } else if constexpr (std::same_as<T, CreateFile>) {
            file_explorer_demo::create_file(explorer);
        } else if constexpr (std::same_as<T, CreateFolder>) {
            file_explorer_demo::create_folder(explorer);
        } else if constexpr (std::same_as<T, DeleteSelected>) {
            file_explorer_demo::delete_selected(explorer);
        } else if constexpr (std::same_as<T, DuplicateSelected>) {
            file_explorer_demo::duplicate_selected(explorer);
        } else if constexpr (std::same_as<T, GoBack>) {
            file_explorer_demo::go_back(explorer);
        } else if constexpr (std::same_as<T, GoForward>) {
            file_explorer_demo::go_forward(explorer);
        } else if constexpr (std::same_as<T, GoUp>) {
            file_explorer_demo::go_up(explorer);
        } else if constexpr (std::same_as<T, CycleSort>) {
            file_explorer_demo::cycle_sort_mode(explorer);
        } else if constexpr (std::same_as<T, Refresh>) {
            explorer.status = "Refreshed " + file_explorer_demo::relative_location(
                explorer.root,
                explorer.current);
        } else if constexpr (std::same_as<T, ResetDemo>) {
            file_explorer_demo::reset_demo_tree(explorer, "desktop");
        } else if constexpr (std::same_as<T, Resized>) {
            explorer.viewport_width = m.width;
            explorer.viewport_height = m.height;
            explorer.viewport_scale = m.scale;
        } else if constexpr (std::same_as<T, Noop>) {
        }
    }, msg);
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

void finder_icon_label_button(std::string const& label,
                              bool selected,
                              float max_width,
                              float font_size) {
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
    options.fixed_height = 46.0f;
    options.max_width = max_width;
    phenotype::widget::canvas_button<Msg>(
        phenotype::str{label},
        max_width,
        46.0f,
        [label, selected, max_width, font_size](phenotype::Painter& painter) {
            auto const ink = selected ? rgba(255, 255, 255) : rgba(28, 28, 30);
            auto const lines = finder_icon_label_lines(
                painter,
                label,
                max_width - 12.0f,
                font_size);
            float y = lines.size() == 1 ? 12.0f : 4.0f;
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

void sidebar_row(std::string_view label,
                 std::string_view icon,
                 std::string location_id,
                 bool selected = false) {
    using namespace phenotype;
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected
        ? rgba(232, 232, 234, 235)
        : rgba(0, 0, 0, 0);
    options.has_hover_background = true;
    options.hover_background = selected
        ? rgba(224, 224, 229, 245)
        : rgba(230, 230, 234, 150);
    options.has_border_color = true;
    options.border_color = rgba(0, 0, 0, 0);
    options.border_width = 0.0f;
    options.border_radius = 8.0f;
    options.max_width = k_sidebar_row_width;
    options.fixed_height = k_sidebar_row_height;

    std::string label_text(label);
    std::string icon_name(icon);
    widget::canvas_button<Msg>(
        str{label_text},
        k_sidebar_row_width,
        k_sidebar_row_height,
        [label_text, icon_name, selected](Painter& painter) {
            paint_sidebar_icon(painter, icon_name);
            auto ink = selected ? rgba(0, 122, 255) : rgba(30, 30, 30);
            painter.text(44.0f,
                         8.0f,
                         label_text.c_str(),
                         static_cast<unsigned int>(label_text.size()),
                         15.0f,
                         ink,
                         finder_font());
        },
        SelectLocation{std::move(location_id)},
        options,
        stable_token(label_text) ^ stable_token(icon_name) ^ 0x510000u);
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
            painter.text(8.0f,
                         7.0f,
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

void finder_sidebar(State const& state) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    auto relative = file_explorer_demo::relative_location(
        explorer.root,
        explorer.current);
    bool const in_root = relative == "Demo Root";
    auto const& labels = state.labels;
    layout::sidebar(k_sidebar_width, [&] {
        layout::spacer(k_integrated_titlebar_height);
        sidebar_row(labels.sidebar_recents, "recents", "root", in_root);
        sidebar_row(labels.sidebar_shared, "folder", "shared",
                    relative == "Demo Root/Shared");
        layout::spacer(14);
        sidebar_heading(labels.favorites);
        sidebar_row(labels.applications, "app", "root");
        sidebar_row(labels.desktop, "desktop", "root");
        sidebar_row(labels.documents, "doc", "documents",
                    relative == "Demo Root/Documents");
        sidebar_row(labels.downloads, "download", "root");
        layout::spacer(14);
        sidebar_heading(labels.locations);
        sidebar_row(labels.icloud_drive, "cloud", "root");
        sidebar_row(labels.home, "home", "root");
        sidebar_row(labels.airdrop, "airdrop", "shared");
        sidebar_row(labels.trash, "trash", "trash", relative == "Trash");
    }, MaterialKind::Thin, SpaceToken::Lg, SpaceToken::Xs);
}

phenotype::Color nav_icon_ink(bool enabled) {
    return enabled ? rgba(82, 82, 86) : rgba(190, 193, 198);
}

void paint_back_icon(phenotype::Painter& painter, bool enabled) {
    auto ink = nav_icon_ink(enabled);
    painter.line(22.0f, 8.0f, 13.0f, 15.0f, 1.8f, ink);
    painter.line(13.0f, 15.0f, 22.0f, 22.0f, 1.8f, ink);
}

void paint_forward_icon(phenotype::Painter& painter, bool enabled) {
    auto ink = nav_icon_ink(enabled);
    painter.line(14.0f, 8.0f, 23.0f, 15.0f, 1.8f, ink);
    painter.line(23.0f, 15.0f, 14.0f, 22.0f, 1.8f, ink);
}

phenotype::ButtonStyleOptions toolbar_icon_button_options(
        bool selected = false,
        bool disabled = false) {
    auto const& t = phenotype::current_theme();
    phenotype::ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected ? rgba(229, 229, 234, 150) : t.transparent;
    options.has_hover_background = true;
    options.hover_background = selected
        ? rgba(216, 216, 222, 190)
        : rgba(229, 229, 234, 120);
    options.has_border_color = true;
    options.border_color = t.transparent;
    options.border_width = 0.0f;
    options.border_radius = 15.0f;
    options.max_width = k_toolbar_icon_button_width;
    options.fixed_height = k_toolbar_icon_button_height;
    options.disabled = disabled;
    return options;
}

phenotype::Color toolbar_icon_ink(bool selected = false) {
    return selected ? rgba(58, 58, 60) : rgba(96, 96, 100);
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

void paint_icon_view(phenotype::Painter& painter, bool selected) {
    auto ink = toolbar_icon_ink(selected);
    for (int col = 0; col < 2; ++col) {
        for (int row = 0; row < 2; ++row) {
            stroke_round(painter,
                         8.0f + static_cast<float>(col) * 12.0f,
                         6.0f + static_cast<float>(row) * 11.0f,
                         8.0f, 8.0f, 2.0f, 1.35f, ink);
        }
    }
}

void paint_list_view(phenotype::Painter& painter, bool selected) {
    auto ink = toolbar_icon_ink(selected);
    for (int i = 0; i < 4; ++i) {
        float y = 6.0f + static_cast<float>(i) * 5.8f;
        painter.line(8.0f, y, 10.0f, y, 1.8f, ink);
        painter.line(16.0f, y, 28.0f, y, 1.45f, ink);
    }
}

void paint_column_view(phenotype::Painter& painter, bool selected) {
    auto ink = toolbar_icon_ink(selected);
    for (int i = 0; i < 3; ++i) {
        stroke_round(painter, 6.0f + static_cast<float>(i) * 8.5f,
                     6.0f, 7.0f, 18.0f, 2.0f, 1.35f, ink);
    }
}

void paint_gallery_view(phenotype::Painter& painter, bool selected) {
    auto ink = toolbar_icon_ink(selected);
    stroke_round(painter, 7.0f, 6.0f, 20.0f, 16.0f, 4.0f, 1.35f, ink);
    painter.line(9.0f, 25.0f, 25.0f, 25.0f, 1.7f, ink);
}

void paint_group_sort_icon(phenotype::Painter& painter) {
    auto ink = toolbar_icon_ink();
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            stroke_round(painter,
                         6.0f + static_cast<float>(col) * 6.6f,
                         5.0f + static_cast<float>(row) * 6.2f,
                         5.4f, 3.6f, 1.0f, 1.25f, ink);
        }
    }
    painter.line(26.0f, 12.0f, 29.0f, 15.0f, 1.55f, ink);
    painter.line(32.0f, 12.0f, 29.0f, 15.0f, 1.55f, ink);
}

void paint_share_icon(phenotype::Painter& painter) {
    auto ink = rgba(170, 174, 181);
    painter.line(17.0f, 5.0f, 17.0f, 19.0f, 1.65f, ink);
    painter.line(11.0f, 11.0f, 17.0f, 5.0f, 1.65f, ink);
    painter.line(23.0f, 11.0f, 17.0f, 5.0f, 1.65f, ink);
    stroke_round(painter, 8.0f, 18.0f, 18.0f, 10.0f, 3.0f, 1.45f, ink);
}

void paint_tag_icon(phenotype::Painter& painter) {
    auto ink = rgba(170, 174, 181);
    auto tag = rounded_rect_path(5.0f, 7.0f, 24.0f, 17.0f, 4.0f);
    painter.stroke_path(tag, 1.5f, rgba(170, 174, 181));
    painter.arc(11.0f, 13.0f, 2.3f, 0.0f, k_tau, 1.25f, ink);
}

void paint_more_icon(phenotype::Painter& painter) {
    auto ink = toolbar_icon_ink();
    for (int i = 0; i < 3; ++i) {
        painter.arc(11.0f + static_cast<float>(i) * 6.0f,
                    15.0f, 2.0f, 0.0f, k_tau, 1.3f, ink);
    }
}

void paint_search_icon(phenotype::Painter& painter) {
    auto ink = toolbar_icon_ink();
    painter.arc(15.0f, 12.0f, 7.5f, 0.0f, k_tau, 1.75f, ink);
    painter.line(20.0f, 17.0f, 28.0f, 25.0f, 1.75f, ink);
}

void paint_new_file_icon(phenotype::Painter& painter, bool enabled) {
    auto ink = nav_icon_ink(enabled);
    stroke_round(painter, 10.0f, 5.0f, 16.0f, 22.0f, 3.0f, 1.45f, ink);
    painter.line(19.0f, 5.0f, 26.0f, 12.0f, 1.45f, ink);
    painter.line(18.0f, 15.0f, 18.0f, 24.0f, 1.75f, ink);
    painter.line(13.5f, 19.5f, 22.5f, 19.5f, 1.75f, ink);
}

void paint_new_folder_icon(phenotype::Painter& painter, bool enabled) {
    auto ink = nav_icon_ink(enabled);
    stroke_round(painter, 6.0f, 11.0f, 22.0f, 15.0f, 4.0f, 1.45f, ink);
    painter.line(8.0f, 11.0f, 13.0f, 7.0f, 1.45f, ink);
    painter.line(13.0f, 7.0f, 21.0f, 7.0f, 1.45f, ink);
    painter.line(18.0f, 15.0f, 18.0f, 23.0f, 1.65f, ink);
    painter.line(14.0f, 19.0f, 22.0f, 19.0f, 1.65f, ink);
}

void paint_duplicate_icon(phenotype::Painter& painter, bool enabled) {
    auto ink = nav_icon_ink(enabled);
    stroke_round(painter, 8.0f, 10.0f, 14.0f, 17.0f, 3.0f, 1.45f, ink);
    stroke_round(painter, 13.0f, 5.0f, 14.0f, 17.0f, 3.0f, 1.45f, ink);
}

void paint_delete_icon(phenotype::Painter& painter, bool enabled) {
    auto ink = nav_icon_ink(enabled);
    stroke_round(painter, 11.0f, 11.0f, 13.0f, 16.0f, 2.0f, 1.45f, ink);
    painter.line(8.0f, 11.0f, 28.0f, 11.0f, 1.65f, ink);
    painter.line(14.0f, 7.0f, 22.0f, 7.0f, 1.65f, ink);
    painter.line(15.0f, 15.0f, 15.0f, 24.0f, 1.3f, ink);
    painter.line(21.0f, 15.0f, 21.0f, 24.0f, 1.3f, ink);
}

template<typename Paint>
void view_mode_button(char const* label,
                      FinderViewMode mode,
                      FinderViewMode current,
                      Paint paint,
                      std::uint64_t token) {
    bool const selected = mode == current;
    std::string semantic_label(label);
    phenotype::widget::canvas_button<Msg>(
        phenotype::str{semantic_label},
        k_toolbar_icon_button_width,
        k_toolbar_icon_button_height,
        [paint, selected](phenotype::Painter& painter) {
            paint(painter, selected);
        },
        SetViewMode{mode},
        toolbar_icon_button_options(selected),
        token ^ (selected ? 0x100000000ull : 0ull));
}

void toolbar_action_button(char const* label,
                           void (*paint)(phenotype::Painter&),
                           std::uint64_t token) {
    std::string semantic_label(label);
    phenotype::widget::canvas_button<Msg>(
        phenotype::str{semantic_label},
        k_toolbar_icon_button_width,
        k_toolbar_icon_button_height,
        [paint](phenotype::Painter& painter) {
            paint(painter);
        },
        ToolbarAction{semantic_label},
        toolbar_icon_button_options(false),
        token);
}

Msg on_search_changed(std::string text) {
    return SearchChanged{std::move(text)};
}

void search_toggle_button(bool selected) {
    std::string semantic_label = "Search Control";
    phenotype::widget::canvas_button<Msg>(
        phenotype::str{semantic_label},
        k_toolbar_icon_button_width,
        k_toolbar_icon_button_height,
        [](phenotype::Painter& painter) {
            paint_search_icon(painter);
        },
        ToggleSearch{},
        toolbar_icon_button_options(selected),
        0x6401u ^ (selected ? 0x400000000ull : 0ull));
}

void sort_action_button(file_explorer_demo::Snapshot const& snap) {
    std::string semantic_label = "Group Sort";
    if (!snap.sort_label.empty())
        semantic_label += " (" + snap.sort_label + ")";
    phenotype::widget::canvas_button<Msg>(
        phenotype::str{semantic_label},
        k_toolbar_icon_button_width,
        k_toolbar_icon_button_height,
        [](phenotype::Painter& painter) {
            paint_group_sort_icon(painter);
        },
        CycleSort{},
        toolbar_icon_button_options(false),
        0x6501u);
}

template<typename Paint>
void file_action_button(char const* label,
                        Msg msg,
                        bool enabled,
                        Paint paint,
                        std::uint64_t token) {
    std::string semantic_label(label);
    phenotype::widget::canvas_button<Msg>(
        phenotype::str{semantic_label},
        k_toolbar_icon_button_width,
        k_toolbar_icon_button_height,
        [paint, enabled](phenotype::Painter& painter) {
            paint(painter, enabled);
        },
        std::move(msg),
        toolbar_icon_button_options(false, !enabled),
        token ^ (enabled ? 0x300000000ull : 0ull));
}

template<typename Paint>
void navigation_button(char const* label,
                       Msg msg,
                       bool enabled,
                       Paint paint,
                       std::uint64_t token) {
    std::string semantic_label(label);
    phenotype::widget::canvas_button<Msg>(
        phenotype::str{semantic_label},
        k_toolbar_icon_button_width,
        k_toolbar_icon_button_height,
        [paint, enabled](phenotype::Painter& painter) {
            paint(painter, enabled);
        },
        std::move(msg),
        toolbar_icon_button_options(false, !enabled),
        token ^ (enabled ? 0x200000000ull : 0ull));
}

void finder_toolbar(State const& state,
                    file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    layout::toolbar(toolbar_shell_options(), [&] {
        layout::toolbar(
            toolbar_group_options("Navigation Controls", 92.0f),
            [&] {
                navigation_button(state.labels.back.c_str(), GoBack{}, snap.can_go_back,
                                  paint_back_icon, 0x6201u);
                toolbar_separator();
                navigation_button(state.labels.forward.c_str(), GoForward{}, snap.can_go_forward,
                                  paint_forward_icon, 0x6202u);
            });
        widget::text(snap.relative_location == "Demo Root"
            ? state.labels.title
            : snap.relative_location,
            TextSize::Heading);
        layout::weighted(1.0f, [] {});
        layout::toolbar(
            toolbar_group_options("View Controls", 216.0f),
            [&] {
                view_mode_button("Icon View", FinderViewMode::Icon,
                                 state.view_mode, paint_icon_view, 0x6301u);
                view_mode_button("List View", FinderViewMode::List,
                                 state.view_mode, paint_list_view, 0x6302u);
                toolbar_separator();
                view_mode_button("Column View", FinderViewMode::Column,
                                 state.view_mode, paint_column_view, 0x6303u);
                toolbar_separator();
                view_mode_button("Gallery View", FinderViewMode::Gallery,
                                 state.view_mode, paint_gallery_view, 0x6304u);
            });
        layout::toolbar(
            toolbar_group_options("File Actions", 172.0f),
            [&] {
                file_action_button(state.labels.create_file.c_str(), CreateFile{},
                                   snap.can_create_file,
                                   paint_new_file_icon, 0x6701u);
                file_action_button(state.labels.create_folder.c_str(), CreateFolder{},
                                   snap.can_create_folder,
                                   paint_new_folder_icon, 0x6704u);
                file_action_button(state.labels.duplicate.c_str(), DuplicateSelected{},
                                   snap.can_duplicate_selected,
                                   paint_duplicate_icon, 0x6702u);
                file_action_button(state.labels.delete_action.c_str(), DeleteSelected{},
                                   snap.can_delete_selected,
                                   paint_delete_icon, 0x6703u);
            });
        layout::toolbar(
            toolbar_group_options("Group Sort", 48.0f),
            [&] {
                sort_action_button(snap);
            });
        layout::toolbar(
            toolbar_group_options("Share Tag More", 128.0f),
            [] {
                toolbar_action_button("Share", paint_share_icon, 0x6601u);
                toolbar_action_button("Tag", paint_tag_icon, 0x6602u);
                toolbar_action_button("More", paint_more_icon, 0x6603u);
            });
        layout::toolbar(
            toolbar_group_options(
                "Search Control",
                state.search_visible ? 220.0f : 48.0f),
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

void finder_grid(State const& state,
                 file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    auto const chrome = file_explorer_demo::explorer_chrome_metrics(
        state.explorer,
        "desktop");
    layout::material_surface(
        content_surface_options(),
        [&] {
            if (entries.empty()) {
                widget::text(state.labels.no_matching_files);
                return;
            }
            layout::spacer(18.0f);
            layout::scroll_view(chrome.icon_grid_scroll_height, [&] {
                auto columns = file_explorer_demo::explorer_icon_grid_columns(chrome);
                layout::grid(std::move(columns), chrome.icon_grid_row_height, [&] {
                    for (auto const& entry : entries) {
                        bool const selected = snap.has_selection
                            && snap.selected.name == entry.name;
                        layout::column([&] {
                            widget::canvas(126.0f, 86.0f,
                                [entry, selected](Painter& painter) {
                                    paint_item_thumbnail(painter, entry, selected);
                                },
                                {},
                                stable_token(entry.name)
                                    ^ (selected ? 0x100000u : 0u));
                            finder_icon_label_button(
                                entry.name,
                                selected,
                                chrome.icon_grid_column_width,
                                15.0f);
                        }, SpaceToken::Xs,
                           CrossAxisAlignment::Center,
                           MainAxisAlignment::Start);
                    }
                }, 24.0f);
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
                                finder_button(
                                    entry.name,
                                    SelectEntry{entry.name},
                                    selected,
                                    410.0f,
                                    15.0f,
                                    false,
                                    !selected);
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
    layout::material_surface(
        content_surface_options(),
        [&] {
            layout::row([&] {
                layout::sized_box(210.0f, [&] {
                    layout::column([&] {
                        widget::text(state.labels.locations, TextSize::Small, TextColor::Muted);
                        finder_button("Demo Root", SelectLocation{"root"},
                                      snap.relative_location == "Demo Root",
                                      190.0f, 14.0f, false, true);
                        finder_button(state.labels.documents, SelectLocation{"documents"},
                                      snap.relative_location == "Demo Root/Documents",
                                      190.0f, 14.0f, false, true);
                        finder_button(state.labels.pictures, SelectLocation{"pictures"},
                                      snap.relative_location == "Demo Root/Pictures",
                                      190.0f, 14.0f, false, true);
                        finder_button(state.labels.sidebar_shared, SelectLocation{"shared"},
                                      snap.relative_location == "Demo Root/Shared",
                                      190.0f, 14.0f, false, true);
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
                                    finder_button(
                                        entry.folder
                                            ? entry.name + " >"
                                            : entry.name,
                                        SelectEntry{entry.name},
                                        selected,
                                        330.0f,
                                        14.0f,
                                        false,
                                        !selected);
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
                                [entry = snap.selected](Painter& painter) {
                                    paint_item_thumbnail(painter, entry, true);
                                },
                                {},
                                stable_token(snap.selected.name) ^ 0x440000u);
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
    file_explorer_demo::Entry hero = snap.has_selection && !snap.selected.name.empty()
        ? snap.selected
        : (entries.empty() ? file_explorer_demo::Entry{} : entries.front());
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
                    [hero](Painter& painter) {
                        paint_item_thumbnail(painter, hero, true);
                    },
                    {},
                    stable_token(hero.name) ^ 0x550000u);
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
                        layout::column([&] {
                            widget::canvas(128.0f, 76.0f,
                                [entry, selected](Painter& painter) {
                                    paint_item_thumbnail(painter, entry, selected);
                                },
                                {},
                                stable_token(entry.name)
                                    ^ (selected ? 0x560000u : 0u));
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
    switch (state.view_mode) {
        case FinderViewMode::List:
            finder_list(state, snap);
            return;
        case FinderViewMode::Column:
            finder_column_view(state, snap);
            return;
        case FinderViewMode::Gallery:
            finder_gallery_view(state, snap);
            return;
        case FinderViewMode::Icon:
        default:
            finder_grid(state, snap);
            return;
    }
}

void finder_status_bar(State const& state,
                       file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::status_bar(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Clear,
            .direction = FlexDirection::Column,
            .padding = SpaceToken::Xs,
            .gap = SpaceToken::Xs,
            .border_radius = 12.0f,
            .border_width = 0.0f,
            .semantic_label = "Status Bar",
        },
        [&] {
            layout::row([&] {
                layout::weighted(1.0f, [&] {
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
                    widget::text(status, TextSize::Small, TextColor::Muted);
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

void view(State const& state) {
    using namespace phenotype;
    auto snap = file_explorer_demo::snapshot(state.explorer);
    layout::material_container(
        layout::MaterialContainerOptions{
            .container_id = 2100u,
            .spacing = 16.0f,
            .morph_transitions = true,
        },
        [&] {
            layout::material_surface(
                layout::MaterialSurfaceOptions{
                    .kind = MaterialKind::Clear,
                    .role = MaterialSurfaceRole::Surface,
                    .direction = FlexDirection::Row,
                    .padding = SpaceToken::Xs,
                    .gap = SpaceToken::Sm,
                    .cross_align = CrossAxisAlignment::Start,
                    .main_align = MainAxisAlignment::Start,
                    .max_width = 0.0f,
                    .fixed_height = -1.0f,
                    .border_radius = k_window_radius,
                    .border_width = 0.0f,
                    .semantic_label = "Finder Window",
                },
                [&] {
                    finder_sidebar(state);
                    layout::weighted(1.0f, [&] {
                        layout::column([&] {
                            finder_toolbar(state, snap);
                            finder_content(state, snap);
                            finder_status_bar(state, snap);
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
    phenotype::set_theme(theme);

    phenotype::native::WindowOptions window_options{
        .chrome = phenotype::native::WindowChromeStyle::IntegratedTitlebar,
        .integrated_titlebar = {
            .height = k_integrated_titlebar_height,
            .drag_region_height = k_integrated_titlebar_height,
            .leading_control_reserved_width = 176.0f,
            .trailing_control_reserved_width = 168.0f,
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

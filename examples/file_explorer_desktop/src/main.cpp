#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
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

struct SelectLocation { std::string id; };
struct SelectEntry { std::string name; };
struct SearchChanged { std::string text; };
struct DraftNameChanged { std::string text; };
struct DraftBodyChanged { std::string text; };
enum class FinderViewMode { Icon, List, Column, Gallery };
struct SetViewMode { FinderViewMode mode; };
struct ToolbarAction { std::string label; };
struct CreateFile {};
struct DeleteSelected {};
struct GoBack {};
struct GoForward {};
struct GoUp {};
struct Refresh {};
struct ResetDemo {};
struct Resized { int width; int height; float scale; };

using Msg = std::variant<
    SelectLocation,
    SelectEntry,
    SearchChanged,
    DraftNameChanged,
    DraftBodyChanged,
    SetViewMode,
    ToolbarAction,
    CreateFile,
    DeleteSelected,
    GoBack,
    GoForward,
    GoUp,
    Refresh,
    ResetDemo,
    Resized>;

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

file_explorer_demo::ExplorerState initial_explorer_state() {
    auto state = file_explorer_demo::make_state("desktop");
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_SCENARIO")) {
        file_explorer_demo::apply_startup_scenario(state, raw);
    }
    return state;
}

struct State {
    file_explorer_demo::ExplorerState explorer = initial_explorer_state();
    FinderViewMode view_mode = initial_view_mode();
};

constexpr float k_pi = 3.14159265358979323846f;
constexpr float k_tau = 6.28318530717958647692f;

phenotype::Color rgba(int r, int g, int b, int a = 255) {
    return phenotype::Color{
        static_cast<unsigned char>(r),
        static_cast<unsigned char>(g),
        static_cast<unsigned char>(b),
        static_cast<unsigned char>(a),
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
    auto path = rounded_rect_path(x, y, w, h, r);
    painter.stroke_path(path, thickness, color);
}

void paint_traffic_lights(phenotype::Painter& painter) {
    painter.arc(16.0f, 12.0f, 6.0f, 0.0f, k_tau, 10.0f, rgba(255, 95, 86));
    painter.arc(42.0f, 12.0f, 6.0f, 0.0f, k_tau, 10.0f, rgba(255, 189, 46));
    painter.arc(68.0f, 12.0f, 6.0f, 0.0f, k_tau, 10.0f, rgba(39, 201, 63));
}

void paint_sidebar_icon(phenotype::Painter& painter, std::string_view id) {
    using phenotype::FontWeight;
    using phenotype::FontSpec;
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
                     FontSpec{.weight = FontWeight::Bold});
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
                 9.0f, rgba(80, 87, 96));
}

void paint_image_thumbnail(phenotype::Painter& painter,
                           bool selected) {
    auto border = selected ? rgba(0, 122, 255) : rgba(224, 228, 234);
    fill_round(painter, 18.0f, 30.0f, 92.0f, 22.0f, 8.0f, rgba(0, 0, 0, 20));
    fill_round(painter, 16.0f, 28.0f, 92.0f, 22.0f, 8.0f, rgba(255, 255, 255, 248));
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
    fill_rect(painter, 15.0f, 22.0f, 92.0f, 9.0f, rgba(42, 123, 222, 210));
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
    std::string text = std::to_string(snap.file_count) + " files";
    if (snap.folder_count > 0)
        text += ", " + std::to_string(snap.folder_count) + " folders";
    if (snap.has_selection)
        text += " - selected " + snap.selected.name;
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

std::vector<file_explorer_demo::Entry> finder_entries(
        file_explorer_demo::Snapshot const& snap) {
    auto entries = snap.entries;
    std::stable_sort(entries.begin(), entries.end(),
                     [](auto const& a, auto const& b) {
        if (a.folder != b.folder)
            return !a.folder && b.folder;
        return file_explorer_demo::lower_copy(a.name)
            < file_explorer_demo::lower_copy(b.name);
    });
    return entries;
}

std::string entry_kind(file_explorer_demo::Entry const& entry) {
    if (entry.folder)
        return "Folder";
    auto ext = extension_lower(entry.name);
    if (ext.empty())
        return "Document";
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return ext + " File";
}

std::string entry_size_label(file_explorer_demo::Entry const& entry) {
    return entry.folder
        ? "--"
        : file_explorer_demo::format_size(entry.size);
}

Msg on_search_changed(std::string text) {
    return SearchChanged{std::move(text)};
}

Msg on_draft_name_changed(std::string text) {
    return DraftNameChanged{std::move(text)};
}

Msg on_draft_body_changed(std::string text) {
    return DraftBodyChanged{std::move(text)};
}

void update(State& state, Msg msg) {
    auto& explorer = state.explorer;
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, SelectLocation>) {
            file_explorer_demo::select_location(explorer, m.id);
        } else if constexpr (std::same_as<T, SelectEntry>) {
            file_explorer_demo::select_entry(explorer, m.name);
        } else if constexpr (std::same_as<T, SearchChanged>) {
            explorer.search = m.text;
            explorer.status = explorer.search.empty()
                ? "Filter cleared."
                : "Filtering by " + explorer.search;
        } else if constexpr (std::same_as<T, DraftNameChanged>) {
            explorer.draft_name = m.text;
        } else if constexpr (std::same_as<T, DraftBodyChanged>) {
            explorer.draft_body = m.text;
        } else if constexpr (std::same_as<T, SetViewMode>) {
            state.view_mode = m.mode;
            explorer.status = "Switched to " + view_mode_name(m.mode) + ".";
        } else if constexpr (std::same_as<T, ToolbarAction>) {
            explorer.status = m.label + " action is available in the native toolbar contract.";
        } else if constexpr (std::same_as<T, CreateFile>) {
            file_explorer_demo::create_file(explorer);
        } else if constexpr (std::same_as<T, DeleteSelected>) {
            file_explorer_demo::delete_selected(explorer);
        } else if constexpr (std::same_as<T, GoBack>) {
            file_explorer_demo::go_back(explorer);
        } else if constexpr (std::same_as<T, GoForward>) {
            file_explorer_demo::go_forward(explorer);
        } else if constexpr (std::same_as<T, GoUp>) {
            file_explorer_demo::go_up(explorer);
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

void icon_canvas(std::string_view id) {
    using namespace phenotype;
    widget::canvas(28.0f, 28.0f,
        [name = std::string(id)](Painter& painter) {
            paint_sidebar_icon(painter, name);
        },
        {},
        stable_token(std::string(id)) | 0x2000u);
}

void sidebar_row(std::string_view label,
                 std::string_view icon,
                 std::string location_id,
                 bool selected = false) {
    using namespace phenotype;
    layout::row([&] {
        icon_canvas(icon);
        finder_button(
            std::string(label),
            SelectLocation{std::move(location_id)},
            selected,
            178.0f,
            17.0f,
            false,
            false);
    }, SpaceToken::Xs, CrossAxisAlignment::Center, MainAxisAlignment::Start);
}

void finder_sidebar(State const& state) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    auto relative = file_explorer_demo::relative_location(
        explorer.root,
        explorer.current);
    bool const in_root = relative == "Demo Root";
    layout::sidebar(252.0f, [&] {
        widget::canvas(86.0f, 24.0f, paint_traffic_lights, {}, 0x51u);
        layout::spacer(28);
        sidebar_row("Recents", "recents", "root", in_root);
        sidebar_row("Shared", "folder", "shared",
                    relative == "Demo Root/Shared");
        layout::spacer(16);
        widget::text("Favorites", TextSize::Small, TextColor::Muted);
        sidebar_row("Applications", "app", "root");
        sidebar_row("Desktop", "desktop", "root");
        sidebar_row("Documents", "doc", "documents",
                    relative == "Demo Root/Documents");
        sidebar_row("Downloads", "download", "root");
        layout::spacer(16);
        widget::text("Locations", TextSize::Small, TextColor::Muted);
        sidebar_row("iCloud Drive", "cloud", "root");
        sidebar_row("kakao", "home", "root");
        sidebar_row("AirDrop", "airdrop", "shared");
        sidebar_row("Trash", "trash", "root");
    }, MaterialKind::Thin, SpaceToken::Lg, SpaceToken::Xs);
}

phenotype::Color nav_icon_ink(bool enabled) {
    return enabled ? rgba(88, 88, 92) : rgba(188, 192, 198);
}

void paint_back_icon(phenotype::Painter& painter, bool enabled) {
    auto ink = nav_icon_ink(enabled);
    painter.line(27.0f, 10.0f, 17.0f, 18.0f, 3.0f, ink);
    painter.line(17.0f, 18.0f, 27.0f, 26.0f, 3.0f, ink);
}

void paint_forward_icon(phenotype::Painter& painter, bool enabled) {
    auto ink = nav_icon_ink(enabled);
    painter.line(17.0f, 10.0f, 27.0f, 18.0f, 3.0f, ink);
    painter.line(27.0f, 18.0f, 17.0f, 26.0f, 3.0f, ink);
}

phenotype::ButtonStyleOptions toolbar_icon_button_options(
        bool selected = false,
        bool disabled = false) {
    auto const& t = phenotype::current_theme();
    phenotype::ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected ? rgba(229, 229, 234, 180) : t.transparent;
    options.has_hover_background = true;
    options.hover_background = selected
        ? rgba(216, 216, 222, 215)
        : rgba(229, 229, 234, 120);
    options.has_border_color = true;
    options.border_color = t.transparent;
    options.border_width = 0.0f;
    options.border_radius = 18.0f;
    options.max_width = 44.0f;
    options.fixed_height = 36.0f;
    options.disabled = disabled;
    return options;
}

phenotype::Color toolbar_icon_ink(bool selected = false) {
    return selected ? rgba(42, 42, 42) : rgba(70, 70, 70);
}

void paint_icon_view(phenotype::Painter& painter, bool selected) {
    auto ink = toolbar_icon_ink(selected);
    for (int col = 0; col < 2; ++col) {
        for (int row = 0; row < 2; ++row) {
            stroke_round(painter,
                         13.0f + static_cast<float>(col) * 15.0f,
                         8.0f + static_cast<float>(row) * 14.0f,
                         10.0f, 10.0f, 2.0f, 2.0f, ink);
        }
    }
}

void paint_list_view(phenotype::Painter& painter, bool selected) {
    auto ink = toolbar_icon_ink(selected);
    for (int i = 0; i < 4; ++i) {
        float y = 8.0f + static_cast<float>(i) * 6.8f;
        painter.line(11.0f, y, 13.0f, y, 3.0f, ink);
        painter.line(20.0f, y, 34.0f, y, 2.2f, ink);
    }
}

void paint_column_view(phenotype::Painter& painter, bool selected) {
    auto ink = toolbar_icon_ink(selected);
    for (int i = 0; i < 3; ++i) {
        stroke_round(painter, 9.0f + static_cast<float>(i) * 10.5f,
                     8.0f, 8.0f, 20.0f, 2.0f, 2.0f, ink);
    }
}

void paint_gallery_view(phenotype::Painter& painter, bool selected) {
    auto ink = toolbar_icon_ink(selected);
    stroke_round(painter, 10.0f, 8.0f, 24.0f, 17.0f, 4.0f, 2.0f, ink);
    painter.line(12.0f, 29.0f, 32.0f, 29.0f, 2.6f, ink);
}

void paint_group_sort_icon(phenotype::Painter& painter) {
    auto ink = toolbar_icon_ink();
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            stroke_round(painter,
                         9.0f + static_cast<float>(col) * 7.8f,
                         7.0f + static_cast<float>(row) * 7.0f,
                         7.0f, 4.0f, 1.0f, 1.7f, ink);
        }
    }
    painter.line(33.0f, 15.0f, 37.0f, 19.0f, 2.2f, ink);
    painter.line(41.0f, 15.0f, 37.0f, 19.0f, 2.2f, ink);
}

void paint_share_icon(phenotype::Painter& painter) {
    auto ink = rgba(170, 174, 181);
    painter.line(22.0f, 8.0f, 22.0f, 23.0f, 2.2f, ink);
    painter.line(16.0f, 14.0f, 22.0f, 8.0f, 2.2f, ink);
    painter.line(28.0f, 14.0f, 22.0f, 8.0f, 2.2f, ink);
    stroke_round(painter, 12.0f, 21.0f, 20.0f, 11.0f, 3.0f, 2.0f, ink);
}

void paint_tag_icon(phenotype::Painter& painter) {
    auto ink = rgba(170, 174, 181);
    auto tag = rounded_rect_path(8.0f, 9.0f, 28.0f, 18.0f, 4.0f);
    painter.stroke_path(tag, 2.0f, rgba(170, 174, 181));
    painter.arc(14.0f, 15.0f, 2.4f, 0.0f, k_tau, 1.6f, ink);
}

void paint_more_icon(phenotype::Painter& painter) {
    auto ink = toolbar_icon_ink();
    for (int i = 0; i < 3; ++i) {
        painter.arc(15.0f + static_cast<float>(i) * 7.0f,
                    18.0f, 2.2f, 0.0f, k_tau, 1.8f, ink);
    }
}

void paint_search_icon(phenotype::Painter& painter) {
    auto ink = toolbar_icon_ink();
    painter.arc(19.0f, 15.0f, 8.0f, 0.0f, k_tau, 2.4f, ink);
    painter.line(25.0f, 21.0f, 33.0f, 29.0f, 2.4f, ink);
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
        44.0f,
        36.0f,
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
        44.0f,
        36.0f,
        [paint](phenotype::Painter& painter) {
            paint(painter);
        },
        ToolbarAction{semantic_label},
        toolbar_icon_button_options(false),
        token);
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
        44.0f,
        36.0f,
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
    auto const& explorer = state.explorer;
    layout::toolbar([&] {
        layout::material_surface(
            layout::MaterialSurfaceOptions{
                .kind = MaterialKind::Thick,
                .role = MaterialSurfaceRole::Toolbar,
                .direction = FlexDirection::Row,
                .padding = SpaceToken::Xs,
                .gap = SpaceToken::Xs,
                .cross_align = CrossAxisAlignment::Center,
                .main_align = MainAxisAlignment::Start,
                .max_width = 104.0f,
                .fixed_height = 48.0f,
                .semantic_label = "Navigation Controls",
            },
            [&] {
                navigation_button("Back", GoBack{}, snap.can_go_back,
                                  paint_back_icon, 0x6201u);
                navigation_button("Forward", GoForward{}, snap.can_go_forward,
                                  paint_forward_icon, 0x6202u);
            });
        widget::text(snap.relative_location == "Demo Root"
            ? "Recents"
            : snap.relative_location,
            TextSize::Heading);
        layout::weighted(1.0f, [] {});
        layout::material_surface(
            layout::MaterialSurfaceOptions{
                .kind = MaterialKind::Thick,
                .role = MaterialSurfaceRole::Toolbar,
                .direction = FlexDirection::Row,
                .padding = SpaceToken::Xs,
                .gap = SpaceToken::Xs,
                .cross_align = CrossAxisAlignment::Center,
                .main_align = MainAxisAlignment::Start,
                .max_width = 260.0f,
                .fixed_height = 48.0f,
                .semantic_label = "View Controls",
            },
            [&] {
                view_mode_button("Icon View", FinderViewMode::Icon,
                                 state.view_mode, paint_icon_view, 0x6301u);
                view_mode_button("List View", FinderViewMode::List,
                                 state.view_mode, paint_list_view, 0x6302u);
                view_mode_button("Column View", FinderViewMode::Column,
                                 state.view_mode, paint_column_view, 0x6303u);
                view_mode_button("Gallery View", FinderViewMode::Gallery,
                                 state.view_mode, paint_gallery_view, 0x6304u);
            });
        layout::material_surface(
            layout::MaterialSurfaceOptions{
                .kind = MaterialKind::Thick,
                .role = MaterialSurfaceRole::Toolbar,
                .direction = FlexDirection::Row,
                .padding = SpaceToken::Xs,
                .gap = SpaceToken::Xs,
                .cross_align = CrossAxisAlignment::Center,
                .main_align = MainAxisAlignment::Start,
                .max_width = 82.0f,
                .fixed_height = 48.0f,
                .semantic_label = "Group Sort",
            },
            [] {
                toolbar_action_button(
                    "Group Sort", paint_group_sort_icon, 0x6501u);
            });
        layout::material_surface(
            layout::MaterialSurfaceOptions{
                .kind = MaterialKind::Thick,
                .role = MaterialSurfaceRole::Toolbar,
                .direction = FlexDirection::Row,
                .padding = SpaceToken::Xs,
                .gap = SpaceToken::Xs,
                .cross_align = CrossAxisAlignment::Center,
                .main_align = MainAxisAlignment::Start,
                .max_width = 164.0f,
                .fixed_height = 48.0f,
                .semantic_label = "Share Tag More",
            },
            [] {
                toolbar_action_button("Share", paint_share_icon, 0x6601u);
                toolbar_action_button("Tag", paint_tag_icon, 0x6602u);
                toolbar_action_button("More", paint_more_icon, 0x6603u);
            });
        layout::material_surface(
            layout::MaterialSurfaceOptions{
                .kind = MaterialKind::Thick,
                .role = MaterialSurfaceRole::Toolbar,
                .direction = FlexDirection::Row,
                .padding = SpaceToken::Xs,
                .gap = SpaceToken::Xs,
                .cross_align = CrossAxisAlignment::Center,
                .main_align = MainAxisAlignment::Start,
                .max_width = 56.0f,
                .fixed_height = 48.0f,
                .semantic_label = "Search Control",
            },
            [] {
                toolbar_action_button(
                    "Search Control", paint_search_icon, 0x6401u);
            });
        layout::sized_box(144.0f, [&] {
            widget::text_field<Msg>("Search", explorer.search, on_search_changed);
        });
    }, MaterialKind::Clear, SpaceToken::Sm, SpaceToken::Sm);
}

void finder_grid(file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Regular,
            .role = MaterialSurfaceRole::Content,
            .direction = FlexDirection::Column,
            .padding = SpaceToken::Lg,
            .gap = SpaceToken::Md,
            .cross_align = CrossAxisAlignment::Start,
            .main_align = MainAxisAlignment::Start,
            .max_width = 0.0f,
            .fixed_height = -1.0f,
            .semantic_label = "Files",
        },
        [&] {
            if (entries.empty()) {
                widget::text("No matching files.");
                return;
            }
            layout::scroll_view(552.0f, [&] {
                std::vector<float> columns{
                    142.0f, 142.0f, 142.0f, 142.0f, 142.0f, 142.0f,
                };
                layout::grid(std::move(columns), 166.0f, [&] {
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
                            finder_button(
                                entry.name,
                                SelectEntry{entry.name},
                                selected,
                                142.0f,
                                15.0f,
                                true);
                        }, SpaceToken::Xs,
                           CrossAxisAlignment::Center,
                           MainAxisAlignment::Start);
                    }
                }, 24.0f);
            }, SpaceToken::Sm);
        });
}

void finder_list(file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Regular,
            .role = MaterialSurfaceRole::Content,
            .direction = FlexDirection::Column,
            .padding = SpaceToken::Lg,
            .gap = SpaceToken::Sm,
            .cross_align = CrossAxisAlignment::Start,
            .main_align = MainAxisAlignment::Start,
            .max_width = 0.0f,
            .fixed_height = -1.0f,
            .semantic_label = "Files",
        },
        [&] {
            layout::row([&] {
                layout::sized_box(420.0f, [&] {
                    widget::text("Name", TextSize::Small, TextColor::Muted);
                });
                layout::sized_box(160.0f, [&] {
                    widget::text("Kind", TextSize::Small, TextColor::Muted);
                });
                layout::sized_box(120.0f, [&] {
                    widget::text("Size", TextSize::Small, TextColor::Muted);
                });
            }, SpaceToken::Sm, CrossAxisAlignment::Center, MainAxisAlignment::Start);
            if (entries.empty()) {
                widget::text("No matching files.");
                return;
            }
            layout::scroll_view(552.0f, [&] {
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
                                widget::text(entry_kind(entry),
                                             TextSize::Small,
                                             TextColor::Muted);
                            });
                            layout::sized_box(120.0f, [&] {
                                widget::text(entry_size_label(entry),
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

void finder_column_view(file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Regular,
            .role = MaterialSurfaceRole::Content,
            .direction = FlexDirection::Column,
            .padding = SpaceToken::Lg,
            .gap = SpaceToken::Md,
            .cross_align = CrossAxisAlignment::Start,
            .main_align = MainAxisAlignment::Start,
            .max_width = 0.0f,
            .fixed_height = -1.0f,
            .semantic_label = "Files",
        },
        [&] {
            layout::row([&] {
                layout::sized_box(210.0f, [&] {
                    layout::column([&] {
                        widget::text("Locations", TextSize::Small, TextColor::Muted);
                        finder_button("Demo Root", SelectLocation{"root"},
                                      snap.relative_location == "Demo Root",
                                      190.0f, 14.0f, false, true);
                        finder_button("Documents", SelectLocation{"documents"},
                                      snap.relative_location == "Demo Root/Documents",
                                      190.0f, 14.0f, false, true);
                        finder_button("Pictures", SelectLocation{"pictures"},
                                      snap.relative_location == "Demo Root/Pictures",
                                      190.0f, 14.0f, false, true);
                        finder_button("Shared", SelectLocation{"shared"},
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
                            widget::text("No matching files.");
                            return;
                        }
                        layout::scroll_view(500.0f, [&] {
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
                        widget::text("Preview", TextSize::Small, TextColor::Muted);
                        if (snap.has_selection) {
                            widget::canvas(160.0f, 110.0f,
                                [entry = snap.selected](Painter& painter) {
                                    paint_item_thumbnail(painter, entry, true);
                                },
                                {},
                                stable_token(snap.selected.name) ^ 0x440000u);
                            widget::text(snap.selected.name, TextSize::Heading);
                            widget::text(entry_kind(snap.selected),
                                         TextSize::Small,
                                         TextColor::Muted);
                            widget::text(compact_preview(snap.preview),
                                         TextSize::Small,
                                         TextColor::Muted);
                        } else {
                            widget::text("Select a file to preview.");
                        }
                    }, SpaceToken::Sm);
                });
            }, SpaceToken::Md, CrossAxisAlignment::Start, MainAxisAlignment::Start);
        });
}

void finder_gallery_view(file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    file_explorer_demo::Entry hero = snap.has_selection && !snap.selected.name.empty()
        ? snap.selected
        : (entries.empty() ? file_explorer_demo::Entry{} : entries.front());
    bool const has_hero = !hero.name.empty();
    layout::material_surface(
        layout::MaterialSurfaceOptions{
            .kind = MaterialKind::Regular,
            .role = MaterialSurfaceRole::Content,
            .direction = FlexDirection::Column,
            .padding = SpaceToken::Lg,
            .gap = SpaceToken::Md,
            .cross_align = CrossAxisAlignment::Start,
            .main_align = MainAxisAlignment::Start,
            .max_width = 0.0f,
            .fixed_height = -1.0f,
            .semantic_label = "Files",
        },
        [&] {
            if (!has_hero) {
                widget::text("No matching files.");
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
                        widget::text(entry_kind(hero),
                                     TextSize::Small,
                                     TextColor::Muted);
                        widget::text(snap.has_selection
                            ? compact_preview(snap.preview)
                            : "Select a file below to read its preview.",
                            TextSize::Small,
                            TextColor::Muted);
                    }, SpaceToken::Sm);
                });
            }, SpaceToken::Lg, CrossAxisAlignment::Center, MainAxisAlignment::Start);
            layout::scroll_view(366.0f, [&] {
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
            finder_list(snap);
            return;
        case FinderViewMode::Column:
            finder_column_view(snap);
            return;
        case FinderViewMode::Gallery:
            finder_gallery_view(snap);
            return;
        case FinderViewMode::Icon:
        default:
            finder_grid(snap);
            return;
    }
}

void finder_status_bar(State const& state,
                       file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::status_bar([&] {
        layout::row([&] {
            layout::weighted(1.0f, [&] {
                widget::text(finder_status(snap), TextSize::Small, TextColor::Muted);
                widget::text(explorer.status, TextSize::Small, TextColor::Muted);
                if (snap.has_selection) {
                    widget::text(compact_preview(snap.preview),
                                 TextSize::Small,
                                 TextColor::Muted);
                }
            });
            layout::sized_box(170.0f, [&] {
                widget::text_field<Msg>("File name", explorer.draft_name,
                                        on_draft_name_changed);
            });
            layout::sized_box(210.0f, [&] {
                widget::text_field<Msg>("File contents", explorer.draft_body,
                                        on_draft_body_changed);
            });
            widget::button<Msg>("Create", CreateFile{}, ButtonVariant::Primary);
            widget::button<Msg>("Up", GoUp{});
            widget::button<Msg>("Delete", DeleteSelected{});
            widget::button<Msg>("Reset", ResetDemo{});
            widget::button<Msg>("Refresh", Refresh{});
        }, SpaceToken::Sm, CrossAxisAlignment::Center, MainAxisAlignment::Start);
    }, MaterialKind::Clear, SpaceToken::Sm, SpaceToken::Xs);
}

void view(State const& state) {
    using namespace phenotype;
    auto snap = file_explorer_demo::snapshot(state.explorer);
    layout::padded(SpaceToken::Sm, [&] {
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
    });
}

} // namespace

int main() {
    phenotype::Theme theme = phenotype::current_theme();
    theme.background = rgba(246, 246, 246);
    theme.foreground = rgba(29, 29, 31);
    theme.accent = rgba(0, 122, 255);
    theme.accent_strong = rgba(0, 99, 204);
    theme.muted = rgba(112, 112, 117);
    theme.border = rgba(226, 226, 230);
    theme.surface = rgba(255, 255, 255);
    theme.code_bg = rgba(242, 242, 247);
    theme.body_font_size = 15.0f;
    theme.heading_font_size = 21.0f;
    theme.small_font_size = 13.0f;
    theme.radius_sm = 10.0f;
    theme.radius_md = 14.0f;
    theme.radius_lg = 22.0f;
    phenotype::set_theme(theme);

    return phenotype::native::run_app<State, Msg>(
        1300,
        760,
        "phenotype file explorer desktop",
        view,
        update,
        [](int width, int height, float scale) -> Msg {
            return Resized{width, height, scale};
        });
}

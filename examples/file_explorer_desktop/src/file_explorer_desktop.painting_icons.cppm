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

export module file_explorer_desktop:painting_icons;

import file_explorer_shared;
import phenotype;
import phenotype.native;
import :messages_and_startup;
import :input_messages;
import :runtime_preferences;
import :state_and_debug;
import :style;

export namespace file_explorer_desktop {
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
    auto const ink = selected
        ? phenotype::current_theme().accent
        : (state.hovered ? finder_primary_ink() : finder_secondary_ink());
    auto presentation = icon_presentation_for_state(
        sidebar_symbol(id),
        phenotype::icons::SymbolPresentationRole::Sidebar,
        selected,
        state);
    presentation.color = ink;
    paint_finder_symbol_centered(
        painter,
        cache,
        presentation,
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

std::uint64_t stable_token(std::string_view value) {
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char ch : value) {
        h ^= ch;
        h *= 1099511628211ull;
    }
    return h ? h : 1ull;
}

phenotype::Color entry_symbol_color(
        file_explorer_demo::Entry const& entry,
        bool selected);

void paint_item_thumbnail(phenotype::Painter& painter,
                          phenotype::icons::SymbolDocumentCache const& cache,
                          file_explorer_demo::Entry const& entry,
                          bool selected,
                          std::string_view preview_source = {}) {
    (void)preview_source;
    auto const symbol = phenotype::icons::from_catalog_symbol(
        file_explorer_demo::entry_symbol(entry));
    auto const box_w = file_explorer_demo::k_desktop_icon_grid_thumbnail_width;
    auto const box_h = file_explorer_demo::k_desktop_icon_grid_thumbnail_height;
    auto const icon_box = std::min(box_w, box_h);
    auto const icon_x = (box_w - icon_box) * 0.5f;
    auto const icon_y = (box_h - icon_box) * 0.5f;
    auto const icon_size = entry.folder ? 58.0f : 54.0f;
    auto color = entry_symbol_color(entry, selected);
    if (!selected) {
        color = finder_dark_palette()
            ? (entry.folder ? finder_primary_ink() : finder_secondary_ink())
            : (entry.folder ? rgba(92, 92, 96) : rgba(72, 72, 76));
    }
    if (selected) {
        fill_round(painter, 20.0f, 3.0f, box_w - 40.0f, box_h - 6.0f,
                   14.0f, rgba(0, 122, 255, 26));
    }
    auto presentation = phenotype::icons::presentation(
        symbol,
        phenotype::icons::SymbolPresentationRole::FileType,
        selected ? phenotype::icons::SymbolTone::Selected
                 : phenotype::icons::SymbolTone::Secondary,
        phenotype::icons::SymbolScale::Large);
    presentation.point_size = icon_size;
    presentation.color = color;
    presentation.stroke_scale = k_file_thumbnail_symbol_stroke_scale;
    presentation = phenotype::icons::with_material_symbol_axes(
        presentation,
        phenotype::icons::MaterialSymbolAxes{
            .fill = true,
            .weight = 500,
            .grade = 0,
            .optical_size = 40,
            .style = phenotype::icons::MaterialSymbolsStyle::Outlined,
        });
    paint_finder_symbol_centered(
        painter,
        cache,
        presentation,
        icon_x,
        icon_y,
        icon_box,
        icon_box);
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
    token ^= 0x200000u;
    return token;
}

phenotype::Color entry_symbol_color(
        file_explorer_demo::Entry const& entry,
        bool selected) {
    if (selected) {
        return entry.folder
            ? phenotype::current_theme().accent
            : phenotype::current_theme().accent_strong;
    }
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

} // namespace file_explorer_desktop

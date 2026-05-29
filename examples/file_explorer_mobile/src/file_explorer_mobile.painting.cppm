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

export module file_explorer_mobile:painting;

import file_explorer_shared;
import phenotype;
import phenotype.native;
import :messages_and_runtime;
import :state_and_debug;
import :input_messages;

export namespace file_explorer_mobile {
phenotype::ButtonStyleOptions mobile_control_button_style(
        bool enabled = true,
        bool selected = false,
        float width = 0.0f,
        float height = -1.0f,
        float border_radius = 14.0f,
        phenotype::MaterialSurfaceRole role =
            phenotype::MaterialSurfaceRole::Control) {
    using namespace phenotype;
    auto const& t = current_theme();
    ButtonStyleOptions options;
    options.disabled = !enabled;
    options.has_background = true;
    options.background = selected ? t.accent : Color{255, 255, 255, 0};
    options.has_hover_background = true;
    options.hover_background = selected
        ? t.accent_strong
        : Color{255, 255, 255, 118};
    options.has_pressed_background = true;
    options.pressed_background = selected
        ? t.accent_strong
        : Color{229, 229, 234, 178};
    options.has_border_color = true;
    options.border_color = Color{255, 255, 255, 0};
    options.has_text_color = true;
    options.text_color = selected ? t.state_active_fg : t.foreground;
    options.border_width = 0.0f;
    options.border_radius = border_radius;
    options.max_width = width;
    options.fixed_height = height;
    options.min_hit_width = width > 0.0f ? width : minimum_button_activation_size;
    options.min_hit_height = height > 0.0f ? height : minimum_button_activation_size;
    options.focus_ring = false;
    return widget::interaction_glass_button_style(
        options,
        role,
        MaterialKind::Clear,
        MaterialKind::Regular);
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
        },
        mobile_control_button_style(
            enabled,
            selected,
            38.0f,
            38.0f,
            14.0f,
            phenotype::MaterialSurfaceRole::Control));
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
    auto options = widget::interaction_glass_button_style(
        widget::glass_selection_button_style(
            GlassSelectionStyleOptions{
                .role = MaterialSurfaceRole::Surface,
                .selected = selected,
                .width = row_width,
                .height = 60.0f,
                .border_radius = 14.0f,
            }),
        MaterialSurfaceRole::Surface,
        MaterialKind::Clear,
        MaterialKind::Regular);
    if (!selected) {
        options.background = Color{
            255,
            255,
            255,
            static_cast<unsigned char>(entry.folder ? 54 : 84)};
        options.hover_background = Color{255, 255, 255, 132};
        options.pressed_background = Color{229, 229, 234, 180};
    }

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

} // namespace file_explorer_mobile

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

export module file_explorer_desktop:overflow_and_grid;

import file_explorer_shared;
import phenotype;
import phenotype.native;
import :messages_and_startup;
import :input_messages;
import :runtime_preferences;
import :state_and_debug;
import :style;
import :painting_icons;
import :text_and_status;
import :update;
import :buttons_and_sidebar;
import :toolbar;

export namespace file_explorer_desktop {

void finder_grid(State const& state,
                 file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto entries = finder_entries(snap);
    auto const chrome = file_explorer_demo::explorer_chrome_metrics(
        state.explorer,
        "desktop");
    auto const& icon_cache = state.icon_cache;
    layout::material_surface(
        content_section_options(),
        [&] {
            if (entries.empty()) {
                widget::text(state.labels.no_matching_files);
                return;
            }
            layout::spacer(chrome.icon_grid_top_inset);
            layout::scroll_view(chrome.icon_grid_scroll_height, [&] {
                auto columns = file_explorer_demo::explorer_icon_grid_columns(chrome);
                layout::grid(std::move(columns), chrome.icon_grid_row_height, [&] {
                    for (auto const& entry : entries) {
                        bool const selected = snap.has_selection
                            && snap.selected.name == entry.name;
                        auto const preview_source = selected
                            ? snap.preview
                            : std::string{};
                        layout::column([&] {
                            widget::canvas(chrome.icon_grid_thumbnail_width,
                                chrome.icon_grid_thumbnail_height,
                                [entry, selected, preview_source, &icon_cache](
                                        Painter& painter) {
                                    paint_item_thumbnail(
                                        painter,
                                        icon_cache,
                                        entry,
                                        selected,
                                        preview_source);
                                },
                                {},
                                thumbnail_paint_token(
                                    entry,
                                    selected,
                                    preview_source));
                            finder_icon_label_button(
                                entry.name,
                                selected,
                                chrome.icon_grid_column_width,
                                chrome.icon_grid_label_font_size,
                                chrome.icon_grid_label_height);
                        }, SpaceToken::Xs,
                           CrossAxisAlignment::Center,
                           MainAxisAlignment::Start);
                    }
                }, chrome.icon_grid_gap);
            }, SpaceToken::Sm);
        });
}

void settings_choice_button(std::string_view label,
                            Msg msg,
                            bool selected,
                            std::uint64_t token) {
    using namespace phenotype;
    auto const& t = current_theme();
    bool const dark = finder_dark_palette();
    ButtonStyleOptions options;
    options.has_background = true;
    options.background = selected ? with_alpha(t.accent, 230)
                                  : (dark ? rgba(255, 255, 255, 24)
                                          : rgba(255, 255, 255, 210));
    options.has_hover_background = true;
    options.hover_background = selected ? with_alpha(t.accent_strong, 240)
                                        : (dark ? rgba(255, 255, 255, 38)
                                                : rgba(235, 235, 240, 240));
    options.has_pressed_background = true;
    options.pressed_background = selected ? with_alpha(t.accent_strong, 250)
                                          : (dark ? rgba(255, 255, 255, 52)
                                                  : rgba(220, 220, 226, 245));
    options.has_text_color = true;
    options.text_color = selected ? rgba(255, 255, 255)
                                  : t.foreground;
    options.has_border_color = true;
    options.border_color = selected ? with_alpha(t.accent, 0)
                                    : with_alpha(t.border, 190);
    options.border_width = selected ? 0.0f : 1.0f;
    options.border_radius = 10.0f;
    options.max_width = 132.0f;
    options.fixed_height = 34.0f;
    options.min_hit_width = 132.0f;
    options.min_hit_height = 34.0f;
    options.focus_ring = false;
    phenotype::keyed(static_cast<std::uint32_t>(token), [&] {
        widget::button<Msg>(str{std::string(label)}, std::move(msg), options);
    });
}

} // namespace file_explorer_desktop

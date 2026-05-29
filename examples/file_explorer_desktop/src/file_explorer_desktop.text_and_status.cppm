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

export module file_explorer_desktop:text_and_status;

import file_explorer_shared;
import phenotype;
import phenotype.native;
import :messages_and_startup;
import :input_messages;
import :runtime_preferences;
import :state_and_debug;
import :style;
import :painting_icons;

export namespace file_explorer_desktop {
std::string finder_status(file_explorer_demo::Snapshot const& snap) {
    std::string text = snap.item_summary;
    if (snap.has_selection)
        text += " - selected " + snap.selected.name;
    if (!snap.sort_label.empty())
        text += " - " + snap.sort_label;
    return text;
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

void pop_utf8_codepoint(std::string& text);

void paint_elided_finder_text(phenotype::Painter& painter,
                              float x,
                              float y,
                              std::string_view text,
                              float max_width,
                              float font_size,
                              phenotype::Color color) {
    if (max_width <= 0.0f || text.empty())
        return;

    auto const font = finder_font();
    auto fits = [&](std::string_view value) {
        return painter.measure_text(
            value.data(),
            static_cast<unsigned int>(value.size()),
            font_size,
            font) <= max_width;
    };
    if (fits(text)) {
        painter.text(x,
                     y,
                     text.data(),
                     static_cast<unsigned int>(text.size()),
                     font_size,
                     color,
                     font);
        return;
    }

    std::string shortened{text};
    while (!shortened.empty()) {
        pop_utf8_codepoint(shortened);
        auto candidate = shortened + "...";
        if (fits(candidate)) {
            painter.text(x,
                         y,
                         candidate.c_str(),
                         static_cast<unsigned int>(candidate.size()),
                         font_size,
                         color,
                         font);
            return;
        }
    }
    painter.text(x, y, "...", 3, font_size, color, font);
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

std::string middle_elide_icon_label_line(
        phenotype::Painter& painter,
        std::string text,
        float max_width,
        float font_size) {
    auto const font = finder_font();
    auto width_of = [&](std::string const& value) {
        return painter.measure_text(
            value.c_str(),
            static_cast<unsigned int>(value.size()),
            font_size,
            font);
    };
    if (width_of(text) <= max_width)
        return text;

    std::string suffix;
    std::string head = text;
    auto const last_break = text.find_last_of(" _-");
    if (last_break != std::string::npos
        && last_break + 1 < text.size()
        && text.size() - last_break - 1 <= 24) {
        head = trim_icon_label_line(text.substr(0, last_break));
        suffix = text.substr(last_break + 1);
    } else if (auto const dot = text.find_last_of('.');
               dot != std::string::npos
               && dot > 0
               && text.size() - dot <= 8) {
        head = text.substr(0, dot);
        suffix = text.substr(dot);
    }

    while (!head.empty() && width_of(head + "..." + suffix) > max_width)
        pop_utf8_codepoint(head);
    if (!head.empty())
        return head + "..." + suffix;

    head = text;
    while (!head.empty() && width_of(head + "...") > max_width)
        pop_utf8_codepoint(head);
    return head.empty() ? std::string{"..."} : head + "...";
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
    auto const tokens = icon_label_tokens(label);
    std::string first_line;
    std::size_t next_token = 0;
    for (; next_token < tokens.size(); ++next_token) {
        auto candidate = first_line + tokens[next_token];
        if (!first_line.empty() && width_of(candidate) > max_width)
            break;
        first_line = std::move(candidate);
    }

    auto first = trim_icon_label_line(std::move(first_line));
    if (first.empty())
        return {middle_elide_icon_label_line(
            painter,
            label,
            max_width,
            font_size)};

    if (next_token >= tokens.size())
        return {std::move(first)};

    std::string remainder;
    for (; next_token < tokens.size(); ++next_token)
        remainder += tokens[next_token];
    auto second = middle_elide_icon_label_line(
        painter,
        trim_icon_label_line(std::move(remainder)),
        max_width,
        font_size);
    return {std::move(first), std::move(second)};
}

} // namespace file_explorer_desktop

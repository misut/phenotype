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

export module file_explorer_shared:input_parsing;

import json;
import phenotype.icon_catalog;
import phenotype.resources;
import phenotype.theme_contract;
import :model_types;
import :desktop_metrics_and_symbols;
import :viewport_and_focus_helpers;
import :chrome_and_geometry;
import :filesystem_model;
import :icon_and_interaction_debug_json;
import :chrome_debug_json;
import :resources_debug_json;
import :debug_payload_json;

export namespace file_explorer_demo {
inline SortMode next_sort_mode(SortMode mode) {
    switch (mode) {
        case SortMode::Recent: return SortMode::Name;
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
        case ExplorerInputKind::FocusSearch: return "focus_search";
        case ExplorerInputKind::MoveSelection: return "move_selection";
        case ExplorerInputKind::OpenEntry: return "open_entry";
        case ExplorerInputKind::ActivateEntry: return "activate_entry";
        case ExplorerInputKind::ActivateSelected: return "activate_selected";
        case ExplorerInputKind::ViewMode: return "view_mode";
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
        case ExplorerInputKind::DismissTransient: return "dismiss_transient";
        case ExplorerInputKind::FocusTarget: return "focus_target";
        case ExplorerInputKind::PointerFocus: return "pointer_focus";
        case ExplorerInputKind::TabFocus: return "tab_focus";
        case ExplorerInputKind::ShiftTabFocus: return "shift_tab_focus";
        case ExplorerInputKind::SetFontFamily: return "set_font_family";
        case ExplorerInputKind::SetFontScale: return "set_font_scale";
        case ExplorerInputKind::SetBodyFontSize: return "set_body_font_size";
        case ExplorerInputKind::SetHeadingFontSize:
            return "set_heading_font_size";
        case ExplorerInputKind::SetSmallFontSize: return "set_small_font_size";
        case ExplorerInputKind::SetLineHeightRatio:
            return "set_line_height_ratio";
        case ExplorerInputKind::SetSystemFontMetrics:
            return "set_system_font_metrics";
        case ExplorerInputKind::SetSystemScrollMetrics:
            return "set_system_scroll_metrics";
        case ExplorerInputKind::SetScrollSpeed: return "set_scroll_speed";
        case ExplorerInputKind::SetHorizontalScrollSpeed:
            return "set_horizontal_scroll_speed";
        case ExplorerInputKind::SetScrollBarVisibility:
            return "set_scroll_bar_visibility";
        case ExplorerInputKind::SetMotionScale: return "set_motion_scale";
        case ExplorerInputKind::SetColorScheme: return "set_color_scheme";
    }
    return "noop";
}

inline std::string explorer_input_label(ExplorerInput const& input) {
    auto label = explorer_input_kind_name(input.kind);
    if (input.kind == ExplorerInputKind::Sort)
        return label + ":" + sort_mode_label(input.sort_mode);
    if (input.kind == ExplorerInputKind::ViewMode)
        return label + ":" + view_mode_value_name(input.view_mode);
    if (input.kind == ExplorerInputKind::MoveSelection)
        return label + ":" + selection_move_value_name(input.selection_move);
    if (input.kind == ExplorerInputKind::FocusTarget
        || input.kind == ExplorerInputKind::PointerFocus)
        return label + ":" + input.value;
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
        case ExplorerExpectationKind::FocusTarget:    return "focus-target";
        case ExplorerExpectationKind::FocusVisible:   return "focus-visible";
        case ExplorerExpectationKind::InputModality:  return "input-modality";
    }
    return "selected";
}

inline std::string compact_preference_number(float value) {
    auto out = std::to_string(value);
    while (!out.empty() && out.back() == '0')
        out.pop_back();
    if (!out.empty() && out.back() == '.')
        out.pop_back();
    return out.empty() ? "0" : out;
}

inline std::optional<float> parse_preference_number(
        std::string_view raw,
        float minimum,
        float maximum) {
    auto text = trim(raw);
    if (text.empty())
        return std::nullopt;
    auto owned = std::string{text};
    char* end = nullptr;
    auto value = std::strtof(owned.c_str(), &end);
    if (end == owned.c_str() || (end && *end != '\0'))
        return std::nullopt;
    if (!(value > 0.0f))
        return std::nullopt;
    if (value < minimum)
        value = minimum;
    if (value > maximum)
        value = maximum;
    return value;
}

inline std::optional<float> parse_motion_preference_number(
        std::string_view raw) {
    auto text = trim(raw);
    if (text.empty())
        return std::nullopt;
    auto owned = std::string{text};
    char* end = nullptr;
    auto value = std::strtof(owned.c_str(), &end);
    if (end == owned.c_str() || (end && *end != '\0'))
        return std::nullopt;
    if (!(value >= 0.0f))
        return std::nullopt;
    if (value > 4.0f)
        value = 4.0f;
    return value;
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
    if (name == "focus" || name == "focus-target"
        || name == "focus_target") {
        auto target = focus_target_from_name(value);
        if (!target) {
            return expectation_parse_error(
                "expectation 'focus-target' has unknown target: " + value);
        }
        return parsed_expectation({
            .kind = ExplorerExpectationKind::FocusTarget,
            .value = focus_target_value_name(*target),
        });
    }
    if (name == "focus-visible" || name == "focus_visible") {
        auto lowered = lower_copy(value);
        if (lowered != "true" && lowered != "false"
            && lowered != "1" && lowered != "0"
            && lowered != "yes" && lowered != "no") {
            return expectation_parse_error(
                "expectation 'focus-visible' requires true or false");
        }
        bool const visible = lowered == "true" || lowered == "1"
            || lowered == "yes";
        return parsed_expectation({
            .kind = ExplorerExpectationKind::FocusVisible,
            .value = visible ? "true" : "false",
        });
    }
    if (name == "modality" || name == "input-modality"
        || name == "input_modality") {
        auto lowered = lower_copy(value);
        if (lowered != "keyboard" && lowered != "pointer"
            && lowered != "programmatic" && lowered != "unspecified") {
            return expectation_parse_error(
                "expectation 'input-modality' requires keyboard, pointer, programmatic, or unspecified");
        }
        return parsed_expectation({
            .kind = ExplorerExpectationKind::InputModality,
            .value = lowered,
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
    if (name == "recent" || name == "recents")
        return SortMode::Recent;
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

    auto parse_move = [&](std::string_view move_name) -> ExplorerInputParseResult {
        auto move = selection_move_from_name(move_name);
        if (!move) {
            return input_parse_error(
                "unknown file explorer selection move: "
                + std::string{move_name});
        }
        return parsed_input(ExplorerInput{
            .kind = ExplorerInputKind::MoveSelection,
            .value = selection_move_value_name(*move),
            .selection_move = *move,
        });
    };

    auto parse_focus = [&](ExplorerInputKind kind,
                           ExplorerInputModality modality)
            -> ExplorerInputParseResult {
        if (value.empty()) {
            return input_parse_error(
                "input '" + name + "' requires a focus target");
        }
        auto target = focus_target_from_name(value);
        if (!target) {
            return input_parse_error(
                "unknown file explorer focus target: " + value);
        }
        return parsed_input(ExplorerInput{
            .kind = kind,
            .value = focus_target_value_name(*target),
            .modality = modality,
        });
    };

    if (name == "noop")
        return parsed_input({});
    if (name == "location" || name == "loc"
        || name == "select-location") {
        return require_value(ExplorerInputKind::SelectLocation);
    }
    if (name == "select" || name == "entry" || name == "select-entry"
        || name == "read") {
        return require_value(ExplorerInputKind::SelectEntry);
    }
    if (name == "open" || name == "open-entry")
        return require_value(ExplorerInputKind::OpenEntry);
    if (name == "click") {
        if (value.empty())
            return input_parse_error("input 'click' requires a value");
        return parsed_input(ExplorerInput{
            .kind = ExplorerInputKind::ActivateEntry,
            .value = value,
            .modality = ExplorerInputModality::Pointer,
        });
    }
    if (name == "activate" || name == "activate-entry")
        return require_value(ExplorerInputKind::ActivateEntry);
    if (name == "open-selected" || name == "open_selection"
        || name == "activate-selected" || name == "activate_selection"
        || name == "enter") {
        return parsed_input({
            .kind = ExplorerInputKind::ActivateSelected,
            .modality = ExplorerInputModality::Keyboard});
    }
    if (name == "tab")
        return parsed_input({
            .kind = ExplorerInputKind::TabFocus,
            .modality = ExplorerInputModality::Keyboard});
    if (name == "shift-tab" || name == "shift_tab" || name == "backtab")
        return parsed_input({
            .kind = ExplorerInputKind::ShiftTabFocus,
            .modality = ExplorerInputModality::Keyboard});
    if (name == "focus" || name == "focus-target" || name == "focus_target")
        return parse_focus(
            ExplorerInputKind::FocusTarget,
            ExplorerInputModality::Keyboard);
    if (name == "pointer" || name == "pointer-focus"
        || name == "pointer_focus" || name == "hover") {
        return parse_focus(
            ExplorerInputKind::PointerFocus,
            ExplorerInputModality::Pointer);
    }
    if (name == "move" || name == "move-selection"
        || name == "move_selection" || name == "navigate") {
        if (value.empty()) {
            return input_parse_error(
                "input '" + name + "' requires a selection move");
        }
        return parse_move(value);
    }
    if (name == "key" || name == "shortcut") {
        auto command = lower_copy(trim(value));
        if (command.empty()) {
            return input_parse_error(
                "input '" + name + "' requires a command name");
        }
        if (command == "tab") {
            return parsed_input({
                .kind = ExplorerInputKind::TabFocus,
                .modality = ExplorerInputModality::Keyboard});
        }
        if (command == "shift-tab" || command == "shift_tab"
            || command == "backtab") {
            return parsed_input({
                .kind = ExplorerInputKind::ShiftTabFocus,
                .modality = ExplorerInputModality::Keyboard});
        }
        if (command == "enter" || command == "return"
            || command == "open" || command == "activate") {
            return parsed_input({
                .kind = ExplorerInputKind::ActivateSelected,
                .modality = ExplorerInputModality::Keyboard});
        }
        if (command == "delete" || command == "backspace"
            || command == "trash") {
            return parsed_input({
                .kind = ExplorerInputKind::DeleteSelected,
                .modality = ExplorerInputModality::Keyboard});
        }
        if (command == "find" || command == "search") {
            return parsed_input({
                .kind = ExplorerInputKind::FocusSearch,
                .modality = ExplorerInputModality::Keyboard});
        }
        if (auto move = selection_move_from_name(command)) {
            return parsed_input(ExplorerInput{
                .kind = ExplorerInputKind::MoveSelection,
                .value = selection_move_value_name(*move),
                .modality = ExplorerInputModality::Keyboard,
                .selection_move = *move,
            });
        }
        if (command == "escape" || command == "dismiss") {
            return parsed_input({
                .kind = ExplorerInputKind::DismissTransient,
                .modality = ExplorerInputModality::Keyboard});
        }
        if (command == "duplicate" || command == "copy") {
            return parsed_input({
                .kind = ExplorerInputKind::DuplicateSelected,
                .modality = ExplorerInputModality::Keyboard});
        }
        if (command == "new-folder" || command == "new_folder"
            || command == "mkdir") {
            return parsed_input({
                .kind = ExplorerInputKind::CreateFolder,
                .modality = ExplorerInputModality::Keyboard});
        }
        if (command == "new-file" || command == "new_file"
            || command == "touch") {
            return parsed_input({
                .kind = ExplorerInputKind::CreateFile,
                .modality = ExplorerInputModality::Keyboard});
        }
        return input_parse_error(
            "unknown file explorer " + name + " command: " + command);
    }
    if (name == "search")
        return require_value(ExplorerInputKind::Search);
    if (name == "view" || name == "view-mode" || name == "view_mode"
        || name == "mode") {
        if (value.empty())
            return input_parse_error(
                "input 'view' requires icon, list, column, or gallery");
        if (!known_view_mode_name(value)) {
            return input_parse_error(
                "input 'view' accepts only icon, list, column, or gallery");
        }
        auto mode = view_mode_from_name(value);
        return parsed_input({
            .kind = ExplorerInputKind::ViewMode,
            .value = view_mode_value_name(mode),
            .view_mode = mode,
        });
    }
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
            return input_parse_error(
                "input 'sort' requires recent, name, kind, or size");
        auto lowered = lower_copy(value);
        if (lowered != "recent" && lowered != "recents"
            && lowered != "name" && lowered != "kind" && lowered != "size") {
            return input_parse_error(
                "input 'sort' accepts only recent, name, kind, or size");
        }
        return parsed_input({
            .kind = ExplorerInputKind::Sort,
            .value = value,
            .sort_mode = sort_mode_from_name(value),
        });
    }
    if (name == "cycle-sort")
        return parsed_input({.kind = ExplorerInputKind::CycleSort});
    if (name == "font" || name == "font-family" || name == "font_family") {
        if (value.empty())
            return input_parse_error(
                "input 'font-family' requires system, pretendard, or a family");
        return parsed_input({
            .kind = ExplorerInputKind::SetFontFamily,
            .value = value,
        });
    }
    if (name == "system-font-metrics"
        || name == "system_font_metrics"
        || name == "font-metrics"
        || name == "font_metrics"
        || name == "use-system-font-size"
        || name == "use_system_font_size") {
        auto lowered = lower_copy(trim(value.empty()
            ? std::string_view{"true"}
            : std::string_view{value}));
        if (lowered != "true" && lowered != "1" && lowered != "yes"
            && lowered != "on" && lowered != "system"
            && lowered != "false" && lowered != "0" && lowered != "no"
            && lowered != "off" && lowered != "package") {
            return input_parse_error(
                "input 'system-font-metrics' requires true, false, system, or package");
        }
        return parsed_input({
            .kind = ExplorerInputKind::SetSystemFontMetrics,
            .value = lowered,
        });
    }
    if (name == "system-scroll-metrics"
        || name == "system_scroll_metrics"
        || name == "scroll-metrics"
        || name == "scroll_metrics"
        || name == "use-system-scroll"
        || name == "use_system_scroll") {
        auto lowered = lower_copy(trim(value.empty()
            ? std::string_view{"true"}
            : std::string_view{value}));
        if (lowered != "true" && lowered != "1" && lowered != "yes"
            && lowered != "on" && lowered != "system"
            && lowered != "false" && lowered != "0" && lowered != "no"
            && lowered != "off" && lowered != "app") {
            return input_parse_error(
                "input 'system-scroll-metrics' requires true, false, system, or app");
        }
        return parsed_input({
            .kind = ExplorerInputKind::SetSystemScrollMetrics,
            .value = lowered,
        });
    }
    if (name == "font-scale" || name == "font_scale"
        || name == "text-scale" || name == "text_size") {
        auto parsed = parse_preference_number(value, 0.75f, 1.8f);
        if (!parsed) {
            return input_parse_error(
                "input 'font-scale' requires a positive number");
        }
        return parsed_input({
            .kind = ExplorerInputKind::SetFontScale,
            .value = compact_preference_number(*parsed),
        });
    }
    if (name == "font-size" || name == "font_size"
        || name == "body-font-size" || name == "body_font_size") {
        auto parsed = parse_preference_number(value, 8.0f, 40.0f);
        if (!parsed) {
            return input_parse_error(
                "input 'font-size' requires a positive point size");
        }
        return parsed_input({
            .kind = ExplorerInputKind::SetBodyFontSize,
            .value = compact_preference_number(*parsed),
        });
    }
    if (name == "heading-font-size" || name == "heading_font_size") {
        auto parsed = parse_preference_number(value, 10.0f, 56.0f);
        if (!parsed) {
            return input_parse_error(
                "input 'heading-font-size' requires a positive point size");
        }
        return parsed_input({
            .kind = ExplorerInputKind::SetHeadingFontSize,
            .value = compact_preference_number(*parsed),
        });
    }
    if (name == "small-font-size" || name == "small_font_size"
        || name == "caption-font-size" || name == "caption_font_size") {
        auto parsed = parse_preference_number(value, 8.0f, 32.0f);
        if (!parsed) {
            return input_parse_error(
                "input 'small-font-size' requires a positive point size");
        }
        return parsed_input({
            .kind = ExplorerInputKind::SetSmallFontSize,
            .value = compact_preference_number(*parsed),
        });
    }
    if (name == "line-height" || name == "line_height"
        || name == "line-height-ratio" || name == "line_height_ratio") {
        auto parsed = parse_preference_number(value, 1.0f, 2.2f);
        if (!parsed) {
            return input_parse_error(
                "input 'line-height' requires a positive ratio");
        }
        return parsed_input({
            .kind = ExplorerInputKind::SetLineHeightRatio,
            .value = compact_preference_number(*parsed),
        });
    }
    if (name == "scroll-speed" || name == "scroll_speed"
        || name == "scroll-scale" || name == "scroll_scale") {
        auto parsed = parse_preference_number(value, 0.25f, 4.0f);
        if (!parsed) {
            return input_parse_error(
                "input 'scroll-speed' requires a positive number");
        }
        return parsed_input({
            .kind = ExplorerInputKind::SetScrollSpeed,
            .value = compact_preference_number(*parsed),
        });
    }
    if (name == "horizontal-scroll-speed"
        || name == "horizontal_scroll_speed"
        || name == "scroll-x-speed"
        || name == "scroll_x_speed"
        || name == "scroll-x-scale"
        || name == "scroll_x_scale") {
        auto parsed = parse_preference_number(value, 0.25f, 4.0f);
        if (!parsed) {
            return input_parse_error(
                "input 'horizontal-scroll-speed' requires a positive number");
        }
        return parsed_input({
            .kind = ExplorerInputKind::SetHorizontalScrollSpeed,
            .value = compact_preference_number(*parsed),
        });
    }
    if (name == "scroll-bar-visibility"
        || name == "scroll_bar_visibility"
        || name == "scrollbar-visibility"
        || name == "scrollbar") {
        auto const raw_mode = lower_copy(trim(value));
        bool const known =
            raw_mode == "auto" || raw_mode == "hidden"
            || raw_mode == "none" || raw_mode == "off"
            || raw_mode == "false" || raw_mode == "0"
            || raw_mode == "always" || raw_mode == "visible"
            || raw_mode == "on" || raw_mode == "true"
            || raw_mode == "1";
        if (!known) {
            return input_parse_error(
                "input 'scroll-bar-visibility' requires auto, always, or hidden");
        }
        auto const mode = normalized_scroll_bar_visibility(value);
        return parsed_input({
            .kind = ExplorerInputKind::SetScrollBarVisibility,
            .value = mode,
        });
    }
    if (name == "motion-scale" || name == "motion_scale"
        || name == "animation-scale" || name == "animation_scale") {
        auto parsed = parse_motion_preference_number(value);
        if (!parsed) {
            return input_parse_error(
                "input 'motion-scale' requires a number from 0 to 4");
        }
        return parsed_input({
            .kind = ExplorerInputKind::SetMotionScale,
            .value = compact_preference_number(*parsed),
        });
    }
    if (name == "appearance" || name == "color-scheme"
        || name == "color_scheme") {
        auto lowered = lower_copy(trim(value));
        if (lowered != "system" && lowered != "light"
            && lowered != "dark" && lowered != "high-contrast-light"
            && lowered != "high-contrast-dark") {
            return input_parse_error(
                "input 'color-scheme' requires system, light, dark, "
                "high-contrast-light, or high-contrast-dark");
        }
        return parsed_input({
            .kind = ExplorerInputKind::SetColorScheme,
            .value = lowered,
        });
    }
    if (name == "reset")
        return parsed_input({.kind = ExplorerInputKind::Reset});
    if (name == "scenario")
        return require_value(ExplorerInputKind::Scenario);

    return input_parse_error("unknown file explorer input: " + name);
}

inline ExplorerInputSequenceParseResult parse_explorer_input_lines(
        std::string_view text,
        std::string_view source_label = "file explorer input script") {
    auto result = ExplorerInputSequenceParseResult{};
    auto lines = std::istringstream{std::string{text}};
    auto line = std::string{};
    auto line_number = std::size_t{0};
    while (std::getline(lines, line)) {
        ++line_number;
        auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.starts_with("#"))
            continue;
        if (trimmed.starts_with("input "))
            trimmed = trim(std::string_view{trimmed}.substr(6));
        auto parsed = parse_explorer_input(trimmed);
        if (!parsed.ok) {
            result.ok = false;
            result.line = line_number;
            result.error = std::string{source_label}
                + ":" + std::to_string(line_number)
                + ": " + parsed.error;
            return result;
        }
        result.inputs.push_back(std::move(parsed.input));
    }
    result.ok = true;
    return result;
}

inline ExplorerInputSequenceParseResult parse_explorer_input_sequence(
        std::string_view text,
        std::string_view source_label = "file explorer input sequence") {
    auto normalized = std::string{text};
    std::replace(normalized.begin(), normalized.end(), ';', '\n');
    return parse_explorer_input_lines(normalized, source_label);
}

} // namespace file_explorer_demo

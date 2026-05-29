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

export module file_explorer_shared:drive_and_expectations;

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
import :input_parsing;
import :filesystem_snapshot_helpers;
import :state_factories;
import :navigation_and_file_actions;
import :focus_and_scripted_inputs;

export namespace file_explorer_demo {
inline void apply_explorer_inputs(
        ExplorerState& state,
        std::span<ExplorerInput const> inputs,
        std::string_view profile) {
    for (auto const& input : inputs)
        apply_explorer_input(state, input, profile);
}

inline ExplorerInputTrace explorer_input_trace(
        ExplorerState const& state,
        ExplorerInput const& input,
        std::string_view profile) {
    auto snap = snapshot(state);
    return ExplorerInputTrace{
        .input = input,
        .chrome = explorer_chrome_metrics(state, profile),
        .status = state.status,
        .relative_location = snap.relative_location,
        .selected_name = state.selected_name,
        .operation_label = snap.operation_label,
        .operation = state.last_operation,
        .visible_entries = snap.entries.size(),
        .has_selection = snap.has_selection,
        .last_input_modality = state.last_input_modality,
        .focus_target = state.focus_target,
        .focus_visible = state.focus_visible,
    };
}

inline ExplorerDriveResult drive_explorer(
        std::string_view profile,
        std::span<ExplorerInput const> inputs) {
    ExplorerDriveResult result;
    result.profile = profile.empty() ? "desktop" : std::string{profile};
    result.state = make_state(result.profile);
    result.trace.reserve(inputs.size());
    for (auto const& input : inputs) {
        apply_explorer_input(result.state, input, result.profile);
        result.trace.push_back(explorer_input_trace(
            result.state,
            input,
            result.profile));
    }
    result.snapshot = snapshot(result.state);
    result.chrome = explorer_chrome_metrics(result.state, result.profile);
    return result;
}

inline bool snapshot_has_entry(Snapshot const& snapshot, std::string_view name) {
    return std::ranges::any_of(snapshot.entries, [&](Entry const& entry) {
        return entry.name == name;
    });
}

inline ExplorerExpectationResult check_explorer_expectation(
        ExplorerDriveResult const& result,
        ExplorerExpectation const& expectation) {
    auto const& snap = result.snapshot;
    auto checked = ExplorerExpectationResult{
        .expectation = expectation,
    };
    switch (expectation.kind) {
        case ExplorerExpectationKind::Selected:
            checked.actual = snap.has_selection ? snap.selected.name : "<none>";
            checked.ok = snap.has_selection && snap.selected.name == expectation.value;
            checked.detail = checked.ok
                ? "selected entry matched"
                : "selected entry did not match";
            return checked;
        case ExplorerExpectationKind::Location:
            checked.actual = snap.relative_location;
            checked.ok = snap.relative_location == expectation.value;
            checked.detail = checked.ok
                ? "location matched"
                : "relative location did not match";
            return checked;
        case ExplorerExpectationKind::Entry:
            checked.actual = snapshot_has_entry(snap, expectation.value)
                ? expectation.value
                : "<missing>";
            checked.ok = checked.actual == expectation.value;
            checked.detail = checked.ok
                ? "entry was visible"
                : "entry was not visible in the final snapshot";
            return checked;
        case ExplorerExpectationKind::MissingEntry:
            checked.actual = snapshot_has_entry(snap, expectation.value)
                ? expectation.value
                : "<missing>";
            checked.ok = checked.actual == "<missing>";
            checked.detail = checked.ok
                ? "entry was absent"
                : "entry was unexpectedly visible in the final snapshot";
            return checked;
        case ExplorerExpectationKind::Operation:
            checked.actual = result.state.last_operation.kind.empty()
                ? "<none>"
                : result.state.last_operation.kind
                    + (result.state.last_operation.ok ? ":ok" : ":fail");
            checked.ok = result.state.last_operation.kind == expectation.value;
            if (expectation.expects_operation_ok) {
                checked.ok = checked.ok
                    && result.state.last_operation.ok == expectation.operation_ok;
            }
            checked.detail = checked.ok
                ? "last operation matched"
                : "last operation did not match";
            return checked;
        case ExplorerExpectationKind::StatusContains:
            checked.actual = result.state.status;
            checked.ok = result.state.status.find(expectation.value)
                != std::string::npos;
            checked.detail = checked.ok
                ? "status contained expected text"
                : "status did not contain expected text";
            return checked;
        case ExplorerExpectationKind::FocusTarget:
            checked.actual =
                focus_target_value_name(result.state.focus_target);
            checked.ok = checked.actual == expectation.value;
            checked.detail = checked.ok
                ? "focus target matched"
                : "focus target did not match";
            return checked;
        case ExplorerExpectationKind::FocusVisible:
            checked.actual = result.state.focus_visible ? "true" : "false";
            checked.ok = checked.actual == expectation.value;
            checked.detail = checked.ok
                ? "focus visibility matched"
                : "focus visibility did not match";
            return checked;
        case ExplorerExpectationKind::InputModality:
            checked.actual =
                input_modality_value_name(result.state.last_input_modality);
            checked.ok = checked.actual == expectation.value;
            checked.detail = checked.ok
                ? "input modality matched"
                : "input modality did not match";
            return checked;
    }
    checked.actual = "<unknown>";
    checked.detail = "unknown expectation";
    return checked;
}

inline std::vector<ExplorerExpectationResult> check_explorer_expectations(
        ExplorerDriveResult const& result,
        std::span<ExplorerExpectation const> expectations) {
    auto checked = std::vector<ExplorerExpectationResult>{};
    checked.reserve(expectations.size());
    for (auto const& expectation : expectations)
        checked.push_back(check_explorer_expectation(result, expectation));
    return checked;
}

inline bool explorer_expectations_ok(
        std::span<ExplorerExpectationResult const> expectations) {
    return std::ranges::all_of(expectations, [](auto const& expectation) {
        return expectation.ok;
    });
}

inline std::string entry_label(Entry const& entry) {
    std::string label = entry.folder ? "[Folder] " : "[File] ";
    label += entry.name;
    return label;
}

} // namespace file_explorer_demo

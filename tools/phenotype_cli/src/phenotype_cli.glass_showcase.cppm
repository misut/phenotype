export module phenotype_cli.glass_showcase;

import cppx.cli;
import cppx.terminal;
import glass_showcase_shared;
import phenotype_cli.common;
import std;

namespace phenotype_cli::glass_showcase {

namespace fs = std::filesystem;
using phenotype_cli::common::json_string;
using phenotype_cli::common::path_string;
using phenotype_cli::common::print_error;
using phenotype_cli::common::read_text_file;
using phenotype_cli::common::string_array_json;

auto trim_copy(std::string_view text) -> std::string {
    auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return {};
    auto last = text.find_last_not_of(" \t\r\n");
    return std::string{text.substr(first, last - first + 1)};
}

export auto glass_state_json(glass_showcase_demo::State const& state)
    -> std::string {
    return std::format(
        "{{\"backdrop\":{},\"high_contrast_backdrop\":{},"
        "\"inspector\":{},\"inspector_open\":{},"
        "\"density\":{},\"density_label\":{},\"density_index\":{},"
        "\"note\":{},\"viewport\":{{\"w\":{},\"h\":{},\"scale\":{}}},"
        "\"progress\":{},\"expected_material_plan_count\":{}}}",
        json_string(glass_showcase_demo::backdrop_value_name(
            state.high_contrast_backdrop)),
        state.high_contrast_backdrop ? "true" : "false",
        json_string(glass_showcase_demo::inspector_value_name(
            state.inspector_open)),
        state.inspector_open ? "true" : "false",
        json_string(glass_showcase_demo::density_value_name(
            state.selected_density)),
        json_string(glass_showcase_demo::density_label(state.selected_density)),
        state.selected_density,
        json_string(state.note),
        state.viewport_width,
        state.viewport_height,
        state.viewport_scale,
        glass_showcase_demo::progress_value(state),
        glass_showcase_demo::expected_material_plan_count(state));
}

export auto glass_input_json(glass_showcase_demo::GlassInput const& input)
    -> std::string {
    return std::format(
        "{{\"kind\":{},\"label\":{},\"value\":{},\"density\":{},"
        "\"flag\":{},\"viewport\":{{\"w\":{},\"h\":{},\"scale\":{}}}}}",
        json_string(glass_showcase_demo::glass_input_kind_name(input.kind)),
        json_string(glass_showcase_demo::glass_input_label(input)),
        json_string(input.value),
        input.density,
        input.flag ? "true" : "false",
        input.viewport_width,
        input.viewport_height,
        input.viewport_scale);
}

export auto glass_trace_json(glass_showcase_demo::GlassInputTrace const& trace,
                             std::size_t index) -> std::string {
    return std::format(
        "{{\"index\":{},\"input\":{},\"state\":{},"
        "\"expected_material_plan_count\":{},\"density_label\":{},"
        "\"backdrop\":{},\"inspector\":{}}}",
        index,
        glass_input_json(trace.input),
        glass_state_json(trace.state),
        trace.expected_material_plan_count,
        json_string(trace.density_label),
        json_string(trace.backdrop_label),
        json_string(trace.inspector_label));
}

export auto glass_trace_array_json(
        std::span<glass_showcase_demo::GlassInputTrace const> trace)
    -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < trace.size(); ++i) {
        if (i > 0)
            out += ",";
        out += glass_trace_json(trace[i], i);
    }
    out += "]";
    return out;
}

export auto glass_expectation_json(
        glass_showcase_demo::GlassExpectationResult const& expectation)
    -> std::string {
    return std::format(
        "{{\"kind\":{},\"value\":{},\"label\":{},\"ok\":{},"
        "\"actual\":{},\"detail\":{}}}",
        json_string(glass_showcase_demo::glass_expectation_kind_name(
            expectation.expectation.kind)),
        json_string(expectation.expectation.value),
        json_string(glass_showcase_demo::glass_expectation_label(
            expectation.expectation)),
        expectation.ok ? "true" : "false",
        json_string(expectation.actual),
        json_string(expectation.detail));
}

export auto glass_expectations_json(
        std::span<glass_showcase_demo::GlassExpectationResult const>
            expectations) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < expectations.size(); ++i) {
        if (i > 0)
            out += ",";
        out += glass_expectation_json(expectations[i]);
    }
    out += "]";
    return out;
}

export auto glass_material_contract_json(
        glass_showcase_demo::State const& state)
    -> std::string {
    auto kinds = glass_showcase_demo::public_material_kinds();
    return std::format(
        "{{\"public_material_kinds\":{},\"public_material_kind_count\":{},"
        "\"base_material_plan_count\":{},\"inspector_surface_present\":{},"
        "\"overlay_surface_present\":true,\"expected_material_plan_count\":{},"
        "\"backdrop_sampling_contract\":\"deterministic backdrop probe canvas\","
        "\"fallback_contract\":\"translucent rounded rect available\","
        "\"state\":{}}}",
        string_array_json(kinds),
        glass_showcase_demo::k_material_kind_count,
        glass_showcase_demo::k_base_material_plan_count,
        state.inspector_open ? "true" : "false",
        glass_showcase_demo::expected_material_plan_count(state),
        glass_state_json(state));
}

export auto glass_drive_json(
        glass_showcase_demo::GlassDriveResult const& result,
        std::span<glass_showcase_demo::GlassExpectationResult const>
            expectations)
    -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"drive glass-showcase\","
        "\"ok\":{},\"input_count\":{},\"state\":{},"
        "\"material_contract\":{},\"trace\":{},\"expectations\":{}}}",
        glass_showcase_demo::glass_expectations_ok(expectations)
            ? "true"
            : "false",
        result.trace.size(),
        glass_state_json(result.state),
        glass_material_contract_json(result.state),
        glass_trace_array_json(result.trace),
        glass_expectations_json(expectations));
}

export auto parse_glass_input_script(fs::path const& path)
    -> std::expected<std::vector<glass_showcase_demo::GlassInput>, std::string> {
    auto ec = std::error_code{};
    if (!fs::exists(path, ec)) {
        return std::unexpected{
            std::format("input script does not exist: {}", path_string(path))};
    }
    auto text = read_text_file(path);
    auto inputs = std::vector<glass_showcase_demo::GlassInput>{};
    auto lines = std::istringstream{text};
    auto line = std::string{};
    auto line_number = std::size_t{0};
    while (std::getline(lines, line)) {
        ++line_number;
        auto trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed.starts_with("#"))
            continue;
        if (trimmed.starts_with("input "))
            trimmed = trim_copy(std::string_view{trimmed}.substr(6));
        auto parsed = glass_showcase_demo::parse_glass_input(trimmed);
        if (!parsed.ok) {
            return std::unexpected{std::format(
                "{}:{}: {}",
                path_string(path),
                line_number,
                parsed.error)};
        }
        inputs.push_back(std::move(parsed.input));
    }
    return inputs;
}

export auto parse_glass_expectations(cppx::cli::Invocation const& invocation)
    -> std::expected<std::vector<glass_showcase_demo::GlassExpectation>,
                     std::string> {
    auto expectations = std::vector<glass_showcase_demo::GlassExpectation>{};
    for (auto const& raw : invocation.values("expect")) {
        auto parsed = glass_showcase_demo::parse_glass_expectation(raw);
        if (!parsed.ok)
            return std::unexpected{parsed.error};
        expectations.push_back(std::move(parsed.expectation));
    }
    return expectations;
}

export int run_drive_glass_showcase(cppx::cli::Invocation const& invocation) {
    auto inputs = std::vector<glass_showcase_demo::GlassInput>{};
    for (auto const& script : invocation.values("script")) {
        auto parsed = parse_glass_input_script(fs::path{script});
        if (!parsed) {
            return print_error(
                "drive glass-showcase",
                parsed.error(),
                invocation.has("json"));
        }
        inputs.insert(inputs.end(),
                      std::make_move_iterator(parsed->begin()),
                      std::make_move_iterator(parsed->end()));
    }
    for (auto const& raw : invocation.values("input")) {
        auto parsed = glass_showcase_demo::parse_glass_input(raw);
        if (!parsed.ok) {
            return print_error(
                "drive glass-showcase",
                parsed.error,
                invocation.has("json"));
        }
        inputs.push_back(std::move(parsed.input));
    }

    auto expectations = parse_glass_expectations(invocation);
    if (!expectations) {
        return print_error(
            "drive glass-showcase",
            expectations.error(),
            invocation.has("json"));
    }

    auto result = glass_showcase_demo::drive_glass_showcase(inputs);
    auto checked_expectations =
        glass_showcase_demo::check_glass_expectations(
            result,
            *expectations);
    if (invocation.has("json")) {
        std::println("{}",
                     glass_drive_json(result, checked_expectations));
    } else {
        auto const& state = result.state;
        auto lines = std::vector<cppx::terminal::StatusLine>{
            {.label = "backdrop",
             .value = glass_showcase_demo::backdrop_value_name(
                 state.high_contrast_backdrop),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "inspector",
             .value = glass_showcase_demo::inspector_value_name(
                 state.inspector_open),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "density",
             .value = glass_showcase_demo::density_label(
                 state.selected_density),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "viewport",
             .value = glass_showcase_demo::viewport_value_label(
                 state.viewport_width,
                 state.viewport_height,
                 state.viewport_scale),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "material plans",
             .value = std::format(
                 "{} expected",
                 glass_showcase_demo::expected_material_plan_count(state)),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "note",
             .value = state.note,
             .status = cppx::terminal::StatusKind::ok},
        };
        std::println("phenotype drive glass-showcase");
        std::println("{}", cppx::terminal::format_status_frame(lines, false));
        if (!checked_expectations.empty()) {
            std::println("expectations:");
            for (auto const& expectation : checked_expectations) {
                std::println("  - {} -> {} ({})",
                             glass_showcase_demo::glass_expectation_label(
                                 expectation.expectation),
                             expectation.ok ? "ok" : "failed",
                             expectation.actual);
            }
        }
        if (!result.trace.empty()) {
            std::println("inputs:");
            for (auto const& trace : result.trace) {
                std::println("  - {} -> {} / {} / {} plans",
                             glass_showcase_demo::glass_input_label(
                                 trace.input),
                             trace.backdrop_label,
                             trace.density_label,
                             trace.expected_material_plan_count);
            }
        }
    }

    return glass_showcase_demo::glass_expectations_ok(checked_expectations)
        ? 0
        : 1;
}

}

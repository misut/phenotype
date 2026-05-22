export module phenotype_cli.glass_showcase;

import cppx.cli;
import cppx.process;
import cppx.process.system;
import cppx.terminal;
import glass_showcase_shared;
import json;
import phenotype_cli.common;
import phenotype_cli.material_budget;
import phenotype_cli.runtime;
import std;

namespace phenotype_cli::glass_showcase {

namespace fs = std::filesystem;
using phenotype_cli::common::json_string;
using phenotype_cli::common::path_string;
using phenotype_cli::common::print_error;
using phenotype_cli::common::read_text_file;
using phenotype_cli::common::string_array_json;
using phenotype_cli::common::absolute_path_string;
using namespace phenotype_cli::material_budget;
using namespace phenotype_cli::runtime;

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

export auto glass_probe_contract_json(
        glass_showcase_demo::State const& state) -> std::string {
    return json::emit(
        glass_showcase_demo::glass_probe_contract_debug_json(state));
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
        "\"probe_contract\":{},\"state\":{}}}",
        string_array_json(kinds),
        glass_showcase_demo::k_material_kind_count,
        glass_showcase_demo::k_base_material_plan_count,
        state.inspector_open ? "true" : "false",
        glass_showcase_demo::expected_material_plan_count(state),
        glass_probe_contract_json(state),
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

struct GlassArtifactGateSummary {
    bool accessibility = false;
    fs::path example_root;
    std::string package_name;
    fs::path executable;
    fs::path artifact_dir;
    fs::path manifest;
    std::string expect_platform;
    std::string accessibility_display;
    std::optional<std::chrono::milliseconds> run_timeout;
    std::optional<cppx::process::CapturedProcessResult> build_result;
    std::optional<cppx::process::CapturedProcessResult> run_result;
    std::optional<cppx::process::CapturedProcessResult> verifier_result;
    std::optional<ArtifactSummary> artifact;
    bool ok = false;
    std::string error;
};

auto timeout_seconds_json(
        std::optional<std::chrono::milliseconds> timeout) -> std::string {
    if (!timeout)
        return "null";
    return std::format(
        "{}",
        std::chrono::duration_cast<std::chrono::seconds>(*timeout).count());
}

auto glass_gate_json(GlassArtifactGateSummary const& summary) -> std::string {
    auto artifact = summary.artifact
        ? artifact_detail_json(*summary.artifact)
        : std::string{"null"};
    auto verifier_report = verifier_report_from_result(summary.verifier_result);
    auto budget = verifier_report
        ? material_budget_from_report(*verifier_report)
        : std::optional<MaterialBudgetSummary>{};
    auto verifier_manifest = verifier_report
        ? verifier_manifest_summary_from_report(*verifier_report)
        : std::optional<VerifierManifestSummary>{};
    auto coverage = material_budget_coverage_summary(budget, verifier_manifest);
    auto bound_summary = verifier_report
        ? material_budget_bound_summary_from_report(*verifier_report)
        : std::optional<MaterialBudgetBoundSummary>{};
    auto bound_results = verifier_report
        ? material_budget_bound_results_from_report(*verifier_report)
        : std::optional<std::vector<MaterialBudgetBoundResult>>{};
    return std::format(
        "{{\"schema_version\":1,\"command\":\"artifact verify-glass-showcase\","
        "\"ok\":{},\"accessibility\":{},\"example_root\":{},"
        "\"package_name\":{},\"executable\":{},\"artifact_dir\":{},"
        "\"manifest\":{},\"expect_platform\":{},"
        "\"accessibility_display\":{},\"timeout_seconds\":{},"
        "\"build\":{},\"run_result\":{},\"verifier\":{},"
        "\"artifact\":{},\"material_budget\":{},"
        "\"verifier_manifest\":{},\"material_budget_coverage\":{},"
        "\"material_budget_bound_summary\":{},"
        "\"material_budget_bound_results\":{},\"error\":{}}}",
        summary.ok ? "true" : "false",
        summary.accessibility ? "true" : "false",
        json_string(path_string(summary.example_root)),
        json_string(summary.package_name),
        json_string(path_string(summary.executable)),
        json_string(path_string(summary.artifact_dir)),
        json_string(path_string(summary.manifest)),
        json_string(summary.expect_platform),
        json_string(summary.accessibility_display),
        timeout_seconds_json(summary.run_timeout),
        process_result_detail_json(summary.build_result),
        process_result_detail_json(summary.run_result),
        process_result_detail_json(summary.verifier_result),
        artifact,
        material_budget_json(budget),
        verifier_manifest_summary_json(verifier_manifest),
        material_budget_coverage_json(coverage),
        material_budget_bound_summary_json(bound_summary),
        material_budget_bound_results_json(bound_results),
        json_string(summary.error));
}

void print_glass_gate(GlassArtifactGateSummary const& summary) {
    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "mode",
         .value = summary.accessibility ? "accessibility" : "standard",
         .status = cppx::terminal::StatusKind::ok},
        {.label = "example",
         .value = path_string(summary.example_root),
         .status = path_is_directory(summary.example_root)
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "artifact",
         .value = summary.artifact
            ? std::format("{} snapshot={} frame={}",
                          path_string(summary.artifact->bundle),
                          summary.artifact->snapshot_json ? "true" : "false",
                          summary.artifact->frame_bmp ? "true" : "false")
            : path_string(summary.artifact_dir),
         .status = summary.artifact && summary.artifact->snapshot_json
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "manifest",
         .value = path_string(summary.manifest),
         .status = path_exists(summary.manifest)
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "build",
         .value = result_value(summary.build_result),
         .status = result_status(summary.build_result)},
        {.label = "run",
         .value = result_value(summary.run_result),
         .status = result_status(summary.run_result)},
        {.label = "verifier",
         .value = result_value(summary.verifier_result),
         .status = result_status(summary.verifier_result)},
    };
    std::println("phenotype artifact verify-glass-showcase");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    if (auto verifier_report =
            verifier_report_from_result(summary.verifier_result)) {
        auto budget = material_budget_from_report(*verifier_report);
        auto manifest = verifier_manifest_summary_from_report(*verifier_report);
        if (manifest) {
            std::println(
                "verifier manifest: name={} runtime-bounds={} "
                "budget-bounds={} pixel-regions={} metrics={} "
                "comparisons={} forbidden-colors={}",
                manifest->name,
                budget_count(manifest->runtime_numeric_bounds),
                budget_count(manifest->material_executor_budget_bounds),
                budget_count(manifest->pixel_regions),
                budget_count(manifest->pixel_region_metrics),
                budget_count(manifest->pixel_region_metric_comparisons),
                budget_count(manifest->forbid_pixel_region_colors));
        }
        if (auto coverage =
                material_budget_coverage_summary(budget, manifest)) {
            std::println(
                "material budget coverage: {}",
                material_budget_coverage_text(*coverage));
        }
        if (auto bound_summary =
                material_budget_bound_summary_from_report(*verifier_report)) {
            std::println(
                "material budget bounds: {}",
                material_budget_bound_summary_text(*bound_summary));
        }
    }
    if (auto budget = material_budget_from_verifier(summary.verifier_result)) {
        std::println("material budget:");
        std::println(
            "  plans: {} sampled={} fallback={} stages={} active={} "
            "backdrop-stages={} dropped={}",
            budget_count(budget->plan_count),
            budget_count(budget->sampled_backdrop_instance_count),
            budget_count(budget->fallback_instance_count),
            budget_count(budget->execution_stage_count),
            budget_count(budget->active_execution_stage_count),
            budget_count(budget->backdrop_execution_stage_count),
            budget_count(budget->dropped_execution_stage_count));
        std::println(
            "  work: draw-calls={} taps={} max-taps={} upload={}/{} bytes "
            "upload-util={} copy={}/{} px copy-util={} "
            "frame-capture={}/{} px surface-sample={} px",
            budget_count(budget->draw_calls),
            budget_count(budget->total_sample_taps),
            budget_count(budget->max_sample_taps),
            budget_count(budget->upload_bytes),
            budget_count(budget->buffer_capacity_bytes),
            budget_utilization_text(budget->upload_utilization),
            budget_count(budget->backdrop_copy_pixels),
            budget_count(budget->max_backdrop_pixels),
            budget_utilization_text(budget->backdrop_copy_utilization),
            budget_count(budget->planned_frame_capture_count),
            budget_count(budget->planned_frame_capture_pixels),
            budget_count(budget->planned_surface_sample_pixels));
        std::println(
            "  status: upload={} draw={} uploaded={} drawn={} "
            "copy-required={} copy-policy={} skip={}",
            budget->upload_status,
            budget->draw_status,
            budget_bool_text(budget->uploaded),
            budget_bool_text(budget->drawn),
            budget_bool_text(budget->backdrop_copy_required),
            budget->backdrop_copy_policy,
            budget->backdrop_copy_skip_reason);
    }
    if (!summary.error.empty()) {
        std::println("{}",
                     cppx::terminal::format_diagnostic({
                         .severity = cppx::terminal::DiagnosticSeverity::error,
                         .message = summary.error,
                         .context = "artifact verify-glass-showcase",
                     }));
    }
    if (summary.verifier_result
        && (summary.verifier_result->timed_out
            || summary.verifier_result->exit_code != 0)
        && !summary.verifier_result->stdout_text.empty()) {
        std::println("verifier report tail:");
        std::println("{}", output_tail(summary.verifier_result->stdout_text));
    }
}

auto glass_verifier_args(fs::path const& bundle,
                         fs::path const& manifest,
                         std::string_view expect_platform)
    -> std::vector<std::string> {
    return {
        "exec",
        "--",
        "uv",
        "run",
        "--frozen",
        "python",
        "tools/verify_artifact_bundle.py",
        path_string(bundle),
        "--manifest",
        path_string(manifest),
        "--expect-platform",
        std::string{expect_platform},
    };
}

auto run_glass_verifier(fs::path const& root,
                        GlassArtifactGateSummary const& summary)
    -> std::expected<cppx::process::CapturedProcessResult,
                     cppx::process::process_error> {
    auto spec = cppx::process::ProcessSpec{
        .program = "mise",
        .args = glass_verifier_args(
            summary.artifact_dir,
            summary.manifest,
            summary.expect_platform),
        .cwd = root,
        .timeout = std::chrono::minutes{5},
        .env_overrides = {
            {"UV_PROJECT_ENVIRONMENT", uv_project_environment()},
        },
    };
    return cppx::process::system::capture(spec);
}

int emit_glass_gate(GlassArtifactGateSummary const& summary,
                    cppx::cli::Invocation const& invocation,
                    int exit_code) {
    if (invocation.has("json")) {
        std::println("{}", glass_gate_json(summary));
    } else {
        print_glass_gate(summary);
    }
    return exit_code;
}

export int run_artifact_verify_glass_showcase(
        cppx::cli::Invocation const& invocation) {
    auto root = find_repo_root(fs::current_path());
    if (!root) {
        return print_error(
            "artifact verify-glass-showcase",
            "could not find phenotype repository root from current directory",
            invocation.has("json"));
    }

    auto summary = GlassArtifactGateSummary{
        .accessibility = invocation.has("accessibility"),
        .example_root = *root / "examples" / "glass_showcase",
    };

    auto package_name = exon_package_name(summary.example_root);
    if (!package_name) {
        summary.error = package_name.error();
        return emit_glass_gate(summary, invocation, 2);
    }
    summary.package_name = *package_name;
    summary.executable =
        summary.example_root / ".exon" / "debug" / executable_filename(*package_name);

    if (auto value = invocation.value("bundle-dir")) {
        summary.artifact_dir =
            fs::path{absolute_path_string(fs::path{std::string{*value}})};
    } else if (auto value = environment_value("PHENOTYPE_ARTIFACT_DIR")) {
        summary.artifact_dir = fs::path{absolute_path_string(fs::path{*value})};
    } else {
        auto temp = make_temp_directory("phenotype-glass-showcase");
        if (!temp) {
            summary.error = temp.error();
            return emit_glass_gate(summary, invocation, 2);
        }
        summary.artifact_dir = *temp;
    }

    auto ec = std::error_code{};
    fs::create_directories(summary.artifact_dir, ec);
    if (ec) {
        summary.error = std::format(
            "could not create artifact directory {}: {}",
            path_string(summary.artifact_dir),
            ec.message());
        return emit_glass_gate(summary, invocation, 2);
    }

    if (auto value = invocation.value("manifest")) {
        summary.manifest =
            fs::path{absolute_path_string(fs::path{std::string{*value}})};
    } else if (auto value = environment_value("PHENOTYPE_ARTIFACT_MANIFEST")) {
        summary.manifest = repo_relative_path(*root, *value);
    } else {
        summary.manifest = *root / "examples" / "glass_showcase"
            / (summary.accessibility
                   ? "artifact_manifest.accessibility.json"
                   : "artifact_manifest.json");
    }

    if (auto value = invocation.value("expect-platform")) {
        summary.expect_platform = std::string{*value};
    } else if (auto value = environment_value("PHENOTYPE_EXPECT_PLATFORM")) {
        summary.expect_platform = *value;
    } else {
        summary.expect_platform = "macos";
    }

    if (auto value = invocation.value("accessibility-display")) {
        summary.accessibility_display = std::string{*value};
    } else if (auto value = environment_value("PHENOTYPE_ACCESSIBILITY_DISPLAY")) {
        summary.accessibility_display = *value;
    } else if (summary.accessibility) {
        summary.accessibility_display =
            "reduce-transparency,increase-contrast,reduce-motion";
    } else {
        summary.accessibility_display = "standard";
    }

    if (auto value = invocation.value("timeout-seconds")) {
        auto timeout = parse_seconds(*value, "--timeout-seconds");
        if (!timeout) {
            summary.error = timeout.error();
            return emit_glass_gate(summary, invocation, 2);
        }
        summary.run_timeout = *timeout;
    } else {
        summary.run_timeout = std::chrono::seconds{120};
    }

    auto build = run_example_build(summary.example_root);
    if (!build) {
        summary.error = std::format(
            "failed to run exon build: {}",
            cppx::process::to_string(build.error()));
        return emit_glass_gate(summary, invocation, 2);
    }
    summary.build_result = std::move(*build);
    if (summary.build_result->timed_out
        || summary.build_result->exit_code != 0) {
        summary.error = "exon build failed";
        auto code = summary.build_result->timed_out
            ? 124
            : summary.build_result->exit_code;
        return emit_glass_gate(summary, invocation, code);
    }

    auto env = std::map<std::string, std::string>{
        {"PHENOTYPE_ARTIFACT_DIR", path_string(summary.artifact_dir)},
        {"PHENOTYPE_ARTIFACT_EXIT", "1"},
        {"PHENOTYPE_ACCESSIBILITY_DISPLAY", summary.accessibility_display},
    };
    if (auto value = environment_value("PHENOTYPE_ARTIFACT_REASON")) {
        env["PHENOTYPE_ARTIFACT_REASON"] = *value;
    } else {
        env["PHENOTYPE_ARTIFACT_REASON"] = summary.accessibility
            ? "glass-showcase-accessibility-gate"
            : "glass-showcase-gate";
    }

    auto run = run_example_binary(
        summary.executable,
        summary.example_root,
        std::move(env),
        summary.run_timeout);
    if (!run) {
        summary.error = std::format(
            "failed to run glass showcase: {}",
            cppx::process::to_string(run.error()));
        return emit_glass_gate(summary, invocation, 2);
    }
    summary.run_result = std::move(*run);
    summary.artifact = artifact_summary(summary.artifact_dir);
    if (summary.run_result->timed_out
        || summary.run_result->exit_code != 0) {
        summary.error = summary.run_result->timed_out
            ? "glass showcase timed out"
            : std::format(
                "glass showcase exited with {}",
                summary.run_result->exit_code);
        auto code = summary.run_result->timed_out
            ? 124
            : summary.run_result->exit_code;
        return emit_glass_gate(summary, invocation, code);
    }

    auto verifier = run_glass_verifier(*root, summary);
    if (!verifier) {
        summary.error = std::format(
            "failed to run mise/uv verifier: {}",
            cppx::process::to_string(verifier.error()));
        return emit_glass_gate(summary, invocation, 2);
    }
    summary.verifier_result = std::move(*verifier);
    summary.ok = summary.artifact
        && summary.artifact->snapshot_json
        && !summary.verifier_result->timed_out
        && summary.verifier_result->exit_code == 0;
    if (!summary.ok && summary.error.empty()) {
        if (!summary.artifact || !summary.artifact->snapshot_json) {
            summary.error = "artifact bundle did not contain snapshot.json";
        } else if (summary.verifier_result->timed_out) {
            summary.error = "artifact verifier timed out";
        } else {
            summary.error = std::format(
                "artifact verifier exited with {}",
                summary.verifier_result->exit_code);
        }
    }

    if (summary.verifier_result->timed_out)
        return emit_glass_gate(summary, invocation, 124);
    return emit_glass_gate(
        summary,
        invocation,
        summary.ok ? 0 : summary.verifier_result->exit_code);
}



}

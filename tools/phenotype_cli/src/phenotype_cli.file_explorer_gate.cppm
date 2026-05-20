export module phenotype_cli.file_explorer_gate;

import cppx.cli;
import cppx.process;
import cppx.process.system;
import cppx.terminal;
import file_explorer_shared;
import phenotype_cli.common;
import phenotype_cli.file_explorer_gate_config;
import phenotype_cli.runtime;
import std;

namespace phenotype_cli::file_explorer_gate {

namespace fs = std::filesystem;
using namespace phenotype_cli::common;
using namespace phenotype_cli::runtime;

struct FileExplorerArtifactCase {
    std::string profile;
    std::string mode;
    std::string scenario;
    std::string artifact_reason;
    fs::path artifact_dir;
    std::optional<cppx::process::CapturedProcessResult> run_result;
    std::optional<cppx::process::CapturedProcessResult> verifier_result;
    std::optional<ArtifactSummary> artifact;
    bool ok = false;
    std::string error;
};

struct FileExplorerArtifactGateSummary {
    std::string profile = "all";
    std::vector<std::string> desktop_modes;
    std::vector<std::string> scenarios;
    fs::path shared_root;
    fs::path desktop_root;
    fs::path mobile_root;
    std::string desktop_package_name;
    std::string mobile_package_name;
    fs::path desktop_executable;
    fs::path mobile_executable;
    std::optional<cppx::process::CapturedProcessResult> shared_test_result;
    std::optional<cppx::process::CapturedProcessResult> desktop_build_result;
    std::optional<cppx::process::CapturedProcessResult> mobile_build_result;
    std::vector<FileExplorerArtifactCase> cases;
    bool ok = false;
    std::string error;
};

auto append_verifier_arg(std::vector<std::string>& args,
                         std::string option,
                         std::string value) {
    args.push_back(std::move(option));
    args.push_back(std::move(value));
}

auto append_required_label(std::vector<std::string>& args,
                           std::string value) {
    append_verifier_arg(args, "--require-label", std::move(value));
}

auto append_required_label_contains(std::vector<std::string>& args,
                                    std::string value) {
    append_verifier_arg(args, "--require-label-contains", std::move(value));
}

auto append_required_role(std::vector<std::string>& args,
                          std::string value) {
    append_verifier_arg(args, "--require-role", std::move(value));
}

auto append_required_material_surface_role(std::vector<std::string>& args,
                                           std::string value) {
    append_verifier_arg(args, "--require-material-surface-role", std::move(value));
}

auto append_required_debug_detail(std::vector<std::string>& args,
                                  std::string value) {
    append_verifier_arg(args, "--require-debug-detail", std::move(value));
}

auto append_required_runtime_detail(std::vector<std::string>& args,
                                    std::string value) {
    append_verifier_arg(args, "--require-runtime-detail", std::move(value));
}

auto append_file_explorer_scenario_requirements(
        std::vector<std::string>& args,
        std::string_view profile,
        std::string_view scenario) -> std::expected<void, std::string> {
    if (scenario.empty() || scenario == "default")
        return {};
    if (scenario == "created-preview") {
        append_required_label_contains(args, "Action Note.txt");
        append_required_label_contains(args, "Created Action Note.txt");
        append_required_label_contains(
            args, "Operation: file_create ok - Action Note.txt");
        append_required_label_contains(args, "Created from artifact scenario");
    } else if (scenario == "deleted-file") {
        append_required_label_contains(args, "Moved Delete Me.txt to Trash");
        append_required_label_contains(
            args, "Operation: file_delete ok - Delete Me.txt");
    } else if (scenario == "trash-view") {
        append_required_label_contains(args, "Trash");
        append_required_label_contains(args, "Trash Note.txt");
        append_required_label_contains(args, "Moved Trash Note.txt to Trash");
        append_required_label_contains(
            args, "Operation: file_delete ok - Trash Note.txt");
    } else if (scenario == "created-folder") {
        append_required_label_contains(args, "Review Folder");
        append_required_label_contains(args, "Created folder Review Folder");
        append_required_label_contains(
            args, "Operation: folder_create ok - Review Folder");
        append_required_label_contains(
            args, "Open this folder to view its files.");
    } else if (scenario == "deleted-folder") {
        append_required_label_contains(
            args, "Moved folder Trash Folder to Trash");
        append_required_label_contains(
            args, "Operation: folder_delete ok - Trash Folder");
    } else if (scenario == "duplicated-file") {
        append_required_label_contains(args, "README copy.txt");
        append_required_label_contains(
            args, "Duplicated README.txt to README copy.txt");
        append_required_label_contains(
            args, "Operation: file_duplicate ok - README copy.txt");
        append_required_label_contains(args, "Phenotype File Explorer");
    } else if (scenario == "documents-preview") {
        append_required_label_contains(args, "Project Notes.txt");
        append_required_label_contains(
            args, "Operation: file_read ok - Project Notes.txt");
        append_required_label_contains(args, "Finder-like desktop layout");
    } else if (scenario == "svg-selected") {
        append_required_label_contains(args, "Glass Symbol.svg");
        append_required_label_contains(
            args, "Operation: file_read ok - Glass Symbol.svg");
        append_required_debug_detail(
            args, "application.file_explorer.selection.present=true");
        append_required_debug_detail(
            args, "application.file_explorer.selection.name=\"Glass Symbol.svg\"");
        append_required_debug_detail(
            args, "application.file_explorer.chrome.thumbnail_system.svg_render_policy=\"phenotype_svg_subset_renders_file_body_inside_finder_preview\"");
        append_required_debug_detail(
            args, "application.file_explorer.chrome.thumbnail_system.svg_external_resource_policy=\"no_external_svg_resources_or_network_fetches\"");
        append_required_debug_detail(
            args, "application.file_explorer.chrome.thumbnail_system.svg_document_cache_policy=\"edge_svg_document_cache_keyed_by_file_preview_body_no_frame_parse_churn\"");
        append_required_debug_detail(
            args, "application.file_explorer.chrome.thumbnail_system.svg_document_cache_limit=32");
    } else if (scenario == "history-forward") {
        append_required_label_contains(args, "Project Notes.txt");
        append_required_label_contains(
            args, "Went forward to Demo Root/Documents");
    } else if (scenario == "sorted-kind") {
        append_required_label_contains(args, "Sort: Kind");
        append_required_debug_detail(
            args, "application.file_explorer.status=\"Sorted by Kind\"");
        append_required_debug_detail(
            args, "application.file_explorer.sort.value=\"kind\"");
    } else if (scenario == "search-active") {
        append_required_role(args, "text_field");
        append_required_label(
            args,
            profile == "mobile" ? "Mobile Search Field" : "Search Field");
        append_required_label_contains(args, "Searching for Screen");
        append_required_label_contains(args, "Screen Recording");
        append_required_label_contains(args, "Screenshot");
    } else if (scenario == "more-actions-open") {
        append_required_label(args, "More Actions Menu");
        append_required_material_surface_role(args, "overlay");
        append_required_label(args, "New File");
        append_required_label(args, "New Folder");
        append_required_label(args, "Duplicate");
        append_required_label(args, "Delete");
        append_required_label(args, "System");
        append_required_label(args, "Pretendard");
        append_required_label(args, "Text +");
        append_required_label(args, "Text -");
        append_required_label(args, "Heading +");
        append_required_label(args, "Small +");
        append_required_label(args, "Scroll +");
        append_required_label(args, "Scroll -");
        append_required_label_contains(args, "Selected README.txt");
        append_required_debug_detail(
            args, "application.file_explorer.chrome.more_actions_open=true");
        append_required_debug_detail(
            args, "application.file_explorer.chrome.overflow_action_button_count=26");
        append_required_debug_detail(
            args, "application.file_explorer.selection.present=true");
        append_required_debug_detail(
            args, "application.file_explorer.selection.name=\"README.txt\"");
    } else if (scenario == "preferences-panel") {
        append_required_label(args, "Display");
        append_required_label(args, "System");
        append_required_label(args, "Pretendard");
        append_required_label(args, "Text +");
        append_required_label(args, "Text -");
        append_required_label(args, "Heading +");
        append_required_label(args, "Small +");
        append_required_label(args, "Scroll +");
        append_required_label(args, "Scroll -");
        append_required_debug_detail(
            args, "application.file_explorer.mobile_tab=2");
        append_required_debug_detail(
            args, "application.file_explorer.status=\"Display preferences ready.\"");
    } else {
        return std::unexpected{std::format(
            "unknown file explorer startup scenario: {}",
            scenario)};
    }
    return {};
}

auto file_explorer_desktop_default_requirements(std::string_view mode)
    -> std::vector<std::string> {
    auto args = std::vector<std::string>{};
    if (mode == "icon") {
        append_required_label(args, "README.txt");
    } else if (mode == "list") {
        append_required_label(args, "Name");
        append_required_label(args, "Kind");
        append_required_label(args, "Size");
        append_required_material_surface_role(args, "content");
    } else if (mode == "column") {
        append_required_label(args, "Preview");
        append_required_label(args, "Demo Root");
    } else if (mode == "gallery") {
        append_required_label(args, "Select a file to preview.");
    }
    return args;
}

auto file_explorer_base_verifier_args(fs::path const& bundle)
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
    };
}

auto file_explorer_verifier_args(fs::path const& root,
                                 fs::path const& bundle,
                                 std::string_view profile,
                                 std::string_view mode,
                                 std::string_view scenario)
    -> std::expected<std::vector<std::string>, std::string> {
    auto args = file_explorer_base_verifier_args(bundle);
    auto const has_scenario = !scenario.empty() && scenario != "default";
    auto const use_manifest =
        !has_scenario && (profile == "mobile" || mode == "icon");
    if (!use_manifest) {
        append_verifier_arg(args, "--expect-platform", "macos");
        args.push_back("--require-frame");
        append_required_role(args, "button");
        append_required_role(args, "material");
        if (profile == "mobile")
            append_required_role(args, "text_field");
        args.push_back("--require-material-plan");
        args.push_back("--require-material-semantic-runtime-match");
        if (profile == "desktop" && !mode.empty()) {
            append_required_debug_detail(args, std::format(
                "application.file_explorer.view_mode.value=\"{}\"",
                mode));
        }
    } else {
        auto manifest = root / "examples"
            / (profile == "mobile"
                   ? fs::path{"file_explorer_mobile/artifact_manifest.json"}
                   : fs::path{"file_explorer_desktop/artifact_manifest.json"});
        append_verifier_arg(args, "--manifest", path_string(manifest));
        if (profile == "desktop") {
            auto extra = file_explorer_desktop_default_requirements(mode);
            args.insert(args.end(), extra.begin(), extra.end());
        }
    }
    append_required_debug_detail(
        args,
        "application.file_explorer.input_model.focus_visibility_policy=\"keyboard_tab_navigation_shows_ring_pointer_click_hides_ring\"");
    append_required_debug_detail(
        args,
        "application.file_explorer.input_model.focus_ring_style=\"macos_blue_keyboard_focus_ring_outset_4px_2px_stroke\"");
    append_required_debug_detail(
        args,
        "application.file_explorer.input_model.focus_visible=false");
    append_required_debug_detail(args, "platform_runtime.focus_visible=false");
    if (profile == "desktop") {
        append_required_runtime_detail(args, "window.chrome=\"integrated_titlebar\"");
        append_required_runtime_detail(args, "window.titlebar_transparent=true");
        append_required_runtime_detail(args, "window.full_size_content_view=true");
        append_required_runtime_detail(args, "window.title_hidden=true");
        append_required_runtime_detail(
            args,
            "window.native_window_controls.integrated_in_content_area=true");
        append_required_runtime_detail(
            args,
            "window.native_window_controls.duplicate_window_controls=false");
        append_required_runtime_detail(
            args,
            "window.native_window_controls.content_drawn_window_control_count=0");
        append_required_runtime_detail(
            args,
            "window.native_window_controls.artifact_drawn_window_control_count=0");
    }
    auto scenario_extra =
        append_file_explorer_scenario_requirements(args, profile, scenario);
    if (!scenario_extra)
        return std::unexpected{scenario_extra.error()};
    return args;
}

auto run_file_explorer_verifier(fs::path const& root,
                                std::vector<std::string> args)
    -> std::expected<cppx::process::CapturedProcessResult,
                     cppx::process::process_error> {
    auto spec = cppx::process::ProcessSpec{
        .program = "mise",
        .args = std::move(args),
        .cwd = root,
        .timeout = std::chrono::minutes{5},
        .env_overrides = {
            {"UV_PROJECT_ENVIRONMENT", uv_project_environment()},
        },
    };
    return cppx::process::system::capture(spec);
}

auto is_desktop_only_file_explorer_scenario(std::string_view scenario) -> bool {
    return scenario == "more-actions-open" || scenario == "svg-selected";
}

auto is_mobile_only_file_explorer_scenario(std::string_view scenario) -> bool {
    return scenario == "preferences-panel";
}

auto artifact_dir_for_case(std::optional<fs::path> const& root,
                           std::string_view prefix,
                           std::string_view root_case,
                           std::string_view case_id)
    -> std::expected<fs::path, std::string> {
    if (root) {
        auto path = *root;
        if (case_id != root_case)
            path = fs::path{path_string(*root) + "-" + std::string{case_id}};
        auto ec = std::error_code{};
        fs::create_directories(path, ec);
        if (ec) {
            return std::unexpected{std::format(
                "could not create artifact directory {}: {}",
                path_string(path),
                ec.message())};
        }
        return path.lexically_normal();
    }
    auto temp = make_temp_directory(std::format("{}-{}", prefix, case_id));
    if (!temp)
        return std::unexpected{temp.error()};
    return *temp;
}

void reset_file_explorer_demo_profile(std::string_view profile) {
    auto ec = std::error_code{};
    auto base = fs::temp_directory_path(ec);
    if (ec)
        return;
    fs::remove_all(base / std::format("phenotype-file-explorer-{}", profile), ec);
}

auto artifact_root_override(cppx::cli::Invocation const& invocation,
                            std::string_view option,
                            std::string_view env_name) -> std::optional<fs::path> {
    if (auto value = invocation.value(option))
        return fs::path{absolute_path_string(fs::path{std::string{*value}})};
    if (auto value = environment_value(env_name))
        return fs::path{absolute_path_string(fs::path{*value})};
    return std::nullopt;
}

auto file_explorer_case_json(FileExplorerArtifactCase const& item)
    -> std::string {
    auto artifact = item.artifact
        ? artifact_detail_json(*item.artifact)
        : std::string{"null"};
    return std::format(
        "{{\"profile\":{},\"mode\":{},\"scenario\":{},"
        "\"artifact_reason\":{},\"artifact_dir\":{},\"ok\":{},"
        "\"run_result\":{},\"verifier\":{},\"artifact\":{},\"error\":{}}}",
        json_string(item.profile),
        json_string(item.mode),
        json_string(item.scenario),
        json_string(item.artifact_reason),
        json_string(path_string(item.artifact_dir)),
        item.ok ? "true" : "false",
        process_result_detail_json(item.run_result),
        process_result_detail_json(item.verifier_result),
        artifact,
        json_string(item.error));
}

auto file_explorer_cases_json(
        std::span<FileExplorerArtifactCase const> cases) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < cases.size(); ++i) {
        if (i > 0)
            out += ",";
        out += file_explorer_case_json(cases[i]);
    }
    out += "]";
    return out;
}

auto file_explorer_gate_json(
        FileExplorerArtifactGateSummary const& summary) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"artifact verify-file-explorer\","
        "\"ok\":{},\"profile\":{},\"desktop_modes\":{},\"scenarios\":{},"
        "\"shared_root\":{},\"desktop_root\":{},\"mobile_root\":{},"
        "\"desktop_executable\":{},\"mobile_executable\":{},"
        "\"shared_test\":{},\"desktop_build\":{},\"mobile_build\":{},"
        "\"cases\":{},\"error\":{}}}",
        summary.ok ? "true" : "false",
        json_string(summary.profile),
        string_array_json(summary.desktop_modes),
        string_array_json(summary.scenarios),
        json_string(path_string(summary.shared_root)),
        json_string(path_string(summary.desktop_root)),
        json_string(path_string(summary.mobile_root)),
        json_string(path_string(summary.desktop_executable)),
        json_string(path_string(summary.mobile_executable)),
        process_result_detail_json(summary.shared_test_result),
        process_result_detail_json(summary.desktop_build_result),
        process_result_detail_json(summary.mobile_build_result),
        file_explorer_cases_json(summary.cases),
        json_string(summary.error));
}

void print_file_explorer_gate(
        FileExplorerArtifactGateSummary const& summary) {
    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "profile",
         .value = summary.profile,
         .status = cppx::terminal::StatusKind::ok},
        {.label = "shared test",
         .value = result_value(summary.shared_test_result),
         .status = result_status(summary.shared_test_result)},
    };
    if (summary.profile == "all" || summary.profile == "desktop") {
        lines.push_back({
            .label = "desktop build",
            .value = result_value(summary.desktop_build_result),
            .status = result_status(summary.desktop_build_result),
        });
    }
    if (summary.profile == "all" || summary.profile == "mobile") {
        lines.push_back({
            .label = "mobile build",
            .value = result_value(summary.mobile_build_result),
            .status = result_status(summary.mobile_build_result),
        });
    }
    auto passed_cases = std::ranges::count_if(
        summary.cases,
        [](FileExplorerArtifactCase const& item) { return item.ok; });
    lines.push_back({
        .label = "artifact cases",
        .value = std::format("{}/{}", passed_cases, summary.cases.size()),
        .status = passed_cases == static_cast<std::ptrdiff_t>(summary.cases.size())
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail,
    });
    std::println("phenotype artifact verify-file-explorer");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    if (!summary.error.empty()) {
        std::println("{}",
                     cppx::terminal::format_diagnostic({
                         .severity = cppx::terminal::DiagnosticSeverity::error,
                         .message = summary.error,
                         .context = "artifact verify-file-explorer",
                     }));
    }
    for (auto const& item : summary.cases) {
        if (item.ok)
            continue;
        std::println("failed case: profile={} mode={} scenario={} bundle={}",
                     item.profile,
                     item.mode.empty() ? "<none>" : item.mode,
                     item.scenario.empty() ? "default" : item.scenario,
                     path_string(item.artifact_dir));
        if (item.verifier_result && !item.verifier_result->stdout_text.empty()) {
            std::println("verifier report tail:");
            std::println("{}", output_tail(item.verifier_result->stdout_text));
        }
        break;
    }
}

int emit_file_explorer_gate(
        FileExplorerArtifactGateSummary const& summary,
        cppx::cli::Invocation const& invocation,
        int exit_code) {
    if (invocation.has("json")) {
        std::println("{}", file_explorer_gate_json(summary));
    } else {
        print_file_explorer_gate(summary);
    }
    return exit_code;
}

auto run_file_explorer_case(fs::path const& root,
                            fs::path const& example_root,
                            fs::path const& executable,
                            std::string profile,
                            std::string mode,
                            std::string scenario,
                            fs::path artifact_dir,
                            std::string accessibility_display,
                            std::chrono::milliseconds settle)
    -> std::expected<FileExplorerArtifactCase, std::string> {
    auto const scenario_active = !scenario.empty() && scenario != "default";
    auto item = FileExplorerArtifactCase{
        .profile = std::move(profile),
        .mode = std::move(mode),
        .scenario = scenario_active ? scenario : std::string{"default"},
        .artifact_dir = std::move(artifact_dir),
    };
    item.artifact_reason = item.profile == "desktop"
        ? std::format("file-explorer-desktop-{}{}{}-gate",
                      item.mode,
                      scenario_active ? "-" : "",
                      scenario_active ? item.scenario : "")
        : std::format("file-explorer-mobile{}{}-gate",
                      scenario_active ? "-" : "",
                      scenario_active ? item.scenario : "");

    auto env = std::map<std::string, std::string>{
        {"PHENOTYPE_ARTIFACT_DIR", path_string(item.artifact_dir)},
        {"PHENOTYPE_ARTIFACT_REASON", item.artifact_reason},
        {"PHENOTYPE_ACCESSIBILITY_DISPLAY", std::move(accessibility_display)},
        {"PHENOTYPE_ARTIFACT_EXIT", "1"},
        {"PHENOTYPE_FILE_EXPLORER_COLOR_SCHEME", "light"},
    };
    if (item.profile == "desktop")
        env["PHENOTYPE_FILE_EXPLORER_VIEW"] = item.mode;
    if (scenario_active) {
        if (item.scenario == "more-actions-open") {
            env["PHENOTYPE_FILE_EXPLORER_MORE_ACTIONS"] = "1";
            env["PHENOTYPE_FILE_EXPLORER_INPUTS"] = "select:README.txt";
        } else if (item.scenario == "svg-selected") {
            env["PHENOTYPE_FILE_EXPLORER_INPUTS"] = "select:Glass Symbol.svg";
        } else {
            env["PHENOTYPE_FILE_EXPLORER_SCENARIO"] = item.scenario;
        }
    }

    reset_file_explorer_demo_profile(item.profile);
    if (settle.count() > 0)
        std::this_thread::sleep_for(settle);
    auto run = run_example_binary(
        executable,
        example_root,
        std::move(env),
        std::chrono::seconds{120});
    if (!run) {
        item.error = std::format(
            "failed to run file explorer example: {}",
            cppx::process::to_string(run.error()));
        return item;
    }
    item.run_result = std::move(*run);
    item.artifact = artifact_summary(item.artifact_dir);
    if (item.run_result->timed_out || item.run_result->exit_code != 0) {
        item.error = item.run_result->timed_out
            ? "file explorer example timed out"
            : std::format(
                "file explorer example exited with {}",
                item.run_result->exit_code);
        return item;
    }

    auto verifier_args = file_explorer_verifier_args(
        root,
        item.artifact_dir,
        item.profile,
        item.mode,
        item.scenario);
    if (!verifier_args) {
        item.error = verifier_args.error();
        return item;
    }
    auto verifier = run_file_explorer_verifier(root, std::move(*verifier_args));
    if (!verifier) {
        item.error = std::format(
            "failed to run mise/uv verifier: {}",
            cppx::process::to_string(verifier.error()));
        return item;
    }
    item.verifier_result = std::move(*verifier);
    item.ok = item.artifact
        && item.artifact->snapshot_json
        && !item.verifier_result->timed_out
        && item.verifier_result->exit_code == 0;
    if (!item.ok) {
        if (!item.artifact || !item.artifact->snapshot_json) {
            item.error = "artifact bundle did not contain snapshot.json";
        } else if (item.verifier_result->timed_out) {
            item.error = "artifact verifier timed out";
        } else {
            item.error = std::format(
                "artifact verifier exited with {}",
                item.verifier_result->exit_code);
        }
    }
    return item;
}

export int run_artifact_verify_file_explorer(
        cppx::cli::Invocation const& invocation) {
    auto root = find_repo_root(fs::current_path());
    if (!root) {
        return print_error(
            "artifact verify-file-explorer",
            "could not find phenotype repository root from current directory",
            invocation.has("json"));
    }

    auto profile = file_explorer_profile_from_invocation(invocation);
    if (!profile) {
        return print_error(
            "artifact verify-file-explorer",
            profile.error(),
            invocation.has("json"));
    }
    auto desktop_modes = invocation_list_or_env(
        invocation,
        "view-mode",
        "PHENOTYPE_FILE_EXPLORER_DESKTOP_MODES",
        default_file_explorer_desktop_modes());
    if (auto valid = validate_desktop_modes(desktop_modes); !valid) {
        return print_error(
            "artifact verify-file-explorer",
            valid.error(),
            invocation.has("json"));
    }
    auto scenarios = normalize_file_explorer_scenarios(invocation_list_or_env(
        invocation,
        "scenario",
        "PHENOTYPE_FILE_EXPLORER_SCENARIOS",
        default_file_explorer_scenarios()));

    auto settle = std::chrono::milliseconds{250};
    if (auto value = invocation.value("settle-seconds")) {
        auto parsed = parse_settle_seconds(*value);
        if (!parsed) {
            return print_error(
                "artifact verify-file-explorer",
                parsed.error(),
                invocation.has("json"));
        }
        settle = *parsed;
    } else if (auto value = environment_value(
                   "PHENOTYPE_FILE_EXPLORER_CAPTURE_SETTLE_SECONDS")) {
        auto parsed = parse_settle_seconds(*value);
        if (!parsed) {
            return print_error(
                "artifact verify-file-explorer",
                parsed.error(),
                invocation.has("json"));
        }
        settle = *parsed;
    }

    auto accessibility_display =
        environment_value("PHENOTYPE_ACCESSIBILITY_DISPLAY").value_or("standard");
    auto desktop_artifact_root = artifact_root_override(
        invocation,
        "desktop-artifact-dir",
        "PHENOTYPE_FILE_EXPLORER_DESKTOP_ARTIFACT_DIR");
    auto mobile_artifact_root = artifact_root_override(
        invocation,
        "mobile-artifact-dir",
        "PHENOTYPE_FILE_EXPLORER_MOBILE_ARTIFACT_DIR");

    auto summary = FileExplorerArtifactGateSummary{
        .profile = *profile,
        .desktop_modes = desktop_modes,
        .scenarios = scenarios,
        .shared_root = *root / "examples" / "file_explorer_shared",
        .desktop_root = *root / "examples" / "file_explorer_desktop",
        .mobile_root = *root / "examples" / "file_explorer_mobile",
    };

    auto desktop_package = exon_package_name(summary.desktop_root);
    if (!desktop_package) {
        summary.error = desktop_package.error();
        return emit_file_explorer_gate(summary, invocation, 2);
    }
    summary.desktop_package_name = *desktop_package;
    summary.desktop_executable = summary.desktop_root / ".exon" / "debug"
        / executable_filename(*desktop_package);
    auto mobile_package = exon_package_name(summary.mobile_root);
    if (!mobile_package) {
        summary.error = mobile_package.error();
        return emit_file_explorer_gate(summary, invocation, 2);
    }
    summary.mobile_package_name = *mobile_package;
    summary.mobile_executable = summary.mobile_root / ".exon" / "debug"
        / executable_filename(*mobile_package);

    auto shared_test = run_exon_at(
        summary.shared_root,
        {"test"},
        std::chrono::minutes{45});
    if (!shared_test) {
        summary.error = std::format(
            "failed to run shared file explorer tests: {}",
            cppx::process::to_string(shared_test.error()));
        return emit_file_explorer_gate(summary, invocation, 2);
    }
    summary.shared_test_result = std::move(*shared_test);
    if (summary.shared_test_result->timed_out
        || summary.shared_test_result->exit_code != 0) {
        summary.error = "shared file explorer tests failed";
        auto code = summary.shared_test_result->timed_out
            ? 124
            : summary.shared_test_result->exit_code;
        return emit_file_explorer_gate(summary, invocation, code);
    }

    if (summary.profile == "all" || summary.profile == "desktop") {
        auto build = run_example_build(summary.desktop_root);
        if (!build) {
            summary.error = std::format(
                "failed to build desktop file explorer: {}",
                cppx::process::to_string(build.error()));
            return emit_file_explorer_gate(summary, invocation, 2);
        }
        summary.desktop_build_result = std::move(*build);
        if (summary.desktop_build_result->timed_out
            || summary.desktop_build_result->exit_code != 0) {
            summary.error = "desktop file explorer build failed";
            auto code = summary.desktop_build_result->timed_out
                ? 124
                : summary.desktop_build_result->exit_code;
            return emit_file_explorer_gate(summary, invocation, code);
        }
        for (auto const& mode : summary.desktop_modes) {
            auto dir = artifact_dir_for_case(
                desktop_artifact_root,
                "phenotype-file-explorer-desktop",
                "icon",
                mode);
            if (!dir) {
                summary.error = dir.error();
                return emit_file_explorer_gate(summary, invocation, 2);
            }
            auto item = run_file_explorer_case(
                *root,
                summary.desktop_root,
                summary.desktop_executable,
                "desktop",
                mode,
                "default",
                *dir,
                accessibility_display,
                settle);
            if (!item) {
                summary.error = item.error();
                return emit_file_explorer_gate(summary, invocation, 2);
            }
            auto exit_code = item->verifier_result
                ? (item->verifier_result->timed_out
                       ? 124
                       : item->verifier_result->exit_code)
                : 1;
            auto ok = item->ok;
            if (!ok)
                summary.error = item->error;
            summary.cases.push_back(std::move(*item));
            if (!ok)
                return emit_file_explorer_gate(summary, invocation, exit_code);
        }
        for (auto const& scenario : summary.scenarios) {
            if (scenario == "default"
                || is_mobile_only_file_explorer_scenario(scenario)) {
                continue;
            }
            auto case_id = std::format("icon-{}", scenario);
            auto dir = artifact_dir_for_case(
                desktop_artifact_root,
                "phenotype-file-explorer-desktop",
                "icon",
                case_id);
            if (!dir) {
                summary.error = dir.error();
                return emit_file_explorer_gate(summary, invocation, 2);
            }
            auto item = run_file_explorer_case(
                *root,
                summary.desktop_root,
                summary.desktop_executable,
                "desktop",
                "icon",
                scenario,
                *dir,
                accessibility_display,
                settle);
            if (!item) {
                summary.error = item.error();
                return emit_file_explorer_gate(summary, invocation, 2);
            }
            auto exit_code = item->verifier_result
                ? (item->verifier_result->timed_out
                       ? 124
                       : item->verifier_result->exit_code)
                : 1;
            auto ok = item->ok;
            if (!ok)
                summary.error = item->error;
            summary.cases.push_back(std::move(*item));
            if (!ok)
                return emit_file_explorer_gate(summary, invocation, exit_code);
        }
    }

    if (summary.profile == "all" || summary.profile == "mobile") {
        auto build = run_example_build(summary.mobile_root);
        if (!build) {
            summary.error = std::format(
                "failed to build mobile file explorer: {}",
                cppx::process::to_string(build.error()));
            return emit_file_explorer_gate(summary, invocation, 2);
        }
        summary.mobile_build_result = std::move(*build);
        if (summary.mobile_build_result->timed_out
            || summary.mobile_build_result->exit_code != 0) {
            summary.error = "mobile file explorer build failed";
            auto code = summary.mobile_build_result->timed_out
                ? 124
                : summary.mobile_build_result->exit_code;
            return emit_file_explorer_gate(summary, invocation, code);
        }
        auto default_dir = artifact_dir_for_case(
            mobile_artifact_root,
            "phenotype-file-explorer-mobile",
            "default",
            "default");
        if (!default_dir) {
            summary.error = default_dir.error();
            return emit_file_explorer_gate(summary, invocation, 2);
        }
        auto default_item = run_file_explorer_case(
            *root,
            summary.mobile_root,
            summary.mobile_executable,
            "mobile",
            "",
            "default",
            *default_dir,
            accessibility_display,
            settle);
        if (!default_item) {
            summary.error = default_item.error();
            return emit_file_explorer_gate(summary, invocation, 2);
        }
        auto default_exit = default_item->verifier_result
            ? (default_item->verifier_result->timed_out
                   ? 124
                   : default_item->verifier_result->exit_code)
            : 1;
        auto default_ok = default_item->ok;
        if (!default_ok)
            summary.error = default_item->error;
        summary.cases.push_back(std::move(*default_item));
        if (!default_ok)
            return emit_file_explorer_gate(summary, invocation, default_exit);

        for (auto const& scenario : summary.scenarios) {
            if (scenario == "default"
                || is_desktop_only_file_explorer_scenario(scenario)) {
                continue;
            }
            auto dir = artifact_dir_for_case(
                mobile_artifact_root,
                "phenotype-file-explorer-mobile",
                "default",
                scenario);
            if (!dir) {
                summary.error = dir.error();
                return emit_file_explorer_gate(summary, invocation, 2);
            }
            auto item = run_file_explorer_case(
                *root,
                summary.mobile_root,
                summary.mobile_executable,
                "mobile",
                "",
                scenario,
                *dir,
                accessibility_display,
                settle);
            if (!item) {
                summary.error = item.error();
                return emit_file_explorer_gate(summary, invocation, 2);
            }
            auto exit_code = item->verifier_result
                ? (item->verifier_result->timed_out
                       ? 124
                       : item->verifier_result->exit_code)
                : 1;
            auto ok = item->ok;
            if (!ok)
                summary.error = item->error;
            summary.cases.push_back(std::move(*item));
            if (!ok)
                return emit_file_explorer_gate(summary, invocation, exit_code);
        }
    }

    summary.ok = std::ranges::all_of(
        summary.cases,
        [](FileExplorerArtifactCase const& item) { return item.ok; });
    return emit_file_explorer_gate(summary, invocation, summary.ok ? 0 : 1);
}


} // namespace phenotype_cli::file_explorer_gate

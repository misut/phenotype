export module phenotype_cli.app;

import cppx.cli;
import cppx.process;
import cppx.process.system;
import cppx.terminal;
import file_explorer_shared;
import json;
import phenotype_cli.common;
import phenotype_cli.commands;
import phenotype_cli.contracts;
import phenotype_cli.file_explorer;
import phenotype_cli.file_explorer_gate;
import phenotype_cli.glass_showcase;
import phenotype_cli.icon_file_types;
import phenotype_cli.icon_sources;
import phenotype_cli.icons;
import phenotype_cli.package;
import phenotype_cli.runtime;
import phenotype.resources;
import std;

namespace {

namespace fs = std::filesystem;
namespace cli_package = phenotype_cli::package;
using namespace phenotype_cli::common;
using namespace phenotype_cli::commands;
using namespace phenotype_cli::contracts;
using namespace phenotype_cli::file_explorer;
using namespace phenotype_cli::file_explorer_gate;
using namespace phenotype_cli::glass_showcase;
using namespace phenotype_cli::icon_file_types;
using namespace phenotype_cli::icon_sources;
using namespace phenotype_cli::icons;
using namespace phenotype_cli::runtime;

struct ToolMigration {
    std::string legacy_path;
    std::string replacement_command;
    std::string status;
    std::string edge_boundary;
    std::string removal_policy;
    bool legacy_present = false;
    bool replacement_implemented = true;
    bool removal_ready = false;
};

struct MaterialObservationSummary {
    bool renderer_details_present = false;
    bool runtime_summary_present = false;
    std::int64_t contract_version = -1;
    std::int64_t declared_plan_count = -1;
    std::size_t plan_count = 0;
    std::size_t fallback_count = 0;
    std::size_t backdrop_sampling_count = 0;
    std::size_t shared_frame_capture_count = 0;
    std::size_t next_frame_capture_required_count = 0;
    std::int64_t runtime_plan_count = -1;
    std::int64_t runtime_backdrop_copy_count = -1;
    std::int64_t runtime_next_frame_capture_plan_count = -1;
    std::vector<std::string> kinds;
    std::vector<std::string> roles;
    std::vector<std::string> fallback_paths;
    std::vector<std::string> fallback_reasons;
    std::vector<std::string> backdrop_sources;
    std::vector<std::string> backdrop_access_sources;
    std::vector<std::string> backdrop_capture_reasons;
    std::vector<std::string> primary_passes;
    std::vector<std::string> primary_executors;
    std::vector<std::string> likely_layers;
    std::vector<std::string> likely_passes;
    std::vector<std::string> decision_blockers;
};

struct SnapshotObservation {
    bool present = false;
    bool parse_ok = false;
    std::string parse_error;
    std::vector<std::string> top_level_keys;
    bool debug_present = false;
    bool semantic_tree_present = false;
    bool platform_capabilities_present = false;
    bool platform_runtime_present = false;
    bool runtime_details_present = false;
    bool renderer_details_present = false;
    bool material_surfaces_capability = false;
    bool material_backdrop_blur_capability = false;
    bool frame_image_capability = false;
    bool write_artifact_bundle_capability = false;
    std::string platform_name;
    MaterialObservationSummary material;
};

struct VerifierObservation {
    bool requested = false;
    bool executed = false;
    bool ok = false;
    int exit_code = 0;
    bool timed_out = false;
    std::string stdout_tail;
    std::string stderr_tail;
    std::optional<json::Value> report;
    std::string report_error;
};

struct ArtifactObservation {
    fs::path bundle;
    ArtifactSummary artifact;
    std::vector<Check> checks;
    SnapshotObservation snapshot;
    VerifierObservation verifier;
    std::vector<std::string> likely_owner_layers;
    std::vector<std::string> suggested_actions;
    bool ok = false;
};



struct ExampleRunSummary {
    std::string example;
    fs::path example_root;
    std::string package_name;
    fs::path executable;
    fs::path artifact_dir;
    bool build_requested = true;
    bool run_requested = true;
    bool artifact_requested = false;
    bool artifact_exit = false;
    bool output_observation_requested = false;
    std::size_t file_explorer_input_count = 0;
    std::size_t file_explorer_script_input_count = 0;
    fs::path file_explorer_script;
    std::string file_explorer_artifact_chrome_markers;
    bool file_explorer_artifact_chrome_markers_injected = false;
    std::optional<std::chrono::milliseconds> run_timeout;
    std::optional<cppx::process::CapturedProcessResult> build_result;
    std::optional<cppx::process::CapturedProcessResult> run_result;
    std::optional<ArtifactSummary> artifact;
    std::optional<ArtifactObservation> output_observation;
    bool ok = false;
    std::string error;
};

auto join_path(fs::path const& base, fs::path const& child) -> std::string {
    auto ec = std::error_code{};
    return path_string(fs::relative(base / child, base, ec));
}

auto parse_locale_strings(fs::path const& path)
    -> std::vector<phenotype::LocaleString> {
    return phenotype::parse_resource_locale_strings(read_text_file(path));
}

auto doctor_checks() -> std::vector<Check> {
    auto checks = std::vector<Check>{};
    auto root = find_repo_root(fs::current_path());

    checks.push_back({
        .name = "repo_root",
        .ok = root.has_value(),
        .detail = root ? path_string(*root) : "not found from current directory",
        .hint = root ? "" : "Run the command from the phenotype repository or worktree.",
    });

    if (!root) {
        return checks;
    }

    auto add_path_check = [&](std::string name, fs::path relative,
                              std::string hint = {}) {
        auto full = *root / relative;
        checks.push_back({
            .name = std::move(name),
            .ok = path_exists(full),
            .detail = join_path(*root, relative),
            .hint = path_exists(full) ? "" : std::move(hint),
        });
    };

    add_path_check("mise_config", "mise.toml",
                   "Install or run inside a checked-out phenotype worktree.");
    add_path_check("artifact_verifier", "tools/verify_artifact_bundle.py",
                   "Keep the Python verifier until CLI parity is complete.");
    add_path_check("glass_showcase_gate", "tools/verify_glass_showcase_artifact.sh",
                   "Compatibility wrapper should delegate to phenotype artifact verify-glass-showcase.");
    add_path_check("android_contract_gate", "tools/android/contract.sh",
                   "Android backend contract coverage should remain callable.");
    add_path_check("cli_roadmap", "docs/PHENOTYPE_CLI_ROADMAP.md",
                   "Document CLI migration before replacing shell or Python tools.");
    add_path_check("file_explorer_shared", "examples/file_explorer_shared/exon.toml",
                   "Shared model package should stay available for examples.");
    add_path_check("glass_showcase_shared", "examples/glass_showcase_shared/exon.toml",
                   "Shared material probe package should stay available for examples.");
    add_path_check("theme_contract_package", "packages/phenotype_theme_contract/exon.toml",
                   "Pure default glass theme metadata should stay package-testable.");
    add_path_check("svg_contract_package", "packages/phenotype_svg_contract/exon.toml",
                   "Pure SVG asset inspection should stay available to Linux CLI gates.");

    return checks;
}

auto tool_migrations(fs::path const& root) -> std::vector<ToolMigration> {
    auto make = [&](std::string legacy_path,
                    std::string replacement_command,
                    std::string status,
                    std::string edge_boundary,
                    std::string removal_policy,
                    bool removal_ready = false) {
        return ToolMigration{
            .legacy_path = legacy_path,
            .replacement_command = replacement_command,
            .status = status,
            .edge_boundary = edge_boundary,
            .removal_policy = removal_policy,
            .legacy_present = path_exists(root / legacy_path),
            .replacement_implemented = true,
            .removal_ready = removal_ready,
        };
    };

    return {
        make("tools/verify_artifact_bundle.py",
             "phenotype artifact verify <bundle>",
             "uv_managed_reference_verifier",
             "Python process execution stays at the CLI edge.",
             "Keep until the native verifier matches representative failure shapes."),
        make("tools/verify_glass_showcase_artifact.sh",
             "phenotype artifact verify-glass-showcase",
             "thin_compatibility_wrapper",
             "Native capture, process execution, and uv verifier calls stay at the CLI edge.",
             "Remove after local docs and developer workflows stop invoking the wrapper.",
             true),
        make("tools/verify_glass_showcase_accessibility_artifact.sh",
             "phenotype artifact verify-glass-showcase --accessibility",
             "thin_compatibility_wrapper",
             "Accessibility capture flags and uv verifier calls stay at the CLI edge.",
             "Remove after local docs and developer workflows stop invoking the wrapper.",
             true),
        make("tools/verify_file_explorer_artifacts.sh",
             "phenotype artifact verify-file-explorer",
             "thin_compatibility_wrapper",
             "Native capture, shared-model tests, and uv verifier calls stay at the CLI edge.",
             "Remove after local docs and developer workflows stop invoking the wrapper.",
             true),
        make("tools/android/doctor.sh",
             "phenotype android doctor",
             "cli_namespace_delegates_to_edge_script",
             "Android SDK, JDK, adb, and emulator probes stay at the CLI edge.",
             "Keep as edge implementation until Android process control moves into the native CLI adapter."),
        make("tools/android/run.sh",
             "phenotype android run",
             "cli_namespace_delegates_to_edge_script",
             "APK install, app launch, and log sampling stay at the CLI edge.",
             "Keep as edge implementation until Android process control moves into the native CLI adapter."),
        make("tools/android/contract.sh",
             "phenotype android contract",
             "cli_namespace_delegates_to_edge_script",
             "Device artifact collection and verifier execution stay at the CLI edge.",
             "Keep as edge implementation until Android process control moves into the native CLI adapter."),
    };
}

auto tool_migration_json(ToolMigration const& migration) -> std::string {
    return std::format(
        "{{\"legacy_path\":{},\"replacement_command\":{},"
        "\"status\":{},\"edge_boundary\":{},\"removal_policy\":{},"
        "\"legacy_present\":{},\"replacement_implemented\":{},"
        "\"removal_ready\":{}}}",
        json_string(migration.legacy_path),
        json_string(migration.replacement_command),
        json_string(migration.status),
        json_string(migration.edge_boundary),
        json_string(migration.removal_policy),
        migration.legacy_present ? "true" : "false",
        migration.replacement_implemented ? "true" : "false",
        migration.removal_ready ? "true" : "false");
}

auto tool_migrations_json(std::span<ToolMigration const> migrations)
    -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < migrations.size(); ++i) {
        if (i > 0)
            out += ",";
        out += tool_migration_json(migrations[i]);
    }
    out += "]";
    return out;
}

auto tool_migration_summary_json(std::span<ToolMigration const> migrations)
    -> std::string {
    auto present = std::size_t{0};
    auto implemented = std::size_t{0};
    auto removal_ready = std::size_t{0};
    for (auto const& migration : migrations) {
        if (migration.legacy_present)
            ++present;
        if (migration.replacement_implemented)
            ++implemented;
        if (migration.removal_ready)
            ++removal_ready;
    }
    return std::format(
        "{{\"legacy_present\":{},\"replacement_implemented\":{},"
        "\"removal_ready\":{},\"entries\":{}}}",
        present,
        implemented,
        removal_ready,
        migrations.size());
}

void append_command_entries(cppx::cli::CommandSpec const& command,
                            std::vector<std::string>& path,
                            std::string& out,
                            bool& first) {
    if (!first)
        out += ",";
    first = false;
    out += std::format(
        "{{\"path\":{},\"spec\":{}}}",
        string_array_json(path),
        cppx::cli::command_metadata_json(command));

    for (auto const& subcommand : command.subcommands) {
        if (subcommand.hidden)
            continue;
        path.push_back(subcommand.name);
        append_command_entries(subcommand, path, out, first);
        path.pop_back();
    }
}

auto command_tree_json(cppx::cli::CommandSpec const& root) -> std::string {
    auto out = std::string{
        "{\"schema_version\":1,\"command\":\"commands\",\"root\":"};
    out += json_string(root.name);
    out += ",\"commands\":[";
    auto first = true;
    for (auto const& command : root.subcommands) {
        if (command.hidden)
            continue;
        auto path = std::vector<std::string>{root.name, command.name};
        append_command_entries(command, path, out, first);
    }
    out += "]}";
    return out;
}

auto artifact_checks(ArtifactSummary const& summary) -> std::vector<Check> {
    return {
        {.name = "bundle",
         .ok = summary.exists && summary.is_directory,
         .detail = path_string(summary.bundle),
         .hint = "Point to the artifact bundle directory produced by the renderer."},
        {.name = "snapshot_json",
         .ok = summary.snapshot_json && summary.snapshot_bytes > 0,
         .detail = std::format("{} bytes", summary.snapshot_bytes),
         .hint = "Expected snapshot.json with semantic/debug state."},
        {.name = "frame_bmp",
         .ok = summary.frame_bmp && summary.frame_bytes > 0,
         .detail = std::format("{} bytes", summary.frame_bytes),
         .hint = "Expected frame.bmp from the captured rendered frame."},
        {.name = "platform",
         .ok = summary.platform_directory && summary.platform_file_count > 0,
         .detail = std::format("{} files", summary.platform_file_count),
         .hint = "Expected platform runtime detail files for LLM debugging."},
    };
}

auto artifact_json(ArtifactSummary const& summary,
                   std::span<Check const> checks) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"artifact summary\",\"ok\":{},"
        "\"bundle\":{},\"snapshot_json\":{{\"present\":{},\"bytes\":{}}},"
        "\"frame_bmp\":{{\"present\":{},\"bytes\":{}}},"
        "\"platform\":{{\"present\":{},\"file_count\":{}}},\"checks\":{}}}",
        all_ok(checks) ? "true" : "false",
        json_string(path_string(summary.bundle)),
        summary.snapshot_json ? "true" : "false",
        summary.snapshot_bytes,
        summary.frame_bmp ? "true" : "false",
        summary.frame_bytes,
        summary.platform_directory ? "true" : "false",
        summary.platform_file_count,
        checks_json(checks));
}

void append_unique(std::vector<std::string>& values, std::string value) {
    if (value.empty())
        return;
    if (!std::ranges::contains(values, value))
        values.push_back(std::move(value));
}

void append_json_string_field(json::Value const& value,
                              std::initializer_list<std::string_view> path,
                              std::vector<std::string>& out) {
    if (auto text = json_string_at(value, path))
        append_unique(out, *text);
}

auto json_report_or_null(std::optional<json::Value> const& value)
    -> std::string {
    return value ? json::emit(*value) : "null";
}

auto material_observation_from_snapshot(json::Value const& snapshot)
    -> MaterialObservationSummary {
    auto summary = MaterialObservationSummary{};
    auto const* renderer = json_object_at(
        snapshot,
        {"debug", "platform_runtime", "details", "renderer"});
    if (!renderer)
        return summary;

    summary.renderer_details_present = true;
    auto renderer_value = json::Value{*renderer};
    if (auto contract = json_integer_at(
            renderer_value,
            {"material_plan_contract_version"})) {
        summary.contract_version = *contract;
    }
    if (auto count = json_integer_at(renderer_value, {"material_plan_count"})) {
        summary.declared_plan_count = *count;
    }

    if (auto const* runtime = json_object_at(
            renderer_value,
            {"material_executor_summary"})) {
        summary.runtime_summary_present = true;
        auto runtime_value = json::Value{*runtime};
        if (auto count = json_integer_at(runtime_value, {"plan_count"}))
            summary.runtime_plan_count = *count;
        if (auto count = json_integer_at(runtime_value, {"backdrop_copy_count"}))
            summary.runtime_backdrop_copy_count = *count;
        if (auto count = json_integer_at(
                runtime_value,
                {"next_frame_capture_plan_count"})) {
            summary.runtime_next_frame_capture_plan_count = *count;
        }
    }

    auto const* plans = json_array_at(renderer_value, {"material_plans"});
    if (!plans)
        return summary;

    summary.plan_count = plans->size();
    for (auto const& plan : *plans) {
        if (!plan.is_object())
            continue;
        append_json_string_field(plan, {"kind"}, summary.kinds);
        append_json_string_field(plan, {"role"}, summary.roles);
        append_json_string_field(plan, {"fallback_path"}, summary.fallback_paths);
        append_json_string_field(plan, {"fallback_reason"}, summary.fallback_reasons);
        append_json_string_field(plan, {"backdrop", "source"}, summary.backdrop_sources);
        append_json_string_field(
            plan,
            {"backdrop_access", "source"},
            summary.backdrop_access_sources);
        append_json_string_field(
            plan,
            {"backdrop_access", "capture_reason"},
            summary.backdrop_capture_reasons);
        append_json_string_field(plan, {"primary_pass", "name"}, summary.primary_passes);
        append_json_string_field(
            plan,
            {"primary_pass", "executor"},
            summary.primary_executors);
        append_json_string_field(plan, {"verifier", "likely_layer"}, summary.likely_layers);
        append_json_string_field(plan, {"verifier", "likely_pass"}, summary.likely_passes);
        append_json_string_field(
            plan,
            {"decision_trace", "first_blocker"},
            summary.decision_blockers);
        if (auto fallback = json_bool_at(plan, {"fallback"}); fallback.value_or(false))
            ++summary.fallback_count;
        if (auto sampling = json_bool_at(plan, {"backdrop_sampling"});
            sampling.value_or(false)) {
            ++summary.backdrop_sampling_count;
        }
        if (auto shared = json_bool_at(
                plan,
                {"backdrop_access", "shared_frame_capture"});
            shared.value_or(false)) {
            ++summary.shared_frame_capture_count;
        }
        if (auto next = json_bool_at(
                plan,
                {"backdrop_access", "next_frame_capture_required"});
            next.value_or(false)) {
            ++summary.next_frame_capture_required_count;
        }
    }
    return summary;
}

auto observe_snapshot(fs::path const& bundle) -> SnapshotObservation {
    auto observation = SnapshotObservation{};
    auto snapshot_path = bundle / "snapshot.json";
    observation.present = path_exists(snapshot_path);
    if (!observation.present)
        return observation;

    auto text = read_text_file(snapshot_path);
    if (text.empty()) {
        observation.parse_error = "snapshot.json is empty or unreadable";
        return observation;
    }

    try {
        auto snapshot = json::parse(text);
        observation.parse_ok = true;
        if (snapshot.is_object()) {
            for (auto const& [key, _] : snapshot.as_object())
                observation.top_level_keys.push_back(key);
        }
        observation.debug_present = json_object_at(snapshot, {"debug"}) != nullptr;
        observation.semantic_tree_present =
            json_object_at(snapshot, {"debug", "semantic_tree"}) != nullptr;
        observation.platform_capabilities_present =
            json_object_at(snapshot, {"debug", "platform_capabilities"}) != nullptr;
        observation.platform_runtime_present =
            json_object_at(snapshot, {"debug", "platform_runtime"}) != nullptr;
        observation.runtime_details_present =
            json_object_at(snapshot, {"debug", "platform_runtime", "details"}) != nullptr;
        observation.renderer_details_present =
            json_object_at(
                snapshot,
                {"debug", "platform_runtime", "details", "renderer"}) != nullptr;
        observation.material_surfaces_capability =
            json_bool_at(
                snapshot,
                {"debug", "platform_capabilities", "material_surfaces"})
                .value_or(false);
        observation.material_backdrop_blur_capability =
            json_bool_at(
                snapshot,
                {"debug", "platform_capabilities", "material_backdrop_blur"})
                .value_or(false);
        observation.frame_image_capability =
            json_bool_at(
                snapshot,
                {"debug", "platform_capabilities", "frame_image"})
                .value_or(false);
        observation.write_artifact_bundle_capability =
            json_bool_at(
                snapshot,
                {"debug", "platform_capabilities", "write_artifact_bundle"})
                .value_or(false);
        if (auto platform = json_string_at(
                snapshot,
                {"debug", "platform_runtime", "platform"})) {
            observation.platform_name = *platform;
        }
        observation.material = material_observation_from_snapshot(snapshot);
    } catch (std::exception const& error) {
        observation.parse_error = error.what();
    }
    return observation;
}

auto material_observation_json(MaterialObservationSummary const& summary)
    -> std::string {
    return std::format(
        "{{\"renderer_details_present\":{},\"runtime_summary_present\":{},"
        "\"contract_version\":{},\"declared_plan_count\":{},"
        "\"plan_count\":{},\"fallback_count\":{},"
        "\"backdrop_sampling_count\":{},"
        "\"shared_frame_capture_count\":{},"
        "\"next_frame_capture_required_count\":{},"
        "\"runtime_plan_count\":{},\"runtime_backdrop_copy_count\":{},"
        "\"runtime_next_frame_capture_plan_count\":{},"
        "\"kinds\":{},\"roles\":{},\"fallback_paths\":{},"
        "\"fallback_reasons\":{},\"backdrop_sources\":{},"
        "\"backdrop_access_sources\":{},"
        "\"backdrop_capture_reasons\":{},\"primary_passes\":{},"
        "\"primary_executors\":{},\"likely_layers\":{},"
        "\"likely_passes\":{},\"decision_blockers\":{}}}",
        summary.renderer_details_present ? "true" : "false",
        summary.runtime_summary_present ? "true" : "false",
        summary.contract_version,
        summary.declared_plan_count,
        summary.plan_count,
        summary.fallback_count,
        summary.backdrop_sampling_count,
        summary.shared_frame_capture_count,
        summary.next_frame_capture_required_count,
        summary.runtime_plan_count,
        summary.runtime_backdrop_copy_count,
        summary.runtime_next_frame_capture_plan_count,
        string_array_json(summary.kinds),
        string_array_json(summary.roles),
        string_array_json(summary.fallback_paths),
        string_array_json(summary.fallback_reasons),
        string_array_json(summary.backdrop_sources),
        string_array_json(summary.backdrop_access_sources),
        string_array_json(summary.backdrop_capture_reasons),
        string_array_json(summary.primary_passes),
        string_array_json(summary.primary_executors),
        string_array_json(summary.likely_layers),
        string_array_json(summary.likely_passes),
        string_array_json(summary.decision_blockers));
}

auto snapshot_observation_json(SnapshotObservation const& observation)
    -> std::string {
    return std::format(
        "{{\"present\":{},\"parse_ok\":{},\"parse_error\":{},"
        "\"top_level_keys\":{},\"debug_present\":{},"
        "\"semantic_tree_present\":{},"
        "\"platform_capabilities_present\":{},"
        "\"platform_runtime_present\":{},"
        "\"runtime_details_present\":{},"
        "\"renderer_details_present\":{},"
        "\"platform_name\":{},"
        "\"capabilities\":{{\"material_surfaces\":{},"
        "\"material_backdrop_blur\":{},\"frame_image\":{},"
        "\"write_artifact_bundle\":{}}},"
        "\"material\":{}}}",
        observation.present ? "true" : "false",
        observation.parse_ok ? "true" : "false",
        json_string(observation.parse_error),
        string_array_json(observation.top_level_keys),
        observation.debug_present ? "true" : "false",
        observation.semantic_tree_present ? "true" : "false",
        observation.platform_capabilities_present ? "true" : "false",
        observation.platform_runtime_present ? "true" : "false",
        observation.runtime_details_present ? "true" : "false",
        observation.renderer_details_present ? "true" : "false",
        json_string(observation.platform_name),
        observation.material_surfaces_capability ? "true" : "false",
        observation.material_backdrop_blur_capability ? "true" : "false",
        observation.frame_image_capability ? "true" : "false",
        observation.write_artifact_bundle_capability ? "true" : "false",
        material_observation_json(observation.material));
}

auto verifier_observation_json(VerifierObservation const& verifier)
    -> std::string {
    return std::format(
        "{{\"requested\":{},\"executed\":{},\"ok\":{},"
        "\"exit_code\":{},\"timed_out\":{},\"stdout_tail\":{},"
        "\"stderr_tail\":{},\"report_error\":{},\"report\":{}}}",
        verifier.requested ? "true" : "false",
        verifier.executed ? "true" : "false",
        verifier.ok ? "true" : "false",
        verifier.exit_code,
        verifier.timed_out ? "true" : "false",
        json_string(verifier.stdout_tail),
        json_string(verifier.stderr_tail),
        json_string(verifier.report_error),
        json_report_or_null(verifier.report));
}

auto artifact_observation_json(ArtifactObservation const& observation)
    -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"observe artifact\","
        "\"ok\":{},\"bundle\":{},\"artifact\":{},"
        "\"snapshot\":{},\"verifier\":{},\"likely_owner_layers\":{},"
        "\"suggested_actions\":{},\"checks\":{}}}",
        observation.ok ? "true" : "false",
        json_string(path_string(observation.bundle)),
        artifact_json(observation.artifact, observation.checks),
        snapshot_observation_json(observation.snapshot),
        verifier_observation_json(observation.verifier),
        string_array_json(observation.likely_owner_layers),
        string_array_json(observation.suggested_actions),
        checks_json(observation.checks));
}


auto script_result_json(std::string_view command,
                        fs::path const& script,
                        cppx::process::CapturedProcessResult const& result)
    -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":{},\"ok\":{},"
        "\"script\":{},\"exit_code\":{},\"timed_out\":{},"
        "\"stdout_line_count\":{},\"stderr_line_count\":{},"
        "\"stdout_tail\":{},\"stderr_tail\":{}}}",
        json_string(command),
        (!result.timed_out && result.exit_code == 0) ? "true" : "false",
        json_string(path_string(script)),
        result.exit_code,
        result.timed_out ? "true" : "false",
        output_line_count(result.stdout_text),
        output_line_count(result.stderr_text),
        json_string(output_tail(result.stdout_text)),
        json_string(output_tail(result.stderr_text)));
}

auto process_result_json(std::string_view command,
                         std::string_view program,
                         std::span<std::string const> args,
                         fs::path const& cwd,
                         cppx::process::CapturedProcessResult const& result)
    -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":{},\"ok\":{},"
        "\"program\":{},\"args\":{},\"cwd\":{},"
        "\"exit_code\":{},\"timed_out\":{},"
        "\"stdout_line_count\":{},\"stderr_line_count\":{},"
        "\"stdout_tail\":{},\"stderr_tail\":{}}}",
        json_string(command),
        (!result.timed_out && result.exit_code == 0) ? "true" : "false",
        json_string(program),
        string_array_json(args),
        json_string(path_string(cwd)),
        result.exit_code,
        result.timed_out ? "true" : "false",
        output_line_count(result.stdout_text),
        output_line_count(result.stderr_text),
        json_string(output_tail(result.stdout_text)),
        json_string(output_tail(result.stderr_text)));
}

auto parse_env_overrides(cppx::cli::Invocation const& invocation)
    -> std::expected<std::map<std::string, std::string>, std::string> {
    auto env = std::map<std::string, std::string>{};
    for (auto const& raw : invocation.values("env")) {
        auto pos = raw.find('=');
        if (pos == std::string::npos) {
            return std::unexpected{
                "--env values must use KEY=VALUE syntax"};
        }
        auto name = raw.substr(0, pos);
        if (!valid_env_name(name)) {
            return std::unexpected{
                std::format("--env key is not a portable environment name: {}", name)};
        }
        env[name] = raw.substr(pos + 1);
    }
    return env;
}

auto resolve_example_root(fs::path const& repo_root,
                          fs::path const& current_path,
                          std::string_view raw)
    -> std::expected<fs::path, std::string> {
    if (raw.empty())
        return std::unexpected{"run requires one example name or path"};

    auto input = fs::path{std::string{raw}};
    auto candidate = fs::path{};
    if (input.is_absolute()) {
        candidate = input;
    } else if (input.has_parent_path() || raw == ".") {
        candidate = current_path / input;
    } else {
        candidate = repo_root / "examples" / input;
    }

    auto ec = std::error_code{};
    candidate = fs::weakly_canonical(candidate, ec);
    if (ec)
        return std::unexpected{std::format("could not resolve example path: {}", ec.message())};

    auto root_error = std::string{};
    if (!path_stays_under_root(repo_root, candidate, root_error)) {
        return std::unexpected{
            std::format("example path must stay under the repository root: {}", root_error)};
    }
    if (!path_is_directory(candidate)) {
        return std::unexpected{
            std::format("example path is not a directory: {}", path_string(candidate))};
    }
    if (!path_exists(candidate / "exon.toml")) {
        return std::unexpected{
            std::format("example path does not contain exon.toml: {}", path_string(candidate))};
    }
    return candidate;
}

auto example_run_json(ExampleRunSummary const& summary) -> std::string {
    auto artifact = std::string{"null"};
    if (summary.artifact) {
        artifact = std::format(
            "{{\"requested\":{},\"bundle\":{},\"snapshot_json\":{{\"present\":{},"
            "\"bytes\":{}}},\"frame_bmp\":{{\"present\":{},\"bytes\":{}}},"
            "\"platform\":{{\"present\":{},\"file_count\":{}}}}}",
            summary.artifact_requested ? "true" : "false",
            json_string(path_string(summary.artifact->bundle)),
            summary.artifact->snapshot_json ? "true" : "false",
            summary.artifact->snapshot_bytes,
            summary.artifact->frame_bmp ? "true" : "false",
            summary.artifact->frame_bytes,
            summary.artifact->platform_directory ? "true" : "false",
            summary.artifact->platform_file_count);
    } else if (summary.artifact_requested) {
        artifact = std::format(
            "{{\"requested\":true,\"bundle\":{}}}",
            json_string(path_string(summary.artifact_dir)));
    }

    auto timeout_seconds = std::string{"null"};
    if (summary.run_timeout) {
        timeout_seconds = std::format(
            "{}",
            std::chrono::duration_cast<std::chrono::seconds>(
                *summary.run_timeout).count());
    }
    auto marker_value = summary.file_explorer_artifact_chrome_markers.empty()
        ? std::string{"null"}
        : json_string(summary.file_explorer_artifact_chrome_markers);
    auto file_explorer_input = std::format(
        "{{\"direct_count\":{},\"script_count\":{},\"script\":{},"
        "\"artifact_chrome_markers\":{},"
        "\"artifact_chrome_markers_injected\":{}}}",
        summary.file_explorer_input_count,
        summary.file_explorer_script_input_count,
        summary.file_explorer_script.empty()
            ? std::string{"null"}
            : json_string(path_string(summary.file_explorer_script)),
        marker_value,
        summary.file_explorer_artifact_chrome_markers_injected
            ? "true"
            : "false");
    auto output_observation = summary.output_observation
        ? artifact_observation_json(*summary.output_observation)
        : std::string{"null"};

    return std::format(
        "{{\"schema_version\":1,\"command\":\"run\",\"ok\":{},"
        "\"example\":{},\"example_root\":{},\"package_name\":{},"
        "\"executable\":{},\"build_requested\":{},\"run_requested\":{},"
        "\"artifact_exit\":{},\"output_observation_requested\":{},"
        "\"file_explorer_input\":{},\"timeout_seconds\":{},"
        "\"build\":{},\"run_result\":{},\"artifact\":{},"
        "\"output_observation\":{},\"error\":{}}}",
        summary.ok ? "true" : "false",
        json_string(summary.example),
        json_string(path_string(summary.example_root)),
        json_string(summary.package_name),
        json_string(path_string(summary.executable)),
        summary.build_requested ? "true" : "false",
        summary.run_requested ? "true" : "false",
        summary.artifact_exit ? "true" : "false",
        summary.output_observation_requested ? "true" : "false",
        file_explorer_input,
        timeout_seconds,
        process_result_detail_json(summary.build_result),
        process_result_detail_json(summary.run_result),
        artifact,
        output_observation,
        json_string(summary.error));
}

void print_captured_output(cppx::process::CapturedProcessResult const& result) {
    if (!result.stdout_text.empty()) {
        std::print("{}", result.stdout_text);
        if (!result.stdout_text.ends_with('\n'))
            std::println("");
    }
    if (!result.stderr_text.empty()) {
        std::print(std::cerr, "{}", result.stderr_text);
        if (!result.stderr_text.ends_with('\n'))
            std::println(std::cerr, "");
    }
}

void print_example_run(ExampleRunSummary const& summary) {
    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "example",
         .value = summary.example,
         .status = summary.error.empty() ? cppx::terminal::StatusKind::ok
                                         : cppx::terminal::StatusKind::fail},
        {.label = "root",
         .value = path_string(summary.example_root),
         .status = path_is_directory(summary.example_root)
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "executable",
         .value = path_string(summary.executable),
         .status = path_exists(summary.executable)
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
    };
    if (summary.build_requested) {
        auto status = cppx::terminal::StatusKind::skip;
        auto value = std::string{"not run"};
        if (summary.build_result) {
            status = (!summary.build_result->timed_out
                      && summary.build_result->exit_code == 0)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail;
            value = summary.build_result->timed_out
                ? "timed out"
                : std::format("exit {}", summary.build_result->exit_code);
        }
        lines.push_back({.label = "build", .value = value, .status = status});
    }
    if (summary.file_explorer_input_count > 0
        || summary.file_explorer_script_input_count > 0
        || !summary.file_explorer_script.empty()) {
        auto value = std::format(
            "direct={} script_inputs={} script={}",
            summary.file_explorer_input_count,
            summary.file_explorer_script_input_count,
            summary.file_explorer_script.empty()
                ? std::string{"<none>"}
                : path_string(summary.file_explorer_script));
        lines.push_back({
            .label = "input",
            .value = std::move(value),
            .status = cppx::terminal::StatusKind::ok,
        });
    }
    if (summary.run_requested) {
        auto status = cppx::terminal::StatusKind::skip;
        auto value = std::string{"not run"};
        if (summary.run_result) {
            status = (!summary.run_result->timed_out
                      && summary.run_result->exit_code == 0)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail;
            value = summary.run_result->timed_out
                ? "timed out"
                : std::format("exit {}", summary.run_result->exit_code);
        }
        lines.push_back({.label = "run", .value = value, .status = status});
    }
    if (summary.artifact_requested) {
        auto status = cppx::terminal::StatusKind::skip;
        auto value = path_string(summary.artifact_dir);
        if (summary.artifact) {
            status = summary.artifact->snapshot_json
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail;
            value = std::format(
                "{} snapshot={} frame={} platform_files={}",
                path_string(summary.artifact->bundle),
                summary.artifact->snapshot_json ? "true" : "false",
                summary.artifact->frame_bmp ? "true" : "false",
                summary.artifact->platform_file_count);
        }
        lines.push_back({.label = "artifact", .value = value, .status = status});
    }
    if (summary.output_observation_requested) {
        auto status = cppx::terminal::StatusKind::skip;
        auto value = std::string{"not run"};
        if (summary.output_observation) {
            status = summary.output_observation->ok
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail;
            value = std::format(
                "{} snapshot={} material_plans={}",
                path_string(summary.output_observation->bundle),
                summary.output_observation->snapshot.parse_ok
                    ? "parsed"
                    : "missing",
                summary.output_observation->snapshot.material.plan_count);
        }
        lines.push_back({
            .label = "output",
            .value = std::move(value),
            .status = status,
        });
    }
    std::println("phenotype run");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    if (!summary.error.empty()) {
        std::println("{}",
                     cppx::terminal::format_diagnostic({
                         .severity = cppx::terminal::DiagnosticSeverity::error,
                         .message = summary.error,
                         .context = "run",
                     }));
    }
}

int run_repo_process(
        std::string_view command,
        std::string program,
        std::vector<std::string> args,
        cppx::cli::Invocation const& invocation,
        std::map<std::string, std::string> env_overrides,
        std::chrono::milliseconds timeout) {
    auto root = find_repo_root(fs::current_path());
    if (!root) {
        return print_error(
            command,
            "could not find phenotype repository root from current directory",
            invocation.has("json"));
    }

    auto spec = cppx::process::ProcessSpec{
        .program = program,
        .args = args,
        .cwd = *root,
        .timeout = timeout,
        .env_overrides = std::move(env_overrides),
    };

    auto result = cppx::process::system::capture(spec);
    if (!result) {
        return print_error(
            command,
            std::format("failed to run process: {}",
                        cppx::process::to_string(result.error())),
            invocation.has("json"));
    }

    if (invocation.has("json")) {
        std::println(
            "{}",
            process_result_json(command, program, args, *root, *result));
    } else {
        if (!result->stdout_text.empty()) {
            std::print("{}", result->stdout_text);
            if (!result->stdout_text.ends_with('\n'))
                std::println("");
        }
        if (!result->stderr_text.empty()) {
            std::print(std::cerr, "{}", result->stderr_text);
            if (!result->stderr_text.ends_with('\n'))
                std::println(std::cerr, "");
        }
    }

    if (result->timed_out)
        return result->exit_code == 0 ? 124 : result->exit_code;
    return result->exit_code;
}

int run_repo_script(
        std::string_view command,
        fs::path script,
        cppx::cli::Invocation const& invocation,
        std::map<std::string, std::string> env_overrides,
        std::chrono::milliseconds timeout) {
    auto root = find_repo_root(fs::current_path());
    if (!root) {
        return print_error(
            command,
            "could not find phenotype repository root from current directory",
            invocation.has("json"));
    }

    script = *root / script;
    auto spec = cppx::process::ProcessSpec{
        .program = "bash",
        .args = {path_string(script)},
        .cwd = *root,
        .timeout = timeout,
        .env_overrides = std::move(env_overrides),
    };

    auto result = cppx::process::system::capture(spec);
    if (!result) {
        return print_error(
            command,
            std::format("failed to run script: {}",
                        cppx::process::to_string(result.error())),
            invocation.has("json"));
    }

    if (invocation.has("json")) {
        std::println("{}", script_result_json(command, script, *result));
    } else {
        if (!result->stdout_text.empty()) {
            std::print("{}", result->stdout_text);
            if (!result->stdout_text.ends_with('\n'))
                std::println("");
        }
        if (!result->stderr_text.empty()) {
            std::print(std::cerr, "{}", result->stderr_text);
            if (!result->stderr_text.ends_with('\n'))
                std::println(std::cerr, "");
        }
    }

    if (result->timed_out)
        return result->exit_code == 0 ? 124 : result->exit_code;
    return result->exit_code;
}

auto find_command(cppx::cli::CommandSpec const& root,
                  std::span<std::string const> path)
    -> cppx::cli::CommandSpec const* {
    auto const* command = &root;
    for (std::size_t i = 1; i < path.size(); ++i) {
        auto found = std::ranges::find_if(
            command->subcommands,
            [&](cppx::cli::CommandSpec const& candidate) {
                return candidate.name == path[i];
            });
        if (found == command->subcommands.end())
            return command;
        command = &*found;
    }
    return command;
}

auto wants_json(std::span<std::string_view const> args) -> bool {
    return std::ranges::any_of(args, [](std::string_view arg) {
        return arg == "--json";
    });
}

auto first_positional_or_error(cppx::cli::Invocation const& invocation,
                               std::string_view command_name)
    -> std::expected<fs::path, std::string> {
    if (invocation.positionals.empty()) {
        return std::unexpected{
            std::format("{} requires one positional path", command_name)};
    }
    if (invocation.positionals.size() > 1) {
        return std::unexpected{
            std::format("{} accepts exactly one positional path", command_name)};
    }
    return fs::path{invocation.positionals.front()};
}

auto optional_positional_or_error(cppx::cli::Invocation const& invocation,
                                  std::string_view command_name,
                                  fs::path fallback)
    -> std::expected<fs::path, std::string> {
    if (invocation.positionals.empty())
        return fallback;
    if (invocation.positionals.size() > 1) {
        return std::unexpected{
            std::format("{} accepts at most one positional path", command_name)};
    }
    return fs::path{invocation.positionals.front()};
}

int run_doctor(cppx::cli::Invocation const& invocation) {
    auto checks = doctor_checks();
    auto root = find_repo_root(fs::current_path());
    auto migrations = root ? tool_migrations(*root) : std::vector<ToolMigration>{};
    if (invocation.has("json")) {
        std::println(
            "{{\"schema_version\":1,\"command\":\"doctor\",\"ok\":{},"
            "\"checks\":{},\"tool_migration\":{{\"summary\":{},"
            "\"entries\":{}}}}}",
            all_ok(checks) ? "true" : "false",
            checks_json(checks),
            tool_migration_summary_json(migrations),
            tool_migrations_json(migrations));
    } else {
        print_checks("phenotype doctor", checks);
        if (!migrations.empty()) {
            auto lines = std::vector<cppx::terminal::StatusLine>{};
            lines.reserve(migrations.size());
            for (auto const& migration : migrations) {
                lines.push_back({
                    .label = migration.legacy_path,
                    .value = migration.replacement_command + " ("
                        + migration.status + ")",
                    .status = migration.legacy_present
                        ? cppx::terminal::StatusKind::ok
                        : cppx::terminal::StatusKind::skip,
                });
            }
            std::println("tool migration");
            std::println("{}",
                         cppx::terminal::format_status_frame(lines, false));
        }
    }
    return all_ok(checks) ? 0 : 1;
}


int run_artifact_summary(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "artifact summary");
    if (!path)
        return print_error("artifact summary", path.error(), invocation.has("json"));

    auto summary = artifact_summary(*path);
    auto checks = artifact_checks(summary);
    if (invocation.has("json")) {
        std::println("{}", artifact_json(summary, checks));
    } else {
        print_checks("phenotype artifact summary", checks);
    }
    return all_ok(checks) ? 0 : 1;
}

void append_option_arg(std::vector<std::string>& args,
                       cppx::cli::Invocation const& invocation,
                       std::string_view name) {
    if (auto value = invocation.value(name)) {
        auto option = std::string{"--"};
        option += name;
        args.push_back(std::move(option));
        args.push_back(std::string{*value});
    }
}

void append_path_option_arg(std::vector<std::string>& args,
                            cppx::cli::Invocation const& invocation,
                            std::string_view name) {
    if (auto value = invocation.value(name)) {
        auto option = std::string{"--"};
        option += name;
        args.push_back(std::move(option));
        args.push_back(absolute_path_string(fs::path{std::string{*value}}));
    }
}

void append_flag_arg(std::vector<std::string>& args,
                     cppx::cli::Invocation const& invocation,
                     std::string_view name) {
    if (!invocation.has(name))
        return;
    auto option = std::string{"--"};
    option += name;
    args.push_back(std::move(option));
}

void append_repeatable_arg(std::vector<std::string>& args,
                           cppx::cli::Invocation const& invocation,
                           std::string_view name) {
    for (auto const& value : invocation.values(name)) {
        auto option = std::string{"--"};
        option += name;
        args.push_back(std::move(option));
        args.push_back(value);
    }
}

auto comma_join(std::span<std::string const> values) -> std::string {
    auto result = std::string{};
    for (auto const& value : values) {
        if (!result.empty())
            result += ",";
        result += value;
    }
    return result;
}

auto artifact_verify_args(fs::path const& bundle,
                          cppx::cli::Invocation const& invocation)
    -> std::vector<std::string> {
    auto args = std::vector<std::string>{
        "exec",
        "--",
        "uv",
        "run",
        "--frozen",
        "python",
        "tools/verify_artifact_bundle.py",
        absolute_path_string(bundle),
    };

    append_path_option_arg(args, invocation, "manifest");
    append_option_arg(args, invocation, "expect-platform");
    append_flag_arg(args, invocation, "require-frame");
    append_repeatable_arg(args, invocation, "require-label");
    append_repeatable_arg(args, invocation, "require-role");
    append_repeatable_arg(args, invocation, "require-material-kind");
    append_repeatable_arg(args, invocation, "require-material-surface-role");
    append_repeatable_arg(args, invocation, "require-capability");
    append_repeatable_arg(args, invocation, "require-runtime-detail");
    append_repeatable_arg(args, invocation, "require-pixel-region");
    append_repeatable_arg(args, invocation, "require-pixel-region-metric");
    return args;
}

auto last_nonempty_line(std::string_view text) -> std::string {
    auto end = text.size();
    while (end > 0 && (text[end - 1] == '\n' || text[end - 1] == '\r'))
        --end;
    auto begin = text.rfind('\n', end == 0 ? 0 : end - 1);
    if (begin == std::string_view::npos)
        return std::string{text.substr(0, end)};
    return std::string{text.substr(begin + 1, end - begin - 1)};
}

auto capture_artifact_verifier(fs::path const& bundle,
                               cppx::cli::Invocation const& invocation)
    -> std::expected<VerifierObservation, std::string> {
    auto root = find_repo_root(fs::current_path());
    if (!root) {
        return std::unexpected{
            "could not find phenotype repository root from current directory"};
    }

    auto spec = cppx::process::ProcessSpec{
        .program = "mise",
        .args = artifact_verify_args(bundle, invocation),
        .cwd = *root,
        .timeout = std::chrono::minutes{5},
        .env_overrides = {
            {"UV_PROJECT_ENVIRONMENT", uv_project_environment()},
        },
    };

    auto result = cppx::process::system::capture(spec);
    if (!result) {
        return std::unexpected{std::format(
            "failed to run mise/uv verifier: {}",
            cppx::process::to_string(result.error()))};
    }

    auto verifier = VerifierObservation{
        .requested = true,
        .executed = true,
        .ok = !result->timed_out && result->exit_code == 0,
        .exit_code = result->exit_code,
        .timed_out = result->timed_out,
        .stdout_tail = std::string{output_tail(result->stdout_text)},
        .stderr_tail = std::string{output_tail(result->stderr_text)},
    };

    if (!result->stdout_text.empty()) {
        try {
            verifier.report = json::parse(result->stdout_text);
        } catch (std::exception const& error) {
            verifier.report_error = error.what();
        }
    }
    if (!verifier.report && !result->stdout_text.empty()) {
        auto line = last_nonempty_line(result->stdout_text);
        if (!line.empty() && line.front() == '{') {
            try {
                verifier.report = json::parse(line);
                verifier.report_error.clear();
            } catch (std::exception const&) {
            }
        }
    }
    if (!verifier.report && !result->stdout_text.empty()
        && verifier.report_error.empty()) {
        verifier.report_error = "verifier stdout did not contain a JSON object";
    }
    return verifier;
}

int run_artifact_verify(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "artifact verify");
    if (!path)
        return print_error("artifact verify", path.error(), invocation.has("json"));

    auto root = find_repo_root(fs::current_path());
    if (!root) {
        return print_error(
            "artifact verify",
            "could not find phenotype repository root from current directory",
            invocation.has("json"));
    }

    auto spec = cppx::process::ProcessSpec{
        .program = "mise",
        .args = artifact_verify_args(*path, invocation),
        .cwd = *root,
        .timeout = std::chrono::minutes{5},
        .env_overrides = {
            {"UV_PROJECT_ENVIRONMENT", uv_project_environment()},
        },
    };

    auto result = cppx::process::system::capture(spec);
    if (!result) {
        return print_error(
            "artifact verify",
            std::format("failed to run mise/uv verifier: {}",
                        cppx::process::to_string(result.error())),
            invocation.has("json"));
    }

    if (!result->stdout_text.empty()) {
        std::print("{}", result->stdout_text);
        if (!result->stdout_text.ends_with('\n'))
            std::println("");
    }
    if (!result->stderr_text.empty()) {
        std::print(std::cerr, "{}", result->stderr_text);
        if (!result->stderr_text.ends_with('\n'))
            std::println(std::cerr, "");
    }

    if (result->timed_out)
        return result->exit_code == 0 ? 124 : result->exit_code;
    return result->exit_code;
}

void append_artifact_observation_guidance(ArtifactObservation& observation) {
    if (!observation.artifact.exists || !observation.artifact.is_directory) {
        append_unique(
            observation.likely_owner_layers,
            "artifact-bundle");
        append_unique(
            observation.suggested_actions,
            "Pass the artifact bundle directory produced by a phenotype runtime capture.");
        return;
    }
    if (!observation.artifact.snapshot_json) {
        append_unique(observation.likely_owner_layers, "snapshot-writer");
        append_unique(
            observation.suggested_actions,
            "Run the example with PHENOTYPE_ARTIFACT_DIR and PHENOTYPE_ARTIFACT_EXIT=1, then inspect snapshot.json.");
    }
    if (observation.artifact.frame_bmp) {
        // Structural frame presence is enough here; pixel metrics remain in the verifier.
    } else {
        append_unique(observation.likely_owner_layers, "frame-capture");
        append_unique(
            observation.suggested_actions,
            "Inspect frame capture support and the backend artifact writer for missing frame.bmp.");
    }
    if (!observation.artifact.platform_directory) {
        append_unique(observation.likely_owner_layers, "platform-runtime");
        append_unique(
            observation.suggested_actions,
            "Inspect platform runtime detail writers; LLM-debuggable artifacts need platform/*.json files.");
    }
    if (observation.snapshot.present && !observation.snapshot.parse_ok) {
        append_unique(observation.likely_owner_layers, "snapshot-json");
        append_unique(
            observation.suggested_actions,
            "Inspect snapshot.json for truncation or malformed JSON before debugging renderer policy.");
    }
    if (observation.snapshot.parse_ok
        && !observation.snapshot.semantic_tree_present) {
        append_unique(observation.likely_owner_layers, "semantic-tree");
        append_unique(
            observation.suggested_actions,
            "Inspect debug.semantic_tree serialization; material failures need semantic node context.");
    }
    if (observation.snapshot.parse_ok
        && observation.snapshot.material.plan_count == 0) {
        append_unique(observation.likely_owner_layers, "material-plan");
        append_unique(
            observation.suggested_actions,
            "Inspect debug.platform_runtime.details.renderer.material_plans and layout material_surface calls.");
    }
    for (auto const& layer : observation.snapshot.material.likely_layers)
        append_unique(observation.likely_owner_layers, layer);
    if (observation.verifier.executed && !observation.verifier.ok) {
        append_unique(observation.likely_owner_layers, "artifact-verifier");
        append_unique(
            observation.suggested_actions,
            "Read verifier.report.failure_summary first, then follow the reported JSON path and likely pass.");
    }
}

auto observe_artifact(fs::path bundle,
                      cppx::cli::Invocation const& invocation)
    -> ArtifactObservation {
    auto observation = ArtifactObservation{
        .bundle = std::move(bundle),
    };
    observation.artifact = artifact_summary(observation.bundle);
    observation.checks = artifact_checks(observation.artifact);
    if (!invocation.has("require-frame")) {
        for (auto& check : observation.checks) {
            if (check.name == "frame_bmp") {
                check.ok = observation.artifact.frame_bmp
                    ? observation.artifact.frame_bytes > 0
                    : true;
                check.hint.clear();
            }
        }
    }
    observation.snapshot = observe_snapshot(observation.bundle);

    auto verifier_requested =
        invocation.has("verify") || invocation.value("manifest").has_value();
    observation.verifier.requested = verifier_requested;
    if (verifier_requested) {
        if (auto verifier = capture_artifact_verifier(
                observation.bundle,
                invocation)) {
            observation.verifier = std::move(*verifier);
        } else {
            observation.verifier = VerifierObservation{
                .requested = true,
                .executed = false,
                .ok = false,
                .report_error = verifier.error(),
            };
        }
    }

    append_artifact_observation_guidance(observation);
    observation.ok = all_ok(observation.checks)
        && observation.snapshot.present
        && observation.snapshot.parse_ok
        && (!verifier_requested || observation.verifier.ok);
    return observation;
}

void print_artifact_observation(ArtifactObservation const& observation) {
    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "bundle",
         .value = path_string(observation.bundle),
         .status = observation.artifact.exists && observation.artifact.is_directory
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "snapshot",
         .value = observation.snapshot.parse_ok
            ? "parsed"
            : (observation.snapshot.present ? "parse failed" : "missing"),
         .status = observation.snapshot.parse_ok
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "material plans",
         .value = std::format(
             "{} plans, {} fallback, {} backdrop",
             observation.snapshot.material.plan_count,
             observation.snapshot.material.fallback_count,
             observation.snapshot.material.backdrop_sampling_count),
         .status = observation.snapshot.material.plan_count > 0
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::skip},
        {.label = "platform",
         .value = observation.snapshot.platform_name.empty()
            ? "<unknown>"
            : observation.snapshot.platform_name,
         .status = observation.snapshot.platform_runtime_present
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::skip},
        {.label = "frame",
         .value = observation.artifact.frame_bmp
            ? std::format("{} bytes", observation.artifact.frame_bytes)
            : "not present",
         .status = observation.artifact.frame_bmp
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::skip},
        {.label = "verifier",
         .value = observation.verifier.requested
            ? (observation.verifier.executed
                ? std::format("exit {}", observation.verifier.exit_code)
                : observation.verifier.report_error)
            : "not requested",
         .status = !observation.verifier.requested
            ? cppx::terminal::StatusKind::skip
            : (observation.verifier.ok ? cppx::terminal::StatusKind::ok
                                       : cppx::terminal::StatusKind::fail)},
    };
    std::println("phenotype observe artifact");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    if (!observation.suggested_actions.empty()) {
        std::println("suggestions:");
        for (auto const& suggestion : observation.suggested_actions)
            std::println("  - {}", suggestion);
    }
}

int run_observe(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "observe");
    if (!path)
        return print_error("observe", path.error(), invocation.has("json"));

    auto observation = observe_artifact(*path, invocation);
    if (invocation.has("json")) {
        std::println("{}", artifact_observation_json(observation));
    } else {
        print_artifact_observation(observation);
    }
    return observation.ok ? 0 : 1;
}

auto android_env_overrides(cppx::cli::Invocation const& invocation)
    -> std::map<std::string, std::string> {
    auto env = std::map<std::string, std::string>{};
    if (auto value = invocation.value("serial")) {
        env["ANDROID_SERIAL"] = std::string{*value};
    }
    if (auto value = invocation.value("avd")) {
        env["PHENOTYPE_ANDROID_AVD"] = std::string{*value};
    }
    if (auto value = invocation.value("state-dir")) {
        env["PHENOTYPE_ANDROID_STATE_DIR"] =
            absolute_path_string(fs::path{std::string{*value}});
    }
    if (auto value = invocation.value("apk")) {
        env["PHENOTYPE_ANDROID_APK"] =
            absolute_path_string(fs::path{std::string{*value}});
    }
    if (auto value = invocation.value("contract-out")) {
        env["PHENOTYPE_ANDROID_CONTRACT_OUT"] =
            absolute_path_string(fs::path{std::string{*value}});
    }
    if (auto value = invocation.value("contract-timeout")) {
        env["PHENOTYPE_ANDROID_CONTRACT_TIMEOUT"] = std::string{*value};
    }
    return env;
}

int run_android_script(cppx::cli::Invocation const& invocation,
                       std::string_view command,
                       fs::path script,
                       std::chrono::milliseconds timeout,
                       std::map<std::string, std::string> env = {}) {
    auto base = android_env_overrides(invocation);
    base.insert(env.begin(), env.end());
    return run_repo_script(
        command,
        std::move(script),
        invocation,
        std::move(base),
        timeout);
}

int run_android_build(cppx::cli::Invocation const& invocation) {
    return run_repo_process(
        "android build",
        "mise",
        {"exec", "--", "exon", "build", "--target", "aarch64-linux-android"},
        invocation,
        android_env_overrides(invocation),
        std::chrono::minutes{30});
}

int run_android_logs(cppx::cli::Invocation const& invocation) {
    auto env = std::map<std::string, std::string>{
        {"PHENOTYPE_ANDROID_LOG_DUMP", "1"},
        {"PHENOTYPE_ANDROID_LOG_LINES", "160"},
    };
    if (auto value = invocation.value("lines")) {
        env["PHENOTYPE_ANDROID_LOG_LINES"] = std::string{*value};
    }
    return run_android_script(
        invocation,
        "android logs",
        "tools/android/logs.sh",
        std::chrono::minutes{2},
        std::move(env));
}

int run_android_command(cppx::cli::Invocation const& invocation) {
    auto const& path = invocation.command_path;
    auto const command = [&] {
        return path.size() >= 3 ? path[2] : std::string{};
    }();

    if (command == "doctor") {
        return run_android_script(
            invocation,
            "android doctor",
            "tools/android/doctor.sh",
            std::chrono::minutes{2});
    }
    if (command == "devices") {
        return run_android_script(
            invocation,
            "android devices",
            "tools/android/emu_list.sh",
            std::chrono::minutes{2});
    }
    if (command == "emu-start") {
        return run_android_script(
            invocation,
            "android emu-start",
            "tools/android/emu_start.sh",
            std::chrono::minutes{5});
    }
    if (command == "emu-stop") {
        return run_android_script(
            invocation,
            "android emu-stop",
            "tools/android/emu_stop.sh",
            std::chrono::minutes{2});
    }
    if (command == "build")
        return run_android_build(invocation);
    if (command == "apk") {
        return run_android_script(
            invocation,
            "android apk",
            "tools/android/apk.sh",
            std::chrono::minutes{45});
    }
    if (command == "install") {
        return run_android_script(
            invocation,
            "android install",
            "tools/android/install.sh",
            std::chrono::minutes{5});
    }
    if (command == "launch") {
        return run_android_script(
            invocation,
            "android launch",
            "tools/android/launch.sh",
            std::chrono::minutes{2});
    }
    if (command == "stop") {
        return run_android_script(
            invocation,
            "android stop",
            "tools/android/stop.sh",
            std::chrono::minutes{2});
    }
    if (command == "run") {
        return run_android_script(
            invocation,
            "android run",
            "tools/android/run.sh",
            std::chrono::minutes{60});
    }
    if (command == "logs")
        return run_android_logs(invocation);
    if (command == "screencap") {
        return run_android_script(
            invocation,
            "android screencap",
            "tools/android/screencap.sh",
            std::chrono::minutes{2});
    }
    if (command == "contract") {
        return run_android_script(
            invocation,
            "android contract",
            "tools/android/contract.sh",
            std::chrono::minutes{75});
    }
    if (command == "clean") {
        return run_android_script(
            invocation,
            "android clean",
            "tools/android/clean.sh",
            std::chrono::minutes{20});
    }

    return print_error(
        "android",
        "unknown Android command",
        invocation.has("json"));
}


int run_example(cppx::cli::Invocation const& invocation) {
    auto root = find_repo_root(fs::current_path());
    if (!root) {
        return print_error(
            "run",
            "could not find phenotype repository root from current directory",
            invocation.has("json"));
    }
    if (invocation.positionals.empty()) {
        return print_error(
            "run",
            "run requires one example name or path",
            invocation.has("json"));
    }
    if (invocation.positionals.size() > 1) {
        return print_error(
            "run",
            "run accepts exactly one example name or path",
            invocation.has("json"));
    }

    auto summary = ExampleRunSummary{
        .example = invocation.positionals.front(),
        .build_requested = !invocation.has("no-build"),
        .artifact_exit =
            invocation.has("artifact-exit") || invocation.has("observe-output"),
        .output_observation_requested = invocation.has("observe-output"),
    };

    auto example_root = resolve_example_root(
        *root,
        fs::current_path(),
        invocation.positionals.front());
    if (!example_root) {
        return print_error("run", example_root.error(), invocation.has("json"));
    }
    summary.example_root = *example_root;

    auto package_name = exon_package_name(summary.example_root);
    if (!package_name) {
        return print_error("run", package_name.error(), invocation.has("json"));
    }
    summary.package_name = *package_name;
    summary.executable =
        summary.example_root / ".exon" / "debug" / executable_filename(*package_name);

    auto env = parse_env_overrides(invocation);
    if (!env) {
        return print_error("run", env.error(), invocation.has("json"));
    }
    if (path_exists(summary.example_root / "phenotype.package.toml")) {
        auto package_root = path_string(summary.example_root);
        if (!env->contains("PHENOTYPE_PACKAGE_ROOT"))
            (*env)["PHENOTYPE_PACKAGE_ROOT"] = package_root;
        if (summary.package_name.starts_with("file_explorer_")
            && !env->contains("PHENOTYPE_FILE_EXPLORER_PACKAGE_ROOT")) {
            (*env)["PHENOTYPE_FILE_EXPLORER_PACKAGE_ROOT"] = package_root;
        }
    }
    auto const is_file_explorer_example =
        summary.package_name.starts_with("file_explorer_");
    auto direct_inputs = invocation.values("input");
    auto script_input = invocation.value("script");
    if ((!direct_inputs.empty() || script_input) && !is_file_explorer_example) {
        return print_error(
            "run",
            "--input and --script are only supported for file_explorer examples",
            invocation.has("json"));
    }
    if (!direct_inputs.empty()) {
        if (env->contains("PHENOTYPE_FILE_EXPLORER_INPUTS")) {
            return print_error(
                "run",
                "--input cannot be combined with PHENOTYPE_FILE_EXPLORER_INPUTS",
                invocation.has("json"));
        }
        for (auto const& raw : direct_inputs) {
            auto parsed = file_explorer_demo::parse_explorer_input(raw);
            if (!parsed.ok) {
                return print_error(
                    "run",
                    parsed.error,
                    invocation.has("json"));
            }
        }
        summary.file_explorer_input_count = direct_inputs.size();
        (*env)["PHENOTYPE_FILE_EXPLORER_INPUTS"] =
            join_explorer_input_lines(direct_inputs);
    }
    if (script_input) {
        if (env->contains("PHENOTYPE_FILE_EXPLORER_SCRIPT")) {
            return print_error(
                "run",
                "--script cannot be combined with PHENOTYPE_FILE_EXPLORER_SCRIPT",
                invocation.has("json"));
        }
        auto script_path = fs::path{absolute_path_string(
            fs::path{std::string{*script_input}})};
        auto parsed = parse_explorer_input_script(script_path);
        if (!parsed) {
            return print_error(
                "run",
                parsed.error(),
                invocation.has("json"));
        }
        summary.file_explorer_script = script_path;
        summary.file_explorer_script_input_count = parsed->size();
        (*env)["PHENOTYPE_FILE_EXPLORER_SCRIPT"] =
            path_string(summary.file_explorer_script);
    }

    if (auto value = invocation.value("artifact-dir")) {
        summary.artifact_requested = true;
        summary.artifact_dir =
            fs::path{absolute_path_string(fs::path{std::string{*value}})};
        (*env)["PHENOTYPE_ARTIFACT_DIR"] = path_string(summary.artifact_dir);
    }
    if (summary.output_observation_requested && !summary.artifact_requested) {
        auto output_dir = make_temp_directory("phenotype-run-output");
        if (!output_dir) {
            return print_error(
                "run",
                output_dir.error(),
                invocation.has("json"));
        }
        summary.artifact_requested = true;
        summary.artifact_dir = *output_dir;
        (*env)["PHENOTYPE_ARTIFACT_DIR"] = path_string(summary.artifact_dir);
    }
    if (summary.artifact_exit) {
        (*env)["PHENOTYPE_ARTIFACT_EXIT"] = "1";
    }
    if (auto marker = env->find("PHENOTYPE_FILE_EXPLORER_ARTIFACT_CHROME_MARKERS");
        marker != env->end()) {
        summary.file_explorer_artifact_chrome_markers = marker->second;
    }
    if (auto value = invocation.value("artifact-reason")) {
        (*env)["PHENOTYPE_ARTIFACT_REASON"] = std::string{*value};
    } else if (summary.artifact_requested) {
        (*env)["PHENOTYPE_ARTIFACT_REASON"] =
            std::format("phenotype-run-{}", summary.package_name);
    }
    if (auto value = invocation.value("accessibility-display")) {
        (*env)["PHENOTYPE_ACCESSIBILITY_DISPLAY"] = std::string{*value};
    }

    if (auto value = invocation.value("timeout-seconds")) {
        auto parsed_timeout = parse_seconds(*value, "--timeout-seconds");
        if (!parsed_timeout) {
            return print_error(
                "run",
                parsed_timeout.error(),
                invocation.has("json"));
        }
        summary.run_timeout = *parsed_timeout;
    } else if (summary.artifact_exit) {
        summary.run_timeout = std::chrono::seconds{120};
    }

    if (summary.build_requested) {
        auto build = run_example_build(summary.example_root);
        if (!build) {
            summary.error = std::format(
                "failed to run exon build: {}",
                cppx::process::to_string(build.error()));
            if (invocation.has("json")) {
                std::println("{}", example_run_json(summary));
            } else {
                print_example_run(summary);
            }
            return 2;
        }
        summary.build_result = std::move(*build);
        if (summary.build_result->timed_out
            || summary.build_result->exit_code != 0) {
            summary.error = "exon build failed";
            if (invocation.has("json")) {
                std::println("{}", example_run_json(summary));
            } else {
                print_captured_output(*summary.build_result);
                print_example_run(summary);
            }
            return summary.build_result->timed_out
                ? 124
                : summary.build_result->exit_code;
        }
    }

    if (!path_exists(summary.executable)) {
        summary.error = std::format(
            "expected executable was not found: {}",
            path_string(summary.executable));
        if (invocation.has("json")) {
            std::println("{}", example_run_json(summary));
        } else {
            if (summary.build_result)
                print_captured_output(*summary.build_result);
            print_example_run(summary);
        }
        return 1;
    }

    auto run = run_example_binary(
        summary.executable,
        summary.example_root,
        std::move(*env),
        summary.run_timeout);
    if (!run) {
        summary.error = std::format(
            "failed to run example: {}",
            cppx::process::to_string(run.error()));
        if (invocation.has("json")) {
            std::println("{}", example_run_json(summary));
        } else {
            if (summary.build_result)
                print_captured_output(*summary.build_result);
            print_example_run(summary);
        }
        return 2;
    }
    summary.run_result = std::move(*run);
    if (summary.artifact_requested)
        summary.artifact = artifact_summary(summary.artifact_dir);
    if (summary.output_observation_requested && summary.artifact) {
        summary.output_observation =
            observe_artifact(summary.artifact_dir, invocation);
    }
    summary.ok = !summary.run_result->timed_out
        && summary.run_result->exit_code == 0
        && (!summary.artifact_requested
            || (summary.artifact && summary.artifact->snapshot_json))
        && (!summary.output_observation_requested
            || (summary.output_observation && summary.output_observation->ok));
    if (!summary.ok && summary.error.empty()) {
        if (summary.run_result->timed_out) {
            summary.error = "example timed out";
        } else if (summary.run_result->exit_code != 0) {
            summary.error = std::format(
                "example exited with {}",
                summary.run_result->exit_code);
        } else if (summary.artifact_requested
                   && (!summary.artifact
                       || !summary.artifact->snapshot_json)) {
            summary.error = "artifact bundle did not contain snapshot.json";
        } else if (summary.output_observation_requested) {
            summary.error = "output observation failed";
        }
    }

    if (invocation.has("json")) {
        std::println("{}", example_run_json(summary));
    } else {
        if (summary.build_result)
            print_captured_output(*summary.build_result);
        if (summary.run_result)
            print_captured_output(*summary.run_result);
        print_example_run(summary);
    }

    if (summary.run_result->timed_out)
        return summary.run_result->exit_code == 0
            ? 124
            : summary.run_result->exit_code;
    return summary.ok ? 0 : (summary.run_result->exit_code == 0
        ? 1
        : summary.run_result->exit_code);
}

int run_commands(cppx::cli::CommandSpec const& root,
                 cppx::cli::Invocation const& invocation) {
    if (invocation.has("json")) {
        std::println("{}", command_tree_json(root));
    } else {
        std::print("{}", cppx::cli::render_help(root));
    }
    return 0;
}

} // namespace

export namespace phenotype_cli::app {

int run(int argc, char** argv) {
    auto args = std::vector<std::string_view>{};
    for (int i = 1; i < argc; ++i)
        args.push_back(argv[i]);

    auto root = spec();
    if (args.empty()) {
        std::print("{}", cppx::cli::render_help(root));
        return 0;
    }
    if (args.size() == 1 && (args[0] == "--help" || args[0] == "-h")) {
        std::print("{}", cppx::cli::render_help(root));
        return 0;
    }

    auto parsed = cppx::cli::parse(root, args);
    if (!parsed) {
        auto message = parsed.error().message;
        if (parsed.error().suggestion)
            message += std::format("; did you mean '{}'?", *parsed.error().suggestion);
        return print_error("parse", message, wants_json(args));
    }

    if (parsed->has("help")) {
        auto const* command = find_command(root, parsed->command_path);
        std::print("{}", cppx::cli::render_help(*command));
        return 0;
    }

    if (parsed->command_path == std::vector<std::string>{"phenotype", "doctor"})
        return run_doctor(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "artifact", "summary"})
        return run_artifact_summary(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "artifact", "verify"})
        return run_artifact_verify(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "artifact", "verify-glass-showcase"})
        return run_artifact_verify_glass_showcase(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "artifact", "verify-file-explorer"})
        return run_artifact_verify_file_explorer(*parsed);
    if (parsed->command_path == std::vector<std::string>{"phenotype", "observe"})
        return run_observe(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "package", "inspect"})
        return cli_package::run_package_inspect(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "package", "list"})
        return cli_package::run_package_list(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "package", "bundle"})
        return cli_package::run_package_bundle(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "package", "verify-bundle"})
        return cli_package::run_package_verify_bundle(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "icons", "catalog"})
        return run_icons_catalog(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "icons", "lookup"})
        return run_icons_lookup(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "icons", "sources"})
        return run_icons_sources(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "icons", "file-types"})
        return run_icons_file_types(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "icons", "svg"})
        return run_icons_svg(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "icons", "present"})
        return run_icons_present(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "icons", "render"})
        return run_icons_render(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "svg", "inspect"})
        return run_svg_inspect(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "theme", "contract"})
        return run_theme_contract(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "io", "contract"})
        return run_io_contract(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "drive", "file-explorer"})
        return run_drive_file_explorer(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "drive", "glass-showcase"})
        return run_drive_glass_showcase(*parsed);
    if (parsed->command_path == std::vector<std::string>{"phenotype", "run"})
        return run_example(*parsed);
    if (parsed->command_path.size() == 3
        && parsed->command_path[0] == "phenotype"
        && parsed->command_path[1] == "android")
        return run_android_command(*parsed);
    if (parsed->command_path == std::vector<std::string>{"phenotype", "commands"})
        return run_commands(root, *parsed);

    auto const* command = find_command(root, parsed->command_path);
    std::print("{}", cppx::cli::render_help(*command));
    return 0;
}

} // namespace phenotype_cli::app

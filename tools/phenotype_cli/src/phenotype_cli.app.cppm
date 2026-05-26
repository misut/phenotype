export module phenotype_cli.app;

import cppx.cli;
import cppx.process;
import cppx.process.system;
import cppx.terminal;
import file_explorer_shared;
import json;
import phenotype_cli.artifacts;
import phenotype_cli.command_tree;
import phenotype_cli.common;
import phenotype_cli.commands;
import phenotype_cli.contracts;
import phenotype_cli.debug;
import phenotype_cli.doctor;
import phenotype_cli.file_explorer;
import phenotype_cli.file_explorer_gate;
import phenotype_cli.glass_showcase;
import phenotype_cli.icon_audit;
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
using namespace phenotype_cli::artifacts;
using namespace phenotype_cli::common;
using namespace phenotype_cli::command_tree;
using namespace phenotype_cli::commands;
using namespace phenotype_cli::contracts;
using namespace phenotype_cli::debug;
using namespace phenotype_cli::doctor;
using namespace phenotype_cli::file_explorer;
using namespace phenotype_cli::file_explorer_gate;
using namespace phenotype_cli::glass_showcase;
using namespace phenotype_cli::icon_audit;
using namespace phenotype_cli::icon_file_types;
using namespace phenotype_cli::icon_sources;
using namespace phenotype_cli::icons;
using namespace phenotype_cli::runtime;

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
        == std::vector<std::string>{"phenotype", "icons", "check"})
        return run_icons_check(*parsed);
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
        == std::vector<std::string>{"phenotype", "theme", "resolve"})
        return run_theme_resolve(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "io", "contract"})
        return run_io_contract(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "debug", "contract"})
        return run_debug_contract(*parsed);
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

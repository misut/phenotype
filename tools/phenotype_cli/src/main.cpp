import cppx.cli;
import cppx.terminal;
import std;

namespace {

namespace fs = std::filesystem;

struct Check {
    std::string name;
    bool ok = false;
    std::string detail;
    std::string hint;
};

struct ArtifactSummary {
    fs::path bundle;
    bool exists = false;
    bool is_directory = false;
    bool snapshot_json = false;
    bool frame_bmp = false;
    bool platform_directory = false;
    std::uintmax_t snapshot_bytes = 0;
    std::uintmax_t frame_bytes = 0;
    std::size_t platform_file_count = 0;
};

struct PackageSummary {
    fs::path root;
    bool exists = false;
    bool is_directory = false;
    bool manifest = false;
    bool assets_directory = false;
    bool locales_directory = false;
    bool fonts_directory = false;
    std::size_t asset_file_count = 0;
    std::size_t locale_file_count = 0;
    std::size_t font_file_count = 0;
};

auto help_option() -> cppx::cli::OptionSpec {
    return {.name = "help", .short_name = 'h', .description = "Show help"};
}

auto json_option() -> cppx::cli::OptionSpec {
    return {.name = "json",
            .arity = cppx::cli::OptionArity::none,
            .description = "Emit machine-readable JSON"};
}

auto spec() -> cppx::cli::CommandSpec {
    return {
        .name = "phenotype",
        .summary = "phenotype packaging, diagnostics, and artifact tooling",
        .description =
            "Host CLI facade for package resources, diagnostic artifacts, and "
            "future renderer observation commands.",
        .options = {help_option()},
        .subcommands = {
            {
                .name = "doctor",
                .summary = "Check repository-local CLI prerequisites",
                .options = {help_option(), json_option()},
                .allow_positionals = false,
                .category = "diagnostics",
                .examples = {"phenotype doctor --json"},
            },
            {
                .name = "artifact",
                .summary = "Inspect debug artifact bundles",
                .options = {help_option()},
                .subcommands = {
                    {
                        .name = "summary",
                        .summary = "Summarize one artifact bundle",
                        .options = {help_option(), json_option()},
                        .positional_name = "bundle",
                        .positional_description =
                            "Path containing snapshot.json, frame.bmp, and "
                            "platform runtime details.",
                        .examples = {
                            "phenotype artifact summary examples/glass_showcase/artifact",
                            "phenotype artifact summary --json /tmp/phenotype-bundle",
                        },
                    },
                },
                .allow_positionals = false,
                .category = "artifacts",
            },
            {
                .name = "package",
                .summary = "Inspect package resource layouts",
                .options = {help_option()},
                .subcommands = {
                    {
                        .name = "inspect",
                        .summary = "Inspect a package manifest directory",
                        .options = {help_option(), json_option()},
                        .positional_name = "path",
                        .positional_description =
                            "Directory expected to contain phenotype.package.toml.",
                        .examples = {
                            "phenotype package inspect .",
                            "phenotype package inspect --json examples/file_explorer_desktop",
                        },
                    },
                },
                .allow_positionals = false,
                .category = "packaging",
            },
            {
                .name = "commands",
                .summary = "Print CLI command metadata",
                .options = {help_option(), json_option()},
                .allow_positionals = false,
                .category = "metadata",
                .examples = {"phenotype commands --json"},
            },
        },
        .allow_positionals = false,
    };
}

auto json_escape(std::string_view text) -> std::string {
    auto out = std::string{};
    out.reserve(text.size() + 8);
    for (unsigned char ch : text) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20) {
                out += std::format("\\u{:04x}", static_cast<unsigned int>(ch));
            } else {
                out.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return out;
}

auto json_string(std::string_view text) -> std::string {
    return std::format("\"{}\"", json_escape(text));
}

auto path_string(fs::path const& path) -> std::string {
    return path.lexically_normal().generic_string();
}

auto path_exists(fs::path const& path) -> bool {
    auto ec = std::error_code{};
    return fs::exists(path, ec);
}

auto path_is_directory(fs::path const& path) -> bool {
    auto ec = std::error_code{};
    return fs::is_directory(path, ec);
}

auto file_size_or_zero(fs::path const& path) -> std::uintmax_t {
    auto ec = std::error_code{};
    auto size = fs::file_size(path, ec);
    return ec ? 0 : size;
}

auto count_regular_files(fs::path const& root) -> std::size_t {
    if (!path_is_directory(root))
        return 0;

    auto count = std::size_t{0};
    auto ec = std::error_code{};
    auto options = fs::directory_options::skip_permission_denied;
    for (auto it = fs::recursive_directory_iterator(root, options, ec);
         !ec && it != fs::recursive_directory_iterator{};
         it.increment(ec)) {
        if (it->is_regular_file(ec))
            ++count;
    }
    return count;
}

auto find_repo_root(fs::path start) -> std::optional<fs::path> {
    auto ec = std::error_code{};
    start = fs::absolute(start, ec);
    if (ec)
        return std::nullopt;

    for (auto cursor = start.lexically_normal(); !cursor.empty();
         cursor = cursor.parent_path()) {
        if (path_exists(cursor / "exon.toml")
            && path_exists(cursor / "src" / "phenotype.cppm")) {
            return cursor;
        }
        if (cursor == cursor.root_path())
            break;
    }
    return std::nullopt;
}

auto join_path(fs::path const& base, fs::path const& child) -> std::string {
    auto ec = std::error_code{};
    return path_string(fs::relative(base / child, base, ec));
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
                   "Local glass artifact verification still owns visual gates.");
    add_path_check("android_contract_gate", "tools/android/contract.sh",
                   "Android backend contract coverage should remain callable.");
    add_path_check("cli_roadmap", "docs/PHENOTYPE_CLI_ROADMAP.md",
                   "Document CLI migration before replacing shell or Python tools.");
    add_path_check("file_explorer_shared", "examples/file_explorer_shared/exon.toml",
                   "Shared model package should stay available for examples.");

    return checks;
}

auto all_ok(std::span<Check const> checks) -> bool {
    return std::ranges::all_of(checks, [](Check const& check) {
        return check.ok;
    });
}

auto check_json(Check const& check) -> std::string {
    return std::format(
        "{{\"name\":{},\"ok\":{},\"detail\":{},\"hint\":{}}}",
        json_string(check.name),
        check.ok ? "true" : "false",
        json_string(check.detail),
        json_string(check.ok ? "" : check.hint));
}

auto string_array_json(std::span<std::string const> values) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0)
            out += ",";
        out += json_string(values[i]);
    }
    out += "]";
    return out;
}

auto checks_json(std::span<Check const> checks) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < checks.size(); ++i) {
        if (i > 0)
            out += ",";
        out += check_json(checks[i]);
    }
    out += "]";
    return out;
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

void print_checks(std::string_view title, std::span<Check const> checks) {
    std::println("{}", title);
    auto lines = std::vector<cppx::terminal::StatusLine>{};
    lines.reserve(checks.size());
    for (auto const& check : checks) {
        lines.push_back({
            .label = check.name,
            .value = check.detail,
            .status = check.ok ? cppx::terminal::StatusKind::ok
                               : cppx::terminal::StatusKind::fail,
        });
    }
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    for (auto const& check : checks) {
        if (!check.ok && !check.hint.empty()) {
            std::println("{}",
                         cppx::terminal::format_diagnostic({
                             .severity = cppx::terminal::DiagnosticSeverity::warning,
                             .message = check.hint,
                             .context = check.name,
                         }));
        }
    }
}

auto artifact_summary(fs::path bundle) -> ArtifactSummary {
    auto summary = ArtifactSummary{.bundle = std::move(bundle)};
    summary.exists = path_exists(summary.bundle);
    summary.is_directory = path_is_directory(summary.bundle);
    auto snapshot = summary.bundle / "snapshot.json";
    auto frame = summary.bundle / "frame.bmp";
    auto platform = summary.bundle / "platform";
    summary.snapshot_json = path_exists(snapshot);
    summary.frame_bmp = path_exists(frame);
    summary.platform_directory = path_is_directory(platform);
    summary.snapshot_bytes = file_size_or_zero(snapshot);
    summary.frame_bytes = file_size_or_zero(frame);
    summary.platform_file_count = count_regular_files(platform);
    return summary;
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

auto package_summary(fs::path root) -> PackageSummary {
    auto summary = PackageSummary{.root = std::move(root)};
    summary.exists = path_exists(summary.root);
    summary.is_directory = path_is_directory(summary.root);
    summary.manifest = path_exists(summary.root / "phenotype.package.toml");
    summary.assets_directory = path_is_directory(summary.root / "assets");
    summary.locales_directory = path_is_directory(summary.root / "locales");
    summary.fonts_directory = path_is_directory(summary.root / "fonts");
    summary.asset_file_count = count_regular_files(summary.root / "assets");
    summary.locale_file_count = count_regular_files(summary.root / "locales");
    summary.font_file_count = count_regular_files(summary.root / "fonts");
    return summary;
}

auto package_checks(PackageSummary const& summary) -> std::vector<Check> {
    return {
        {.name = "package_root",
         .ok = summary.exists && summary.is_directory,
         .detail = path_string(summary.root),
         .hint = "Pass a directory that owns phenotype package resources."},
        {.name = "manifest",
         .ok = summary.manifest,
         .detail = "phenotype.package.toml",
         .hint = "Add phenotype.package.toml before using CLI packaging."},
        {.name = "assets",
         .ok = summary.assets_directory,
         .detail = std::format("{} files", summary.asset_file_count),
         .hint = "Add assets/ for renderer-visible packaged resources."},
        {.name = "locales",
         .ok = summary.locales_directory,
         .detail = std::format("{} files", summary.locale_file_count),
         .hint = "Add locales/ for future i18n resources."},
        {.name = "fonts",
         .ok = summary.fonts_directory,
         .detail = std::format("{} files", summary.font_file_count),
         .hint = "Add fonts/ with Pretendard or aliases for portable UI text."},
    };
}

auto package_json(PackageSummary const& summary,
                  std::span<Check const> checks) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"package inspect\",\"ok\":{},"
        "\"root\":{},\"manifest\":{},"
        "\"assets\":{{\"present\":{},\"file_count\":{}}},"
        "\"locales\":{{\"present\":{},\"file_count\":{}}},"
        "\"fonts\":{{\"present\":{},\"file_count\":{}}},\"checks\":{}}}",
        all_ok(checks) ? "true" : "false",
        json_string(path_string(summary.root)),
        summary.manifest ? "true" : "false",
        summary.assets_directory ? "true" : "false",
        summary.asset_file_count,
        summary.locales_directory ? "true" : "false",
        summary.locale_file_count,
        summary.fonts_directory ? "true" : "false",
        summary.font_file_count,
        checks_json(checks));
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

auto error_json(std::string_view command, std::string_view message) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":{},\"ok\":false,\"error\":{}}}",
        json_string(command),
        json_string(message));
}

int print_error(std::string_view command, std::string_view message, bool json) {
    if (json) {
        std::println("{}", error_json(command, message));
    } else {
        std::println(std::cerr, "{}",
                     cppx::terminal::format_diagnostic({
                         .severity = cppx::terminal::DiagnosticSeverity::error,
                         .message = std::string{message},
                         .context = std::string{command},
                     }));
    }
    return 2;
}

int run_doctor(cppx::cli::Invocation const& invocation) {
    auto checks = doctor_checks();
    if (invocation.has("json")) {
        std::println(
            "{{\"schema_version\":1,\"command\":\"doctor\",\"ok\":{},\"checks\":{}}}",
            all_ok(checks) ? "true" : "false",
            checks_json(checks));
    } else {
        print_checks("phenotype doctor", checks);
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

int run_package_inspect(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "package inspect");
    if (!path)
        return print_error("package inspect", path.error(), invocation.has("json"));

    auto summary = package_summary(*path);
    auto checks = package_checks(summary);
    if (invocation.has("json")) {
        std::println("{}", package_json(summary, checks));
    } else {
        print_checks("phenotype package inspect", checks);
    }
    return all_ok(checks) ? 0 : 1;
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

int main(int argc, char** argv) {
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
        == std::vector<std::string>{"phenotype", "package", "inspect"})
        return run_package_inspect(*parsed);
    if (parsed->command_path == std::vector<std::string>{"phenotype", "commands"})
        return run_commands(root, *parsed);

    auto const* command = find_command(root, parsed->command_path);
    std::print("{}", cppx::cli::render_help(*command));
    return 0;
}

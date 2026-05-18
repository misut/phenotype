export module phenotype_cli.doctor;

import cppx.cli;
import cppx.terminal;
import phenotype_cli.common;
import phenotype_cli.runtime;
import std;

export namespace phenotype_cli::doctor {

namespace fs = std::filesystem;
using namespace phenotype_cli::common;
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

} // namespace phenotype_cli::doctor

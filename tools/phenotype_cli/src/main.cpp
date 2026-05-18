import cppx.cli;
import cppx.checksum;
import cppx.checksum.system;
import cppx.process;
import cppx.process.system;
import cppx.terminal;
import file_explorer_shared;
import glass_showcase_shared;
import json;
import phenotype_cli.common;
import phenotype_cli.contracts;
import phenotype_cli.file_explorer;
import phenotype_cli.icons;
import phenotype_cli.package;
import phenotype.icon_catalog;
import phenotype.resources;
import phenotype.svg_contract;
import std;

namespace {

namespace fs = std::filesystem;
namespace icon_catalog = phenotype::icon_catalog;
namespace svg_contract = phenotype::svg_contract;
using namespace phenotype_cli::common;
using namespace phenotype_cli::contracts;
using namespace phenotype_cli::file_explorer;
using namespace phenotype_cli::icons;
using namespace phenotype_cli::package;

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

auto help_option() -> cppx::cli::OptionSpec {
    return {.name = "help", .short_name = 'h', .description = "Show help"};
}

auto json_option() -> cppx::cli::OptionSpec {
    return {.name = "json",
            .arity = cppx::cli::OptionArity::none,
            .description = "Emit machine-readable JSON"};
}

auto android_common_options(std::vector<cppx::cli::OptionSpec> extra = {})
    -> std::vector<cppx::cli::OptionSpec> {
    auto options = std::vector<cppx::cli::OptionSpec>{
        help_option(),
        json_option(),
        {.name = "serial",
         .arity = cppx::cli::OptionArity::one,
         .value_name = "adb-serial",
         .description = "ADB serial forwarded through ANDROID_SERIAL"},
        {.name = "avd",
         .arity = cppx::cli::OptionArity::one,
         .value_name = "name",
         .description = "Android virtual device name"},
        {.name = "state-dir",
         .arity = cppx::cli::OptionArity::one,
         .value_name = "dir",
         .description = "Android workflow state directory"},
        {.name = "apk",
         .arity = cppx::cli::OptionArity::one,
         .value_name = "path",
         .description = "Debug APK path override"},
    };
    options.insert(options.end(), extra.begin(), extra.end());
    return options;
}

auto android_command_spec() -> cppx::cli::CommandSpec {
    return {
        .name = "android",
        .summary = "Build, install, run, and observe the Android example",
        .options = {help_option()},
        .subcommands = {
            {
                .name = "doctor",
                .summary = "Check Android SDK, NDK, JDK, adb, emulator, and AVDs",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android doctor --json"},
            },
            {
                .name = "devices",
                .summary = "List configured AVDs and online adb devices",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android devices --json"},
            },
            {
                .name = "emu-start",
                .summary = "Start or reuse the selected Android emulator",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android emu-start --avd Pixel_8"},
            },
            {
                .name = "emu-stop",
                .summary = "Stop the running Android emulator",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android emu-stop"},
            },
            {
                .name = "build",
                .summary = "Build phenotype for aarch64-linux-android",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android build --json"},
            },
            {
                .name = "apk",
                .summary = "Build the Android debug APK",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android apk --json"},
            },
            {
                .name = "install",
                .summary = "Install the Android debug APK on the selected target",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android install --serial emulator-5554"},
            },
            {
                .name = "launch",
                .summary = "Launch the Android example activity",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android launch"},
            },
            {
                .name = "stop",
                .summary = "Force-stop the Android example app",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android stop"},
            },
            {
                .name = "run",
                .summary = "Build, install, force-stop, launch, and sample logs",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android run --json"},
            },
            {
                .name = "logs",
                .summary = "Dump recent filtered Android logs",
                .options = android_common_options({
                    {.name = "lines",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "count",
                     .description = "Recent logcat line count. Defaults to 160"},
                }),
                .allow_positionals = false,
                .examples = {"phenotype android logs --lines 240 --json"},
            },
            {
                .name = "screencap",
                .summary = "Capture an Android screenshot into the state directory",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android screencap --json"},
            },
            {
                .name = "contract",
                .summary = "Run the Android artifact contract on a device or emulator",
                .options = android_common_options({
                    {.name = "contract-out",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "dir",
                     .description = "Pulled contract artifact bundle directory"},
                    {.name = "contract-timeout",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "seconds",
                     .description = "Artifact wait timeout in seconds"},
                }),
                .allow_positionals = false,
                .examples = {"phenotype android contract --json"},
            },
            {
                .name = "clean",
                .summary = "Clean Android Gradle/exon outputs and stop emulator",
                .options = android_common_options(),
                .allow_positionals = false,
                .examples = {"phenotype android clean"},
            },
        },
        .allow_positionals = false,
        .category = "android",
    };
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
                        .name = "verify",
                        .summary = "Run the artifact verifier through mise and uv",
                        .options = {
                            help_option(),
                            json_option(),
                            {.name = "manifest",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "path",
                             .description = "Verifier manifest JSON"},
                            {.name = "expect-platform",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "name",
                             .description = "Expected platform name"},
                            {.name = "require-frame",
                             .arity = cppx::cli::OptionArity::none,
                             .description = "Require frame.bmp"},
                            {.name = "require-label",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "label",
                             .description = "Require an exact semantic label"},
                            {.name = "require-role",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "role",
                             .description = "Require a semantic role"},
                            {.name = "require-material-kind",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "kind",
                             .description = "Require a material kind"},
                            {.name = "require-material-surface-role",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "role",
                             .description = "Require a material surface role"},
                            {.name = "require-capability",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "name",
                             .description = "Require a platform capability"},
                            {.name = "require-runtime-detail",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "path=json",
                             .description = "Require a runtime detail value"},
                            {.name = "require-pixel-region",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "spec",
                             .description = "Require a pixel region contract"},
                            {.name = "require-pixel-region-metric",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "spec",
                             .description = "Require a pixel metric bound"},
                        },
                        .positional_name = "bundle",
                        .positional_description =
                            "Artifact bundle directory passed to the uv-managed verifier.",
                        .examples = {
                            "phenotype artifact verify /tmp/phenotype-glass-showcase --manifest examples/glass_showcase/artifact_manifest.json",
                            "phenotype artifact verify --json /tmp/phenotype-bundle --expect-platform macos --require-frame",
                        },
                    },
                    {
                        .name = "verify-glass-showcase",
                        .summary = "Run the local glass showcase artifact gate",
                        .options = {
                            help_option(),
                            json_option(),
                            {.name = "accessibility",
                             .arity = cppx::cli::OptionArity::none,
                             .description = "Run the accessibility glass gate"},
                            {.name = "bundle-dir",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "path",
                             .description = "Artifact bundle directory override"},
                            {.name = "manifest",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "path",
                             .description = "Verifier manifest JSON override"},
                            {.name = "expect-platform",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "name",
                             .description = "Expected platform name"},
                            {.name = "accessibility-display",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "flags",
                             .description = "Accessibility display flags passed to the example"},
                            {.name = "timeout-seconds",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "seconds",
                             .description = "Native example run timeout"},
                        },
                        .examples = {
                            "phenotype artifact verify-glass-showcase",
                            "phenotype artifact verify-glass-showcase --accessibility --json",
                        },
                    },
                    {
                        .name = "verify-file-explorer",
                        .summary = "Run the local desktop/mobile file explorer artifact gate",
                        .options = {
                            help_option(),
                            json_option(),
                            {.name = "desktop-artifact-dir",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "path",
                             .description = "Desktop artifact bundle root override"},
                            {.name = "mobile-artifact-dir",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "path",
                             .description = "Mobile artifact bundle root override"},
                            {.name = "settle-seconds",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "seconds",
                             .description = "Native window settle time before capture"},
                            {.name = "profile",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "all|desktop|mobile",
                             .description = "Limit the gate to one file explorer profile"},
                            {.name = "view-mode",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "mode",
                             .description = "Desktop view mode to capture"},
                            {.name = "scenario",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "name",
                             .description =
                                "Startup or desktop chrome scenario to capture"},
                        },
                        .examples = {
                            "phenotype artifact verify-file-explorer",
                            "phenotype artifact verify-file-explorer --json",
                            "phenotype artifact verify-file-explorer --profile desktop --view-mode icon --scenario search-active",
                            "phenotype artifact verify-file-explorer --profile desktop --view-mode icon --scenario more-actions-open",
                        },
                    },
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
                .name = "observe",
                .summary = "Emit a unified artifact output observation",
                .options = {
                    help_option(),
                    json_option(),
                    {.name = "manifest",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "path",
                     .description = "Verifier manifest JSON; implies --verify"},
                    {.name = "verify",
                     .arity = cppx::cli::OptionArity::none,
                     .description = "Run the uv-managed artifact verifier and embed its report"},
                    {.name = "expect-platform",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "name",
                     .description = "Expected platform name forwarded to the verifier"},
                    {.name = "require-frame",
                     .arity = cppx::cli::OptionArity::none,
                     .description = "Require frame.bmp in structural and verifier checks"},
                },
                .category = "runtime",
                .positional_name = "bundle",
                .positional_description =
                    "Artifact bundle directory containing snapshot.json, frame.bmp, and platform details.",
                .examples = {
                    "phenotype observe --json /tmp/phenotype-glass-showcase",
                    "phenotype observe --json /tmp/phenotype-glass-showcase --manifest examples/glass_showcase/artifact_manifest.json",
                },
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
                    {
                        .name = "list",
                        .summary = "List package manifests below a root",
                        .options = {help_option(), json_option()},
                        .positional_name = "root",
                        .positional_description =
                            "Directory to scan. Defaults to the current directory.",
                        .examples = {
                            "phenotype package list examples",
                            "phenotype package list --json examples",
                        },
                    },
                    {
                        .name = "bundle",
                        .summary = "Stage package resources into a bundle directory",
                        .options = {
                            help_option(),
                            json_option(),
                            {.name = "output",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "dir",
                             .description = "Output staging directory for copied resources"},
                            {.name = "format",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "resources|macos-app",
                             .description = "Bundle layout to produce"},
                        },
                        .positional_name = "path",
                        .positional_description =
                            "Directory expected to contain phenotype.package.toml.",
                        .examples = {
                            "phenotype package bundle examples/file_explorer_desktop --output /tmp/phenotype-file-explorer",
                            "phenotype package bundle examples/file_explorer_desktop --format macos-app --output '/tmp/Phenotype File Explorer.app'",
                            "phenotype package bundle --json examples/file_explorer_mobile --output /tmp/phenotype-mobile",
                        },
                    },
                    {
                        .name = "verify-bundle",
                        .summary = "Verify a staged package bundle directory",
                        .options = {help_option(), json_option()},
                        .positional_name = "path",
                        .positional_description =
                            "Bundle directory containing phenotype.bundle.json and staged resources.",
                        .examples = {
                            "phenotype package verify-bundle /tmp/phenotype-file-explorer",
                            "phenotype package verify-bundle --json /tmp/phenotype-mobile",
                        },
                    },
                },
                .allow_positionals = false,
                .category = "packaging",
            },
            {
                .name = "icons",
                .summary = "Inspect built-in icon catalog contracts",
                .options = {help_option()},
                .subcommands = {
                    {
                        .name = "catalog",
                        .summary =
                            "Emit the macOS-style SVG icon catalog metadata",
                        .options = {help_option(), json_option()},
                        .allow_positionals = false,
                        .examples = {
                            "phenotype icons catalog --json",
                            "phenotype icons catalog",
                        },
                    },
                    {
                        .name = "lookup",
                        .summary =
                            "Resolve one built-in icon by name or semantic reference",
                        .options = {help_option(), json_option()},
                        .positional_name = "name-or-reference",
                        .positional_description =
                            "Built-in symbol name such as recents or SF Symbols semantic reference such as clock.",
                        .examples = {
                            "phenotype icons lookup recents --json",
                            "phenotype icons lookup magnifyingglass --json",
                        },
                    },
                    {
                        .name = "svg",
                        .summary =
                            "Emit one audited built-in icon SVG source",
                        .options = {help_option(), json_option()},
                        .positional_name = "name-or-reference",
                        .positional_description =
                            "Built-in symbol name such as applications or SF Symbols semantic reference such as desktopcomputer.",
                        .examples = {
                            "phenotype icons svg recents",
                            "phenotype icons svg desktopcomputer --json",
                        },
                    },
                    {
                        .name = "present",
                        .summary =
                            "Resolve one icon's macOS-style presentation recipe",
                        .options = {
                            help_option(),
                            json_option(),
                            {.name = "role",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name =
                                 "toolbar|navigation|sidebar|file_type|action",
                             .description =
                                 "Presentation role. Defaults to the symbol's catalog role."},
                            {.name = "phase",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "normal|hovered|pressed",
                             .description =
                                 "Interaction phase. Defaults to normal."},
                            {.name = "selected",
                             .arity = cppx::cli::OptionArity::none,
                             .description = "Resolve the selected-state recipe"},
                            {.name = "disabled",
                             .arity = cppx::cli::OptionArity::none,
                             .description = "Resolve the disabled-state recipe"},
                        },
                        .positional_name = "name-or-reference",
                        .positional_description =
                            "Built-in symbol name such as search or SF Symbols semantic reference such as magnifyingglass.",
                        .examples = {
                            "phenotype icons present magnifyingglass --role toolbar --phase pressed --json",
                            "phenotype icons present recents --role sidebar --selected --json",
                        },
                    },
                    {
                        .name = "render",
                        .summary =
                            "Emit one icon as a standalone macOS-style SVG",
                        .options = {
                            help_option(),
                            json_option(),
                            {.name = "role",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name =
                                 "toolbar|navigation|sidebar|file_type|action",
                             .description =
                                 "Presentation role. Defaults to the symbol's catalog role."},
                            {.name = "phase",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "normal|hovered|pressed",
                             .description =
                                 "Interaction phase. Defaults to normal."},
                            {.name = "selected",
                             .arity = cppx::cli::OptionArity::none,
                             .description = "Resolve the selected-state recipe"},
                            {.name = "disabled",
                             .arity = cppx::cli::OptionArity::none,
                             .description = "Resolve the disabled-state recipe"},
                            {.name = "output",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "path",
                             .description =
                                 "Write the rendered standalone SVG to this path"},
                        },
                        .positional_name = "name-or-reference",
                        .positional_description =
                            "Built-in symbol name such as search or SF Symbols semantic reference such as magnifyingglass.",
                        .examples = {
                            "phenotype icons render magnifyingglass --role toolbar --phase pressed",
                            "phenotype icons render recents --role sidebar --selected --output /tmp/recents.svg",
                            "phenotype icons render doc.richtext --role file_type --json",
                        },
                    },
                },
                .allow_positionals = false,
                .category = "metadata",
            },
            {
                .name = "svg",
                .summary = "Inspect pure SVG vector image contracts",
                .options = {help_option()},
                .subcommands = {
                    {
                        .name = "inspect",
                        .summary =
                            "Parse an SVG file through phenotype's pure SVG subset",
                        .options = {help_option(), json_option()},
                        .positional_name = "path",
                        .positional_description =
                            "Path to an SVG file to parse and summarize.",
                        .examples = {
                            "phenotype svg inspect assets/file-explorer-icon.svg --json",
                            "phenotype svg inspect /tmp/icon.svg",
                        },
                    },
                },
                .allow_positionals = false,
                .category = "metadata",
            },
            {
                .name = "theme",
                .summary = "Inspect default glass theme contracts",
                .options = {help_option()},
                .subcommands = {
                    {
                        .name = "contract",
                        .summary =
                            "Emit the Apple-like default glass theme contract",
                        .options = {help_option(), json_option()},
                        .allow_positionals = false,
                        .examples = {
                            "phenotype theme contract --json",
                            "phenotype theme contract",
                        },
                    },
                },
                .allow_positionals = false,
                .category = "metadata",
            },
            {
                .name = "io",
                .summary = "Inspect pure input and output abstraction contracts",
                .options = {help_option()},
                .subcommands = {
                    {
                        .name = "contract",
                        .summary =
                            "Emit the pure CLI/native input-output contract",
                        .options = {help_option(), json_option()},
                        .allow_positionals = false,
                        .examples = {
                            "phenotype io contract --json",
                            "phenotype io contract",
                        },
                    },
                },
                .allow_positionals = false,
                .category = "metadata",
            },
            {
                .name = "drive",
                .summary = "Drive deterministic app input contracts",
                .options = {help_option()},
                .subcommands = {
                    {
                        .name = "file-explorer",
                        .summary = "Drive the shared file explorer model without a native window",
                        .options = {
                            help_option(),
                            json_option(),
                            {.name = "profile",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "desktop|mobile",
                             .description = "Demo profile. Defaults to desktop"},
                            {.name = "scenario",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "name",
                             .description = "Startup scenario to apply before explicit inputs"},
                            {.name = "script",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "path",
                             .description = "Line-based input script; blank lines and # comments are ignored"},
                            {.name = "input",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "kind[:value]",
                             .description = "Typed input such as viewport:900x620@2, click:README.txt, key:tab, focus:search, pointer:content-grid, create-file, delete, sort:recent"},
                            {.name = "expect",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "kind:value",
                             .description = "Assert final output, such as selected:README.txt, entry:Note.txt, operation:file_delete:ok, focus-visible:false"},
                            {.name = "locale",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "tag",
                             .description = "Locale used for observed labels. Defaults to package/default locale"},
                            {.name = "package",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "path",
                             .description = "Package root whose manifest/locales provide observed resources"},
                        },
                        .allow_positionals = false,
                        .examples = {
                            "phenotype drive file-explorer --json --input select:README.txt --input duplicate",
                            "phenotype drive file-explorer --json --input open:Documents --input select:'Project Notes.txt'",
                            "phenotype drive file-explorer --json --input click:README.txt --expect focus-visible:false --expect input-modality:pointer",
                            "phenotype drive file-explorer --json --input key:tab --expect focus-visible:true --expect focus-target:sidebar",
                            "phenotype drive file-explorer --profile mobile --scenario created-preview --json",
                            "phenotype drive file-explorer --script explorer.drive --expect operation:file_create:ok --json",
                        },
                    },
                    {
                        .name = "glass-showcase",
                        .summary = "Drive the shared glass showcase material model without a native window",
                        .options = {
                            help_option(),
                            json_option(),
                            {.name = "script",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "path",
                             .description = "Line-based input script; blank lines and # comments are ignored"},
                            {.name = "input",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "kind[:value]",
                             .description = "Typed input such as viewport:520x760@2, density:dense, backdrop:high, inspector:closed"},
                            {.name = "expect",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "kind:value",
                             .description = "Assert final state, such as density:dense, backdrop:high, material-count:7"},
                        },
                        .allow_positionals = false,
                        .examples = {
                            "phenotype drive glass-showcase --json --input density:dense --input backdrop:high",
                            "phenotype drive glass-showcase --script glass.drive --expect material-count:7 --json",
                        },
                    },
                },
                .allow_positionals = false,
                .category = "runtime",
            },
            {
                .name = "run",
                .summary = "Build and run a repository example",
                .options = {
                    help_option(),
                    json_option(),
                    {.name = "no-build",
                     .arity = cppx::cli::OptionArity::none,
                     .description = "Run the existing .exon/debug binary without rebuilding"},
                    {.name = "artifact-dir",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "dir",
                     .description = "Set PHENOTYPE_ARTIFACT_DIR for startup capture"},
                    {.name = "artifact-exit",
                     .arity = cppx::cli::OptionArity::none,
                     .description = "Set PHENOTYPE_ARTIFACT_EXIT=1"},
                    {.name = "observe-output",
                     .arity = cppx::cli::OptionArity::none,
                     .description =
                        "Capture a startup artifact, exit, and embed the parsed output observation"},
                    {.name = "artifact-reason",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "text",
                     .description = "Set PHENOTYPE_ARTIFACT_REASON"},
                    {.name = "accessibility-display",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "mode",
                     .description = "Set PHENOTYPE_ACCESSIBILITY_DISPLAY"},
                    {.name = "script",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "path",
                     .description = "Apply a file explorer input script before startup capture"},
                    {.name = "input",
                     .arity = cppx::cli::OptionArity::one,
                     .repeatable = true,
                     .value_name = "kind[:value]",
                     .description = "Apply file explorer input before startup capture"},
                    {.name = "env",
                     .arity = cppx::cli::OptionArity::one,
                     .repeatable = true,
                     .value_name = "KEY=VALUE",
                     .description = "Add an environment override for the example process"},
                    {.name = "timeout-seconds",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "seconds",
                     .description = "Kill the example if it does not exit before the timeout"},
                },
                .category = "runtime",
                .positional_name = "example",
                .positional_description =
                    "Example name such as glass_showcase or path such as examples/file_explorer_desktop.",
                .examples = {
                    "phenotype run glass_showcase --artifact-dir /tmp/phenotype-glass --artifact-exit",
                    "phenotype run examples/file_explorer_desktop --artifact-exit --input select:README.txt",
                    "phenotype run examples/file_explorer_desktop --observe-output --input select:README.txt --json",
                },
            },
            android_command_spec(),
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

auto read_text_file(fs::path const& path) -> std::string {
    auto input = std::ifstream{path, std::ios::binary};
    if (!input)
        return {};
    auto out = std::ostringstream{};
    out << input.rdbuf();
    return out.str();
}

auto write_text_file(fs::path const& path,
                     std::string_view text,
                     std::string& error) -> bool {
    auto ec = std::error_code{};
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    auto out = std::ofstream{path, std::ios::binary};
    if (!out) {
        error = "failed to open file for writing";
        return false;
    }
    out << text;
    if (!out) {
        error = "failed to write file";
        return false;
    }
    return true;
}

auto sha256_file_or_error(fs::path const& path)
    -> std::expected<std::string, std::string> {
    auto digest = cppx::checksum::system::sha256_file(path);
    if (digest)
        return *digest;
    return std::unexpected{std::format(
        "{}: {}",
        cppx::checksum::to_string(digest.error().code),
        digest.error().message)};
}

auto trim_copy(std::string_view text) -> std::string {
    auto begin = text.begin();
    auto end = text.end();
    while (begin != end
           && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (begin != end
           && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string{begin, end};
}

auto safe_relative_path(fs::path const& path) -> bool {
    if (path.empty() || path.is_absolute())
        return false;
    for (auto const& part : path.lexically_normal()) {
        if (part == "..")
            return false;
    }
    return true;
}

auto path_stays_under_root(fs::path const& root,
                           fs::path const& path,
                           std::string& error) -> bool {
    auto ec = std::error_code{};
    auto canonical_root = fs::weakly_canonical(root, ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    auto canonical_path = fs::weakly_canonical(path, ec);
    if (ec) {
        error = ec.message();
        return false;
    }

    auto relative = canonical_path.lexically_relative(canonical_root);
    if (relative.empty()) {
        error = "source path must stay under the package root";
        return false;
    }
    for (auto const& part : relative) {
        if (part == "..") {
            error = "source path must stay under the package root";
            return false;
        }
    }
    return true;
}

auto strip_toml_comment(std::string_view line) -> std::string {
    auto in_string = false;
    auto escaped = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        auto ch = line[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\' && in_string) {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (ch == '#' && !in_string)
            return trim_copy(line.substr(0, i));
    }
    return trim_copy(line);
}

auto quoted_value_for_key(std::string_view line, std::string_view key)
    -> std::optional<std::string> {
    auto trimmed = strip_toml_comment(line);
    if (!trimmed.starts_with(key))
        return std::nullopt;
    auto rest = trim_copy(std::string_view{trimmed}.substr(key.size()));
    if (!rest.starts_with("="))
        return std::nullopt;
    rest = trim_copy(std::string_view{rest}.substr(1));
    if (rest.size() < 2 || rest.front() != '"')
        return std::nullopt;

    auto value = std::string{};
    auto escaped = false;
    for (std::size_t i = 1; i < rest.size(); ++i) {
        auto ch = rest[i];
        if (escaped) {
            switch (ch) {
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            default: value.push_back(ch); break;
            }
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"')
            return value;
        value.push_back(ch);
    }
    return std::nullopt;
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

auto json_object_member(json::Object const& object, std::string_view key)
    -> json::Value const* {
    auto found = object.find(std::string{key});
    return found == object.end() ? nullptr : &found->second;
}

auto json_at(json::Value const& value,
             std::initializer_list<std::string_view> path)
    -> json::Value const* {
    auto const* current = &value;
    for (auto key : path) {
        if (!current || !current->is_object())
            return nullptr;
        current = json_object_member(current->as_object(), key);
    }
    return current;
}

auto json_object_at(json::Value const& value,
                    std::initializer_list<std::string_view> path)
    -> json::Object const* {
    auto const* found = json_at(value, path);
    return found && found->is_object() ? &found->as_object() : nullptr;
}

auto json_array_at(json::Value const& value,
                   std::initializer_list<std::string_view> path)
    -> json::Array const* {
    auto const* found = json_at(value, path);
    return found && found->is_array() ? &found->as_array() : nullptr;
}

auto json_string_at(json::Value const& value,
                    std::initializer_list<std::string_view> path)
    -> std::optional<std::string> {
    auto const* found = json_at(value, path);
    if (!found || !found->is_string())
        return std::nullopt;
    return found->as_string();
}

auto json_integer_at(json::Value const& value,
                     std::initializer_list<std::string_view> path)
    -> std::optional<std::int64_t> {
    auto const* found = json_at(value, path);
    if (!found || !found->is_number())
        return std::nullopt;
    return found->as_integer();
}

auto json_bool_at(json::Value const& value,
                  std::initializer_list<std::string_view> path)
    -> std::optional<bool> {
    auto const* found = json_at(value, path);
    if (!found || !found->is_bool())
        return std::nullopt;
    return found->as_bool();
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


auto output_line_count(std::string_view text) -> std::size_t {
    return static_cast<std::size_t>(
        std::ranges::count(text, '\n')
        + (text.empty() || text.ends_with('\n') ? 0 : 1));
}

auto output_tail(std::string_view text, std::size_t max_bytes = 16384)
    -> std::string_view {
    if (text.size() <= max_bytes)
        return text;
    return text.substr(text.size() - max_bytes);
}


auto glass_state_json(glass_showcase_demo::State const& state) -> std::string {
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

auto glass_input_json(glass_showcase_demo::GlassInput const& input)
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

auto glass_trace_json(glass_showcase_demo::GlassInputTrace const& trace,
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

auto glass_trace_array_json(
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

auto glass_expectation_json(
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

auto glass_expectations_json(
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

auto glass_material_contract_json(glass_showcase_demo::State const& state)
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

auto glass_drive_json(
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

auto process_result_detail_json(
        std::optional<cppx::process::CapturedProcessResult> const& result)
    -> std::string {
    if (!result)
        return "{\"executed\":false}";
    return std::format(
        "{{\"executed\":true,\"ok\":{},\"exit_code\":{},\"timed_out\":{},"
        "\"stdout_line_count\":{},\"stderr_line_count\":{},"
        "\"stdout_tail\":{},\"stderr_tail\":{}}}",
        (!result->timed_out && result->exit_code == 0) ? "true" : "false",
        result->exit_code,
        result->timed_out ? "true" : "false",
        output_line_count(result->stdout_text),
        output_line_count(result->stderr_text),
        json_string(output_tail(result->stdout_text)),
        json_string(output_tail(result->stderr_text)));
}

auto executable_filename(std::string const& package_name) -> std::string {
#if defined(_WIN32)
    return package_name + ".exe";
#else
    return package_name;
#endif
}

auto parse_seconds(std::string_view text, std::string_view option_name)
    -> std::expected<std::optional<std::chrono::milliseconds>, std::string> {
    if (text.empty()) {
        return std::unexpected{
            std::format("{} requires a numeric value", option_name)};
    }
    long long value = 0;
    auto first = text.data();
    auto last = text.data() + text.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last || value < 0 || value > 86400) {
        return std::unexpected{
            std::format("{} must be an integer from 0 to 86400", option_name)};
    }
    if (value == 0)
        return std::optional<std::chrono::milliseconds>{};
    return std::optional<std::chrono::milliseconds>{
        std::chrono::seconds{value}};
}

auto valid_env_name(std::string_view name) -> bool {
    if (name.empty())
        return false;
    auto first = static_cast<unsigned char>(name.front());
    if (!(std::isalpha(first) || name.front() == '_'))
        return false;
    return std::ranges::all_of(name.substr(1), [](char ch) {
        auto uch = static_cast<unsigned char>(ch);
        return std::isalnum(uch) || ch == '_';
    });
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

auto exon_package_name(fs::path const& example_root)
    -> std::expected<std::string, std::string> {
    auto text = read_text_file(example_root / "exon.toml");
    if (text.empty())
        return std::unexpected{"exon.toml is empty or unreadable"};

    auto input = std::istringstream{text};
    auto line = std::string{};
    auto in_package = false;
    while (std::getline(input, line)) {
        auto trimmed = strip_toml_comment(line);
        if (trimmed.empty())
            continue;
        if (trimmed.size() >= 2 && trimmed.front() == '['
            && trimmed.back() == ']') {
            in_package = trimmed == "[package]";
            continue;
        }
        if (!in_package)
            continue;
        if (auto value = quoted_value_for_key(trimmed, "name")) {
            if (value->empty())
                return std::unexpected{"[package].name is empty"};
            return *value;
        }
    }
    return std::unexpected{"exon.toml does not define [package].name"};
}

auto run_example_build(fs::path const& example_root)
    -> std::expected<cppx::process::CapturedProcessResult,
                     cppx::process::process_error> {
    auto spec = cppx::process::ProcessSpec{
        .program = "mise",
        .args = {"exec", "--", "exon", "build"},
        .cwd = example_root,
        .timeout = std::chrono::minutes{45},
    };
    return cppx::process::system::capture(spec);
}

auto run_exon_at(fs::path const& cwd,
                 std::vector<std::string> args,
                 std::chrono::milliseconds timeout)
    -> std::expected<cppx::process::CapturedProcessResult,
                     cppx::process::process_error> {
    auto full_args = std::vector<std::string>{"exec", "--", "exon"};
    full_args.insert(full_args.end(), args.begin(), args.end());
    auto spec = cppx::process::ProcessSpec{
        .program = "mise",
        .args = std::move(full_args),
        .cwd = cwd,
        .timeout = timeout,
    };
    return cppx::process::system::capture(spec);
}

auto run_example_binary(fs::path const& executable,
                        fs::path const& example_root,
                        std::map<std::string, std::string> env,
                        std::optional<std::chrono::milliseconds> timeout)
    -> std::expected<cppx::process::CapturedProcessResult,
                     cppx::process::process_error> {
    auto spec = cppx::process::ProcessSpec{
        .program = path_string(executable),
        .cwd = example_root,
        .timeout = timeout,
        .env_overrides = std::move(env),
    };
    return cppx::process::system::capture(spec);
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

int print_error(std::string_view command, std::string_view message, bool json);

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

auto single_positional_text_or_error(cppx::cli::Invocation const& invocation,
                                     std::string_view command_name,
                                     std::string_view value_name)
    -> std::expected<std::string, std::string> {
    if (invocation.positionals.empty()) {
        return std::unexpected{
            std::format("{} requires one positional {}", command_name, value_name)};
    }
    if (invocation.positionals.size() > 1) {
        return std::unexpected{
            std::format("{} accepts exactly one positional {}", command_name, value_name)};
    }
    return invocation.positionals.front();
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

auto uv_project_environment() -> std::string;

auto environment_value(std::string_view name) -> std::optional<std::string> {
    auto key = std::string{name};
    auto const* value = std::getenv(key.c_str());
    if (value == nullptr || value[0] == '\0')
        return std::nullopt;
    return std::string{value};
}

auto repo_relative_path(fs::path const& root, std::string_view value) -> fs::path {
    auto path = fs::path{std::string{value}};
    if (path.is_relative())
        path = root / path;
    return path.lexically_normal();
}

auto make_temp_directory(std::string_view prefix)
    -> std::expected<fs::path, std::string> {
    auto ec = std::error_code{};
    auto base = fs::temp_directory_path(ec);
    if (ec) {
        return std::unexpected{std::format(
            "could not resolve temporary directory: {}",
            ec.message())};
    }

    auto seed = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    for (auto attempt = 0u; attempt < 64u; ++attempt) {
        auto candidate = base / std::format(
            "{}.{:x}.{}",
            prefix,
            seed,
            attempt);
        ec.clear();
        if (fs::create_directory(candidate, ec))
            return candidate.lexically_normal();
        if (ec && !path_exists(candidate)) {
            return std::unexpected{std::format(
                "could not create temporary artifact directory {}: {}",
                path_string(candidate),
                ec.message())};
        }
    }
    return std::unexpected{
        "could not create a unique temporary artifact directory"};
}

auto glass_artifact_detail_json(ArtifactSummary const& artifact) -> std::string {
    return std::format(
        "{{\"bundle\":{},\"exists\":{},\"is_directory\":{},"
        "\"snapshot_json\":{{\"present\":{},\"bytes\":{}}},"
        "\"frame_bmp\":{{\"present\":{},\"bytes\":{}}},"
        "\"platform\":{{\"present\":{},\"file_count\":{}}}}}",
        json_string(path_string(artifact.bundle)),
        artifact.exists ? "true" : "false",
        artifact.is_directory ? "true" : "false",
        artifact.snapshot_json ? "true" : "false",
        artifact.snapshot_bytes,
        artifact.frame_bmp ? "true" : "false",
        artifact.frame_bytes,
        artifact.platform_directory ? "true" : "false",
        artifact.platform_file_count);
}

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
        ? glass_artifact_detail_json(*summary.artifact)
        : std::string{"null"};
    return std::format(
        "{{\"schema_version\":1,\"command\":\"artifact verify-glass-showcase\","
        "\"ok\":{},\"accessibility\":{},\"example_root\":{},"
        "\"package_name\":{},\"executable\":{},\"artifact_dir\":{},"
        "\"manifest\":{},\"expect_platform\":{},"
        "\"accessibility_display\":{},\"timeout_seconds\":{},"
        "\"build\":{},\"run_result\":{},\"verifier\":{},"
        "\"artifact\":{},\"error\":{}}}",
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
        json_string(summary.error));
}

auto result_status(
        std::optional<cppx::process::CapturedProcessResult> const& result)
    -> cppx::terminal::StatusKind {
    if (!result)
        return cppx::terminal::StatusKind::skip;
    return (!result->timed_out && result->exit_code == 0)
        ? cppx::terminal::StatusKind::ok
        : cppx::terminal::StatusKind::fail;
}

auto result_value(
        std::optional<cppx::process::CapturedProcessResult> const& result)
    -> std::string {
    if (!result)
        return "not run";
    if (result->timed_out)
        return "timed out";
    return std::format("exit {}", result->exit_code);
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

auto uv_project_environment() -> std::string {
    if (auto const* explicit_env =
            std::getenv("PHENOTYPE_UV_PROJECT_ENVIRONMENT")) {
        if (explicit_env[0] != '\0')
            return explicit_env;
    }
    auto base = std::string{"/tmp"};
    if (auto const* tmp = std::getenv("TMPDIR")) {
        if (tmp[0] != '\0')
            base = tmp;
    }
    auto path = fs::path{base} / "phenotype-uv-tools";
    return path_string(path);
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

int run_artifact_verify_glass_showcase(
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

auto split_cli_list(std::string_view value) -> std::vector<std::string> {
    auto out = std::vector<std::string>{};
    auto token = std::string{};
    for (char ch : value) {
        auto const byte = static_cast<unsigned char>(ch);
        if (ch == ',' || std::isspace(byte)) {
            if (!token.empty()) {
                out.push_back(std::move(token));
                token.clear();
            }
        } else {
            token.push_back(ch);
        }
    }
    if (!token.empty())
        out.push_back(std::move(token));
    return out;
}

auto default_file_explorer_desktop_modes() -> std::vector<std::string> {
    return {"icon", "list", "column", "gallery"};
}

auto default_file_explorer_scenarios() -> std::vector<std::string> {
    return {
        "created-preview",
        "deleted-file",
        "trash-view",
        "created-folder",
        "deleted-folder",
        "duplicated-file",
        "documents-preview",
        "history-forward",
        "sorted-kind",
        "search-active",
    };
}

auto invocation_list_or_env(cppx::cli::Invocation const& invocation,
                            std::string_view option,
                            std::string_view env_name,
                            std::vector<std::string> fallback)
    -> std::vector<std::string> {
    auto values = invocation.values(option);
    if (!values.empty())
        return std::vector<std::string>{values.begin(), values.end()};
    if (auto value = environment_value(env_name))
        return split_cli_list(*value);
    return fallback;
}

auto parse_settle_seconds(std::string_view text)
    -> std::expected<std::chrono::milliseconds, std::string> {
    if (text.empty())
        return std::unexpected{"--settle-seconds requires a numeric value"};
    try {
        auto copy = std::string{text};
        std::size_t parsed = 0;
        auto seconds = std::stod(copy, &parsed);
        if (parsed != copy.size() || seconds < 0.0 || seconds > 60.0) {
            return std::unexpected{
                "--settle-seconds must be a number from 0 to 60"};
        }
        auto millis = static_cast<long long>((seconds * 1000.0) + 0.5);
        return std::chrono::milliseconds{millis};
    } catch (std::exception const&) {
        return std::unexpected{
            "--settle-seconds must be a number from 0 to 60"};
    }
}

auto file_explorer_profile_from_invocation(
        cppx::cli::Invocation const& invocation)
    -> std::expected<std::string, std::string> {
    auto profile = std::string{"all"};
    if (auto value = invocation.value("profile")) {
        profile = std::string{*value};
    } else if (auto value = environment_value("PHENOTYPE_FILE_EXPLORER_PROFILE")) {
        profile = *value;
    }
    if (profile != "all" && profile != "desktop" && profile != "mobile") {
        return std::unexpected{"profile must be all, desktop, or mobile"};
    }
    return profile;
}

auto validate_desktop_modes(std::span<std::string const> modes)
    -> std::expected<void, std::string> {
    for (auto const& mode : modes) {
        if (mode != "icon" && mode != "list"
            && mode != "column" && mode != "gallery") {
            return std::unexpected{
                "view-mode must be icon, list, column, or gallery"};
        }
    }
    return {};
}

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

auto append_required_debug_detail(std::vector<std::string>& args,
                                  std::string value) {
    append_verifier_arg(args, "--require-debug-detail", std::move(value));
}

auto append_file_explorer_scenario_requirements(
        std::vector<std::string>& args,
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
        append_required_label(args, "Search");
        append_required_label_contains(args, "Searching for Screen");
        append_required_label_contains(args, "Screen Recording");
        append_required_label_contains(args, "Screenshot");
    } else if (scenario == "more-actions-open") {
        append_required_label(args, "More Actions Menu");
        append_required_label(args, "New File");
        append_required_label(args, "New Folder");
        append_required_label(args, "Duplicate");
        append_required_label(args, "Delete");
        append_required_label_contains(args, "Selected README.txt");
        append_required_debug_detail(
            args, "application.file_explorer.chrome.more_actions_open=true");
        append_required_debug_detail(
            args, "application.file_explorer.chrome.overflow_action_button_count=4");
        append_required_debug_detail(
            args, "application.file_explorer.selection.present=true");
        append_required_debug_detail(
            args, "application.file_explorer.selection.name=\"README.txt\"");
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
    auto scenario_extra =
        append_file_explorer_scenario_requirements(args, scenario);
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
    return scenario == "more-actions-open";
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
        ? glass_artifact_detail_json(*item.artifact)
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
    };
    if (item.profile == "desktop")
        env["PHENOTYPE_FILE_EXPLORER_VIEW"] = item.mode;
    if (scenario_active) {
        if (item.scenario == "more-actions-open") {
            env["PHENOTYPE_FILE_EXPLORER_MORE_ACTIONS"] = "1";
            env["PHENOTYPE_FILE_EXPLORER_INPUTS"] = "select:README.txt";
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

int run_artifact_verify_file_explorer(
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
    auto scenarios = invocation_list_or_env(
        invocation,
        "scenario",
        "PHENOTYPE_FILE_EXPLORER_SCENARIOS",
        default_file_explorer_scenarios());

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
            if (scenario == "default")
                continue;
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

int run_icons_catalog(cppx::cli::Invocation const& invocation) {
    auto checks = icon_catalog_checks();
    if (invocation.has("json")) {
        std::println("{}", icon_catalog_json(checks));
        return all_ok(checks) ? 0 : 1;
    }

    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "style",
         .value = std::string{icon_catalog::style_name()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "source",
         .value = std::string{icon_catalog::source_format()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "svg subset",
         .value = std::string{icon_catalog::svg_supported_path_commands()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "symbols",
         .value = std::format(
             "{} total, {} sidebar, {} toolbar, {} file-type",
             icon_catalog::all_symbol_count,
             icon_catalog::sidebar_symbol_count,
             icon_catalog::toolbar_symbol_count,
             icon_catalog::file_type_symbol_count),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "reference",
         .value = std::string{icon_catalog::reference_family()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "rendering",
         .value = std::string{icon_catalog::rendering_capability_policy()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "assets",
         .value = std::string{icon_catalog::asset_policy()},
         .status = all_ok(checks) ? cppx::terminal::StatusKind::ok
                                  : cppx::terminal::StatusKind::fail},
    };
    std::println("phenotype icons catalog");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    print_checks("checks", checks);
    return all_ok(checks) ? 0 : 1;
}

int run_icons_lookup(cppx::cli::Invocation const& invocation) {
    auto query = single_positional_text_or_error(
        invocation,
        "icons lookup",
        "name-or-reference");
    if (!query)
        return print_error("icons lookup", query.error(), invocation.has("json"));

    auto result = lookup_icon_symbol(*query);
    if (!result) {
        if (invocation.has("json")) {
            std::println("{}", icon_lookup_not_found_json(*query));
        } else {
            std::println(
                std::cerr,
                "{}",
                cppx::terminal::format_diagnostic({
                    .severity = cppx::terminal::DiagnosticSeverity::error,
                    .message = std::format(
                        "symbol '{}' was not found; run 'phenotype icons catalog' to list built-in symbols",
                        *query),
                    .context = "icons lookup",
                }));
        }
        return 2;
    }

    if (invocation.has("json")) {
        std::println("{}", icon_lookup_json(*query, *result));
        return 0;
    }

    auto const desc = icon_catalog::descriptor(result->symbol);
    auto const role = icon_catalog::default_presentation_role(result->symbol);
    auto const metrics = icon_catalog::metrics(role);
    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "query",
         .value = *query,
         .status = cppx::terminal::StatusKind::ok},
        {.label = "match",
         .value = std::string{result->match_kind},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "symbol",
         .value = std::string{desc.name},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "semantic reference",
         .value = std::string{desc.semantic_reference_name},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "role",
         .value = std::string{icon_catalog::symbol_presentation_role_name(role)},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "metrics",
         .value = std::format(
             "{}pt symbol, {}pt hit target",
             metrics.point_size,
             metrics.hit_target_size),
         .status = cppx::terminal::StatusKind::ok},
    };
    std::println("phenotype icons lookup");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    return 0;
}

int run_icons_svg(cppx::cli::Invocation const& invocation) {
    auto query = single_positional_text_or_error(
        invocation,
        "icons svg",
        "name-or-reference");
    if (!query)
        return print_error("icons svg", query.error(), invocation.has("json"));

    auto result = lookup_icon_symbol(*query);
    if (!result) {
        if (invocation.has("json")) {
            std::println("{}", icon_svg_not_found_json(*query));
        } else {
            std::println(
                std::cerr,
                "{}",
                cppx::terminal::format_diagnostic({
                    .severity = cppx::terminal::DiagnosticSeverity::error,
                    .message = std::format(
                        "symbol '{}' was not found; run 'phenotype icons catalog' to list built-in symbols",
                        *query),
                    .context = "icons svg",
                }));
        }
        return 2;
    }

    if (invocation.has("json")) {
        std::println("{}", icon_svg_json(*query, *result));
        return 0;
    }

    std::println("{}", icon_catalog::svg_source(result->symbol));
    return 0;
}

int run_icons_present(cppx::cli::Invocation const& invocation) {
    auto query = single_positional_text_or_error(
        invocation,
        "icons present",
        "name-or-reference");
    if (!query)
        return print_error("icons present", query.error(), invocation.has("json"));

    auto result = lookup_icon_symbol(*query);
    if (!result) {
        if (invocation.has("json")) {
            std::println("{}", icon_present_not_found_json(*query));
        } else {
            std::println(
                std::cerr,
                "{}",
                cppx::terminal::format_diagnostic({
                    .severity = cppx::terminal::DiagnosticSeverity::error,
                    .message = std::format(
                        "symbol '{}' was not found; run 'phenotype icons catalog' to list built-in symbols",
                        *query),
                    .context = "icons present",
                }));
        }
        return 2;
    }

    auto role = icon_presentation_role_from_invocation(invocation, result->symbol);
    if (!role)
        return print_error("icons present", role.error(), invocation.has("json"));
    auto phase = icon_interaction_phase_from_invocation(invocation);
    if (!phase)
        return print_error("icons present", phase.error(), invocation.has("json"));

    auto const selected = invocation.has("selected");
    auto const enabled = !invocation.has("disabled");
    if (invocation.has("json")) {
        std::println(
            "{}",
            icon_presentation_json(
                *query,
                *result,
                *role,
                *phase,
                selected,
                enabled));
        return 0;
    }

    auto const desc = icon_catalog::descriptor(result->symbol);
    auto const metrics = icon_catalog::metrics(*role);
    auto const recipe = icon_catalog::macos_state_recipe(
        *role,
        icon_catalog::SymbolInteractionState{selected, enabled},
        *phase);
    auto const visible_color =
        icon_color_with_opacity(recipe.symbol_color, recipe.symbol_opacity);
    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "symbol",
         .value = std::string{desc.name},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "semantic reference",
         .value = std::string{desc.semantic_reference_name},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "state",
         .value = std::format(
             "{} {} selected={} enabled={}",
             icon_catalog::symbol_presentation_role_name(*role),
             icon_catalog::symbol_interaction_phase_name(*phase),
             selected ? "true" : "false",
             enabled ? "true" : "false"),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "tone",
         .value = std::string{icon_catalog::symbol_tone_name(recipe.symbol_tone)},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "visible rgba",
         .value = std::format(
             "{},{},{},{}",
             visible_color.r,
             visible_color.g,
             visible_color.b,
             visible_color.a),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "metrics",
         .value = std::format(
             "{}pt symbol, {}pt effective, {}pt hit target",
             metrics.point_size,
             metrics.point_size * recipe.scale,
             recipe.hit_target_size),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "asset policy",
         .value = std::string{icon_catalog::asset_policy()},
         .status = cppx::terminal::StatusKind::ok},
    };
    std::println("phenotype icons present");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    return 0;
}

int run_icons_render(cppx::cli::Invocation const& invocation) {
    auto query = single_positional_text_or_error(
        invocation,
        "icons render",
        "name-or-reference");
    if (!query)
        return print_error("icons render", query.error(), invocation.has("json"));

    auto result = lookup_icon_symbol(*query);
    if (!result) {
        if (invocation.has("json")) {
            std::println("{}", icon_render_not_found_json(*query));
        } else {
            std::println(
                std::cerr,
                "{}",
                cppx::terminal::format_diagnostic({
                    .severity = cppx::terminal::DiagnosticSeverity::error,
                    .message = std::format(
                        "symbol '{}' was not found; run 'phenotype icons catalog' to list built-in symbols",
                        *query),
                    .context = "icons render",
                }));
        }
        return 2;
    }

    auto role = icon_presentation_role_from_invocation(invocation, result->symbol);
    if (!role)
        return print_error("icons render", role.error(), invocation.has("json"));
    auto phase = icon_interaction_phase_from_invocation(invocation);
    if (!phase)
        return print_error("icons render", phase.error(), invocation.has("json"));

    auto const selected = invocation.has("selected");
    auto const enabled = !invocation.has("disabled");
    auto source = rendered_icon_svg_source(
        *result,
        *role,
        *phase,
        selected,
        enabled);
    auto output_path = fs::path{};
    if (auto output = invocation.value("output")) {
        output_path = fs::path{absolute_path_string(fs::path{std::string{*output}})};
        auto error = std::string{};
        if (!write_text_file(output_path, source, error)) {
            return print_error(
                "icons render",
                std::format(
                    "failed to write rendered icon SVG to {}: {}",
                    path_string(output_path),
                    error),
                invocation.has("json"));
        }
    }
    if (invocation.has("json")) {
        std::println(
            "{}",
            icon_render_json(
                *query,
                *result,
                *role,
                *phase,
                selected,
                enabled,
                source,
                output_path));
        return 0;
    }

    if (!output_path.empty()) {
        auto lines = std::vector<cppx::terminal::StatusLine>{
            {.label = "symbol",
             .value = std::string{
                 icon_catalog::name(result->symbol)},
             .status = cppx::terminal::StatusKind::ok},
            {.label = "output",
             .value = path_string(output_path),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "bytes",
             .value = std::format("{}", source.size()),
             .status = cppx::terminal::StatusKind::ok},
        };
        std::println("phenotype icons render");
        std::println("{}", cppx::terminal::format_status_frame(lines, false));
        return 0;
    }

    std::println("{}", source);
    return 0;
}

int run_svg_inspect(cppx::cli::Invocation const& invocation) {
    auto path_value = single_positional_text_or_error(
        invocation,
        "svg inspect",
        "path");
    if (!path_value)
        return print_error("svg inspect", path_value.error(), invocation.has("json"));

    auto path = fs::path{std::string{*path_value}};
    auto ec = std::error_code{};
    if (!fs::is_regular_file(path, ec) || ec) {
        return print_error(
            "svg inspect",
            std::format(
                "SVG file '{}' is not readable",
                path_string(path)),
            invocation.has("json"));
    }

    auto source = read_text_file(path);
    auto inspection = svg_contract::inspect_source(source);
    auto const summary = inspection.summary;
    if (invocation.has("json")) {
        std::println("{}", svg_inspect_json(path, source, inspection));
        return summary.paintable ? 0 : 2;
    }

    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "path",
         .value = path_string(path),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "subset",
         .value = std::string{svg_contract::subset_policy()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "viewBox",
         .value = std::format(
             "{} {} {} {}",
             summary.view_min_x,
             summary.view_min_y,
             summary.view_width,
             summary.view_height),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "shapes",
         .value = std::format("{}", summary.shape_count),
         .status = summary.paintable
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "unsupported",
         .value = std::format("{}", summary.unsupported_count),
         .status = summary.unsupported_count == 0
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "diagnostics",
         .value = std::format("{}", summary.diagnostic_count),
         .status = summary.diagnostic_count == 0
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "render path",
         .value = std::string{svg_contract::render_pipeline_policy()},
         .status = cppx::terminal::StatusKind::ok},
    };
    std::println("phenotype svg inspect");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    return summary.paintable ? 0 : 2;
}


auto parse_explorer_input_script(fs::path const& path)
    -> std::expected<std::vector<file_explorer_demo::ExplorerInput>, std::string> {
    if (!path_exists(path)) {
        return std::unexpected{
            std::format("input script does not exist: {}", path_string(path))};
    }
    auto text = read_text_file(path);
    auto parsed = file_explorer_demo::parse_explorer_input_lines(
        text,
        path_string(path));
    if (!parsed.ok)
        return std::unexpected{parsed.error};
    return std::move(parsed.inputs);
}

auto join_explorer_input_lines(std::span<std::string const> inputs)
    -> std::string {
    auto out = std::string{};
    for (auto const& input : inputs) {
        if (!out.empty())
            out.push_back('\n');
        out += input;
    }
    return out;
}

auto parse_glass_input_script(fs::path const& path)
    -> std::expected<std::vector<glass_showcase_demo::GlassInput>, std::string> {
    if (!path_exists(path)) {
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

auto explorer_drive_resources(
        std::string_view profile,
        cppx::cli::Invocation const& invocation)
    -> std::expected<ExplorerDriveResources, std::string> {
    auto resources = ExplorerDriveResources{};
    resources.catalog = file_explorer_demo::file_explorer_resource_catalog(profile);

    if (auto package_root = invocation.value("package")) {
        auto package = package_summary(fs::path{std::string{*package_root}});
        auto checks = package_checks(package);
        if (!all_ok(checks)) {
            return std::unexpected{std::format(
                "package resources failed checks: {}",
                path_string(package.root))};
        }
        resources.source = "package";
        resources.package_root = package.root;
        resources.catalog = std::move(package.catalog);
        resources.diagnostics = std::move(package.catalog_diagnostics);
        resources.checks = std::move(checks);
    }

    resources.locale = resources.catalog.default_locale.empty()
        ? "en"
        : resources.catalog.default_locale;
    if (auto locale = invocation.value("locale"))
        resources.locale = std::string{*locale};
    return resources;
}

auto parse_explorer_expectations(cppx::cli::Invocation const& invocation)
    -> std::expected<std::vector<file_explorer_demo::ExplorerExpectation>,
                     std::string> {
    auto expectations = std::vector<file_explorer_demo::ExplorerExpectation>{};
    for (auto const& raw : invocation.values("expect")) {
        auto parsed = file_explorer_demo::parse_explorer_expectation(raw);
        if (!parsed.ok)
            return std::unexpected{parsed.error};
        expectations.push_back(std::move(parsed.expectation));
    }
    return expectations;
}

auto parse_glass_expectations(cppx::cli::Invocation const& invocation)
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

int run_drive_file_explorer(cppx::cli::Invocation const& invocation) {
    auto profile = std::string{"desktop"};
    if (auto value = invocation.value("profile")) {
        profile = std::string{*value};
    }
    auto normalized_profile = file_explorer_demo::lower_copy(profile);
    if (normalized_profile != "desktop" && normalized_profile != "mobile") {
        return print_error(
            "drive file-explorer",
            "profile must be 'desktop' or 'mobile'",
            invocation.has("json"));
    }

    auto inputs = std::vector<file_explorer_demo::ExplorerInput>{};
    if (auto scenario = invocation.value("scenario")) {
        inputs.push_back({
            .kind = file_explorer_demo::ExplorerInputKind::Scenario,
            .value = std::string{*scenario},
        });
    }
    for (auto const& script : invocation.values("script")) {
        auto parsed = parse_explorer_input_script(fs::path{script});
        if (!parsed) {
            return print_error(
                "drive file-explorer",
                parsed.error(),
                invocation.has("json"));
        }
        inputs.insert(inputs.end(),
                      std::make_move_iterator(parsed->begin()),
                      std::make_move_iterator(parsed->end()));
    }
    for (auto const& raw : invocation.values("input")) {
        auto parsed = file_explorer_demo::parse_explorer_input(raw);
        if (!parsed.ok) {
            return print_error(
                "drive file-explorer",
                parsed.error,
                invocation.has("json"));
        }
        inputs.push_back(std::move(parsed.input));
    }

    auto resources = explorer_drive_resources(normalized_profile, invocation);
    if (!resources) {
        return print_error(
            "drive file-explorer",
            resources.error(),
            invocation.has("json"));
    }
    auto labels = file_explorer_demo::file_explorer_labels(
        resources->locale,
        resources->catalog);
    auto expectations = parse_explorer_expectations(invocation);
    if (!expectations) {
        return print_error(
            "drive file-explorer",
            expectations.error(),
            invocation.has("json"));
    }

    auto result = file_explorer_demo::drive_explorer(normalized_profile, inputs);
    auto checked_expectations =
        file_explorer_demo::check_explorer_expectations(
            result,
            *expectations);
    if (invocation.has("json")) {
        std::println("{}",
                     explorer_drive_json(
                         result,
                         *resources,
                         labels,
                         checked_expectations));
    } else {
        auto const& snap = result.snapshot;
        auto lines = std::vector<cppx::terminal::StatusLine>{
            {.label = "profile",
             .value = result.profile,
             .status = cppx::terminal::StatusKind::ok},
            {.label = "locale",
             .value = resources->locale,
             .status = cppx::terminal::StatusKind::ok},
            {.label = "resources",
             .value = resources->source == "package"
                ? path_string(resources->package_root)
                : "builtin",
             .status = cppx::terminal::StatusKind::ok},
            {.label = "location",
             .value = snap.relative_location,
             .status = cppx::terminal::StatusKind::ok},
            {.label = "selected",
             .value = snap.has_selection ? snap.selected.name : "<none>",
             .status = snap.has_selection ? cppx::terminal::StatusKind::ok
                                          : cppx::terminal::StatusKind::skip},
            {.label = "entries",
             .value = std::format("{} files, {} folders",
                                  snap.file_count,
                                  snap.folder_count),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "view",
             .value = file_explorer_demo::view_mode_label(snap.view_mode),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "focus",
             .value = std::format(
                 "{} visible={} modality={}",
                 file_explorer_demo::focus_target_value_name(
                     result.state.focus_target),
                 result.state.focus_visible ? "true" : "false",
                 file_explorer_demo::input_modality_value_name(
                     result.state.last_input_modality)),
             .status = result.state.focus_visible
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::skip},
            {.label = "chrome",
             .value = std::format("{} cols x {} rows, viewport {}x{}@{}",
                                  result.chrome.icon_grid_columns,
                                  result.chrome.icon_grid_visible_rows,
                                  result.chrome.viewport.width,
                                  result.chrome.viewport.height,
                                  result.chrome.viewport.scale),
             .status = cppx::terminal::StatusKind::ok},
            {.label = "status",
             .value = result.state.status,
             .status = explorer_drive_ok(result)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
        };
        if (!result.state.last_operation.kind.empty()) {
            lines.push_back({
                .label = "operation",
                .value = file_explorer_demo::operation_label(
                    result.state.last_operation),
                .status = result.state.last_operation.ok
                    ? cppx::terminal::StatusKind::ok
                    : cppx::terminal::StatusKind::fail,
            });
        }
        std::println("phenotype drive file-explorer");
        std::println("{}", cppx::terminal::format_status_frame(lines, false));
        if (!checked_expectations.empty()) {
            std::println("expectations:");
            for (auto const& expectation : checked_expectations) {
                std::println("  - {} -> {} ({})",
                             file_explorer_demo::explorer_expectation_label(
                                 expectation.expectation),
                             expectation.ok ? "ok" : "failed",
                             expectation.actual);
            }
        }
        if (!result.trace.empty()) {
            std::println("inputs:");
            for (auto const& trace : result.trace) {
                std::println("  - {} -> {}",
                             file_explorer_demo::explorer_input_label(
                                 trace.input),
                             trace.status);
            }
        }
    }
    return explorer_contract_ok(result, checked_expectations) ? 0 : 1;
}

int run_drive_glass_showcase(cppx::cli::Invocation const& invocation) {
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
        return run_package_inspect(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "package", "list"})
        return run_package_list(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "package", "bundle"})
        return run_package_bundle(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "package", "verify-bundle"})
        return run_package_verify_bundle(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "icons", "catalog"})
        return run_icons_catalog(*parsed);
    if (parsed->command_path
        == std::vector<std::string>{"phenotype", "icons", "lookup"})
        return run_icons_lookup(*parsed);
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

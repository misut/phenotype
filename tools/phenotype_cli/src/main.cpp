import cppx.cli;
import cppx.checksum;
import cppx.checksum.system;
import cppx.process;
import cppx.process.system;
import cppx.terminal;
import file_explorer_shared;
import json;
import phenotype.resources;
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

struct PackageSummary {
    fs::path root;
    bool exists = false;
    bool is_directory = false;
    bool manifest = false;
    bool application_section = false;
    bool resources_section = false;
    bool debug_section = false;
    bool artifact_manifest_exists = false;
    bool default_font_pretendard = false;
    bool assets_directory = false;
    bool locales_directory = false;
    bool fonts_directory = false;
    std::string application_id;
    std::string display_name;
    std::string version;
    std::string entry;
    std::vector<std::string> platforms;
    std::string default_font_family;
    std::string artifact_manifest;
    std::string debug_probe_scene;
    std::string debug_verifier;
    phenotype::ResourceCatalog catalog;
    std::vector<phenotype::ResourceDiagnostic> catalog_diagnostics;
    std::vector<std::string> missing_sources;
    std::uintmax_t manifest_bytes = 0;
    std::size_t manifest_asset_count = 0;
    std::size_t manifest_locale_count = 0;
    std::size_t manifest_font_count = 0;
    std::size_t source_reference_count = 0;
    std::size_t locale_string_count = 0;
    std::size_t asset_file_count = 0;
    std::size_t locale_file_count = 0;
    std::size_t font_file_count = 0;
};

struct BundleFileRecord {
    std::string kind;
    std::string name;
    std::string content_type;
    fs::path source;
    fs::path destination;
    std::uintmax_t bytes = 0;
    bool present = false;
    bool copied = false;
    bool preload = false;
    bool runtime_visible = false;
    std::string sha256;
    std::string integrity_error;
    std::string error;
};

struct BundleSummary {
    std::string command = "package bundle";
    fs::path package_root;
    fs::path output_root;
    fs::path manifest_path;
    PackageSummary package;
    std::vector<BundleFileRecord> files;
    std::vector<Check> checks;
    std::uintmax_t total_bytes = 0;
    std::size_t verified_file_count = 0;
    bool bundle_manifest_present = false;
    std::uintmax_t bundle_manifest_bytes = 0;
    bool ok = false;
    std::string error;
};

struct ExplorerDriveResources {
    std::string source = "builtin";
    fs::path package_root;
    std::string locale = "en";
    phenotype::ResourceCatalog catalog;
    std::vector<phenotype::ResourceDiagnostic> diagnostics;
    std::vector<Check> checks;
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
    std::optional<std::chrono::milliseconds> run_timeout;
    std::optional<cppx::process::CapturedProcessResult> build_result;
    std::optional<cppx::process::CapturedProcessResult> run_result;
    std::optional<ArtifactSummary> artifact;
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
                            "Artifact bundle directory passed to tools/verify_artifact_bundle.py.",
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
                            {.name = "expect-platform",
                             .arity = cppx::cli::OptionArity::one,
                             .value_name = "name",
                             .description = "Expected platform name"},
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
                             .description = "Startup scenario to capture"},
                        },
                        .examples = {
                            "phenotype artifact verify-file-explorer",
                            "phenotype artifact verify-file-explorer --json",
                            "phenotype artifact verify-file-explorer --profile desktop --view-mode icon --scenario search-active",
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
                .positional_name = "bundle",
                .positional_description =
                    "Artifact bundle directory containing snapshot.json, frame.bmp, and platform details.",
                .category = "runtime",
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
                        },
                        .positional_name = "path",
                        .positional_description =
                            "Directory expected to contain phenotype.package.toml.",
                        .examples = {
                            "phenotype package bundle examples/file_explorer_desktop --output /tmp/phenotype-file-explorer",
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
                             .description = "Typed input such as viewport:900x620@2, select:README.txt, create-file, delete, sort:recent"},
                            {.name = "expect",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "kind:value",
                             .description = "Assert final output, such as selected:README.txt, entry:Note.txt, operation:file_delete:ok"},
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
                            "phenotype drive file-explorer --profile mobile --scenario created-preview --json",
                            "phenotype drive file-explorer --script explorer.drive --expect operation:file_create:ok --json",
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
                    {.name = "artifact-reason",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "text",
                     .description = "Set PHENOTYPE_ARTIFACT_REASON"},
                    {.name = "accessibility-display",
                     .arity = cppx::cli::OptionArity::one,
                     .value_name = "mode",
                     .description = "Set PHENOTYPE_ACCESSIBILITY_DISPLAY"},
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
                    "phenotype run examples/file_explorer_desktop --env PHENOTYPE_FILE_EXPLORER_VIEW=icon",
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

auto absolute_path_string(fs::path const& path) -> std::string {
    auto ec = std::error_code{};
    auto absolute = fs::absolute(path, ec);
    return ec ? path_string(path) : path_string(absolute);
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

auto package_summary(fs::path root) -> PackageSummary {
    auto summary = PackageSummary{.root = std::move(root)};
    summary.exists = path_exists(summary.root);
    summary.is_directory = path_is_directory(summary.root);
    summary.catalog.default_locale = "en";
    summary.catalog.default_font_family = "Pretendard";
    auto manifest_path = summary.root / "phenotype.package.toml";
    summary.manifest = path_exists(manifest_path);
    summary.manifest_bytes = file_size_or_zero(manifest_path);
    summary.assets_directory = path_is_directory(summary.root / "assets");
    summary.locales_directory = path_is_directory(summary.root / "locales");
    summary.fonts_directory = path_is_directory(summary.root / "fonts");
    summary.asset_file_count = count_regular_files(summary.root / "assets");
    summary.locale_file_count = count_regular_files(summary.root / "locales");
    summary.font_file_count = count_regular_files(summary.root / "fonts");

    if (summary.manifest) {
        auto text = read_text_file(manifest_path);
        auto parsed = phenotype::parse_resource_manifest(text);
        summary.catalog = std::move(parsed.catalog);
        summary.application_section = parsed.application_section;
        summary.resources_section = parsed.resources_section;
        summary.debug_section = parsed.debug_section;
        summary.artifact_manifest = std::move(parsed.artifact_manifest);
        summary.artifact_manifest_exists = !summary.artifact_manifest.empty()
            && path_exists(summary.root / summary.artifact_manifest);
        summary.source_reference_count = parsed.source_references.size();
        for (auto const& source : parsed.source_references) {
            if (!path_exists(summary.root / source))
                summary.missing_sources.push_back(source);
        }
    }
    summary.application_id = summary.catalog.application.id;
    summary.display_name = summary.catalog.application.display_name;
    summary.version = summary.catalog.application.version;
    summary.entry = summary.catalog.application.entry;
    summary.platforms = summary.catalog.application.platforms;
    summary.default_font_family = summary.catalog.default_font_family;
    summary.default_font_pretendard =
        summary.catalog.default_font_family == "Pretendard";
    summary.debug_probe_scene = summary.catalog.debug.probe_scene;
    summary.debug_verifier = summary.catalog.debug.verifier;
    summary.manifest_asset_count = summary.catalog.assets.size();
    summary.manifest_locale_count = summary.catalog.locales.size();
    summary.manifest_font_count = summary.catalog.fonts.size();

    for (auto& locale : summary.catalog.locales) {
        if (locale.source.empty())
            continue;
        auto source_path = summary.root / locale.source;
        if (!path_exists(source_path))
            continue;
        locale.strings = parse_locale_strings(source_path);
        summary.locale_string_count += locale.strings.size();
    }
    if (summary.catalog.debug.artifact_manifest.empty()
        && !summary.artifact_manifest.empty()) {
        summary.catalog.debug.artifact_manifest = summary.artifact_manifest;
    }

    auto required_keys = std::vector<std::string>{};
    if (auto locale = phenotype::find_locale(
            summary.catalog,
            summary.catalog.default_locale)) {
        for (auto const& text : locale->get().strings)
            required_keys.push_back(text.key);
    }
    auto required_views = std::vector<std::string_view>{};
    required_views.reserve(required_keys.size());
    for (auto const& key : required_keys)
        required_views.push_back(key);
    summary.catalog_diagnostics =
        phenotype::validate_resource_catalog(summary.catalog, required_views);
    return summary;
}

auto package_checks(PackageSummary const& summary) -> std::vector<Check> {
    return {
        {.name = "package_root",
         .ok = summary.exists && summary.is_directory,
         .detail = path_string(summary.root),
         .hint = "Pass a directory that owns phenotype package resources."},
        {.name = "manifest",
         .ok = summary.manifest && summary.manifest_bytes > 0,
         .detail = std::format("phenotype.package.toml ({} bytes)",
                               summary.manifest_bytes),
         .hint = "Add phenotype.package.toml before using CLI packaging."},
        {.name = "manifest_sections",
         .ok = summary.application_section && summary.resources_section
             && summary.debug_section,
         .detail = std::format("application={} resources={} debug={}",
                               summary.application_section ? "true" : "false",
                               summary.resources_section ? "true" : "false",
                               summary.debug_section ? "true" : "false"),
         .hint = "Expected [application], [resources], and [debug] sections."},
        {.name = "application_metadata",
         .ok = !summary.application_id.empty() && !summary.display_name.empty()
             && !summary.version.empty() && !summary.entry.empty()
             && !summary.platforms.empty(),
         .detail = std::format("id={} entry={} platforms={}",
                               summary.application_id.empty()
                                   ? "<missing>"
                                   : summary.application_id,
                               summary.entry.empty() ? "<missing>" : summary.entry,
                               summary.platforms.size()),
         .hint = "Expected application id, display_name, version, entry, and platforms."},
        {.name = "manifest_resources",
         .ok = summary.manifest_asset_count > 0
             && summary.manifest_locale_count > 0
             && summary.manifest_font_count > 0,
         .detail = std::format("assets={} locales={} fonts={}",
                               summary.manifest_asset_count,
                               summary.manifest_locale_count,
                               summary.manifest_font_count),
         .hint = "Expected at least one asset, locale, and font declaration."},
        {.name = "resource_catalog",
         .ok = summary.catalog_diagnostics.empty(),
         .detail = std::format("{} diagnostics, {} locale strings",
                               summary.catalog_diagnostics.size(),
                               summary.locale_string_count),
         .hint = "Fix the manifest fields reported by ResourceCatalog diagnostics."},
        {.name = "manifest_sources",
         .ok = summary.source_reference_count > 0
             && summary.missing_sources.empty(),
         .detail = std::format("{} sources, {} missing",
                               summary.source_reference_count,
                               summary.missing_sources.size()),
         .hint = "Every manifest source path must exist relative to the package root."},
        {.name = "artifact_manifest",
         .ok = !summary.artifact_manifest.empty()
             && summary.artifact_manifest_exists,
         .detail = summary.artifact_manifest.empty()
             ? "artifact_manifest=<missing>"
             : std::format("{} present={}",
                           summary.artifact_manifest,
                           summary.artifact_manifest_exists ? "true" : "false"),
         .hint = "Package debug resources should point at an existing artifact manifest."},
        {.name = "debug_metadata",
         .ok = !summary.debug_probe_scene.empty()
             && !summary.debug_verifier.empty(),
         .detail = std::format("probe_scene={} verifier={}",
                               summary.debug_probe_scene.empty()
                                   ? "<missing>"
                                   : summary.debug_probe_scene,
                               summary.debug_verifier.empty()
                                   ? "<missing>"
                                   : summary.debug_verifier),
         .hint = "Expected [debug] probe_scene and verifier metadata."},
        {.name = "default_font",
         .ok = summary.default_font_pretendard,
         .detail = summary.default_font_family.empty()
             ? "default_font_family=<missing>"
             : std::format("default_font_family={}", summary.default_font_family),
         .hint = "Package resources should default to Pretendard for UI text."},
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

auto resource_diagnostic_json(phenotype::ResourceDiagnostic const& diagnostic)
    -> std::string {
    return std::format(
        "{{\"severity\":{},\"kind\":{},\"path\":{},\"message\":{},"
        "\"expected\":{},\"actual\":{}}}",
        json_string(phenotype::resource_diagnostic_severity_name(
            diagnostic.severity)),
        json_string(phenotype::resource_diagnostic_kind_name(diagnostic.kind)),
        json_string(diagnostic.path),
        json_string(diagnostic.message),
        json_string(diagnostic.expected),
        json_string(diagnostic.actual));
}

auto resource_diagnostics_json(
        std::span<phenotype::ResourceDiagnostic const> diagnostics)
    -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
        if (i > 0)
            out += ",";
        out += resource_diagnostic_json(diagnostics[i]);
    }
    out += "]";
    return out;
}

auto locale_key_array_json(
        std::span<phenotype::LocaleString const> strings) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < strings.size(); ++i) {
        if (i > 0)
            out += ",";
        out += json_string(strings[i].key);
    }
    out += "]";
    return out;
}

auto assets_catalog_json(
        std::span<phenotype::AssetDescriptor const> assets) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < assets.size(); ++i) {
        auto const& asset = assets[i];
        if (i > 0)
            out += ",";
        out += std::format(
            "{{\"name\":{},\"source\":{},\"content_type\":{},"
            "\"preload\":{},\"runtime_visible\":{}}}",
            json_string(asset.name),
            json_string(asset.source),
            json_string(asset.content_type),
            asset.preload ? "true" : "false",
            asset.runtime_visible ? "true" : "false");
    }
    out += "]";
    return out;
}

auto locales_catalog_json(
        std::span<phenotype::LocaleDescriptor const> locales) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < locales.size(); ++i) {
        auto const& locale = locales[i];
        if (i > 0)
            out += ",";
        out += std::format(
            "{{\"tag\":{},\"source\":{},\"fallback\":{},"
            "\"string_count\":{},\"keys\":{}}}",
            json_string(locale.tag),
            json_string(locale.source),
            string_array_json(locale.fallback),
            locale.strings.size(),
            locale_key_array_json(locale.strings));
    }
    out += "]";
    return out;
}

auto fonts_catalog_json(
        std::span<phenotype::FontDescriptor const> fonts) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < fonts.size(); ++i) {
        auto const& font = fonts[i];
        if (i > 0)
            out += ",";
        out += std::format(
            "{{\"family\":{},\"source\":{},\"register\":{},"
            "\"fallback\":{}}}",
            json_string(font.family),
            json_string(font.source),
            font.register_font ? "true" : "false",
            string_array_json(font.fallback));
    }
    out += "]";
    return out;
}

auto resource_catalog_json(PackageSummary const& summary) -> std::string {
    auto const& catalog = summary.catalog;
    return std::format(
        "{{\"application\":{{\"id\":{},\"display_name\":{},\"version\":{},"
        "\"entry\":{},\"platforms\":{}}},"
        "\"defaults\":{{\"locale\":{},\"font_family\":{}}},"
        "\"assets\":{},\"locales\":{},\"fonts\":{},"
        "\"debug\":{{\"artifact_manifest\":{},\"probe_scene\":{},"
        "\"verifier\":{}}},\"diagnostics\":{}}}",
        json_string(catalog.application.id),
        json_string(catalog.application.display_name),
        json_string(catalog.application.version),
        json_string(catalog.application.entry),
        string_array_json(catalog.application.platforms),
        json_string(catalog.default_locale),
        json_string(catalog.default_font_family),
        assets_catalog_json(catalog.assets),
        locales_catalog_json(catalog.locales),
        fonts_catalog_json(catalog.fonts),
        json_string(catalog.debug.artifact_manifest),
        json_string(catalog.debug.probe_scene),
        json_string(catalog.debug.verifier),
        resource_diagnostics_json(summary.catalog_diagnostics));
}

auto resource_catalog_summary_json(PackageSummary const& summary)
    -> std::string {
    auto const& catalog = summary.catalog;
    return std::format(
        "{{\"default_locale\":{},\"default_font_family\":{},"
        "\"assets\":{},\"locales\":{},\"locale_strings\":{},"
        "\"fonts\":{},\"diagnostics\":{}}}",
        json_string(catalog.default_locale),
        json_string(catalog.default_font_family),
        catalog.assets.size(),
        catalog.locales.size(),
        summary.locale_string_count,
        catalog.fonts.size(),
        summary.catalog_diagnostics.size());
}

auto package_json(PackageSummary const& summary,
                  std::span<Check const> checks) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"package inspect\",\"ok\":{},"
        "\"root\":{},"
        "\"manifest\":{{\"present\":{},\"bytes\":{},"
        "\"application\":{{\"id\":{},\"display_name\":{},\"version\":{},"
        "\"entry\":{},\"platforms\":{}}},"
        "\"sections\":{{\"application\":{},\"resources\":{},\"debug\":{}}},"
        "\"declared_resources\":{{\"assets\":{},\"locales\":{},\"fonts\":{}}},"
        "\"source_reference_count\":{},\"missing_sources\":{},"
        "\"locale_string_count\":{},"
        "\"artifact_manifest\":{{\"path\":{},\"present\":{}}},"
        "\"debug\":{{\"probe_scene\":{},\"verifier\":{}}},"
        "\"default_font_family\":{},\"default_font_pretendard\":{}}},"
        "\"assets\":{{\"present\":{},\"file_count\":{}}},"
        "\"locales\":{{\"present\":{},\"file_count\":{}}},"
        "\"fonts\":{{\"present\":{},\"file_count\":{}}},"
        "\"resource_catalog\":{},\"checks\":{}}}",
        all_ok(checks) ? "true" : "false",
        json_string(path_string(summary.root)),
        summary.manifest ? "true" : "false",
        summary.manifest_bytes,
        json_string(summary.application_id),
        json_string(summary.display_name),
        json_string(summary.version),
        json_string(summary.entry),
        string_array_json(summary.platforms),
        summary.application_section ? "true" : "false",
        summary.resources_section ? "true" : "false",
        summary.debug_section ? "true" : "false",
        summary.manifest_asset_count,
        summary.manifest_locale_count,
        summary.manifest_font_count,
        summary.source_reference_count,
        string_array_json(summary.missing_sources),
        summary.locale_string_count,
        json_string(summary.artifact_manifest),
        summary.artifact_manifest_exists ? "true" : "false",
        json_string(summary.debug_probe_scene),
        json_string(summary.debug_verifier),
        json_string(summary.default_font_family),
        summary.default_font_pretendard ? "true" : "false",
        summary.assets_directory ? "true" : "false",
        summary.asset_file_count,
        summary.locales_directory ? "true" : "false",
        summary.locale_file_count,
        summary.fonts_directory ? "true" : "false",
        summary.font_file_count,
        resource_catalog_json(summary),
        checks_json(checks));
}

auto package_entry_json(PackageSummary const& summary,
                        std::span<Check const> checks) -> std::string {
    return std::format(
        "{{\"root\":{},\"ok\":{},\"application_id\":{},"
        "\"display_name\":{},\"entry\":{},\"platforms\":{},"
        "\"default_font_family\":{},\"artifact_manifest\":{},"
        "\"debug_verifier\":{},\"resource_catalog\":{},\"checks\":{}}}",
        json_string(path_string(summary.root)),
        all_ok(checks) ? "true" : "false",
        json_string(summary.application_id),
        json_string(summary.display_name),
        json_string(summary.entry),
        string_array_json(summary.platforms),
        json_string(summary.default_font_family),
        json_string(summary.artifact_manifest),
        json_string(summary.debug_verifier),
        resource_catalog_summary_json(summary),
        checks_json(checks));
}

auto bundle_file_json(BundleFileRecord const& file,
                      fs::path const& output_root) -> std::string {
    auto ec = std::error_code{};
    auto relative_destination = fs::relative(file.destination, output_root, ec);
    return std::format(
        "{{\"kind\":{},\"name\":{},\"content_type\":{},"
        "\"source\":{},\"destination\":{},\"bytes\":{},"
        "\"present\":{},\"copied\":{},\"preload\":{},"
        "\"runtime_visible\":{},\"integrity\":{{\"algorithm\":\"sha256\","
        "\"digest\":{},\"ok\":{},\"error\":{}}},\"error\":{}}}",
        json_string(file.kind),
        json_string(file.name),
        json_string(file.content_type),
        json_string(path_string(file.source)),
        json_string(ec ? path_string(file.destination)
                       : path_string(relative_destination)),
        file.bytes,
        file.present ? "true" : "false",
        file.copied ? "true" : "false",
        file.preload ? "true" : "false",
        file.runtime_visible ? "true" : "false",
        json_string(file.sha256),
        (!file.sha256.empty() && file.integrity_error.empty())
            ? "true" : "false",
        json_string(file.integrity_error),
        json_string(file.error));
}

auto bundle_file_integrity_ok(BundleFileRecord const& file,
                              bool require_copied) -> bool {
    return file.present
        && (!require_copied || file.copied)
        && file.error.empty()
        && file.integrity_error.empty()
        && !file.sha256.empty();
}

auto bundle_files_integrity_ok(std::span<BundleFileRecord const> files,
                               bool require_copied) -> bool {
    return std::ranges::all_of(files, [&](auto const& file) {
        return bundle_file_integrity_ok(file, require_copied);
    });
}

auto bundle_total_bytes(std::span<BundleFileRecord const> files)
    -> std::uintmax_t {
    auto total = std::uintmax_t{0};
    for (auto const& file : files) {
        if (file.present)
            total += file.bytes;
    }
    return total;
}

auto bundle_verified_file_count(std::span<BundleFileRecord const> files)
    -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count_if(
        files,
        [](auto const& file) {
            return file.present
                && file.error.empty()
                && file.integrity_error.empty()
                && !file.sha256.empty();
        }));
}

auto bundle_files_json(std::span<BundleFileRecord const> files,
                       fs::path const& output_root) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (i > 0)
            out += ",";
        out += bundle_file_json(files[i], output_root);
    }
    out += "]";
    return out;
}

auto bundle_json(BundleSummary const& summary) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":{},"
        "\"ok\":{},\"package_root\":{},\"output_root\":{},"
        "\"bundle_manifest\":{},\"application\":{{\"id\":{},"
        "\"display_name\":{},\"version\":{},\"entry\":{},"
        "\"platforms\":{}}},\"defaults\":{{\"locale\":{},"
        "\"font_family\":{}}},\"debug\":{{\"artifact_manifest\":{},"
        "\"probe_scene\":{},\"verifier\":{}}},"
        "\"integrity\":{{\"algorithm\":\"sha256\",\"ok\":{},"
        "\"file_count\":{},\"verified_file_count\":{},"
        "\"total_bytes\":{},\"bundle_manifest\":{{\"present\":{},"
        "\"bytes\":{}}}}},\"file_count\":{},"
        "\"files\":{},\"checks\":{},\"error\":{}}}",
        json_string(summary.command),
        summary.ok ? "true" : "false",
        json_string(path_string(summary.package_root)),
        json_string(path_string(summary.output_root)),
        json_string(path_string(summary.manifest_path)),
        json_string(summary.package.application_id),
        json_string(summary.package.display_name),
        json_string(summary.package.version),
        json_string(summary.package.entry),
        string_array_json(summary.package.platforms),
        json_string(summary.package.catalog.default_locale),
        json_string(summary.package.catalog.default_font_family),
        json_string(summary.package.catalog.debug.artifact_manifest),
        json_string(summary.package.catalog.debug.probe_scene),
        json_string(summary.package.catalog.debug.verifier),
        bundle_files_integrity_ok(summary.files, summary.command == "package bundle")
            ? "true" : "false",
        summary.files.size(),
        summary.verified_file_count,
        summary.total_bytes,
        summary.bundle_manifest_present ? "true" : "false",
        summary.bundle_manifest_bytes,
        summary.files.size(),
        bundle_files_json(summary.files, summary.output_root),
        checks_json(summary.checks),
        json_string(summary.error));
}

auto package_manifest_roots(fs::path root) -> std::vector<fs::path> {
    auto roots = std::vector<fs::path>{};
    if (!path_is_directory(root))
        return roots;

    auto ec = std::error_code{};
    auto options = fs::directory_options::skip_permission_denied;
    for (auto it = fs::recursive_directory_iterator(root, options, ec);
         !ec && it != fs::recursive_directory_iterator{};
         it.increment(ec)) {
        auto path = it->path();
        auto name = path.filename().string();
        if (it->is_directory(ec)) {
            if (name == ".git" || name == ".exon" || name == "build") {
                it.disable_recursion_pending();
            }
            continue;
        }
        ec.clear();
        if (it->is_regular_file(ec) && name == "phenotype.package.toml") {
            roots.push_back(path.parent_path());
        }
    }
    std::ranges::sort(roots);
    return roots;
}

auto copy_bundle_file(fs::path const& package_root,
                      fs::path const& output_root,
                      std::string kind,
                      std::string name,
                      fs::path relative_source,
                      std::string content_type = {},
                      bool preload = false,
                      bool runtime_visible = false) -> BundleFileRecord {
    BundleFileRecord record{
        .kind = std::move(kind),
        .name = std::move(name),
        .content_type = std::move(content_type),
        .source = package_root / relative_source,
        .destination = output_root / relative_source,
        .preload = preload,
        .runtime_visible = runtime_visible,
    };

    if (!safe_relative_path(relative_source)) {
        record.error = "source path must be a safe relative path";
        return record;
    }

    if (!path_stays_under_root(package_root, record.source, record.error))
        return record;

    auto ec = std::error_code{};
    if (!fs::is_regular_file(record.source, ec) || ec) {
        record.error = ec ? ec.message() : "source file is missing";
        return record;
    }
    record.present = true;

    fs::create_directories(record.destination.parent_path(), ec);
    if (ec) {
        record.error = ec.message();
        return record;
    }

    ec.clear();
    fs::copy_file(
        record.source,
        record.destination,
        fs::copy_options::overwrite_existing,
        ec);
    if (ec) {
        record.error = ec.message();
        return record;
    }

    record.bytes = file_size_or_zero(record.destination);
    record.copied = true;
    if (auto digest = sha256_file_or_error(record.destination)) {
        record.sha256 = *digest;
    } else {
        record.integrity_error = digest.error();
    }
    return record;
}

auto inspect_bundle_file(fs::path const& bundle_root,
                         std::string kind,
                         std::string name,
                         fs::path relative_source,
                         std::string content_type = {},
                         bool preload = false,
                         bool runtime_visible = false) -> BundleFileRecord {
    BundleFileRecord record{
        .kind = std::move(kind),
        .name = std::move(name),
        .content_type = std::move(content_type),
        .source = bundle_root / relative_source,
        .destination = bundle_root / relative_source,
        .preload = preload,
        .runtime_visible = runtime_visible,
    };

    if (!safe_relative_path(relative_source)) {
        record.error = "source path must be a safe relative path";
        return record;
    }

    if (!path_stays_under_root(bundle_root, record.destination, record.error))
        return record;

    auto ec = std::error_code{};
    if (!fs::is_regular_file(record.destination, ec) || ec) {
        record.error = ec ? ec.message() : "bundle file is missing";
        return record;
    }

    record.present = true;
    record.bytes = file_size_or_zero(record.destination);
    if (auto digest = sha256_file_or_error(record.destination)) {
        record.sha256 = *digest;
    } else {
        record.integrity_error = digest.error();
    }
    return record;
}

void append_expected_package_files(BundleSummary& bundle, bool copy_files) {
    auto append = [&](std::string kind,
                      std::string name,
                      fs::path source,
                      std::string content_type = {},
                      bool preload = false,
                      bool runtime_visible = false) {
        if (copy_files) {
            bundle.files.push_back(copy_bundle_file(
                bundle.package_root,
                bundle.output_root,
                std::move(kind),
                std::move(name),
                std::move(source),
                std::move(content_type),
                preload,
                runtime_visible));
        } else {
            bundle.files.push_back(inspect_bundle_file(
                bundle.output_root,
                std::move(kind),
                std::move(name),
                std::move(source),
                std::move(content_type),
                preload,
                runtime_visible));
        }
    };

    append("manifest",
           "phenotype.package.toml",
           "phenotype.package.toml",
           "application/toml");

    for (auto const& asset : bundle.package.catalog.assets) {
        append("asset",
               asset.name,
               asset.source,
               asset.content_type,
               asset.preload,
               asset.runtime_visible);
    }
    for (auto const& locale : bundle.package.catalog.locales) {
        append("locale", locale.tag, locale.source, "application/toml");
    }
    for (auto const& font : bundle.package.catalog.fonts) {
        append("font", font.family, font.source, "application/toml");
    }
    if (!bundle.package.catalog.debug.artifact_manifest.empty()) {
        append("debug",
               "artifact_manifest",
               bundle.package.catalog.debug.artifact_manifest,
               "application/json");
    }
}

bool write_bundle_manifest(BundleSummary& bundle) {
    bundle.bundle_manifest_present = true;
    for (int attempt = 0; attempt < 4; ++attempt) {
        auto manifest = bundle_json(bundle);
        if (!write_text_file(bundle.manifest_path, manifest, bundle.error)) {
            bundle.bundle_manifest_present = false;
            return false;
        }
        auto bytes = file_size_or_zero(bundle.manifest_path);
        if (bytes == bundle.bundle_manifest_bytes)
            return true;
        bundle.bundle_manifest_bytes = bytes;
    }
    return true;
}

auto build_package_bundle(fs::path package_root,
                          fs::path output_root) -> BundleSummary {
    BundleSummary bundle{
        .package_root = std::move(package_root),
        .output_root = std::move(output_root),
    };
    bundle.package = package_summary(bundle.package_root);
    bundle.checks = package_checks(bundle.package);
    if (!all_ok(bundle.checks)) {
        bundle.error = "package inspect checks failed";
        return bundle;
    }

    auto ec = std::error_code{};
    fs::create_directories(bundle.output_root, ec);
    if (ec) {
        bundle.error = ec.message();
        return bundle;
    }

    append_expected_package_files(bundle, true);
    bundle.total_bytes = bundle_total_bytes(bundle.files);
    bundle.verified_file_count = bundle_verified_file_count(bundle.files);

    if (!bundle_files_integrity_ok(bundle.files, true)) {
        bundle.error = "one or more package resources failed to copy or verify";
        return bundle;
    }

    bundle.manifest_path = bundle.output_root / "phenotype.bundle.json";
    bundle.ok = true;
    if (!write_bundle_manifest(bundle)) {
        bundle.ok = false;
        return bundle;
    }
    return bundle;
}

auto verify_package_bundle(fs::path bundle_root) -> BundleSummary {
    BundleSummary bundle{
        .command = "package verify-bundle",
        .package_root = bundle_root,
        .output_root = std::move(bundle_root),
    };
    bundle.manifest_path = bundle.output_root / "phenotype.bundle.json";
    bundle.package = package_summary(bundle.output_root);
    bundle.checks = package_checks(bundle.package);
    bundle.bundle_manifest_present = path_exists(bundle.manifest_path);
    bundle.bundle_manifest_bytes = file_size_or_zero(bundle.manifest_path);
    bundle.checks.push_back({
        .name = "bundle_manifest",
        .ok = bundle.bundle_manifest_present && bundle.bundle_manifest_bytes > 0,
        .detail = std::format("phenotype.bundle.json ({} bytes)",
                              bundle.bundle_manifest_bytes),
        .hint = "Run phenotype package bundle before verifying a bundle directory.",
    });

    if (!all_ok(bundle.checks)) {
        bundle.error = "bundle package checks failed";
        return bundle;
    }

    append_expected_package_files(bundle, false);
    bundle.total_bytes = bundle_total_bytes(bundle.files);
    bundle.verified_file_count = bundle_verified_file_count(bundle.files);

    if (!bundle_files_integrity_ok(bundle.files, false)) {
        bundle.error = "one or more staged package resources failed integrity validation";
        return bundle;
    }

    bundle.ok = true;
    return bundle;
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

auto operation_receipt_json(
        file_explorer_demo::OperationReceipt const& receipt) -> std::string {
    return std::format(
        "{{\"kind\":{},\"target\":{},\"ok\":{},\"detail\":{}}}",
        json_string(receipt.kind),
        json_string(receipt.target),
        receipt.ok ? "true" : "false",
        json_string(receipt.detail));
}

auto explorer_input_json(file_explorer_demo::ExplorerInput const& input)
    -> std::string {
    return std::format(
        "{{\"kind\":{},\"value\":{},\"sort_mode\":{},"
        "\"viewport\":{{\"w\":{},\"h\":{},\"scale\":{}}},\"label\":{}}}",
        json_string(file_explorer_demo::explorer_input_kind_name(input.kind)),
        json_string(input.value),
        json_string(file_explorer_demo::sort_mode_label(input.sort_mode)),
        input.viewport_width,
        input.viewport_height,
        input.viewport_scale,
        json_string(file_explorer_demo::explorer_input_label(input)));
}

auto explorer_entry_json(file_explorer_demo::Entry const& entry)
    -> std::string {
    return std::format(
        "{{\"name\":{},\"folder\":{},\"size\":{},\"kind\":{}}}",
        json_string(entry.name),
        entry.folder ? "true" : "false",
        entry.size,
        json_string(file_explorer_demo::entry_kind_label(entry)));
}

auto explorer_entries_json(
        std::span<file_explorer_demo::Entry const> entries) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (i > 0)
            out += ",";
        out += explorer_entry_json(entries[i]);
    }
    out += "]";
    return out;
}

auto explorer_labels_json(file_explorer_demo::ExplorerLabels const& labels)
    -> std::string {
    return std::format(
        "{{\"title\":{},\"mobile_title\":{},\"sidebar_recents\":{},"
        "\"sidebar_shared\":{},\"favorites\":{},\"locations\":{},"
        "\"applications\":{},\"desktop\":{},\"documents\":{},"
        "\"pictures\":{},\"downloads\":{},\"icloud_drive\":{},"
        "\"home\":{},\"airdrop\":{},\"trash\":{},"
        "\"search_placeholder\":{},\"status_ready\":{},"
        "\"tab_browse\":{},\"tab_preview\":{},\"tab_create\":{},"
        "\"create_file\":{},\"create_folder\":{},\"duplicate\":{},"
        "\"duplicate_file\":{},\"delete\":{},\"delete_selected\":{},"
        "\"preview\":{},\"root\":{},\"docs\":{},\"pics\":{},"
        "\"back\":{},\"forward\":{},\"up\":{},\"sort\":{},"
        "\"name\":{},\"kind\":{},\"size\":{},"
        "\"no_matching_files\":{},\"select_file_to_preview\":{},"
        "\"select_file_from_browse\":{},\"create_hint\":{},"
        "\"file_name\":{},\"contents\":{},\"folder_name\":{},"
        "\"reset_demo_files\":{},\"status\":{}}}",
        json_string(labels.title),
        json_string(labels.mobile_title),
        json_string(labels.sidebar_recents),
        json_string(labels.sidebar_shared),
        json_string(labels.favorites),
        json_string(labels.locations),
        json_string(labels.applications),
        json_string(labels.desktop),
        json_string(labels.documents),
        json_string(labels.pictures),
        json_string(labels.downloads),
        json_string(labels.icloud_drive),
        json_string(labels.home),
        json_string(labels.airdrop),
        json_string(labels.trash),
        json_string(labels.search_placeholder),
        json_string(labels.status_ready),
        json_string(labels.tab_browse),
        json_string(labels.tab_preview),
        json_string(labels.tab_create),
        json_string(labels.create_file),
        json_string(labels.create_folder),
        json_string(labels.duplicate),
        json_string(labels.duplicate_file),
        json_string(labels.delete_action),
        json_string(labels.delete_selected),
        json_string(labels.preview),
        json_string(labels.root),
        json_string(labels.docs),
        json_string(labels.pics),
        json_string(labels.back),
        json_string(labels.forward),
        json_string(labels.up),
        json_string(labels.sort),
        json_string(labels.name),
        json_string(labels.kind),
        json_string(labels.size),
        json_string(labels.no_matching_files),
        json_string(labels.select_file_to_preview),
        json_string(labels.select_file_from_browse),
        json_string(labels.create_hint),
        json_string(labels.file_name),
        json_string(labels.contents),
        json_string(labels.folder_name),
        json_string(labels.reset_demo_files),
        json_string(labels.status));
}

auto explorer_chrome_json(
        file_explorer_demo::ExplorerChromeMetrics const& chrome) -> std::string {
    return std::format(
        "{{\"viewport\":{{\"w\":{},\"h\":{},\"scale\":{}}},"
        "\"integrated_titlebar_height\":{},\"sidebar_width\":{},"
        "\"sidebar_row_width\":{},\"sidebar_row_height\":{},"
        "\"sidebar_heading_height\":{},\"toolbar_group_height\":{},"
        "\"toolbar_group_radius\":{},\"toolbar_icon_button_width\":{},"
        "\"toolbar_icon_button_height\":{},\"window_radius\":{},"
        "\"icon_grid\":{{\"columns\":{},\"visible_rows\":{},"
        "\"visible_capacity\":{},\"column_width\":{},\"row_height\":{},"
        "\"column_pitch\":{},\"scroll_height\":{}}},"
        "\"toolbar\":{{\"group_count\":{},\"separator_count\":{},"
        "\"icon_button_count\":{},\"finder_segmented\":{}}},"
        "\"native_window\":{{\"integrated_titlebar\":{},"
        "\"native_window_controls\":{},\"duplicate_window_controls\":{}}}}}",
        chrome.viewport.width,
        chrome.viewport.height,
        chrome.viewport.scale,
        chrome.integrated_titlebar_height,
        chrome.sidebar_width,
        chrome.sidebar_row_width,
        chrome.sidebar_row_height,
        chrome.sidebar_heading_height,
        chrome.toolbar_group_height,
        chrome.toolbar_group_radius,
        chrome.toolbar_icon_button_width,
        chrome.toolbar_icon_button_height,
        chrome.window_radius,
        chrome.icon_grid_columns,
        chrome.icon_grid_visible_rows,
        chrome.icon_grid_visible_capacity,
        chrome.icon_grid_column_width,
        chrome.icon_grid_row_height,
        chrome.icon_grid_column_pitch,
        chrome.icon_grid_scroll_height,
        chrome.toolbar_group_count,
        chrome.toolbar_separator_count,
        chrome.toolbar_icon_button_count,
        chrome.finder_segmented_toolbar ? "true" : "false",
        chrome.integrated_titlebar ? "true" : "false",
        chrome.native_window_controls ? "true" : "false",
        chrome.duplicate_window_controls ? "true" : "false");
}

auto explorer_trace_json(
        file_explorer_demo::ExplorerInputTrace const& trace,
        std::size_t index) -> std::string {
    return std::format(
        "{{\"index\":{},\"input\":{},\"chrome\":{},\"status\":{},"
        "\"relative_location\":{},\"selected_name\":{},"
        "\"visible_entries\":{},\"has_selection\":{},"
        "\"operation_label\":{},\"operation\":{}}}",
        index,
        explorer_input_json(trace.input),
        explorer_chrome_json(trace.chrome),
        json_string(trace.status),
        json_string(trace.relative_location),
        json_string(trace.selected_name),
        trace.visible_entries,
        trace.has_selection ? "true" : "false",
        json_string(trace.operation_label),
        operation_receipt_json(trace.operation));
}

auto explorer_trace_array_json(
        std::span<file_explorer_demo::ExplorerInputTrace const> trace)
    -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < trace.size(); ++i) {
        if (i > 0)
            out += ",";
        out += explorer_trace_json(trace[i], i);
    }
    out += "]";
    return out;
}

auto explorer_drive_ok(file_explorer_demo::ExplorerDriveResult const& result)
    -> bool {
    return std::ranges::all_of(result.trace, [](auto const& trace) {
        return trace.operation.kind.empty() || trace.operation.ok;
    });
}

auto explorer_expectation_json(
        file_explorer_demo::ExplorerExpectationResult const& expectation)
    -> std::string {
    return std::format(
        "{{\"kind\":{},\"value\":{},\"expects_operation_ok\":{},"
        "\"operation_ok\":{},\"label\":{},\"ok\":{},\"actual\":{},"
        "\"detail\":{}}}",
        json_string(file_explorer_demo::explorer_expectation_kind_name(
            expectation.expectation.kind)),
        json_string(expectation.expectation.value),
        expectation.expectation.expects_operation_ok ? "true" : "false",
        expectation.expectation.operation_ok ? "true" : "false",
        json_string(file_explorer_demo::explorer_expectation_label(
            expectation.expectation)),
        expectation.ok ? "true" : "false",
        json_string(expectation.actual),
        json_string(expectation.detail));
}

auto explorer_expectations_json(
        std::span<file_explorer_demo::ExplorerExpectationResult const>
            expectations) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < expectations.size(); ++i) {
        if (i > 0)
            out += ",";
        out += explorer_expectation_json(expectations[i]);
    }
    out += "]";
    return out;
}

auto explorer_resources_json(ExplorerDriveResources const& resources)
    -> std::string {
    return std::format(
        "{{\"source\":{},\"package_root\":{},\"locale\":{},"
        "\"application_id\":{},\"display_name\":{},\"entry\":{},"
        "\"default_locale\":{},\"default_font_family\":{},"
        "\"diagnostics\":{},\"checks\":{}}}",
        json_string(resources.source),
        json_string(path_string(resources.package_root)),
        json_string(resources.locale),
        json_string(resources.catalog.application.id),
        json_string(resources.catalog.application.display_name),
        json_string(resources.catalog.application.entry),
        json_string(resources.catalog.default_locale),
        json_string(resources.catalog.default_font_family),
        resource_diagnostics_json(resources.diagnostics),
        checks_json(resources.checks));
}

auto explorer_contract_ok(
        file_explorer_demo::ExplorerDriveResult const& result,
        std::span<file_explorer_demo::ExplorerExpectationResult const>
            expectations) -> bool {
    return explorer_drive_ok(result)
        && file_explorer_demo::explorer_expectations_ok(expectations);
}

auto explorer_drive_json(
        file_explorer_demo::ExplorerDriveResult const& result,
        ExplorerDriveResources const& resources,
        file_explorer_demo::ExplorerLabels const& labels,
        std::span<file_explorer_demo::ExplorerExpectationResult const>
            expectations)
    -> std::string {
    auto const& snap = result.snapshot;
    return std::format(
        "{{\"schema_version\":1,\"command\":\"drive file-explorer\","
        "\"ok\":{},\"profile\":{},\"input_count\":{},"
        "\"resources\":{},\"labels\":{},"
        "\"root\":{},\"current\":{},\"relative_location\":{},"
        "\"status\":{},\"sort_label\":{},"
        "\"viewport\":{{\"w\":{},\"h\":{},\"scale\":{}}},"
        "\"chrome\":{},"
        "\"selected\":{{\"present\":{},\"name\":{},\"kind\":{},"
        "\"size\":{},\"path_label\":{},\"preview_excerpt\":{}}},"
        "\"counts\":{{\"visible_entries\":{},\"files\":{},\"folders\":{}}},"
        "\"capabilities\":{{\"can_go_back\":{},\"can_go_forward\":{},"
        "\"can_create_file\":{},\"can_create_folder\":{},"
        "\"can_delete_selected\":{},\"can_duplicate_selected\":{},"
        "\"can_preview_selected\":{}}},"
        "\"operation\":{},\"entries\":{},\"trace\":{},"
        "\"expectations\":{}}}",
        explorer_contract_ok(result, expectations) ? "true" : "false",
        json_string(result.profile),
        result.trace.size(),
        explorer_resources_json(resources),
        explorer_labels_json(labels),
        json_string(path_string(result.state.root)),
        json_string(path_string(result.state.current)),
        json_string(snap.relative_location),
        json_string(result.state.status),
        json_string(snap.sort_label),
        result.state.viewport_width,
        result.state.viewport_height,
        result.state.viewport_scale,
        explorer_chrome_json(result.chrome),
        snap.has_selection ? "true" : "false",
        json_string(snap.has_selection ? snap.selected.name : ""),
        json_string(snap.selected_kind_label),
        json_string(snap.selected_size_label),
        json_string(snap.selected_path_label),
        json_string(output_tail(snap.preview, 512)),
        snap.entries.size(),
        snap.file_count,
        snap.folder_count,
        snap.can_go_back ? "true" : "false",
        snap.can_go_forward ? "true" : "false",
        snap.can_create_file ? "true" : "false",
        snap.can_create_folder ? "true" : "false",
        snap.can_delete_selected ? "true" : "false",
        snap.can_duplicate_selected ? "true" : "false",
        snap.can_preview_selected ? "true" : "false",
        operation_receipt_json(result.state.last_operation),
        explorer_entries_json(snap.entries),
        explorer_trace_array_json(result.trace),
        explorer_expectations_json(expectations));
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

    return std::format(
        "{{\"schema_version\":1,\"command\":\"run\",\"ok\":{},"
        "\"example\":{},\"example_root\":{},\"package_name\":{},"
        "\"executable\":{},\"build_requested\":{},\"run_requested\":{},"
        "\"artifact_exit\":{},\"timeout_seconds\":{},"
        "\"build\":{},\"run_result\":{},\"artifact\":{},\"error\":{}}}",
        summary.ok ? "true" : "false",
        json_string(summary.example),
        json_string(path_string(summary.example_root)),
        json_string(summary.package_name),
        json_string(path_string(summary.executable)),
        summary.build_requested ? "true" : "false",
        summary.run_requested ? "true" : "false",
        summary.artifact_exit ? "true" : "false",
        timeout_seconds,
        process_result_detail_json(summary.build_result),
        process_result_detail_json(summary.run_result),
        artifact,
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
    auto env = std::map<std::string, std::string>{};
    env["PHENOTYPE_CLI_SCRIPT_COMPAT"] = "1";
    if (auto value = invocation.value("bundle-dir")) {
        env["PHENOTYPE_ARTIFACT_DIR"] =
            absolute_path_string(fs::path{std::string{*value}});
    }
    if (auto value = invocation.value("expect-platform")) {
        env["PHENOTYPE_EXPECT_PLATFORM"] = std::string{*value};
    }
    auto script = invocation.has("accessibility")
        ? fs::path{"tools/verify_glass_showcase_accessibility_artifact.sh"}
        : fs::path{"tools/verify_glass_showcase_artifact.sh"};
    return run_repo_script(
        "artifact verify-glass-showcase",
        script,
        invocation,
        std::move(env),
        std::chrono::minutes{20});
}

int run_artifact_verify_file_explorer(
        cppx::cli::Invocation const& invocation) {
    auto env = std::map<std::string, std::string>{};
    env["PHENOTYPE_CLI_SCRIPT_COMPAT"] = "1";
    if (auto value = invocation.value("profile")) {
        auto profile = std::string{*value};
        if (profile != "all" && profile != "desktop" && profile != "mobile") {
            return print_error(
                "artifact verify-file-explorer",
                "profile must be all, desktop, or mobile",
                invocation.has("json"));
        }
        env["PHENOTYPE_FILE_EXPLORER_PROFILE"] = std::move(profile);
    }
    for (auto const& mode : invocation.values("view-mode")) {
        if (mode != "icon" && mode != "list"
            && mode != "column" && mode != "gallery") {
            return print_error(
                "artifact verify-file-explorer",
                "view-mode must be icon, list, column, or gallery",
                invocation.has("json"));
        }
    }
    if (!invocation.values("view-mode").empty()) {
        env["PHENOTYPE_FILE_EXPLORER_DESKTOP_MODES"] =
            comma_join(invocation.values("view-mode"));
    }
    if (!invocation.values("scenario").empty()) {
        env["PHENOTYPE_FILE_EXPLORER_SCENARIOS"] =
            comma_join(invocation.values("scenario"));
    }
    if (auto value = invocation.value("desktop-artifact-dir")) {
        env["PHENOTYPE_FILE_EXPLORER_DESKTOP_ARTIFACT_DIR"] =
            absolute_path_string(fs::path{std::string{*value}});
    }
    if (auto value = invocation.value("mobile-artifact-dir")) {
        env["PHENOTYPE_FILE_EXPLORER_MOBILE_ARTIFACT_DIR"] =
            absolute_path_string(fs::path{std::string{*value}});
    }
    if (auto value = invocation.value("settle-seconds")) {
        env["PHENOTYPE_FILE_EXPLORER_CAPTURE_SETTLE_SECONDS"] =
            std::string{*value};
    }
    return run_repo_script(
        "artifact verify-file-explorer",
        "tools/verify_file_explorer_artifacts.sh",
        invocation,
        std::move(env),
        std::chrono::minutes{45});
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

int run_package_list(cppx::cli::Invocation const& invocation) {
    auto root = optional_positional_or_error(invocation, "package list", ".");
    if (!root)
        return print_error("package list", root.error(), invocation.has("json"));

    auto manifests = package_manifest_roots(*root);
    auto entries = std::vector<std::pair<PackageSummary, std::vector<Check>>>{};
    entries.reserve(manifests.size());
    auto ok = path_is_directory(*root) && !manifests.empty();
    for (auto const& manifest_root : manifests) {
        auto summary = package_summary(manifest_root);
        auto checks = package_checks(summary);
        ok = ok && all_ok(checks);
        entries.push_back({std::move(summary), std::move(checks)});
    }

    if (invocation.has("json")) {
        auto packages = std::string{"["};
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (i > 0)
                packages += ",";
            packages += package_entry_json(entries[i].first, entries[i].second);
        }
        packages += "]";
        std::println(
            "{{\"schema_version\":1,\"command\":\"package list\","
            "\"ok\":{},\"root\":{},\"package_count\":{},\"packages\":{}}}",
            ok ? "true" : "false",
            json_string(path_string(*root)),
            entries.size(),
            packages);
    } else {
        auto lines = std::vector<cppx::terminal::StatusLine>{};
        lines.reserve(entries.size());
        for (auto const& [summary, checks] : entries) {
            lines.push_back({
                .label = summary.application_id.empty()
                    ? path_string(summary.root)
                    : summary.application_id,
                .value = path_string(summary.root),
                .status = all_ok(checks) ? cppx::terminal::StatusKind::ok
                                         : cppx::terminal::StatusKind::fail,
            });
        }
        if (lines.empty()) {
            lines.push_back({
                .label = "packages",
                .value = "none",
                .status = cppx::terminal::StatusKind::fail,
            });
        }
        std::println("phenotype package list");
        std::println("{}", cppx::terminal::format_status_frame(lines, false));
    }
    return ok ? 0 : 1;
}

int run_package_bundle(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "package bundle");
    if (!path)
        return print_error("package bundle", path.error(), invocation.has("json"));
    auto output = invocation.value("output");
    if (!output) {
        return print_error(
            "package bundle",
            "package bundle requires --output <dir>",
            invocation.has("json"));
    }

    auto bundle = build_package_bundle(
        fs::path{*path},
        fs::path{std::string{*output}});
    if (invocation.has("json")) {
        std::println("{}", bundle_json(bundle));
    } else {
        auto lines = std::vector<cppx::terminal::StatusLine>{
            {.label = "package",
             .value = path_string(bundle.package_root),
             .status = all_ok(bundle.checks)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
            {.label = "output",
             .value = path_string(bundle.output_root),
             .status = bundle.ok ? cppx::terminal::StatusKind::ok
                                 : cppx::terminal::StatusKind::fail},
            {.label = "manifest",
             .value = path_string(bundle.manifest_path),
             .status = bundle.ok ? cppx::terminal::StatusKind::ok
                                 : cppx::terminal::StatusKind::skip},
            {.label = "files",
             .value = std::format("{}", bundle.files.size()),
             .status = bundle_files_integrity_ok(bundle.files, true)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
        };
        std::println("phenotype package bundle");
        std::println("{}", cppx::terminal::format_status_frame(lines, false));
        if (!bundle.error.empty()) {
            std::println("{}",
                         cppx::terminal::format_diagnostic({
                             .severity = cppx::terminal::DiagnosticSeverity::error,
                             .message = bundle.error,
                             .context = "package bundle",
                         }));
        }
    }
    return bundle.ok ? 0 : 1;
}

int run_package_verify_bundle(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "package verify-bundle");
    if (!path) {
        return print_error(
            "package verify-bundle",
            path.error(),
            invocation.has("json"));
    }

    auto bundle = verify_package_bundle(fs::path{*path});
    if (invocation.has("json")) {
        std::println("{}", bundle_json(bundle));
    } else {
        auto lines = std::vector<cppx::terminal::StatusLine>{
            {.label = "bundle",
             .value = path_string(bundle.output_root),
             .status = all_ok(bundle.checks)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
            {.label = "manifest",
             .value = path_string(bundle.manifest_path),
             .status = bundle.bundle_manifest_present
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
            {.label = "integrity",
             .value = std::format("{}/{} files, {} bytes",
                                  bundle.verified_file_count,
                                  bundle.files.size(),
                                  bundle.total_bytes),
             .status = bundle_files_integrity_ok(bundle.files, false)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
        };
        std::println("phenotype package verify-bundle");
        std::println("{}", cppx::terminal::format_status_frame(lines, false));
        if (!bundle.error.empty()) {
            std::println("{}",
                         cppx::terminal::format_diagnostic({
                             .severity = cppx::terminal::DiagnosticSeverity::error,
                             .message = bundle.error,
                             .context = "package verify-bundle",
                         }));
        }
    }
    return bundle.ok ? 0 : 1;
}

auto parse_explorer_input_script(fs::path const& path)
    -> std::expected<std::vector<file_explorer_demo::ExplorerInput>, std::string> {
    if (!path_exists(path)) {
        return std::unexpected{
            std::format("input script does not exist: {}", path_string(path))};
    }
    auto text = read_text_file(path);
    auto inputs = std::vector<file_explorer_demo::ExplorerInput>{};
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
        auto parsed = file_explorer_demo::parse_explorer_input(trimmed);
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
        .artifact_exit = invocation.has("artifact-exit"),
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

    if (auto value = invocation.value("artifact-dir")) {
        summary.artifact_requested = true;
        summary.artifact_dir =
            fs::path{absolute_path_string(fs::path{std::string{*value}})};
        (*env)["PHENOTYPE_ARTIFACT_DIR"] = path_string(summary.artifact_dir);
    }
    if (summary.artifact_exit) {
        (*env)["PHENOTYPE_ARTIFACT_EXIT"] = "1";
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
    summary.ok = !summary.run_result->timed_out
        && summary.run_result->exit_code == 0
        && (!summary.artifact_requested
            || (summary.artifact && summary.artifact->snapshot_json));
    if (!summary.ok && summary.error.empty()) {
        if (summary.run_result->timed_out) {
            summary.error = "example timed out";
        } else if (summary.run_result->exit_code != 0) {
            summary.error = std::format(
                "example exited with {}",
                summary.run_result->exit_code);
        } else if (summary.artifact_requested) {
            summary.error = "artifact bundle did not contain snapshot.json";
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
        == std::vector<std::string>{"phenotype", "drive", "file-explorer"})
        return run_drive_file_explorer(*parsed);
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
